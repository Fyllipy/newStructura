#include "LoadSupportVisualMapper.h"

#include "app/LoadSystemUtils.h"
#include "app/LookupUtils.h"

#include <QHash>

#include <cmath>

namespace Structura::Viz {

QVector<SceneController::NodalLoadVisual> LoadSupportVisualMapper::mapNodalLoadVisuals(const SceneController *sceneController,
                                                                                         const QVector<NodalLoad> &nodalLoads)
{
    QVector<SceneController::NodalLoadVisual> nodalVisuals;
    if (!sceneController) {
        return nodalVisuals;
    }

    const auto nodeInfos = sceneController->nodeInfos();
    const QHash<int, SceneController::NodeInfo> nodesByExternal =
        Structura::App::buildLookup<int, SceneController::NodeInfo>(
            nodeInfos,
            [](const SceneController::NodeInfo &info) { return info.externalId; });

    nodalVisuals.reserve(nodalLoads.size());
    for (const auto &load : nodalLoads) {
        const auto it = nodesByExternal.constFind(load.nodeId);
        if (it == nodesByExternal.constEnd()) {
            continue;
        }

        SceneController::NodalLoadVisual visual;
        visual.position = QVector3D(static_cast<float>(it->x), static_cast<float>(it->y), static_cast<float>(it->z));
        visual.force = QVector3D(static_cast<float>(load.fx), static_cast<float>(load.fy), static_cast<float>(load.fz));
        visual.moment = QVector3D(static_cast<float>(load.mx), static_cast<float>(load.my), static_cast<float>(load.mz));
        nodalVisuals.append(visual);
    }

    return nodalVisuals;
}

QVector<SceneController::MemberLoadVisual> LoadSupportVisualMapper::mapMemberLoadVisuals(const SceneController *sceneController,
                                                                                           const QVector<MemberLoad> &memberLoads)
{
    QVector<SceneController::MemberLoadVisual> memberVisuals;
    if (!sceneController) {
        return memberVisuals;
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

    memberVisuals.reserve(memberLoads.size());
    for (const auto &load : memberLoads) {
        const auto barIt = barsByExternal.constFind(load.memberId);
        if (barIt == barsByExternal.constEnd()) {
            continue;
        }

        const auto startNodeIt = nodesById.constFind(barIt->startNodeId);
        const auto endNodeIt = nodesById.constFind(barIt->endNodeId);
        if (startNodeIt == nodesById.constEnd() || endNodeIt == nodesById.constEnd()) {
            continue;
        }

        const QVector3D start(static_cast<float>(startNodeIt->x), static_cast<float>(startNodeIt->y), static_cast<float>(startNodeIt->z));
        const QVector3D end(static_cast<float>(endNodeIt->x), static_cast<float>(endNodeIt->y), static_cast<float>(endNodeIt->z));
        const QVector3D barVector = end - start;
        if (barVector.lengthSquared() < 1e-6f) {
            continue;
        }

        QVector3D loadVector(static_cast<float>(load.qx), static_cast<float>(load.qy), static_cast<float>(load.qz));
        const bool isLocal = Structura::App::isLocalMemberLoadSystem(load.system);

        if (isLocal) {
            QVector3D xDir = barVector.normalized();
            QVector3D reference(0.0f, 0.0f, 1.0f);
            if (std::abs(QVector3D::dotProduct(xDir, reference)) > 0.95f) {
                reference = QVector3D(0.0f, 1.0f, 0.0f);
            }

            QVector3D yDir = QVector3D::crossProduct(reference, xDir);
            if (yDir.lengthSquared() < 1e-6f) {
                reference = QVector3D(1.0f, 0.0f, 0.0f);
                yDir = QVector3D::crossProduct(reference, xDir);
            }
            if (yDir.lengthSquared() < 1e-6f) {
                yDir = QVector3D(0.0f, 1.0f, 0.0f);
            }
            yDir.normalize();

            QVector3D zDir = QVector3D::crossProduct(xDir, yDir);
            if (zDir.lengthSquared() < 1e-6f) {
                zDir = QVector3D(0.0f, 0.0f, 1.0f);
            }
            zDir.normalize();

            loadVector = static_cast<float>(load.qx) * xDir
                       + static_cast<float>(load.qy) * yDir
                       + static_cast<float>(load.qz) * zDir;
        }

        if (loadVector.lengthSquared() < 1e-6f) {
            continue;
        }

        SceneController::MemberLoadVisual visual;
        visual.position = (start + end) * 0.5f;
        visual.force = loadVector;
        visual.barVector = barVector;
        visual.localSystem = isLocal;
        memberVisuals.append(visual);
    }

    return memberVisuals;
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
