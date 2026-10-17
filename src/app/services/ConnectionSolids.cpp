#include "app/services/ConnectionSolids.h"

#include "design/BasePlate.h"

#include <algorithm>
#include <cmath>

namespace Structura::App::Services {

using Structura::App::ProjectData::NodeConnection;
using Primitive = ConnectionSolids::Primitive;
using Kind = ConnectionSolids::Kind;
using Role = ConnectionSolids::Role;

namespace {

constexpr double kPi = 3.14159265358979323846;

double val(const NodeConnection &c, const char *key, double fallback = 0.0)
{
    return c.values.value(QString::fromLatin1(key), fallback);
}

int count(const NodeConnection &c, const char *key, double fallback)
{
    return std::max(1, static_cast<int>(std::lround(val(c, key, fallback))));
}

Primitive box(Role role, double s, double t, double u, double ds, double dt, double du)
{
    Primitive p;
    p.kind = Kind::Box;
    p.role = role;
    p.a = {s, t, u};
    p.size = {ds, dt, du};
    return p;
}

Primitive cylinder(Role role, std::array<double, 3> a, std::array<double, 3> b, double radius)
{
    Primitive p;
    p.kind = Kind::Cylinder;
    p.role = role;
    p.a = a;
    p.b = b;
    p.radius = radius;
    return p;
}

/// Parafuso com eixo ao longo de `axis` (0 = s, 1 = t, 2 = u): cabeca em `lo`, porca em `hi`.
void addBolt(std::vector<Primitive> &out, std::array<double, 3> center, int axis, double lo, double hi, double db)
{
    auto at = [&](double along) {
        std::array<double, 3> p = center;
        p[static_cast<std::size_t>(axis)] = along;
        return p;
    };
    out.push_back(cylinder(Role::Bolt, at(lo - 0.65 * db), at(lo), 0.8 * db));
    out.push_back(cylinder(Role::Bolt, at(lo), at(hi), 0.5 * db));
    out.push_back(cylinder(Role::Bolt, at(hi), at(hi + 0.8 * db), 0.8 * db));
}

void addPlate(std::vector<Primitive> &out, double s0, double s1, double h, double u0, double u1)
{
    Primitive p;
    p.kind = Kind::Prism;
    p.role = Role::Plate;
    p.polygon = {{s0, -h / 2.0}, {s1, -h / 2.0}, {s1, h / 2.0}, {s0, h / 2.0}};
    p.u0 = u0;
    p.u1 = u1;
    out.push_back(std::move(p));
}

void shearTab(const NodeConnection &c, std::vector<Primitive> &out, bool angles)
{
    const double db = val(c, "db", 20.0), pitch = val(c, "pitch", 70.0), edgeV = val(c, "edgeV", 35.0), webT = val(c, "webT", 6.0);
    const int rows = count(c, "rows", 3.0);
    const double height = (rows - 1) * pitch + 2.0 * edgeV;
    out.push_back(box(Role::Concrete, -10.0, 0.0, 0.0, 20.0, std::max(val(c, "beamDepth", 0.0), height + 40.0), std::max(webT, 60.0) + 60.0));   // apoio (referencia)
    if (!angles) {
        const double a = val(c, "a", 60.0), edgeH = val(c, "edgeH", 35.0), tp = val(c, "plateT", 9.5), weld = val(c, "weldLeg", 6.0);
        const double u0 = webT / 2.0, u1 = u0 + tp;
        addPlate(out, 0.0, a + edgeH, height, u0, u1);
        out.push_back(box(Role::Weld, weld / 2.0, 0.0, u1 + weld / 2.0, weld, height, weld));
        for (int i = 0; i < rows; ++i) {
            addBolt(out, {a, (i - (rows - 1) / 2.0) * pitch, 0.0}, 2, -webT / 2.0 - 3.0, u1 + 3.0, db);
        }
    } else {
        const double gw = val(c, "gaugeWeb", 40.0), gs = val(c, "gaugeSupport", 40.0), we = val(c, "webEnd", 35.0), se = val(c, "supEdge", 35.0), tp = val(c, "angleT", 9.5);
        const double lw = gw + we, ls = gs + se;
        for (const double side : {-1.0, 1.0}) {
            const double face = side * webT / 2.0;
            const double u0 = side > 0 ? face : face - tp, u1 = side > 0 ? face + tp : face;
            addPlate(out, 0.0, lw, height, u0, u1);                                                       // aba da alma
            out.push_back(box(Role::Plate, tp / 2.0, 0.0, face + side * (ls / 2.0 + tp / 2.0), tp, height, ls));   // aba do apoio
            for (int i = 0; i < rows; ++i) {
                const double t = (i - (rows - 1) / 2.0) * pitch;
                addBolt(out, {gw, t, 0.0}, 2, -webT / 2.0 - tp - 3.0, webT / 2.0 + tp + 3.0, db);
            }
        }
    }
}

void gusset(const NodeConnection &c, std::vector<Primitive> &out)
{
    const double db = val(c, "db", 20.0), pitch = val(c, "pitch", 70.0), gauge = val(c, "gauge", 60.0), gT = val(c, "gT", 9.5);
    const int rows = count(c, "rows", 3.0), lines = count(c, "lines", 1.0);
    const double freeL = val(c, "freeL", 80.0), endG = val(c, "endG", 35.0), edgeG = val(c, "edgeG", 40.0), width = val(c, "gWidth", 200.0);
    const double tip = std::min((lines - 1) * gauge + 2.0 * edgeG, width);
    const double length = freeL + (rows - 1) * pitch + endG;
    Primitive p;
    p.kind = Kind::Prism;
    p.role = Role::Plate;
    p.polygon = {{0.0, -width / 2.0}, {length, -tip / 2.0}, {length, tip / 2.0}, {0.0, width / 2.0}};
    p.u0 = -gT / 2.0;
    p.u1 = gT / 2.0;
    out.push_back(std::move(p));
    const double supT = val(c, "supT", 12.0), weld = val(c, "weldLeg", 6.0);
    out.push_back(box(Role::Concrete, -supT / 2.0, 0.0, 0.0, supT, width + 80.0, 3.0 * gT + 60.0));
    out.push_back(box(Role::Weld, weld / 2.0, 0.0, gT / 2.0 + weld / 2.0, weld, width, weld));
    out.push_back(box(Role::Weld, weld / 2.0, 0.0, -gT / 2.0 - weld / 2.0, weld, width, weld));
    const double legT = std::max(val(c, "legT", 6.0), 3.0);
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < lines; ++j) {
            addBolt(out, {freeL + i * pitch, (j - (lines - 1) / 2.0) * gauge, 0.0}, 2, -gT / 2.0 - legT - 3.0, gT / 2.0 + legT + 3.0, db);
        }
    }
}

