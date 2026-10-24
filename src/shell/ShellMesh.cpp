#include "core/math/Vec3Ops.h"
#include "shell/ShellMesh.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace Structura::Shell {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMergeTol = 1.0e-6;

using Structura::Math::cross;
using Structura::Math::dot;

Vec3 operator+(const Vec3 &a, const Vec3 &b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 operator-(const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 operator*(const Vec3 &a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double length(const Vec3 &a) { return std::sqrt(dot(a, a)); }
Vec3 normalized(const Vec3 &a)
{
    const double l = length(a);
    return l > 0.0 ? a * (1.0 / l) : Vec3 {0.0, 0.0, 1.0};
}
Vec3 mid(const Vec3 &a, const Vec3 &b) { return (a + b) * 0.5; }

/// Coordenadas isoparametricas (xi, eta em [-1, 1]) do ponto mais proximo de `x` na superficie bilinear do elemento e distancia a ela.
struct Location {
    double xi {0.0}, eta {0.0}, distance {1e300};
};

const double kSx[4] = {-1.0, 1.0, 1.0, -1.0};
const double kSy[4] = {-1.0, -1.0, 1.0, 1.0};

Location locateInQuad(const std::array<Vec3, 4> &p, const Vec3 &x)
{
    Location loc;
    for (int iter = 0; iter < 12; ++iter) {
        Vec3 pos {0, 0, 0}, dxi {0, 0, 0}, deta {0, 0, 0};
        for (std::size_t i = 0; i < 4; ++i) {
            const double nv = 0.25 * (1.0 + kSx[i] * loc.xi) * (1.0 + kSy[i] * loc.eta);
            const double nx = 0.25 * kSx[i] * (1.0 + kSy[i] * loc.eta), ny = 0.25 * kSy[i] * (1.0 + kSx[i] * loc.xi);
            pos = pos + p[i] * nv;
            dxi = dxi + p[i] * nx;
            deta = deta + p[i] * ny;
        }
        const Vec3 r = pos - x;
        const double a11 = dot(dxi, dxi), a12 = dot(dxi, deta), a22 = dot(deta, deta);
        const double det = a11 * a22 - a12 * a12;
        if (std::abs(det) < 1e-18) {
            break;
        }
        const double b1 = -dot(dxi, r), b2 = -dot(deta, r);
        const double dxiStep = (b1 * a22 - b2 * a12) / det, detaStep = (a11 * b2 - a12 * b1) / det;
        loc.xi += dxiStep;
        loc.eta += detaStep;
        if (std::abs(dxiStep) + std::abs(detaStep) < 1e-12) {
            break;
        }
    }
    Vec3 pos {0, 0, 0};
    for (std::size_t i = 0; i < 4; ++i) {
        pos = pos + p[i] * (0.25 * (1.0 + kSx[i] * loc.xi) * (1.0 + kSy[i] * loc.eta));
    }
    loc.distance = length(pos - x);
    return loc;
}

Tie bilinearTie(const MeshQuad &q, double xi, double eta)
{
    Tie tie;
    tie.slave = -1;
    for (std::size_t i = 0; i < 4; ++i) {
        const double w = 0.25 * (1.0 + kSx[i] * xi) * (1.0 + kSy[i] * eta);
        if (w > 1e-9) {
            tie.masters.push_back(q.n[i]);
            tie.weights.push_back(w);
        }
    }
    double total = 0.0;
    for (const double w : tie.weights) {
        total += w;
    }
    for (double &w : tie.weights) {
        w /= total;
    }
    return tie;
}

class Builder
{
public:
    explicit Builder(Mesh &mesh)
        : m_mesh(mesh)
    {
    }

    int zone(const std::string &name)
    {
        for (std::size_t i = 0; i < m_mesh.zones.size(); ++i) {
            if (m_mesh.zones[i] == name) {
                return static_cast<int>(i);
            }
        }
        m_mesh.zones.push_back(name);
        return static_cast<int>(m_mesh.zones.size()) - 1;
    }

    int node(const Vec3 &p)
    {
        const std::array<long long, 3> cell = cellOf(p);
        for (long long dx = -1; dx <= 1; ++dx) {
            for (long long dy = -1; dy <= 1; ++dy) {
                for (long long dz = -1; dz <= 1; ++dz) {
                    const auto it = m_grid.find({cell[0] + dx, cell[1] + dy, cell[2] + dz});
                    if (it == m_grid.end()) {
                        continue;
                    }
                    for (const int id : it->second) {
                        if (length(m_mesh.nodes[static_cast<std::size_t>(id)] - p) < kMergeTol) {
                            return id;
                        }
                    }
                }
            }
        }
        const int id = static_cast<int>(m_mesh.nodes.size());
        m_mesh.nodes.push_back(p);
        m_grid[cell].push_back(id);
        return id;
    }

    /// Acrescenta o quadrilatero; se a normal geometrica contraria a esperada, inverte a ordem dos nos.
    void quad(std::array<int, 4> n, int object, int zone, double thickness, int material, const Vec3 &expected)
    {
        const Vec3 &p1 = m_mesh.nodes[static_cast<std::size_t>(n[0])], &p2 = m_mesh.nodes[static_cast<std::size_t>(n[1])];
        const Vec3 &p3 = m_mesh.nodes[static_cast<std::size_t>(n[2])], &p4 = m_mesh.nodes[static_cast<std::size_t>(n[3])];
        const Vec3 normal = cross(p3 - p1, p4 - p2);
        if (length(normal) < 1e-18) {
            return;   // elemento degenerado
        }
        if (dot(normal, expected) < 0.0) {
            std::swap(n[1], n[3]);
        }
        MeshQuad q;
        q.n = n;
        q.object = object;
        q.zone = zone;
        q.thickness = thickness;
        q.material = material;
        m_mesh.quads.push_back(q);
    }

private:
    static std::array<long long, 3> cellOf(const Vec3 &p)
    {
        constexpr double cell = 1.0e-4;
        return {static_cast<long long>(std::floor(p[0] / cell)), static_cast<long long>(std::floor(p[1] / cell)), static_cast<long long>(std::floor(p[2] / cell))};
    }

    Mesh &m_mesh;
    std::map<std::array<long long, 3>, std::vector<int>> m_grid;
};

Vec3 polygonNormal(const std::vector<Vec3> &poly)
{
    Vec3 n {0.0, 0.0, 0.0};
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const Vec3 &a = poly[i], &b = poly[(i + 1) % poly.size()];
        n[0] += (a[1] - b[1]) * (a[2] + b[2]);
        n[1] += (a[2] - b[2]) * (a[0] + b[0]);
        n[2] += (a[0] - b[0]) * (a[1] + b[1]);
    }
    return normalized(n);
}

// ---- chapas ----------------------------------------------------------------------------------------------------------------

void structuredQuad(Builder &b, const std::array<Vec3, 4> &p, double h, int object, int zone, double t, int material, const Vec3 &normal)
{
    const int nx = std::max(1, static_cast<int>(std::ceil(std::max(length(p[1] - p[0]), length(p[2] - p[3])) / h - 1e-9)));
    const int ny = std::max(1, static_cast<int>(std::ceil(std::max(length(p[3] - p[0]), length(p[2] - p[1])) / h - 1e-9)));
    std::vector<int> ids(static_cast<std::size_t>((nx + 1) * (ny + 1)));
    for (int j = 0; j <= ny; ++j) {
        for (int i = 0; i <= nx; ++i) {
            const double a = static_cast<double>(i) / nx, c = static_cast<double>(j) / ny;
            const Vec3 pt = p[0] * ((1 - a) * (1 - c)) + p[1] * (a * (1 - c)) + p[2] * (a * c) + p[3] * ((1 - a) * c);
            ids[static_cast<std::size_t>(j * (nx + 1) + i)] = b.node(pt);
        }
    }
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            auto id = [&](int ii, int jj) { return ids[static_cast<std::size_t>(jj * (nx + 1) + ii)]; };
            b.quad({id(i, j), id(i + 1, j), id(i + 1, j + 1), id(i, j + 1)}, object, zone, t, material, normal);
        }
    }
}

