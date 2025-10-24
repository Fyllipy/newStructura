#pragma once

#include "Vector3.h"
#include <QUuid>
#include <QString>
#include <array>
#include <optional>

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

/**
 * @brief Represents material properties for structural analysis.
 * 
 * Invariants:
 * - youngModulus should be positive (> 0)
 * - shearModulus should be positive (> 0)
 * - name should not be empty (recommended)
 */
class Material
{
public:
    /// Default constructor
    Material()
        : m_id(QUuid::createUuid())
    {
    }

    /// Full constructor
    Material(const QUuid &id,
             int externalId,
             QString name,
             double youngModulus,
             double shearModulus)
        : m_id(id)
        , m_externalId(externalId)
        , m_name(std::move(name))
        , m_youngModulus(youngModulus)
        , m_shearModulus(shearModulus)
    {
    }

    // Identification
    const QUuid& id() const noexcept { return m_id; }
    
    int externalId() const noexcept { return m_externalId; }
    void setExternalId(int externalId) noexcept { m_externalId = externalId; }

    // Properties
    const QString& name() const noexcept { return m_name; }
    void setName(const QString &name) { m_name = name; }

    double youngModulus() const noexcept { return m_youngModulus; }
    void setYoungModulus(double value) noexcept { m_youngModulus = value; }

    double shearModulus() const noexcept { return m_shearModulus; }
    void setShearModulus(double value) noexcept { m_shearModulus = value; }

    /// Validate that material properties are physically reasonable
    bool isValid() const noexcept
    {
        return m_youngModulus > 0.0 && m_shearModulus > 0.0 && !m_name.isEmpty();
    }

private:
    QUuid m_id;
    int m_externalId{0};
    QString m_name;
    double m_youngModulus{0.0};
    double m_shearModulus{0.0};
};

/**
 * @brief Represents cross-section properties for structural elements.
 * 
 * Invariants:
 * - area should be positive (> 0)
 * - iz, iy, torsionalConstant should be positive (> 0)
 * - name should not be empty (recommended)
 */
class Section
{
public:
    /// Default constructor
    Section()
        : m_id(QUuid::createUuid())
    {
    }

    /// Full constructor
    Section(const QUuid &id,
            int externalId,
            QString name,
            double area,
            double iz,
            double iy,
            double torsionalConstant)
        : m_id(id)
        , m_externalId(externalId)
        , m_name(std::move(name))
        , m_area(area)
        , m_iz(iz)
        , m_iy(iy)
        , m_torsionalConstant(torsionalConstant)
    {
    }

    // Identification
    const QUuid& id() const noexcept { return m_id; }
    
    int externalId() const noexcept { return m_externalId; }
    void setExternalId(int externalId) noexcept { m_externalId = externalId; }

    // Properties
    const QString& name() const noexcept { return m_name; }
    void setName(const QString &name) { m_name = name; }

    double area() const noexcept { return m_area; }
    void setArea(double value) noexcept { m_area = value; }

    double iz() const noexcept { return m_iz; }
    void setIz(double value) noexcept { m_iz = value; }

    double iy() const noexcept { return m_iy; }
    void setIy(double value) noexcept { m_iy = value; }

    double torsionalConstant() const noexcept { return m_torsionalConstant; }
    void setTorsionalConstant(double value) noexcept { m_torsionalConstant = value; }

    /// Validate that section properties are physically reasonable
    bool isValid() const noexcept
    {
        return m_area > 0.0 && m_iz > 0.0 && m_iy > 0.0 && 
               m_torsionalConstant > 0.0 && !m_name.isEmpty();
    }

private:
    QUuid m_id;
    int m_externalId{0};
    QString m_name;
    double m_area{0.0};
    double m_iz{0.0};
    double m_iy{0.0};
    double m_torsionalConstant{0.0};
};

/**
 * @brief Represents a grid line in the 3D modeling space.
 * 
 * Grid lines help users position nodes and elements in a structured manner.
 * Each line is parallel to one of the principal axes (X, Y, or Z).
 */
class GridLine
{
public:
    enum class Axis {
        X,
        Y,
        Z
    };

    /// Default constructor
    GridLine()
        : m_id(QUuid::createUuid())
    {
    }

    /// Full constructor
    GridLine(const QUuid &id,
             Axis axis,
             double offset,
             int index,
             double coordinate1 = 0.0,
             double coordinate2 = 0.0)
        : m_id(id)
        , m_axis(axis)
        , m_offset(offset)
        , m_index(index)
        , m_coord1(coordinate1)
        , m_coord2(coordinate2)
    {
    }

    // Identification
    const QUuid& id() const noexcept { return m_id; }
    
    // Grid properties
    Axis axis() const noexcept { return m_axis; }
    void setAxis(Axis axis) noexcept { m_axis = axis; }

    double offset() const noexcept { return m_offset; }
    void setOffset(double value) noexcept { m_offset = value; }

    int index() const noexcept { return m_index; }
    void setIndex(int index) noexcept { m_index = index; }

    // Visual state (UI concern, kept for compatibility)
    bool isHighlighted() const noexcept { return m_highlighted; }
    void setHighlighted(bool highlighted) noexcept { m_highlighted = highlighted; }

    bool isGhost() const noexcept { return m_ghost; }
    void setGhost(bool ghost) noexcept { m_ghost = ghost; }

    // Coordinate range
    double coordinate1() const noexcept { return m_coord1; }
    void setCoordinate1(double value) noexcept { m_coord1 = value; }

    double coordinate2() const noexcept { return m_coord2; }
    void setCoordinate2(double value) noexcept { m_coord2 = value; }

    // Endpoint positions
    const Vector3& startPoint() const noexcept { return m_startPoint; }
    std::array<double, 3> startPointArray() const noexcept
    {
        return {{m_startPoint.x(), m_startPoint.y(), m_startPoint.z()}};
    }
    void setStartPoint(double x, double y, double z) noexcept
    {
        m_startPoint.set(x, y, z);
    }
    void setStartPoint(const Vector3 &point) noexcept
    {
        m_startPoint = point;
    }

    const Vector3& endPoint() const noexcept { return m_endPoint; }
    std::array<double, 3> endPointArray() const noexcept
    {
        return {{m_endPoint.x(), m_endPoint.y(), m_endPoint.z()}};
    }
    void setEndPoint(double x, double y, double z) noexcept
    {
        m_endPoint.set(x, y, z);
    }
    void setEndPoint(const Vector3 &point) noexcept
    {
        m_endPoint = point;
    }

    void setEndpoints(double x0, double y0, double z0, double x1, double y1, double z1) noexcept
    {
        m_startPoint.set(x0, y0, z0);
        m_endPoint.set(x1, y1, z1);
    }

private:
    QUuid m_id;
    Axis m_axis{Axis::X};
    double m_offset{0.0};
    int m_index{0};
    bool m_highlighted{false};
    bool m_ghost{false};
    double m_coord1{0.0};
    double m_coord2{0.0};
    Vector3 m_startPoint{0.0, 0.0, 0.0};
    Vector3 m_endPoint{0.0, 0.0, 0.0};
};

} // namespace Structura::Model
