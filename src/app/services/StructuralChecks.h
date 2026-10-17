#pragma once

#include "app/ProjectDataTypes.h"
#include "app/SolverBridge.h"
#include "app/services/DesignService.h"

#include <array>
#include <functional>

#include <QMap>
#include <QString>
#include <QVector>

namespace Structura::App::Services {

/**
 * @brief Verificacoes de estado-limite de servico (NBR 8800, Anexo B) e amplificacao de esforcos (Anexo C).
 *
 *  - flechas de barras por integracao da curvatura (M/EI) a partir dos esforcos reconstruidos;
 *  - deslocamentos horizontais por andar (topo e relativo entre pisos);
 *  - B2 por andar e combinacao: B2 = 1 / (1 - (1/Rs)(dh/h)(SN/SH)).
 */
class StructuralChecks
{
public:
    using ResultSet = DesignService::ResultSet;
    using DesignSettings = Structura::App::ProjectData::DesignSettings;

    struct DeflectionResult {
        int memberId {0};
        QString combination;
        double deflectionMm {0.0};   ///< maxima flecha (resultante) em relacao a corda
        double positionMm {0.0};
        double lengthMm {0.0};
        double limitMm {0.0};
        int denominator {0};
        bool cantilever {false};
        double ratio {0.0};          ///< flecha / limite
    };

    struct Story {
        int index {0};
        double zBottom {0.0};        ///< m
        double zTop {0.0};
        QVector<int> columns;        ///< ids das barras verticais
    };

    struct StoryResult {
        int story {0};
        QString combination;
        double heightMm {0.0};
        double driftXMm {0.0};
        double driftYMm {0.0};
        double sumN {0.0};           ///< N (compressao total nos pilares)
        double sumHx {0.0};          ///< N
        double sumHy {0.0};
        double b2x {1.0};
        double b2y {1.0};
        double topDisplacementMm {0.0};   ///< deslocamento horizontal do topo em relacao a base (acumulado)
        double heightFromBaseMm {0.0};
    };

    struct Output {
        QVector<Story> stories;
        QVector<StoryResult> storyResults;               ///< por andar e combinacao
        QMap<int, int> storyOfMember;                    ///< barra -> indice do andar
        QMap<QString, QMap<int, double>> b2;             ///< combinacao -> barra -> B2
        QStringList warnings;
    };

    /// Andares, deslocamentos relativos e B2 por combinacao. `sets` sao as combinacoes de 1a ordem.
    static Output stories(const Structura::App::SolverBridge::ModelData &geometry, const std::vector<ResultSet> &sets,
                          const DesignSettings &settings, double forceFactorToN, double lengthFactorToMm);

    /// Flechas das barras nas combinacoes dadas (esforcos em unidades do modelo; E e I do modelo).
    /// `inertiaRatio(barra, |Mz|max, |My|max)` (momentos de servico em unidades do modelo) devolve {Ief/I em torno de z local, Ief/I em torno
    /// de y local}: reduz a rigidez de perfis formados a frio pela secao efetiva (NBR 14762, 9.8.5).
    using InertiaRatio = std::function<std::array<double, 2>(int, double, double)>;
    static QVector<DeflectionResult> deflections(const Structura::App::SolverBridge::ModelData &geometry,
                                                 const std::vector<ResultSet> &sets, const DesignSettings &settings,
                                                 double lengthFactorToMm, const InertiaRatio &inertiaRatio = {});
};

} // namespace Structura::App::Services