/// Triangulacao por orelhas no plano local (u, v) com u x v = normal.
std::vector<std::array<int, 3>> earClip(const std::vector<std::array<double, 2>> &pts)
{
    std::vector<std::array<int, 3>> tri;
    std::vector<int> idx(pts.size());
    for (std::size_t i = 0; i < idx.size(); ++i) {
        idx[i] = static_cast<int>(i);
    }
    auto cross2 = [&](int a, int b, int c) {
        return (pts[b][0] - pts[a][0]) * (pts[c][1] - pts[a][1]) - (pts[b][1] - pts[a][1]) * (pts[c][0] - pts[a][0]);
    };
    auto inside = [&](int a, int b, int c, int p) {
        return cross2(a, b, p) >= -1e-12 && cross2(b, c, p) >= -1e-12 && cross2(c, a, p) >= -1e-12;
    };
    int guard = 0;
    while (idx.size() > 3 && guard++ < 10000) {
        bool clipped = false;
        for (std::size_t i = 0; i < idx.size(); ++i) {
            const int a = idx[(i + idx.size() - 1) % idx.size()], b = idx[i], c = idx[(i + 1) % idx.size()];
            if (cross2(a, b, c) <= 1e-14) {
                continue;   // vertice reflexo ou colinear
            }
            bool ear = true;
            for (const int p : idx) {
                if (p != a && p != b && p != c && inside(a, b, c, p)) {
                    ear = false;
                    break;
                }
            }
            if (ear) {
                tri.push_back({a, b, c});
                idx.erase(idx.begin() + static_cast<long>(i));
                clipped = true;
                break;
            }
        }
        if (!clipped) {
            break;
        }
    }
    if (idx.size() == 3) {
        tri.push_back({idx[0], idx[1], idx[2]});
    }
    return tri;
}

