#pragma once

#include "Vector3.h"
#include <QUuid>
#include <array>

namespace Structura::Model {

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
