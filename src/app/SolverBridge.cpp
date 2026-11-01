#include "SolverBridge.h"

#include <QLibrary>

#include <algorithm>
#include <cmath>
#include <string>

namespace Structura::App {

namespace {

using StructuraSolveFn = int (*) (
    int, const int*, const double*, const double*,
    int, const int*, const double*, const double*, const double*, const double*,
    int, const int*, const double*, const double*, const double*, const int*,
    int, const int*, const int*, const int*, const int*, const int*,
    int, const int*, const double*,
    int, const int*, const int*, const double*, const double*, const double*,
    double*, double*, double*, double*,
    int, char*);

using StructuraSolveV2Fn = int (*) (
    int, const int*, const double*, const double*,
    int, const int*, const double*, const double*, const double*, const double*,
    int, const int*, const double*, const double*, const double*, const int*,
    int, const int*, const int*, const int*, const int*, const int*,
    int, const int*, const double*,
    int, const int*, const int*, const double*, const double*, const double*,
    int, const int*, const int*, const double*, const double*, const double*,
    double*, double*, double*, double*,
    int, char*);

using StructuraSolveV4Fn = int (*) (
    int, const int*, const double*, const double*,
    int, const int*, const double*, const double*, const double*, const double*,
    int, const int*, const double*, const double*, const double*, const int*,
    int, const int*, const int*, const int*, const int*, const int*,
    int, const int*, const double*,
    int, const int*, const int*, const double*, const double*, const double*,
    int, const int*, const int*, const double*, const double*, const double*,
    const double*, const double*, const int*,
    double*, double*, double*, double*,
    int, char*);

using StructuraSolveV3Fn = int (*) (
    int, const int*, const double*, const double*,
    int, const int*, const double*, const double*, const double*, const double*,
    int, const int*, const double*, const double*, const double*, const int*,
    int, const int*, const int*, const int*, const int*, const int*,
    int, const int*, const double*,
    int, const int*, const int*, const double*, const double*, const double*,
    int, const int*, const int*, const double*, const double*, const double*,
    const double*, const double*,
    double*, double*, double*, double*,
    int, char*);

[[nodiscard]] std::string errorFromLibrary(QLibrary &library)
{
    const QString message = library.errorString();
    if (message.isEmpty()) {
        return "Unable to load solver shared library.";
    }
    return message.toStdString();
}

} // namespace

SolverBridge::SolverBridge(QString libraryPath)
    : m_libraryPath(std::move(libraryPath))
{
}

SolverBridge::Result SolverBridge::solve(const ModelData &model) const
{
    QLibrary solverLibrary;
    if (!m_libraryPath.isEmpty()) {
        solverLibrary.setFileName(m_libraryPath);
    } else {
        // Base name lets Qt resolve rds.dll on Windows and librds.so on Linux.
        solverLibrary.setFileName(QStringLiteral("rds"));
    }

    if (!solverLibrary.load()) {
        throw SolverBridgeError(errorFromLibrary(solverLibrary));
    }

    auto *rawSymbolV4 = solverLibrary.resolve("structura_solve_v4");
    auto *rawSymbolV3 = solverLibrary.resolve("structura_solve_v3");
    auto *rawSymbolV2 = solverLibrary.resolve("structura_solve_v2");
    auto *rawSymbol = solverLibrary.resolve("structura_solve");
    if (!rawSymbolV4 && !rawSymbolV3 && !rawSymbolV2 && !rawSymbol) {
        throw SolverBridgeError("Failed to resolve exported function structura_solve.");
    }
    if (!rawSymbolV4 && !rawSymbolV3 && !rawSymbolV2 && !model.memberMoments.empty()) {
        throw SolverBridgeError("A DLL do solver nao suporta momentos distribuidos em barras (structura_solve_v2 ausente).");
    }
    const bool needsPlacement = std::any_of(model.members.begin(), model.members.end(),
                                            [](const MemberData &member) { return member.hasPlacement(); });
    const bool needsReleases = std::any_of(model.members.begin(), model.members.end(),
                                           [](const MemberData &member) { return member.hasReleases(); });
    if (!rawSymbolV4 && needsReleases) {
        throw SolverBridgeError("A DLL do solver nao suporta articulacoes de extremidade (structura_solve_v4 ausente). "
                                "Recompile Structuradll e copie rds.dll para a raiz.");
    }
    if (!rawSymbolV4 && !rawSymbolV3 && needsPlacement) {
        throw SolverBridgeError("A DLL do solver nao suporta giro/excentricidade de secao (structura_solve_v3 ausente). "
                                "Recompile Structuradll e copie rds.dll para a raiz.");
    }

    using SetOptionsFn = void (*)(int, int, double);
    auto setOptions = reinterpret_cast<SetOptionsFn>(solverLibrary.resolve("structura_set_options"));
    if (model.secondOrder && !setOptions) {
        throw SolverBridgeError("A DLL do solver nao suporta analise de segunda ordem (structura_set_options ausente). "
                                "Recompile Structuradll e copie rds.dll para a raiz.");
    }
    if (setOptions) {
        setOptions(model.secondOrder ? 1 : 0, 0, 0.0);
    }
    using SetAnalysisFn = void (*)(int, int, int);
    auto setAnalysis = reinterpret_cast<SetAnalysisFn>(solverLibrary.resolve("structura_set_analysis_options"));
    using SetShearAreasFn = void (*)(int, const int *, const double *, const double *);
    auto setShearAreas = reinterpret_cast<SetShearAreasFn>(solverLibrary.resolve("structura_set_shear_areas"));
    if ((model.shearDeformation || model.stabilityFunctions || model.loadSteps > 1) && (!setAnalysis || (model.shearDeformation && !setShearAreas))) {
        throw SolverBridgeError("A DLL do solver nao suporta deformacao por cortante/funcoes de estabilidade (structura_set_analysis_options ausente). "
                                "Recompile Structuradll e copie rds.dll para a raiz.");
    }
    if (setAnalysis) {
        setAnalysis(model.shearDeformation ? 1 : 0, model.stabilityFunctions ? 1 : 0, model.loadSteps);
    }
    if (setShearAreas) {
        std::vector<int> shearIds;
        std::vector<double> shearY, shearZ;
        if (model.shearDeformation) {
            for (const auto &section : model.sections) {
                shearIds.push_back(section.id);
                shearY.push_back(section.shearAreaY);
                shearZ.push_back(section.shearAreaZ);
            }
        }
        setShearAreas(static_cast<int>(shearIds.size()), shearIds.data(), shearY.data(), shearZ.data());
    }
    {
        using SetWarpingSectionsFn = void (*)(int, const int *, const double *);
        using SetWarpingMembersFn = void (*)(int, const int *, const int *, const int *);
        auto setWarpingSections = reinterpret_cast<SetWarpingSectionsFn>(solverLibrary.resolve("structura_set_warping_sections"));
        auto setWarpingMembers = reinterpret_cast<SetWarpingMembersFn>(solverLibrary.resolve("structura_set_warping_members"));
        std::vector<int> sectionIds, memberIds, modeStart, modeEnd;
        std::vector<double> constants;
        for (const auto &member : model.members) {
            if (member.warping) {
                memberIds.push_back(member.id);
                modeStart.push_back(member.warpingStartMode);
                modeEnd.push_back(member.warpingEndMode);
            }
        }
        if (!memberIds.empty()) {
            for (const auto &section : model.sections) {
                if (section.warpingConstant > 0.0) {
                    sectionIds.push_back(section.id);
                    constants.push_back(section.warpingConstant);
                }
            }
        }
        if (!memberIds.empty() && (!setWarpingSections || !setWarpingMembers)) {
            throw SolverBridgeError("A DLL do solver nao suporta torcao com empenamento (structura_set_warping_* ausente). "
                                    "Recompile Structuradll e copie rds.dll para a raiz.");
        }
        if (setWarpingSections && setWarpingMembers) {
            setWarpingSections(static_cast<int>(sectionIds.size()), sectionIds.data(), constants.data());
            setWarpingMembers(static_cast<int>(memberIds.size()), memberIds.data(), modeStart.data(), modeEnd.data());
        }
        using SetShearCentersFn = void (*)(int, const int *, const double *, const double *);
        auto setShearCenters = reinterpret_cast<SetShearCentersFn>(solverLibrary.resolve("structura_set_shear_centers"));
        std::vector<int> centerIds;
        std::vector<double> centerY, centerZ;
        for (const auto &section : model.sections) {
            if (section.warpingConstant > 0.0 && (section.shearCenterY != 0.0 || section.shearCenterZ != 0.0)) {
                centerIds.push_back(section.id);
                centerY.push_back(section.shearCenterY);
                centerZ.push_back(section.shearCenterZ);
            }
        }
        if (!centerIds.empty() && !memberIds.empty() && !setShearCenters) {
            throw SolverBridgeError("A DLL do solver nao suporta o centro de cisalhamento fora do centroide (structura_set_shear_centers ausente). "
                                    "Recompile Structuradll e copie rds.dll para a raiz.");
        }
        if (setShearCenters) {
            setShearCenters(static_cast<int>(centerIds.size()), centerIds.data(), centerY.data(), centerZ.data());
        }
    }
    {
        using SetTaperFn = void (*)(int, const int *, const double *, const double *, const double *, const double *, const double *, const int *);
        auto setTaper = reinterpret_cast<SetTaperFn>(solverLibrary.resolve("structura_set_taper"));
        std::vector<int> ids, segments;
        std::vector<double> d1, d2, bf, tw, tf;
        for (const auto &member : model.members) {
            if (member.taper) {
                ids.push_back(member.id);
                d1.push_back(member.taperD1);
                d2.push_back(member.taperD2);
                bf.push_back(member.taperBf);
                tw.push_back(member.taperTw);
                tf.push_back(member.taperTf);
                segments.push_back(member.taperSegments);
            }
        }
        if (!ids.empty() && !setTaper) {
            throw SolverBridgeError("A DLL do solver nao suporta barras de secao variavel (structura_set_taper ausente). "
                                    "Recompile Structuradll e copie rds.dll para a raiz.");
        }
        if (setTaper) {
            setTaper(static_cast<int>(ids.size()), ids.data(), d1.data(), d2.data(), bf.data(), tw.data(), tf.data(), segments.data());
        }
    }
    using SetRigidZonesFn = void (*)(int, const int *, const double *, const double *);
    auto setRigidZones = reinterpret_cast<SetRigidZonesFn>(solverLibrary.resolve("structura_set_rigid_zones"));
    {
        std::vector<int> rigidIds;
        std::vector<double> rigidStart, rigidEnd;
        for (const auto &member : model.members) {
            if (member.rigidStart > 0.0 || member.rigidEnd > 0.0) {
                rigidIds.push_back(member.id);
                rigidStart.push_back(member.rigidStart);
                rigidEnd.push_back(member.rigidEnd);
            }
        }
        if (!rigidIds.empty() && !setRigidZones) {
            throw SolverBridgeError("A DLL do solver nao suporta zonas rigidas (structura_set_rigid_zones ausente). "
                                    "Recompile Structuradll e copie rds.dll para a raiz.");
        }
        if (setRigidZones) {
            setRigidZones(static_cast<int>(rigidIds.size()), rigidIds.data(), rigidStart.data(), rigidEnd.data());
        }
    }
    {
        using SetMemberActionsFn = void (*)(int, const int *, const int *, const int *, const double *, const double *, const double *, const double *);
        auto setMemberActions = reinterpret_cast<SetMemberActionsFn>(solverLibrary.resolve("structura_set_member_actions"));
        if (!model.memberActions.empty() && !setMemberActions) {
            throw SolverBridgeError("A DLL do solver nao suporta cargas de barra com posicao (trapezoidais, pontuais, temperatura; structura_set_member_actions ausente). "
                                    "Recompile Structuradll e copie rds.dll para a raiz.");
        }
        if (setMemberActions) {
            std::vector<int> ids, kinds, systems;
            std::vector<double> a, b, v1, v2;
            for (const auto &action : model.memberActions) {
                ids.push_back(action.memberId);
                kinds.push_back(action.kind);
                systems.push_back(action.coordSystem == MemberLoadCoordSystem::Global ? 1 : 0);
                a.push_back(action.a);
                b.push_back(action.b);
                v1.insert(v1.end(), action.v1.begin(), action.v1.end());
                v2.insert(v2.end(), action.v2.begin(), action.v2.end());
            }
            setMemberActions(static_cast<int>(ids.size()), ids.data(), kinds.data(), systems.data(), a.data(), b.data(), v1.data(), v2.data());
        }
    }
    {
        // semirrigidez, rotulas plasticas, vinculos, ligacoes entre nos e controle da analise nao linear (estado global da DLL: sempre redefinido)
        auto resolve = [&](const char *name) { return solverLibrary.resolve(name); };
        auto *setEndSprings = reinterpret_cast<void (*)(int, const int *, const double *)>(resolve("structura_set_end_springs"));
        auto *setHinges = reinterpret_cast<void (*)(int, const int *, const int *, const double *, const double *, const double *, const double *, const double *)>(
            resolve("structura_set_plastic_hinges"));
        auto *setConstraints = reinterpret_cast<void (*)(int, const int *, const int *, const int *, const int *, int, const int *, const int *, const int *, const int *,
                                                         int, const int *, const double *, const int *, int, const int *, const int *, const int *, const double *, const int *)>(
            resolve("structura_set_constraints"));
        auto *setLinks = reinterpret_cast<void (*)(int, const int *, const int *, const int *, const double *, const int *, const double *, const double *, const double *,
                                                   const int *, const double *, const double *)>(resolve("structura_set_links"));
        auto *setNonlinear = reinterpret_cast<void (*)(int, int, int, double, int, int, int, int, int, double, double)>(resolve("structura_set_nonlinear_options"));
        const bool needsNew = !model.endSprings.empty() || !model.plasticHinges.empty() || !model.diaphragms.empty() || !model.rigidLinks.empty()
                              || !model.inclinedSupports.empty() || !model.constraints.empty() || !model.links.empty() || model.nonlinear.force;
        if (needsNew && (!setEndSprings || !setHinges || !setConstraints || !setLinks || !setNonlinear)) {
            throw SolverBridgeError("A DLL do solver nao suporta molas de extremidade, rotulas plasticas, vinculos, ligacoes entre nos ou analise nao linear "
                                    "(funcoes structura_set_* ausentes). Recompile Structuradll e copie rds.dll para a raiz.");
        }
        if (setEndSprings) {
            std::vector<int> ids;
            std::vector<double> k;
            for (const auto &spring : model.endSprings) {
                ids.push_back(spring.memberId);
                k.insert(k.end(), spring.k.begin(), spring.k.end());
            }
            setEndSprings(static_cast<int>(ids.size()), ids.data(), k.data());
        }
        if (setHinges) {
            std::vector<int> ids, dofs;
            std::vector<double> kEl, my, ratio, ny, exponent;
            for (const auto &h : model.plasticHinges) {
                ids.push_back(h.memberId);
                dofs.push_back(h.dof);
                kEl.push_back(h.kElastic);
                my.push_back(h.yieldMoment);
                ratio.push_back(h.hardeningRatio);
                ny.push_back(h.axialYield);
                exponent.push_back(h.exponent);
            }
            setHinges(static_cast<int>(ids.size()), ids.data(), dofs.data(), kEl.data(), my.data(), ratio.data(), ny.data(), exponent.data());
        }
        if (setConstraints) {
            std::vector<int> diaMasters, diaNormals, diaCounts, diaSlaves, linkMasters, linkSlaves, linkKinds, linkDofs, incNodes, incMasks, genCounts, genNodes, genDofs, genReport;
            std::vector<double> incAxes, genCoefs;
            for (const auto &d : model.diaphragms) {
                diaMasters.push_back(d.masterNodeId);
                diaNormals.push_back(d.normalAxis);
                diaCounts.push_back(static_cast<int>(d.slaveNodeIds.size()));
                diaSlaves.insert(diaSlaves.end(), d.slaveNodeIds.begin(), d.slaveNodeIds.end());
            }
            for (const auto &l : model.rigidLinks) {
                linkMasters.push_back(l.masterNodeId);
                linkSlaves.push_back(l.slaveNodeId);
                linkKinds.push_back(l.kind);
                linkDofs.push_back(l.dof);
            }
            for (const auto &s : model.inclinedSupports) {
                incNodes.push_back(s.nodeId);
                incAxes.insert(incAxes.end(), s.axes.begin(), s.axes.end());
                incMasks.push_back(s.mask);
            }
            for (const auto &c : model.constraints) {
                genCounts.push_back(static_cast<int>(c.nodeIds.size()));
                genNodes.insert(genNodes.end(), c.nodeIds.begin(), c.nodeIds.end());
                genDofs.insert(genDofs.end(), c.dofs.begin(), c.dofs.end());
                genCoefs.insert(genCoefs.end(), c.coefs.begin(), c.coefs.end());
                genReport.push_back(c.reportReaction ? 1 : 0);
            }
            setConstraints(static_cast<int>(diaMasters.size()), diaMasters.data(), diaNormals.data(), diaCounts.data(), diaSlaves.data(),
                           static_cast<int>(linkMasters.size()), linkMasters.data(), linkSlaves.data(), linkKinds.data(), linkDofs.data(),
                           static_cast<int>(incNodes.size()), incNodes.data(), incAxes.data(), incMasks.data(),
                           static_cast<int>(genCounts.size()), genCounts.data(), genNodes.data(), genDofs.data(), genCoefs.data(), genReport.data());
        }
        if (setLinks) {
            std::vector<int> ids, nodeI, nodeJ, kinds, tableCounts;
            std::vector<double> axes, k, p1, p2, tableDef, tableForce;
            for (const auto &link : model.links) {
                ids.push_back(link.id);
                nodeI.push_back(link.nodeI);
                nodeJ.push_back(link.nodeJ);
                axes.insert(axes.end(), link.axes.begin(), link.axes.end());
                for (const auto &law : link.laws) {
                    kinds.push_back(law.kind);
                    k.push_back(law.k);
                    p1.push_back(law.p1);
                    p2.push_back(law.p2);
                    tableCounts.push_back(static_cast<int>(law.table.size()));
                    for (const auto &point : law.table) {
                        tableDef.push_back(point.first);
                        tableForce.push_back(point.second);
                    }
                }
            }
            setLinks(static_cast<int>(ids.size()), ids.data(), nodeI.data(), nodeJ.data(), axes.data(), kinds.data(), k.data(), p1.data(), p2.data(), tableCounts.data(),
                     tableDef.data(), tableForce.data());
        }
        if (auto *setSolverOptions = reinterpret_cast<void (*)(int, int, int, double, int)>(resolve("structura_set_solver_options"))) {
            setSolverOptions(model.linearSolver, model.eigenSolver, 400, 1e-10, 5000);
        }
        if (setNonlinear) {
            const auto &o = model.nonlinear;
            setNonlinear(o.force ? 1 : 0, o.steps, o.maxIterations, o.tolerance, o.criteria, o.maxCutbacks, o.controlMode, o.controlNodeId, o.controlDof, o.controlTarget,
                         o.loadFactorMax);
        }
    }
    using SetBucklingFn = void (*)(int);
    auto setBuckling = reinterpret_cast<SetBucklingFn>(solverLibrary.resolve("structura_set_buckling"));
    if (model.bucklingModes > 0 && !setBuckling) {
        throw SolverBridgeError("A DLL do solver nao suporta flambagem elastica (structura_set_buckling ausente). "
                                "Recompile Structuradll e copie rds.dll para a raiz.");
    }
    if (setBuckling) {
        setBuckling(model.secondOrder ? 0 : model.bucklingModes);
    }
    using SetModalFn = void (*)(int, int, const int *, const double *, int, const int *, const double *);
    auto setModal = reinterpret_cast<SetModalFn>(solverLibrary.resolve("structura_set_modal"));
    if (model.modalModes > 0 && !setModal) {
        throw SolverBridgeError("A DLL do solver nao suporta analise modal (structura_set_modal ausente). "
                                "Recompile Structuradll e copie rds.dll para a raiz.");
    }
    if (setModal) {
        std::vector<int> densityIds, massNodes;
        std::vector<double> densities, masses;
        for (const auto &material : model.materials) {
            densityIds.push_back(material.id);
            densities.push_back(material.density);
        }
        for (const auto &extra : model.extraMasses) {
            massNodes.push_back(extra.first);
            masses.push_back(extra.second);
        }
        setModal(model.secondOrder ? 0 : model.modalModes, static_cast<int>(densityIds.size()), densityIds.data(), densities.data(),
                 static_cast<int>(massNodes.size()), massNodes.data(), masses.data());
    }
    std::vector<int> tensionOnlyIds;
    for (const auto &member : model.members) {
        if (member.tensionOnly) {
            tensionOnlyIds.push_back(member.id);
        }
    }
    using SetTensionOnlyFn = void (*)(int, const int *);
    auto setTensionOnly = reinterpret_cast<SetTensionOnlyFn>(solverLibrary.resolve("structura_set_tension_only"));
    if (!tensionOnlyIds.empty() && !setTensionOnly) {
        throw SolverBridgeError("A DLL do solver nao suporta tirantes (structura_set_tension_only ausente). "
                                "Recompile Structuradll e copie rds.dll para a raiz.");
    }
    if (setTensionOnly) {
        setTensionOnly(static_cast<int>(tensionOnlyIds.size()), tensionOnlyIds.empty() ? nullptr : tensionOnlyIds.data());
    }

    std::vector<int> materialIds;
    std::vector<double> materialE;
    std::vector<double> materialG;
    materialIds.reserve(model.materials.size());
    materialE.reserve(model.materials.size());
    materialG.reserve(model.materials.size());
    for (const auto &material : model.materials) {
        materialIds.push_back(material.id);
        materialE.push_back(material.e);
        materialG.push_back(material.g);
    }

    std::vector<int> sectionIds;
    std::vector<double> sectionA;
    std::vector<double> sectionIz;
    std::vector<double> sectionIy;
    std::vector<double> sectionJ;
    sectionIds.reserve(model.sections.size());
    sectionA.reserve(model.sections.size());
    sectionIz.reserve(model.sections.size());
    sectionIy.reserve(model.sections.size());
    sectionJ.reserve(model.sections.size());
    for (const auto &section : model.sections) {
        sectionIds.push_back(section.id);
        sectionA.push_back(section.area);
        sectionIz.push_back(section.iz);
        sectionIy.push_back(section.iy);
        sectionJ.push_back(section.j);
    }

    std::vector<int> nodeIds;
    std::vector<double> nodeX;
    std::vector<double> nodeY;
    std::vector<double> nodeZ;
    std::vector<int> nodeRestraints6;
    nodeIds.reserve(model.nodes.size());
    nodeX.reserve(model.nodes.size());
    nodeY.reserve(model.nodes.size());
    nodeZ.reserve(model.nodes.size());
    nodeRestraints6.reserve(model.nodes.size() * 6);
    for (const auto &node : model.nodes) {
        nodeIds.push_back(node.id);
        nodeX.push_back(node.x);
        nodeY.push_back(node.y);
        nodeZ.push_back(node.z);
        for (const int restraint : node.restraints) {
            nodeRestraints6.push_back(restraint != 0 ? 1 : 0);
        }
    }

    // nos das cascas (alem dos nos das barras)
    const std::size_t barNodeCount = model.nodes.size();
    if (model.shell) {
        for (const auto &node : model.shell->nodes) {
            nodeIds.push_back(node.id);
            nodeX.push_back(node.x);
            nodeY.push_back(node.y);
            nodeZ.push_back(node.z);
            for (const int restraint : node.restraints) {
                nodeRestraints6.push_back(restraint != 0 ? 1 : 0);
            }
        }
    }
    const std::size_t totalNodeCount = nodeIds.size();

    // estado das cascas na DLL (global): sempre redefinido, para nao herdar o da chamada anterior
    using ClearShellsFn = void (*)();
    using SetShellsFn = int (*)(int, const int *, const int *, const double *, const double *, const double *, const double *,
                                int, const int *, const int *, const double *,
                                int, const int *, const int *, const int *, const double *, const int *,
                                int, const int *, const double *, const int *, const double *);
    using GetShellResultsFn = int (*)(int, double *, double *);
    auto clearShells = reinterpret_cast<ClearShellsFn>(solverLibrary.resolve("structura_clear_shells"));
    auto setShells = reinterpret_cast<SetShellsFn>(solverLibrary.resolve("structura_set_shells"));
    auto getShellResults = reinterpret_cast<GetShellResultsFn>(solverLibrary.resolve("structura_get_shell_results"));
    const bool hasShells = model.shell && !model.shell->empty();
    if (hasShells && (!setShells || !getShellResults || !rawSymbolV4)) {
        throw SolverBridgeError("A DLL do solver nao suporta cascas acopladas (structura_set_shells ausente). "
                                "Recompile Structuradll e copie rds.dll para a raiz.");
    }
    if (!hasShells && clearShells) {
        clearShells();
    }
    if (hasShells) {
        std::vector<int> shellIds, shellNodes4, loadShells, loadKinds, tieSlaves, tieCounts, tieMasters, tieMasks, springNodes, springCompression;
        std::vector<double> shellE, shellNu, shellDensity, shellThickness, loadValues, tieWeights, springK, springNormals;
        for (const auto &e : model.shell->elements) {
            shellIds.push_back(e.id);
            shellNodes4.insert(shellNodes4.end(), e.nodes.begin(), e.nodes.end());
            shellE.push_back(e.e);
            shellNu.push_back(e.nu);
            shellDensity.push_back(e.density);
            shellThickness.push_back(e.thickness);
        }
        for (const auto &l : model.shellLoads) {
            loadShells.push_back(l.shellId);
            loadKinds.push_back(l.kind);
            loadValues.insert(loadValues.end(), l.v.begin(), l.v.end());
        }
        for (const auto &t : model.shell->ties) {
            tieSlaves.push_back(t.slave);
            tieCounts.push_back(static_cast<int>(t.masters.size()));
            tieMasters.insert(tieMasters.end(), t.masters.begin(), t.masters.end());
            tieWeights.insert(tieWeights.end(), t.weights.begin(), t.weights.end());
            tieMasks.push_back(t.dofMask);
        }
        for (const auto &s : model.shell->springs) {
            springNodes.push_back(s.node);
            springK.insert(springK.end(), s.k.begin(), s.k.end());
            springCompression.push_back(s.compressionOnly ? 1 : 0);
            springNormals.insert(springNormals.end(), s.normal.begin(), s.normal.end());
        }
        const int code = setShells(static_cast<int>(shellIds.size()), shellIds.data(), shellNodes4.data(), shellE.data(), shellNu.data(), shellDensity.data(),
                                   shellThickness.data(), static_cast<int>(loadShells.size()), loadShells.data(), loadKinds.data(), loadValues.data(),
                                   static_cast<int>(tieSlaves.size()), tieSlaves.data(), tieCounts.data(), tieMasters.data(), tieWeights.data(), tieMasks.data(),
                                   static_cast<int>(springNodes.size()), springNodes.data(), springK.data(), springCompression.data(), springNormals.data());
        if (code != 0) {
            throw SolverBridgeError("Dados de cascas invalidos para o solver.");
        }
        using SetShellAlphaFn = int (*)(int, const double *);
        auto setShellAlpha = reinterpret_cast<SetShellAlphaFn>(solverLibrary.resolve("structura_set_shell_alpha"));
        std::vector<double> shellAlpha;
        for (const auto &e : model.shell->elements) {
            shellAlpha.push_back(e.alpha);
        }
        const bool anyOrtho = std::any_of(model.shell->elements.begin(), model.shell->elements.end(), [](const auto &e) { return e.orthotropic; });
        if (anyOrtho) {
            using SetShellOrthoFn = int (*)(int, const int *, const double *, const double *, const double *, const double *, const double *, const double *);
            auto setOrtho = reinterpret_cast<SetShellOrthoFn>(solverLibrary.resolve("structura_set_shell_orthotropy"));
            if (!setOrtho) {
                throw SolverBridgeError("A DLL do solver nao suporta material ortotropico em cascas (structura_set_shell_orthotropy ausente). "
                                        "Recompile Structuradll e copie rds.dll para a raiz.");
            }
            std::vector<int> flags;
            std::vector<double> e2, nu12, g12, b1, b2, axes;
            for (const auto &e : model.shell->elements) {
                flags.push_back(e.orthotropic ? 1 : 0);
                e2.push_back(e.e2);
                nu12.push_back(e.nu12);
                g12.push_back(e.g12);
                b1.push_back(e.bend1);
                b2.push_back(e.bend2);
                axes.insert(axes.end(), e.axis.begin(), e.axis.end());
            }
            if (setOrtho(static_cast<int>(flags.size()), flags.data(), e2.data(), nu12.data(), g12.data(), b1.data(), b2.data(), axes.data()) != 0) {
                throw SolverBridgeError("Dados de ortotropia das cascas invalidos para o solver.");
            }
        }
        if (setShellAlpha) {
            setShellAlpha(static_cast<int>(shellAlpha.size()), shellAlpha.data());
        } else if (std::any_of(shellAlpha.begin(), shellAlpha.end(), [](double a) { return std::abs(a - 1.2e-5) > 1e-12; })) {
            throw SolverBridgeError("A DLL do solver nao aceita o coeficiente de dilatacao das cascas (structura_set_shell_alpha ausente). "
                                    "Recompile Structuradll e copie rds.dll para a raiz.");
        }
    }

    std::vector<int> memberIds;
    std::vector<int> memberNodeI;
    std::vector<int> memberNodeJ;
    std::vector<int> memberMaterialId;
    std::vector<int> memberSectionId;
    std::vector<double> memberSectionRotationDeg;
    std::vector<double> memberOffsets4;
    std::vector<int> memberReleases12;
    memberIds.reserve(model.members.size());
    memberNodeI.reserve(model.members.size());
    memberNodeJ.reserve(model.members.size());
    memberMaterialId.reserve(model.members.size());
    memberSectionId.reserve(model.members.size());
    for (const auto &member : model.members) {
        memberIds.push_back(member.id);
        memberNodeI.push_back(member.nodeI);
        memberNodeJ.push_back(member.nodeJ);
        memberMaterialId.push_back(member.materialId);
        memberSectionId.push_back(member.sectionId);
        memberSectionRotationDeg.push_back(member.sectionRotationDeg);
        memberOffsets4.push_back(member.offsetY1);
        memberOffsets4.push_back(member.offsetZ1);
        memberOffsets4.push_back(member.offsetY2);
        memberOffsets4.push_back(member.offsetZ2);
        for (const int flag : member.releases) {
            memberReleases12.push_back(flag);
        }
    }

    std::vector<int> nodalLoadNodeIds;
    std::vector<double> nodalLoadValues6;
    nodalLoadNodeIds.reserve(model.nodalLoads.size());
    nodalLoadValues6.reserve(model.nodalLoads.size() * 6);
    for (const auto &load : model.nodalLoads) {
        nodalLoadNodeIds.push_back(load.nodeId);
        for (const double value : load.values) {
            nodalLoadValues6.push_back(value);
        }
    }

    std::vector<int> memberLoadMemberIds;
    std::vector<int> memberLoadCoordSystem;
    std::vector<double> memberLoadQx;
    std::vector<double> memberLoadQy;
    std::vector<double> memberLoadQz;
    memberLoadMemberIds.reserve(model.memberLoads.size());
    memberLoadCoordSystem.reserve(model.memberLoads.size());
    memberLoadQx.reserve(model.memberLoads.size());
    memberLoadQy.reserve(model.memberLoads.size());
    memberLoadQz.reserve(model.memberLoads.size());
    for (const auto &load : model.memberLoads) {
        memberLoadMemberIds.push_back(load.memberId);
        memberLoadCoordSystem.push_back(load.coordSystem == MemberLoadCoordSystem::Global ? 1 : 0);
        memberLoadQx.push_back(load.qx);
        memberLoadQy.push_back(load.qy);
        memberLoadQz.push_back(load.qz);
    }

    std::vector<int> memberMomentMemberIds;
    std::vector<int> memberMomentCoordSystem;
    std::vector<double> memberMomentMx;
    std::vector<double> memberMomentMy;
    std::vector<double> memberMomentMz;
    for (const auto &moment : model.memberMoments) {
        memberMomentMemberIds.push_back(moment.memberId);
        memberMomentCoordSystem.push_back(moment.coordSystem == MemberLoadCoordSystem::Global ? 1 : 0);
        memberMomentMx.push_back(moment.mx);
        memberMomentMy.push_back(moment.my);
        memberMomentMz.push_back(moment.mz);
    }

    std::vector<double> outNodeDisplacements6(totalNodeCount * 6, 0.0);
    std::vector<double> outNodeReactions6(totalNodeCount * 6, 0.0);
    // (o minimo de 1 evita ponteiro nulo quando nao ha barras: modelo so de cascas)
    std::vector<double> outMemberLocalForces12(std::max<std::size_t>(1, model.members.size() * 12), 0.0);
    std::vector<double> outMemberAuxK3(std::max<std::size_t>(1, model.members.size() * 3), 0.0);

    std::vector<char> errorBuffer(2048, '\0');

    const int returnCode = rawSymbolV4
        ? reinterpret_cast<StructuraSolveV4Fn>(rawSymbolV4)(
        static_cast<int>(materialIds.size()), materialIds.data(), materialE.data(), materialG.data(),
        static_cast<int>(sectionIds.size()), sectionIds.data(), sectionA.data(), sectionIz.data(), sectionIy.data(), sectionJ.data(),
        static_cast<int>(nodeIds.size()), nodeIds.data(), nodeX.data(), nodeY.data(), nodeZ.data(), nodeRestraints6.data(),
        static_cast<int>(memberIds.size()), memberIds.data(), memberNodeI.data(), memberNodeJ.data(), memberMaterialId.data(), memberSectionId.data(),
        static_cast<int>(nodalLoadNodeIds.size()), nodalLoadNodeIds.data(), nodalLoadValues6.data(),
        static_cast<int>(memberLoadMemberIds.size()), memberLoadMemberIds.data(), memberLoadCoordSystem.data(),
        memberLoadQx.data(), memberLoadQy.data(), memberLoadQz.data(),
        static_cast<int>(memberMomentMemberIds.size()), memberMomentMemberIds.data(), memberMomentCoordSystem.data(),
        memberMomentMx.data(), memberMomentMy.data(), memberMomentMz.data(),
        memberSectionRotationDeg.data(), memberOffsets4.data(), memberReleases12.data(),
        outNodeDisplacements6.data(), outNodeReactions6.data(), outMemberLocalForces12.data(), outMemberAuxK3.data(),
        static_cast<int>(errorBuffer.size()), errorBuffer.data())
        : rawSymbolV3
        ? reinterpret_cast<StructuraSolveV3Fn>(rawSymbolV3)(
        static_cast<int>(materialIds.size()), materialIds.data(), materialE.data(), materialG.data(),
        static_cast<int>(sectionIds.size()), sectionIds.data(), sectionA.data(), sectionIz.data(), sectionIy.data(), sectionJ.data(),
        static_cast<int>(nodeIds.size()), nodeIds.data(), nodeX.data(), nodeY.data(), nodeZ.data(), nodeRestraints6.data(),
        static_cast<int>(memberIds.size()), memberIds.data(), memberNodeI.data(), memberNodeJ.data(), memberMaterialId.data(), memberSectionId.data(),
        static_cast<int>(nodalLoadNodeIds.size()), nodalLoadNodeIds.data(), nodalLoadValues6.data(),
        static_cast<int>(memberLoadMemberIds.size()), memberLoadMemberIds.data(), memberLoadCoordSystem.data(),
        memberLoadQx.data(), memberLoadQy.data(), memberLoadQz.data(),
        static_cast<int>(memberMomentMemberIds.size()), memberMomentMemberIds.data(), memberMomentCoordSystem.data(),
        memberMomentMx.data(), memberMomentMy.data(), memberMomentMz.data(),
        memberSectionRotationDeg.data(), memberOffsets4.data(),
        outNodeDisplacements6.data(), outNodeReactions6.data(), outMemberLocalForces12.data(), outMemberAuxK3.data(),
        static_cast<int>(errorBuffer.size()), errorBuffer.data())
        : rawSymbolV2
        ? reinterpret_cast<StructuraSolveV2Fn>(rawSymbolV2)(
        static_cast<int>(materialIds.size()), materialIds.data(), materialE.data(), materialG.data(),
        static_cast<int>(sectionIds.size()), sectionIds.data(), sectionA.data(), sectionIz.data(), sectionIy.data(), sectionJ.data(),
        static_cast<int>(nodeIds.size()), nodeIds.data(), nodeX.data(), nodeY.data(), nodeZ.data(), nodeRestraints6.data(),
        static_cast<int>(memberIds.size()), memberIds.data(), memberNodeI.data(), memberNodeJ.data(), memberMaterialId.data(), memberSectionId.data(),
        static_cast<int>(nodalLoadNodeIds.size()), nodalLoadNodeIds.data(), nodalLoadValues6.data(),
        static_cast<int>(memberLoadMemberIds.size()), memberLoadMemberIds.data(), memberLoadCoordSystem.data(),
        memberLoadQx.data(), memberLoadQy.data(), memberLoadQz.data(),
        static_cast<int>(memberMomentMemberIds.size()), memberMomentMemberIds.data(), memberMomentCoordSystem.data(),
        memberMomentMx.data(), memberMomentMy.data(), memberMomentMz.data(),
        outNodeDisplacements6.data(), outNodeReactions6.data(), outMemberLocalForces12.data(), outMemberAuxK3.data(),
        static_cast<int>(errorBuffer.size()), errorBuffer.data())
        : reinterpret_cast<StructuraSolveFn>(rawSymbol)(
        static_cast<int>(materialIds.size()), materialIds.data(), materialE.data(), materialG.data(),
        static_cast<int>(sectionIds.size()), sectionIds.data(), sectionA.data(), sectionIz.data(), sectionIy.data(), sectionJ.data(),
        static_cast<int>(nodeIds.size()), nodeIds.data(), nodeX.data(), nodeY.data(), nodeZ.data(), nodeRestraints6.data(),
        static_cast<int>(memberIds.size()), memberIds.data(), memberNodeI.data(), memberNodeJ.data(), memberMaterialId.data(), memberSectionId.data(),
        static_cast<int>(nodalLoadNodeIds.size()), nodalLoadNodeIds.data(), nodalLoadValues6.data(),
        static_cast<int>(memberLoadMemberIds.size()), memberLoadMemberIds.data(), memberLoadCoordSystem.data(),
        memberLoadQx.data(), memberLoadQy.data(), memberLoadQz.data(),
        outNodeDisplacements6.data(), outNodeReactions6.data(), outMemberLocalForces12.data(), outMemberAuxK3.data(),
        static_cast<int>(errorBuffer.size()), errorBuffer.data());

    if (returnCode != 0) {
        std::string errorMessage(errorBuffer.data());
        if (errorMessage.empty()) {
            errorMessage = "structura_solve returned an error without message.";
        }
        throw SolverBridgeError(errorMessage);
    }

    Result result;
    result.nodeResults.resize(model.nodes.size());
    result.memberResults.resize(model.members.size());

    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        auto &nodeResult = result.nodeResults[i];
        nodeResult.nodeId = model.nodes[i].id;
        for (int dof = 0; dof < 6; ++dof) {
            nodeResult.displacements[static_cast<std::size_t>(dof)] = outNodeDisplacements6[i * 6 + static_cast<std::size_t>(dof)];
            nodeResult.reactions[static_cast<std::size_t>(dof)] = outNodeReactions6[i * 6 + static_cast<std::size_t>(dof)];
        }
    }

