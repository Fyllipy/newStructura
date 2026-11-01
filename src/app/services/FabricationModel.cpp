#include "app/services/FabricationModel.h"

#include "app/services/ConnectionSolids.h"
#include "design/Connections.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace Structura::App::Services {

namespace {

constexpr double kSteelDensity = 7850.0e-9;   // kg/mm3
constexpr double kPi = 3.14159265358979323846;

using Primitive = ConnectionSolids::Primitive;
using Kind = ConnectionSolids::Kind;
using Role = ConnectionSolids::Role;

struct RawBolt {
    std::array<double, 3> center {};
    int axis {2};
    double lo {0.0}, hi {0.0};
    double diameter {0.0};
};

bool inside(const std::vector<std::array<double, 2>> &polygon, double x, double y)
{
    bool in = false;
    const std::size_t n = polygon.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const auto &p = polygon[i];
        const auto &q = polygon[j];
        if (((p[1] > y) != (q[1] > y)) && (x < (q[0] - p[0]) * (y - p[1]) / (q[1] - p[1]) + p[0])) {
            in = !in;
        }
    }
    return in;
}

double polygonArea(const std::vector<std::array<double, 2>> &poly)
{
    double sum = 0.0;
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const auto &p = poly[i];
        const auto &q = poly[(i + 1) % poly.size()];
        sum += p[0] * q[1] - q[0] * p[1];
    }
    return std::abs(sum) / 2.0;
}

/// Parafusos: cada um e uma sequencia cabeca (raio 0,8 d), corpo (raio 0,5 d) e porca (raio 0,8 d) com o mesmo eixo.
std::vector<RawBolt> collectBolts(const std::vector<Primitive> &prims)
{
    std::vector<RawBolt> bolts;
    for (std::size_t i = 0; i < prims.size(); ++i) {
        const Primitive &p = prims[i];
        if (p.role != Role::Bolt || p.kind != Kind::Cylinder) {
            continue;
        }
        // o corpo e o cilindro cujo raio e 0,5 d com cabeca e porca vizinhas de raio 0,8 d (= 1,6 x o raio do corpo)
        const bool hasNeighbours = i > 0 && i + 1 < prims.size() && prims[i - 1].role == Role::Bolt && prims[i + 1].role == Role::Bolt;
        if (!hasNeighbours || std::abs(prims[i - 1].radius - 1.6 * p.radius) > 1e-6 || std::abs(prims[i + 1].radius - 1.6 * p.radius) > 1e-6) {
            continue;
        }
        RawBolt b;
        int axis = 0;
        double span = 0.0;
        for (int k = 0; k < 3; ++k) {
            const double d = std::abs(p.b[static_cast<std::size_t>(k)] - p.a[static_cast<std::size_t>(k)]);
            if (d > span) {
                span = d;
                axis = k;
            }
        }
        b.axis = axis;
        b.center = p.a;
        b.lo = std::min(p.a[static_cast<std::size_t>(axis)], p.b[static_cast<std::size_t>(axis)]);
        b.hi = std::max(p.a[static_cast<std::size_t>(axis)], p.b[static_cast<std::size_t>(axis)]);
        b.diameter = 2.0 * p.radius;
        bolts.push_back(b);
    }
    return bolts;
}

