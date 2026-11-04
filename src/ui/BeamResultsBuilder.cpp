#include "BeamResultsBuilder.h"

#include "app/services/PositionedLoadConversion.h"
#include "viz/LocalCoordinateSystem.h"

#include <cmath>
#include <unordered_map>

namespace Structura::UI {

std::vector<Structura::Visualization::BeamElementResults> buildBeamResults(
    const Structura::App::SolverBridge::ModelData& geometry,
    const Structura::App::SolverBridge::Result& result,
    const std::vector<std::pair<const Structura::App::SolverBridge::ModelData*, double>>& terms)
{
    // Cargas distribuídas e momentos por barra (todos os casos do termo, ponderados)
    std::unordered_multimap<int, Structura::Visualization::UniformDistributedLoad> loadMap;
    std::unordered_multimap<int, Structura::Visualization::UniformDistributedMoment> momentMap;
    std::unordered_multimap<int, Structura::Visualization::PositionedLoad> positionedMap;
    for (const auto& term : terms) {
        if (!term.first) {
            continue;
        }
        const double factor = term.second;
        Structura::App::Services::collectPositionedLoads(*term.first, factor, positionedMap);
        for (const auto& ml : term.first->memberLoads) {
            Structura::Visualization::UniformDistributedLoad udl;
            udl.qx = ml.qx * factor;
            udl.qy = ml.qy * factor;
            udl.qz = ml.qz * factor;
            udl.isLocalSystem =
                (ml.coordSystem == Structura::App::SolverBridge::MemberLoadCoordSystem::Local);
            loadMap.emplace(ml.memberId, udl);
        }
        for (const auto& mm : term.first->memberMoments) {
            Structura::Visualization::UniformDistributedMoment udm;
            udm.mx = mm.mx * factor;
            udm.my = mm.my * factor;
            udm.mz = mm.mz * factor;
            udm.isLocalSystem =
                (mm.coordSystem == Structura::App::SolverBridge::MemberLoadCoordSystem::Local);
            momentMap.emplace(mm.memberId, udm);
        }
    }

    // Mapa rápido: nodeId → coordenadas
    std::unordered_map<int, std::array<double, 3>> nodeMap;
    for (const auto& nd : geometry.nodes) {
        nodeMap[nd.id] = {nd.x, nd.y, nd.z};
    }

    // Mapa rápido: memberId → MemberData (para nodeI/nodeJ)
    std::unordered_map<int, Structura::App::SolverBridge::MemberData> memberMap;
    for (const auto& mb : geometry.members) {
        memberMap[mb.id] = mb;
    }

    // Construir BeamElementResults para cada MemberResult
    Structura::Geometry::DefaultLocalAxisProvider lcsProvider;
    std::vector<Structura::Visualization::BeamElementResults> postResults;
    postResults.reserve(result.memberResults.size());

    for (const auto& mr : result.memberResults) {
        const auto mbIt = memberMap.find(mr.memberId);
        if (mbIt == memberMap.end()) continue;
        const auto& mb = mbIt->second;

        const auto n1It = nodeMap.find(mb.nodeI);
        const auto n2It = nodeMap.find(mb.nodeJ);
        if (n1It == nodeMap.end() || n2It == nodeMap.end()) continue;

        Structura::Visualization::BeamElementResults ber;
        ber.externalId = mr.memberId;
        ber.node1 = n1It->second;
        ber.node2 = n2It->second;

        // Eixos locais: se o solver forneceu auxK, ele é o vetor vAux, não um ponto global.
        const auto& ak = mr.auxK;
        if (ak[0] != 0.0 || ak[1] != 0.0 || ak[2] != 0.0) {
            // Calcula xLocal
            double dx = ber.node2[0] - ber.node1[0];
            double dy = ber.node2[1] - ber.node1[1];
            double dz = ber.node2[2] - ber.node1[2];
            double len = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (len > 1e-9) {
                ber.xLocal = {dx/len, dy/len, dz/len};
            } else {
                ber.xLocal = {1.0, 0.0, 0.0};
            }

            // zLocal = xLocal x auxK
            auto cross = [](const std::array<double,3>& a, const std::array<double,3>& b) -> std::array<double,3> {
                return {a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0]};
            };
            std::array<double, 3> vAux = {ak[0], ak[1], ak[2]};
            auto zL = cross(ber.xLocal, vAux);
            double zLen = std::sqrt(zL[0]*zL[0] + zL[1]*zL[1] + zL[2]*zL[2]);
            if (zLen > 1e-9) {
                ber.zLocal = {zL[0]/zLen, zL[1]/zLen, zL[2]/zLen};
            } else {
                ber.zLocal = {0.0, 0.0, 1.0};
            }

            // yLocal = zLocal x xLocal
            ber.yLocal = cross(ber.zLocal, ber.xLocal);
        } else {
            // Fallback se auxK não estiver presente
            const auto lcs = lcsProvider.computeLCS(ber.node1, ber.node2, std::nullopt);
            ber.xLocal = lcs.xPrime;
            ber.yLocal = lcs.yPrime;
            ber.zLocal = lcs.zPrime;
        }

        // Excentricidade da secao: os esforcos atuam no eixo da barra (centroide), deslocado do no.
        if (mb.offsetY1 != 0.0 || mb.offsetZ1 != 0.0 || mb.offsetY2 != 0.0 || mb.offsetZ2 != 0.0) {
            for (std::size_t k = 0; k < 3; ++k) {
                ber.node1[k] += mb.offsetY1 * ber.yLocal[k] + mb.offsetZ1 * ber.zLocal[k];
                ber.node2[k] += mb.offsetY2 * ber.yLocal[k] + mb.offsetZ2 * ber.zLocal[k];
            }
        }

        // Esforços internos nodais mapeados para a convenção estrutural PURA:
        // Nó I (face negativa) = -lf, Nó J (face positiva) = +lf para TODOS os eixos.
        // A inversão visual de Ftool (tração na fibra) é feita exclusivamente no DiagramMeshBuilder.
        const auto& lf = mr.localForces;
        ber.endForces = {
            -lf[0], -lf[1], -lf[2], -lf[3], -lf[4], -lf[5],   // nó i (N, Vy, Vz, Mt, My, Mz)
             lf[6],  lf[7],  lf[8],  lf[9],  lf[10], lf[11]   // nó j (N, Vy, Vz, Mt, My, Mz)
        };

        const auto loadRange = loadMap.equal_range(mr.memberId);
        for (auto it = loadRange.first; it != loadRange.second; ++it) {
            ber.distributedLoads.push_back(it->second);
        }
        const auto momentRange = momentMap.equal_range(mr.memberId);
        for (auto it = momentRange.first; it != momentRange.second; ++it) {
            ber.distributedMoments.push_back(it->second);
        }
        const auto positionedRange = positionedMap.equal_range(mr.memberId);
        for (auto it = positionedRange.first; it != positionedRange.second; ++it) {
            ber.positionedLoads.push_back(it->second);
        }

        postResults.push_back(std::move(ber));
    }
    return postResults;
}

} // namespace Structura::UI