void splice(const NodeConnection &c, std::vector<Primitive> &out)
{
    const double db = val(c, "db", 20.0), d = val(c, "beamD", 300.0), tfl = val(c, "tfl", 10.0), tw = val(c, "tw", 6.0), gap = val(c, "gap", 10.0);
    const int fRows = count(c, "fRows", 3.0), fLines = count(c, "fLines", 2.0);
    const double fPitch = val(c, "fPitch", 70.0), fGauge = val(c, "fGauge", 80.0), fEnd = val(c, "fEnd", 40.0), opW = val(c, "opW", 150.0), opT = val(c, "opT", 12.5);
    const double ipT = val(c, "ipT", 0.0), ipW = val(c, "ipW", 40.0);
    const double halfFlange = gap / 2.0 + fEnd + (fRows - 1) * fPitch + fEnd;
    for (const double top : {-1.0, 1.0}) {
        out.push_back(box(Role::Plate, 0.0, top * (d / 2.0 + opT / 2.0), 0.0, 2.0 * halfFlange, opT, opW));
        if (ipT > 0.0) {
            for (const double side : {-1.0, 1.0}) {
                out.push_back(box(Role::Plate, 0.0, top * (d / 2.0 - tfl - ipT / 2.0), side * (tw / 2.0 + ipW / 2.0), 2.0 * halfFlange, ipT, ipW));
            }
        }
        const double tIn = top * (d / 2.0 - tfl - ipT), tOut = top * (d / 2.0 + opT);
        for (const double end : {-1.0, 1.0}) {
            for (int i = 0; i < fRows; ++i) {
                for (int j = 0; j < fLines; ++j) {
                    addBolt(out, {end * (gap / 2.0 + fEnd + i * fPitch), 0.0, (j - (fLines - 1) / 2.0) * fGauge}, 1, std::min(tIn, tOut) - 2.0, std::max(tIn, tOut) + 2.0, db);
                }
            }
        }
    }
    const int wRows = count(c, "wRows", 3.0), wCols = count(c, "wCols", 1.0);
    const double wPitch = val(c, "wPitch", 70.0), wGauge = val(c, "wGauge", 60.0), wEdgeH = val(c, "wEdgeH", 40.0), wEdgeV = val(c, "wEdgeV", 35.0), wpT = val(c, "wpT", 6.3);
    const double wHeight = (wRows - 1) * wPitch + 2.0 * wEdgeV;
    const double wHalf = gap / 2.0 + wEdgeH + (wCols - 1) * wGauge + wEdgeH;
    for (const double side : {-1.0, 1.0}) {
        out.push_back(box(Role::Plate, 0.0, 0.0, side * (tw / 2.0 + wpT / 2.0), 2.0 * wHalf, wHeight, wpT));
    }
    for (const double end : {-1.0, 1.0}) {
        for (int i = 0; i < wRows; ++i) {
            for (int j = 0; j < wCols; ++j) {
                addBolt(out, {end * (gap / 2.0 + wEdgeH + j * wGauge), (i - (wRows - 1) / 2.0) * wPitch, 0.0}, 2, -tw / 2.0 - wpT - 2.0, tw / 2.0 + wpT + 2.0, db);
            }
        }
    }
}

