#include "interface_dll.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "solver_core.h"

namespace {
struct GlobalOptions {
    bool secondOrder = false;
    int maxIterations = 30;
    double tolerance = 1e-7;
};
GlobalOptions g_options;
std::vector<int> g_tensionOnly;
int g_bucklingModes = 0;
int g_modalModes = 0;
std::vector<std::pair<int, double>> g_modalDensities;   // material id, density
std::vector<std::pair<int, double>> g_modalMasses;      // node id, lumped mass
SolverOutput g_lastOutput;
std::vector<ShellElementData> g_shells;
std::vector<ShellLoadData> g_shellLoads;
std::vector<TieData> g_ties;
std::vector<SpringData> g_springs;
} // namespace

extern "C" void structura_clear_shells() {
    g_shells.clear();
    g_shellLoads.clear();
    g_ties.clear();
    g_springs.clear();
}

extern "C" int structura_set_shells(
    int shellCount, const int* shellIds, const int* shellNodeIds4, const double* shellE, const double* shellNu, const double* shellDensity,
    const double* shellThickness, int loadCount, const int* loadShellIds, const int* loadKinds, const double* loadValues4, int tieCount,
    const int* tieSlaveIds, const int* tieMasterCounts, const int* tieMasterIds, const double* tieWeights, const int* tieDofMasks, int springCount,
    const int* springNodeIds, const double* springK6, const int* springCompressionOnly, const double* springNormals3) {
    structura_clear_shells();
    if (shellCount < 0 || loadCount < 0 || tieCount < 0 || springCount < 0) {
        return 1;
    }
    for (int i = 0; i < shellCount; ++i) {
        ShellElementData s;
        s.id = shellIds[i];
        for (int k = 0; k < 4; ++k) {
            s.nodeIds[static_cast<size_t>(k)] = shellNodeIds4[i * 4 + k];
        }
        s.E = shellE[i];
        s.nu = shellNu[i];
        s.density = shellDensity != nullptr ? shellDensity[i] : 0.0;
        s.thickness = shellThickness[i];
        g_shells.push_back(s);
    }
    for (int i = 0; i < loadCount; ++i) {
        ShellLoadData l;
        l.shellId = loadShellIds[i];
        l.kind = loadKinds[i];
        for (int k = 0; k < 4; ++k) {
            l.v[static_cast<size_t>(k)] = loadValues4[i * 4 + k];
        }
        g_shellLoads.push_back(l);
    }
    int cursor = 0;
    for (int i = 0; i < tieCount; ++i) {
        TieData t;
        t.slaveId = tieSlaveIds[i];
        for (int k = 0; k < tieMasterCounts[i]; ++k) {
            t.masterIds.push_back(tieMasterIds[cursor + k]);
            t.weights.push_back(tieWeights[cursor + k]);
        }
        cursor += tieMasterCounts[i];
        t.dofMask = tieDofMasks != nullptr ? tieDofMasks[i] : 63;
        g_ties.push_back(std::move(t));
    }
    for (int i = 0; i < springCount; ++i) {
        SpringData sp;
        sp.nodeId = springNodeIds[i];
        for (int k = 0; k < 6; ++k) {
            sp.k[static_cast<size_t>(k)] = springK6[i * 6 + k];
        }
        sp.compressionOnly = springCompressionOnly[i] != 0;
        for (int k = 0; k < 3; ++k) {
            sp.normal[static_cast<size_t>(k)] = springNormals3[i * 3 + k];
        }
        g_springs.push_back(sp);
    }
    return 0;
}

extern "C" int structura_get_shell_results(int shellCount, double* values40, double* axes9) {
    const int available = static_cast<int>(g_lastOutput.shells.size());
    if (values40 == nullptr || axes9 == nullptr || shellCount != available) {
        return 0;
    }
    for (int i = 0; i < available; ++i) {
        for (int k = 0; k < 40; ++k) {
            values40[i * 40 + k] = g_lastOutput.shells[static_cast<size_t>(i)].values[static_cast<size_t>(k)];
        }
        for (int k = 0; k < 9; ++k) {
            axes9[i * 9 + k] = g_lastOutput.shells[static_cast<size_t>(i)].axes[static_cast<size_t>(k)];
        }
    }
    return available;
}

