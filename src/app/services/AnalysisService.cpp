#include "AnalysisService.h"

#include "app/LoadSystemUtils.h"
#include "app/services/IdGeneratorService.h"
#include "app/services/SectionPlacementService.h"

#include <QHash>

#include <iomanip>
#include <sstream>

namespace Structura::App::Services {

AnalysisService::BuildResult AnalysisService::buildModel(Structura::App::ISceneModelGateway *sceneController,
                                                         QVector<MaterialInfo> &materials,
                                                         QVector<SectionInfo> &sections,
                                                         const QVector<NodeSupport> &supports,
                                                         const QVector<NodalLoad> &nodalLoads,
                                                         const QVector<MemberLoad> &memberLoads,
                                                         const Structura::App::ProjectData::ProjectExtras &extras,
                                                         const QString &loadCase) const
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
        data.density = mat.unitWeight / 9.80665;   // massa especifica coerente com o sistema de unidades do projeto
        model.materials.push_back(data);
    }

    // Secoes assimetricas (ex.: cantoneiras): o solver recebe as inercias principais e a barra e girada
    // ate os eixos principais (giro do usuario + giro da secao).
    QHash<int, double> principalAngleBySection;
    for (const auto &sec : sections) {
        Structura::App::SolverBridge::SectionData data;
        data.id = sec.externalId;
        data.area = sec.area;
        data.iz = sec.iz;
        data.iy = sec.iy;
        data.j = sec.j;
        const auto principal = Structura::App::Services::SectionPlacementService::principalProperties(sec);
        if (principal.valid && principal.inclined) {
            data.iz = principal.izPrincipal;
            data.iy = principal.iyPrincipal;
            principalAngleBySection.insert(sec.externalId, principal.thetaDeg);
        }
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
        const double principalAngle = principalAngleBySection.value(sectionIt->externalId, 0.0);
        data.sectionRotationDeg = principalAngle;
        for (const auto &placement : extras.placements) {
            if (placement.memberId != bar.externalId) {
                continue;
            }
            using Service = Structura::App::Services::SectionPlacementService;
            const SectionInfo &section = sectionIt.value();
            // Posicao do centroide em relacao a cada no, nos eixos da secao girada pelo usuario.
            auto endOffset = [&](double extraA, double extraB) {
                Structura::App::ProjectData::MemberPlacement end = placement;
                end.offsetA = extraA;
                end.offsetB = extraB;
                return Service::toPrincipalFrame(Service::axisOffset(&section, end), principalAngle);
            };
            const auto startOffset = endOffset(placement.offsetA, placement.offsetB);
            const auto finishOffset = endOffset(placement.endOffsetA(), placement.endOffsetB());
            data.sectionRotationDeg = placement.rotationDeg + principalAngle;
            data.offsetY1 = startOffset.dy;
            data.offsetZ1 = startOffset.dz;
            data.offsetY2 = finishOffset.dy;
            data.offsetZ2 = finishOffset.dz;
            for (int bit = 0; bit < 12; ++bit) {
                data.releases[static_cast<std::size_t>(bit)] = ((placement.releaseMask >> bit) & 1) ? 1 : 0;
            }
            if (placement.memberType != Structura::App::ProjectData::MemberType::kNormal) {
                // trelica e tirante: biarticulados (so esforco axial)
                for (int bit = 0; bit < 12; ++bit) {
                    if ((Structura::App::ProjectData::MemberType::kReleaseRotations >> bit) & 1) {
                        data.releases[static_cast<std::size_t>(bit)] = 1;
                    }
                }
                data.tensionOnly = (placement.memberType == Structura::App::ProjectData::MemberType::kTie);
            }
            break;
        }
        model.members.push_back(data);
    }

    // Com `loadCase` vazio, todas as cargas entram (compatibilidade); senao so as do caso.
    const QString defaultCase = extras.loadCases.isEmpty() ? Structura::App::ProjectData::defaultLoadCaseName()
                                                           : extras.loadCases.first().name;
    auto belongsToCase = [&](const QString &name) {
        if (loadCase.isEmpty()) {
            return true;
        }
        return (name.isEmpty() ? defaultCase : name) == loadCase;
    };

    for (const auto &load : nodalLoads) {
        if (!belongsToCase(load.loadCase)) {
            continue;
        }
        Structura::App::SolverBridge::NodalLoadData data;
        data.nodeId = load.nodeId;
        data.values = {{load.fx, load.fy, load.fz, load.mx, load.my, load.mz}};
        model.nodalLoads.push_back(data);
    }

    for (const auto &load : memberLoads) {
        if (!belongsToCase(load.loadCase)) {
            continue;
        }
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

    for (const auto &moment : extras.memberMoments) {
        if (!belongsToCase(moment.loadCase)) {
            continue;
        }
        Structura::App::SolverBridge::MemberMomentData data;
        data.memberId = moment.memberId;
        const QString system = Structura::App::normalizeMemberLoadSystem(moment.system);
        data.coordSystem = (system == QLatin1String("GLOBAL"))
            ? Structura::App::SolverBridge::MemberLoadCoordSystem::Global
            : Structura::App::SolverBridge::MemberLoadCoordSystem::Local;
        data.mx = moment.mx;
        data.my = moment.my;
        data.mz = moment.mz;
        model.memberMoments.push_back(data);
    }

    // Peso proprio: carga distribuida global -Z = gama (kN/m3) x area da secao (m2).
    bool includeSelfWeight = false;
    for (const auto &candidate : extras.loadCases) {
        if (candidate.name == loadCase && candidate.selfWeight) {
            includeSelfWeight = true;
        }
    }
    if (includeSelfWeight) {
        for (const auto &bar : bars) {
            const auto materialIt = materialsById.constFind(bar.materialId);
            const auto sectionIt = sectionsById.constFind(bar.sectionId);
            if (materialIt == materialsById.constEnd() || sectionIt == sectionsById.constEnd()) {
                continue;
            }
            const double weight = materialIt->unitWeight * sectionIt->area;
            if (weight == 0.0) {
                continue;
            }
            Structura::App::SolverBridge::MemberLoadData data;
            data.memberId = bar.externalId;
            data.coordSystem = Structura::App::SolverBridge::MemberLoadCoordSystem::Global;
            data.qz = -weight;
            model.memberLoads.push_back(data);
        }
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
