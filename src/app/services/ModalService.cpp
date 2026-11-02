#include "ModalService.h"

#include "app/services/LoadMassService.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>

namespace Structura::App::Services {

std::vector<std::pair<int, double>> ModalService::massesFromLoads(const SolverBridge::ModelData &model)
{
    return LoadMassService::massesFromLoads(model);
}

SolverBridge::Result ModalService::run(SolverBridge::ModelData model, int modes, bool includeMaterialMass)
{
    model.secondOrder = false;
    model.modalModes = std::max(1, modes);
    model.extraMasses = massesFromLoads(model);
    for (auto &material : model.materials) {
        if (!includeMaterialMass) {
            material.density = 0.0;
        }
    }
    // as cargas nao fazem parte da analise modal; mantidas apenas para o solver estatico (nao afetam K)
    SolverBridge solver;
    return solver.solve(model);
}

} // namespace Structura::App::Services
