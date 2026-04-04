#pragma once

#include "Vector3.h"
#include <QUuid>
#include <array>

namespace Structura::Model {

/**
 * @brief Represents a structural node (joint) in the model.
 * 
 * A Node is a point in 3D space where structural elements connect.
 * It can have restraints (boundary conditions) applied to it.
 * 
 * Invariants:
 * - id is always valid and unique (set at construction)
 * - externalId is used for display and export purposes
 * - position can be any valid point in 3D space
 * - restraints array has exactly 6 elements (UX, UY, UZ, RX, RY, RZ)
 */
class Node
{
public:
    /// Default constructor - creates node at origin with new UUID
    Node()
        : m_id(QUuid::createUuid())
        , m_externalId(0)
        , m_position(0.0, 0.0, 0.0)
    {
    }

    /// Full constructor
    Node(const QUuid &id, int externalId, double x, double y, double z)
        : m_id(id)
        , m_externalId(externalId)
        , m_position(x, y, z)
    {
    }

    /// Constructor with Vector3 position
    Node(const QUuid &id, int externalId, const Vector3 &position)
        : m_id(id)
        , m_externalId(externalId)
        , m_position(position)
    {
    }

    // Identification
    const QUuid& id() const noexcept { return m_id; }
    
    int externalId() const noexcept { return m_externalId; }
    void setExternalId(int externalId) noexcept { m_externalId = externalId; }

    // Position accessors
    double x() const noexcept { return m_position.x(); }
    double y() const noexcept { return m_position.y(); }
    double z() const noexcept { return m_position.z(); }
    
    const Vector3& position() const noexcept { return m_position; }
    
    /// For backward compatibility: return position as array
    std::array<double, 3> positionArray() const noexcept
    {
        return {{m_position.x(), m_position.y(), m_position.z()}};
    }
    
    void setPosition(double x, double y, double z) noexcept
    {
        m_position.set(x, y, z);
    }
    
    void setPosition(const Vector3 &pos) noexcept
    {
        m_position = pos;
    }

    /// Move node to new position
    void moveTo(const Vector3 &newPosition) noexcept
    {
        m_position = newPosition;
    }

    /// Calculate distance to another node
    double distanceTo(const Node &other) const noexcept
    {
        return m_position.distanceTo(other.m_position);
    }

    // Selection state (UI concern, but kept for compatibility)
    bool isSelected() const noexcept { return m_selected; }
    void setSelected(bool selected) noexcept { m_selected = selected; }

    // Restraints (boundary conditions)
    /// Get all restraints as array [UX, UY, UZ, RX, RY, RZ]
    std::array<bool, 6> restraints() const noexcept { return m_restraints; }
    
    /// Set a specific restraint (index 0-5 for UX, UY, UZ, RX, RY, RZ)
    void setRestraint(int index, bool fixed) noexcept
    {
        if (index >= 0 && index < 6) {
            m_restraints[static_cast<std::size_t>(index)] = fixed;
        }
    }
    
    /// Clear all restraints (free node)
    void clearRestraints() noexcept
    {
        m_restraints.fill(false);
    }
    
    /// Check if node has any restraints
    bool hasRestraints() const noexcept
    {
        for (bool r : m_restraints) {
            if (r) return true;
        }
        return false;
    }

private:
    QUuid m_id;
    int m_externalId;
    Vector3 m_position;
    std::array<bool, 6> m_restraints{{false, false, false, false, false, false}};
    bool m_selected{false};
};

} // namespace Structura::Model