extern "C" void structura_set_options(int secondOrder, int maxIterations, double tolerance) {
    g_options.secondOrder = secondOrder != 0;
    g_options.maxIterations = maxIterations > 0 ? maxIterations : 30;
    g_options.tolerance = tolerance > 0.0 ? tolerance : 1e-7;
}

extern "C" void structura_set_tension_only(int count, const int* memberIds) {
    g_tensionOnly.assign(memberIds, memberIds + std::max(0, count));
}

extern "C" void structura_set_buckling(int modes) {
    g_bucklingModes = std::max(0, modes);
}

extern "C" void structura_set_modal(int modes, int materialCount, const int* materialIds, const double* densities, int massCount, const int* massNodeIds, const double* masses) {
    g_modalModes = std::max(0, modes);
    g_modalDensities.clear();
    g_modalMasses.clear();
    for (int i = 0; i < materialCount && materialIds && densities; ++i) {
        g_modalDensities.emplace_back(materialIds[i], densities[i]);
    }
    for (int i = 0; i < massCount && massNodeIds && masses; ++i) {
        g_modalMasses.emplace_back(massNodeIds[i], masses[i]);
    }
}

extern "C" int structura_get_modal(int maxModes, int nodeCount, double* omega, double* massRatio3, double* shapes) {
    const int available = static_cast<int>(g_lastOutput.modalOmega.size());
    const int count = std::min(available, std::max(0, maxModes));
    if (omega == nullptr || massRatio3 == nullptr || shapes == nullptr || static_cast<int>(g_lastOutput.nodes.size()) != nodeCount) {
        return 0;
    }
    for (int m = 0; m < count; ++m) {
        omega[m] = g_lastOutput.modalOmega[static_cast<size_t>(m)];
        for (int d = 0; d < 3; ++d) {
            massRatio3[m * 3 + d] = g_lastOutput.modalMassRatio[static_cast<size_t>(m)][static_cast<size_t>(d)];
        }
        const auto& shape = g_lastOutput.modalShapes[static_cast<size_t>(m)];
        for (int i = 0; i < nodeCount * 6; ++i) {
            shapes[static_cast<size_t>(m) * static_cast<size_t>(nodeCount) * 6U + static_cast<size_t>(i)] = shape[static_cast<size_t>(i)];
        }
    }
    return count;
}

extern "C" int structura_get_buckling(int maxModes, int nodeCount, double* factors, double* modes) {
    const int available = static_cast<int>(g_lastOutput.bucklingFactors.size());
    const int count = std::min(available, std::max(0, maxModes));
    if (factors == nullptr || modes == nullptr || static_cast<int>(g_lastOutput.nodes.size()) != nodeCount) {
        return 0;
    }
    for (int m = 0; m < count; ++m) {
        factors[m] = g_lastOutput.bucklingFactors[static_cast<size_t>(m)];
        const auto& shape = g_lastOutput.bucklingModes[static_cast<size_t>(m)];
        for (int i = 0; i < nodeCount * 6; ++i) {
            modes[static_cast<size_t>(m) * static_cast<size_t>(nodeCount) * 6U + static_cast<size_t>(i)] = shape[static_cast<size_t>(i)];
        }
    }
    return count;
}

namespace {
void writeError(const std::string& message, int errorBufferSize, char* errorBuffer) {
    if (errorBuffer == nullptr || errorBufferSize <= 0) {
        return;
    }

    const size_t maxLength = static_cast<size_t>(errorBufferSize - 1);
    const size_t copyLength = std::min(maxLength, message.size());
    std::memcpy(errorBuffer, message.data(), copyLength);
    errorBuffer[copyLength] = '\0';
}

bool hasRequiredPointer(int count, const void* ptr) {
    return count == 0 || ptr != nullptr;
}
} // namespace

