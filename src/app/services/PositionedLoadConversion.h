#pragma once

#include "app/SolverBridge.h"
#include "viz/postprocessor/PostProcessorDataTypes.h"

#include <unordered_map>

namespace Structura::App::Services {

/// Converte as cargas gerais de barra de um caso (ponderadas pelo fator da combinacao) nas cargas com posicao do pos-processador.
/// A deformacao imposta (temperatura/retracao) nao entra: nao e carga transversal e nao altera o equilibrio das secoes.
inline void collectPositionedLoads(const SolverBridge::ModelData &model, double factor,
                                   std::unordered_multimap<int, Structura::Visualization::PositionedLoad> &out)
{
    using Structura::Visualization::PositionedLoad;
    for (const auto &action : model.memberActions) {
        if (action.kind < 0 || action.kind > 2) {
            continue;
        }
        PositionedLoad load;
        load.kind = action.kind == 0 ? PositionedLoad::Kind::Trapezoid
                  : action.kind == 1 ? PositionedLoad::Kind::PointForce
                                     : PositionedLoad::Kind::PointMoment;
        load.a = action.a;
        load.b = action.b;
        for (std::size_t i = 0; i < 3; ++i) {
            load.v1[i] = action.v1[i] * factor;
            load.v2[i] = action.v2[i] * factor;
        }
        load.isLocalSystem = action.coordSystem == SolverBridge::MemberLoadCoordSystem::Local;
        out.emplace(action.memberId, load);
    }
}

} // namespace Structura::App::Services
