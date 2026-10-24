#pragma once

#include "app/ProjectDataTypes.h"
#include "design/ShearConnections.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Ligacoes do modelo: valores padrao, verificacao, proposta automatica e lista de pecas.
 *
 * Tipologias: chapa de cisalhamento e cantoneira dupla (viga-apoio), chapa de no (gusset) de trelica/contraventamento,
 * emenda de viga parafusada, base de pilar com chumbadores (NBR 8800:2024, 6.7) e ligacoes soldadas de trelica tubular (CIDECT DG1).
 * Os parametros ficam em `NodeConnection::values` (chaves abaixo); esforcos em N e N.mm.
 */
class ConnectionService
{
public:
    struct FieldDef {
        QString key;
        QString label;
        double min {0.0};
        double max {1e6};
        int decimals {1};
        QString suffix;
        QString group;          ///< titulo da secao do formulario
        QStringList options;    ///< se nao vazio, campo de escolha (o valor guardado e o indice)
        QString tip;
    };

    /// Barra (viga, pilar, diagonal ou banzo) a que a ligacao se refere.
    struct BeamInfo {
        QString name;
        QString family;
        QMap<QString, double> geometry;
        double depth {0.0};   ///< mm
        double bf {0.0};
        double tw {6.0};
        double tf {10.0};
        double area {0.0};    ///< mm2
        double fy {250.0};
        double fu {400.0};
    };

    /// Diagonal ou montante que chega ao no do banzo (ligacao tubular).
    struct BraceInfo {
        int memberId {0};
        bool atEnd {false};
        double d {0.0};
        double t {0.0};
        double fy {355.0};
        double angleDeg {90.0};   ///< angulo entre a diagonal e o eixo do banzo (0 a 90)
        bool sameSideAsFirst {true};
        // ligacoes de no de trelica (barras de perfil laminado ou formado a frio)
        double area {0.0};        ///< mm2
        double legB {0.0};        ///< largura da aba ligada (mm)
        double legT {0.0};        ///< espessura da aba (mm)
        double ec {0.0};          ///< excentricidade do centroide (mm)
        double fu {400.0};
        bool alongChord {true};   ///< a barra se afasta do no no mesmo sentido do eixo do banzo (do no para dentro dele)
    };

    /// Combinacao de esforcos (N, N.mm) na extremidade da barra.
    struct ForceCase {
        QString name;
        double shear {0.0};
        double axial {0.0};   ///< + tracao
        double moment {0.0};
        QVector<double> braceAxial;   ///< modulo da forca axial de cada diagonal (ligacao tubular)
        QVector<double> braceSigned;  ///< idem, com sinal (+ tracao): ligacoes de no de trelica
        /// Cortante no painel de alma do pilar (N) pelo equilibrio do no (mesas das vigas + cortante dos pilares, com sinal); < 0 = nao calculado
        double panelShear {-1.0};
        double columnAxial {0.0};     ///< esforco normal do pilar no no (N, modulo), com `panelShear`
    };

    struct Forces {
        double shear {0.0};         ///< N (resultante)
        double axial {0.0};         ///< N (+ tracao), dominante
        double tension {0.0};       ///< maior tracao (N)
        double compression {0.0};   ///< maior compressao (N)
        double moment {0.0};        ///< maior momento (N.mm)
        QVector<ForceCase> cases;   ///< por combinacao (usado por emenda, base e ligacao tubular)
        /// Solicitacao das OUTRAS ligacoes que chegam a mesma barra de apoio no mesmo no (N), somada a desta na verificacao do apoio.
        double hostTension {0.0};
        double hostCompression {0.0};
        int hostConnections {0};
    };

    /// Forcas (tracao, compressao; N) que a ligacao transmite normalmente a face da barra de apoio.
    static std::pair<double, double> supportDemand(const Structura::App::ProjectData::NodeConnection &connection, const Forces &forces);

    struct Evaluation {
        std::vector<Structura::Design::Connections::Check> checks;
        std::vector<Structura::Design::Connections::Part> parts;
        double maxRatio {0.0};
        QString governing;
        QString governingCase;   ///< combinacao que governa (tipologias avaliadas por combinacao)
        QString description;     ///< resumo da ligacao projetada
    };

