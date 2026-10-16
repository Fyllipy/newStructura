#pragma once

#include <array>
#include <cmath>

#include <QString>

namespace Structura::Construction {

using Vec3 = std::array<double, 3>;

/// Plano de construcao ativo: define em que plano global o cursor lanca elementos.
enum class PlaneMode {
    XY,
    XZ,
    YZ
};

/// Indices globais (0=X, 1=Y, 2=Z) do eixo normal e dos dois eixos no plano.
struct PlaneAxes {
    int normal {2};
    int u {0};
    int v {1};
};

inline PlaneAxes planeAxes(PlaneMode mode) noexcept
{
    switch (mode) {
    case PlaneMode::XZ:
        return {1, 0, 2};
    case PlaneMode::YZ:
        return {0, 1, 2};
    case PlaneMode::XY:
    default:
        return {2, 0, 1};
    }
}

inline QString planeName(PlaneMode mode)
{
    switch (mode) {
    case PlaneMode::XZ:
        return QStringLiteral("XZ");
    case PlaneMode::YZ:
        return QStringLiteral("YZ");
    case PlaneMode::XY:
    default:
        return QStringLiteral("XY");
    }
}

/// Retangulo no plano (coordenadas u/v do plano ativo).
struct Window {
    double minU {0.0};
    double maxU {0.0};
    double minV {0.0};
    double maxV {0.0};

    bool operator==(const Window &o) const
    {
        return minU == o.minU && maxU == o.maxU && minV == o.minV && maxV == o.maxV;
    }
};

inline Vec3 operator+(const Vec3 &a, const Vec3 &b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
inline Vec3 operator-(const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
inline Vec3 operator*(const Vec3 &a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
inline double dot(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline double norm(const Vec3 &a) { return std::sqrt(dot(a, a)); }

} // namespace Structura::Construction