FabPart makePart(const Primitive &p)
{
    FabPart part;
    std::vector<std::array<double, 2>> poly;
    if (p.kind == Kind::Prism) {
        part.thicknessAxis = 2;
        part.planeAxes = {0, 1};
        poly = p.polygon;
        part.thickness = std::abs(p.u1 - p.u0);
        part.midOffset = (p.u0 + p.u1) / 2.0;
    } else {
        int thin = 0;
        for (int k = 1; k < 3; ++k) {
            if (p.size[static_cast<std::size_t>(k)] < p.size[static_cast<std::size_t>(thin)]) {
                thin = k;
            }
        }
        part.thicknessAxis = thin;
        int idx = 0;
        for (int k = 0; k < 3; ++k) {
            if (k != thin) {
                part.planeAxes[static_cast<std::size_t>(idx++)] = k;
            }
        }
        const int ax = part.planeAxes[0], bx = part.planeAxes[1];
        const double ca = p.a[static_cast<std::size_t>(ax)], cb = p.a[static_cast<std::size_t>(bx)];
        const double ha = p.size[static_cast<std::size_t>(ax)] / 2.0, hb = p.size[static_cast<std::size_t>(bx)] / 2.0;
        poly = {{ca - ha, cb - hb}, {ca + ha, cb - hb}, {ca + ha, cb + hb}, {ca - ha, cb + hb}};
        part.thickness = p.size[static_cast<std::size_t>(thin)];
        part.midOffset = p.a[static_cast<std::size_t>(thin)];
    }
    double minA = 1e300, minB = 1e300, maxA = -1e300, maxB = -1e300;
    for (const auto &v : poly) {
        minA = std::min(minA, v[0]);
        minB = std::min(minB, v[1]);
        maxA = std::max(maxA, v[0]);
        maxB = std::max(maxB, v[1]);
    }
    part.origin = {minA, minB};
    part.width = maxA - minA;
    part.height = maxB - minB;
    for (const auto &v : poly) {
        part.outline.push_back({v[0] - minA, v[1] - minB});
    }
    part.grossArea = polygonArea(part.outline);
    part.rectangular = part.outline.size() == 4 && std::abs(part.grossArea - part.width * part.height) < 1e-6 * std::max(1.0, part.width * part.height);
    return part;
}

QString fmt(double v)
{
    QString s = QString::number(v, 'f', std::abs(v - std::round(v)) < 1e-6 ? 0 : 1);
    return s.replace(QLatin1Char('.'), QLatin1Char(','));
}

std::string holeKey(const FabPart &p)
{
    std::vector<std::tuple<long, long, long>> holes;
    for (const auto &h : p.holes) {
        holes.emplace_back(std::lround(h.a * 10.0), std::lround(h.b * 10.0), std::lround(h.diameter * 10.0));
    }
    std::sort(holes.begin(), holes.end());
    std::string key = std::to_string(std::lround(p.thickness * 100.0)) + "|";
    for (const auto &v : p.outline) {
        key += std::to_string(std::lround(v[0] * 10.0)) + "," + std::to_string(std::lround(v[1] * 10.0)) + ";";
    }
    key += "|";
    for (const auto &h : holes) {
        key += std::to_string(std::get<0>(h)) + "," + std::to_string(std::get<1>(h)) + "," + std::to_string(std::get<2>(h)) + ";";
    }
    return key;
}

} // namespace

std::array<double, 3> FabricationModel::centre(const FabPart &part)
{
    std::array<double, 3> c {};
    c[static_cast<std::size_t>(part.thicknessAxis)] = part.midOffset;
    c[static_cast<std::size_t>(part.planeAxes[0])] = part.origin[0] + part.width / 2.0;
    c[static_cast<std::size_t>(part.planeAxes[1])] = part.origin[1] + part.height / 2.0;
    return c;
}

double FabricationModel::holeDiameter(double bolt)
{
    return Structura::Design::Connections::standardHoleDiameter(bolt);
}

double FabricationModel::boltMass(double diameter, double length)
{
    // corpo + cabeca/porca (0,8 d de altura, 1,6 d de diametro medio) + duas arruelas (0,15 d x 3 d de diametro)
    const double shank = kPi / 4.0 * diameter * diameter * length;
    const double head = kPi / 4.0 * (1.6 * diameter) * (1.6 * diameter) * 0.65 * diameter;
    const double nut = kPi / 4.0 * (1.6 * diameter) * (1.6 * diameter) * 0.8 * diameter;
    const double washers = 2.0 * kPi / 4.0 * (3.0 * diameter) * (3.0 * diameter) * 0.15 * diameter;
    return (shank + head + nut + washers) * kSteelDensity;
}

int FabAssembly::plateCount() const
{
    int total = 0;
    for (const auto &p : parts) {
        total += p.kind == FabPart::Kind::Plate ? p.quantity : 0;
    }
    return total;
}