void basePlate(const NodeConnection &c, std::vector<Primitive> &out)
{
    namespace D = Structura::Design::Connections;
    D::BasePlateInput in;
    in.d = val(c, "colD", 300.0);
    in.bf = val(c, "colBf", 150.0);
    in.tw = val(c, "colTw", 6.0);
    in.tf = val(c, "colTf", 10.0);
    in.anchorDiameter = val(c, "anchorD", 25.0);
    in.anchors = std::clamp(static_cast<int>(std::lround(val(c, "anchors", 4.0))), 4, 8);
    in.plateThickness = val(c, "plateT", 25.0);
    in.blockN = val(c, "blockN", 0.0);
    in.blockB = val(c, "blockB", 0.0);
    const auto g = D::basePlateGeometry(in);
    const double lx = g.lx > 0.0 ? g.lx : in.d + 120.0, ly = g.ly > 0.0 ? g.ly : in.bf + 120.0, tp = in.plateThickness;
    const auto &anchor = D::anchorFor(in.anchorDiameter);
    const double grout = 30.0;
    out.push_back(box(Role::Plate, -tp / 2.0, 0.0, 0.0, tp, lx, ly));
    const double blockN = g.blockN > 0.0 ? g.blockN : lx + 200.0, blockB = g.blockB > 0.0 ? g.blockB : ly + 200.0;
    const double depth = std::max(anchor.h1 + 100.0, 400.0);
    out.push_back(box(Role::Concrete, -tp - grout - depth / 2.0, 0.0, 0.0, depth, blockN, blockB));
    out.push_back(box(Role::Concrete, -tp - grout / 2.0, 0.0, 0.0, grout, lx, ly));
    const double a1 = anchor.a1 > 0.0 ? anchor.a1 : 40.0;
    const double along = lx / 2.0 - a1, across = ly / 2.0 - a1;
    std::vector<std::array<double, 2>> positions = {{along, across}, {along, -across}, {-along, across}, {-along, -across}};
    if (in.anchors >= 6) {
        positions.push_back({0.0, across});
        positions.push_back({0.0, -across});
    }
    if (in.anchors >= 8) {
        positions.push_back({along, 0.0});
        positions.push_back({-along, 0.0});
    }
    positions.resize(static_cast<std::size_t>(in.anchors));
    for (const auto &p : positions) {
        out.push_back(cylinder(Role::Bolt, {-tp - grout - std::max(anchor.h1, 150.0), p[0], p[1]}, {40.0, p[0], p[1]}, in.anchorDiameter / 2.0));
        out.push_back(box(Role::Bolt, 12.0, p[0], p[1], 0.8 * in.anchorDiameter, 1.7 * in.anchorDiameter, 1.7 * in.anchorDiameter));
    }
    const double weld = val(c, "weldLeg", 6.0);
    out.push_back(box(Role::Weld, weld / 2.0, 0.0, in.bf / 2.0 + weld / 2.0, weld, in.d, weld));
    out.push_back(box(Role::Weld, weld / 2.0, 0.0, -in.bf / 2.0 - weld / 2.0, weld, in.d, weld));
}

} // namespace

