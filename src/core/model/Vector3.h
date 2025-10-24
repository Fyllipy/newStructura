#pragma once

#include <array>
#include <cmath>

namespace Structura::Model {

/**
 * @brief A simple 3D vector class for pure domain logic.
 * 
 * This class represents a point or vector in 3D space without any
 * dependencies on Qt or VTK. It provides basic vector operations
 * commonly needed in structural analysis.
 */
class Vector3
{
public:
    /// Default constructor - initializes to origin (0, 0, 0)
    constexpr Vector3() noexcept
        : m_data{{0.0, 0.0, 0.0}}
    {
    }

    /// Construct from three coordinates
    constexpr Vector3(double x, double y, double z) noexcept
        : m_data{{x, y, z}}
    {
    }

    /// Construct from std::array
    explicit constexpr Vector3(const std::array<double, 3> &data) noexcept
        : m_data(data)
    {
    }

    // Accessors
    constexpr double x() const noexcept { return m_data[0]; }
    constexpr double y() const noexcept { return m_data[1]; }
    constexpr double z() const noexcept { return m_data[2]; }

    // Mutators
    constexpr void setX(double x) noexcept { m_data[0] = x; }
    constexpr void setY(double y) noexcept { m_data[1] = y; }
    constexpr void setZ(double z) noexcept { m_data[2] = z; }
    constexpr void set(double x, double y, double z) noexcept
    {
        m_data[0] = x;
        m_data[1] = y;
        m_data[2] = z;
    }

    /// Access by index (0=x, 1=y, 2=z)
    constexpr double operator[](std::size_t index) const noexcept { return m_data[index]; }
    constexpr double& operator[](std::size_t index) noexcept { return m_data[index]; }

    /// Get underlying array
    constexpr const std::array<double, 3>& data() const noexcept { return m_data; }

    /// Calculate the Euclidean length (magnitude) of the vector
    double length() const noexcept
    {
        return std::sqrt(m_data[0] * m_data[0] + 
                        m_data[1] * m_data[1] + 
                        m_data[2] * m_data[2]);
    }

    /// Calculate the squared length (useful for comparisons without sqrt)
    constexpr double lengthSquared() const noexcept
    {
        return m_data[0] * m_data[0] + 
               m_data[1] * m_data[1] + 
               m_data[2] * m_data[2];
    }

    /// Calculate distance to another point
    double distanceTo(const Vector3 &other) const noexcept
    {
        const double dx = m_data[0] - other.m_data[0];
        const double dy = m_data[1] - other.m_data[1];
        const double dz = m_data[2] - other.m_data[2];
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    /// Calculate squared distance to another point
    constexpr double distanceSquaredTo(const Vector3 &other) const noexcept
    {
        const double dx = m_data[0] - other.m_data[0];
        const double dy = m_data[1] - other.m_data[1];
        const double dz = m_data[2] - other.m_data[2];
        return dx * dx + dy * dy + dz * dz;
    }

    /// Normalize the vector (make it unit length)
    /// Returns false if the vector has zero length
    bool normalize() noexcept
    {
        const double len = length();
        if (len < 1e-10) {
            return false;
        }
        m_data[0] /= len;
        m_data[1] /= len;
        m_data[2] /= len;
        return true;
    }

    /// Return a normalized copy of this vector
    Vector3 normalized() const noexcept
    {
        Vector3 result = *this;
        result.normalize();
        return result;
    }

    /// Dot product
    constexpr double dot(const Vector3 &other) const noexcept
    {
        return m_data[0] * other.m_data[0] + 
               m_data[1] * other.m_data[1] + 
               m_data[2] * other.m_data[2];
    }

    /// Cross product
    constexpr Vector3 cross(const Vector3 &other) const noexcept
    {
        return Vector3(
            m_data[1] * other.m_data[2] - m_data[2] * other.m_data[1],
            m_data[2] * other.m_data[0] - m_data[0] * other.m_data[2],
            m_data[0] * other.m_data[1] - m_data[1] * other.m_data[0]
        );
    }

    // Arithmetic operators
    constexpr Vector3 operator+(const Vector3 &other) const noexcept
    {
        return Vector3(m_data[0] + other.m_data[0],
                      m_data[1] + other.m_data[1],
                      m_data[2] + other.m_data[2]);
    }

    constexpr Vector3 operator-(const Vector3 &other) const noexcept
    {
        return Vector3(m_data[0] - other.m_data[0],
                      m_data[1] - other.m_data[1],
                      m_data[2] - other.m_data[2]);
    }

    constexpr Vector3 operator*(double scalar) const noexcept
    {
        return Vector3(m_data[0] * scalar,
                      m_data[1] * scalar,
                      m_data[2] * scalar);
    }

    constexpr Vector3 operator/(double scalar) const noexcept
    {
        return Vector3(m_data[0] / scalar,
                      m_data[1] / scalar,
                      m_data[2] / scalar);
    }

    constexpr Vector3 operator-() const noexcept
    {
        return Vector3(-m_data[0], -m_data[1], -m_data[2]);
    }

    // Compound assignment operators
    constexpr Vector3& operator+=(const Vector3 &other) noexcept
    {
        m_data[0] += other.m_data[0];
        m_data[1] += other.m_data[1];
        m_data[2] += other.m_data[2];
        return *this;
    }

    constexpr Vector3& operator-=(const Vector3 &other) noexcept
    {
        m_data[0] -= other.m_data[0];
        m_data[1] -= other.m_data[1];
        m_data[2] -= other.m_data[2];
        return *this;
    }

    constexpr Vector3& operator*=(double scalar) noexcept
    {
        m_data[0] *= scalar;
        m_data[1] *= scalar;
        m_data[2] *= scalar;
        return *this;
    }

    constexpr Vector3& operator/=(double scalar) noexcept
    {
        m_data[0] /= scalar;
        m_data[1] /= scalar;
        m_data[2] /= scalar;
        return *this;
    }

    // Comparison operators
    constexpr bool operator==(const Vector3 &other) const noexcept
    {
        return m_data[0] == other.m_data[0] &&
               m_data[1] == other.m_data[1] &&
               m_data[2] == other.m_data[2];
    }

    constexpr bool operator!=(const Vector3 &other) const noexcept
    {
        return !(*this == other);
    }

private:
    std::array<double, 3> m_data;
};

/// Scalar multiplication with scalar on the left
constexpr inline Vector3 operator*(double scalar, const Vector3 &vec) noexcept
{
    return vec * scalar;
}

} // namespace Structura::Model