void polygonMesh(Builder &b, const std::vector<Vec3> &poly, double h, int object, int zone, double t, int material, std::vector<std::string> &warnings, const std::string &name)
{
    const Vec3 normal = polygonNormal(poly);
    if (poly.size() == 4) {
        structuredQuad(b, {poly[0], poly[1], poly[2], poly[3]}, h, object, zone, t, material, normal);
        return;
    }
    // sistema local
    Vec3 u = normalized(poly[1] - poly[0]);
    u = normalized(u - normal * dot(u, normal));
    const Vec3 v = cross(normal, u);
    std::vector<std::array<double, 2>> pts;
    for (const Vec3 &p : poly) {
        pts.push_back({dot(p - poly[0], u), dot(p - poly[0], v)});
    }
    const auto triangles = earClip(pts);
    if (triangles.empty()) {
        warnings.push_back(name + ": poligono nao pode ser triangulado (autointersecao ou vertices colineares).");
        return;
    }
    std::vector<std::array<Vec3, 4>> quads;
    for (const auto &t3 : triangles) {
        const Vec3 &a = poly[static_cast<std::size_t>(t3[0])], &bb = poly[static_cast<std::size_t>(t3[1])], &c = poly[static_cast<std::size_t>(t3[2])];
        const Vec3 g = (a + bb + c) * (1.0 / 3.0);
        quads.push_back({a, mid(a, bb), g, mid(c, a)});
        quads.push_back({bb, mid(bb, c), g, mid(a, bb)});
        quads.push_back({c, mid(c, a), g, mid(bb, c)});
    }
    auto maxEdge = [&]() {
        double e = 0.0;
        for (const auto &q : quads) {
            for (int i = 0; i < 4; ++i) {
                e = std::max(e, length(q[static_cast<std::size_t>((i + 1) % 4)] - q[static_cast<std::size_t>(i)]));
            }
        }
        return e;
    };
    for (int level = 0; level < 8 && maxEdge() > h * 1.0001; ++level) {
        std::vector<std::array<Vec3, 4>> next;
        next.reserve(quads.size() * 4);
        for (const auto &q : quads) {
            const Vec3 m01 = mid(q[0], q[1]), m12 = mid(q[1], q[2]), m23 = mid(q[2], q[3]), m30 = mid(q[3], q[0]);
            const Vec3 c = (q[0] + q[1] + q[2] + q[3]) * 0.25;
            next.push_back({q[0], m01, c, m30});
            next.push_back({m01, q[1], m12, c});
            next.push_back({c, m12, q[2], m23});
            next.push_back({m30, c, m23, q[3]});
        }
        quads = std::move(next);
    }
    for (const auto &q : quads) {
        b.quad({b.node(q[0]), b.node(q[1]), b.node(q[2]), b.node(q[3])}, object, zone, t, material, normal);
    }
}

// ---- superficies de revolucao -----------------------------------------------------------------------------------------------

struct Profile {
    std::vector<std::array<double, 2>> pts;   ///< (r, z)
    double thickness {0.0};
    int zone {0};
};

/// Sistema de referencia da superficie de revolucao: eixo ez, angulo medido de ex para ey.
struct Frame {
    Vec3 origin {0.0, 0.0, 0.0};
    Vec3 ex {1.0, 0.0, 0.0}, ey {0.0, 1.0, 0.0}, ez {0.0, 0.0, 1.0};
    Vec3 map(const Vec3 &local) const { return origin + rotate(local); }
    Vec3 rotate(const Vec3 &local) const { return ex * local[0] + ey * local[1] + ez * local[2]; }
};

/// Eixo vertical (padrao) ou horizontal ao longo de X (vasos horizontais).
Frame axisFrame(const Vec3 &origin, bool horizontal)
{
    Frame f;
    f.origin = origin;
    if (horizontal) {
        f.ex = {0.0, 0.0, -1.0};
        f.ey = {0.0, 1.0, 0.0};
        f.ez = {1.0, 0.0, 0.0};
    }
    return f;
}

/// Eixo qualquer: ez = direcao do eixo e ex = `radial` projetada no plano normal (se nula, um vetor perpendicular qualquer).
Frame generalFrame(const Vec3 &origin, const Vec3 &direction, const Vec3 &radial)
{
    Frame f;
    f.origin = origin;
    f.ez = normalized(direction);
    Vec3 ex = radial - f.ez * dot(radial, f.ez);
    if (length(ex) < 1e-9) {
        ex = std::abs(f.ez[0]) < 0.9 ? Vec3 {1.0, 0.0, 0.0} : Vec3 {0.0, 1.0, 0.0};
        ex = ex - f.ez * dot(ex, f.ez);
    }
    f.ex = normalized(ex);
    f.ey = cross(f.ez, f.ex);
    return f;
}

void line(Profile &p, double r0, double z0, double r1, double z1, double h)
{
    const int n = std::max(1, static_cast<int>(std::ceil(std::hypot(r1 - r0, z1 - z0) / h - 1e-9)));
    for (int i = (p.pts.empty() ? 0 : 1); i <= n; ++i) {
        const double a = static_cast<double>(i) / n;
        p.pts.push_back({r0 + (r1 - r0) * a, z0 + (z1 - z0) * a});
    }
}

/// Arco de elipse centrado em (rc, zc), semi-eixos (ar, az), do angulo a0 ao a1 (rad): (rc + ar cos, zc + az sin).
void arc(Profile &p, double rc, double zc, double ar, double az, double a0, double a1, double h)
{
    // comprimento aproximado por amostragem
    double len = 0.0;
    constexpr int probe = 64;
    for (int i = 0; i < probe; ++i) {
        const double s0 = a0 + (a1 - a0) * i / probe, s1 = a0 + (a1 - a0) * (i + 1) / probe;
        len += std::hypot(ar * (std::cos(s1) - std::cos(s0)), az * (std::sin(s1) - std::sin(s0)));
    }
    const int n = std::max(2, static_cast<int>(std::ceil(len / h - 1e-9)));
    for (int i = (p.pts.empty() ? 0 : 1); i <= n; ++i) {
        const double a = a0 + (a1 - a0) * i / n;
        p.pts.push_back({rc + ar * std::cos(a), zc + az * std::sin(a)});
    }
}

