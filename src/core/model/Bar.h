#pragma once

#include "Vector3.h"
#include <QUuid>
#include <optional>
#include <array>

namespace Structura::Model {

/**
 * @brief Represents a structural bar (beam/column) element.
 * 
 * A Bar connects two nodes and has material and section properties.
 * It can optionally have a K-point for defining local coordinate system.
 * 
 * Invariants:
 * - id is always valid and unique
 * - startNodeId and endNodeId must reference existing nodes
 * - startNodeId != endNodeId (a bar cannot connect to itself)
 * - materialId and sectionId should reference existing entities (optional validation)
 */
class Bar
{
public:
    /// Default constructor
    Bar()
        : m_id(QUuid::createUuid())
    {
    }

    /// Full constructor
    Bar(const QUuid &id,
        const QUuid &startNodeId,
        const QUuid &endNodeId,
        const QUuid &materialId = QUuid(),
        const QUuid &sectionId = QUuid())
        : m_id(id)
        , m_startNodeId(startNodeId)
        , m_endNodeId(endNodeId)
        , m_materialId(materialId)
        , m_sectionId(sectionId)
    {
    }

    // Identification
    const QUuid& id() const noexcept { return m_id; }
    
    int externalId() const noexcept { return m_externalId; }
    void setExternalId(int externalId) noexcept { m_externalId = externalId; }

    // Connectivity
    const QUuid& startNodeId() const noexcept { return m_startNodeId; }
    void setStartNodeId(const QUuid &id) noexcept { m_startNodeId = id; }

    const QUuid& endNodeId() const noexcept { return m_endNodeId; }
    void setEndNodeId(const QUuid &id) noexcept { m_endNodeId = id; }

    // Properties
    const QUuid& materialId() const noexcept { return m_materialId; }
    void setMaterialId(const QUuid &id) noexcept { m_materialId = id; }

    const QUuid& sectionId() const noexcept { return m_sectionId; }
    void setSectionId(const QUuid &id) noexcept { m_sectionId = id; }

    // Selection state
    bool isSelected() const noexcept { return m_selected; }
    void setSelected(bool selected) noexcept { m_selected = selected; }

    // Local Coordinate System (LCS) support
    /// Get the K-point used for defining local coordinate system
    const std::optional<Vector3>& kPoint() const noexcept { return m_kPoint; }
    
    /// Set K-point and mark LCS as dirty
    void setKPoint(const Vector3 &point) noexcept
    {
        m_kPoint = point;
        m_lcsDirty = true;
    }
    
    /// Set K-point from array and mark LCS as dirty
    void setKPoint(const std::array<double, 3> &point) noexcept
    {
        m_kPoint = Vector3(point[0], point[1], point[2]);
        m_lcsDirty = true;
    }
    
    /// Clear K-point and mark LCS as dirty
    void clearKPoint() noexcept
    {
        m_kPoint.reset();
        m_lcsDirty = true;
    }
    
    bool hasKPoint() const noexcept { return m_kPoint.has_value(); }
    
    /// Check if local coordinate system needs recomputation
    bool isLCSDirty() const noexcept { return m_lcsDirty; }
    void setLCSDirty(bool dirty) noexcept { m_lcsDirty = dirty; }

    /**
     * @brief Calculate the length of the bar given node positions.
     * @param startPos Position of start node
     * @param endPos Position of end node
     * @return Length of the bar
     */
    static double calculateLength(const Vector3 &startPos, const Vector3 &endPos) noexcept
    {
        return startPos.distanceTo(endPos);
    }

private:
    QUuid m_id;
    int m_externalId{0};
    QUuid m_startNodeId;
    QUuid m_endNodeId;
    QUuid m_materialId;
    QUuid m_sectionId;
    bool m_selected{false};
    
    // Local Coordinate System attributes
    std::optional<Vector3> m_kPoint;
    bool m_lcsDirty{true};
};

} // namespace Structura::Model