extern "C" int structura_solve_v4(
    int materialCount,
    const int* materialIds,
    const double* materialE,
    const double* materialG,
    int sectionCount,
    const int* sectionIds,
    const double* sectionA,
    const double* sectionIz,
    const double* sectionIy,
    const double* sectionJ,
    int nodeCount,
    const int* nodeIds,
    const double* nodeX,
    const double* nodeY,
    const double* nodeZ,
    const int* nodeRestraints6,
    int memberCount,
    const int* memberIds,
    const int* memberNodeI,
    const int* memberNodeJ,
    const int* memberMaterialId,
    const int* memberSectionId,
    int nodalLoadCount,
    const int* nodalLoadNodeIds,
    const double* nodalLoadValues6,
    int memberLoadCount,
    const int* memberLoadMemberIds,
    const int* memberLoadCoordSystem,
    const double* memberLoadQx,
    const double* memberLoadQy,
    const double* memberLoadQz,
    int memberMomentCount,
    const int* memberMomentMemberIds,
    const int* memberMomentCoordSystem,
    const double* memberMomentMx,
    const double* memberMomentMy,
    const double* memberMomentMz,
    const double* memberSectionRotationDeg,
    const double* memberOffsets4,
    const int* memberReleases12,
    double* outNodeDisplacements6,
    double* outNodeReactions6,
    double* outMemberLocalForces12,
    double* outMemberAuxK3,
    int errorBufferSize,
    char* errorBuffer) {
    if (materialCount < 0 || sectionCount < 0 || nodeCount < 0 || memberCount < 0 ||
        nodalLoadCount < 0 || memberLoadCount < 0 || memberMomentCount < 0) {
        writeError("Counts cannot be negative", errorBufferSize, errorBuffer);
        return 1;
    }

    if (!hasRequiredPointer(materialCount, materialIds) ||
        !hasRequiredPointer(materialCount, materialE) ||
        !hasRequiredPointer(materialCount, materialG) ||
        !hasRequiredPointer(sectionCount, sectionIds) ||
        !hasRequiredPointer(sectionCount, sectionA) ||
        !hasRequiredPointer(sectionCount, sectionIz) ||
        !hasRequiredPointer(sectionCount, sectionIy) ||
        !hasRequiredPointer(sectionCount, sectionJ) ||
        !hasRequiredPointer(nodeCount, nodeIds) ||
        !hasRequiredPointer(nodeCount, nodeX) ||
        !hasRequiredPointer(nodeCount, nodeY) ||
        !hasRequiredPointer(nodeCount, nodeZ) ||
        !hasRequiredPointer(nodeCount, nodeRestraints6) ||
        !hasRequiredPointer(memberCount, memberIds) ||
        !hasRequiredPointer(memberCount, memberNodeI) ||
        !hasRequiredPointer(memberCount, memberNodeJ) ||
        !hasRequiredPointer(memberCount, memberMaterialId) ||
        !hasRequiredPointer(memberCount, memberSectionId) ||
        !hasRequiredPointer(nodalLoadCount, nodalLoadNodeIds) ||
        !hasRequiredPointer(nodalLoadCount, nodalLoadValues6) ||
        !hasRequiredPointer(memberLoadCount, memberLoadMemberIds) ||
        !hasRequiredPointer(memberLoadCount, memberLoadCoordSystem) ||
        !hasRequiredPointer(memberLoadCount, memberLoadQx) ||
        !hasRequiredPointer(memberLoadCount, memberLoadQy) ||
        !hasRequiredPointer(memberLoadCount, memberLoadQz) ||
        !hasRequiredPointer(memberMomentCount, memberMomentMemberIds) ||
        !hasRequiredPointer(memberMomentCount, memberMomentCoordSystem) ||
        !hasRequiredPointer(memberMomentCount, memberMomentMx) ||
        !hasRequiredPointer(memberMomentCount, memberMomentMy) ||
        !hasRequiredPointer(memberMomentCount, memberMomentMz) ||
        outNodeDisplacements6 == nullptr || outNodeReactions6 == nullptr ||
        outMemberLocalForces12 == nullptr || outMemberAuxK3 == nullptr) {
        writeError("One or more required pointers are null", errorBufferSize, errorBuffer);
        return 2;
    }

    SolverInput input;
    input.secondOrder = g_options.secondOrder;
    input.tensionOnlyMembers = g_tensionOnly;
    input.bucklingModes = g_bucklingModes;
    input.modalModes = g_modalModes;
    input.extraNodalMasses = g_modalMasses;
    input.maxIterations = g_options.maxIterations;
    input.tolerance = g_options.tolerance;
    input.shells = g_shells;
    input.shellLoads = g_shellLoads;
    input.ties = g_ties;
    input.springs = g_springs;
    input.materials.reserve(static_cast<size_t>(materialCount));
    input.sections.reserve(static_cast<size_t>(sectionCount));
    input.nodes.reserve(static_cast<size_t>(nodeCount));
    input.members.reserve(static_cast<size_t>(memberCount));
    input.nodalLoads.reserve(static_cast<size_t>(nodalLoadCount));
    input.memberLoads.reserve(static_cast<size_t>(memberLoadCount));

    for (int i = 0; i < materialCount; ++i) {
        MaterialData material;
        material.id = materialIds[i];
        material.E = materialE[i];
        material.G = materialG[i];
        for (const auto& entry : g_modalDensities) {
            if (entry.first == material.id) {
                material.density = entry.second;
            }
        }
        input.materials.push_back(material);
    }

    for (int i = 0; i < sectionCount; ++i) {
        SectionData section;
        section.id = sectionIds[i];
        section.A = sectionA[i];
        section.Iz = sectionIz[i];
        section.Iy = sectionIy[i];
        section.J = sectionJ[i];
        input.sections.push_back(section);
    }

    for (int i = 0; i < nodeCount; ++i) {
        NodeData node;
        node.id = nodeIds[i];
        node.x = nodeX[i];
        node.y = nodeY[i];
        node.z = nodeZ[i];
        for (int dof = 0; dof < 6; ++dof) {
            node.restraints[static_cast<size_t>(dof)] = nodeRestraints6[i * 6 + dof];
        }
        input.nodes.push_back(node);
    }

    for (int i = 0; i < memberCount; ++i) {
        MemberData member;
        member.id = memberIds[i];
        member.nodeI = memberNodeI[i];
        member.nodeJ = memberNodeJ[i];
        member.materialId = memberMaterialId[i];
        member.sectionId = memberSectionId[i];
        if (memberSectionRotationDeg != nullptr) {
            member.sectionRotationDeg = memberSectionRotationDeg[i];
        }
        if (memberOffsets4 != nullptr) {
            member.offsetY1 = memberOffsets4[i * 4 + 0];
            member.offsetZ1 = memberOffsets4[i * 4 + 1];
            member.offsetY2 = memberOffsets4[i * 4 + 2];
            member.offsetZ2 = memberOffsets4[i * 4 + 3];
        }
        if (memberReleases12 != nullptr) {
            for (int k = 0; k < 12; ++k) {
                member.releases[static_cast<size_t>(k)] = memberReleases12[i * 12 + k];
            }
        }
        input.members.push_back(member);
    }

    for (int i = 0; i < nodalLoadCount; ++i) {
        NodalLoadData load;
        load.nodeId = nodalLoadNodeIds[i];
        for (int dof = 0; dof < 6; ++dof) {
            load.values[static_cast<size_t>(dof)] = nodalLoadValues6[i * 6 + dof];
        }
        input.nodalLoads.push_back(load);
    }

    for (int i = 0; i < memberLoadCount; ++i) {
        MemberLoadData load;
        load.memberId = memberLoadMemberIds[i];
        load.system = (memberLoadCoordSystem[i] == 0) ? DistributedLoadSystem::Local : DistributedLoadSystem::Global;
        load.qx = memberLoadQx[i];
        load.qy = memberLoadQy[i];
        load.qz = memberLoadQz[i];
        input.memberLoads.push_back(load);
    }

    input.memberMoments.reserve(static_cast<size_t>(memberMomentCount));
    for (int i = 0; i < memberMomentCount; ++i) {
        MemberMomentData moment;
        moment.memberId = memberMomentMemberIds[i];
        moment.system = (memberMomentCoordSystem[i] == 0) ? DistributedLoadSystem::Local : DistributedLoadSystem::Global;
        moment.mx = memberMomentMx[i];
        moment.my = memberMomentMy[i];
        moment.mz = memberMomentMz[i];
        input.memberMoments.push_back(moment);
    }

    SolverCore solver;
    SolverOutput output;
    std::string errorMessage;
    if (!solver.run(input, output, errorMessage)) {
        writeError(errorMessage, errorBufferSize, errorBuffer);
        return 3;
    }

    g_lastOutput = output;
    if (static_cast<int>(output.nodes.size()) != nodeCount || static_cast<int>(output.members.size()) != memberCount) {
        writeError("Unexpected solver output dimensions", errorBufferSize, errorBuffer);
        return 4;
    }

    for (int i = 0; i < nodeCount; ++i) {
        for (int dof = 0; dof < 6; ++dof) {
            outNodeDisplacements6[i * 6 + dof] = output.nodes[static_cast<size_t>(i)].displacements[static_cast<size_t>(dof)];
            outNodeReactions6[i * 6 + dof] = output.nodes[static_cast<size_t>(i)].reactions[static_cast<size_t>(dof)];
        }
    }

    for (int i = 0; i < memberCount; ++i) {
        for (int j = 0; j < 12; ++j) {
            outMemberLocalForces12[i * 12 + j] = output.members[static_cast<size_t>(i)].localForces[static_cast<size_t>(j)];
        }

        outMemberAuxK3[i * 3 + 0] = output.members[static_cast<size_t>(i)].auxiliaryK[0];
        outMemberAuxK3[i * 3 + 1] = output.members[static_cast<size_t>(i)].auxiliaryK[1];
        outMemberAuxK3[i * 3 + 2] = output.members[static_cast<size_t>(i)].auxiliaryK[2];
    }

    writeError("", errorBufferSize, errorBuffer);
    return 0;
}

