#include "SolverBridge.h"

#include <QLibrary>

#include <algorithm>
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
                                int, const int *, const int *, const int *, const double *,
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
        std::vector<int> shellIds, shellNodes4, loadShells, loadKinds, tieSlaves, tieCounts, tieMasters, springNodes, springCompression;
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
        }
        for (const auto &s : model.shell->springs) {
            springNodes.push_back(s.node);
            springK.insert(springK.end(), s.k.begin(), s.k.end());
            springCompression.push_back(s.compressionOnly ? 1 : 0);
            springNormals.insert(springNormals.end(), s.normal.begin(), s.normal.end());
        }
        const int code = setShells(static_cast<int>(shellIds.size()), shellIds.data(), shellNodes4.data(), shellE.data(), shellNu.data(), shellDensity.data(),
                                   shellThickness.data(), static_cast<int>(loadShells.size()), loadShells.data(), loadKinds.data(), loadValues.data(),
                                   static_cast<int>(tieSlaves.size()), tieSlaves.data(), tieCounts.data(), tieMasters.data(), tieWeights.data(),
                                   static_cast<int>(springNodes.size()), springNodes.data(), springK.data(), springCompression.data(), springNormals.data());
        if (code != 0) {
            throw SolverBridgeError("Dados de cascas invalidos para o solver.");
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

    return result;
}

} // namespace Structura::App
