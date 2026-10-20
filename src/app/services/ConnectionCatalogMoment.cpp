#include "app/services/ConnectionCatalogInternal.h"

#include "design/MomentConnection.h"

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

D::MomentConnectionInput toInput(const NodeConnection &c, double shear, double axial, double moment, double panelShear = -1.0, double columnAxial = 0.0)
{
    D::MomentConnectionInput in;
    in.flangeMode = static_cast<int>(val(c, "flangeMode", 0.0) + 0.5);
    in.webMode = static_cast<int>(val(c, "webMode", 1.0) + 0.5);
    in.grade = gradeByName(c.grade);
    in.db = val(c, "db", 20.0);
    in.threadInShearPlane = val(c, "thread", 1.0) != 0.0;
    in.beamDepth = val(c, "beamD", 300.0);
    in.beamBf = val(c, "beamBf", 150.0);
    in.beamTf = val(c, "beamTf", 10.0);
    in.beamTw = val(c, "webT", 6.0);
    in.beam = {val(c, "webFy", 250.0), val(c, "webFu", 400.0)};
    in.flangeFillet = val(c, "flFillet", 0.0) != 0.0;
    in.flangeWeldLeg = val(c, "flWeld", 8.0);
    in.weldFw = val(c, "weldFw", 485.0);
    in.webWeldLeg = val(c, "webWeldLeg", 6.0);
    in.stubStemThickness = val(c, "stT", 12.5);
    in.stubFlangeThickness = val(c, "stF", 16.0);
    in.stubWidth = val(c, "stW", 150.0);
    in.stub = {val(c, "plateFy", 250.0), val(c, "plateFu", 400.0)};
    in.stubColumnBoltsPerRow = std::max(1, static_cast<int>(val(c, "stCols", 2.0) + 0.5));
    in.stubColumnRows = std::max(1, static_cast<int>(val(c, "stRows", 1.0) + 0.5));
    in.stubColumnPitch = val(c, "stPitch", 70.0);
    in.stubGauge = val(c, "stGauge", 90.0);
    in.stubB = val(c, "stB", 40.0);
    in.stubA = val(c, "stA", 35.0);
    in.stubBeamBolts = std::max(2, static_cast<int>(val(c, "stBeam", 4.0) + 0.5));
    in.stubBeamPitch = val(c, "stBeamP", 70.0);
    in.stubBeamEnd = val(c, "stBeamE", 40.0);
    in.stubBeamGauge = val(c, "stBeamG", 60.0);
    // tala(s) de alma: mesmas chaves da familia de cortante
    D::WebShearInput &w = in.web;
    w.plates = in.webMode == D::MomentConnectionInput::DoublePlate ? 2 : 1;
    w.webWelded = val(c, "webWelded", 0.0) != 0.0;
    w.support = D::WebShearInput::ColumnFlange;
    w.grade = in.grade;
    w.db = in.db;
    w.threadInShearPlane = in.threadInShearPlane;
    w.rows = count(c, "rows", 3.0);
    w.lines = std::clamp(count(c, "lines", 1.0), 1, 2);
    w.pitch = val(c, "pitch", 70.0);
    w.gauge = val(c, "gauge", 60.0);
    w.edgeV = val(c, "edgeV", 35.0);
    w.edgeH = val(c, "edgeH", 35.0);
    w.boltLineToSupport = val(c, "a", 60.0);
    w.plate = {val(c, "plateFy", 250.0), val(c, "plateFu", 400.0)};
    w.plateThickness = val(c, "plateT", 9.5);
    w.plateHeight = val(c, "plateH", 0.0);
    w.plateLength = val(c, "plateL", 120.0);
    w.weldLegWeb = val(c, "weldWeb", 6.0);
    w.weldLegSupport = val(c, "weldSup", 6.0);
    w.weldFw = in.weldFw;
    w.web = in.beam;
    w.webThickness = in.beamTw;
    w.beamDepth = in.beamDepth;
    w.beamBf = in.beamBf;
    w.beamTf = in.beamTf;
    w.beamFillet = val(c, "beamR", 0.0);
    w.gap = val(c, "gap", 12.0);
    w.support_ = {val(c, "supFy", 250.0), val(c, "supFu", 400.0)};
    w.supportThickness = std::max(val(c, "supTf", 12.0), 6.0);
    w.supD = val(c, "supD", 0.0);
    w.supBf = val(c, "supBf", 0.0);
    w.supTf = val(c, "supTf", 0.0);
    w.supTw = val(c, "supTw", 0.0);
    w.supFillet = val(c, "supR", 0.0);
    in.colDepth = val(c, "supD", 260.0) > 0.0 ? val(c, "supD", 260.0) : 260.0;
    in.colBf = val(c, "supBf", 146.0) > 0.0 ? val(c, "supBf", 146.0) : 146.0;
    in.colTf = val(c, "supTf", 12.7) > 0.0 ? val(c, "supTf", 12.7) : 12.7;
    in.colTw = val(c, "supTw", 7.6) > 0.0 ? val(c, "supTw", 7.6) : 7.6;
    in.colRadius = val(c, "supR", 10.0);
    in.column = {val(c, "supFy", 250.0), val(c, "supFu", 400.0)};
    in.colUnbraced = val(c, "supLb", 0.0);
    in.doubler = val(c, "doubler", 0.0);
    in.panelStiffened = val(c, "panelStiff", 0.0) != 0.0;
    in.colAxial = val(c, "colN", 0.0) * 1000.0;
    in.colShear = val(c, "colV", 0.0) * 1000.0;
    in.shear = shear;
    in.axial = axial;
    in.moment = moment;
    in.panelShear = panelShear;
    if (panelShear >= 0.0) {
        in.colAxial = columnAxial;
    }
    return in;
}

} // namespace

