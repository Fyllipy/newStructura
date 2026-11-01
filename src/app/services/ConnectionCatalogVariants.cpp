#include "app/services/ConnectionCatalogInternal.h"

#include "design/ShearConnections.h"
#include "design/SupportWeld.h"

#include <QObject>

#include <algorithm>
#include <cmath>

namespace Structura::App::Services::catalog {

namespace D = Structura::Design::Connections;
using Structura::App::ProjectData::NodeConnection;
using Cat = ConnectionCatalog;

// ---- nervuras das bases de pilar ---------------------------------------------------------------------------------------------------------------

QVector<Cat::FieldDef> outsideFields()
{
    const QString group = QObject::tr("Posição da junta");
    const QString weld = QObject::tr("Solda da viga ao apoio");
    const QString mm = QObject::tr(" mm");
    return {{QStringLiteral("outDist"), QObject::tr("Distância da face do apoio à junta"), 50, 3000, 0, mm, group, {},
             QObject::tr("Ligação fora do nó: a junta (talas ou chapas de topo) fica a esta distância da face do pilar/viga de apoio, medida ao longo da viga; a viga se liga ao apoio por solda.")},
            {QStringLiteral("supWeldKind"), QObject::tr("Solda das mesas"), 0, 1, 0, QString(), weld, {QObject::tr("Penetração total"), QObject::tr("Filete")},
             QObject::tr("Penetração total: a resistência é a do metal-base da mesa (6.2.4). Filete: dimensionado pela perna.")},
            {QStringLiteral("supWeldLegF"), QObject::tr("Perna do filete das mesas"), 3, 25, 1, mm, weld, {}, QObject::tr("Usada quando a solda das mesas é de filete.")},
            {QStringLiteral("supWeldLegW"), QObject::tr("Perna dos filetes da alma"), 3, 25, 1, mm, weld, {}, QString()},
            {QStringLiteral("weldFw"), QObject::tr("fw – resistência do metal da solda"), 350, 700, 0, QObject::tr(" MPa"), weld, {}, QObject::tr("Tabela A.4: E60 = 415 MPa, E70 = 485 MPa.")}};
}

void supportWeldDefaults(NodeConnection &c)
{
    // pernas padrao: 6 mm na alma e 8 mm nas mesas, limitadas ao maximo do filete na borda (t - 1,5 mm para t >= 6,35 mm) e arredondadas para baixo a 0,5 mm
    const double tw = val(c, "tw", 6.0), tf = val(c, "tfl", 10.0);
    auto limited = [](double leg, double t) { return std::max(3.0, std::floor(std::min(leg, D::maximumFilletLeg(t)) * 2.0) / 2.0); };
    c.values["supWeldKind"] = 0.0;
    c.values["supWeldLegF"] = limited(8.0, tf);
    c.values["supWeldLegW"] = limited(6.0, tw);
    if (!c.values.contains(QStringLiteral("weldFw"))) {
        c.values["weldFw"] = 485.0;
    }
}

namespace {

D::SupportWeldInput supportWeldInput(const NodeConnection &c, const Cat::Forces &f)
{
    D::SupportWeldInput in;
    in.depth = val(c, "beamD", 300.0);
    in.flangeWidth = val(c, "bfl", 150.0);
    in.flangeThickness = val(c, "tfl", 10.0);
    in.webThickness = val(c, "tw", 6.0);
    in.beamFy = val(c, "beamFy", val(c, "webFy", 250.0));
    in.beamFu = val(c, "beamFu", val(c, "webFu", 400.0));
    in.flangeFullPenetration = val(c, "supWeldKind", 0.0) < 0.5;
    in.flangeLeg = val(c, "supWeldLegF", 8.0);
    in.webLeg = val(c, "supWeldLegW", 6.0);
    in.fw = val(c, "weldFw", 485.0);
    in.shear = std::abs(f.shear);
    in.axial = f.axial;
    in.moment = std::abs(f.moment);
    // espessura da parte do apoio onde a viga e soldada (mesa do pilar), quando a barra de apoio foi escolhida; na flangeada o campo supTf e da outra chapa
    if (c.type != 147) {
        in.supportThickness = val(c, "supTf", 0.0);
        in.supportFy = val(c, "supFy", 250.0);
        in.supportFu = val(c, "supFu", 400.0);
    }
    return in;
}

} // namespace

void appendSupportWeld(const NodeConnection &c, const Cat::Forces &forces, Cat::Evaluation &evaluation)
{
    if (val(c, "outside", 0.0) == 0.0) {
        return;
    }
    const auto in = supportWeldInput(c, forces);
    for (auto &check : D::checkSupportWeld(in)) {
        evaluation.checks.push_back(std::move(check));
    }
    const double beamD = in.depth, bf = in.flangeWidth, tw = in.webThickness, tf = in.flangeThickness;
    const double hw = std::max(1.0, beamD - 2.0 * tf);
    const double flangeLength = in.flangeFullPenetration ? 2.0 * bf : 2.0 * std::max(2.0 * bf - tw, bf);
    D::Part flange;
    flange.description = in.flangeFullPenetration ? "Solda viga-apoio: penetracao total nas mesas (comprimento total)" : "Solda viga-apoio: filete " + std::to_string(static_cast<int>(in.flangeLeg)) + " mm nas mesas";
    flange.quantity = 1;
    flange.length = flangeLength;
    flange.unit = "mm";
    evaluation.parts.push_back(flange);
    D::Part web;
    web.description = "Solda viga-apoio: filete " + std::to_string(static_cast<int>(in.webLeg)) + " mm na alma (2 filetes)";
    web.quantity = 1;
    web.length = 2.0 * hw;
    web.unit = "mm";
    evaluation.parts.push_back(web);
    evaluation.maxRatio = D::worstRatio(evaluation.checks);
    evaluation.governing.clear();
    if (const auto *g = D::governing(evaluation.checks)) {
        evaluation.governing = QString::fromStdString(g->name);
    }
}

bool proposeSupportWeld(NodeConnection &c, const Cat::Forces &forces)
{
    if (val(c, "outside", 0.0) == 0.0) {
        return false;
    }
    const D::SupportWeldInput base = supportWeldInput(c, forces);
    bool changed = false;
    auto passes = [&](const D::SupportWeldInput &in, const char *key) {
        for (const auto &check : D::checkSupportWeld(in)) {
            if (check.name.find(key) != std::string::npos && !check.ok()) {
                return false;
            }
        }
        return true;
    };
    // pernas: a menor (de 1 em 1 mm, entre a minima e a maxima da Tabela 11 / 6.2.5.2) que atende o grupo correspondente
    auto choose = [&](const char *valueKey, const char *checkKey, double minLeg, double maxLeg, bool flange) {
        double chosen = maxLeg;
        for (double leg = std::ceil(minLeg); leg <= maxLeg + 1e-9; leg += 1.0) {
            D::SupportWeldInput in = base;
            (flange ? in.flangeLeg : in.webLeg) = leg;
            if (passes(in, checkKey)) {
                chosen = leg;
                break;
            }
        }
        if (std::abs(c.values.value(QString::fromLatin1(valueKey)) - chosen) > 1e-9) {
            c.values[QString::fromLatin1(valueKey)] = chosen;
            changed = true;
        }
    };
    choose("supWeldLegW", "alma", D::minimumFilletLeg(std::min(base.webThickness, base.supportThickness > 0.0 ? base.supportThickness : base.webThickness)), D::maximumFilletLeg(base.webThickness), false);
    if (!base.flangeFullPenetration) {
        choose("supWeldLegF", "mesas", D::minimumFilletLeg(std::min(base.flangeThickness, base.supportThickness > 0.0 ? base.supportThickness : base.flangeThickness)),
               D::maximumFilletLeg(base.flangeThickness), true);
    }
    return changed;
}

void addSupportWeldSolids(const NodeConnection &c, std::vector<Cat::Primitive> &out)
{
    if (val(c, "outside", 0.0) == 0.0) {
        return;
    }
    const double d = val(c, "beamD", 300.0), bf = val(c, "bfl", 150.0), tf = val(c, "tfl", 10.0), tw = val(c, "tw", 6.0);
    const bool cjp = val(c, "supWeldKind", 0.0) < 0.5;
    const double legF = cjp ? std::max(6.0, 0.7 * tf) : val(c, "supWeldLegF", 8.0);
    const double legW = val(c, "supWeldLegW", 6.0);
    // plano da face do apoio: s = -outDist (a junta fica em s = 0); a solda se estende da face para dentro da viga
    const double face = -val(c, "outDist", 260.0);
    for (const double side : {-1.0, 1.0}) {
        out.push_back(sb::box(Role::Weld, face + legF / 2.0, side * (d / 2.0 - tf / 2.0), 0.0, legF, tf + 2.0 * legF, bf + 2.0 * legF));
        out.push_back(sb::box(Role::Weld, face + legW / 2.0, 0.0, side * (tw / 2.0 + legW / 2.0), legW, std::max(1.0, d - 2.0 * tf), legW));
    }
}

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
    const double colD = 240.0, colBf = 170.0, colTf = 12.0, colTw = 8.0, colH = d + 260.0, cc = -(val(c, "outDist", 260.0) + colD / 2.0);
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
    if (flag("outside")) {
        addSupportWeldSolids(c, out);
    }
}

} // namespace Structura::App::Services::catalog
