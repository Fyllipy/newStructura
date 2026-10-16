#include "solver_core.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>

#include "load.h"
#include "material.h"
#include "member.h"
#include "node.h"
#include "section.h"

namespace {
constexpr double kAssemblyTolerance = 1e-12;

int toGlobalDof(const std::unordered_map<int, int>& nodeIndexById, int nodeId, int localDof) {
    const auto it = nodeIndexById.find(nodeId);
    if (it == nodeIndexById.end()) {
        throw std::runtime_error("Node not found while mapping DOF: " + std::to_string(nodeId));
    }
    return it->second * 6 + localDof;
}

void assembleGlobalStiffness(
    const std::vector<std::shared_ptr<Member>>& members,
    const std::unordered_map<int, int>& nodeIndexById,
    int totalDof,
    Eigen::SparseMatrix<double>& globalStiffness) {
    std::vector<Eigen::Triplet<double>> triplets;
    triplets.reserve(static_cast<size_t>(members.size()) * 144U);

    for (const auto& member : members) {
        const Eigen::MatrixXd memberStiffness = member->getGlobalStiffnessMatrix();
        std::array<int, 12> dofIndices{};

        const int nodeI = member->getNodeI()->getId();
        const int nodeJ = member->getNodeJ()->getId();
        for (int i = 0; i < 6; ++i) {
            dofIndices[i] = toGlobalDof(nodeIndexById, nodeI, i);
            dofIndices[i + 6] = toGlobalDof(nodeIndexById, nodeJ, i);
        }

        for (int i = 0; i < 12; ++i) {
            for (int j = 0; j < 12; ++j) {
                const double value = memberStiffness(i, j);
                if (std::abs(value) > kAssemblyTolerance) {
                    triplets.emplace_back(dofIndices[static_cast<size_t>(i)], dofIndices[static_cast<size_t>(j)], value);
                }
            }
        }
    }

    globalStiffness.resize(totalDof, totalDof);
    globalStiffness.setZero();
    globalStiffness.setFromTriplets(triplets.begin(), triplets.end());
    globalStiffness.makeCompressed();
}

void assembleGlobalLoad(
    const std::vector<std::shared_ptr<Load>>& loads,
    const std::unordered_map<int, int>& nodeIndexById,
    const std::unordered_map<int, std::shared_ptr<Member>>& memberById,
    int totalDof,
    Eigen::VectorXd& globalLoad) {
    globalLoad = Eigen::VectorXd::Zero(totalDof);

    for (const auto& load : loads) {
        if (const auto nodalLoad = std::dynamic_pointer_cast<NodalLoad>(load)) {
            for (int localDof = 0; localDof < 6; ++localDof) {
                const int globalDof = toGlobalDof(nodeIndexById, nodalLoad->getNodeId(), localDof);
                globalLoad(globalDof) += nodalLoad->getForce(localDof);
            }
        }

        if (const auto distLoad = std::dynamic_pointer_cast<DistributedLoad>(load)) {
            const auto memberIt = memberById.find(distLoad->getMemberId());
            if (memberIt == memberById.end()) {
                throw std::runtime_error("Distributed load references unknown member: " + std::to_string(distLoad->getMemberId()));
            }

            const auto& member = memberIt->second;
            const auto equivalentForces = distLoad->getEquivalentNodalForces(member.get());
            std::array<int, 12> dofIndices{};

            const int nodeI = member->getNodeI()->getId();
            const int nodeJ = member->getNodeJ()->getId();
            for (int i = 0; i < 6; ++i) {
                dofIndices[i] = toGlobalDof(nodeIndexById, nodeI, i);
                dofIndices[i + 6] = toGlobalDof(nodeIndexById, nodeJ, i);
            }

            for (int i = 0; i < 12; ++i) {
                globalLoad(dofIndices[static_cast<size_t>(i)]) += equivalentForces[static_cast<size_t>(i)];
            }
        }

        if (const auto distMoment = std::dynamic_pointer_cast<DistributedMoment>(load)) {
            const auto memberIt = memberById.find(distMoment->getMemberId());
            if (memberIt == memberById.end()) {
                throw std::runtime_error("Distributed moment references unknown member: " + std::to_string(distMoment->getMemberId()));
            }

            const auto& member = memberIt->second;
            const auto equivalentForces = distMoment->getEquivalentNodalForces(member.get());
            const int nodeI = member->getNodeI()->getId();
            const int nodeJ = member->getNodeJ()->getId();
            for (int i = 0; i < 6; ++i) {
                globalLoad(toGlobalDof(nodeIndexById, nodeI, i)) += equivalentForces[static_cast<size_t>(i)];
                globalLoad(toGlobalDof(nodeIndexById, nodeJ, i)) += equivalentForces[static_cast<size_t>(i + 6)];
            }
        }
    }
}

void applyBoundaryConditions(
    const std::vector<int>& restrainedDof,
    Eigen::SparseMatrix<double>& stiffness,
    Eigen::VectorXd& loadVector) {
    const int totalDof = static_cast<int>(loadVector.size());
    if (static_cast<int>(restrainedDof.size()) == totalDof) {
        throw std::runtime_error("No free DOF available after restraints");
    }

    std::vector<bool> isRestrained(static_cast<size_t>(totalDof), false);
    for (const int dof : restrainedDof) {
        if (dof < 0 || dof >= totalDof) {
            throw std::runtime_error("Restrained DOF out of range");
        }
        isRestrained[static_cast<size_t>(dof)] = true;
    }

    std::vector<Eigen::Triplet<double>> triplets;
    triplets.reserve(static_cast<size_t>(stiffness.nonZeros()) + restrainedDof.size());

    for (int col = 0; col < stiffness.outerSize(); ++col) {
        for (Eigen::SparseMatrix<double>::InnerIterator it(stiffness, col); it; ++it) {
            const int row = it.row();
            if (!isRestrained[static_cast<size_t>(row)] && !isRestrained[static_cast<size_t>(col)]) {
                triplets.emplace_back(row, col, it.value());
            }
        }
    }

    for (const int dof : restrainedDof) {
        triplets.emplace_back(dof, dof, 1.0);
        loadVector(dof) = 0.0;
    }

    Eigen::SparseMatrix<double> modified(totalDof, totalDof);
    modified.setFromTriplets(triplets.begin(), triplets.end());
    modified.makeCompressed();
    stiffness = std::move(modified);
}

void fillOutput(
    const std::vector<std::shared_ptr<Node>>& nodes,
    const std::vector<std::shared_ptr<Member>>& members,
    SolverOutput& output) {
    output.nodes.clear();
    output.members.clear();
    output.nodes.reserve(nodes.size());
    output.members.reserve(members.size());

    for (const auto& node : nodes) {
        NodeResult nodeResult;
        nodeResult.id = node->getId();
        for (int i = 0; i < 6; ++i) {
            nodeResult.displacements[static_cast<size_t>(i)] = node->getDisplacement(i);
            nodeResult.reactions[static_cast<size_t>(i)] = node->getReaction(i);
        }
        output.nodes.push_back(nodeResult);
    }

    for (const auto& member : members) {
        MemberResult memberResult;
        memberResult.id = member->getId();
        memberResult.nodeI = member->getNodeI()->getId();
        memberResult.nodeJ = member->getNodeJ()->getId();

        const auto& forces = member->getInternalForces();
        for (int i = 0; i < 12; ++i) {
            memberResult.localForces[static_cast<size_t>(i)] = forces[static_cast<size_t>(i)];
        }

        const Eigen::Vector3d k = member->getAuxiliaryVectorK();
        memberResult.auxiliaryK[0] = k.x();
        memberResult.auxiliaryK[1] = k.y();
        memberResult.auxiliaryK[2] = k.z();
        output.members.push_back(memberResult);
    }
}

} // namespace