QVector<Cat::FieldDef> momentFields(const Cat::Entry &entry)
{
    QVector<Cat::FieldDef> list;
    QString group;
    auto section = [&group](const QString &title) { group = title; };
    auto add = [&](const char *k, const QString &label, double min, double max, int decimals, const QString &suffix, const QStringList &options = {}, const QString &tip = {}) {
        list.append({QString::fromLatin1(k), label, min, max, decimals, suffix, group, options, tip});
    };
    const QString mm = QObject::tr(" mm"), mpa = QObject::tr(" MPa");
    const int flangeMode = static_cast<int>(entry.preset.value(QStringLiteral("flangeMode")) + 0.5);
    const int webMode = static_cast<int>(entry.preset.value(QStringLiteral("webMode")) + 0.5);
    section(QObject::tr("Mesas da viga"));
    if (flangeMode == 0) {
        add("flFillet", QObject::tr("Solda das mesas"), 0, 1, 0, QString(), {QObject::tr("Penetração total"), QObject::tr("Filetes (dois lados)")});
        add("flWeld", QObject::tr("Perna do filete das mesas"), 3, 30, 1, mm);
    } else {
        section(QObject::tr("Perfis T (um em cada mesa)"));
        add("stF", QObject::tr("Espessura da aba do T (lado do pilar)"), 6, 60, 1, mm);
        add("stT", QObject::tr("Espessura da alma do T"), 6, 60, 1, mm);
        add("stW", QObject::tr("Largura do T"), 60, 500, 0, mm);
        add("plateFy", QObject::tr("fy do T"), 150, 800, 0, mpa);
        add("plateFu", QObject::tr("fu do T"), 250, 1000, 0, mpa);
        add("stCols", QObject::tr("Parafusos por fileira (aba)"), 1, 4, 0, QString());
        add("stRows", QObject::tr("Fileiras de cada lado da alma do T"), 1, 4, 0, QString());
        add("stPitch", QObject::tr("Espaçamento das fileiras"), 40, 300, 0, mm);
        add("stGauge", QObject::tr("Gabarito dos parafusos da aba"), 40, 400, 0, mm);
        add("stB", QObject::tr("Parafuso – face da alma do T (b)"), 15, 200, 0, mm);
        add("stA", QObject::tr("Parafuso – borda da aba (a)"), 15, 200, 0, mm);
        add("stBeam", QObject::tr("Parafusos do T na mesa da viga"), 2, 16, 0, QString());
        add("stBeamP", QObject::tr("Espaçamento (mesa da viga)"), 40, 300, 0, mm);
        add("stBeamE", QObject::tr("Distância à extremidade do T"), 20, 200, 0, mm);
        add("stBeamG", QObject::tr("Entre colunas de parafusos (mesa da viga)"), 40, 400, 0, mm);
    }
    section(QObject::tr("Alma da viga"));
    if (webMode == 0) {
        add("webWeldLeg", QObject::tr("Perna do filete da alma"), 3, 20, 1, mm);
    } else {
        add("plateT", QObject::tr("Espessura da tala de alma"), 3, 40, 1, mm);
        add("plateFy", QObject::tr("fy da tala"), 150, 800, 0, mpa);
        add("plateFu", QObject::tr("fu da tala"), 250, 1000, 0, mpa);
        add("rows", QObject::tr("Parafusos por linha"), 2, 12, 0, QString());
        add("lines", QObject::tr("Linhas de parafusos"), 1, 2, 0, QString());
        add("pitch", QObject::tr("Espaçamento vertical"), 30, 300, 0, mm);
        add("gauge", QObject::tr("Entre linhas"), 40, 300, 0, mm);
        add("edgeV", QObject::tr("Furo extremo – borda (vertical)"), 15, 200, 0, mm);
        add("edgeH", QObject::tr("Última linha – borda livre"), 15, 200, 0, mm);
        add("a", QObject::tr("Primeira linha – face do pilar"), 30, 400, 0, mm);
        add("weldSup", QObject::tr("Perna do filete tala–pilar"), 3, 25, 1, mm);
        add("db", QObject::tr("Diâmetro do parafuso"), 10, 40, 1, mm);
        add("thread", QObject::tr("Plano de corte"), 0, 1, 0, QString(), {QObject::tr("Fora da rosca"), QObject::tr("Na rosca")});
    }
    add("weldFw", QObject::tr("fw (eletrodo)"), 300, 700, 0, mpa);
    section(QObject::tr("Viga ligada"));
    add("beamD", QObject::tr("Altura da viga"), 80, 2000, 0, mm);
    add("beamBf", QObject::tr("Largura da mesa"), 40, 600, 0, mm);
    add("beamTf", QObject::tr("Espessura da mesa"), 3, 60, 1, mm);
    add("webT", QObject::tr("Espessura da alma"), 3, 40, 1, mm);
    add("beamR", QObject::tr("Raio de concordância"), 0, 40, 1, mm);
    add("webFy", QObject::tr("fy da viga"), 150, 800, 0, mpa);
    add("webFu", QObject::tr("fu da viga"), 250, 1000, 0, mpa);
    add("gap", QObject::tr("Folga viga–pilar"), 0, 60, 0, mm);
    section(QObject::tr("Pilar (reforço do painel)"));
    add("doubler", QObject::tr("Chapa de reforço da alma (soma dos dois lados)"), 0, 60, 1, mm);
    add("panelStiff", QObject::tr("Enrijecedores transversais alinhados às mesas da viga"), 0, 1, 0, QString(), {QObject::tr("Não"), QObject::tr("Sim")});
    add("colN", QObject::tr("Força axial no pilar (se o nó não for calculado)"), 0, 100000, 1, QObject::tr(" kN"), {}, QObject::tr("Usada só quando os esforços do nó não estão disponíveis; com a análise, o programa obtém NSd e o cortante do pilar do equilíbrio do nó."));
    add("colV", QObject::tr("Cortante do pilar (se o nó não for calculado)"), 0, 100000, 1, QObject::tr(" kN"), {}, QObject::tr("Usado só quando os esforços do nó não estão disponíveis (equilíbrio do nó automático quando há análise)."));
    add("supLb", QObject::tr("Comprimento destravado do pilar (0 = contido)"), 0, 20000, 0, mm);
    return list;
}

