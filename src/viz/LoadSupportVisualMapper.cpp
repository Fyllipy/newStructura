#include "LoadSupportVisualMapper.h"

#include "app/LoadSystemUtils.h"
#include "app/LookupUtils.h"
#include "viz/LocalCoordinateSystem.h"

#include <QHash>

#include <cmath>

namespace Structura::Viz {

namespace {

bool inActiveCase(const QString &loadCase, const QString &active, const QString &defaultCase)
{
    if (active.isEmpty()) {
        return true;
    }
    return (loadCase.isEmpty() ? defaultCase : loadCase) == active;
}

QString defaultCaseOf(const QVector<Structura::App::ProjectData::LoadCase> *cases)
{
    return (cases && !cases->isEmpty()) ? cases->first().name : Structura::App::ProjectData::defaultLoadCaseName();
}

bool isCaseShown(const QString &loadCase, const LoadSupportVisualMapper::LoadContext &context, const QString &defaultCase)
{
    // a lista de casos visiveis (janela "Casos visiveis") tem precedencia sobre o caso ativo da barra de vista
    if (context.filterVisibleCases) {
        return context.visibleCases.contains(loadCase.isEmpty() ? defaultCase : loadCase);
    }
    return inActiveCase(loadCase, context.activeCase, defaultCase);
}

} // namespace

QVector<SceneController::NodalLoadVisual> LoadSupportVisualMapper::mapNodalLoadVisuals(const SceneController *sceneController,
                                                                                         const LoadContext &context)
{
    QVector<SceneController::NodalLoadVisual> nodalVisuals;
    if (!sceneController || !context.nodalLoads) {
        return nodalVisuals;
    }

    const auto nodeInfos = sceneController->nodeInfos();
    const QHash<int, SceneController::NodeInfo> nodesByExternal =
        Structura::App::buildLookup<int, SceneController::NodeInfo>(
            nodeInfos,
            [](const SceneController::NodeInfo &info) { return info.externalId; });

    const QString defaultCase = defaultCaseOf(context.loadCases);
    for (int loadIndex = 0; loadIndex < context.nodalLoads->size(); ++loadIndex) {
        const auto &load = (*context.nodalLoads)[loadIndex];
        if (!isCaseShown(load.loadCase, context, defaultCase)) {
            continue;
        }
        const auto it = nodesByExternal.constFind(load.nodeId);
        if (it == nodesByExternal.constEnd()) {
            continue;
        }

        SceneController::NodalLoadVisual visual;
        visual.position = QVector3D(static_cast<float>(it->x), static_cast<float>(it->y), static_cast<float>(it->z));
        visual.force = QVector3D(static_cast<float>(load.fx), static_cast<float>(load.fy), static_cast<float>(load.fz));
        visual.moment = QVector3D(static_cast<float>(load.mx), static_cast<float>(load.my), static_cast<float>(load.mz));
        visual.sourceKind = 0;
        visual.sourceIndex = loadIndex;
        nodalVisuals.append(visual);
    }

    return nodalVisuals;
}

QVector<SceneController::MemberActionVisual> LoadSupportVisualMapper::mapMemberActionVisuals(const SceneController *sceneController,
                                                                                               const LoadContext &context)
{
    QVector<SceneController::MemberActionVisual> visuals;
    if (!sceneController) {
        return visuals;
    }

    const auto nodeInfos = sceneController->nodeInfos();
    const QHash<QUuid, SceneController::NodeInfo> nodesById =
        Structura::App::buildLookup<QUuid, SceneController::NodeInfo>(
            nodeInfos,
            [](const SceneController::NodeInfo &info) { return info.id; });

    const auto barInfos = sceneController->bars();
    const QHash<int, SceneController::BarInfo> barsByExternal =
        Structura::App::buildPositiveExternalLookup<SceneController::BarInfo>(
            barInfos,
            [](const SceneController::BarInfo &info) { return info.externalId; },
            [](const SceneController::BarInfo &info) { return info.externalId > 0; });

    const QString defaultCase = defaultCaseOf(context.loadCases);
    Structura::Geometry::DefaultLocalAxisProvider axisProvider;

    // Geometria e eixos locais (mesma convencao do solver) de uma barra.
    auto makeBase = [&](int memberId, SceneController::MemberActionVisual &visual) {
        const auto barIt = barsByExternal.constFind(memberId);
        if (barIt == barsByExternal.constEnd()) {
            return false;
        }
        const auto startIt = nodesById.constFind(barIt->startNodeId);
        const auto endIt = nodesById.constFind(barIt->endNodeId);
        if (startIt == nodesById.constEnd() || endIt == nodesById.constEnd()) {
            return false;
        }
        const std::array<double, 3> a {startIt->x, startIt->y, startIt->z};
        const std::array<double, 3> b {endIt->x, endIt->y, endIt->z};
        const double dx = b[0] - a[0];
        const double dy = b[1] - a[1];
        const double dz = b[2] - a[2];
        if (dx * dx + dy * dy + dz * dz < 1e-12) {
            return false;
        }
        double rotationDeg = 0.0;
        if (context.placements) {
            for (const auto &placement : *context.placements) {
                if (placement.memberId == memberId) {
                    rotationDeg = placement.rotationDeg;
                    break;
                }
            }
        }
        const auto lcs = Structura::Geometry::rotatedAboutX(axisProvider.computeLCS(a, b, std::nullopt), rotationDeg);
        auto toVector = [](const std::array<double, 3> &v) {
            return QVector3D(static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2]));
        };
        visual.start = toVector(a);
        visual.end = toVector(b);
        visual.xAxis = toVector(lcs.xPrime);
        visual.yAxis = toVector(lcs.yPrime);
        visual.zAxis = toVector(lcs.zPrime);
        return true;
    };

    if (context.memberLoads) {
        for (int loadIndex = 0; loadIndex < context.memberLoads->size(); ++loadIndex) {
            const auto &load = (*context.memberLoads)[loadIndex];
            if (!isCaseShown(load.loadCase, context, defaultCase)) {
                continue;
            }
            SceneController::MemberActionVisual visual;
            if (!makeBase(load.memberId, visual)) {
                continue;
            }
            visual.values = QVector3D(static_cast<float>(load.qx), static_cast<float>(load.qy), static_cast<float>(load.qz));
            visual.local = Structura::App::isLocalMemberLoadSystem(load.system);
            visual.moment = false;
            visual.sourceKind = 1;
            visual.sourceIndex = loadIndex;
            if (!visual.values.isNull()) {
                visuals.append(visual);
            }
        }
    }

    if (context.memberMoments) {
        for (int loadIndex = 0; loadIndex < context.memberMoments->size(); ++loadIndex) {
            const auto &moment = (*context.memberMoments)[loadIndex];
            if (!isCaseShown(moment.loadCase, context, defaultCase)) {
                continue;
            }
            SceneController::MemberActionVisual visual;
            if (!makeBase(moment.memberId, visual)) {
                continue;
            }
            visual.values = QVector3D(static_cast<float>(moment.mx), static_cast<float>(moment.my), static_cast<float>(moment.mz));
            visual.local = Structura::App::isLocalMemberLoadSystem(moment.system);
            visual.moment = true;
            visual.sourceKind = 2;
            visual.sourceIndex = loadIndex;
            if (!visual.values.isNull()) {
                visuals.append(visual);
            }
        }
    }

    // Peso proprio: gama (kN/m3) x area (m2), global -Z, somado por caso que o inclui.
    if (context.loadCases && context.materials && context.sections) {
        int selfWeightCases = 0;
        for (const auto &loadCase : *context.loadCases) {
            if (loadCase.selfWeight && isCaseShown(loadCase.name, context, defaultCase)) {
                ++selfWeightCases;
            }
        }
        if (selfWeightCases > 0) {
            QHash<QUuid, double> gammaByMaterial;
            for (const auto &material : *context.materials) {
                gammaByMaterial.insert(material.uuid, material.unitWeight);
            }
            QHash<QUuid, double> areaBySection;
            for (const auto &section : *context.sections) {
                areaBySection.insert(section.uuid, section.area);
            }
            for (const auto &bar : barInfos) {
                const double weight = gammaByMaterial.value(bar.materialId, 0.0) * areaBySection.value(bar.sectionId, 0.0)
                                      * selfWeightCases;
                if (weight == 0.0 || bar.externalId <= 0) {
                    continue;
                }
                SceneController::MemberActionVisual visual;
                if (!makeBase(bar.externalId, visual)) {
                    continue;
                }
                visual.values = QVector3D(0.0f, 0.0f, static_cast<float>(-weight));
                visual.local = false;
                visual.selfWeight = true;
                visual.sourceKind = 3;
                visual.sourceIndex = bar.externalId;
                visuals.append(visual);
            }
        }
    }

    return visuals;
}

QVector<SceneController::SupportVisual> LoadSupportVisualMapper::mapSupportVisuals(const SceneController *sceneController,
                                                                                    const QVector<NodeSupport> &supports)
{
    QVector<SceneController::SupportVisual> supportVisuals;
    if (!sceneController) {
        return supportVisuals;
    }

    const auto nodeInfos = sceneController->nodeInfos();
    const QHash<int, SceneController::NodeInfo> nodesByExternal =
        Structura::App::buildLookup<int, SceneController::NodeInfo>(
            nodeInfos,
            [](const SceneController::NodeInfo &info) { return info.externalId; });

    supportVisuals.reserve(supports.size());
    for (const auto &support : supports) {
        const auto it = nodesByExternal.constFind(support.nodeId);
        if (it == nodesByExternal.constEnd()) {
            continue;
        }

        SceneController::SupportVisual visual;
        visual.position = QVector3D(static_cast<float>(it->x), static_cast<float>(it->y), static_cast<float>(it->z));
        for (int i = 0; i < 6; ++i) {
            visual.restraints[i] = support.restraints[i];
        }
        supportVisuals.append(visual);
    }

    return supportVisuals;
}

} // namespace Structura::Viz
