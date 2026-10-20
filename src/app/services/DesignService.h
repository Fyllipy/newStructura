#pragma once

#include "app/ProjectDataTypes.h"
#include "app/SolverBridge.h"
#include "design/BarDesign.h"
#include "viz/postprocessor/PostProcessorDataTypes.h"

#include <QMap>
#include <QString>
#include <QStringList>

#include <array>
#include <QVector>

#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace Structura::App::Services {

/**
 * @brief Dimensionamento de barras de aco (NBR 8800:2024 e NBR 14762:2010) a partir dos resultados do solver.
 *
 * Converte os esforcos do solver (por caso/combinacao) em esforcos ao longo de cada barra, monta a secao de
 * dimensionamento a partir do perfil atribuido e aplica o modulo `Structura::Design`.
 * Tambem escolhe o menor perfil (menor peso) de uma serie que atende as verificacoes.
 */
class DesignService
{
public:
    using SectionInfo = Structura::App::ProjectData::SectionInfo;
    using MaterialInfo = Structura::App::ProjectData::MaterialInfo;
    using DesignSettings = Structura::App::ProjectData::DesignSettings;
    using DesignMemberParams = Structura::App::ProjectData::DesignMemberParams;

    struct SteelOption {
        QString name;
        double fy {0.0};   ///< MPa
        double fu {0.0};   ///< MPa
        bool coldFormed {false};
    };

    static QVector<SteelOption> steelGrades();
    static std::optional<SteelOption> findSteel(const QString &name);

    /// Uma combinacao (ou caso) ja resolvida: resultado do solver e os casos que a compoem (modelo + fator).
    struct ResultSet {
        QString name;
        const Structura::App::SolverBridge::Result *result {nullptr};
        std::vector<std::pair<const Structura::App::SolverBridge::ModelData *, double>> terms;
    };

    /// Barra reconstruida para o pos-processamento (eixos locais, esforcos de extremidade e cargas) de uma combinacao.
    struct ElementData {
        Structura::Visualization::BeamElementResults element;
        int nodeI {0};
        int nodeJ {0};
        double qy {0.0};   ///< carga distribuida resultante nos eixos locais
        double qz {0.0};
    };
    static std::vector<ElementData> buildElements(const Structura::App::SolverBridge::ModelData &geometry, const ResultSet &set);

    /// Esforcos de uma barra em todas as combinacoes selecionadas, ja em N e mm.
    /// Forcas nas extremidades da barra (eixos locais N, Vy, Vz, Mt, My, Mz do no i e do no j; N e N.mm) e os eixos locais no sistema global:
    /// convencao do avaliador de esforcos internos (esforco na face positiva da secao). Permitem o equilibrio do no (painel de alma do pilar).
    struct EndRecord {
        std::string combo;
        std::array<double, 3> x {}, y {}, z {};
        std::array<double, 12> f {};
    };
    struct MemberEffects {
        int memberId {0};
        double lengthMm {0.0};
        std::vector<Structura::Design::ComboEffects> combos;
        std::vector<EndRecord> ends;
    };

    /// Converte os resultados do solver em esforcos por barra. `geometry` fornece nos e barras.
    /// `forceFactorToN`: fator das unidades de forca do modelo para N; `lengthFactorToMm`: das unidades de comprimento para mm.
    static QVector<MemberEffects> collectEffects(const Structura::App::SolverBridge::ModelData &geometry,
                                                 const std::vector<ResultSet> &sets, double forceFactorToN,
                                                 double lengthFactorToMm, int stations = 21);

    /// Define o B2 de cada combinacao/barra (`b2[combinacao][barra]`); ignora combinacoes ausentes.
    static void applyB2(QVector<MemberEffects> &effects, const QMap<QString, QMap<int, double>> &b2);

    /// Fatores de unidade deduzidos do modulo de elasticidade do material (Pa -> N e m; kN/m2 -> kN e m).
    static std::pair<double, double> unitFactors(const QVector<MaterialInfo> &materials);

    /// Nomes das combinacoes ultimas por padrao (as que nao comecam por "ELS").
    static QStringList defaultUltimateCombinations(const QStringList &allNames);

    struct MemberVerdict {
        int memberId {0};
        QString sectionName;
        QString steel;
        bool supported {false};
        double lengthM {0.0};
        Structura::Design::BarDesignOutput output;
    };

    /// Secao de dimensionamento correspondente ao perfil (propriedades brutas nos eixos principais).
    static Structura::Design::DesignSection makeSection(const SectionInfo &section, const QString &rectTubeStandard);

    /// Razoes Ief/I (eixos X e Y de projeto) para as flechas de ELS: secao efetiva em perfis formados a frio (momentos de servico
    /// `mxNmm`, `myNmm` em N.mm), 1,0 nos demais.
    static std::array<double, 2> serviceInertiaRatios(const Structura::Design::DesignSection &section, const QString &steelName, double mxNmm, double myNmm);

    /// Verifica uma barra com o perfil informado.
    static MemberVerdict verifyMember(const MemberEffects &effects, const SectionInfo &section,
                                      const DesignMemberParams &params, const DesignSettings &settings,
                                      double lengthFactorToMm);

    /// Verifica todas as barras (`sectionOf` devolve o perfil atribuido a cada uma).
    struct MemberRef {
        int memberId {0};
        const SectionInfo *section {nullptr};
    };
    /// As barras sao verificadas em paralelo (verificacoes independentes); `progress(feitas, total)` e chamado de qualquer thread.
    static QVector<MemberVerdict> verifyAll(const QVector<MemberEffects> &effects, const std::vector<MemberRef> &members,
                                            const DesignSettings &settings, double lengthFactorToMm,
                                            const std::function<void(int, int)> &progress = {});

    /// Perfis candidatos (mesma serie/familia do perfil informado), ordenados por peso crescente.
    static QVector<SectionInfo> candidateSeries(const SectionInfo &current);

    struct BestChoice {
        bool found {false};
        SectionInfo section;
        double maxRatio {0.0};
        QString note;
    };
    /// Menor perfil da serie que atende todas as barras de `members` (esforcos fixos, sem reanalise).
    /// `minInertia` (m4): descarta perfis cuja maior inercia seja menor (piso de rigidez para o limite de flecha).
    static BestChoice chooseBest(const QVector<MemberEffects> &effects, const QVector<int> &members,
                                 const SectionInfo &current, const DesignSettings &settings, double lengthFactorToMm,
                                 double maxRatio = 1.0, double minInertia = 0.0);

    /// Relatorio de calculo (memoria) de uma barra em texto simples.
    static QString report(const MemberVerdict &verdict, const DesignSettings &settings);
};

} // namespace Structura::App::Services