void revolve(Builder &b, const std::vector<Profile> &profiles, int m, int object, int material, const Frame &frame, double span = 2.0 * kPi, double orient = 1.0)
{
    const bool closed = span > 2.0 * kPi - 1e-9;
    const int columns = closed ? m : m + 1;
    auto wrap = [&](int i) { return closed ? i % m : i; };
    auto ring = [&](double r, double z) {
        std::vector<int> ids;
        if (r < 1e-9) {
            ids.push_back(b.node(frame.map({0.0, 0.0, z})));
            return ids;
        }
        for (int i = 0; i < columns; ++i) {
            const double th = span * i / m;
            ids.push_back(b.node(frame.map({r * std::cos(th), r * std::sin(th), z})));
        }
        return ids;
    };
    for (const Profile &p : profiles) {
        for (std::size_t k = 0; k + 1 < p.pts.size(); ++k) {
            const double ra = p.pts[k][0], za = p.pts[k][1], rb = p.pts[k + 1][0], zb = p.pts[k + 1][1];
            const std::vector<int> a = ring(ra, za), c = ring(rb, zb);
            const double dr = rb - ra, dz = zb - za;
            auto expected = [&](double th) {
                const Vec3 thetaHat {-std::sin(th), std::cos(th), 0.0};
                const Vec3 sHat = normalized(Vec3 {dr * std::cos(th), dr * std::sin(th), dz});
                return frame.rotate(cross(thetaHat, sHat)) * orient;
            };
            if (a.size() == 1 && c.size() == 1) {
                continue;
            }
            if (a.size() == 1) {
                for (int i = 0; i < m / 2; ++i) {
                    const double th = span * (2 * i + 1) / m;
                    b.quad({a[0], c[static_cast<std::size_t>(2 * i)], c[static_cast<std::size_t>(2 * i + 1)], c[static_cast<std::size_t>(wrap(2 * i + 2))]}, object, p.zone, p.thickness, material, expected(th));
                }
            } else if (c.size() == 1) {
                for (int i = 0; i < m / 2; ++i) {
                    const double th = span * (2 * i + 1) / m;
                    b.quad({a[static_cast<std::size_t>(2 * i)], a[static_cast<std::size_t>(2 * i + 1)], a[static_cast<std::size_t>(wrap(2 * i + 2))], c[0]}, object, p.zone, p.thickness, material, expected(th));
                }
            } else {
                for (int i = 0; i < m; ++i) {
                    const double th = span * (i + 0.5) / m;
                    b.quad({a[static_cast<std::size_t>(i)], a[static_cast<std::size_t>(wrap(i + 1))], c[static_cast<std::size_t>(wrap(i + 1))], c[static_cast<std::size_t>(i)]}, object, p.zone, p.thickness, material, expected(th));
                }
            }
        }
    }
}

int circumferentialCount(double rMax, double h)
{
    const int n = static_cast<int>(std::ceil(2.0 * kPi * rMax / h / 4.0)) * 4;
    return std::max(8, n);
}

// ---- tipos de objeto ----------------------------------------------------------------------------------------------------------------

void rectTank(Builder &b, const Object &o, int index, Mesh &mesh)
{
    const double lx = o.param("lx", 4.0), ly = o.param("ly", 4.0), hh = o.param("h", 3.0);
    const double tw = o.param("tWall", 0.2), tb = o.param("tBottom", 0.2), tr = o.param("tRoof", 0.0);
    const double x0 = o.param("x0"), y0 = o.param("y0"), z0 = o.param("z0");
    const double h = o.meshSize;
    auto P = [&](double x, double y, double z) { return Vec3 {x0 + x, y0 + y, z0 + z}; };
    const int mat = o.material;
    auto face = [&](const std::string &name, const std::array<Vec3, 4> &poly, double t) {
        if (t <= 0.0) {
            return;
        }
        const int zone = b.zone(o.name + ": " + name);
        structuredQuad(b, poly, h, index, zone, t, mat, polygonNormal({poly[0], poly[1], poly[2], poly[3]}));
    };
    face("fundo", {P(0, 0, 0), P(0, ly, 0), P(lx, ly, 0), P(lx, 0, 0)}, tb);
    face("parede y-", {P(0, 0, 0), P(lx, 0, 0), P(lx, 0, hh), P(0, 0, hh)}, tw);
    face("parede x+", {P(lx, 0, 0), P(lx, ly, 0), P(lx, ly, hh), P(lx, 0, hh)}, tw);
    face("parede y+", {P(lx, ly, 0), P(0, ly, 0), P(0, ly, hh), P(lx, ly, hh)}, tw);
    face("parede x-", {P(0, ly, 0), P(0, 0, 0), P(0, 0, hh), P(0, ly, hh)}, tw);
    face("tampa", {P(0, 0, hh), P(lx, 0, hh), P(lx, ly, hh), P(0, ly, hh)}, tr);
    (void)mesh;
}

