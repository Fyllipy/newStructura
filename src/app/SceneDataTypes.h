#pragma once

#include <QUuid>

namespace Structura::App {

struct SceneNodeInfo {
    QUuid id;
    int externalId;
    double x;
    double y;
    double z;
};

struct SceneBarInfo {
    QUuid id;
    QUuid startNodeId;
    QUuid endNodeId;
    QUuid materialId;
    QUuid sectionId;
    int externalId {0};
};

} // namespace Structura::App
