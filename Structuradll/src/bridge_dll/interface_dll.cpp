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
bool g_shearDeformation = false;
bool g_stabilityFunctions = false;
int g_loadSteps = 0;
struct ShearAreaEntry {
    int sectionId = 0;
    double ay = 0.0, az = 0.0;
};
std::vector<ShearAreaEntry> g_shearAreas;
struct RigidZoneEntry {
    int memberId = 0;
    double lengthI = 0.0, lengthJ = 0.0;
};
std::vector<RigidZoneEntry> g_rigidZones;
struct WarpingSectionEntry {
    int sectionId = 0;
    double cw = 0.0;
};
std::vector<WarpingSectionEntry> g_warpingSections;
struct ShearCenterEntry {
    int sectionId = 0;
    double y = 0.0, z = 0.0;
};
std::vector<ShearCenterEntry> g_shearCenters;
struct WarpingMemberEntry {
    int memberId = 0;
    int modeStart = 0, modeEnd = 0;
};
std::vector<WarpingMemberEntry> g_warpingMembers;
struct TaperEntry {
    int memberId = 0;
    double d1 = 0.0, d2 = 0.0, bf = 0.0, tw = 0.0, tf = 0.0;
    int segments = 8;
};
std::vector<TaperEntry> g_tapers;
std::vector<int> g_tensionOnly;
std::vector<MemberActionData> g_memberActions;
std::vector<EndSpringData> g_endSprings;
std::vector<PlasticHingeData> g_plasticHinges;
std::vector<DiaphragmData> g_diaphragms;
std::vector<RigidLinkData> g_rigidLinks;
std::vector<InclinedSupportData> g_inclinedSupports;
std::vector<LinearConstraintData> g_constraints;
std::vector<LinkData> g_links;
NonlinearOptions g_nonlinear;
int g_linearSolver = 0;
int g_eigenSolver = 0;
int g_eigenDenseLimit = 400;
double g_iterativeTolerance = 1e-10;
int g_iterativeMaxIterations = 5000;
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

extern "C" int structura_set_shell_alpha(int shellCount, const double* alphas) {
    // coeficiente de dilatacao termica por casca, na ordem de structura_set_shells (chamar depois dela)
    if (shellCount != static_cast<int>(g_shells.size()) || (shellCount > 0 && alphas == nullptr)) {
        return 1;
    }
    for (int i = 0; i < shellCount; ++i) {
        g_shells[static_cast<size_t>(i)].alpha = alphas[i];
    }
    return 0;
}