bool SolverCore::run(const SolverInput& input, SolverOutput& output, std::string& errorMessage) const {
    if (input.tensionOnlyMembers.empty()) {
        return runPass(input, {}, output, errorMessage);
    }

    // Tension-only members: iterate, deactivating members that shorten and reactivating those that elongate.
    std::unordered_map<int, const MemberData*> memberById;
    for (const auto& m : input.members) {
        memberById[m.id] = &m;
    }
    std::unordered_map<int, const NodeData*> nodeById;
    for (const auto& n : input.nodes) {
        nodeById[n.id] = &n;
    }
    std::unordered_map<int, const NodeResult*> resultById;

    std::unordered_set<int> inactive;
    for (int iteration = 0; iteration < 40; ++iteration) {
        if (!runPass(input, inactive, output, errorMessage)) {
            return false;
        }
        resultById.clear();
        for (const auto& n : output.nodes) {
            resultById[n.id] = &n;
        }
        bool changed = false;
        for (const int id : input.tensionOnlyMembers) {
            const auto mIt = memberById.find(id);
            if (mIt == memberById.end()) {
                continue;
            }
            const auto& m = *mIt->second;
            const auto* ni = nodeById[m.nodeI];
            const auto* nj = nodeById[m.nodeJ];
            const auto ri = resultById.find(m.nodeI);
            const auto rj = resultById.find(m.nodeJ);
            if (!ni || !nj || ri == resultById.end() || rj == resultById.end()) {
                continue;
            }
            const double dx = nj->x - ni->x, dy = nj->y - ni->y, dz = nj->z - ni->z;
            const double length = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (length < 1e-12) {
                continue;
            }
            const double elongation = ((rj->second->displacements[0] - ri->second->displacements[0]) * dx
                                       + (rj->second->displacements[1] - ri->second->displacements[1]) * dy
                                       + (rj->second->displacements[2] - ri->second->displacements[2]) * dz) / length;
            const double tolerance = 1e-12 * length;
            const bool isInactive = inactive.count(id) > 0;
            if (!isInactive && elongation < -tolerance) {
                inactive.insert(id);
                changed = true;
            } else if (isInactive && elongation > tolerance) {
                inactive.erase(id);
                changed = true;
            }
        }
        if (!changed) {
            return true;
        }
    }
    errorMessage.clear();
    return true;   // accepts the last pass when the active set keeps oscillating
}