NodeConnection momentDefault(const Cat::Entry &entry, const Cat::BeamInfo &beam, double plateFy, double plateFu)
{
    NodeConnection c;
    c.grade = QString::fromStdString(D::boltGrades().size() > 2 ? D::boltGrades()[2].name : D::boltGrades().front().name);
    const double d = beam.depth > 0.0 ? beam.depth : 300.0;
    const double bf = beam.bf > 0.0 ? beam.bf : 150.0;
    const double tf = beam.tf > 0.0 ? beam.tf : 10.0;
    const double r = beam.geometry.value(QStringLiteral("r"), 0.0);
    const double clear = std::max(60.0, d - 2.0 * tf - 2.0 * r);
    const int rows = std::clamp(static_cast<int>(std::floor((0.9 * clear - 70.0) / 70.0)) + 1, 2, 8);
    const int webMode = static_cast<int>(entry.preset.value(QStringLiteral("webMode")) + 0.5);
    c.values = {{"flangeMode", entry.preset.value(QStringLiteral("flangeMode"))}, {"webMode", static_cast<double>(webMode)}, {"supType", 1.0},
                {"flFillet", entry.preset.value(QStringLiteral("flFillet"), 0.0)}, {"flWeld", std::clamp(std::round(tf * 0.8), 5.0, 14.0)}, {"weldFw", 485.0}, {"webWeldLeg", 6.0},
                {"stF", 16.0}, {"stT", 12.5}, {"stW", std::round(bf)}, {"stCols", 2.0}, {"stRows", 1.0}, {"stPitch", 70.0}, {"stGauge", std::max(60.0, std::round(bf * 0.6))}, {"stB", 40.0}, {"stA", 35.0},
                {"stBeam", 4.0}, {"stBeamP", 70.0}, {"stBeamE", 40.0}, {"stBeamG", 60.0}, {"panelStiff", 0.0}, {"colN", 0.0}, {"colV", 0.0},
                {"db", 20.0}, {"thread", 1.0}, {"rows", static_cast<double>(rows)}, {"lines", 1.0}, {"pitch", 70.0}, {"gauge", 60.0}, {"edgeV", 35.0}, {"edgeH", 35.0}, {"a", 60.0},
                {"plateT", webMode == 2 ? 8.0 : 9.5}, {"plateFy", plateFy}, {"plateFu", plateFu}, {"plateH", 0.0}, {"plateL", 120.0}, {"weldSup", 6.0}, {"weldWeb", 6.0}, {"webWelded", 0.0},
                {"beamD", d}, {"beamBf", bf}, {"beamTf", tf}, {"webT", beam.tw > 0.0 ? beam.tw : 6.0}, {"beamR", r}, {"webFy", beam.fy}, {"webFu", beam.fu}, {"gap", 12.0},
                {"supD", 0.0}, {"supBf", 0.0}, {"supTf", 0.0}, {"supTw", 0.0}, {"supR", 0.0}, {"supFy", plateFy}, {"supFu", plateFu}, {"supEnd", 0.0}, {"supLb", 0.0}, {"doubler", 0.0}};
    return c;
}