FabAssembly FabricationModel::fromConnection(const Structura::App::ProjectData::NodeConnection &connection)
{
    FabAssembly out;
    const auto prims = ConnectionSolids::build(connection, true);
    const auto bolts = collectBolts(prims);
    const QString material = connection.values.contains(QStringLiteral("plateFy")) ? QStringLiteral("fy %1 MPa").arg(fmt(connection.values.value(QStringLiteral("plateFy")))) : QString();

    // pecas planas (chapas e mesas/almas dos perfis ligados) com os furos dos parafusos que as atravessam
    std::vector<FabPart> plates, memberParts;
    for (const Primitive &p : prims) {
        if ((p.role != Role::Plate && p.role != Role::Member) || p.kind == Kind::Cylinder) {
            continue;
        }
        FabPart part = makePart(p);
        part.kind = p.role == Role::Plate ? FabPart::Kind::Plate : FabPart::Kind::MemberPlate;
        part.material = part.kind == FabPart::Kind::Plate ? material : QString();
        const double lo = part.midOffset - part.thickness / 2.0, hi = part.midOffset + part.thickness / 2.0;
        std::vector<std::array<double, 2>> world;
        for (const auto &v : part.outline) {
            world.push_back({v[0] + part.origin[0], v[1] + part.origin[1]});
        }
        double holeArea = 0.0;
        for (const RawBolt &b : bolts) {
            if (b.axis != part.thicknessAxis || b.hi < lo + 1e-6 || b.lo > hi - 1e-6) {
                continue;
            }
            const double pa = b.center[static_cast<std::size_t>(part.planeAxes[0])], pb = b.center[static_cast<std::size_t>(part.planeAxes[1])];
            if (!inside(world, pa, pb)) {
                continue;
            }
            const double dh = holeDiameter(b.diameter);
            part.holes.push_back({pa - part.origin[0], pb - part.origin[1], dh});
            holeArea += kPi / 4.0 * dh * dh;
        }
        part.massKg = (part.grossArea - holeArea) * part.thickness * kSteelDensity;
        if (part.kind == FabPart::Kind::Plate) {
            const QString dims = QStringLiteral("PL %1 x %2 x %3").arg(fmt(part.thickness), fmt(std::min(part.width, part.height)), fmt(std::max(part.width, part.height)));
            part.name = part.rectangular ? dims : dims + QStringLiteral(" (contorno poligonal)");
            plates.push_back(std::move(part));
        } else {
            if (part.holes.empty()) {
                continue;   // so interessam as pecas do perfil que recebem furos
            }
            part.name = QStringLiteral("Perfil ligado: placa %1 x %2 x %3 mm").arg(fmt(part.thickness), fmt(part.width), fmt(part.height));
            memberParts.push_back(std::move(part));
        }
    }

    // chapas soldadas ao perfil: soldas que encostam ao mesmo tempo na chapa e em uma peca do perfil
    {
        struct Box {
            std::array<double, 3> lo {}, hi {};
        };
        auto boxOf = [](const Primitive &p, Box &b) {
            if (p.kind == Kind::Box) {
                for (int k = 0; k < 3; ++k) {
                    b.lo[static_cast<std::size_t>(k)] = p.a[static_cast<std::size_t>(k)] - p.size[static_cast<std::size_t>(k)] / 2.0;
                    b.hi[static_cast<std::size_t>(k)] = p.a[static_cast<std::size_t>(k)] + p.size[static_cast<std::size_t>(k)] / 2.0;
                }
                return true;
            }
            if (p.kind == Kind::Prism && !p.polygon.empty()) {
                b.lo = {1e300, 1e300, std::min(p.u0, p.u1)};
                b.hi = {-1e300, -1e300, std::max(p.u0, p.u1)};
                for (const auto &v : p.polygon) {
                    b.lo[0] = std::min(b.lo[0], v[0]);
                    b.lo[1] = std::min(b.lo[1], v[1]);
                    b.hi[0] = std::max(b.hi[0], v[0]);
                    b.hi[1] = std::max(b.hi[1], v[1]);
                }
                return true;
            }
            return false;
        };
        auto touches = [](const Box &a, const Box &b) {
            for (int k = 0; k < 3; ++k) {
                const auto i = static_cast<std::size_t>(k);
                if (a.hi[i] < b.lo[i] - 0.6 || b.hi[i] < a.lo[i] - 0.6) {
                    return false;
                }
            }
            return true;
        };
        std::vector<Box> memberBoxes, weldBoxes;
        for (const Primitive &p : prims) {
            Box b;
            if (p.role == Role::Member && boxOf(p, b)) {
                memberBoxes.push_back(b);
            } else if (p.role == Role::Weld && boxOf(p, b)) {
                weldBoxes.push_back(b);
            }
        }
        std::size_t plateIndexInPrims = 0;
        for (const Primitive &p : prims) {
            if (p.role != Role::Plate || p.kind == Kind::Cylinder) {
                continue;
            }
            Box plateBox;
            if (boxOf(p, plateBox) && plateIndexInPrims < plates.size()) {
                bool welded = false;
                for (const Box &w : weldBoxes) {
                    if (!touches(plateBox, w)) {
                        continue;
                    }
                    for (const Box &m : memberBoxes) {
                        welded = welded || touches(w, m);
                    }
                }
                plates[plateIndexInPrims].weldedToMember = welded;
            }
            ++plateIndexInPrims;
        }
    }

    // recortes das mesas do perfil: mesa (placa fina em t) que comeca depois da extremidade da alma
    {
        double gap = 1e300;
        for (const Primitive &p : prims) {
            if (p.role == Role::Member && p.kind == Kind::Box && p.a[0] - p.size[0] / 2.0 > -1e-6) {
                gap = std::min(gap, p.a[0] - p.size[0] / 2.0);
            }
        }
        const double clearance = connection.values.value(QStringLiteral("copeC"), 0.0);
        for (const Primitive &p : prims) {
            if (p.role != Role::Member || p.kind != Kind::Box || p.size[1] >= p.size[0] || p.size[1] >= p.size[2]) {
                continue;   // so as mesas (finas em t)
            }
            const double start = p.a[0] - p.size[0] / 2.0;
            if (start < -1e-6 || gap > 1e299 || start <= gap + 0.5) {
                continue;
            }
            FabCope cope;
            cope.top = p.a[1] > 0.0;
            cope.length = start - gap;
            cope.depth = p.size[1] + clearance;
            out.copes.push_back(cope);
        }
    }
    out.endAngleDeg = connection.values.value(QStringLiteral("skew"), 0.0);

    // marcas: chapas identicas (espessura, contorno e furacao) compartilham a marca
    std::map<std::string, int> indexOfKey;
    for (FabPart &p : plates) {
        const std::string key = holeKey(p) + (p.weldedToMember ? "|m" : "|s");
        const auto it = indexOfKey.find(key);
        if (it != indexOfKey.end()) {
            ++out.parts[it->second].quantity;
            FabPart copy = p;
            copy.mark = out.parts[it->second].mark;
            out.plateInstances.push_back(copy);
        } else {
            p.mark = QStringLiteral("C%1").arg(out.parts.size() + 1);
            indexOfKey[key] = out.parts.size();
            out.parts.push_back(p);
            out.plateInstances.push_back(p);
        }
    }
    for (const FabPart &p : out.parts) {
        out.plateMassKg += p.massKg * p.quantity;
    }
    for (FabPart &p : memberParts) {
        out.parts.push_back(std::move(p));
    }

    // parafusos agrupados por diametro e comprimento
    std::map<std::pair<long, long>, int> boltGroups;
    std::map<std::pair<long, long>, RawBolt> boltSample;
    std::map<std::pair<long, long>, std::vector<std::array<double, 3>>> boltPositions;
    for (const RawBolt &b : bolts) {
        const double grip = b.hi - b.lo;
        const std::pair<long, long> key {std::lround(b.diameter * 10.0), std::lround(grip * 10.0)};
        boltGroups[key] += 1;
        boltSample.emplace(key, b);
        std::array<double, 3> centre = b.center;
        centre[static_cast<std::size_t>(b.axis)] = (b.lo + b.hi) / 2.0;
        boltPositions[key].push_back(centre);
    }
    int boltIndex = 0;
    for (const auto &kv : boltGroups) {
        FabBolt fb;
        fb.mark = QStringLiteral("B%1").arg(++boltIndex);
        fb.diameter = kv.first.first / 10.0;
        fb.gripLength = kv.first.second / 10.0;
        fb.length = std::ceil((fb.gripLength + 0.8 * fb.diameter + 2.0 * 0.15 * fb.diameter + 3.0 * 2.5) / 5.0) * 5.0;
        fb.grade = connection.grade;
        fb.quantity = kv.second;
        const RawBolt &sample = boltSample[kv.first];
        fb.position = sample.center;
        fb.position[static_cast<std::size_t>(sample.axis)] = (sample.lo + sample.hi) / 2.0;
        fb.axis = sample.axis;
        fb.positions = boltPositions[kv.first];
        out.boltMassKg += boltMass(fb.diameter, fb.length) * fb.quantity;
        out.bolts.push_back(fb);
    }

    // soldas: caixas do papel Weld (perna = menor lado, comprimento = maior), agrupadas
    std::map<std::pair<long, long>, int> weldGroups;
    std::map<std::pair<long, long>, std::array<double, 3>> weldSample;
    for (const Primitive &p : prims) {
        if (p.role != Role::Weld || p.kind != Kind::Box) {
            continue;
        }
        double smallest = 1e300, largest = 0.0;
        for (const double v : p.size) {
            smallest = std::min(smallest, v);
            largest = std::max(largest, v);
        }
        const std::pair<long, long> weldKey {std::lround(smallest * 10.0), std::lround(largest * 10.0)};
        weldGroups[weldKey] += 1;
        weldSample.emplace(weldKey, p.a);
    }
    int weldIndex = 0;
    for (const auto &kv : weldGroups) {
        FabWeld w;
        w.mark = QStringLiteral("S%1").arg(++weldIndex);
        w.leg = kv.first.first / 10.0;
        w.length = kv.first.second / 10.0;
        w.quantity = kv.second;
        w.totalLength = w.length * w.quantity;
        w.position = weldSample[kv.first];
        out.welds.push_back(w);
    }
    return out;
}

