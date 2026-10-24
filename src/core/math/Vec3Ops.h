#pragma once

#include <array>

/// Operacoes basicas sobre vetores 3D (`std::array<double, 3>`), compartilhadas pelos modulos (antes repetidas, identicas, em varios arquivos).
namespace Structura::Math {

using Vec3 = std::array<double, 3>;

inline double dot(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

inline Vec3 cross(const Vec3 &a, const Vec3 &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }

inline Vec3 sub(const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }

} // namespace Structura::Math
