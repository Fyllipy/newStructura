#include "ModalService.h"

#include <algorithm>
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
    // cargas gerais de barra com posicao: so as gravitacionais globais viram massa, repartidas entre os nos pela posicao da resultante
    for (const auto &action : model.memberActions) {
        if (action.coordSystem != SolverBridge::MemberLoadCoordSystem::Global || (action.kind != 0 && action.kind != 1)) {
            continue;
        }
        const auto mi = members.find(action.memberId);
        if (mi == members.end()) {
            continue;
        }
        const auto *a = nodes[mi->second->nodeI];
        const auto *b = nodes[mi->second->nodeJ];
        if (!a || !b) {
            continue;
        }
        const double length = std::sqrt((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) + (a->z - b->z) * (a->z - b->z));
        if (length <= 0.0) {
            continue;
        }
        double resultant = 0.0, position = action.a;
        if (action.kind == 1) {
            resultant = action.v1[2];
        } else {
            const double q1 = action.v1[2], q2 = action.v2[2], span = std::abs(action.b - action.a);
            resultant = 0.5 * (q1 + q2) * span;
            if (std::abs(q1 + q2) > 0.0) {
                position = std::min(action.a, action.b) + span * (q1 + 2.0 * q2) / (3.0 * (q1 + q2));
            }
            if (q1 > 0.0 || q2 > 0.0) {
                continue;   // carga que muda de sentido ou ascendente: nao e peso
            }
        }
        if (resultant >= 0.0) {
            continue;
        }
        const double share = std::clamp(position / length, 0.0, 1.0);
        const double total = -resultant / kGravity;
        mass[mi->second->nodeI] += total * (1.0 - share);
        mass[mi->second->nodeJ] += total * share;
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
