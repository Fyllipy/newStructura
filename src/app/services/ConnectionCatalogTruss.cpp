#include "app/services/ConnectionCatalogInternal.h"

#include "design/TrussNodeConnection.h"

#include <QObject>

#include <algorithm>
#include <cmath>

namespace Structura::App::Services::catalog {

namespace D = Structura::Design::Connections;
using Structura::App::ProjectData::NodeConnection;
using Cat = ConnectionCatalog;

namespace {

constexpr double kDeg = 3.14159265358979323846 / 180.0;

QString key(int bar, const char *name)
{
    return QStringLiteral("b%1%2").arg(bar).arg(QString::fromLatin1(name));
}

double barVal(const NodeConnection &c, int bar, const char *name, double fallback = 0.0)
{
    return c.values.value(key(bar, name), fallback);
}

D::TrussNodeInput toInput(const NodeConnection &c, const QVector<double> &forces)
{
    D::TrussNodeInput in;
    in.barMode = static_cast<int>(val(c, "barMode", 0.0) + 0.5);
    in.chordMode = static_cast<int>(val(c, "chordMode", 1.0) + 0.5);
    in.grade = gradeByName(c.grade);
    in.db = val(c, "db", 20.0);
    in.threadInShearPlane = val(c, "thread", 1.0) != 0.0;
    for (int i = 1; i <= 3; ++i) {
        auto &b = in.bars[static_cast<std::size_t>(i - 1)];
        b.on = barVal(c, i, "on", i <= 2 ? 1.0 : 0.0) != 0.0;
        b.force = i - 1 < forces.size() ? forces[i - 1] : 0.0;
        b.angleDeg = barVal(c, i, "ang", 45.0);
        b.toRight = barVal(c, i, "right", i == 2 ? 1.0 : 0.0) != 0.0;
        b.area = barVal(c, i, "area", 800.0);
        b.legWidth = barVal(c, i, "b", 64.0);
        b.legThickness = barVal(c, i, "t", 6.35);
        b.eccentricity = barVal(c, i, "ec", 18.0);
        b.steel = {barVal(c, i, "fy", 250.0), barVal(c, i, "fu", 400.0)};
        b.bars = std::clamp(static_cast<int>(barVal(c, i, "n", 1.0) + 0.5), 1, 2);
        b.weldLeg = barVal(c, i, "w", 5.0);
        b.lengthHeel = barVal(c, i, "lh", 80.0);
        b.lengthToe = barVal(c, i, "lt", 80.0);
        b.rows = std::max(1, static_cast<int>(barVal(c, i, "rows", 3.0) + 0.5));
        b.lines = std::clamp(static_cast<int>(barVal(c, i, "lines", 1.0) + 0.5), 1, 2);
        b.pitch = barVal(c, i, "pitch", 70.0);
        b.gauge = barVal(c, i, "gauge", 60.0);
        b.endDistance = barVal(c, i, "end", 35.0);
        b.edgeDistance = barVal(c, i, "edge", 30.0);
    }
    in.gussetThickness = val(c, "gT", 9.5);
    in.gussetWidth = val(c, "gW", 220.0);
    in.gussetHeight = val(c, "gH", 220.0);
    in.freeLength = val(c, "freeL", 80.0);
    in.gusset = {val(c, "gFy", 250.0), val(c, "gFu", 400.0)};
    in.chordWeldLeg = val(c, "cWeld", 6.0);
    in.fourSides = val(c, "cFour", 0.0) != 0.0;
    in.chordBolts = std::max(2, static_cast<int>(val(c, "cBolts", 4.0) + 0.5));
    in.chordBoltPitch = val(c, "cPitch", 70.0);
    in.weldFw = val(c, "weldFw", 485.0);
    in.chordThickness = val(c, "supT", 12.0);
    in.chord = {val(c, "supFy", 250.0), val(c, "supFu", 400.0)};
    in.ex = val(c, "ex", 0.0);
    in.ey = val(c, "ey", 0.0);
    return in;
}

} // namespace

QVector<Cat::FieldDef> trussNodeFields(const Cat::Entry &entry)
{
    QVector<Cat::FieldDef> list;
    QString group;
    auto section = [&group](const QString &title) { group = title; };
    auto add = [&](const QString &k, const QString &label, double min, double max, int decimals, const QString &suffix, const QStringList &options = {}, const QString &tip = {}) {
        list.append({k, label, min, max, decimals, suffix, group, options, tip});
    };
    const QString mm = QObject::tr(" mm"), mpa = QObject::tr(" MPa"), deg = QObject::tr(" °"), mm2 = QObject::tr(" mm²");
    const int barMode = static_cast<int>(entry.preset.value(QStringLiteral("barMode")) + 0.5);
    const int chordMode = static_cast<int>(entry.preset.value(QStringLiteral("chordMode")) + 0.5);
    const bool direct = chordMode == 0;

    for (int i = 1; i <= 3; ++i) {
        section(i == 3 ? QObject::tr("Montante (barra 3)") : QObject::tr("Diagonal %1 (barra %1)").arg(i));
        add(key(i, "on"), QObject::tr("Barra presente"), 0, 1, 0, QString(), {QObject::tr("Não"), QObject::tr("Sim")});
        add(key(i, "ang"), QObject::tr("Ângulo com o banzo"), 5, 90, 1, deg);
        add(key(i, "right"), QObject::tr("Lado em relação ao nó"), 0, 1, 0, QString(), {QObject::tr("Esquerdo"), QObject::tr("Direito")});
        add(key(i, "area"), QObject::tr("Área bruta de uma peça"), 10, 50000, 0, mm2);
        add(key(i, "b"), QObject::tr("Largura da aba ligada (b)"), 10, 400, 1, mm);
        add(key(i, "t"), QObject::tr("Espessura da aba ligada"), 2, 40, 2, mm);
        add(key(i, "ec"), QObject::tr("Excentricidade do centroide (ec)"), 0, 200, 1, mm);
        add(key(i, "n"), QObject::tr("Peças por barra"), 1, 2, 0, QString(), {}, QObject::tr("2 = par de cantoneiras com a chapa entre elas."));
        add(key(i, "fy"), QObject::tr("fy da barra"), 150, 800, 0, mpa);
        add(key(i, "fu"), QObject::tr("fu da barra"), 250, 1000, 0, mpa);
        if (barMode == 0 || direct) {
            add(key(i, "w"), QObject::tr("Perna do filete"), 3, 20, 1, mm);
            add(key(i, "lh"), QObject::tr("Comprimento do filete do calcanhar"), 20, 600, 0, mm);
            add(key(i, "lt"), QObject::tr("Comprimento do filete da ponta"), 20, 600, 0, mm);
        } else {
            add(key(i, "rows"), QObject::tr("Parafusos ao longo da barra"), 1, 12, 0, QString());
            add(key(i, "lines"), QObject::tr("Linhas de parafusos"), 1, 2, 0, QString());
            add(key(i, "pitch"), QObject::tr("Espaçamento"), 30, 300, 0, mm);
            add(key(i, "gauge"), QObject::tr("Entre linhas"), 30, 300, 0, mm);
            add(key(i, "end"), QObject::tr("Distância do furo extremo à extremidade"), 15, 200, 0, mm);
            add(key(i, "edge"), QObject::tr("Distância à borda da aba"), 15, 200, 0, mm);
        }
    }
    if (barMode == 1 && !direct) {
        section(QObject::tr("Parafusos das barras"));
        add(QStringLiteral("db"), QObject::tr("Diâmetro do parafuso"), 12, 40, 1, mm);
        add(QStringLiteral("thread"), QObject::tr("Plano de corte"), 0, 1, 0, QString(), {QObject::tr("Fora da rosca"), QObject::tr("Na rosca")});
    }
    if (!direct) {
        section(QObject::tr("Chapa de nó (gousset)"));
        add(QStringLiteral("gT"), QObject::tr("Espessura"), 4, 50, 1, mm);
        add(QStringLiteral("gW"), QObject::tr("Largura (ao longo do banzo)"), 60, 1500, 0, mm);
        add(QStringLiteral("gH"), QObject::tr("Altura"), 60, 1500, 0, mm);
        add(QStringLiteral("freeL"), QObject::tr("Distância do banzo ao primeiro parafuso/filete"), 10, 400, 0, mm);
        add(QStringLiteral("gFy"), QObject::tr("fy da chapa"), 150, 800, 0, mpa);
        add(QStringLiteral("gFu"), QObject::tr("fu da chapa"), 250, 1000, 0, mpa);
        section(QObject::tr("Ligação do gousset ao banzo"));
        if (chordMode == 2) {
            add(QStringLiteral("cWeld"), QObject::tr("Perna do filete"), 3, 25, 1, mm);
            add(QStringLiteral("cFour"), QObject::tr("Filetes"), 0, 1, 0, QString(), {QObject::tr("3 lados"), QObject::tr("4 lados")});
        } else if (chordMode == 3) {
            add(QStringLiteral("db"), QObject::tr("Diâmetro do parafuso"), 12, 40, 1, mm);
            add(QStringLiteral("cBolts"), QObject::tr("Número de parafusos (duas colunas)"), 2, 24, 0, QString());
            add(QStringLiteral("cPitch"), QObject::tr("Espaçamento"), 40, 300, 0, mm);
        }
        add(QStringLiteral("weldFw"), QObject::tr("fw (eletrodo)"), 300, 700, 0, mpa);
        add(QStringLiteral("ex"), QObject::tr("Excentricidade ao longo do banzo (ex)"), -500, 500, 1, mm);
        add(QStringLiteral("ey"), QObject::tr("Excentricidade normal ao banzo (ey)"), -500, 500, 1, mm);
    } else {
        add(QStringLiteral("weldFw"), QObject::tr("fw (eletrodo)"), 300, 700, 0, mpa);
    }
    section(QObject::tr("Banzo"));
    add(QStringLiteral("supT"), QObject::tr("Espessura da mesa onde se solda"), 3, 60, 1, mm);
    add(QStringLiteral("supFy"), QObject::tr("fy do banzo"), 150, 800, 0, mpa);
    add(QStringLiteral("supFu"), QObject::tr("fu do banzo"), 250, 1000, 0, mpa);
    add(QStringLiteral("supD"), QObject::tr("Altura do perfil do banzo"), 0, 2000, 0, mm);
    add(QStringLiteral("supBf"), QObject::tr("Largura da mesa do banzo"), 0, 600, 0, mm);
    add(QStringLiteral("supTf"), QObject::tr("Espessura da mesa do banzo"), 0, 60, 1, mm);
    add(QStringLiteral("supTw"), QObject::tr("Espessura da alma do banzo"), 0, 40, 1, mm);
    add(QStringLiteral("supR"), QObject::tr("Raio de concordância"), 0, 40, 1, mm);
    add(QStringLiteral("supLb"), QObject::tr("Comprimento destravado (0 = contido)"), 0, 20000, 0, mm);
    add(QStringLiteral("supEnd"), QObject::tr("Nó na extremidade do banzo"), 0, 1, 0, QString(), {QObject::tr("Não (banzo contínuo)"), QObject::tr("Sim")});
    return list;
}

NodeConnection trussNodeDefault(const Cat::Entry &entry, const Cat::BeamInfo &chord, double plateFy, double plateFu, const QVector<Cat::BraceInfo> &braces)
{
    NodeConnection c;
    c.grade = defaultBoltGradeName();
    const int barMode = static_cast<int>(entry.preset.value(QStringLiteral("barMode")) + 0.5);
    const int chordMode = static_cast<int>(entry.preset.value(QStringLiteral("chordMode")) + 0.5);
    c.values = {{"barMode", static_cast<double>(barMode)}, {"chordMode", static_cast<double>(chordMode)}, {"supType", 1.0}, {"db", 20.0}, {"thread", 1.0},
                {"gT", 9.5}, {"gW", 220.0}, {"gH", 220.0}, {"freeL", 80.0}, {"gFy", plateFy}, {"gFu", plateFu}, {"cWeld", 6.0}, {"cFour", 0.0}, {"cBolts", 4.0}, {"cPitch", 70.0},
                {"weldFw", 485.0}, {"ex", 0.0}, {"ey", 0.0},
                {"supT", chord.tf > 0.0 ? chord.tf : 12.0}, {"supFy", chord.fy}, {"supFu", chord.fu}, {"supD", chord.depth}, {"supBf", chord.bf}, {"supTf", chord.tf},
                {"supTw", chord.tw}, {"supR", chord.geometry.value(QStringLiteral("r"), 0.0)}, {"supLb", 0.0}, {"supEnd", 0.0}};
    for (int i = 1; i <= 3; ++i) {
        const bool have = i - 1 < braces.size();
        const Cat::BraceInfo b = have ? braces[i - 1] : Cat::BraceInfo();
        const bool on = have || i <= 2;
        const double legT = b.legT > 0.0 ? b.legT : 6.35;
        const double leg = b.legB > 0.0 ? b.legB : 64.0;
        c.values[key(i, "on")] = on ? 1.0 : 0.0;
        c.values[key(i, "ang")] = have ? b.angleDeg : (i == 3 ? 90.0 : 45.0);
        c.values[key(i, "right")] = have ? (b.alongChord ? 1.0 : 0.0) : (i == 2 ? 1.0 : 0.0);
        c.values[key(i, "area")] = b.area > 0.0 ? b.area : 800.0;
        c.values[key(i, "b")] = leg;
        c.values[key(i, "t")] = legT;
        c.values[key(i, "ec")] = b.ec > 0.0 ? b.ec : 0.28 * leg;
        c.values[key(i, "n")] = 1.0;
        c.values[key(i, "fy")] = b.fy;
        c.values[key(i, "fu")] = b.fu;
        c.values[key(i, "w")] = std::clamp(std::floor(legT * 0.8), 3.0, 12.0);
        c.values[key(i, "lh")] = 90.0;
        c.values[key(i, "lt")] = 70.0;
        c.values[key(i, "rows")] = 3.0;
        c.values[key(i, "lines")] = 1.0;
        c.values[key(i, "pitch")] = 70.0;
        c.values[key(i, "gauge")] = 60.0;
        c.values[key(i, "end")] = 35.0;
        c.values[key(i, "edge")] = std::max(25.0, 0.5 * leg);
        if (have) {
            c.values[QStringLiteral("br%1").arg(i)] = b.memberId;
            c.values[QStringLiteral("br%1End").arg(i)] = b.atEnd ? 1.0 : 0.0;
        }
    }
    return c;
}

Cat::Evaluation trussNodeEvaluate(const NodeConnection &c, const Cat::Forces &f)
{
    Cat::Evaluation e;
    // combinacoes: a mais desfavoravel (esforcos de cada barra do no)
    QVector<ConnectionService::ForceCase> cases = f.cases;
    if (cases.isEmpty()) {
        ConnectionService::ForceCase single;
        single.name = QObject::tr("envoltória");
        single.braceSigned = {f.axial, 0.0, 0.0};
        cases.append(single);
    }
    double worst = -1.0;
    D::TrussNodeInput worstInput;
    for (const auto &fc : cases) {
        QVector<double> forces = fc.braceSigned;
        if (forces.isEmpty()) {
            forces = {fc.axial};
        }
        const D::TrussNodeInput in = toInput(c, forces);
        auto checks = D::checkTrussNode(in);
        const double ratio = D::worstRatio(checks);
        if (ratio > worst) {
            worst = ratio;
            e.checks = std::move(checks);
            e.governingCase = fc.name;
            worstInput = in;
        }
    }
    e.parts = D::partsTrussNode(worstInput);
    // verificacao local do banzo com a componente normal da resultante
    const auto r = D::trussInterfaceForces(worstInput);
    ConnectionService::appendSupportLocal(e, c, f, std::max(worstInput.gussetWidth, 50.0), worstInput.gussetThickness + 2.0 * worstInput.chordWeldLeg, r.pull, r.push);
    e.maxRatio = D::worstRatio(e.checks);
    if (const auto *g = D::governing(e.checks)) {
        e.governing = QString::fromStdString(g->name);
    }
    e.description = trussNodeDescribe(c);
    return e;
}

QString trussNodeDescribe(const NodeConnection &c)
{
    const int barMode = static_cast<int>(val(c, "barMode", 0.0) + 0.5);
    const int chordMode = static_cast<int>(val(c, "chordMode", 1.0) + 0.5);
    int bars = 0;
    for (int i = 1; i <= 3; ++i) {
        bars += barVal(c, i, "on", 0.0) != 0.0 ? 1 : 0;
    }
    const QString bar = barMode == 0 ? QObject::tr("soldadas") : QObject::tr("parafusadas");
    if (chordMode == 0) {
        return QObject::tr("%1 barra(s) soldada(s) direto no banzo").arg(bars);
    }
    const QString chord = chordMode == 1 ? QObject::tr("solda de topo") : (chordMode == 2 ? QObject::tr("filetes (%1 lados)").arg(val(c, "cFour") != 0.0 ? 4 : 3) : QObject::tr("%1 parafusos").arg(static_cast<int>(val(c, "cBolts") + 0.5)));
    return QObject::tr("%1 barra(s) %2 na chapa de nó t = %3 mm (%4 x %5 mm), chapa ao banzo por %6").arg(bars).arg(bar).arg(val(c, "gT"), 0, 'f', 1).arg(val(c, "gW"), 0, 'f', 0).arg(val(c, "gH"), 0, 'f', 0).arg(chord);
}

namespace {

/// Retangulo orientado: centro (s, t), direcao (ds, dt) e dimensoes ao longo da direcao e perpendicular.
std::vector<std::array<double, 2>> orientedRect(double cs, double ct, double ds, double dt, double length, double width)
{
    const double ps = -dt, pt = ds;
    std::vector<std::array<double, 2>> poly;
    for (const auto &[a, b] : {std::pair<double, double> {-1, -1}, {1, -1}, {1, 1}, {-1, 1}}) {
        poly.push_back({cs + a * length / 2.0 * ds + b * width / 2.0 * ps, ct + a * length / 2.0 * dt + b * width / 2.0 * pt});
    }
    return poly;
}

void addPrism(std::vector<ConnectionSolids::Primitive> &out, Role role, const std::vector<std::array<double, 2>> &poly, double u0, double u1)
{
    ConnectionSolids::Primitive p;
    p.kind = Kind::Prism;
    p.role = role;
    p.polygon = poly;
    p.u0 = u0;
    p.u1 = u1;
    out.push_back(std::move(p));
}

} // namespace

void trussNodeSolids(const NodeConnection &c, bool context, std::vector<ConnectionSolids::Primitive> &out)
{
    const D::TrussNodeInput in = toInput(c, {});
    const double chordD = std::max(val(c, "supD", 0.0), 120.0), chordBf = std::max(val(c, "supBf", 0.0), 100.0), chordTf = std::max(val(c, "supTf", 0.0), 8.0), chordTw = std::max(val(c, "supTw", 0.0), 6.0);
    const double top = chordD / 2.0;
    const double gT = in.gussetThickness, gW = in.gussetWidth, gH = in.gussetHeight;
    const bool direct = in.chordMode == D::TrussNodeInput::Direct;
    // banzo (perfil I com a alma no plano do no)
    for (const double side : {-1.0, 1.0}) {
        out.push_back(sb::box(Role::Member, 0.0, side * (chordD - chordTf) / 2.0, 0.0, 700.0, chordTf, chordBf));
    }
    out.push_back(sb::box(Role::Member, 0.0, 0.0, 0.0, 700.0, chordD - 2.0 * chordTf, chordTw));
    if (!direct) {
        ConnectionSolids::Primitive plate;
        plate.kind = Kind::Prism;
        plate.role = Role::Plate;
        plate.polygon = {{-gW / 2.0, top}, {gW / 2.0, top}, {gW / 2.0, top + gH}, {-gW / 2.0, top + gH}};
        plate.u0 = -gT / 2.0;
        plate.u1 = gT / 2.0;
        out.push_back(std::move(plate));
        if (in.chordMode == D::TrussNodeInput::BoltedToChord) {
            const int rows = std::max(1, in.chordBolts / 2);
            for (int r = 0; r < rows; ++r) {
                for (const double col : {-1.0, 1.0}) {
                    sb::addBolt(out, {col * 40.0 + 0.0, top - chordTf / 2.0, (r - (rows - 1) / 2.0) * in.chordBoltPitch}, 1, top - chordTf - 2.0, top + gT / 2.0 + 2.0, in.db);
                }
            }
        } else {
            const double leg = in.chordWeldLeg;
            for (const double side : {-1.0, 1.0}) {
                out.push_back(sb::box(Role::Weld, 0.0, top + leg / 2.0, side * (gT / 2.0 + leg / 2.0), gW, leg, leg));
            }
            if (in.chordMode == D::TrussNodeInput::ButtWeld) {
                out.push_back(sb::box(Role::Weld, 0.0, top + leg / 2.0, 0.0, gW, leg, gT));
            }
        }
    }
    // barras (cantoneiras): aba ligada no plano do gousset, perna livre perpendicular
    int index = 0;
    for (const auto &b : in.bars) {
        ++index;
        if (!b.on) {
            continue;
        }
        const double th = b.angleDeg * kDeg;
        const double sideSign = b.toRight ? 1.0 : -1.0;
        const double ds = std::cos(th) * sideSign, dt = std::sin(th);
        const double foot = (direct ? 0.0 : sideSign * gW * 0.18) + (index == 3 ? 0.0 : 0.0);
        const double baseT = top + (direct ? 0.0 : gH * 0.18);
        const double length = 380.0;
        const double cs = foot + ds * length / 2.0, ct = baseT + dt * length / 2.0 + 15.0;
        const double legW = std::max(b.legWidth, 20.0), legT = std::max(b.legThickness, 3.0);
        const double u0 = direct ? chordBf / 2.0 : gT / 2.0;
        for (int piece = 0; piece < b.bars; ++piece) {
            const double face = piece == 0 ? 1.0 : -1.0;
            const double ua = face > 0 ? u0 : -u0 - legT, ub = face > 0 ? u0 + legT : -u0;
            addPrism(out, Role::Member, orientedRect(cs, ct, ds, dt, length, legW), ua, ub);
            // perna livre (perpendicular a aba), na borda externa
            const double edgeS = cs - dt * (legW / 2.0 - legT / 2.0) * -1.0, edgeT = ct + ds * (legW / 2.0 - legT / 2.0) * -1.0;
            addPrism(out, Role::Member, orientedRect(edgeS, edgeT, ds, dt, length, legT), face > 0 ? ub : ua, face > 0 ? ub + legW * 0.8 : ua - legW * 0.8);
        }
        // ligacao da barra: filetes ou parafusos
        if (in.barMode == D::TrussNodeInput::Welded || direct) {
            for (const double pos : {-1.0, 1.0}) {
                const double cs2 = foot + ds * (b.lengthHeel / 2.0 + 10.0) - dt * pos * (legW / 2.0) * -1.0, ct2 = baseT + dt * (b.lengthHeel / 2.0 + 10.0) + ds * pos * (legW / 2.0) * -1.0 + 15.0;
                out.push_back(sb::box(Role::Weld, cs2, ct2, u0 + legT + b.weldLeg / 2.0, std::abs(ds) * b.lengthHeel + b.weldLeg, std::abs(dt) * b.lengthHeel + b.weldLeg, b.weldLeg));
            }
        } else {
            for (int r = 0; r < b.rows; ++r) {
                for (int l = 0; l < b.lines; ++l) {
                    const double along = in.freeLength + r * b.pitch, across = (l - (b.lines - 1) / 2.0) * b.gauge;
                    const double ps = -dt, pt = ds;
                    sb::addBolt(out, {foot + ds * along + ps * across, baseT + dt * along + pt * across + 15.0, 0.0}, 2, -gT / 2.0 - legT - 3.0, gT / 2.0 + legT + 3.0, in.db);
                }
            }
        }
    }
    (void)context;
}

std::vector<Cat::Dim> trussNodeDims(const NodeConnection &c)
{
    const D::TrussNodeInput in = toInput(c, {});
    std::vector<Cat::Dim> dims;
    if (in.chordMode == D::TrussNodeInput::Direct) {
        return dims;
    }
    const double top = std::max(val(c, "supD", 0.0), 120.0) / 2.0;
    auto text = [](const char *label, double v) {
        char buffer[48];
        std::snprintf(buffer, sizeof buffer, "%s = %.0f", label, v);
        return std::string(buffer);
    };
    dims.push_back({-in.gussetWidth / 2.0, top - 18.0, in.gussetWidth / 2.0, top - 18.0, -16.0, text("Lg", in.gussetWidth)});
    dims.push_back({in.gussetWidth / 2.0 + 12.0, top, in.gussetWidth / 2.0 + 12.0, top + in.gussetHeight, 12.0, text("Hg", in.gussetHeight)});
    return dims;
}

std::vector<std::string> trussNodeNotes(const NodeConnection &c)
{
    const D::TrussNodeInput in = toInput(c, {});
    std::vector<std::string> notes;
    char buffer[240];
    const char *chord[] = {"BARRAS SOLDADAS DIRETO NO BANZO", "CHAPA DE NÓ SOLDADA AO BANZO COM SOLDA DE TOPO (PENETRAÇÃO TOTAL)", "CHAPA DE NÓ SOLDADA AO BANZO COM FILETES", "CHAPA DE NÓ PARAFUSADA AO BANZO"};
    notes.push_back(chord[std::clamp(in.chordMode, 0, 3)]);
    if (in.chordMode != 0) {
        std::snprintf(buffer, sizeof buffer, "CHAPA DE NÓ t = %.1f mm, %.0f x %.0f mm; BARRAS %s.", in.gussetThickness, in.gussetWidth, in.gussetHeight, in.barMode == 0 ? "SOLDADAS (FILETES)" : "PARAFUSADAS");
        notes.push_back(buffer);
    }
    int index = 0;
    for (const auto &b : in.bars) {
        ++index;
        if (b.on) {
            std::snprintf(buffer, sizeof buffer, "BARRA %d: ÂNGULO %.0f°, %s.", index, b.angleDeg, in.barMode == 0 || in.chordMode == 0 ? "FILETE DE SOLDA" : "PARAFUSOS");
            notes.push_back(buffer);
        }
    }
    return notes;
}

QPair<QString, QString> trussNodeSteelKeys()
{
    return {QStringLiteral("gFy"), QStringLiteral("gFu")};
}

} // namespace Structura::App::Services::catalog