bool SolverCore::runPass(const SolverInput& input, const std::unordered_set<int>& inactiveMembers, SolverOutput& output, std::string& errorMessage) const {
    try {
        if (input.nodes.empty()) {
            throw std::runtime_error("Input has no nodes");
        }

        std::vector<std::shared_ptr<Material>> materials;
        std::vector<std::shared_ptr<Section>> sections;
        std::vector<std::shared_ptr<Node>> nodes;
        std::vector<std::shared_ptr<Member>> members;
        std::vector<std::shared_ptr<Load>> loads;

        std::unordered_map<int, std::shared_ptr<Material>> materialById;
        std::unordered_map<int, std::shared_ptr<Section>> sectionById;
        std::unordered_map<int, std::shared_ptr<Node>> nodeById;
        std::unordered_map<int, std::shared_ptr<Member>> memberById;
        std::unordered_map<int, int> nodeIndexById;

        materials.reserve(input.materials.size());
        for (const auto& materialData : input.materials) {
            auto material = std::make_shared<Material>(materialData.id, materialData.E, materialData.G);
            material->setDensity(materialData.density);
            materials.push_back(material);
            materialById[materialData.id] = material;
        }

        sections.reserve(input.sections.size());
        for (const auto& sectionData : input.sections) {
            auto section = std::make_shared<Section>(sectionData.id, sectionData.A, sectionData.Iz, sectionData.Iy, sectionData.J);
            sections.push_back(section);
            sectionById[sectionData.id] = section;
        }

        nodes.reserve(input.nodes.size());
        for (size_t i = 0; i < input.nodes.size(); ++i) {
            const auto& nodeData = input.nodes[i];
            std::vector<bool> restraints(6, false);
            for (int dof = 0; dof < 6; ++dof) {
                restraints[static_cast<size_t>(dof)] = (nodeData.restraints[static_cast<size_t>(dof)] != 0);
            }

            auto node = std::make_shared<Node>(nodeData.id, nodeData.x, nodeData.y, nodeData.z, restraints);
            nodes.push_back(node);
            nodeById[nodeData.id] = node;
            nodeIndexById[nodeData.id] = static_cast<int>(i);
        }

        members.reserve(input.members.size());
        for (const auto& memberData : input.members) {
            const auto nodeIIt = nodeById.find(memberData.nodeI);
            const auto nodeJIt = nodeById.find(memberData.nodeJ);
            const auto materialIt = materialById.find(memberData.materialId);
            const auto sectionIt = sectionById.find(memberData.sectionId);

            if (nodeIIt == nodeById.end() || nodeJIt == nodeById.end()) {
                throw std::runtime_error("Member references unknown node");
            }
            if (materialIt == materialById.end()) {
                throw std::runtime_error("Member references unknown material");
            }
            if (sectionIt == sectionById.end()) {
                throw std::runtime_error("Member references unknown section");
            }

            auto member = std::make_shared<Member>(
                memberData.id,
                nodeIIt->second,
                nodeJIt->second,
                materialIt->second,
                sectionIt->second);

            member->setSectionRotationDegrees(memberData.sectionRotationDeg);
            member->setEndOffsets(memberData.offsetY1, memberData.offsetZ1,
                                  memberData.offsetY2, memberData.offsetZ2);
            std::array<bool, 12> releaseFlags{};
            for (size_t k = 0; k < 12; ++k) {
                releaseFlags[k] = memberData.releases[k] != 0;
            }
            member->setReleases(releaseFlags);
            if (inactiveMembers.count(memberData.id) > 0) {
                member->setStiffnessFactor(1e-9);
            }

            members.push_back(member);
            memberById[memberData.id] = member;
        }

        loads.reserve(input.nodalLoads.size() + input.memberLoads.size());
        for (const auto& nodalLoadData : input.nodalLoads) {
            std::vector<double> values(6, 0.0);
            for (int i = 0; i < 6; ++i) {
                values[static_cast<size_t>(i)] = nodalLoadData.values[static_cast<size_t>(i)];
            }
            loads.push_back(std::make_shared<NodalLoad>(nodalLoadData.nodeId, values));
        }

        for (const auto& memberLoadData : input.memberLoads) {
            const DistributedLoad::CoordinateSystem system =
                memberLoadData.system == DistributedLoadSystem::Local
                ? DistributedLoad::CoordinateSystem::Local
                : DistributedLoad::CoordinateSystem::Global;

            loads.push_back(std::make_shared<DistributedLoad>(
                memberLoadData.memberId,
                Eigen::Vector3d(memberLoadData.qx, memberLoadData.qy, memberLoadData.qz),
                system));
        }

        for (const auto& momentData : input.memberMoments) {
            const DistributedLoad::CoordinateSystem system =
                momentData.system == DistributedLoadSystem::Local
                ? DistributedLoad::CoordinateSystem::Local
                : DistributedLoad::CoordinateSystem::Global;

            loads.push_back(std::make_shared<DistributedMoment>(
                momentData.memberId,
                Eigen::Vector3d(momentData.mx, momentData.my, momentData.mz),
                system));
        }

        const int totalDof = static_cast<int>(nodes.size()) * 6;
        std::vector<int> restrainedDof;
        restrainedDof.reserve(static_cast<size_t>(totalDof));
        for (const auto& node : nodes) {
            for (int localDof = 0; localDof < 6; ++localDof) {
                if (node->isRestrained(localDof)) {
                    restrainedDof.push_back(toGlobalDof(nodeIndexById, node->getId(), localDof));
                }
            }
        }

        Eigen::SparseMatrix<double> fullStiffness;
        Eigen::VectorXd fullLoad;
        assembleGlobalStiffness(members, nodeIndexById, totalDof, fullStiffness);
        assembleGlobalLoad(loads, nodeIndexById, memberById, totalDof, fullLoad);

        Eigen::SparseMatrix<double> stiffnessForSolve = fullStiffness;
        Eigen::VectorXd loadForSolve = fullLoad;
        // Graus de liberdade sem rigidez alguma (por exemplo, a rotacao de um no onde todas as barras
        // chegam articuladas) sao fixados automaticamente; se houver carga neles a estrutura e instavel.
        {
            std::vector<bool> alreadyRestrained(static_cast<size_t>(totalDof), false);
            for (const int dof : restrainedDof) {
                alreadyRestrained[static_cast<size_t>(dof)] = true;
            }
            double maxDiagonal = 0.0;
            for (int i = 0; i < totalDof; ++i) {
                maxDiagonal = std::max(maxDiagonal, std::abs(stiffnessForSolve.coeff(i, i)));
            }
            const double threshold = std::max(maxDiagonal * 1e-12, 1e-30);
            for (int i = 0; i < totalDof; ++i) {
                if (alreadyRestrained[static_cast<size_t>(i)] || std::abs(stiffnessForSolve.coeff(i, i)) > threshold) {
                    continue;
                }
                if (std::abs(loadForSolve(i)) > 1e-9) {
                    throw std::runtime_error("Unstable structure: load applied to a degree of freedom without stiffness (node "
                                             + std::to_string(nodes[static_cast<size_t>(i / 6)]->getId())
                                             + ", dof " + std::to_string(i % 6 + 1) + ")");
                }
                restrainedDof.push_back(i);
            }
        }
        applyBoundaryConditions(restrainedDof, stiffnessForSolve, loadForSolve);

        Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> linearSolver;
        linearSolver.compute(stiffnessForSolve);
        if (linearSolver.info() != Eigen::Success) {
            throw std::runtime_error("Failed to decompose global stiffness matrix");
        }

        Eigen::VectorXd displacement = linearSolver.solve(loadForSolve);
        if (linearSolver.info() != Eigen::Success) {
            throw std::runtime_error("Failed to solve linear system");
        }

        const double residual = (stiffnessForSolve * displacement - loadForSolve).norm();
        if (residual > 1e-6) {
            throw std::runtime_error("Linear solve residual is too large: " + std::to_string(residual));
        }

        auto registerMemberLoads = [&]() {
            for (auto& member : members) {
                member->clearDistributedLoads();
            }
            for (const auto& load : loads) {
                if (const auto distributedLoad = std::dynamic_pointer_cast<DistributedLoad>(load)) {
                    memberById.at(distributedLoad->getMemberId())->addDistributedLoad(
                        distributedLoad->getEquivalentNodalForces(memberById.at(distributedLoad->getMemberId()).get()));
                }
                if (const auto distributedMoment = std::dynamic_pointer_cast<DistributedMoment>(load)) {
                    memberById.at(distributedMoment->getMemberId())->addDistributedLoad(
                        distributedMoment->getEquivalentNodalForces(memberById.at(distributedMoment->getMemberId()).get()));
                }
            }
        };
        auto publishDisplacements = [&]() {
            for (const auto& node : nodes) {
                for (int localDof = 0; localDof < 6; ++localDof) {
                    node->setDisplacement(localDof, displacement(toGlobalDof(nodeIndexById, node->getId(), localDof)));
                }
            }
        };

        std::vector<double> bucklingFactors;
        std::vector<std::vector<double>> bucklingShapes;
        if (input.bucklingModes > 0 && !input.secondOrder) {
            registerMemberLoads();
            publishDisplacements();
            Eigen::SparseMatrix<double> k0;
            assembleGlobalStiffness(members, nodeIndexById, totalDof, k0);
            for (auto& member : members) {
                member->calculateInternalForces();
                const auto& f = member->getInternalForces();
                member->setGeometricAxialForce(0.5 * (f[6] - f[0]));
            }
            Eigen::SparseMatrix<double> k1;
            assembleGlobalStiffness(members, nodeIndexById, totalDof, k1);
            for (auto& member : members) {
                member->setGeometricAxialForce(0.0);
            }
            const Eigen::SparseMatrix<double> kg = k1 - k0;
            std::vector<bool> fixedDof(static_cast<size_t>(totalDof), false);
            for (const int dof : restrainedDof) {
                fixedDof[static_cast<size_t>(dof)] = true;
            }
            std::vector<int> freeDofs;
            for (int i = 0; i < totalDof; ++i) {
                if (!fixedDof[static_cast<size_t>(i)]) {
                    freeDofs.push_back(i);
                }
            }
            const int n = static_cast<int>(freeDofs.size());
            if (n > 2200) {
                throw std::runtime_error("Buckling analysis: model too large for the dense eigen solver (" + std::to_string(n) + " free degrees of freedom; limit 2200)");
            }
            std::vector<int> freeIndex(static_cast<size_t>(totalDof), -1);
            for (int i = 0; i < n; ++i) {
                freeIndex[static_cast<size_t>(freeDofs[static_cast<size_t>(i)])] = i;
            }
            Eigen::MatrixXd matA = Eigen::MatrixXd::Zero(n, n);
            Eigen::MatrixXd matB = Eigen::MatrixXd::Zero(n, n);
            for (int col = 0; col < k0.outerSize(); ++col) {
                for (Eigen::SparseMatrix<double>::InnerIterator it(k0, col); it; ++it) {
                    const int r = freeIndex[static_cast<size_t>(it.row())], c = freeIndex[static_cast<size_t>(col)];
                    if (r >= 0 && c >= 0) {
                        matA(r, c) += it.value();
                    }
                }
            }
            for (int col = 0; col < kg.outerSize(); ++col) {
                for (Eigen::SparseMatrix<double>::InnerIterator it(kg, col); it; ++it) {
                    const int r = freeIndex[static_cast<size_t>(it.row())], c = freeIndex[static_cast<size_t>(col)];
                    if (r >= 0 && c >= 0) {
                        matB(r, c) -= it.value();
                    }
                }
            }
            Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::MatrixXd> es(matB, matA);   // B x = mu A x, A positive definite
            if (es.info() != Eigen::Success) {
                throw std::runtime_error("Buckling analysis: the eigenvalue problem failed (unstable or singular stiffness)");
            }
            const Eigen::VectorXd& mu = es.eigenvalues();
            bucklingFactors.clear();
            bucklingShapes.clear();
            for (int k = n - 1; k >= 0 && static_cast<int>(bucklingFactors.size()) < input.bucklingModes; --k) {
                if (mu(k) <= 1e-14) {
                    break;
                }
                std::vector<double> shape(static_cast<size_t>(totalDof), 0.0);
                double maxAbs = 0.0;
                for (int i = 0; i < n; ++i) {
                    const double value = es.eigenvectors()(i, k);
                    shape[static_cast<size_t>(freeDofs[static_cast<size_t>(i)])] = value;
                    if (i % 6 < 3 || true) {
                        maxAbs = std::max(maxAbs, std::abs(value));
                    }
                }
                if (maxAbs > 0.0) {
                    for (double& v : shape) {
                        v /= maxAbs;
                    }
                }
                bucklingFactors.push_back(1.0 / mu(k));
                bucklingShapes.push_back(std::move(shape));
            }
        }

        std::vector<double> modalOmega;
        std::vector<std::vector<double>> modalShapes;
        std::vector<std::array<double, 3>> modalMassRatio;
        if (input.modalModes > 0 && !input.secondOrder) {
            Eigen::SparseMatrix<double> massMatrix(totalDof, totalDof);
            {
                std::vector<Eigen::Triplet<double>> triplets;
                for (const auto& member : members) {
                    const Eigen::MatrixXd mm = member->getGlobalMassMatrix();
                    std::array<int, 12> dofs{};
                    for (int i = 0; i < 6; ++i) {
                        dofs[i] = toGlobalDof(nodeIndexById, member->getNodeI()->getId(), i);
                        dofs[i + 6] = toGlobalDof(nodeIndexById, member->getNodeJ()->getId(), i);
                    }
                    for (int i = 0; i < 12; ++i) {
                        for (int j = 0; j < 12; ++j) {
                            if (std::abs(mm(i, j)) > 0.0) {
                                triplets.emplace_back(dofs[static_cast<size_t>(i)], dofs[static_cast<size_t>(j)], mm(i, j));
                            }
                        }
                    }
                }
                for (const auto& extra : input.extraNodalMasses) {
                    for (int i = 0; i < 3; ++i) {
                        const int dof = toGlobalDof(nodeIndexById, extra.first, i);
                        triplets.emplace_back(dof, dof, extra.second);
                    }
                }
                massMatrix.setFromTriplets(triplets.begin(), triplets.end());
            }
            std::vector<bool> fixedDof(static_cast<size_t>(totalDof), false);
            for (const int dof : restrainedDof) {
                fixedDof[static_cast<size_t>(dof)] = true;
            }
            std::vector<int> freeDofs;
            std::vector<int> freeIndex(static_cast<size_t>(totalDof), -1);
            for (int i = 0; i < totalDof; ++i) {
                if (!fixedDof[static_cast<size_t>(i)]) {
                    freeIndex[static_cast<size_t>(i)] = static_cast<int>(freeDofs.size());
                    freeDofs.push_back(i);
                }
            }
            const int n = static_cast<int>(freeDofs.size());
            if (n > 2200) {
                throw std::runtime_error("Modal analysis: model too large for the dense eigen solver (" + std::to_string(n) + " free degrees of freedom; limit 2200)");
            }
            Eigen::MatrixXd matK = Eigen::MatrixXd::Zero(n, n);
            Eigen::MatrixXd matM = Eigen::MatrixXd::Zero(n, n);
            for (int col = 0; col < fullStiffness.outerSize(); ++col) {
                for (Eigen::SparseMatrix<double>::InnerIterator it(fullStiffness, col); it; ++it) {
                    const int r = freeIndex[static_cast<size_t>(it.row())], c = freeIndex[static_cast<size_t>(col)];
                    if (r >= 0 && c >= 0) {
                        matK(r, c) += it.value();
                    }
                }
            }
            for (int col = 0; col < massMatrix.outerSize(); ++col) {
                for (Eigen::SparseMatrix<double>::InnerIterator it(massMatrix, col); it; ++it) {
                    const int r = freeIndex[static_cast<size_t>(it.row())], c = freeIndex[static_cast<size_t>(col)];
                    if (r >= 0 && c >= 0) {
                        matM(r, c) += it.value();
                    }
                }
            }
            double totalMass[3] = {0.0, 0.0, 0.0};
            std::vector<Eigen::VectorXd> influence(3, Eigen::VectorXd::Zero(totalDof));
            for (int d = 0; d < 3; ++d) {
                for (int node = 0; node < static_cast<int>(nodes.size()); ++node) {
                    influence[static_cast<size_t>(d)](node * 6 + d) = 1.0;
                }
                totalMass[d] = influence[static_cast<size_t>(d)].dot(massMatrix * influence[static_cast<size_t>(d)]);
            }
            if (totalMass[0] <= 0.0) {
                throw std::runtime_error("Modal analysis: the model has no mass (set the material density / unit weight)");
            }
            Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::MatrixXd> es(matM, matK);   // M x = mu K x, K positive definite
            if (es.info() != Eigen::Success) {
                throw std::runtime_error("Modal analysis: the eigenvalue problem failed (unstable or singular stiffness)");
            }
            const Eigen::VectorXd& mu = es.eigenvalues();
            const double muMax = mu.size() > 0 ? mu(mu.size() - 1) : 0.0;
            for (int k = n - 1; k >= 0 && static_cast<int>(modalOmega.size()) < input.modalModes; --k) {
                if (mu(k) <= muMax * 1e-14 || mu(k) <= 0.0) {
                    break;
                }
                Eigen::VectorXd full = Eigen::VectorXd::Zero(totalDof);
                for (int i = 0; i < n; ++i) {
                    full(freeDofs[static_cast<size_t>(i)]) = es.eigenvectors()(i, k);
                }
                const double generalizedMass = full.dot(massMatrix * full);
                std::array<double, 3> ratio{0.0, 0.0, 0.0};
                for (int d = 0; d < 3; ++d) {
                    const double gamma = full.dot(massMatrix * influence[static_cast<size_t>(d)]);
                    ratio[static_cast<size_t>(d)] = (generalizedMass > 0.0 && totalMass[d] > 0.0) ? gamma * gamma / generalizedMass / totalMass[d] : 0.0;
                }
                const double maxAbs = full.cwiseAbs().maxCoeff();
                std::vector<double> shape(static_cast<size_t>(totalDof), 0.0);
                for (int i = 0; i < totalDof; ++i) {
                    shape[static_cast<size_t>(i)] = maxAbs > 0.0 ? full(i) / maxAbs : 0.0;
                }
                modalOmega.push_back(1.0 / std::sqrt(mu(k)));
                modalShapes.push_back(std::move(shape));
                modalMassRatio.push_back(ratio);
            }
        }

        if (input.secondOrder) {
            registerMemberLoads();
            bool converged = false;
            for (int iteration = 0; iteration < std::max(1, input.maxIterations); ++iteration) {
                publishDisplacements();
                for (auto& member : members) {
                    member->calculateInternalForces();
                    const auto& f = member->getInternalForces();
                    // mean axial force (tension > 0): section force at J is +f[6], at I is -f[0]
                    member->setGeometricAxialForce(0.5 * (f[6] - f[0]));
                }
                assembleGlobalStiffness(members, nodeIndexById, totalDof, fullStiffness);
                stiffnessForSolve = fullStiffness;
                loadForSolve = fullLoad;
                applyBoundaryConditions(restrainedDof, stiffnessForSolve, loadForSolve);
                linearSolver.compute(stiffnessForSolve);
                if (linearSolver.info() != Eigen::Success) {
                    throw std::runtime_error("Second-order analysis: failed to decompose the stiffness matrix");
                }
                if (linearSolver.vectorD().minCoeff() <= 0.0) {
                    throw std::runtime_error("Second-order analysis: the structure is unstable under these loads (critical load reached or exceeded)");
                }
                const Eigen::VectorXd next = linearSolver.solve(loadForSolve);
                const double change = (next - displacement).norm() / std::max(next.norm(), 1e-30);
                displacement = next;
                if (change < input.tolerance) {
                    converged = true;
                    break;
                }
            }
            if (!converged) {
                throw std::runtime_error("Second-order analysis did not converge (loads close to the critical load)");
            }
        }

        for (const auto& node : nodes) {
            for (int localDof = 0; localDof < 6; ++localDof) {
                const int globalDof = toGlobalDof(nodeIndexById, node->getId(), localDof);
                node->setDisplacement(localDof, displacement(globalDof));
            }
        }

        const Eigen::VectorXd reactionVector = fullStiffness * displacement - fullLoad;
        for (const auto& node : nodes) {
            for (int localDof = 0; localDof < 6; ++localDof) {
                if (node->isRestrained(localDof)) {
                    const int globalDof = toGlobalDof(nodeIndexById, node->getId(), localDof);
                    node->setReaction(localDof, reactionVector(globalDof));
                }
            }
        }

        for (auto& member : members) {
            member->clearDistributedLoads();
        }

        for (const auto& load : loads) {
            if (const auto distributedLoad = std::dynamic_pointer_cast<DistributedLoad>(load)) {
                const auto memberIt = memberById.find(distributedLoad->getMemberId());
                if (memberIt == memberById.end()) {
                    throw std::runtime_error("Distributed load references unknown member in post-processing");
                }
                memberIt->second->addDistributedLoad(distributedLoad->getEquivalentNodalForces(memberIt->second.get()));
            }
            if (const auto distributedMoment = std::dynamic_pointer_cast<DistributedMoment>(load)) {
                const auto memberIt = memberById.find(distributedMoment->getMemberId());
                if (memberIt == memberById.end()) {
                    throw std::runtime_error("Distributed moment references unknown member in post-processing");
                }
                memberIt->second->addDistributedLoad(distributedMoment->getEquivalentNodalForces(memberIt->second.get()));
            }
        }

        for (auto& member : members) {
            member->calculateInternalForces();
        }

        fillOutput(nodes, members, output);
        output.bucklingFactors = bucklingFactors;
        output.bucklingModes = bucklingShapes;
        output.modalOmega = modalOmega;
        output.modalShapes = modalShapes;
        output.modalMassRatio = modalMassRatio;
        errorMessage.clear();
        return true;
    }
    catch (const std::exception& ex) {
        errorMessage = ex.what();
        return false;
    }
}