void cylTank(Builder &b, const Object &o, int index)
{
    const double R = o.param("r", 5.0), H = o.param("h", 8.0), h = o.meshSize;
    const int courses = std::max(1, static_cast<int>(o.param("courses", 1.0) + 0.5));
    const double tb = o.param("tBottom", 0.008);
    const int roof = static_cast<int>(o.param("roof", 0.0) + 0.5);
    const double rise = o.param("roofRise", 1.0), tr = o.param("tRoof", 0.005);
    std::vector<Profile> profiles;
    if (tb > 0.0) {
        Profile bottom;
        bottom.thickness = tb;
        bottom.zone = b.zone(o.name + ": fundo");
        line(bottom, 0.0, 0.0, R, 0.0, h);
        profiles.push_back(bottom);
    }
    for (int c = 0; c < courses; ++c) {
        Profile wall;
        const double t = o.param("t" + std::to_string(c + 1), o.param("t1", 0.008));
        wall.thickness = t;
        wall.zone = b.zone(o.name + ": virola " + std::to_string(c + 1));
        line(wall, R, H * c / courses, R, H * (c + 1) / courses, h);
        profiles.push_back(wall);
    }
    if (roof > 0 && tr > 0.0) {
        Profile top;
        top.thickness = tr;
        top.zone = b.zone(o.name + ": teto");
        if (roof == 1) {
            line(top, R, H, 0.0, H, h);
        } else if (roof == 2) {
            line(top, R, H, 0.0, H + rise, h);
        } else {
            const double rs = (R * R + rise * rise) / (2.0 * rise);
            const double phi0 = std::asin(std::min(1.0, R / rs));
            // centro da esfera em (0, H + rise - rs); ponto do arco: (rs sin phi, zc + rs cos phi)
            const double zc = H + rise - rs;
            const int n = std::max(2, static_cast<int>(std::ceil(rs * phi0 / h - 1e-9)));
            for (int i = 0; i <= n; ++i) {
                const double phi = phi0 * (1.0 - static_cast<double>(i) / n);
                top.pts.push_back({rs * std::sin(phi), zc + rs * std::cos(phi)});
            }
        }
        profiles.push_back(top);
    }
    const Frame frame = axisFrame({o.param("x0"), o.param("y0"), o.param("z0")}, false);
    revolve(b, profiles, circumferentialCount(R, h), index, o.material, frame);
}

void vessel(Builder &b, const Object &o, int index)
{
    const double R = o.param("r", 1.0), Lc = o.param("length", 3.0), t = o.param("t", 0.012), h = o.meshSize;
    const int head = static_cast<int>(o.param("head", 1.0) + 0.5);
    const double th = o.param("tHead", t);
    const double depth = head == 0 ? R : (head == 1 ? R / 2.0 : 0.0);
    const bool horizontal = o.param("horizontal", 0.0) != 0.0;
    const double skirt = horizontal ? 0.0 : o.param("skirt", 0.0), tSkirt = o.param("tSkirt", t);
    std::vector<Profile> profiles;
    if (skirt > 0.0) {
        Profile s;
        s.thickness = tSkirt;
        s.zone = b.zone(o.name + ": saia");
        line(s, R, -skirt, R, 0.0, h);
        profiles.push_back(s);
    }
    Profile bottom;
    bottom.thickness = th;
    bottom.zone = b.zone(o.name + ": tampo inferior");
    if (depth > 0.0) {
        // ponto (r, z) = (R sin phi, -depth cos phi), phi de 0 (polo) a 90 graus (junta com o cilindro)
        const double arcLen = 0.5 * kPi * 0.5 * (R + depth);
        const int n = std::max(2, static_cast<int>(std::ceil(arcLen / h - 1e-9)));
        for (int i = 0; i <= n; ++i) {
            const double phi = 0.5 * kPi * i / n;
            bottom.pts.push_back({R * std::sin(phi), -depth * std::cos(phi)});
        }
    } else {
        line(bottom, 0.0, 0.0, R, 0.0, h);
    }
    profiles.push_back(bottom);
    Profile shellPart;
    shellPart.thickness = t;
    shellPart.zone = b.zone(o.name + ": casco");
    line(shellPart, R, 0.0, R, Lc, h);
    profiles.push_back(shellPart);
    Profile topHead;
    topHead.thickness = th;
    topHead.zone = b.zone(o.name + ": tampo superior");
    if (depth > 0.0) {
        const double arcLen = 0.5 * kPi * 0.5 * (R + depth);
        const int n = std::max(2, static_cast<int>(std::ceil(arcLen / h - 1e-9)));
        for (int i = 0; i <= n; ++i) {
            const double phi = 0.5 * kPi * i / n;
            topHead.pts.push_back({R * std::cos(phi), Lc + depth * std::sin(phi)});
        }
    } else {
        line(topHead, R, Lc, 0.0, Lc, h);
    }
    profiles.push_back(topHead);
    const Frame frame = axisFrame({o.param("x0"), o.param("y0"), o.param("z0")}, horizontal);
    revolve(b, profiles, circumferentialCount(R, h), index, o.material, frame);
}

// ---- superficies desenhadas no modelo -----------------------------------------------------------------------------------------------

/// Numero de divisoes circunferenciais para um arco de raio `r` e abertura `span`.
int arcDivisions(double r, double span, double h)
{
    if (span > 2.0 * kPi - 1e-9) {
        return circumferentialCount(r, h);
    }
    return std::max(2, 2 * static_cast<int>(std::ceil(span * r / h / 2.0)));
}

