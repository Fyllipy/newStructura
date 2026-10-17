#include "drawing/HiddenLine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Structura::Drawing {

namespace {

constexpr double kPi = 3.14159265358979323846;

Vec3 sub(const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double dot(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3 &a, const Vec3 &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
Vec3 unit(const Vec3 &a)
{
    const double n = std::sqrt(dot(a, a));
    return n > 1e-12 ? Vec3 {a[0] / n, a[1] / n, a[2] / n} : Vec3 {1, 0, 0};
}

struct CameraFrame {
    Vec3 eye, right, up, forward;
    bool perspective {false};
};

CameraFrame frameOf(const Camera3D &c)
{
    CameraFrame f;
    f.eye = c.position;
    f.forward = unit(sub(c.target, c.position));
    Vec3 up = unit(c.up);
    if (std::abs(dot(f.forward, up)) > 0.9999) {
        up = {0, 1, 0};   // olhando na vertical: usa Y como "cima" provisorio
    }
    f.right = unit(cross(f.forward, up));
    f.up = cross(f.right, f.forward);
    f.perspective = c.perspective;
    return f;
}

/// Ponto em coordenadas da camera: x, y (ja projetados) e profundidade z.
struct P3 {
    double x, y, z;
};

constexpr double kNear = 1.0;

/// Coordenadas da camera sem projetar (cx, cy, cz).
Vec3 toCamera(const CameraFrame &f, const Vec3 &p)
{
    const Vec3 v = sub(p, f.eye);
    return {dot(v, f.right), dot(v, f.up), dot(v, f.forward)};
}

P3 projectPoint(const CameraFrame &f, const Vec3 &cam)
{
    if (f.perspective) {
        return {cam[0] / cam[2], cam[1] / cam[2], cam[2]};
    }
    return {cam[0], cam[1], cam[2]};
}

} // namespace

Camera3D cameraFromAngles(double azimuthDeg, double elevationDeg, const std::array<double, 3> &target, double distance)
{
    const double az = azimuthDeg * kPi / 180.0, el = elevationDeg * kPi / 180.0;
    const Vec3 dir {std::sin(az) * std::cos(el), -std::cos(az) * std::cos(el), std::sin(el)};
    Camera3D c;
    c.target = target;
    c.position = {target[0] + dir[0] * distance, target[1] + dir[1] * distance, target[2] + dir[2] * distance};
    c.up = {0, 0, 1};
    c.perspective = false;
    return c;
}

std::vector<int> triangulatePolygon(const std::vector<std::array<double, 2>> &polygon)
{
    const int n = static_cast<int>(polygon.size());
    std::vector<int> result;
    if (n < 3) {
        return result;
    }
    double area = 0.0;
    for (int i = 0; i < n; ++i) {
        const auto &a = polygon[static_cast<std::size_t>(i)];
        const auto &b = polygon[static_cast<std::size_t>((i + 1) % n)];
        area += a[0] * b[1] - b[0] * a[1];
    }
    std::vector<int> idx(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        idx[static_cast<std::size_t>(i)] = area >= 0.0 ? i : n - 1 - i;   // CCW
    }
    auto P = [&](int i) -> const std::array<double, 2> & { return polygon[static_cast<std::size_t>(i)]; };
    auto cross2 = [](const std::array<double, 2> &a, const std::array<double, 2> &b, const std::array<double, 2> &c) {
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    };
    auto inside = [&](const std::array<double, 2> &p, const std::array<double, 2> &a, const std::array<double, 2> &b, const std::array<double, 2> &c) {
        return cross2(a, b, p) >= -1e-12 && cross2(b, c, p) >= -1e-12 && cross2(c, a, p) >= -1e-12;
    };
    int guard = 0;
    while (idx.size() > 3 && guard++ < 10000) {
        bool clipped = false;
        const int m = static_cast<int>(idx.size());
        for (int i = 0; i < m; ++i) {
            const int i0 = idx[static_cast<std::size_t>((i + m - 1) % m)], i1 = idx[static_cast<std::size_t>(i)], i2 = idx[static_cast<std::size_t>((i + 1) % m)];
            if (cross2(P(i0), P(i1), P(i2)) <= 1e-12) {
                continue;   // reflexo ou degenerado
            }
            bool empty = true;
            for (int j = 0; j < m && empty; ++j) {
                const int k = idx[static_cast<std::size_t>(j)];
                if (k == i0 || k == i1 || k == i2) {
                    continue;
                }
                if (inside(P(k), P(i0), P(i1), P(i2))) {
                    empty = false;
                }
            }
            if (!empty) {
                continue;
            }
            result.push_back(i0);
            result.push_back(i1);
            result.push_back(i2);
            idx.erase(idx.begin() + i);
            clipped = true;
            break;
        }
        if (!clipped) {
            break;   // poligono degenerado: sai com o que ja foi recortado
        }
    }
    if (idx.size() == 3) {
        result.push_back(idx[0]);
        result.push_back(idx[1]);
        result.push_back(idx[2]);
    }
    return result;
}

HiddenLineResult computeHiddenLines(const Camera3D &camera, const std::vector<Triangle3D> &faces, const std::vector<Edge3D> &edges, bool keepHidden, int cells)
{
    HiddenLineResult result;
    const CameraFrame frame = frameOf(camera);

    // triangulos projetados (com recorte no plano proximo)
    struct Tri {
        P3 a, b, c;
    };
    std::vector<Tri> tris;
    tris.reserve(faces.size());
    double minX = std::numeric_limits<double>::max(), maxX = -minX, minY = minX, maxY = -minX, minZ = minX, maxZ = -minX;
    auto grow = [&](const P3 &p) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
        minZ = std::min(minZ, p.z);
        maxZ = std::max(maxZ, p.z);
    };
    for (const auto &face : faces) {
        std::vector<Vec3> poly = {toCamera(frame, face.a), toCamera(frame, face.b), toCamera(frame, face.c)};
        if (frame.perspective) {
            std::vector<Vec3> clipped;
            for (std::size_t i = 0; i < poly.size(); ++i) {
                const Vec3 &p = poly[i], &q = poly[(i + 1) % poly.size()];
                const bool pin = p[2] >= kNear, qin = q[2] >= kNear;
                if (pin) {
                    clipped.push_back(p);
                }
                if (pin != qin) {
                    const double t = (kNear - p[2]) / (q[2] - p[2]);
                    clipped.push_back({p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t, kNear});
                }
            }
            poly = clipped;
        }
        if (poly.size() < 3) {
            continue;
        }
        for (std::size_t i = 1; i + 1 < poly.size(); ++i) {
            Tri t {projectPoint(frame, poly[0]), projectPoint(frame, poly[i]), projectPoint(frame, poly[i + 1])};
            tris.push_back(t);
            grow(t.a);
            grow(t.b);
            grow(t.c);
        }
    }
    struct Edge {
        P3 a, b;
        int tag;
        bool valid;
    };
    std::vector<Edge> projectedEdges;
    projectedEdges.reserve(edges.size());
    for (const auto &e : edges) {
        Vec3 a = toCamera(frame, e.a), b = toCamera(frame, e.b);
        bool valid = true;
        if (frame.perspective) {
            if (a[2] < kNear && b[2] < kNear) {
                valid = false;
            } else if (a[2] < kNear) {
                const double t = (kNear - a[2]) / (b[2] - a[2]);
                a = {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, kNear};
            } else if (b[2] < kNear) {
                const double t = (kNear - b[2]) / (a[2] - b[2]);
                b = {b[0] + (a[0] - b[0]) * t, b[1] + (a[1] - b[1]) * t, kNear};
            }
        }
        if (!valid) {
            continue;
        }
        Edge pe {projectPoint(frame, a), projectPoint(frame, b), e.tag, true};
        grow(pe.a);
        grow(pe.b);
        projectedEdges.push_back(pe);
    }
    if (projectedEdges.empty() && tris.empty()) {
        return result;
    }
    result.minX = minX;
    result.maxX = maxX;
    result.minY = minY;
    result.maxY = maxY;
    result.ok = true;

    const double w = std::max(1e-9, maxX - minX), h = std::max(1e-9, maxY - minY);
    const double cell = std::max(w, h) / std::max(100, cells);
    const int nx = static_cast<int>(std::ceil(w / cell)) + 3, ny = static_cast<int>(std::ceil(h / cell)) + 3;
    std::vector<float> depth(static_cast<std::size_t>(nx) * static_cast<std::size_t>(ny), std::numeric_limits<float>::infinity());
    auto cellX = [&](double x) { return static_cast<int>(std::floor((x - minX) / cell)) + 1; };
    auto cellY = [&](double y) { return static_cast<int>(std::floor((y - minY) / cell)) + 1; };
    auto at = [&](int ix, int iy) -> float & { return depth[static_cast<std::size_t>(iy) * static_cast<std::size_t>(nx) + static_cast<std::size_t>(ix)]; };

    // rasterizacao dos triangulos
    for (const Tri &t : tris) {
        const double area = (t.b.x - t.a.x) * (t.c.y - t.a.y) - (t.b.y - t.a.y) * (t.c.x - t.a.x);
        if (std::abs(area) < cell * cell * 1e-3) {
            continue;
        }
        const int x0 = std::max(0, cellX(std::min({t.a.x, t.b.x, t.c.x})) - 1), x1 = std::min(nx - 1, cellX(std::max({t.a.x, t.b.x, t.c.x})) + 1);
        const int y0 = std::max(0, cellY(std::min({t.a.y, t.b.y, t.c.y})) - 1), y1 = std::min(ny - 1, cellY(std::max({t.a.y, t.b.y, t.c.y})) + 1);
        const double ia = frame.perspective ? 1.0 / t.a.z : t.a.z, ib = frame.perspective ? 1.0 / t.b.z : t.b.z, ic = frame.perspective ? 1.0 / t.c.z : t.c.z;
        for (int iy = y0; iy <= y1; ++iy) {
            const double py = minY + (iy - 1 + 0.5) * cell;
            for (int ix = x0; ix <= x1; ++ix) {
                const double px = minX + (ix - 1 + 0.5) * cell;
                const double w0 = ((t.b.x - px) * (t.c.y - py) - (t.b.y - py) * (t.c.x - px)) / area;
                const double w1 = ((t.c.x - px) * (t.a.y - py) - (t.c.y - py) * (t.a.x - px)) / area;
                const double w2 = 1.0 - w0 - w1;
                if (w0 < -1e-6 || w1 < -1e-6 || w2 < -1e-6) {
                    continue;
                }
                const double v = w0 * ia + w1 * ib + w2 * ic;
                const float z = static_cast<float>(frame.perspective ? 1.0 / v : v);
                float &d = at(ix, iy);
                if (z < d) {
                    d = z;
                }
            }
        }
    }

    // visibilidade das arestas
    const double bias = 0.0012 * std::max(1.0, maxZ - minZ) + 1.0;
    for (const Edge &e : projectedEdges) {
        const double dx = e.b.x - e.a.x, dy = e.b.y - e.a.y;
        const double length = std::hypot(dx, dy);
        const int steps = std::max(2, static_cast<int>(std::ceil(length / cell)));
        bool runVisible = false;
        double startS = 0.0;
        auto flush = [&](double s0, double s1, bool visible) {
            if (s1 - s0 <= 1e-9) {
                return;
            }
            Segment2D seg {e.a.x + dx * s0, e.a.y + dy * s0, e.a.x + dx * s1, e.a.y + dy * s1, e.tag, !visible};
            if (visible || keepHidden) {
                result.segments.push_back(seg);
            }
        };
        double previousS = 0.0;
        for (int i = 0; i <= steps; ++i) {
            const double s = static_cast<double>(i) / steps;
            const double x = e.a.x + dx * s, y = e.a.y + dy * s;
            const double inv = frame.perspective ? (1.0 - s) / e.a.z + s / e.b.z : 0.0;
            const double z = frame.perspective ? 1.0 / inv : e.a.z + (e.b.z - e.a.z) * s;
            const int ix = std::clamp(cellX(x), 0, nx - 1), iy = std::clamp(cellY(y), 0, ny - 1);
            const bool visible = z <= static_cast<double>(at(ix, iy)) + bias;
            if (i == 0) {
                runVisible = visible;
                startS = 0.0;
            } else if (visible != runVisible) {
                const double mid = 0.5 * (previousS + s);
                flush(startS, mid, runVisible);
                startS = mid;
                runVisible = visible;
            }
            previousS = s;
        }
        flush(startS, 1.0, runVisible);
    }
    return result;
}

} // namespace Structura::Drawing
