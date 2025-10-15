#pragma once

#include <QUuid>
#include <QString>

#include <array>
#include <utility>

namespace Structura::Model {

class Node
{
public:
    Node()
        : m_id(QUuid::createUuid())
    {
    }

    Node(const QUuid &id, int externalId, double x, double y, double z)
        : m_id(id)
        , m_externalId(externalId)
        , m_position({ { x, y, z } })
    {
    }

    const QUuid &id() const noexcept { return m_id; }

    int externalId() const noexcept { return m_externalId; }
    void setExternalId(int externalId) noexcept { m_externalId = externalId; }

    double x() const noexcept { return m_position[0]; }
    double y() const noexcept { return m_position[1]; }
    double z() const noexcept { return m_position[2]; }
    std::array<double, 3> position() const noexcept { return m_position; }
    void setPosition(double x, double y, double z) noexcept { m_position = { { x, y, z } }; }

    bool isSelected() const noexcept { return m_selected; }
    void setSelected(bool selected) noexcept { m_selected = selected; }

    std::array<bool, 6> restraints() const noexcept { return m_restraints; }
    void setRestraint(int index, bool fixed) noexcept
    {
        if (index >= 0 && index < 6) {
            m_restraints[static_cast<std::size_t>(index)] = fixed;
        }
    }
    void clearRestraints() noexcept { m_restraints.fill(false); }

private:
    QUuid m_id;
    int m_externalId {0};
    std::array<double, 3> m_position { {0.0, 0.0, 0.0} };
    std::array<bool, 6> m_restraints { {false, false, false, false, false, false} };
    bool m_selected {false};
};

class Bar
{
public:
    Bar()
        : m_id(QUuid::createUuid())
    {
    }

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

    const QUuid &id() const noexcept { return m_id; }
    int externalId() const noexcept { return m_externalId; }
    void setExternalId(int externalId) noexcept { m_externalId = externalId; }

    const QUuid &startNodeId() const noexcept { return m_startNodeId; }
    void setStartNodeId(const QUuid &id) noexcept { m_startNodeId = id; }

    const QUuid &endNodeId() const noexcept { return m_endNodeId; }
    void setEndNodeId(const QUuid &id) noexcept { m_endNodeId = id; }

    const QUuid &materialId() const noexcept { return m_materialId; }
    void setMaterialId(const QUuid &id) noexcept { m_materialId = id; }

    const QUuid &sectionId() const noexcept { return m_sectionId; }
    void setSectionId(const QUuid &id) noexcept { m_sectionId = id; }

    bool isSelected() const noexcept { return m_selected; }
    void setSelected(bool selected) noexcept { m_selected = selected; }

private:
    QUuid m_id;
    int m_externalId {0};
    QUuid m_startNodeId;
    QUuid m_endNodeId;
    QUuid m_materialId;
    QUuid m_sectionId;
    bool m_selected {false};
};

class Material
{
public:
    Material()
        : m_id(QUuid::createUuid())
    {
    }

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

    const QUuid &id() const noexcept { return m_id; }
    int externalId() const noexcept { return m_externalId; }
    void setExternalId(int externalId) noexcept { m_externalId = externalId; }

    const QString &name() const noexcept { return m_name; }
    void setName(const QString &name) { m_name = name; }

    double youngModulus() const noexcept { return m_youngModulus; }
    void setYoungModulus(double value) noexcept { m_youngModulus = value; }

    double shearModulus() const noexcept { return m_shearModulus; }
    void setShearModulus(double value) noexcept { m_shearModulus = value; }

private:
    QUuid m_id;
    int m_externalId {0};
    QString m_name;
    double m_youngModulus {0.0};
    double m_shearModulus {0.0};
};

class Section
{
public:
    Section()
        : m_id(QUuid::createUuid())
    {
    }

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

    const QUuid &id() const noexcept { return m_id; }
    int externalId() const noexcept { return m_externalId; }
    void setExternalId(int externalId) noexcept { m_externalId = externalId; }

    const QString &name() const noexcept { return m_name; }
    void setName(const QString &name) { m_name = name; }

    double area() const noexcept { return m_area; }
    void setArea(double value) noexcept { m_area = value; }

    double iz() const noexcept { return m_iz; }
    void setIz(double value) noexcept { m_iz = value; }

    double iy() const noexcept { return m_iy; }
    void setIy(double value) noexcept { m_iy = value; }

    double torsionalConstant() const noexcept { return m_torsionalConstant; }
    void setTorsionalConstant(double value) noexcept { m_torsionalConstant = value; }

private:
    QUuid m_id;
    int m_externalId {0};
    QString m_name;
    double m_area {0.0};
    double m_iz {0.0};
    double m_iy {0.0};
    double m_torsionalConstant {0.0};
};

class GridLine
{
public:
    enum class Axis {
        X,
        Y,
        Z
    };

    GridLine()
        : m_id(QUuid::createUuid())
    {
    }

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

    const QUuid &id() const noexcept { return m_id; }
    Axis axis() const noexcept { return m_axis; }
    void setAxis(Axis axis) noexcept { m_axis = axis; }

    double offset() const noexcept { return m_offset; }
    void setOffset(double value) noexcept { m_offset = value; }

    int index() const noexcept { return m_index; }
    void setIndex(int index) noexcept { m_index = index; }

    bool isHighlighted() const noexcept { return m_highlighted; }
    void setHighlighted(bool highlighted) noexcept { m_highlighted = highlighted; }

    bool isGhost() const noexcept { return m_ghost; }
    void setGhost(bool ghost) noexcept { m_ghost = ghost; }

    double coordinate1() const noexcept { return m_coord1; }
    void setCoordinate1(double value) noexcept { m_coord1 = value; }

    double coordinate2() const noexcept { return m_coord2; }
    void setCoordinate2(double value) noexcept { m_coord2 = value; }

    const std::array<double, 3> &startPoint() const noexcept { return m_startPoint; }
    void setStartPoint(double x, double y, double z) noexcept { m_startPoint = { {x, y, z} }; }

    const std::array<double, 3> &endPoint() const noexcept { return m_endPoint; }
    void setEndPoint(double x, double y, double z) noexcept { m_endPoint = { {x, y, z} }; }

    void setEndpoints(double x0, double y0, double z0, double x1, double y1, double z1) noexcept
    {
        m_startPoint = { {x0, y0, z0} };
        m_endPoint = { {x1, y1, z1} };
    }

private:
    QUuid m_id;
    Axis m_axis {Axis::X};
    double m_offset {0.0};
    int m_index {0};
    bool m_highlighted {false};
    bool m_ghost {false};
    double m_coord1 {0.0};
    double m_coord2 {0.0};
    std::array<double, 3> m_startPoint { {0.0, 0.0, 0.0} };
    std::array<double, 3> m_endPoint { {0.0, 0.0, 0.0} };
};

} // namespace Structura::Model
