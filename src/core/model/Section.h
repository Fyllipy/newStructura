#pragma once

#include "ModelValidationUtils.h"

#include <QUuid>
#include <QString>

namespace Structura::Model {

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
        return isPositive(m_area)
            && isPositive(m_iz)
            && isPositive(m_iy)
            && isPositive(m_torsionalConstant)
            && hasName(m_name);
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

} // namespace Structura::Model