void disk(Builder &b, const Object &o, int index)
{
    if (o.polygon.empty()) {
        return;
    }
    const double R = o.param("r", 1.0), rIn = std::min(o.param("rIn", 0.0), R * 0.999);
    const Vec3 n {o.param("nx", 0.0), o.param("ny", 0.0), o.param("nz", 1.0)};
    const Frame frame = generalFrame(o.polygon[0], n, {0.0, 0.0, 0.0});
    Profile p;
    p.thickness = o.param("t", 0.01);
    p.zone = b.zone(o.name);
    line(p, rIn, 0.0, R, 0.0, o.meshSize);
    // a normal natural da revolucao (raio crescente) e -ez: inverte para que a normal positiva seja a informada
    const double orient = o.param("flip", 0.0) != 0.0 ? 1.0 : -1.0;
    revolve(b, {p}, arcDivisions(R, 2.0 * kPi, o.meshSize), index, o.material, frame, 2.0 * kPi, orient);
}

void extrusion(Builder &b, const Object &o, int index)
{
    const Vec3 v {o.param("vx"), o.param("vy"), o.param("vz", 1.0)};
    const double len = length(v);
    if (o.polygon.empty() || len < 1e-9) {
        return;
    }
    const double h = o.meshSize, t = o.param("t", 0.01);
    const double orient = o.param("flip", 0.0) != 0.0 ? -1.0 : 1.0;
    if (o.param("circle", 0.0) != 0.0) {
        // cilindro, cone ou tronco: revolucao em torno do eixo de extrusao
        const double R = o.param("r", 1.0), scale = o.param("endScale", 1.0);
        Profile p;
        p.thickness = t;
        p.zone = b.zone(o.name);
        line(p, R, 0.0, R * scale, len, h);
        revolve(b, {p}, circumferentialCount(std::max(R, R * scale), h), index, o.material, generalFrame(o.polygon[0], v, {0.0, 0.0, 0.0}), 2.0 * kPi, orient);
        return;
    }
    if (o.polygon.size() < 2) {
        return;
    }
    const bool closed = o.param("closed", 0.0) != 0.0 && o.polygon.size() >= 3;
    // pontos ao longo da linha base, subdivididos
    std::vector<Vec3> pts;
    std::vector<Vec3> dirs;   // direcao do segmento que parte de cada ponto
    const std::size_t segments = closed ? o.polygon.size() : o.polygon.size() - 1;
    for (std::size_t s = 0; s < segments; ++s) {
        const Vec3 &a = o.polygon[s], &c = o.polygon[(s + 1) % o.polygon.size()];
        const double l = length(c - a);
        if (l < 1e-9) {
            continue;
        }
        const int parts = std::max(1, static_cast<int>(std::ceil(l / h - 1e-9)));
        for (int k = 0; k < parts; ++k) {
            pts.push_back(a + (c - a) * (static_cast<double>(k) / parts));
            dirs.push_back(normalized(c - a));
        }
    }
    if (!closed) {
        pts.push_back(o.polygon.back());
        dirs.push_back(dirs.empty() ? Vec3 {1.0, 0.0, 0.0} : dirs.back());
    }
    const int rows = std::max(1, static_cast<int>(std::ceil(len / h - 1e-9)));
    const int zone = b.zone(o.name);
    const int cols = static_cast<int>(pts.size());
    auto id = [&](int i, int j) {
        const Vec3 p = pts[static_cast<std::size_t>(i % cols)] + v * (static_cast<double>(j) / rows);
        return b.node(p);
    };
    const int count = closed ? cols : cols - 1;
    for (int i = 0; i < count; ++i) {
        for (int j = 0; j < rows; ++j) {
            const Vec3 expected = cross(dirs[static_cast<std::size_t>(i)], normalized(v)) * orient;
            b.quad({id(i, j), id(i + 1, j), id(i + 1, j + 1), id(i, j + 1)}, index, zone, t, o.material, expected);
        }
    }
}

void revolution(Builder &b, const Object &o, int index, std::vector<std::string> &warnings)
{
    if (o.polygon.size() < 2) {
        return;
    }
    const Vec3 axisPoint {o.param("ax"), o.param("ay"), o.param("az")};
    const Vec3 dir {o.param("dx"), o.param("dy"), o.param("dz", 1.0)};
    if (length(dir) < 1e-12) {
        warnings.push_back(o.name + ": eixo de revolucao invalido.");
        return;
    }
    const Vec3 axis = normalized(dir);
    // direcao radial de referencia: ponto do perfil mais afastado do eixo
    Vec3 radial {0.0, 0.0, 0.0};
    double far = -1.0;
    for (const Vec3 &p : o.polygon) {
        const Vec3 d = p - axisPoint;
        const Vec3 r = d - axis * dot(d, axis);
        if (length(r) > far) {
            far = length(r);
            radial = r;
        }
    }
    if (far < 1e-9) {
        warnings.push_back(o.name + ": o perfil esta sobre o eixo.");
        return;
    }
    const Frame frame = generalFrame(axisPoint, axis, radial);
    Profile prof;
    prof.thickness = o.param("t", 0.01);
    prof.zone = b.zone(o.name);
    double rMax = 0.0;
    for (std::size_t i = 0; i + 1 < o.polygon.size(); ++i) {
        auto toRZ = [&](const Vec3 &p) {
            const Vec3 d = p - axisPoint;
            const double z = dot(d, axis);
            const Vec3 r = d - axis * z;
            // lado do perfil em relacao a direcao de referencia (negativo: espelhado)
            const double signedR = dot(r, frame.ex);
            return std::array<double, 2> {std::max(0.0, signedR), z};
        };
        const auto a = toRZ(o.polygon[i]), c = toRZ(o.polygon[i + 1]);
        rMax = std::max({rMax, a[0], c[0]});
        line(prof, a[0], a[1], c[0], c[1], o.meshSize);
    }
    const double angle = std::clamp(o.param("angle", 360.0), 1.0, 360.0) * kPi / 180.0;
    const double orient = o.param("flip", 0.0) != 0.0 ? -1.0 : 1.0;
    revolve(b, {prof}, arcDivisions(std::max(rMax, o.meshSize), angle, o.meshSize), index, o.material, frame, angle, orient);
}

} // namespace

