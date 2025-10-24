#include "NodeService.h"
#include <algorithm>

namespace Structura::App {

NodeService::NodeService(IModelRepository *repository, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
{
    Q_ASSERT(repository != nullptr);
}

QUuid NodeService::createNode(const Vector3 &position)
{
    int externalId = nextExternalId();
    QUuid id = QUuid::createUuid();
    
    Node node(id, externalId, position);
    
    if (m_repository->addNode(node)) {
        emit nodeCreated(id);
        return id;
    }
    
    return QUuid(); // Should not happen with unique UUID
}

QUuid NodeService::createNodeWithExternalId(const Vector3 &position, int externalId)
{
    // Check if external ID already exists
    if (m_repository->findNodeByExternalId(externalId).has_value()) {
        return QUuid(); // External ID conflict
    }
    
    QUuid id = QUuid::createUuid();
    Node node(id, externalId, position);
    
    if (m_repository->addNode(node)) {
        emit nodeCreated(id);
        return id;
    }
    
    return QUuid();
}

bool NodeService::deleteNode(const QUuid &id)
{
    if (m_repository->removeNode(id)) {
        emit nodeDeleted(id);
        return true;
    }
    return false;
}

bool NodeService::setNodePosition(const QUuid &id, const Vector3 &newPosition)
{
    auto nodeOpt = m_repository->findNode(id);
    if (!nodeOpt.has_value()) {
        return false;
    }
    
    Node node = nodeOpt.value();
    node.setPosition(newPosition);
    
    if (m_repository->updateNode(node)) {
        emit nodePositionChanged(id, newPosition);
        emit nodeUpdated(id);
        return true;
    }
    
    return false;
}

bool NodeService::setNodeRestraints(const QUuid &id, const std::array<bool, 6> &restraints)
{
    auto nodeOpt = m_repository->findNode(id);
    if (!nodeOpt.has_value()) {
        return false;
    }
    
    Node node = nodeOpt.value();
    for (int i = 0; i < 6; ++i) {
        node.setRestraint(i, restraints[static_cast<size_t>(i)]);
    }
    
    if (m_repository->updateNode(node)) {
        emit nodeUpdated(id);
        return true;
    }
    
    return false;
}

std::optional<Node> NodeService::findNode(const QUuid &id) const
{
    return m_repository->findNode(id);
}

std::optional<Node> NodeService::findNodeByExternalId(int externalId) const
{
    return m_repository->findNodeByExternalId(externalId);
}

std::vector<Node> NodeService::allNodes() const
{
    return m_repository->allNodes();
}

size_t NodeService::nodeCount() const
{
    return m_repository->nodeCount();
}

int NodeService::nextExternalId() const
{
    auto nodes = m_repository->allNodes();
    if (nodes.empty()) {
        return 1;
    }
    
    int maxId = 0;
    for (const auto &node : nodes) {
        if (node.externalId() > maxId) {
            maxId = node.externalId();
        }
    }
    
    return maxId + 1;
}

bool NodeService::nodeExists(const QUuid &id) const
{
    return m_repository->findNode(id).has_value();
}

} // namespace Structura::App