extern "C" int structura_solve_v2(
    int materialCount,
    const int* materialIds,
    const double* materialE,
    const double* materialG,
    int sectionCount,
    const int* sectionIds,
    const double* sectionA,
    const double* sectionIz,
    const double* sectionIy,
    const double* sectionJ,
    int nodeCount,
    const int* nodeIds,
    const double* nodeX,
    const double* nodeY,
    const double* nodeZ,
    const int* nodeRestraints6,
    int memberCount,
    const int* memberIds,
    const int* memberNodeI,
    const int* memberNodeJ,
    const int* memberMaterialId,
    const int* memberSectionId,
    int nodalLoadCount,
    const int* nodalLoadNodeIds,
    const double* nodalLoadValues6,
    int memberLoadCount,
    const int* memberLoadMemberIds,
    const int* memberLoadCoordSystem,
    const double* memberLoadQx,
    const double* memberLoadQy,
    const double* memberLoadQz,
    int memberMomentCount,
    const int* memberMomentMemberIds,
    const int* memberMomentCoordSystem,
    const double* memberMomentMx,
    const double* memberMomentMy,
    const double* memberMomentMz,
    double* outNodeDisplacements6,
    double* outNodeReactions6,
    double* outMemberLocalForces12,
    double* outMemberAuxK3,
    int errorBufferSize,
    char* errorBuffer) {
    return structura_solve_v3(
        materialCount, materialIds, materialE, materialG,
        sectionCount, sectionIds, sectionA, sectionIz, sectionIy, sectionJ,
        nodeCount, nodeIds, nodeX, nodeY, nodeZ, nodeRestraints6,
        memberCount, memberIds, memberNodeI, memberNodeJ, memberMaterialId, memberSectionId,
        nodalLoadCount, nodalLoadNodeIds, nodalLoadValues6,
        memberLoadCount, memberLoadMemberIds, memberLoadCoordSystem,
        memberLoadQx, memberLoadQy, memberLoadQz,
        memberMomentCount, memberMomentMemberIds, memberMomentCoordSystem,
        memberMomentMx, memberMomentMy, memberMomentMz,
        nullptr, nullptr,
        outNodeDisplacements6, outNodeReactions6, outMemberLocalForces12, outMemberAuxK3,
        errorBufferSize, errorBuffer);
}