FabBom FabricationModel::bom(const QVector<Structura::App::ProjectData::NodeConnection> &connections)
{
    FabBom out;
    out.connectionCount = connections.size();
    std::map<std::string, int> plateIndex;
    std::map<std::tuple<long, QString, long>, int> boltIndex;
    std::map<long, int> weldIndex;
    for (const auto &connection : connections) {
        const FabAssembly fab = fromConnection(connection);
        for (const FabPart &p : fab.parts) {
            if (p.kind != FabPart::Kind::Plate) {
                continue;
            }
            const std::string key = holeKey(p) + p.material.toStdString();
            auto it = plateIndex.find(key);
            if (it == plateIndex.end()) {
                FabBom::PlateRow row;
                row.name = p.name;
                row.material = p.material;
                row.thickness = p.thickness;
                row.width = p.width;
                row.height = p.height;
                row.holes = static_cast<int>(p.holes.size());
                row.unitMassKg = p.massKg;
                it = plateIndex.emplace(key, static_cast<int>(out.plates.size())).first;
                out.plates.push_back(row);
            }
            FabBom::PlateRow &row = out.plates[it->second];
            row.quantity += p.quantity;
            row.totalMassKg += p.massKg * p.quantity;
            if (!row.connections.contains(connection.id)) {
                row.connections.push_back(connection.id);
            }
            out.plateMassKg += p.massKg * p.quantity;
        }
        for (const FabBolt &b : fab.bolts) {
            const auto key = std::make_tuple(std::lround(b.diameter * 10.0), b.grade, std::lround(b.length * 10.0));
            auto it = boltIndex.find(key);
            if (it == boltIndex.end()) {
                FabBom::BoltRow row;
                row.diameter = b.diameter;
                row.length = b.length;
                row.grade = b.grade;
                it = boltIndex.emplace(key, static_cast<int>(out.bolts.size())).first;
                out.bolts.push_back(row);
            }
            out.bolts[it->second].quantity += b.quantity;
            const double mass = boltMass(b.diameter, b.length) * b.quantity;
            out.bolts[it->second].totalMassKg += mass;
            out.boltMassKg += mass;
        }
        for (const FabWeld &w : fab.welds) {
            const long key = std::lround(w.leg * 10.0);
            auto it = weldIndex.find(key);
            if (it == weldIndex.end()) {
                FabBom::WeldRow row;
                row.leg = w.leg;
                it = weldIndex.emplace(key, static_cast<int>(out.welds.size())).first;
                out.welds.push_back(row);
            }
            out.welds[it->second].totalLength += w.totalLength;
            out.welds[it->second].beads += w.quantity;
        }
    }
    std::sort(out.plates.begin(), out.plates.end(), [](const FabBom::PlateRow &a, const FabBom::PlateRow &b) {
        return std::tie(a.thickness, a.width, a.height) < std::tie(b.thickness, b.width, b.height);
    });
    return out;
}

} // namespace Structura::App::Services
