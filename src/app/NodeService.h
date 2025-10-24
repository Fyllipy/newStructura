#pragma once

#include "IModelRepository.h"
#include "../core/model/Vector3.h"
#include <QObject>
#include <QUuid>
#include <memory>
#include <optional>

namespace Structura::App {

using Vector3 = Structura::Model::Vector3;

/**
 * @brief Service for managing Node entities.
 * 
 * This service provides high-level operations for node management,
 * including creation, modification, deletion, and queries.
 * It emits signals for UI updates when nodes change.
 */
class NodeService : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct NodeService with a repository.
     * @param repository Pointer to model repository (must not be null)
     * @param parent Qt parent object
     */
    explicit NodeService(IModelRepository *repository, QObject *parent = nullptr);
    ~NodeService() override = default;

    /**
     * @brief Create a new node at the specified position.
     * @param position 3D position for the node
     * @return UUID of the created node
     */
    QUuid createNode(const Vector3 &position);
    
    /**
     * @brief Create a new node with a specific external ID.
     * @param position 3D position for the node
     * @param externalId External ID to assign
     * @return UUID of the created node, or null UUID if externalId already exists
     */
    QUuid createNodeWithExternalId(const Vector3 &position, int externalId);
    
    /**
     * @brief Delete a node by its UUID.
     * @param id UUID of the node to delete
     * @return true if node was deleted, false if not found
     */
    bool deleteNode(const QUuid &id);
    
    /**
     * @brief Update the position of a node.
     * @param id UUID of the node
     * @param newPosition New position for the node
     * @return true if successful, false if node not found
     */
    bool setNodePosition(const QUuid &id, const Vector3 &newPosition);
    
    /**
     * @brief Set restraints for a node.
     * @param id UUID of the node
     * @param restraints Array of 6 restraints [UX, UY, UZ, RX, RY, RZ]
     * @return true if successful, false if node not found
     */
    bool setNodeRestraints(const QUuid &id, const std::array<bool, 6> &restraints);
    
    /**
     * @brief Find a node by its UUID.
     * @param id UUID to search for
     * @return Optional containing the node if found
     */
    std::optional<Node> findNode(const QUuid &id) const;
    
    /**
     * @brief Find a node by its external ID.
     * @param externalId External ID to search for
     * @return Optional containing the node if found
     */
    std::optional<Node> findNodeByExternalId(int externalId) const;
    
    /**
     * @brief Get all nodes.
     * @return Vector of all nodes
     */
    std::vector<Node> allNodes() const;
    
    /**
     * @brief Get the count of nodes.
     * @return Number of nodes
     */
    size_t nodeCount() const;
    
    /**
     * @brief Generate the next available external ID for nodes.
     * @return Next external ID
     */
    int nextExternalId() const;
    
    /**
     * @brief Check if a node exists with the given UUID.
     * @param id UUID to check
     * @return true if node exists
     */
    bool nodeExists(const QUuid &id) const;

signals:
    /**
     * @brief Emitted when a node is created.
     * @param nodeId UUID of the created node
     */
    void nodeCreated(const QUuid &nodeId);
    
    /**
     * @brief Emitted when a node is deleted.
     * @param nodeId UUID of the deleted node
     */
    void nodeDeleted(const QUuid &nodeId);
    
    /**
     * @brief Emitted when a node is updated.
     * @param nodeId UUID of the updated node
     */
    void nodeUpdated(const QUuid &nodeId);
    
    /**
     * @brief Emitted when the position of a node changes.
     * @param nodeId UUID of the node
     * @param newPosition New position
     */
    void nodePositionChanged(const QUuid &nodeId, const Vector3 &newPosition);

private:
    IModelRepository *m_repository;
};

} // namespace Structura::App