extern "C" int structura_solve_v3(
    int materialCount,
    const int* materialIds,
    const double* materialE,
    const double* materialG,
    int sectionCount,
    const int* sectionIds,
    const double* sectionA,
    const double* sectionIz,
    const double* sectionIy,
    const double* sectionJ,
    int nodeCount,
    const int* nodeIds,
    const double* nodeX,
    const double* nodeY,
    const double* nodeZ,
    const int* nodeRestraints6,
    int memberCount,
    const int* memberIds,
    const int* memberNodeI,
    const int* memberNodeJ,
    const int* memberMaterialId,
    const int* memberSectionId,
    int nodalLoadCount,
    const int* nodalLoadNodeIds,
    const double* nodalLoadValues6,
    int memberLoadCount,
    const int* memberLoadMemberIds,
    const int* memberLoadCoordSystem,
    const double* memberLoadQx,
    const double* memberLoadQy,
    const double* memberLoadQz,
    int memberMomentCount,
    const int* memberMomentMemberIds,
    const int* memberMomentCoordSystem,
    const double* memberMomentMx,
    const double* memberMomentMy,
    const double* memberMomentMz,
    const double* memberSectionRotationDeg,
    const double* memberOffsets4,
    double* outNodeDisplacements6,
    double* outNodeReactions6,
    double* outMemberLocalForces12,
    double* outMemberAuxK3,
    int errorBufferSize,
    char* errorBuffer) {
    return structura_solve_v4(
        materialCount, materialIds, materialE, materialG,
        sectionCount, sectionIds, sectionA, sectionIz, sectionIy, sectionJ,
        nodeCount, nodeIds, nodeX, nodeY, nodeZ, nodeRestraints6,
        memberCount, memberIds, memberNodeI, memberNodeJ, memberMaterialId, memberSectionId,
        nodalLoadCount, nodalLoadNodeIds, nodalLoadValues6,
        memberLoadCount, memberLoadMemberIds, memberLoadCoordSystem,
        memberLoadQx, memberLoadQy, memberLoadQz,
        memberMomentCount, memberMomentMemberIds, memberMomentCoordSystem,
        memberMomentMx, memberMomentMy, memberMomentMz,
        memberSectionRotationDeg, memberOffsets4, nullptr,
        outNodeDisplacements6, outNodeReactions6, outMemberLocalForces12, outMemberAuxK3,
        errorBufferSize, errorBuffer);
}