    if (hasShells) {
        for (std::size_t i = barNodeCount; i < totalNodeCount; ++i) {
            NodeResult nodeResult;
            nodeResult.nodeId = nodeIds[i];
            for (int dof = 0; dof < 6; ++dof) {
                nodeResult.displacements[static_cast<std::size_t>(dof)] = outNodeDisplacements6[i * 6 + static_cast<std::size_t>(dof)];
                nodeResult.reactions[static_cast<std::size_t>(dof)] = outNodeReactions6[i * 6 + static_cast<std::size_t>(dof)];
            }
            result.shellNodeResults.push_back(nodeResult);
        }
        const std::size_t count = model.shell->elements.size();
        std::vector<double> values(count * 40U), axes(count * 9U);
        if (getShellResults(static_cast<int>(count), values.data(), axes.data()) == static_cast<int>(count)) {
            for (std::size_t e = 0; e < count; ++e) {
                ShellElementResult r;
                r.shellId = model.shell->elements[e].id;
                std::copy(values.begin() + static_cast<std::ptrdiff_t>(e * 40U), values.begin() + static_cast<std::ptrdiff_t>(e * 40U + 40U), r.values.begin());
                std::copy(axes.begin() + static_cast<std::ptrdiff_t>(e * 9U), axes.begin() + static_cast<std::ptrdiff_t>(e * 9U + 9U), r.axes.begin());
                result.shellResults.push_back(r);
            }
        }
    }

