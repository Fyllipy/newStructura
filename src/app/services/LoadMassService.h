#pragma once

#include "app/SolverBridge.h"

#include <utility>
#include <vector>

namespace Structura::App::Services {

/// Massas nodais a partir das cargas gravitacionais do modelo (usadas pelas forcas nocionais e pela analise modal).
class LoadMassService
{
public:
    static constexpr double kGravity = 9.80665;

    /// Massas nodais concentradas (id do no, massa) a partir das cargas verticais (-Z) do modelo.
    static std::vector<std::pair<int, double>> massesFromLoads(const Structura::App::SolverBridge::ModelData &model);
};

} // namespace Structura::App::Services
