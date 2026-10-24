#include "core/math/Vec3Ops.h"
#include "shell/ShellFactory.h"

#include <algorithm>
#include <cmath>

namespace Structura::Shell {

namespace {


using Structura::Math::sub;
double length(const Vec3 &a) { return std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); }

Vec3 polygonNormal(const std::vector<Vec3> &poly)
{
    Vec3 n {0.0, 0.0, 0.0};
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const Vec3 &a = poly[i], &b = poly[(i + 1) % poly.size()];
        n[0] += (a[1] - b[1]) * (a[2] + b[2]);
        n[1] += (a[2] - b[2]) * (a[0] + b[0]);
        n[2] += (a[0] - b[0]) * (a[1] + b[1]);
    }
    return n;
}

std::vector<Vec3> orientedTowards(std::vector<Vec3> poly, int axis)
{
    if (polygonNormal(poly)[static_cast<std::size_t>(axis)] < 0.0) {
        std::reverse(poly.begin(), poly.end());
    }
    return poly;
}

} // namespace

double suggestedMeshSize(const std::vector<Vec3> &points, double extra)
{
    Vec3 lo {1e300, 1e300, 1e300}, hi {-1e300, -1e300, -1e300};
    for (const Vec3 &p : points) {
        for (std::size_t k = 0; k < 3; ++k) {
            lo[k] = std::min(lo[k], p[k]);
            hi[k] = std::max(hi[k], p[k]);
        }
    }
    const double diagonal = points.empty() ? extra : std::max(length(sub(hi, lo)), extra);
    const double raw = std::clamp(diagonal / 16.0, 0.02, 2.0);
    const double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
    const double mantissa = raw / magnitude;
    const double nice = mantissa < 1.5 ? 1.0 : (mantissa < 3.5 ? 2.0 : (mantissa < 7.5 ? 5.0 : 10.0));
    return nice * magnitude;
}

bool translateObject(Model &model, int objectId, const Vec3 &delta, int copyId)
{
    const auto it = std::find_if(model.objects.begin(), model.objects.end(), [objectId](const Object &o) { return o.id == objectId; });
    if (it == model.objects.end()) {
        return false;
    }
    auto shiftObject = [&delta](Object &o) {
        for (Vec3 &p : o.polygon) {
            for (std::size_t k = 0; k < 3; ++k) {
                p[k] += delta[k];
            }
        }
        static const char *const origin[3] = {"x0", "y0", "z0"};
        static const char *const axisPoint[3] = {"ax", "ay", "az"};
        const bool parametric = o.kind == Kind::RectTank || o.kind == Kind::CylTank || o.kind == Kind::Vessel;
        for (std::size_t k = 0; k < 3; ++k) {
            if (parametric) {
                o.p[origin[k]] += delta[k];   // origem ausente = 0
            }
            for (const char *key : {origin[k], axisPoint[k]}) {
                if (parametric && key == origin[k]) {
                    continue;
                }
                const auto found = o.p.find(key);
                if (found != o.p.end()) {
                    found->second += delta[k];
                }
            }
        }
    };
    auto shiftSupport = [&delta](Support &s) {
        if (s.selector == Support::Plane && s.axis >= 0 && s.axis <= 2) {
            s.value += delta[static_cast<std::size_t>(s.axis)];
        } else if (s.selector == Support::Point) {
            s.value += delta[0];
            s.value2 += delta[1];
            s.value3 += delta[2];
        }
        if (s.limitAxis >= 0 && s.limitAxis <= 2) {
            s.limitValue += delta[static_cast<std::size_t>(s.limitAxis)];
        }
    };
    auto shiftLoad = [&delta](Load &l) {
        if (l.type == Load::Hydrostatic) {
            l.v[1] += delta[2];
        }
    };
    if (copyId <= 0) {
        shiftObject(*it);
        for (Support &s : model.supports) {
            if (s.object == objectId) {
                shiftSupport(s);
            }
        }
        for (Load &l : model.loads) {
            if (l.object == objectId) {
                shiftLoad(l);
            }
        }
        return true;
    }
    Object copy = *it;
    copy.id = copyId;
    copy.name += " (copia)";
    shiftObject(copy);
    const std::vector<Support> supports = model.supports;
    for (Support s : supports) {
        if (s.object == objectId) {
            s.object = copyId;
            shiftSupport(s);
            model.supports.push_back(s);
        }
    }
    const std::vector<Load> loads = model.loads;
    for (Load l : loads) {
        if (l.object == objectId) {
            l.object = copyId;
            shiftLoad(l);
            model.loads.push_back(l);
        }
    }
    model.objects.push_back(std::move(copy));
    return true;
}