    for (std::size_t i = 0; i < model.members.size(); ++i) {
        auto &memberResult = result.memberResults[i];
        memberResult.memberId = model.members[i].id;
        for (int j = 0; j < 12; ++j) {
            memberResult.localForces[static_cast<std::size_t>(j)] = outMemberLocalForces12[i * 12 + static_cast<std::size_t>(j)];
        }
        for (int j = 0; j < 3; ++j) {
            memberResult.auxK[static_cast<std::size_t>(j)] = outMemberAuxK3[i * 3 + static_cast<std::size_t>(j)];
        }
    }

    {
        std::vector<int> warpIds;
        std::vector<std::size_t> warpIndex;
        for (std::size_t i = 0; i < model.members.size(); ++i) {
            if (model.members[i].warping) {
                warpIds.push_back(model.members[i].id);
                warpIndex.push_back(i);
            }
        }
        using GetWarpingFn = int (*)(int, const int *, double *);
        auto getWarping = reinterpret_cast<GetWarpingFn>(solverLibrary.resolve("structura_get_warping"));
        if (!warpIds.empty() && getWarping) {
            std::vector<double> values(warpIds.size() * 4U, 0.0);
            const bool used = getWarping(static_cast<int>(warpIds.size()), warpIds.data(), values.data()) > 0;
            for (std::size_t k = 0; k < warpIds.size(); ++k) {
                auto &memberResult = result.memberResults[warpIndex[k]];
                const double *v = values.data() + k * 4U;
                memberResult.hasWarping = used;
                memberResult.warpingRate = {v[0], v[1]};
                memberResult.warpingForce = {v[2], v[3]};
            }
        }
    }

