#include "DimensionService.h"

#include <algorithm>
#include <cmath>

namespace Structura::App::Services {

namespace {

using Vec3 = DimensionService::Vec3;

double norm(const Vec3 &v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
Vec3 add(const Vec3 &a, const Vec3 &b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 scale(const Vec3 &a, double k) { return {a[0] * k, a[1] * k, a[2] * k}; }

} // namespace

DimensionService::Vec3 DimensionService::defaultOffset(const Vec3 &a, const Vec3 &b)
{
    const Vec3 d = sub(b, a);
    const double length = norm(d);
    if (length < 1e-9) {
        return {0.0, 0.5, 0.0};
    }
    const double magnitude = std::clamp(0.1 * length, 0.3, 2.0);
    Vec3 direction;
    if (std::abs(d[2]) >= 0.9 * length) {
        direction = {1.0, 0.0, 0.0};   // segmento vertical: a cota fica ao lado, na direcao X
    } else {
        const double horizontal = std::hypot(d[0], d[1]);
        direction = {-d[1] / horizontal, d[0] / horizontal, 0.0};   // perpendicular em planta
    }
    return scale(direction, magnitude);
}

DimensionService::Geometry DimensionService::build(const Structura::App::ProjectData::DimensionLine &dimension)
{
    Geometry geometry;
    const Vec3 a = dimension.a, b = dimension.b, offset = dimension.offset;
    const Vec3 d = sub(b, a);
    geometry.length = norm(d);
    const double offsetLength = norm(offset);
    if (geometry.length < 1e-9) {
        return geometry;
    }
    const Vec3 along = scale(d, 1.0 / geometry.length);
    const Vec3 across = offsetLength > 1e-9 ? scale(offset, 1.0 / offsetLength) : Vec3 {0.0, 0.0, 0.0};
    const Vec3 a2 = add(a, offset), b2 = add(b, offset);
    // linhas de chamada: do ponto (com uma pequena folga) ate um pouco alem da linha de cota
    const double gap = 0.15 * offsetLength, beyond = 0.12 * offsetLength;
    geometry.lines.push_back({add(a, scale(across, gap)), add(a2, scale(across, beyond))});
    geometry.lines.push_back({add(b, scale(across, gap)), add(b2, scale(across, beyond))});
    geometry.lines.push_back({a2, b2});
    // marcas inclinadas (45 graus) nas extremidades da linha de cota
    const double tick = std::max(0.12, 0.25 * offsetLength);
    Vec3 slash = add(along, across);
    const double slashLength = norm(slash);
    if (slashLength > 1e-9) {
        slash = scale(slash, tick / slashLength);
        geometry.lines.push_back({sub(a2, slash), add(a2, slash)});
        geometry.lines.push_back({sub(b2, slash), add(b2, slash)});
    }
    geometry.labelPosition = scale(add(a2, b2), 0.5);
    return geometry;
}

} // namespace Structura::App::Services
