#include "AnalysisService.h"

#include "app/LoadSystemUtils.h"
#include "app/services/IdGeneratorService.h"

#include <QHash>

#include <iomanip>
#include <sstream>

namespace Structura::App::Services {

AnalysisService::BuildResult AnalysisService::buildModel(Structura::App::ISceneModelGateway *sceneController,
                                                         QVector<MaterialInfo> &materials,
                                                         QVector<SectionInfo> &sections,
                                                         const QVector<NodeSupport> &supports,
                                                         const QVector<NodalLoad> &nodalLoads,
                                                         const QVector<MemberLoad> &memberLoads) const
{
    BuildResult result;
    if (!sceneController) {
        result.errors.append(QStringLiteral("Scene controller indisponivel."));
        return result;
    }

    auto bars = sceneController->bars();
    auto nodes = sceneController->nodeInfos();

    IdGeneratorService::ensureMaterialExternalIds(materials);
    IdGeneratorService::ensureSectionExternalIds(sections);

    IdGeneratorService::ensureBarExternalIds(sceneController);
    bars = sceneController->bars();

    std::sort(nodes.begin(), nodes.end(), [](const Structura::App::SceneNodeInfo &a, const Structura::App::SceneNodeInfo &b) {
        return a.externalId < b.externalId;
    });
    std::sort(bars.begin(), bars.end(), [](const Structura::App::SceneBarInfo &a, const Structura::App::SceneBarInfo &b) {
        return a.externalId < b.externalId;
    });

    QHash<QUuid, Structura::App::SceneNodeInfo> nodeInfoMap;
    for (const auto &node : nodes) {
        nodeInfoMap.insert(node.id, node);
    }

    QHash<int, NodeSupport> supportMap;
    for (const auto &support : supports) {
        supportMap.insert(support.nodeId, support);
    }

    QHash<QUuid, MaterialInfo> materialsById;
    for (const auto &mat : materials) {
        materialsById.insert(mat.uuid, mat);
    }

    QHash<QUuid, SectionInfo> sectionsById;
    for (const auto &sec : sections) {
        sectionsById.insert(sec.uuid, sec);
    }

    auto &model = result.model;
    model.materials.reserve(static_cast<std::size_t>(materials.size()));
    model.sections.reserve(static_cast<std::size_t>(sections.size()));
    model.nodes.reserve(nodes.size());
    model.members.reserve(bars.size());
    model.nodalLoads.reserve(static_cast<std::size_t>(nodalLoads.size()));
    model.memberLoads.reserve(static_cast<std::size_t>(memberLoads.size()));

    for (const auto &mat : materials) {
        Structura::App::SolverBridge::MaterialData data;
        data.id = mat.externalId;
        data.e = mat.youngModulus;
        data.g = mat.shearModulus;
        model.materials.push_back(data);
    }

    for (const auto &sec : sections) {
        Structura::App::SolverBridge::SectionData data;
        data.id = sec.externalId;
        data.area = sec.area;
        data.iz = sec.iz;
        data.iy = sec.iy;
        data.j = sec.j;
        model.sections.push_back(data);
    }

    for (const auto &node : nodes) {
        Structura::App::SolverBridge::NodeData data;
        data.id = node.externalId;
        data.x = node.x;
        data.y = node.y;
        data.z = node.z;
        const auto supportIt = supportMap.constFind(node.externalId);
        if (supportIt != supportMap.constEnd()) {
            for (int i = 0; i < 6; ++i) {
                data.restraints[static_cast<std::size_t>(i)] = supportIt.value().restraints[i] ? 1 : 0;
            }
        }
        model.nodes.push_back(data);
    }

    for (const auto &bar : bars) {
        const auto startIt = nodeInfoMap.constFind(bar.startNodeId);
        const auto endIt = nodeInfoMap.constFind(bar.endNodeId);
        const auto materialIt = materialsById.constFind(bar.materialId);
        const auto sectionIt = sectionsById.constFind(bar.sectionId);

        if (startIt == nodeInfoMap.constEnd() || endIt == nodeInfoMap.constEnd()) {
            result.errors.append(QStringLiteral("Barra %1 referencia no(s) inexistente(s).").arg(bar.externalId));
            continue;
        }
        if (materialIt == materialsById.constEnd() || materialIt->externalId <= 0) {
            result.errors.append(QStringLiteral("Barra %1 sem material valido.").arg(bar.externalId));
            continue;
        }
        if (sectionIt == sectionsById.constEnd() || sectionIt->externalId <= 0) {
            result.errors.append(QStringLiteral("Barra %1 sem secao valida.").arg(bar.externalId));
            continue;
        }

        Structura::App::SolverBridge::MemberData data;
        data.id = bar.externalId;
        data.nodeI = startIt.value().externalId;
        data.nodeJ = endIt.value().externalId;
        data.materialId = materialIt->externalId;
        data.sectionId = sectionIt->externalId;
        model.members.push_back(data);
    }

    for (const auto &load : nodalLoads) {
        Structura::App::SolverBridge::NodalLoadData data;
        data.nodeId = load.nodeId;
        data.values = {{load.fx, load.fy, load.fz, load.mx, load.my, load.mz}};
        model.nodalLoads.push_back(data);
    }

    for (const auto &load : memberLoads) {
        Structura::App::SolverBridge::MemberLoadData data;
        data.memberId = load.memberId;
        const QString system = Structura::App::normalizeMemberLoadSystem(load.system);
        data.coordSystem = (system == QLatin1String("GLOBAL"))
            ? Structura::App::SolverBridge::MemberLoadCoordSystem::Global
            : Structura::App::SolverBridge::MemberLoadCoordSystem::Local;
        data.qx = load.qx;
        data.qy = load.qy;
        data.qz = load.qz;
        model.memberLoads.push_back(data);
    }

    result.success = result.errors.isEmpty();
    return result;
}

QString AnalysisService::formatReport(const Structura::App::SolverBridge::Result &result) const
{
    std::ostringstream report;
    report.setf(std::ios::scientific);
    report << std::setprecision(6);

    report << "ANALISE ESTRUTURAL - RESULTADOS (PROVISORIO)\n";
    report << "==========================================\n\n";
    report << "Nos resolvidos: " << result.nodeResults.size() << "\n";
    report << "Barras resolvidas: " << result.memberResults.size() << "\n\n";

    report << "[DESLOCAMENTOS E REACOES NODAIS]\n";
    report << "# No      UX            UY            UZ            RX            RY            RZ"
           << " | Fx            Fy            Fz            Mx            My            Mz\n";
    for (const auto &nodeResult : result.nodeResults) {
        report << std::setw(6) << nodeResult.nodeId << " ";
        for (const double value : nodeResult.displacements) {
            report << std::setw(13) << value << " ";
        }
        report << "| ";
        for (const double value : nodeResult.reactions) {
            report << std::setw(13) << value << " ";
        }
        report << "\n";
    }

    report << "\n[ESFORCOS LOCAIS EM BARRAS]\n";
    report << "# Barra   N_i           Vy_i          Vz_i          T_i           My_i          Mz_i"
           << "          N_j           Vy_j          Vz_j          T_j           My_j          Mz_j"
           << " | k1            k2            k3\n";
    for (const auto &memberResult : result.memberResults) {
        report << std::setw(7) << memberResult.memberId << " ";
        for (const double value : memberResult.localForces) {
            report << std::setw(13) << value << " ";
        }
        report << "| ";
        for (const double value : memberResult.auxK) {
            report << std::setw(13) << value << " ";
        }
        report << "\n";
    }

    return QString::fromStdString(report.str());
}

} // namespace Structura::App::Services