    if (model.bucklingModes > 0 && !model.secondOrder) {
        using GetBucklingFn = int (*)(int, int, double *, double *);
        auto getBuckling = reinterpret_cast<GetBucklingFn>(solverLibrary.resolve("structura_get_buckling"));
        if (getBuckling) {
            const int nodeCount = static_cast<int>(totalNodeCount);
            std::vector<double> factors(static_cast<std::size_t>(model.bucklingModes));
            std::vector<double> shapes(static_cast<std::size_t>(model.bucklingModes) * totalNodeCount * 6U);
            const int count = getBuckling(model.bucklingModes, nodeCount, factors.data(), shapes.data());
            for (int m = 0; m < count; ++m) {
                result.bucklingFactors.push_back(factors[static_cast<std::size_t>(m)]);
                const auto begin = shapes.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(m) * totalNodeCount * 6U);
                result.bucklingModes.emplace_back(begin, begin + static_cast<std::ptrdiff_t>(model.nodes.size() * 6U));
            }
        }
    }

    if (model.modalModes > 0 && !model.secondOrder) {
        using GetModalFn = int (*)(int, int, double *, double *, double *);
        auto getModal = reinterpret_cast<GetModalFn>(solverLibrary.resolve("structura_get_modal"));
        if (getModal) {
            const int nodeCount = static_cast<int>(totalNodeCount);
            std::vector<double> omega(static_cast<std::size_t>(model.modalModes));
            std::vector<double> ratios(static_cast<std::size_t>(model.modalModes) * 3U);
            std::vector<double> shapes(static_cast<std::size_t>(model.modalModes) * totalNodeCount * 6U);
            const int count = getModal(model.modalModes, nodeCount, omega.data(), ratios.data(), shapes.data());
            for (int m = 0; m < count; ++m) {
                result.modalOmega.push_back(omega[static_cast<std::size_t>(m)]);
                result.modalMassRatio.push_back({ratios[static_cast<std::size_t>(m) * 3U], ratios[static_cast<std::size_t>(m) * 3U + 1U], ratios[static_cast<std::size_t>(m) * 3U + 2U]});
                const auto begin = shapes.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(m) * totalNodeCount * 6U);
                result.modalShapes.emplace_back(begin, begin + static_cast<std::ptrdiff_t>(model.nodes.size() * 6U));
            }
        }
    }

    {
        // analise incremental nao linear e vinculos: resultados globais da DLL
        using GetSummaryFn = void (*)(double *);
        using GetCurveFn = int (*)(int, double *);
        using GetHingesFn = int (*)(int, int *, double *);
        using GetLinksFn = int (*)(int, int *, double *, double *);
        using GetConstraintForcesFn = int (*)(int, double *);
        auto getSummary = reinterpret_cast<GetSummaryFn>(solverLibrary.resolve("structura_get_nonlinear_summary"));
        auto getCurve = reinterpret_cast<GetCurveFn>(solverLibrary.resolve("structura_get_nonlinear_curve"));
        auto getHinges = reinterpret_cast<GetHingesFn>(solverLibrary.resolve("structura_get_hinge_results"));
        auto getLinks = reinterpret_cast<GetLinksFn>(solverLibrary.resolve("structura_get_link_results"));
        auto getConstraintForces = reinterpret_cast<GetConstraintForcesFn>(solverLibrary.resolve("structura_get_constraint_forces"));
        if (getSummary && getCurve && getHinges && getLinks) {
            double summary[8] = {0, 0, 0, 0, 0, 0, 0, 0};
            getSummary(summary);
            result.nonlinear.used = summary[0] != 0.0;
            if (result.nonlinear.used) {
                result.nonlinear.converged = summary[1] != 0.0;
                result.nonlinear.loadFactor = summary[2];
                result.nonlinear.steps = static_cast<int>(summary[3]);
                result.nonlinear.iterations = static_cast<int>(summary[4]);
                const int curveCount = static_cast<int>(summary[5]), hingeCount = static_cast<int>(summary[6]), linkCount = static_cast<int>(summary[7]);
                std::vector<double> curve(static_cast<std::size_t>(std::max(curveCount, 1)) * 2U);
                const int gotCurve = getCurve(curveCount, curve.data());
                for (int i = 0; i < gotCurve; ++i) {
                    result.nonlinear.curve.push_back({curve[static_cast<std::size_t>(i) * 2U], curve[static_cast<std::size_t>(i) * 2U + 1U]});
                }
                std::vector<int> hingeIds(static_cast<std::size_t>(std::max(hingeCount, 1)) * 2U);
                std::vector<double> hingeValues(static_cast<std::size_t>(std::max(hingeCount, 1)) * 3U);
                const int gotHinges = getHinges(hingeCount, hingeIds.data(), hingeValues.data());
                for (int i = 0; i < gotHinges; ++i) {
                    HingeResult h;
                    h.memberId = hingeIds[static_cast<std::size_t>(i) * 2U];
                    h.dof = hingeIds[static_cast<std::size_t>(i) * 2U + 1U];
                    h.plasticRotation = hingeValues[static_cast<std::size_t>(i) * 3U];
                    h.moment = hingeValues[static_cast<std::size_t>(i) * 3U + 1U];
                    h.yielded = hingeValues[static_cast<std::size_t>(i) * 3U + 2U] != 0.0;
                    result.nonlinear.hinges.push_back(h);
                }
                std::vector<int> linkIds(static_cast<std::size_t>(std::max(linkCount, 1)));
                std::vector<double> deformation(static_cast<std::size_t>(std::max(linkCount, 1)) * 6U), force(static_cast<std::size_t>(std::max(linkCount, 1)) * 6U);
                const int gotLinks = getLinks(linkCount, linkIds.data(), deformation.data(), force.data());
                for (int i = 0; i < gotLinks; ++i) {
                    LinkResult l;
                    l.id = linkIds[static_cast<std::size_t>(i)];
                    for (std::size_t k = 0; k < 6; ++k) {
                        l.deformation[k] = deformation[static_cast<std::size_t>(i) * 6U + k];
                        l.force[k] = force[static_cast<std::size_t>(i) * 6U + k];
                    }
                    result.nonlinear.links.push_back(l);
                }
            }
        }
        if (getConstraintForces) {
            std::vector<double> forces(4096, 0.0);
            const int count = getConstraintForces(static_cast<int>(forces.size()), forces.data());
            result.constraintForces.assign(forces.begin(), forces.begin() + count);
        }
    }

    using GetWarningsFn = int (*)(char *, int);
    if (auto getWarnings = reinterpret_cast<GetWarningsFn>(solverLibrary.resolve("structura_get_warnings"))) {
        std::vector<char> text(8192, '\0');
        if (getWarnings(text.data(), static_cast<int>(text.size())) > 0) {
            result.warnings = QString::fromUtf8(text.data()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        }
    }

    return result;
}

} // namespace Structura::App