extern "C" int structura_set_shell_orthotropy(int shellCount, const int* flags, const double* E2, const double* nu12, const double* G12, const double* bend1,
                                              const double* bend2, const double* axis3) {
    if (shellCount != static_cast<int>(g_shells.size()) || (shellCount > 0 && (!flags || !E2 || !nu12 || !G12 || !bend1 || !bend2 || !axis3))) {
        return 1;
    }
    for (int i = 0; i < shellCount; ++i) {
        ShellElementData& s = g_shells[static_cast<size_t>(i)];
        s.orthotropic = flags[i] != 0;
        s.E2 = E2[i];
        s.nu12 = nu12[i];
        s.G12 = G12[i];
        s.bend1 = bend1[i];
        s.bend2 = bend2[i];
        s.axis = {axis3[i * 3], axis3[i * 3 + 1], axis3[i * 3 + 2]};
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

extern "C" void structura_set_analysis_options(int shearDeformation, int stabilityFunctions, int loadSteps) {
    g_shearDeformation = shearDeformation != 0;
    g_stabilityFunctions = stabilityFunctions != 0;
    g_loadSteps = std::max(0, loadSteps);
}

extern "C" void structura_set_shear_areas(int count, const int* sectionIds, const double* shearAreaY, const double* shearAreaZ) {
    g_shearAreas.clear();
    for (int i = 0; i < count && sectionIds != nullptr && shearAreaY != nullptr && shearAreaZ != nullptr; ++i) {
        g_shearAreas.push_back({sectionIds[i], shearAreaY[i], shearAreaZ[i]});
    }
}

extern "C" void structura_set_rigid_zones(int count, const int* memberIds, const double* lengthI, const double* lengthJ) {
    g_rigidZones.clear();
    for (int i = 0; i < count && memberIds != nullptr && lengthI != nullptr && lengthJ != nullptr; ++i) {
        g_rigidZones.push_back({memberIds[i], lengthI[i], lengthJ[i]});
    }
}

extern "C" void structura_set_taper(int count, const int* memberIds, const double* d1, const double* d2, const double* bf, const double* tw, const double* tf, const int* segments) {
    g_tapers.clear();
    for (int i = 0; i < count; ++i) {
        g_tapers.push_back({memberIds[i], d1[i], d2[i], bf[i], tw[i], tf[i], segments != nullptr ? segments[i] : 8});
    }
}

extern "C" void structura_set_warping_sections(int count, const int* sectionIds, const double* warpingConstants) {
    g_warpingSections.clear();
    for (int i = 0; i < count; ++i) {
        g_warpingSections.push_back({sectionIds[i], warpingConstants[i]});
    }
}

extern "C" void structura_set_shear_centers(int count, const int* sectionIds, const double* scY, const double* scZ) {
    g_shearCenters.clear();
    for (int i = 0; i < count; ++i) {
        g_shearCenters.push_back({sectionIds[i], scY[i], scZ[i]});
    }
}

extern "C" void structura_set_warping_members(int count, const int* memberIds, const int* modeStart, const int* modeEnd) {
    g_warpingMembers.clear();
    for (int i = 0; i < count; ++i) {
        g_warpingMembers.push_back({memberIds[i], modeStart[i], modeEnd[i]});
    }
}

extern "C" int structura_get_warping(int count, const int* memberIds, double* out4) {
    int found = 0;
    for (int i = 0; i < count; ++i) {
        for (int k = 0; k < 4; ++k) {
            out4[i * 4 + k] = 0.0;
        }
        for (const auto& member : g_lastOutput.members) {
            if (member.id == memberIds[i] && member.hasWarping) {
                out4[i * 4 + 0] = member.warpingRate[0];
                out4[i * 4 + 1] = member.warpingRate[1];
                out4[i * 4 + 2] = member.warpingForce[0];
                out4[i * 4 + 3] = member.warpingForce[1];
                ++found;
                break;
            }
        }
    }
    return found;
}

extern "C" int structura_get_warnings(char* buffer, int bufferSize) {
    std::string joined;
    for (const auto& warning : g_lastOutput.warnings) {
        if (!joined.empty()) {
            joined += '\n';
        }
        joined += warning;
    }
    if (buffer != nullptr && bufferSize > 0) {
        const size_t count = std::min(static_cast<size_t>(bufferSize - 1), joined.size());
        std::memcpy(buffer, joined.data(), count);
        buffer[count] = '\0';
    }
    return static_cast<int>(g_lastOutput.warnings.size());
}

extern "C" void structura_set_tension_only(int count, const int* memberIds) {
    g_tensionOnly.assign(memberIds, memberIds + std::max(0, count));
}

extern "C" void structura_set_member_actions(int count, const int* memberIds, const int* kinds, const int* coordSystems, const double* a, const double* b,
                                             const double* v1_3, const double* v2_3) {
    g_memberActions.clear();
    for (int i = 0; i < count && memberIds && kinds && coordSystems && a && b && v1_3 && v2_3; ++i) {
        MemberActionData action;
        action.memberId = memberIds[i];
        action.kind = kinds[i];
        action.system = coordSystems[i] == 0 ? DistributedLoadSystem::Local : DistributedLoadSystem::Global;
        action.a = a[i];
        action.b = b[i];
        for (size_t k = 0; k < 3; ++k) {
            action.v1[k] = v1_3[i * 3 + static_cast<int>(k)];
            action.v2[k] = v2_3[i * 3 + static_cast<int>(k)];
        }
        g_memberActions.push_back(action);
    }
}

extern "C" void structura_set_end_springs(int count, const int* memberIds, const double* k12) {
    g_endSprings.clear();
    for (int i = 0; i < count && memberIds && k12; ++i) {
        EndSpringData spring;
        spring.memberId = memberIds[i];
        for (int k = 0; k < 12; ++k) {
            spring.k[static_cast<size_t>(k)] = k12[i * 12 + k];
        }
        g_endSprings.push_back(spring);
    }
}

extern "C" void structura_set_plastic_hinges(int count, const int* memberIds, const int* dofs, const double* kElastic, const double* yieldMoment,
                                             const double* hardeningRatio, const double* axialYield, const double* exponent) {
    g_plasticHinges.clear();
    for (int i = 0; i < count && memberIds && dofs && kElastic && yieldMoment && hardeningRatio; ++i) {
        PlasticHingeData hinge;
        hinge.memberId = memberIds[i];
        hinge.dof = dofs[i];
        hinge.kElastic = kElastic[i];
        hinge.yieldMoment = yieldMoment[i];
        hinge.hardeningRatio = hardeningRatio[i];
        hinge.axialYield = axialYield ? axialYield[i] : 0.0;
        hinge.exponent = exponent ? exponent[i] : 2.0;
        g_plasticHinges.push_back(hinge);
    }
}

extern "C" void structura_set_constraints(int diaphragmCount, const int* diaMasters, const int* diaNormals, const int* diaSlaveCounts, const int* diaSlaves,
                                          int linkCount, const int* linkMasters, const int* linkSlaves, const int* linkKinds, const int* linkDofs,
                                          int inclinedCount, const int* incNodes, const double* incAxes9, const int* incMasks,
                                          int genericCount, const int* genTermCounts, const int* genNodes, const int* genDofs, const double* genCoefs,
                                          const int* genReport) {
    g_diaphragms.clear();
    g_rigidLinks.clear();
    g_inclinedSupports.clear();
    g_constraints.clear();
    int cursor = 0;
    for (int i = 0; i < diaphragmCount && diaMasters && diaSlaveCounts && diaSlaves; ++i) {
        DiaphragmData d;
        d.masterNodeId = diaMasters[i];
        d.normalAxis = diaNormals ? diaNormals[i] : 2;
        for (int k = 0; k < diaSlaveCounts[i]; ++k) {
            d.slaveNodeIds.push_back(diaSlaves[cursor + k]);
        }
        cursor += diaSlaveCounts[i];
        g_diaphragms.push_back(std::move(d));
    }
    for (int i = 0; i < linkCount && linkMasters && linkSlaves && linkKinds; ++i) {
        RigidLinkData l;
        l.masterNodeId = linkMasters[i];
        l.slaveNodeId = linkSlaves[i];
        l.kind = linkKinds[i];
        l.dof = linkDofs ? linkDofs[i] : 0;
        g_rigidLinks.push_back(l);
    }
    for (int i = 0; i < inclinedCount && incNodes && incAxes9 && incMasks; ++i) {
        InclinedSupportData s;
        s.nodeId = incNodes[i];
        for (int k = 0; k < 9; ++k) {
            s.axes[static_cast<size_t>(k)] = incAxes9[i * 9 + k];
        }
        s.mask = incMasks[i];
        g_inclinedSupports.push_back(s);
    }
    cursor = 0;
    for (int i = 0; i < genericCount && genTermCounts && genNodes && genDofs && genCoefs; ++i) {
        LinearConstraintData c;
        for (int k = 0; k < genTermCounts[i]; ++k) {
            c.nodeIds.push_back(genNodes[cursor + k]);
            c.dofs.push_back(genDofs[cursor + k]);
            c.coefs.push_back(genCoefs[cursor + k]);
        }
        cursor += genTermCounts[i];
        c.reportReaction = genReport && genReport[i] != 0;
        g_constraints.push_back(std::move(c));
    }
}

extern "C" void structura_set_links(int count, const int* ids, const int* nodeI, const int* nodeJ, const double* axes9,
                                    const int* lawKinds6, const double* lawK6, const double* lawP1_6, const double* lawP2_6, const int* tableCounts6,
                                    const double* tableDef, const double* tableForce) {
    g_links.clear();
    int cursor = 0;
    for (int i = 0; i < count && ids && nodeI && nodeJ && lawKinds6 && lawK6; ++i) {
        LinkData link;
        link.id = ids[i];
        link.nodeI = nodeI[i];
        link.nodeJ = nodeJ[i];
        if (axes9) {
            for (int k = 0; k < 9; ++k) {
                link.axes[static_cast<size_t>(k)] = axes9[i * 9 + k];
            }
        }
        for (int d = 0; d < 6; ++d) {
            LinkLawData& law = link.laws[static_cast<size_t>(d)];
            law.kind = lawKinds6[i * 6 + d];
            law.k = lawK6[i * 6 + d];
            law.p1 = lawP1_6 ? lawP1_6[i * 6 + d] : 0.0;
            law.p2 = lawP2_6 ? lawP2_6[i * 6 + d] : 0.0;
            const int points = tableCounts6 ? tableCounts6[i * 6 + d] : 0;
            for (int p = 0; p < points && tableDef && tableForce; ++p) {
                law.table.emplace_back(tableDef[cursor + p], tableForce[cursor + p]);
            }
            cursor += points;
        }
        g_links.push_back(std::move(link));
    }
}

extern "C" void structura_set_nonlinear_options(int force, int steps, int maxIterations, double tolerance, int criteria, int maxCutbacks, int controlMode,
                                                int controlNode, int controlDof, double controlTarget, double loadFactorMax) {
    g_nonlinear.force = force != 0;
    g_nonlinear.steps = std::max(1, steps);
    g_nonlinear.maxIterations = maxIterations > 0 ? maxIterations : 40;
    g_nonlinear.tolerance = tolerance > 0.0 ? tolerance : 1e-6;
    g_nonlinear.criteria = criteria > 0 ? criteria : 5;
    g_nonlinear.maxCutbacks = std::max(0, maxCutbacks);
    g_nonlinear.controlMode = controlMode;
    g_nonlinear.controlNodeId = controlNode;
    g_nonlinear.controlDof = controlDof;
    g_nonlinear.controlTarget = controlTarget;
    g_nonlinear.loadFactorMax = loadFactorMax > 0.0 ? loadFactorMax : 1.0;
}

extern "C" void structura_set_solver_options(int linearSolver, int eigenSolver, int eigenDenseLimit, double iterativeTolerance, int iterativeMaxIterations) {
    g_linearSolver = linearSolver == 1 ? 1 : 0;
    g_eigenSolver = eigenSolver >= 1 && eigenSolver <= 2 ? eigenSolver : 0;
    g_eigenDenseLimit = eigenDenseLimit > 0 ? eigenDenseLimit : 400;
    g_iterativeTolerance = iterativeTolerance > 0.0 ? iterativeTolerance : 1e-10;
    g_iterativeMaxIterations = iterativeMaxIterations > 0 ? iterativeMaxIterations : 5000;
}

extern "C" void structura_get_nonlinear_summary(double* out8) {
    if (!out8) {
        return;
    }
    const NonlinearSummary& s = g_lastOutput.nonlinear;
    out8[0] = s.used ? 1.0 : 0.0;
    out8[1] = s.converged ? 1.0 : 0.0;
    out8[2] = s.loadFactor;
    out8[3] = s.steps;
    out8[4] = s.iterations;
    out8[5] = static_cast<double>(s.curve.size());
    out8[6] = static_cast<double>(s.hinges.size());
    out8[7] = static_cast<double>(s.links.size());
}

extern "C" int structura_get_nonlinear_curve(int maxCount, double* out2) {
    const int count = std::min(static_cast<int>(g_lastOutput.nonlinear.curve.size()), std::max(0, maxCount));
    for (int i = 0; out2 && i < count; ++i) {
        out2[i * 2] = g_lastOutput.nonlinear.curve[static_cast<size_t>(i)][0];
        out2[i * 2 + 1] = g_lastOutput.nonlinear.curve[static_cast<size_t>(i)][1];
    }
    return count;
}

extern "C" int structura_get_hinge_results(int maxCount, int* memberDof2, double* out3) {
    const int count = std::min(static_cast<int>(g_lastOutput.nonlinear.hinges.size()), std::max(0, maxCount));
    for (int i = 0; memberDof2 && out3 && i < count; ++i) {
        const HingeResult& h = g_lastOutput.nonlinear.hinges[static_cast<size_t>(i)];
        memberDof2[i * 2] = h.memberId;
        memberDof2[i * 2 + 1] = h.dof;
        out3[i * 3] = h.plasticRotation;
        out3[i * 3 + 1] = h.moment;
        out3[i * 3 + 2] = h.yielded ? 1.0 : 0.0;
    }
    return count;
}

extern "C" int structura_get_link_results(int maxCount, int* ids, double* deformation6, double* force6) {
    const int count = std::min(static_cast<int>(g_lastOutput.nonlinear.links.size()), std::max(0, maxCount));
    for (int i = 0; ids && deformation6 && force6 && i < count; ++i) {
        const LinkResult& l = g_lastOutput.nonlinear.links[static_cast<size_t>(i)];
        ids[i] = l.id;
        for (int k = 0; k < 6; ++k) {
            deformation6[i * 6 + k] = l.deformation[static_cast<size_t>(k)];
            force6[i * 6 + k] = l.force[static_cast<size_t>(k)];
        }
    }
    return count;
}

extern "C" int structura_get_constraint_forces(int maxCount, double* forces) {
    const int count = std::min(static_cast<int>(g_lastOutput.constraintForces.size()), std::max(0, maxCount));
    for (int i = 0; forces && i < count; ++i) {
        forces[i] = g_lastOutput.constraintForces[static_cast<size_t>(i)];
    }
    return count;
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
    input.memberActions = g_memberActions;
    input.endSprings = g_endSprings;
    input.plasticHinges = g_plasticHinges;
    input.diaphragms = g_diaphragms;
    input.rigidLinks = g_rigidLinks;
    input.inclinedSupports = g_inclinedSupports;
    input.constraints = g_constraints;
    input.links = g_links;
    input.nonlinear = g_nonlinear;
    input.linearSolver = g_linearSolver;
    input.eigenSolver = g_eigenSolver;
    input.eigenDenseLimit = g_eigenDenseLimit;
    input.iterativeTolerance = g_iterativeTolerance;
    input.iterativeMaxIterations = g_iterativeMaxIterations;
    input.bucklingModes = g_bucklingModes;
    input.modalModes = g_modalModes;
    input.extraNodalMasses = g_modalMasses;
    input.maxIterations = g_options.maxIterations;
    input.tolerance = g_options.tolerance;
    input.shells = g_shells;
    input.shellLoads = g_shellLoads;
    input.ties = g_ties;
    input.springs = g_springs;
    input.shearDeformation = g_shearDeformation;
    input.stabilityFunctions = g_stabilityFunctions;
    input.loadSteps = g_loadSteps;
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
        for (const auto& entry : g_shearAreas) {
            if (entry.sectionId == section.id) {
                section.Ay = entry.ay;
                section.Az = entry.az;
            }
        }
        for (const auto& entry : g_warpingSections) {
            if (entry.sectionId == section.id) {
                section.Cw = entry.cw;
            }
        }
        for (const auto& entry : g_shearCenters) {
            if (entry.sectionId == section.id) {
                section.scY = entry.y;
                section.scZ = entry.z;
            }
        }
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
        for (const auto& zone : g_rigidZones) {
            if (zone.memberId == member.id) {
                member.rigidI = zone.lengthI;
                member.rigidJ = zone.lengthJ;
            }
        }
        for (const auto& entry : g_tapers) {
            if (entry.memberId == member.id) {
                member.taper = true;
                member.taperD1 = entry.d1;
                member.taperD2 = entry.d2;
                member.taperBf = entry.bf;
                member.taperTw = entry.tw;
                member.taperTf = entry.tf;
                member.taperSegments = entry.segments;
            }
        }
        for (const auto& entry : g_warpingMembers) {
            if (entry.memberId == member.id) {
                member.warping = 1;
                member.warpingStart = entry.modeStart;
                member.warpingEnd = entry.modeEnd;
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
