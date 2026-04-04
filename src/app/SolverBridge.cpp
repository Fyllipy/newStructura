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
        // Base name lets Qt resolve solver_dll.dll on Windows and libsolver_dll.so on Linux.
        solverLibrary.setFileName(QStringLiteral("solver_dll"));
    }

    if (!solverLibrary.load()) {
        throw SolverBridgeError(errorFromLibrary(solverLibrary));
    }

    auto *rawSymbol = solverLibrary.resolve("structura_solve");
    if (!rawSymbol) {
        throw SolverBridgeError("Failed to resolve exported function structura_solve.");
    }
    const auto solveFn = reinterpret_cast<StructuraSolveFn>(rawSymbol);

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

    std::vector<int> memberIds;
    std::vector<int> memberNodeI;
    std::vector<int> memberNodeJ;
    std::vector<int> memberMaterialId;
    std::vector<int> memberSectionId;
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

    std::vector<double> outNodeDisplacements6(model.nodes.size() * 6, 0.0);
    std::vector<double> outNodeReactions6(model.nodes.size() * 6, 0.0);
    std::vector<double> outMemberLocalForces12(model.members.size() * 12, 0.0);
    std::vector<double> outMemberAuxK3(model.members.size() * 3, 0.0);

    std::vector<char> errorBuffer(2048, '\0');

    const int returnCode = solveFn(
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

    return result;
}

} // namespace Structura::App
