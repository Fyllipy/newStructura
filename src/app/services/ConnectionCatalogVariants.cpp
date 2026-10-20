#include "app/services/ConnectionCatalogInternal.h"

#include "design/ShearConnections.h"

#include <QObject>

#include <algorithm>
#include <cmath>

namespace Structura::App::Services::catalog {

namespace D = Structura::Design::Connections;
using Structura::App::ProjectData::NodeConnection;
using Cat = ConnectionCatalog;

// ---- nervuras das bases de pilar ---------------------------------------------------------------------------------------------------------------

QVector<Cat::FieldDef> baseRibFields()
{
    QVector<Cat::FieldDef> list;
    const QString group = QObject::tr("Nervuras de enrijecimento");
    const QString mm = QObject::tr(" mm");
    list.append({QStringLiteral("ribs"), QObject::tr("Número de nervuras"), 0, 12, 0, QString(), group, {}, QObject::tr("0 = sem nervuras; as nervuras reduzem o balanço da placa a metade.")});
    list.append({QStringLiteral("ribT"), QObject::tr("Espessura"), 4, 40, 1, mm, group, {}, QString()});
    list.append({QStringLiteral("ribH"), QObject::tr("Altura sobre a placa"), 30, 600, 0, mm, group, {}, QString()});
    list.append({QStringLiteral("ribL"), QObject::tr("Comprimento a partir da mesa do pilar"), 30, 600, 0, mm, group, {}, QString()});
    list.append({QStringLiteral("ribW"), QObject::tr("Perna do filete"), 3, 20, 1, mm, group, {}, QString()});
    return list;
}

void baseRibDefaults(NodeConnection &c, const Cat::BeamInfo &beam)
{
    const double h = std::clamp(std::round(0.6 * (beam.depth > 0.0 ? beam.depth : 300.0) / 10.0) * 10.0, 100.0, 400.0);
    c.values["ribs"] = c.values.value(QStringLiteral("ribs"), 4.0);
    c.values["ribT"] = 12.5;
    c.values["ribH"] = h;
    c.values["ribL"] = std::max(100.0, h * 0.7);
    c.values["ribW"] = 6.0;
}

void baseRibSolids(const NodeConnection &c, bool, std::vector<Cat::Primitive> &out)
{
    const int ribs = static_cast<int>(val(c, "ribs", 0.0) + 0.5);
    if (ribs <= 0) {
        return;
    }
    const double d = val(c, "colD", 300.0), bf = val(c, "colBf", 150.0), rt = val(c, "ribT", 12.5), rh = val(c, "ribH", 150.0), rl = val(c, "ribL", 100.0);
    const int perSide = std::max(1, ribs / 2);
    for (const double side : {-1.0, 1.0}) {
        for (int i = 0; i < perSide; ++i) {
            const double u = perSide == 1 ? 0.0 : (i - (perSide - 1) / 2.0) * (bf * 0.8 / (perSide - 1));
            out.push_back(sb::box(Role::Plate, rh / 2.0, side * (d / 2.0 + rl / 2.0), u, rh, rl, rt));
        }
    }
}

// ---- 3D das variantes ------------------------------------------------------------------------------------------------------------------------------

namespace {

double beamDepthOf(const NodeConnection &c)
{
    for (const char *key : {"beamD", "beamDepth"}) {
        if (val(c, key, 0.0) > 0.0) {
            return val(c, key);
        }
    }
    const double height = (count(c, "rows", 3.0) - 1) * val(c, "pitch", 70.0) + 2.0 * val(c, "edgeV", 35.0);
    return height + 90.0;
}

void addSupportBeam(const NodeConnection &c, std::vector<Cat::Primitive> &out)
{
    // viga de apoio: perfil I perpendicular, com a alma contra a viga ligada
    const double d = beamDepthOf(c);
    const double dm = d + 40.0, tfm = 11.0, twm = 8.0, bfm = 140.0, span = 420.0;
    out.push_back(sb::box(Role::Concrete, -twm / 2.0, 0.0, 0.0, twm, dm - 2.0 * tfm, span));
    for (const double side : {-1.0, 1.0}) {
        out.push_back(sb::box(Role::Concrete, -bfm / 2.0, side * (dm - tfm) / 2.0, 0.0, bfm, tfm, span));
    }
}

void addColumnBeyond(const NodeConnection &c, std::vector<Cat::Primitive> &out)
{
    const double d = beamDepthOf(c);
    const double colD = 240.0, colBf = 170.0, colTf = 12.0, colTw = 8.0, colH = d + 260.0, cc = -(260.0 + colD / 2.0);
    for (const double side : {-1.0, 1.0}) {
        out.push_back(sb::box(Role::Concrete, cc + side * (colD - colTf) / 2.0, 0.0, 0.0, colTf, colH, colBf));
    }
    out.push_back(sb::box(Role::Concrete, cc, 0.0, 0.0, colD - 2.0 * colTf, colH, colTw));
}

} // namespace

void variantDecorate(const NodeConnection &c, bool context, std::vector<Cat::Primitive> &out)
{
    const auto *entry = ConnectionCatalog::find(c.type);
    if (!entry) {
        return;
    }
    const auto flag = [&entry](const char *key) { return entry->preset.value(QString::fromLatin1(key), 0.0) != 0.0; };
    if (val(c, "ribs", 0.0) > 0.0 && ConnectionCatalog::baseType(c.type) == NodeConnection::kBasePlate) {
        baseRibSolids(c, context, out);
    }
    if (flag("supBeam") && context) {
        addSupportBeam(c, out);
    }
    if (flag("outside") && context) {
        addColumnBeyond(c, out);
    }
}

} // namespace Structura::App::Services::catalog