Vec3 quadNormal(const Mesh &mesh, const MeshQuad &q)
{
    const Vec3 &p1 = mesh.nodes[static_cast<std::size_t>(q.n[0])], &p2 = mesh.nodes[static_cast<std::size_t>(q.n[1])];
    const Vec3 &p3 = mesh.nodes[static_cast<std::size_t>(q.n[2])], &p4 = mesh.nodes[static_cast<std::size_t>(q.n[3])];
    return normalized(cross(p3 - p1, p4 - p2));
}

double quadArea(const Mesh &mesh, const MeshQuad &q)
{
    const Vec3 &p1 = mesh.nodes[static_cast<std::size_t>(q.n[0])], &p2 = mesh.nodes[static_cast<std::size_t>(q.n[1])];
    const Vec3 &p3 = mesh.nodes[static_cast<std::size_t>(q.n[2])], &p4 = mesh.nodes[static_cast<std::size_t>(q.n[3])];
    return 0.5 * length(cross(p3 - p1, p4 - p2));
}

Mesh generateMesh(const Model &model)
{
    Mesh mesh;
    Builder builder(mesh);
    for (std::size_t i = 0; i < model.objects.size(); ++i) {
        const Object &o = model.objects[i];
        if (!o.visible) {
            continue;
        }
        if (o.meshSize <= 1.0e-4) {
            mesh.warnings.push_back(o.name + ": dimensao de malha invalida.");
            continue;
        }
        if (o.material < 0 || o.material >= static_cast<int>(model.materials.size())) {
            mesh.warnings.push_back(o.name + ": material inexistente.");
            continue;
        }
        const int index = static_cast<int>(i);
        switch (o.kind) {
        case Kind::Plate: {
            if (o.polygon.size() < 3 || o.param("t") <= 0.0) {
                mesh.warnings.push_back(o.name + ": poligono ou espessura invalidos.");
                break;
            }
            polygonMesh(builder, o.polygon, o.meshSize, index, builder.zone(o.name), o.param("t"), o.material, mesh.warnings, o.name);
            break;
        }
        case Kind::RectTank: rectTank(builder, o, index, mesh); break;
        case Kind::CylTank: cylTank(builder, o, index); break;
        case Kind::Vessel: vessel(builder, o, index); break;
        case Kind::Disk: disk(builder, o, index); break;
        case Kind::Extrusion: extrusion(builder, o, index); break;
        case Kind::Revolution: revolution(builder, o, index, mesh.warnings); break;
        }
    }
    mesh.ties = findEdgeTies(model, mesh);
    return mesh;
}

