#pragma once

#include <QUuid>

#include <vector>

#include "app/SceneDataTypes.h"
#include "core/model/Node.h"

namespace Structura::App {

class ISceneModelGateway
{
public:
    virtual ~ISceneModelGateway() = default;

    virtual std::vector<SceneNodeInfo> nodeInfos() const = 0;
    virtual std::vector<SceneBarInfo> bars() const = 0;

    virtual QUuid addPointWithId(double x, double y, double z, int externalId) = 0;
    virtual Structura::Model::Node *findNode(const QUuid &id) = 0;
    virtual QUuid addBar(const QUuid &startNodeId,
                         const QUuid &endNodeId,
                         const QUuid &materialId,
                         const QUuid &sectionId) = 0;
    virtual void setBarExternalId(const QUuid &barId, int externalId) = 0;
};

} // namespace Structura::App