namespace {

/// Perfil I deitado ao longo de s (comprimento de s0 a s1), altura em t e largura de mesa em u.
void addIBeam(std::vector<Primitive> &out, double s0, double s1, double depth, double bf, double tf, double tw)
{
    const double len = s1 - s0, mid = (s0 + s1) / 2.0;
    for (const double side : {-1.0, 1.0}) {
        out.push_back(box(Role::Member, mid, side * (depth - tf) / 2.0, 0.0, len, tf, bf));
    }
    out.push_back(box(Role::Member, mid, 0.0, 0.0, len, depth - 2.0 * tf, tw));
}

/// Pilar I ao longo de s, com a alma paralela a t (mesas nas faces u = +-bf/2... aqui: altura d em t, mesa bf em u).
void addContext(const NodeConnection &c, std::vector<Primitive> &out)
{
    switch (c.type) {
    case NodeConnection::kShearTab:
    case NodeConnection::kDoubleAngle: {
        const double height = (count(c, "rows", 3.0) - 1) * val(c, "pitch", 70.0) + 2.0 * val(c, "edgeV", 35.0);
        const double depth = val(c, "beamDepth", 0.0) > 0.0 ? val(c, "beamDepth") : height + 90.0;
        const double setback = c.type == NodeConnection::kShearTab ? val(c, "setback", 12.0) : 10.0;
        addIBeam(out, setback, setback + 520.0, depth, depth * 0.5, std::max(8.0, depth / 28.0), val(c, "webT", 6.0));
        break;
    }
    case NodeConnection::kGusset: {
        const double legT = std::max(val(c, "legT", 6.0), 3.0), gT = val(c, "gT", 9.5);
        const int rows = count(c, "rows", 3.0), lines = count(c, "lines", 1.0);
        const double length = val(c, "freeL", 80.0) + (rows - 1) * val(c, "pitch", 70.0) + val(c, "endG", 35.0);
        const double width = (lines - 1) * val(c, "gauge", 60.0) + 2.0 * val(c, "edgeLeg", 30.0);
        const double s0 = val(c, "freeL", 80.0) - val(c, "endLeg", 35.0);
        for (const double side : {-1.0, 1.0}) {
            out.push_back(box(Role::Member, (s0 + length + 200.0) / 2.0, 0.0, side * (gT / 2.0 + legT / 2.0), length + 200.0 - s0, width, legT));
        }
        break;
    }
    case NodeConnection::kSplice: {
        const double d = val(c, "beamD", 300.0), gap = val(c, "gap", 10.0);
        for (const double end : {-1.0, 1.0}) {
            addIBeam(out, end > 0 ? gap / 2.0 : -gap / 2.0 - 450.0, end > 0 ? gap / 2.0 + 450.0 : -gap / 2.0, d, val(c, "bfl", 150.0), val(c, "tfl", 10.0), val(c, "tw", 6.0));
        }
        break;
    }
    case NodeConnection::kBasePlate: {
        const double d = val(c, "colD", 300.0), bf = val(c, "colBf", 150.0), tf = val(c, "colTf", 10.0), tw = val(c, "colTw", 6.0);
        const double len = std::max(500.0, 1.5 * d);
        for (const double side : {-1.0, 1.0}) {
            out.push_back(box(Role::Member, len / 2.0, side * (d - tf) / 2.0, 0.0, len, tf, bf));
        }
        out.push_back(box(Role::Member, len / 2.0, 0.0, 0.0, len, d - 2.0 * tf, tw));
        break;
    }
    default: {
        const double d0 = val(c, "d0", 100.0), reach = std::max(4.0 * d0, 500.0);
        out.push_back(cylinder(Role::Member, {-reach, 0.0, 0.0}, {reach, 0.0, 0.0}, d0 / 2.0));
        const double gap = val(c, "gap", 20.0);
        const int kind = static_cast<int>(std::lround(val(c, "kindT", 0.0)));
        for (int n = 0; n < 2; ++n) {
            const double d = val(c, n == 0 ? "d1" : "d2", 60.0), th = val(c, n == 0 ? "th1" : "th2", 45.0) * kPi / 180.0;
            if (n == 1 && kind == 0) {
                continue;
            }
            const double sign = n == 0 ? (kind >= 2 ? -1.0 : 1.0) : 1.0;
            const double foot = kind == 2 ? sign * (gap / 2.0 + d / 2.0) : (kind == 3 ? sign * d * 0.25 : 0.0);
            const double dirS = std::cos(th) * sign, dirT = std::sin(th);
            out.push_back(cylinder(Role::Member, {foot, d0 / 2.0, 0.0}, {foot + dirS * reach, d0 / 2.0 + dirT * reach, 0.0}, d / 2.0));
            if (kind == 1) {
                out.push_back(cylinder(Role::Member, {0.0, -d0 / 2.0, 0.0}, {-dirS * reach, -d0 / 2.0 - dirT * reach, 0.0}, d / 2.0));
            }
        }
        break;
    }
    }
}

} // namespace