Cat::Evaluation momentEvaluate(const NodeConnection &c, const Cat::Forces &f)
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
        auto checks = D::checkMomentConnection(toInput(c, fc.shear, fc.axial, fc.moment, fc.panelShear, fc.columnAxial));
        const double ratio = D::worstRatio(checks);
        if (ratio > worst) {
            worst = ratio;
            e.checks = std::move(checks);
            e.governingCase = fc.name;
        }
    }
    e.parts = D::partsMomentConnection(toInput(c, 0.0, 0.0, 0.0));
    e.maxRatio = D::worstRatio(e.checks);
    if (const auto *g = D::governing(e.checks)) {
        e.governing = QString::fromStdString(g->name);
    }
    e.description = momentDescribe(c);
    return e;
}

QString momentDescribe(const NodeConnection &c)
{
    const int flangeMode = static_cast<int>(val(c, "flangeMode") + 0.5), webMode = static_cast<int>(val(c, "webMode") + 0.5);
    const QString flange = flangeMode == 1 ? QObject::tr("perfis T parafusados nas mesas") : (val(c, "flFillet") != 0.0 ? QObject::tr("mesas soldadas por filetes") : QObject::tr("mesas soldadas (penetração total)"));
    const QString web = webMode == 0 ? QObject::tr("alma soldada ao pilar") : (webMode == 1 ? QObject::tr("tala simples de alma t = %1 mm").arg(val(c, "plateT"), 0, 'f', 1) : QObject::tr("dupla tala de alma t = %1 mm").arg(val(c, "plateT"), 0, 'f', 1));
    return QObject::tr("Ligação rígida: %1; %2").arg(flange, web);
}