std::vector<Tie> findEdgeTies(const Model &model, const Mesh &mesh)
{
    std::vector<Tie> ties;
    const std::size_t nn = mesh.nodes.size();
    if (nn == 0 || mesh.quads.empty()) {
        return ties;
    }
    // arestas livres (um unico elemento) e objetos de cada no
    std::map<std::pair<int, int>, int> edgeCount;
    std::vector<std::vector<int>> nodeObjects(nn);
    std::vector<double> nodeSize(nn, 1e300);
    std::vector<char> onBoundary(nn, 0);
    for (const MeshQuad &q : mesh.quads) {
        for (int i = 0; i < 4; ++i) {
            const int a = q.n[static_cast<std::size_t>(i)], b = q.n[static_cast<std::size_t>((i + 1) % 4)];
            ++edgeCount[{std::min(a, b), std::max(a, b)}];
            const double len = length(mesh.nodes[static_cast<std::size_t>(a)] - mesh.nodes[static_cast<std::size_t>(b)]);
            nodeSize[static_cast<std::size_t>(a)] = std::min(nodeSize[static_cast<std::size_t>(a)], len);
            nodeSize[static_cast<std::size_t>(b)] = std::min(nodeSize[static_cast<std::size_t>(b)], len);
        }
        for (const int n : q.n) {
            auto &list = nodeObjects[static_cast<std::size_t>(n)];
            if (std::find(list.begin(), list.end(), q.object) == list.end()) {
                list.push_back(q.object);
            }
        }
    }
    for (const auto &[edge, count] : edgeCount) {
        if (count == 1) {
            onBoundary[static_cast<std::size_t>(edge.first)] = 1;
            onBoundary[static_cast<std::size_t>(edge.second)] = 1;
        }
    }
    auto curved = [&](int object) {
        const Kind k = model.objects[static_cast<std::size_t>(object)].kind;
        return k == Kind::CylTank || k == Kind::Vessel || k == Kind::Revolution || k == Kind::Disk || k == Kind::Extrusion;
    };
    struct Box {
        Vec3 lo, hi;
        double size {0.0};
    };
    std::vector<Box> boxes(mesh.quads.size());
    for (std::size_t e = 0; e < mesh.quads.size(); ++e) {
        const MeshQuad &q = mesh.quads[e];
        Box &b = boxes[e];
        b.lo = {1e300, 1e300, 1e300};
        b.hi = {-1e300, -1e300, -1e300};
        for (const int n : q.n) {
            for (std::size_t k = 0; k < 3; ++k) {
                b.lo[k] = std::min(b.lo[k], mesh.nodes[static_cast<std::size_t>(n)][k]);
                b.hi[k] = std::max(b.hi[k], mesh.nodes[static_cast<std::size_t>(n)][k]);
            }
        }
        double sum = 0.0;
        for (int i = 0; i < 4; ++i) {
            sum += length(mesh.nodes[static_cast<std::size_t>(q.n[static_cast<std::size_t>(i)])] - mesh.nodes[static_cast<std::size_t>(q.n[static_cast<std::size_t>((i + 1) % 4)])]);
        }
        b.size = sum / 4.0;
    }
    for (std::size_t n = 0; n < nn; ++n) {
        if (!onBoundary[n] || nodeObjects[n].size() != 1) {
            continue;
        }
        const int ownObject = nodeObjects[n].front();
        const Vec3 &x = mesh.nodes[n];
        int bestHost = -1;
        double bestDistance = 1e300;
        double bestXi = 0.0, bestEta = 0.0;
        for (std::size_t e = 0; e < mesh.quads.size(); ++e) {
            const MeshQuad &q = mesh.quads[e];
            if (q.object == ownObject) {
                continue;
            }
            const Box &b = boxes[e];
            const double tolDist = kMergeTol + ((curved(ownObject) || curved(q.object)) ? 0.12 * b.size : 0.0);
            bool outside = false;
            for (std::size_t k = 0; k < 3 && !outside; ++k) {
                outside = x[k] < b.lo[k] - tolDist || x[k] > b.hi[k] + tolDist;
            }
            if (outside) {
                continue;
            }
            // o lado mais fino e o escravo
            const double ratio = nodeSize[n] / b.size;
            if (!(ratio < 0.95 || (ratio < 1.05 && ownObject > q.object))) {
                continue;
            }
            // coordenadas isoparametricas do ponto no elemento (Gauss-Newton em 3D)
            std::array<Vec3, 4> p;
            for (std::size_t i = 0; i < 4; ++i) {
                p[i] = mesh.nodes[static_cast<std::size_t>(q.n[i])];
            }
            const Location loc = locateInQuad(p, x);
            const double xi = loc.xi, eta = loc.eta, distance = loc.distance;
            const double slack = tolDist / std::max(b.size, 1e-12) * 2.0 + 1e-9;
            if (distance <= tolDist && std::abs(xi) <= 1.0 + slack && std::abs(eta) <= 1.0 + slack && distance < bestDistance) {
                bestDistance = distance;
                bestHost = static_cast<int>(e);
                bestXi = std::clamp(xi, -1.0, 1.0);
                bestEta = std::clamp(eta, -1.0, 1.0);
            }
        }
        if (bestHost < 0) {
            continue;
        }
        Tie tie = bilinearTie(mesh.quads[static_cast<std::size_t>(bestHost)], bestXi, bestEta);
        tie.slave = static_cast<int>(n);
        ties.push_back(std::move(tie));
    }
    return ties;
}

std::optional<Tie> hostTie(const Mesh &mesh, const Vec3 &x, double tol)
{
    std::optional<Tie> best;
    double bestDistance = 1e300;
    for (const MeshQuad &q : mesh.quads) {
        std::array<Vec3, 4> p;
        Vec3 lo {1e300, 1e300, 1e300}, hi {-1e300, -1e300, -1e300};
        for (std::size_t i = 0; i < 4; ++i) {
            p[i] = mesh.nodes[static_cast<std::size_t>(q.n[i])];
            for (std::size_t k = 0; k < 3; ++k) {
                lo[k] = std::min(lo[k], p[i][k]);
                hi[k] = std::max(hi[k], p[i][k]);
            }
        }
        if (x[0] < lo[0] - tol || x[0] > hi[0] + tol || x[1] < lo[1] - tol || x[1] > hi[1] + tol || x[2] < lo[2] - tol || x[2] > hi[2] + tol) {
            continue;
        }
        const Location loc = locateInQuad(p, x);
        const double size = std::max(length(p[2] - p[0]), length(p[3] - p[1])) / 1.4142135623730951;
        const double slack = tol / std::max(size, 1e-12) * 2.0 + 1e-9;
        if (loc.distance <= tol && std::abs(loc.xi) <= 1.0 + slack && std::abs(loc.eta) <= 1.0 + slack && loc.distance < bestDistance) {
            bestDistance = loc.distance;
            best = bilinearTie(q, std::clamp(loc.xi, -1.0, 1.0), std::clamp(loc.eta, -1.0, 1.0));
        }
    }
    return best;
}

} // namespace Structura::Shell
