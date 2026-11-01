#pragma once

#include "app/SolverBridge.h"

#include <QString>
#include <QVector>

#include <functional>
#include <map>
#include <set>

namespace Structura::App::Services {

/**
 * @brief Fases construtivas lineares com inercia reduzida por fase e acumulacao incremental.
 *
 * Cada fase e resolvida com a rigidez que a estrutura tem naquela fase (barras ainda nao executadas com rigidez desprezivel e sem cargas;
 * barras com area/inercias reduzidas, por exemplo fissuracao ou cura do concreto) e apenas com as cargas incrementais da fase.
 * O resultado acumulado e a soma dos incrementos (valido enquanto a resposta de cada fase e linear).
 */
class ConstructionStageService
{
public:
    struct Reduction {
        double area {1.0};
        double inertia {1.0};   ///< multiplica iy, iz, j e as areas de cisalhamento
    };

    struct Stage {
        QString name;
        /// Cargas incrementais da fase: so os vetores de cargas deste modelo sao usados; a geometria/secoes vem do modelo-base.
        Structura::App::SolverBridge::ModelData loads;
        /// Barras existentes na fase; vazio = todas.
        std::set<int> activeMembers;
        /// Reducoes por barra (id) nesta fase.
        std::map<int, Reduction> reductions;
    };

    using Solver = std::function<Structura::App::SolverBridge::Result(const Structura::App::SolverBridge::ModelData &)>;

    struct Output {
        QVector<Structura::App::SolverBridge::Result> perStage;   ///< incremento de cada fase
        Structura::App::SolverBridge::Result accumulated;
        QStringList notes;
    };

    static constexpr double kInactiveStiffness = 1e-6;

    /// Modelo da fase: base + cargas da fase + secoes reduzidas (clones) e barras inativas amolecidas.
    static Structura::App::SolverBridge::ModelData stageModel(const Structura::App::SolverBridge::ModelData &base, const Stage &stage);

    static Output run(const Structura::App::SolverBridge::ModelData &base, const QVector<Stage> &stages, const Solver &solver);
};

} // namespace Structura::App::Services
