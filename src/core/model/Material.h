#pragma once

#include <QUuid>
#include <QString>

namespace Structura::Model {

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

} // namespace Structura::Model
