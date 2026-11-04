#pragma once

#include "app/SolverBridge.h"
#include "viz/postprocessor/PostProcessorDataTypes.h"

#include <utility>
#include <vector>

namespace Structura::UI {

/// Monta, para cada barra do resultado, o elemento do pos-processador: eixos locais da barra, esforcos de extremidade (convencao estrutural,
/// no I = -lf) e as cargas dos casos de `terms` ponderadas pelo fator. Serve aos diagramas e as comparacoes (esforcos ao longo da barra).
std::vector<Structura::Visualization::BeamElementResults> buildBeamResults(
    const Structura::App::SolverBridge::ModelData& geometry,
    const Structura::App::SolverBridge::Result& result,
    const std::vector<std::pair<const Structura::App::SolverBridge::ModelData*, double>>& terms);

} // namespace Structura::UI