std::optional<Object> makePlate(const std::vector<Vec3> &points, const PlaneSpec &plane)
{
    if (points.size() < 3 || length(polygonNormal(points)) < 1e-9) {
        return std::nullopt;
    }
    Object o;
    o.kind = Kind::Plate;
    o.polygon = orientedTowards(points, plane.normal);
    o.meshSize = suggestedMeshSize(points);
    return o;
}

std::optional<Object> makeRectangle(const Vec3 &a, const Vec3 &b, const PlaneSpec &plane)
{
    const std::size_t u = static_cast<std::size_t>(plane.u), v = static_cast<std::size_t>(plane.v);
    if (std::abs(a[u] - b[u]) < 1e-6 || std::abs(a[v] - b[v]) < 1e-6) {
        return std::nullopt;
    }
    Vec3 c1 = a, c2 = a, c3 = a;
    c1[u] = b[u];
    c2[u] = b[u];
    c2[v] = b[v];
    c3[v] = b[v];
    return makePlate({a, c1, c2, c3}, plane);
}

std::optional<Object> makeDisk(const Vec3 &center, const Vec3 &edge, const PlaneSpec &plane)
{
    const std::size_t u = static_cast<std::size_t>(plane.u), v = static_cast<std::size_t>(plane.v);
    const double radius = std::hypot(edge[u] - center[u], edge[v] - center[v]);
    if (radius < 1e-6) {
        return std::nullopt;
    }
    Vec3 normal {0.0, 0.0, 0.0};
    normal[static_cast<std::size_t>(plane.normal)] = 1.0;
    Object o;
    o.kind = Kind::Disk;
    o.polygon = {center};
    o.p = {{"r", radius}, {"rIn", 0.0}, {"nx", normal[0]}, {"ny", normal[1]}, {"nz", normal[2]}};
    o.meshSize = suggestedMeshSize({center}, 2.0 * radius);
    return o;
}

std::optional<Object> makeExtrusion(const std::vector<Vec3> &path, bool closed, const Vec3 &v)
{
    if (path.size() < 2 || length(v) < 1e-9) {
        return std::nullopt;
    }
    Object o;
    o.kind = Kind::Extrusion;
    o.polygon = path;
    o.p = {{"closed", closed && path.size() >= 3 ? 1.0 : 0.0}, {"vx", v[0]}, {"vy", v[1]}, {"vz", v[2]}};
    std::vector<Vec3> all = path;
    all.push_back({path.front()[0] + v[0], path.front()[1] + v[1], path.front()[2] + v[2]});
    o.meshSize = suggestedMeshSize(all);
    return o;
}

std::optional<Object> makeCylinder(const Vec3 &center, double radius, const Vec3 &v, double endScale)
{
    if (radius < 1e-6 || length(v) < 1e-9) {
        return std::nullopt;
    }
    Object o;
    o.kind = Kind::Extrusion;
    o.polygon = {center};
    o.p = {{"circle", 1.0}, {"r", radius}, {"vx", v[0]}, {"vy", v[1]}, {"vz", v[2]}, {"endScale", endScale}};
    o.meshSize = suggestedMeshSize({center}, std::max(2.0 * radius, length(v)));
    return o;
}

std::optional<Object> makeRevolution(const std::vector<Vec3> &profile, const Vec3 &axisPoint, const Vec3 &axisDirection)
{
    if (profile.size() < 2 || length(axisDirection) < 1e-9) {
        return std::nullopt;
    }
    Object o;
    o.kind = Kind::Revolution;
    o.polygon = profile;
    o.p = {{"ax", axisPoint[0]}, {"ay", axisPoint[1]}, {"az", axisPoint[2]}, {"dx", axisDirection[0]}, {"dy", axisDirection[1]}, {"dz", axisDirection[2]}, {"angle", 360.0}};
    std::vector<Vec3> all = profile;
    all.push_back(axisPoint);
    o.meshSize = suggestedMeshSize(all);
    return o;
}

} // namespace Structura::Shell