std::vector<Primitive> ConnectionSolids::build(const NodeConnection &c, bool context)
{
    std::vector<Primitive> out;
    if (context) {
        addContext(c, out);
    }
    switch (c.type) {
    case NodeConnection::kShearTab: shearTab(c, out, false); break;
    case NodeConnection::kDoubleAngle: shearTab(c, out, true); break;
    case NodeConnection::kGusset: gusset(c, out); break;
    case NodeConnection::kSplice: splice(c, out); break;
    case NodeConnection::kBasePlate: basePlate(c, out); break;
    default: {
        // ligacao tubular soldada: disco-marcador em torno do banzo
        if (!context) {
            const double d0 = val(c, "d0", 100.0);
            out.push_back(cylinder(Role::Marker, {-15.0, 0.0, 0.0}, {15.0, 0.0, 0.0}, d0 / 2.0 + 8.0));
        }
        break;
    }
    }
    return out;
}

std::array<unsigned char, 3> ConnectionSolids::color(Role role, double eta)
{
    switch (role) {
    case Role::Bolt: return {{215, 150, 50}};
    case Role::Weld: return {{200, 45, 45}};
    case Role::Concrete: return {{165, 168, 172}};
    case Role::Member: return {{150, 160, 176}};
    case Role::Plate:
    case Role::Marker:
        if (eta > 0.0) {
            if (eta <= 0.7) {
                return {{60, 170, 90}};
            }
            return eta <= 1.0 ? std::array<unsigned char, 3> {{230, 180, 40}} : std::array<unsigned char, 3> {{205, 50, 50}};
        }
        return {{70, 120, 190}};
    }
    return {{128, 128, 128}};
}

namespace {

using V3 = std::array<double, 3>;

struct Emitter {
    ConnectionSolids::Mesh &mesh;
    const ConnectionSolids::Frame &f;
    std::array<unsigned char, 3> color;

    V3 world(const V3 &p) const
    {
        V3 r;
        for (std::size_t i = 0; i < 3; ++i) {
            r[i] = f.origin[i] + f.scale * (p[0] * f.s[i] + p[1] * f.t[i] + p[2] * f.u[i]);
        }
        return r;
    }
    V3 direction(const V3 &n) const
    {
        V3 r;
        for (std::size_t i = 0; i < 3; ++i) {
            r[i] = n[0] * f.s[i] + n[1] * f.t[i] + n[2] * f.u[i];
        }
        return r;
    }
    int vertex(const V3 &p, const V3 &n)
    {
        const V3 w = world(p), nw = direction(n);
        mesh.points.push_back({static_cast<float>(w[0]), static_cast<float>(w[1]), static_cast<float>(w[2])});
        mesh.normals.push_back({static_cast<float>(nw[0]), static_cast<float>(nw[1]), static_cast<float>(nw[2])});
        mesh.colors.push_back(color);
        return static_cast<int>(mesh.points.size()) - 1;
    }
    void quad(const V3 &p0, const V3 &p1, const V3 &p2, const V3 &p3, const V3 &n)
    {
        const int a = vertex(p0, n), b = vertex(p1, n), c = vertex(p2, n), d = vertex(p3, n);
        mesh.triangles.push_back({a, b, c});
        mesh.triangles.push_back({a, c, d});
    }
};

void emitBox(Emitter &e, const V3 &c, const V3 &size)
{
    const double hx = size[0] / 2.0, hy = size[1] / 2.0, hz = size[2] / 2.0;
    auto P = [&](double sx, double sy, double sz) { return V3 {c[0] + sx * hx, c[1] + sy * hy, c[2] + sz * hz}; };
    e.quad(P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1), {1, 0, 0});
    e.quad(P(-1, -1, 1), P(-1, 1, 1), P(-1, 1, -1), P(-1, -1, -1), {-1, 0, 0});
    e.quad(P(-1, 1, -1), P(-1, 1, 1), P(1, 1, 1), P(1, 1, -1), {0, 1, 0});
    e.quad(P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), P(-1, -1, -1), {0, -1, 0});
    e.quad(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), {0, 0, 1});
    e.quad(P(-1, 1, -1), P(1, 1, -1), P(1, -1, -1), P(-1, -1, -1), {0, 0, -1});
}

