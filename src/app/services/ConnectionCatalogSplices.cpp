#include "app/services/ConnectionCatalogInternal.h"

#include "design/CoverSplice.h"

#include <QObject>

#include <algorithm>
#include <cmath>

namespace Structura::App::Services::catalog {

namespace D = Structura::Design::Connections;
using Structura::App::ProjectData::NodeConnection;
using Cat = ConnectionCatalog;

namespace {

D::BoltGrade gradeByName(const QString &name)
{
    for (const auto &g : D::boltGrades()) {
        if (QString::fromStdString(g.name) == name) {
            return g;
        }
    }
    return D::boltGrades().size() > 2 ? D::boltGrades()[2] : D::boltGrades().front();
}

D::CoverSpliceInput toCover(const NodeConnection &c, double axial)
{
    D::CoverSpliceInput in;
    in.shape = static_cast<int>(val(c, "shape", 0.0) + 0.5);
    in.grade = gradeByName(c.grade);
    in.db = val(c, "db", 20.0);
    in.threadInShearPlane = val(c, "thread", 1.0) != 0.0;
    in.area = val(c, "area", 0.0);
    in.thickness = val(c, "t", 10.0);
    in.width = val(c, "bw", 100.0);
    in.eccentricity = val(c, "ec", 0.0);
    in.bar = {val(c, "barFy", 250.0), val(c, "barFu", 400.0)};
    in.covers = std::clamp(static_cast<int>(val(c, "covN", 2.0) + 0.5), 1, 2);
    in.coverThickness = val(c, "covT", 8.0);
    in.coverWidth = val(c, "covW", 100.0);
    in.cover = {val(c, "covFy", 250.0), val(c, "covFu", 400.0)};
    in.rows = count(c, "rows", 3.0);
    in.lines = std::clamp(count(c, "lines", 1.0), 1, 3);
    in.pitch = val(c, "pitch", 60.0);
    in.gauge = val(c, "gauge", 50.0);
    in.endDistance = val(c, "endD", 35.0);
    in.edgeDistance = val(c, "edgeD", 30.0);
    in.gap = val(c, "gap", 10.0);
    in.axial = axial;
    return in;
}

D::TubeFlangeInput toTube(const NodeConnection &c, double shear, double axial, double moment)
{
    D::TubeFlangeInput in;
    in.grade = gradeByName(c.grade);
    in.db = val(c, "db", 20.0);
    in.threadInShearPlane = val(c, "thread", 1.0) != 0.0;
    in.tubeDiameter = val(c, "tubeD", 114.3);
    in.tubeThickness = val(c, "tubeT", 6.0);
    in.tube = {val(c, "tubeFy", 250.0), val(c, "tubeFu", 400.0)};
    in.plateDiameter = val(c, "plateD", 240.0);
    in.plateThickness = val(c, "plateT", 20.0);
    in.plate = {val(c, "plateFy", 250.0), val(c, "plateFu", 400.0)};
    in.bolts = std::max(3, static_cast<int>(val(c, "nb", 6.0) + 0.5));
    in.boltCircle = val(c, "bc", 190.0);
    in.weldLeg = val(c, "weldLeg", 6.0);
    in.weldFw = val(c, "weldFw", 485.0);
    in.shear = shear;
    in.axial = axial;
    in.moment = moment;
    return in;
}

} // namespace

// ---- cobrejuntas -----------------------------------------------------------------------------------------------------------------------------

QVector<Cat::FieldDef> coverSpliceFields(const Cat::Entry &entry)
{
    QVector<Cat::FieldDef> list;
    QString group;
    auto section = [&group](const QString &title) { group = title; };
    auto add = [&](const char *k, const QString &label, double min, double max, int decimals, const QString &suffix, const QStringList &options = {}, const QString &tip = {}) {
        list.append({QString::fromLatin1(k), label, min, max, decimals, suffix, group, options, tip});
    };
    const QString mm = QObject::tr(" mm"), mpa = QObject::tr(" MPa"), mm2 = QObject::tr(" mm²");
    const bool angle = entry.preset.value(QStringLiteral("shape")) >= 1.0;
    section(angle ? QObject::tr("Cantoneira") : QObject::tr("Barra (chapa)"));
    add("area", QObject::tr("Área bruta"), 10, 100000, 0, mm2, {}, QObject::tr("0 = largura × espessura."));
    add("bw", angle ? QObject::tr("Largura da aba ligada") : QObject::tr("Largura"), 10, 800, 1, mm);
    add("t", QObject::tr("Espessura"), 2, 60, 2, mm);
    if (angle) {
        add("ec", QObject::tr("Excentricidade do centroide (ec)"), 0, 200, 1, mm);
    }
    add("barFy", QObject::tr("fy da barra"), 150, 800, 0, mpa);
    add("barFu", QObject::tr("fu da barra"), 250, 1000, 0, mpa);
    section(QObject::tr("Cobrejuntas"));
    add("covN", QObject::tr("Número de cobrejuntas"), 1, 2, 0, QString(), {}, QObject::tr("2 = corte duplo; 1 = corte simples."));
    add("covT", QObject::tr("Espessura de cada uma"), 3, 50, 1, mm);
    add("covW", QObject::tr("Largura"), 20, 800, 0, mm);
    add("covFy", QObject::tr("fy"), 150, 800, 0, mpa);
    add("covFu", QObject::tr("fu"), 250, 1000, 0, mpa);
    section(QObject::tr("Parafusos (de cada lado da junta)"));
    add("db", QObject::tr("Diâmetro do parafuso"), 10, 40, 1, mm);
    add("thread", QObject::tr("Plano de corte"), 0, 1, 0, QString(), {QObject::tr("Fora da rosca"), QObject::tr("Na rosca")});
    add("rows", QObject::tr("Parafusos ao longo da barra"), 1, 12, 0, QString());
    add("lines", QObject::tr("Linhas de parafusos"), 1, 3, 0, QString());
    add("pitch", QObject::tr("Espaçamento"), 25, 300, 0, mm);
    add("gauge", QObject::tr("Entre linhas"), 25, 300, 0, mm);
    add("endD", QObject::tr("Furo extremo – extremidade da barra"), 15, 200, 0, mm);
    add("edgeD", QObject::tr("Linha de furos – borda"), 15, 200, 0, mm);
    add("gap", QObject::tr("Folga entre as barras"), 0, 60, 0, mm);
    return list;
}

NodeConnection coverSpliceDefault(const Cat::Entry &entry, const Cat::BeamInfo &beam, double plateFy, double plateFu)
{
    NodeConnection c;
    c.grade = QString::fromStdString(D::boltGrades().size() > 2 ? D::boltGrades()[2].name : D::boltGrades().front().name);
    const int shape = static_cast<int>(entry.preset.value(QStringLiteral("shape")) + 0.5);
    const int covers = static_cast<int>(entry.preset.value(QStringLiteral("covN"), 2.0) + 0.5);
    const auto &g = beam.geometry;
    const double t = std::max(g.value(QStringLiteral("t"), beam.tf > 0.0 ? beam.tf : 8.0), 3.0);
    double width = g.value(QStringLiteral("b"), std::max(g.value(QStringLiteral("bf"), 0.0), beam.bf > 0.0 ? beam.bf : 80.0));
    double ec = 0.0;
    if (shape == 1) {
        ec = (width * width + width * t - t * t) / (2.0 * (2.0 * width - t));
    }
    const double area = beam.area > 0.0 ? beam.area : width * t;
    c.values = {{"shape", static_cast<double>(shape)}, {"covN", static_cast<double>(covers)}, {"area", area}, {"bw", width}, {"t", t}, {"ec", ec},
                {"barFy", beam.fy}, {"barFu", beam.fu}, {"covT", std::max(6.3, std::ceil(t))}, {"covW", width}, {"covFy", plateFy}, {"covFu", plateFu},
                {"db", t < 8.0 ? 16.0 : 20.0}, {"thread", 1.0}, {"rows", 3.0}, {"lines", 1.0}, {"pitch", 60.0}, {"gauge", 50.0}, {"endD", 35.0}, {"edgeD", std::max(25.0, width / 2.0)}, {"gap", 10.0}};
    return c;
}

Cat::Evaluation coverSpliceEvaluate(const NodeConnection &c, const Cat::Forces &f)
{
    Cat::Evaluation e;
    const double tension = std::max({f.tension, std::max(f.axial, 0.0), 0.0});
    const double compression = std::max({f.compression, std::max(-f.axial, 0.0), 0.0});
    double worst = -1.0;
    for (const double axial : {tension, -compression}) {
        if (axial == 0.0 && worst >= 0.0) {
            continue;
        }
        auto checks = D::checkCoverSplice(toCover(c, axial));
        if (axial < 0.0) {
            for (auto &chk : checks) {
                chk.name += QObject::tr(" [compressão]").toStdString();
            }
        }
        const double ratio = D::worstRatio(checks);
        if (worst < 0.0 || axial < 0.0) {
            // junta as duas situacoes (tracao e compressao): permanecem todas as verificacoes
            e.checks.insert(e.checks.end(), checks.begin(), checks.end());
        }
        worst = std::max(worst, ratio);
    }
    e.parts = D::partsCoverSplice(toCover(c, 0.0));
    e.maxRatio = D::worstRatio(e.checks);
    if (const auto *g = D::governing(e.checks)) {
        e.governing = QString::fromStdString(g->name);
    }
    e.description = coverSpliceDescribe(c);
    return e;
}

QString coverSpliceDescribe(const NodeConnection &c)
{
    return QObject::tr("%1 cobrejunta(s) %2 × %3 mm, %4 parafusos d%5 %6 por lado")
        .arg(static_cast<int>(val(c, "covN") + 0.5)).arg(val(c, "covT"), 0, 'f', 1).arg(val(c, "covW"), 0, 'f', 0).arg(count(c, "rows", 3.0) * count(c, "lines", 1.0))
        .arg(val(c, "db"), 0, 'f', 0).arg(c.grade);
}

void coverSpliceSolids(const NodeConnection &c, bool context, std::vector<ConnectionSolids::Primitive> &out)
{
    const D::CoverSpliceInput in = toCover(c, 0.0);
    const double L = D::coverSpliceLength(in), half = L / 2.0;
    const double reach = context ? 200.0 : 60.0;
    const double t = in.thickness, w = in.width, ct = in.coverThickness, cw = in.coverWidth;
    const bool angle = in.shape == D::CoverSpliceInput::Angle;
    // barras (de cada lado da junta)
    for (const double side : {-1.0, 1.0}) {
        const double s0 = side * (in.gap / 2.0), s1 = side * (half + reach);
        out.push_back(sb::box(Role::Member, (s0 + s1) / 2.0, 0.0, 0.0, std::abs(s1 - s0), w, t));
        if (angle) {
            out.push_back(sb::box(Role::Member, (s0 + s1) / 2.0, w / 2.0 - t / 2.0, -t / 2.0 - w * 0.4, std::abs(s1 - s0), t, w * 0.8));
        }
    }
    // cobrejuntas
    for (int i = 0; i < in.covers; ++i) {
        const double u = (i == 0 ? 1.0 : -1.0) * (t / 2.0 + ct / 2.0);
        out.push_back(sb::box(Role::Plate, 0.0, 0.0, u, L, cw, ct));
    }
    // parafusos
    const double u0 = in.covers == 2 ? -t / 2.0 - ct : -t / 2.0, u1 = t / 2.0 + ct;
    for (const double side : {-1.0, 1.0}) {
        for (int r = 0; r < in.rows; ++r) {
            for (int l = 0; l < in.lines; ++l) {
                sb::addBolt(out, {side * (in.gap / 2.0 + in.endDistance + r * in.pitch), (l - (in.lines - 1) / 2.0) * in.gauge, 0.0}, 2, u0 - 2.0, u1 + 2.0, in.db);
            }
        }
    }
}

QPair<QString, QString> coverSpliceSteelKeys()
{
    return {QStringLiteral("covFy"), QStringLiteral("covFu")};
}

// ---- emenda de tubo com chapas de extremidade ------------------------------------------------------------------------------------------------

QVector<Cat::FieldDef> tubeSpliceFields(const Cat::Entry &)
{
    QVector<Cat::FieldDef> list;
    QString group;
    auto section = [&group](const QString &title) { group = title; };
    auto add = [&](const char *k, const QString &label, double min, double max, int decimals, const QString &suffix, const QStringList &options = {}, const QString &tip = {}) {
        list.append({QString::fromLatin1(k), label, min, max, decimals, suffix, group, options, tip});
    };
    const QString mm = QObject::tr(" mm"), mpa = QObject::tr(" MPa");
    section(QObject::tr("Tubo circular"));
    add("tubeD", QObject::tr("Diâmetro externo"), 20, 1500, 1, mm);
    add("tubeT", QObject::tr("Espessura"), 2, 60, 1, mm);
    add("tubeFy", QObject::tr("fy do tubo"), 150, 800, 0, mpa);
    add("tubeFu", QObject::tr("fu do tubo"), 250, 1000, 0, mpa);
    section(QObject::tr("Chapas de extremidade"));
    add("plateD", QObject::tr("Diâmetro da chapa"), 60, 2000, 0, mm);
    add("plateT", QObject::tr("Espessura"), 6, 80, 1, mm);
    add("plateFy", QObject::tr("fy da chapa"), 150, 800, 0, mpa);
    add("plateFu", QObject::tr("fu da chapa"), 250, 1000, 0, mpa);
    section(QObject::tr("Parafusos e solda"));
    add("db", QObject::tr("Diâmetro do parafuso"), 10, 40, 1, mm);
    add("thread", QObject::tr("Plano de corte"), 0, 1, 0, QString(), {QObject::tr("Fora da rosca"), QObject::tr("Na rosca")});
    add("nb", QObject::tr("Número de parafusos"), 3, 40, 0, QString());
    add("bc", QObject::tr("Diâmetro do círculo de parafusos"), 40, 1900, 0, mm);
    add("weldLeg", QObject::tr("Perna do filete tubo–chapa"), 3, 25, 1, mm);
    add("weldFw", QObject::tr("fw (eletrodo)"), 300, 700, 0, mpa);
    return list;
}

NodeConnection tubeSpliceDefault(const Cat::Entry &, const Cat::BeamInfo &beam, double plateFy, double plateFu)
{
    NodeConnection c;
    c.grade = QString::fromStdString(D::boltGrades().size() > 2 ? D::boltGrades()[2].name : D::boltGrades().front().name);
    const double d = beam.geometry.value(QStringLiteral("D"), beam.geometry.value(QStringLiteral("d"), beam.depth > 0.0 ? beam.depth : 114.3));
    const double t = beam.geometry.value(QStringLiteral("t"), 6.0);
    const double plateD = std::ceil((d + 110.0) / 10.0) * 10.0;
    c.values = {{"tubeD", d}, {"tubeT", t}, {"tubeFy", beam.fy}, {"tubeFu", beam.fu}, {"plateD", plateD}, {"plateT", std::max(16.0, std::ceil(d / 6.0))}, {"plateFy", plateFy}, {"plateFu", plateFu},
                {"db", 20.0}, {"thread", 1.0}, {"nb", 6.0}, {"bc", d + 70.0}, {"weldLeg", std::min(12.0, std::max(5.0, std::ceil(t)))}, {"weldFw", 485.0}};
    return c;
}

Cat::Evaluation tubeSpliceEvaluate(const NodeConnection &c, const Cat::Forces &f)
{
    Cat::Evaluation e;
    QVector<ConnectionService::ForceCase> cases = f.cases;
    if (cases.isEmpty()) {
        ConnectionService::ForceCase single;
        single.name = QObject::tr("envoltória");
        single.shear = f.shear;
        single.axial = f.axial;
        single.moment = f.moment;
        cases.append(single);
    }
    double worst = -1.0;
    for (const auto &fc : cases) {
        auto checks = D::checkTubeFlange(toTube(c, fc.shear, fc.axial, fc.moment));
        const double ratio = D::worstRatio(checks);
        if (ratio > worst) {
            worst = ratio;
            e.checks = std::move(checks);
            e.governingCase = fc.name;
        }
    }
    e.parts = D::partsTubeFlange(toTube(c, 0.0, 0.0, 0.0));
    e.maxRatio = D::worstRatio(e.checks);
    if (const auto *g = D::governing(e.checks)) {
        e.governing = QString::fromStdString(g->name);
    }
    e.description = tubeSpliceDescribe(c);
    return e;
}

QString tubeSpliceDescribe(const NodeConnection &c)
{
    return QObject::tr("Tubo Ø%1 × %2 mm, 2 chapas Ø%3 × %4 mm, %5 parafusos d%6 %7")
        .arg(val(c, "tubeD"), 0, 'f', 1).arg(val(c, "tubeT"), 0, 'f', 1).arg(val(c, "plateD"), 0, 'f', 0).arg(val(c, "plateT"), 0, 'f', 1).arg(static_cast<int>(val(c, "nb") + 0.5))
        .arg(val(c, "db"), 0, 'f', 0).arg(c.grade);
}

void tubeSpliceSolids(const NodeConnection &c, bool context, std::vector<ConnectionSolids::Primitive> &out)
{
    const D::TubeFlangeInput in = toTube(c, 0.0, 0.0, 0.0);
    const double tp = in.plateThickness, reach = context ? 320.0 : 100.0;
    for (const double side : {-1.0, 1.0}) {
        out.push_back(sb::cylinder(Role::Plate, {side * tp, 0.0, 0.0}, {side * 0.0, 0.0, 0.0}, in.plateDiameter / 2.0));
        out.push_back(sb::cylinder(Role::Member, {side * tp, 0.0, 0.0}, {side * (tp + reach), 0.0, 0.0}, in.tubeDiameter / 2.0));
        out.push_back(sb::box(Role::Weld, side * (tp + in.weldLeg / 2.0), 0.0, 0.0, in.weldLeg, in.tubeDiameter + 2.0 * in.weldLeg, in.tubeDiameter + 2.0 * in.weldLeg));
    }
    const double pi = 3.14159265358979323846;
    for (int i = 0; i < in.bolts; ++i) {
        const double a = 2.0 * pi * i / in.bolts;
        sb::addBolt(out, {0.0, in.boltCircle / 2.0 * std::cos(a), in.boltCircle / 2.0 * std::sin(a)}, 0, -tp - 2.0, tp + 2.0, in.db);
    }
}

QPair<QString, QString> tubeSpliceSteelKeys()
{
    return {QStringLiteral("plateFy"), QStringLiteral("plateFu")};
}

} // namespace Structura::App::Services::catalog