void momentSolids(const NodeConnection &c, bool context, std::vector<ConnectionSolids::Primitive> &out)
{
    const D::MomentConnectionInput in = toInput(c, 0.0, 0.0, 0.0);
    const double d = in.beamDepth, bf = in.beamBf, tf = in.beamTf, tw = in.beamTw;
    const double gap = val(c, "gap", 12.0), reach = context ? 420.0 : 160.0;
    // pilar: mesa de face
    const double colTf = std::max(in.colTf, 8.0);
    out.push_back(sb::box(Role::Concrete, -colTf / 2.0, 0.0, 0.0, colTf, d + 240.0, std::max(in.colBf, bf + 60.0)));
    // viga
    for (const double side : {-1.0, 1.0}) {
        out.push_back(sb::box(Role::Member, gap + reach / 2.0, side * (d - tf) / 2.0, 0.0, reach, tf, bf));
    }
    out.push_back(sb::box(Role::Member, gap + reach / 2.0, 0.0, 0.0, reach, d - 2.0 * tf, tw));
    // mesas
    if (in.flangeMode == D::MomentConnectionInput::Welded) {
        for (const double side : {-1.0, 1.0}) {
            out.push_back(sb::box(Role::Weld, gap / 2.0, side * (d / 2.0 - tf / 2.0), 0.0, gap + 4.0, tf + 2.0 * in.flangeWeldLeg, bf + 2.0 * in.flangeWeldLeg));
        }
    } else {
        const double stF = in.stubFlangeThickness, stT = in.stubStemThickness, stW = in.stubWidth, stemLen = 40.0 + (in.stubBeamBolts / 2) * in.stubBeamPitch + 40.0;
        const double abaH = stT + 2.0 * (in.stubB + (in.stubColumnRows - 1) * in.stubColumnPitch + in.stubA);
        for (const double side : {-1.0, 1.0}) {
            const double tStem = side * (d / 2.0 + stT / 2.0);
            // aba contra o pilar (altura cobre as fileiras de parafusos dos dois lados da alma) e alma sobre a mesa da viga
            out.push_back(sb::box(Role::Plate, stF / 2.0, tStem, 0.0, stF, abaH, stW));
            out.push_back(sb::box(Role::Plate, stF + stemLen / 2.0, tStem, 0.0, stemLen, stT, stW));
            for (int r = 0; r < in.stubColumnRows; ++r) {
                for (int k = 0; k < in.stubColumnBoltsPerRow; ++k) {
                    for (const double lane : {-1.0, 1.0}) {
                        const double tpos = tStem + lane * (in.stubB + stT / 2.0 + r * in.stubColumnPitch);
                        sb::addBolt(out, {0.0, tpos, (k - (in.stubColumnBoltsPerRow - 1) / 2.0) * in.stubGauge}, 0, -colTf - 2.0, stF + 2.0, in.db);
                    }
                }
            }
            for (int b = 0; b < in.stubBeamBolts / 2; ++b) {
                for (const double col : {-1.0, 1.0}) {
                    sb::addBolt(out, {stF + in.stubBeamEnd + b * in.stubBeamPitch, 0.0, col * 40.0}, 1, side * (d / 2.0 - tf - 2.0), side * (d / 2.0 + stT + 2.0), in.db);
                }
            }
        }
    }
    // alma
    if (in.webMode == D::MomentConnectionInput::DirectWeld) {
        for (const double side : {-1.0, 1.0}) {
            out.push_back(sb::box(Role::Weld, gap / 2.0, 0.0, side * (tw / 2.0 + in.webWeldLeg / 2.0), gap + 4.0, d - 2.0 * tf, in.webWeldLeg));
        }
    } else {
        const D::WebShearInput &w = in.web;
        const double H = D::webShearPlateHeight(w), W = D::webShearPlateLength(w), tp = w.plateThickness;
        for (int i = 0; i < w.plates; ++i) {
            const double side = w.plates == 1 ? 1.0 : (i == 0 ? 1.0 : -1.0);
            sb::addPlate(out, 0.0, W, H, side > 0 ? tw / 2.0 : -tw / 2.0 - tp, side > 0 ? tw / 2.0 + tp : -tw / 2.0);
            out.push_back(sb::box(Role::Weld, w.weldLegSupport / 2.0, 0.0, side * (tw / 2.0 + tp + w.weldLegSupport / 2.0), w.weldLegSupport, H, w.weldLegSupport));
        }
        if (!w.webWelded) {
            for (int r = 0; r < w.rows; ++r) {
                for (int l = 0; l < w.lines; ++l) {
                    sb::addBolt(out, {w.boltLineToSupport + l * w.gauge, (r - (w.rows - 1) / 2.0) * w.pitch, 0.0}, 2, -tw / 2.0 - (w.plates == 2 ? tp : 0.0) - 3.0, tw / 2.0 + tp + 3.0, w.db);
                }
            }
        }
    }
}

std::vector<std::string> momentNotes(const NodeConnection &c)
{
    const D::MomentConnectionInput in = toInput(c, 0.0, 0.0, 0.0);
    std::vector<std::string> notes;
    char buffer[220];
    if (in.flangeMode == D::MomentConnectionInput::Welded) {
        std::snprintf(buffer, sizeof buffer, "MESAS DA VIGA: %s.", in.flangeFillet ? "FILETES NOS DOIS LADOS" : "SOLDA DE PENETRAÇÃO TOTAL (CJP)");
    } else {
        std::snprintf(buffer, sizeof buffer, "MESAS: PERFIS T PARAFUSADOS (ABA %.1f mm, ALMA %.1f mm).", in.stubFlangeThickness, in.stubStemThickness);
    }
    notes.push_back(buffer);
    notes.push_back(in.webMode == 0 ? "ALMA: FILETES DIRETO NO PILAR." : (in.webMode == 1 ? "ALMA: TALA SIMPLES SOLDADA AO PILAR E PARAFUSADA NA ALMA." : "ALMA: DUPLA TALA SOLDADA AO PILAR E PARAFUSADA NA ALMA."));
    return notes;
}

QPair<QString, QString> momentSteelKeys()
{
    return {QStringLiteral("plateFy"), QStringLiteral("plateFu")};
}

} // namespace Structura::App::Services::catalog
