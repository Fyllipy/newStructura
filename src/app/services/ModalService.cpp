#include "ModalService.h"

#include <cmath>
#include <map>
#include <unordered_map>

namespace Structura::App::Services {

std::vector<std::pair<int, double>> ModalService::massesFromLoads(const SolverBridge::ModelData &model)
{
    std::map<int, double> mass;
    for (const auto &load : model.nodalLoads) {
        const double fz = load.values[2];
        if (fz < 0.0) {
            mass[load.nodeId] += -fz / kGravity;
        }
    }
    std::unordered_map<int, const SolverBridge::NodeData *> nodes;
    for (const auto &n : model.nodes) {
        nodes[n.id] = &n;
    }
    std::unordered_map<int, const SolverBridge::MemberData *> members;
    for (const auto &m : model.members) {
        members[m.id] = &m;
    }
    for (const auto &load : model.memberLoads) {
        if (load.coordSystem != SolverBridge::MemberLoadCoordSystem::Global || load.qz >= 0.0) {
            continue;   // so cargas gravitacionais globais viram massa
        }
        const auto mi = members.find(load.memberId);
        if (mi == members.end()) {
            continue;
        }
        const auto *a = nodes[mi->second->nodeI];
        const auto *b = nodes[mi->second->nodeJ];
        if (!a || !b) {
            continue;
        }
        const double length = std::sqrt((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) + (a->z - b->z) * (a->z - b->z));
        const double half = -load.qz * length / kGravity / 2.0;
        mass[mi->second->nodeI] += half;
        mass[mi->second->nodeJ] += half;
    }
    return {mass.begin(), mass.end()};
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