    static QVector<FieldDef> fields(int type);
    /// Campos da verificacao de fadiga (NBR 8800:2024, Anexo H, H.5 e H.6) das ligacoes parafusadas e por pino.
    static QVector<FieldDef> fatigueFields();
    static QString typeName(int type);
    static QString typeSummary(int type);
    /// A tipologia usa parafusos (grau e plano de corte no formulario).
    static bool usesBolts(int type);
    /// Chaves de fy/fu da chapa principal (preenchidas pelo atalho de aco); vazio se nao ha.
    static QPair<QString, QString> steelKeys(int type);

    /// Valores iniciais a partir da barra e do aco padrao das chapas.
    static Structura::App::ProjectData::NodeConnection makeDefault(int type, const BeamInfo &beam, double plateFy, double plateFu);
    /// Idem, para a ligacao tubular (banzo + diagonais que chegam ao no).
    static Structura::App::ProjectData::NodeConnection makeTubular(const BeamInfo &chord, const QVector<BraceInfo> &braces);

    /// Verifica a ligacao com os esforcos dados. Ligacoes de NBR 8800 (exceto diagonais/contraventamentos, nos de trelica, tubulares e perfis formados a frio) sao
    /// dimensionadas, no minimo, para 45 kN em qualquer direcao (6.1.5.2): esforcos menores sao elevados proporcionalmente a 45 kN.
    static Evaluation evaluate(const Structura::App::ProjectData::NodeConnection &connection, const Forces &forces);
    /// Idem, sem o minimo de 6.1.5.2 (esforcos usados como informados). Barra inclinada em relacao a face do apoio (`skew` > 0 nos valores da ligacao):
    /// cada verificacao fica com a mais desfavoravel entre os esforcos nos eixos da barra e os resolvidos no plano do apoio (`interfaceForces`).
    static Evaluation evaluateExact(const Structura::App::ProjectData::NodeConnection &connection, const Forces &forces);
    /// Evaluacao com os esforcos exatamente como informados (sem resolver a inclinacao).
    static Evaluation evaluateExactBeamFrame(const Structura::App::ProjectData::NodeConnection &connection, const Forces &forces);

    /// Angulo (graus) entre o eixo da barra e a normal da face do apoio (0 = perpendicular a face; barra de tesoura sobre pilar: a inclinacao do banzo).
    static double skewDegrees(const std::array<double, 3> &beamAxis, const std::array<double, 3> &faceNormal);
    /// Esforcos de uma barra que chega inclinada de `skewDeg` em relacao a normal da face, resolvidos no plano do apoio (normal a face e paralelo a ela):
    /// N_n = |N| cos a + |V| sen a (soma em modulo: sentido de V no plano desconhecido) e V_p = |N| sen a + |V|. Conservador; igual aos esforcos da barra para a = 0.
    static Forces interfaceForces(const Forces &forces, double skewDeg);
    /// A forca minima de 6.1.5.2 vale para esta tipologia?
    static bool minimumForceApplies(const Structura::App::ProjectData::NodeConnection &connection);
    static constexpr double kMinimumDesignForce = 45000.0;   ///< N (6.1.5.2)

    /// Verificacao local do perfil de apoio (mesa ou alma onde a ligacao se solda), conforme `supType`/`supD`... de `connection.values`.
    /// `lengthAlong`/`lengthTransverse`: dimensoes da area carregada (mm); `tension`/`compression`: forcas normais a face (N).
    static void appendSupportLocal(Evaluation &evaluation, const Structura::App::ProjectData::NodeConnection &connection, const Forces &forces,
                                   double lengthAlong, double lengthTransverse, double tension, double compression);

    /// Substitui geometria pela proposta mais leve que atende; false se nenhuma atende (ou a tipologia nao tem proposta automatica).
    static bool propose(Structura::App::ProjectData::NodeConnection &connection, const Forces &forces);
    static bool hasProposal(int type);

    /// Resumo em uma linha ("Chapa 9,5 mm, 3 parafusos d20 A325, solda 6 mm").
    static QString describe(const Structura::App::ProjectData::NodeConnection &connection);
};

} // namespace Structura::App::Services