void emitCylinder(Emitter &e, const V3 &a, const V3 &b, double radius)
{
    V3 axis {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const double len = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    if (len < 1e-9 || radius <= 0.0) {
        return;
    }
    for (double &v : axis) {
        v /= len;
    }
    const V3 ref = std::abs(axis[0]) < 0.9 ? V3 {1, 0, 0} : V3 {0, 1, 0};
    V3 p1 {axis[1] * ref[2] - axis[2] * ref[1], axis[2] * ref[0] - axis[0] * ref[2], axis[0] * ref[1] - axis[1] * ref[0]};
    const double l1 = std::sqrt(p1[0] * p1[0] + p1[1] * p1[1] + p1[2] * p1[2]);
    for (double &v : p1) {
        v /= l1;
    }
    const V3 p2 {axis[1] * p1[2] - axis[2] * p1[1], axis[2] * p1[0] - axis[0] * p1[2], axis[0] * p1[1] - axis[1] * p1[0]};
    constexpr int kSides = 14;
    auto radial = [&](int i) {
        const double ang = 2.0 * kPi * i / kSides;
        return V3 {std::cos(ang) * p1[0] + std::sin(ang) * p2[0], std::cos(ang) * p1[1] + std::sin(ang) * p2[1], std::cos(ang) * p1[2] + std::sin(ang) * p2[2]};
    };
    auto offset = [&](const V3 &base, const V3 &n) { return V3 {base[0] + radius * n[0], base[1] + radius * n[1], base[2] + radius * n[2]}; };
    std::vector<int> bottom, top;
    for (int i = 0; i < kSides; ++i) {
        const V3 n = radial(i);
        bottom.push_back(e.vertex(offset(a, n), n));
        top.push_back(e.vertex(offset(b, n), n));
    }
    for (int i = 0; i < kSides; ++i) {
        const auto si = static_cast<std::size_t>(i), sj = static_cast<std::size_t>((i + 1) % kSides);
        e.mesh.triangles.push_back({bottom[si], bottom[sj], top[sj]});
        e.mesh.triangles.push_back({bottom[si], top[sj], top[si]});
    }
    for (int cap = 0; cap < 2; ++cap) {
        const V3 &center = cap == 0 ? a : b;
        const V3 n = cap == 0 ? V3 {-axis[0], -axis[1], -axis[2]} : axis;
        const int hub = e.vertex(center, n);
        std::vector<int> ring;
        for (int i = 0; i < kSides; ++i) {
            ring.push_back(e.vertex(offset(center, radial(i)), n));
        }
        for (int i = 0; i < kSides; ++i) {
            e.mesh.triangles.push_back({hub, ring[static_cast<std::size_t>(i)], ring[static_cast<std::size_t>((i + 1) % kSides)]});
        }
    }
}

void emitPrism(Emitter &e, const std::vector<std::array<double, 2>> &poly, double u0, double u1)
{
    const std::size_t n = poly.size();
    if (n < 3) {
        return;
    }
    for (const double u : {u0, u1}) {
        const V3 normal {0, 0, u == u1 ? 1.0 : -1.0};
        std::vector<int> ids;
        for (const auto &p : poly) {
            ids.push_back(e.vertex({p[0], p[1], u}, normal));
        }
        for (std::size_t i = 1; i + 1 < n; ++i) {
            e.mesh.triangles.push_back({ids[0], ids[i], ids[i + 1]});
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        const auto &p = poly[i];
        const auto &q = poly[(i + 1) % n];
        const double nx = q[1] - p[1], ny = -(q[0] - p[0]);
        const double len = std::hypot(nx, ny);
        if (len < 1e-9) {
            continue;
        }
        e.quad({p[0], p[1], u0}, {q[0], q[1], u0}, {q[0], q[1], u1}, {p[0], p[1], u1}, {nx / len, ny / len, 0.0});
    }
}

} // namespace

void ConnectionSolids::append(Mesh &mesh, const std::vector<Primitive> &primitives, const Frame &frame, double eta)
{
    for (const auto &p : primitives) {
        Emitter e {mesh, frame, color(p.role, eta)};
        switch (p.kind) {
        case Kind::Box: emitBox(e, p.a, p.size); break;
        case Kind::Cylinder: emitCylinder(e, p.a, p.b, p.radius); break;
        case Kind::Prism: emitPrism(e, p.polygon, p.u0, p.u1); break;
        }
    }
}

} // namespace Structura::App::Services