extern "C" int structura_solve(
    int materialCount,
    const int* materialIds,
    const double* materialE,
    const double* materialG,
    int sectionCount,
    const int* sectionIds,
    const double* sectionA,
    const double* sectionIz,
    const double* sectionIy,
    const double* sectionJ,
    int nodeCount,
    const int* nodeIds,
    const double* nodeX,
    const double* nodeY,
    const double* nodeZ,
    const int* nodeRestraints6,
    int memberCount,
    const int* memberIds,
    const int* memberNodeI,
    const int* memberNodeJ,
    const int* memberMaterialId,
    const int* memberSectionId,
    int nodalLoadCount,
    const int* nodalLoadNodeIds,
    const double* nodalLoadValues6,
    int memberLoadCount,
    const int* memberLoadMemberIds,
    const int* memberLoadCoordSystem,
    const double* memberLoadQx,
    const double* memberLoadQy,
    const double* memberLoadQz,
    double* outNodeDisplacements6,
    double* outNodeReactions6,
    double* outMemberLocalForces12,
    double* outMemberAuxK3,
    int errorBufferSize,
    char* errorBuffer) {
    return structura_solve_v2(
        materialCount, materialIds, materialE, materialG,
        sectionCount, sectionIds, sectionA, sectionIz, sectionIy, sectionJ,
        nodeCount, nodeIds, nodeX, nodeY, nodeZ, nodeRestraints6,
        memberCount, memberIds, memberNodeI, memberNodeJ, memberMaterialId, memberSectionId,
        nodalLoadCount, nodalLoadNodeIds, nodalLoadValues6,
        memberLoadCount, memberLoadMemberIds, memberLoadCoordSystem,
        memberLoadQx, memberLoadQy, memberLoadQz,
        0, nullptr, nullptr, nullptr, nullptr, nullptr,
        outNodeDisplacements6, outNodeReactions6, outMemberLocalForces12, outMemberAuxK3,
        errorBufferSize, errorBuffer);
}
