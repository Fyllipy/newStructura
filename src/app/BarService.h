#pragma once

#include "IModelRepository.h"
#include "../core/model/Vector3.h"
#include <QObject>
#include <QUuid>
#include <memory>
#include <optional>
#include <vector>

namespace Structura::App {

using Vector3 = Structura::Model::Vector3;

/**
 * @brief Service for managing Bar entities.
 * 
 * This service provides high-level operations for bar (structural element) management,
 * including creation, modification, deletion, and queries.
 * It emits signals for UI updates when bars change.
 */
class BarService : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct BarService with a repository.
     * @param repository Pointer to model repository (must not be null)
     * @param parent Qt parent object
     */
    explicit BarService(IModelRepository *repository, QObject *parent = nullptr);
    ~BarService() override = default;

    /**
     * @brief Create a new bar connecting two nodes.
     * @param startNodeId UUID of the start node
     * @param endNodeId UUID of the end node
     * @param materialId Optional UUID of the material
     * @param sectionId Optional UUID of the section
     * @return UUID of the created bar, or null UUID if nodes don't exist or are the same
     */
    QUuid createBar(const QUuid &startNodeId,
                    const QUuid &endNodeId,
                    const QUuid &materialId = QUuid(),
                    const QUuid &sectionId = QUuid());
    
    /**
     * @brief Delete a bar by its UUID.
     * @param id UUID of the bar to delete
     * @return true if bar was deleted, false if not found
     */
    bool deleteBar(const QUuid &id);
    
    /**
     * @brief Update bar connectivity (change connected nodes).
     * @param id UUID of the bar
     * @param startNodeId New start node UUID
     * @param endNodeId New end node UUID
     * @return true if successful, false if bar not found or nodes invalid
     */
    bool updateBarConnectivity(const QUuid &id, const QUuid &startNodeId, const QUuid &endNodeId);
    
    /**
     * @brief Assign material to a bar.
     * @param barId UUID of the bar
     * @param materialId UUID of the material to assign
     * @return true if successful, false if bar not found
     */
    bool assignMaterial(const QUuid &barId, const QUuid &materialId);
    
    /**
     * @brief Assign section to a bar.
     * @param barId UUID of the bar
     * @param sectionId UUID of the section to assign
     * @return true if successful, false if bar not found
     */
    bool assignSection(const QUuid &barId, const QUuid &sectionId);
    
    /**
     * @brief Assign both material and section to a bar.
     * @param barId UUID of the bar
     * @param materialId UUID of the material
     * @param sectionId UUID of the section
     * @return true if successful, false if bar not found
     */
    bool assignProperties(const QUuid &barId, const QUuid &materialId, const QUuid &sectionId);
    
    /**
     * @brief Assign properties to multiple bars at once.
     * @param barIds Vector of bar UUIDs
     * @param materialId UUID of the material
     * @param sectionId UUID of the section
     * @return Number of bars successfully updated
     */
    int assignPropertiesToMultipleBars(const std::vector<QUuid> &barIds,
                                       const QUuid &materialId,
                                       const QUuid &sectionId);
    
    /**
     * @brief Set K-point for bar's local coordinate system.
     * @param barId UUID of the bar
     * @param kPoint K-point position
     * @return true if successful, false if bar not found
     */
    bool setKPoint(const QUuid &barId, const Vector3 &kPoint);
    
    /**
     * @brief Clear K-point from a bar.
     * @param barId UUID of the bar
     * @return true if successful, false if bar not found
     */
    bool clearKPoint(const QUuid &barId);
    
    /**
     * @brief Calculate the length of a bar.
     * @param barId UUID of the bar
     * @return Length of the bar, or 0.0 if bar or nodes not found
     */
    double calculateBarLength(const QUuid &barId) const;
    
    /**
     * @brief Find a bar by its UUID.
     * @param id UUID to search for
     * @return Optional containing the bar if found
     */
    std::optional<Bar> findBar(const QUuid &id) const;
    
    /**
     * @brief Find a bar by its external ID.
     * @param externalId External ID to search for
     * @return Optional containing the bar if found
     */
    std::optional<Bar> findBarByExternalId(int externalId) const;
    
    /**
     * @brief Get all bars.
     * @return Vector of all bars
     */
    std::vector<Bar> allBars() const;
    
    /**
     * @brief Get the count of bars.
     * @return Number of bars
     */
    size_t barCount() const;
    
    /**
     * @brief Find all bars connected to a specific node.
     * @param nodeId UUID of the node
     * @return Vector of bars connected to the node
     */
    std::vector<Bar> findBarsConnectedToNode(const QUuid &nodeId) const;
    
    /**
     * @brief Generate the next available external ID for bars.
     * @return Next external ID
     */
    int nextExternalId() const;
    
    /**
     * @brief Check if a bar exists with the given UUID.
     * @param id UUID to check
     * @return true if bar exists
     */
    bool barExists(const QUuid &id) const;
    
    /**
     * @brief Validate that a bar's connected nodes exist.
     * @param barId UUID of the bar to validate
     * @return true if both connected nodes exist
     */
    bool validateBarConnectivity(const QUuid &barId) const;

signals:
    /**
     * @brief Emitted when a bar is created.
     * @param barId UUID of the created bar
     */
    void barCreated(const QUuid &barId);
    
    /**
     * @brief Emitted when a bar is deleted.
     * @param barId UUID of the deleted bar
     */
    void barDeleted(const QUuid &barId);
    
    /**
     * @brief Emitted when a bar is updated.
     * @param barId UUID of the updated bar
     */
    void barUpdated(const QUuid &barId);
    
    /**
     * @brief Emitted when bar properties (material/section) are assigned.
     * @param barId UUID of the bar
     */
    void barPropertiesAssigned(const QUuid &barId);

private:
    IModelRepository *m_repository;
};

} // namespace Structura::App
