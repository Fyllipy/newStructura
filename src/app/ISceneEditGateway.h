#pragma once

#include <QVector>
#include <QVector3D>
#include <QUuid>

#include <optional>
#include <vector>

#include "core/model/Bar.h"
#include "core/model/Node.h"

namespace Structura::App {

class ISceneEditGateway
{
public:
    virtual ~ISceneEditGateway() = default;

    virtual const Structura::Model::Node *findNode(const QUuid &id) const = 0;
    virtual const Structura::Model::Bar *findBar(const QUuid &id) const = 0;

    virtual bool updateNodePositions(const QVector<QUuid> &nodeIds,
                                     const QVector<QVector3D> &positions) = 0;

    virtual void assignBarProperties(const std::vector<QUuid> &barIds,
                                     const std::optional<QUuid> &materialId,
                                     const std::optional<QUuid> &sectionId) = 0;
};

} // namespace Structura::App
