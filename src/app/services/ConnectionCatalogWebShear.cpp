#include "app/services/ConnectionCatalogInternal.h"

#include "design/WebShearConnection.h"

#include <QObject>

#include <algorithm>
#include <cmath>

namespace Structura::App::Services::catalog {

namespace D = Structura::Design::Connections;
using Structura::App::ProjectData::NodeConnection;
using Cat = ConnectionCatalog;

namespace {

D::WebShearInput toInput(const NodeConnection &c, const Cat::Forces &f)
{
    D::WebShearInput in;
    in.plates = static_cast<int>(val(c, "plates", 1.0) + 0.5);
    in.webWelded = val(c, "webWelded", 0.0) != 0.0;
    in.support = static_cast<int>(val(c, "support", 2.0) + 0.5);
    in.grade = gradeByName(c.grade);
    in.db = val(c, "db", 20.0);
    in.threadInShearPlane = val(c, "thread", 1.0) != 0.0;
    in.rows = count(c, "rows", 3.0);
    in.lines = std::clamp(count(c, "lines", 1.0), 1, 2);
    in.pitch = val(c, "pitch", 70.0);
    in.gauge = val(c, "gauge", 60.0);
    in.edgeV = val(c, "edgeV", 35.0);
    in.edgeH = val(c, "edgeH", 35.0);
    in.boltLineToSupport = val(c, "a", 60.0);
    in.plate = {val(c, "plateFy", 250.0), val(c, "plateFu", 400.0)};
    in.plateThickness = val(c, "plateT", 9.5);
    in.plateHeight = val(c, "plateH", 0.0);
    in.plateLength = val(c, "plateL", 100.0);
    in.weldLegWeb = val(c, "weldWeb", 6.0);
    in.weldLegSupport = val(c, "weldSup", 6.0);
    in.weldFw = val(c, "weldFw", 485.0);
    in.weldEnd = val(c, "weldEnd", 0.0) != 0.0;
    in.web = {val(c, "webFy", 250.0), val(c, "webFu", 400.0)};
    in.webThickness = val(c, "webT", 6.0);
    in.beamDepth = val(c, "beamD", 0.0);
    in.beamBf = val(c, "beamBf", 0.0);
    in.beamTf = val(c, "beamTf", 0.0);
    in.beamFillet = val(c, "beamR", 0.0);
    in.gap = val(c, "gap", 12.0);
    in.support_ = {val(c, "supFy", 250.0), val(c, "supFu", 400.0)};
    in.supportThickness = val(c, "supT", 12.0);
    in.supD = val(c, "supD", 0.0);
    in.supBf = val(c, "supBf", 0.0);
    in.supTf = val(c, "supTf", 0.0);
    in.supTw = val(c, "supTw", 0.0);
    in.supFillet = val(c, "supR", 0.0);
    in.copeClearance = val(c, "copeC", 15.0);
    in.shear = std::abs(f.shear);
    in.axial = f.axial;
    return in;
}

} // namespace

QVector<Cat::FieldDef> webShearFields(const Cat::Entry &entry)
{
    QVector<Cat::FieldDef> list;
    QString group;
    auto section = [&group](const QString &title) { group = title; };
    auto add = [&](const char *key, const QString &label, double min, double max, int decimals, const QString &suffix, const QStringList &options = {}, const QString &tip = {}) {
        list.append({QString::fromLatin1(key), label, min, max, decimals, suffix, group, options, tip});
    };
    const QString mm = QObject::tr(" mm"), mpa = QObject::tr(" MPa");
    const bool welded = entry.preset.value(QStringLiteral("webWelded")) != 0.0;
    const bool doublePlate = entry.preset.value(QStringLiteral("plates")) >= 2.0;
    const int support = static_cast<int>(entry.preset.value(QStringLiteral("support")) + 0.5);

    section(QObject::tr("Tala"));
    add("plateT", doublePlate ? QObject::tr("Espessura de cada tala") : QObject::tr("Espessura da tala"), 3, 50, 1, mm);
    add("plateFy", QObject::tr("fy da tala"), 150, 800, 0, mpa);
    add("plateFu", QObject::tr("fu da tala"), 250, 1000, 0, mpa);
    if (welded) {
        add("plateH", QObject::tr("Altura da tala (0 = automática)"), 0, 1500, 0, mm, {}, QObject::tr("Altura da tala sobre a alma; automática = 80 % da altura livre da alma."));
        add("plateL", QObject::tr("Comprimento soldado à alma"), 20, 600, 0, mm);
        add("weldEnd", QObject::tr("Filete vertical na extremidade"), 0, 1, 0, QString(), {QObject::tr("Não (2 filetes horizontais)"), QObject::tr("Sim (contorno em C)")});
    } else {
        section(QObject::tr("Parafusos na alma da viga"));
        add("db", QObject::tr("Diâmetro do parafuso"), 12, 40, 1, mm);
        add("thread", QObject::tr("Plano de corte"), 0, 1, 0, QString(), {QObject::tr("Fora da rosca"), QObject::tr("Na rosca")});
        add("rows", QObject::tr("Parafusos por linha"), 2, 12, 0, QString());
        add("lines", QObject::tr("Linhas de parafusos"), 1, 2, 0, QString());
        add("pitch", QObject::tr("Espaçamento vertical"), 30, 300, 0, mm);
        add("gauge", QObject::tr("Espaçamento entre linhas (g)"), 40, 300, 0, mm);
        add("edgeV", QObject::tr("Distância do furo extremo à borda (vertical)"), 15, 200, 0, mm);
        add("edgeH", QObject::tr("Distância da última linha à borda livre"), 15, 200, 0, mm);
        add("a", QObject::tr("Primeira linha de furos – face do apoio"), 30, 400, 0, mm);
    }
    section(QObject::tr("Soldas"));
    if (welded) {
        add("weldWeb", QObject::tr("Perna do filete tala–alma da viga"), 3, 25, 1, mm);
    }
    add("weldSup", QObject::tr("Perna do filete tala–apoio"), 3, 25, 1, mm);
    add("weldFw", QObject::tr("fw (eletrodo)"), 300, 700, 0, mpa);
    section(QObject::tr("Viga ligada"));
    add("beamD", QObject::tr("Altura da viga"), 80, 2000, 0, mm);
    add("beamBf", QObject::tr("Largura da mesa"), 40, 600, 0, mm);
    add("beamTf", QObject::tr("Espessura da mesa"), 3, 60, 1, mm);
    add("webT", QObject::tr("Espessura da alma"), 3, 40, 1, mm);
    add("beamR", QObject::tr("Raio de concordância alma–mesa"), 0, 40, 1, mm);
    add("webFy", QObject::tr("fy da viga"), 150, 800, 0, mpa);
    add("webFu", QObject::tr("fu da viga"), 250, 1000, 0, mpa);
    add("gap", QObject::tr("Folga entre a viga e o apoio (e)"), 0, 60, 0, mm);
    section(support == 0 ? QObject::tr("Viga de apoio (viga principal)") : (support == 1 ? QObject::tr("Pilar de apoio (ligação na alma)") : QObject::tr("Pilar de apoio (ligação na mesa)")));
    add("supT", support == 2 ? QObject::tr("Espessura da mesa onde a tala se solda") : QObject::tr("Espessura da alma onde a tala se solda"), 3, 60, 1, mm);
    add("supFy", QObject::tr("fy do apoio"), 150, 800, 0, mpa);
    add("supFu", QObject::tr("fu do apoio"), 250, 1000, 0, mpa);
    add("supD", QObject::tr("Altura do perfil de apoio"), 0, 2000, 0, mm, {}, QObject::tr("Necessária à verificação geométrica e local do apoio."));
    add("supBf", QObject::tr("Largura da mesa do apoio"), 0, 600, 0, mm);
    add("supTf", QObject::tr("Espessura da mesa do apoio"), 0, 60, 1, mm);
    add("supTw", QObject::tr("Espessura da alma do apoio"), 0, 40, 1, mm);
    add("supR", QObject::tr("Raio de concordância do apoio"), 0, 40, 1, mm);
    add("supLb", QObject::tr("Comprimento destravado do apoio (0 = contido)"), 0, 20000, 0, mm);
    add("supEnd", QObject::tr("Ligação na extremidade do apoio"), 0, 1, 0, QString(), {QObject::tr("Não (apoio contínuo)"), QObject::tr("Sim")});
    if (support == 0) {
        add("copeC", QObject::tr("Folga do recorte das mesas"), 5, 50, 0, mm);
    }
    if (!welded) {
        section(QObject::tr("Ligação por atrito (opcional)"));
        add("slip", QObject::tr("Verificar deslizamento"), 0, 2, 0, QString(), {QObject::tr("Não"), QObject::tr("Furo-padrão"), QObject::tr("Furo alargado/alongado")});
        add("slipClass", QObject::tr("Superfície"), 0, 2, 0, QString(), {QObject::tr("A"), QObject::tr("B"), QObject::tr("C")});
        add("fillers", QObject::tr("Chapas de enchimento"), 0, 1, 0, QString(), {QObject::tr("Não"), QObject::tr("Sim")});
    }
    return list;
}

NodeConnection webShearDefault(const Cat::Entry &entry, const Cat::BeamInfo &beam, double plateFy, double plateFu)
{
    NodeConnection c;
    c.grade = defaultBoltGradeName();
    const bool welded = entry.preset.value(QStringLiteral("webWelded")) != 0.0;
    const bool doublePlate = entry.preset.value(QStringLiteral("plates")) >= 2.0;
    const int support = static_cast<int>(entry.preset.value(QStringLiteral("support")) + 0.5);
    const double d = beam.depth > 0.0 ? beam.depth : 300.0;
    const double tf = beam.tf > 0.0 ? beam.tf : 10.0;
    const double r = beam.geometry.value(QStringLiteral("r"), 0.0);
    const double clear = std::max(60.0, d - 2.0 * tf - 2.0 * r);
    const double pitch = 70.0, edgeV = 35.0;
    const int rows = std::clamp(static_cast<int>(std::floor((0.9 * clear - 2.0 * edgeV) / pitch)) + 1, 2, 8);
    c.values = {{"plates", doublePlate ? 2.0 : 1.0}, {"webWelded", welded ? 1.0 : 0.0}, {"support", static_cast<double>(support)},
                {"supType", support == 2 ? 1.0 : 2.0},
                {"db", 20.0}, {"thread", 1.0}, {"rows", static_cast<double>(rows)}, {"lines", 1.0}, {"pitch", pitch}, {"gauge", 60.0}, {"edgeV", edgeV}, {"edgeH", 35.0}, {"a", 60.0},
                {"plateT", doublePlate ? 8.0 : 9.5}, {"plateFy", plateFy}, {"plateFu", plateFu}, {"plateH", 0.0}, {"plateL", 120.0}, {"weldEnd", 0.0},
                {"weldWeb", 6.0}, {"weldSup", 6.0}, {"weldFw", 485.0},
                {"beamD", d}, {"beamBf", beam.bf > 0.0 ? beam.bf : 150.0}, {"beamTf", tf}, {"webT", beam.tw > 0.0 ? beam.tw : 6.0}, {"beamR", r},
                {"webFy", beam.fy}, {"webFu", beam.fu}, {"gap", 12.0},
                {"supT", 12.0}, {"supFy", plateFy}, {"supFu", plateFu}, {"supD", 0.0}, {"supBf", 0.0}, {"supTf", 0.0}, {"supTw", 0.0}, {"supR", 0.0}, {"supLb", 0.0}, {"supEnd", 1.0},
                {"copeC", 15.0}, {"slip", 0.0}, {"slipClass", 0.0}, {"fillers", 0.0}};
    return c;
}

Cat::Evaluation webShearEvaluate(const NodeConnection &c, const Cat::Forces &f)
{
    Cat::Evaluation e;
    const D::WebShearInput in = toInput(c, f);
    e.checks = D::checkWebShear(in);
    e.parts = D::partsWebShear(in);
    const double H = D::webShearPlateHeight(in);
    const double a = D::webShearEccentricity(in);
    const double couple = H > 0.0 ? 1.5 * std::abs(f.shear) * a / (in.plates * H) : 0.0;
    ConnectionService::appendSupportLocal(e, c, f, std::max(0.5 * H, 1.0), in.plates * in.plateThickness + 2.0 * in.weldLegSupport,
                                          std::max(f.axial, 0.0) + couple, std::max(-f.axial, 0.0) + couple);
    if (!in.webWelded) {
        const int slip = static_cast<int>(val(c, "slip", 0.0) + 0.5);
        if (slip > 0) {
            D::SlipInput s;
            s.grade = in.grade;
            s.db = in.db;
            s.surfaceClass = static_cast<int>(val(c, "slipClass", 0.0) + 0.5);
            s.fillers = val(c, "fillers", 0.0) != 0.0;
            s.oversized = slip == 2;
            s.bolts = in.rows * in.lines;
            s.planes = in.plates;
            s.shear = std::abs(f.shear);
            s.tension = std::max(f.axial, 0.0);
            for (auto &check : D::checkSlip(s)) {
                e.checks.push_back(std::move(check));
            }
        }
    }
    e.maxRatio = D::worstRatio(e.checks);
    if (const auto *g = D::governing(e.checks)) {
        e.governing = QString::fromStdString(g->name);
    }
    e.description = webShearDescribe(c);
    return e;
}

QString webShearDescribe(const NodeConnection &c)
{
    const int plates = static_cast<int>(val(c, "plates", 1.0) + 0.5);
    const bool welded = val(c, "webWelded", 0.0) != 0.0;
    const QString talas = plates == 2 ? QObject::tr("2 talas t = %1 mm").arg(val(c, "plateT"), 0, 'f', 1) : QObject::tr("tala t = %1 mm").arg(val(c, "plateT"), 0, 'f', 1);
    if (welded) {
        return QObject::tr("%1 soldada(s) à alma (filete %2 mm, L = %3 mm) e ao apoio (filete %4 mm)").arg(talas).arg(val(c, "weldWeb"), 0, 'f', 0).arg(val(c, "plateL"), 0, 'f', 0).arg(val(c, "weldSup"), 0, 'f', 0);
    }
    return QObject::tr("%1, %2 parafuso(s) d%3 %4 em %5 linha(s), solda ao apoio %6 mm").arg(talas).arg(count(c, "rows", 3) * count(c, "lines", 1)).arg(val(c, "db"), 0, 'f', 0).arg(c.grade)
        .arg(count(c, "lines", 1)).arg(val(c, "weldSup"), 0, 'f', 0);
}

namespace {

/// Perfil I deitado ao longo de s, com recortes (cope) opcionais nas mesas a partir de s0.
void beamWithCopes(std::vector<Cat::Primitive> &out, double s0, double s1, double d, double bf, double tf, double tw, double copeLength, bool top, bool bottom)
{
    for (const double side : {-1.0, 1.0}) {
        const bool cut = side > 0 ? top : bottom;
        const double start = cut ? s0 + copeLength : s0;
        if (s1 > start) {
            out.push_back(sb::box(Role::Member, (start + s1) / 2.0, side * (d - tf) / 2.0, 0.0, s1 - start, tf, bf));
        }
    }
    out.push_back(sb::box(Role::Member, (s0 + s1) / 2.0, 0.0, 0.0, s1 - s0, d - 2.0 * tf, tw));
}

} // namespace

void webShearSolids(const NodeConnection &c, bool context, std::vector<Cat::Primitive> &out)
{
    
    const D::WebShearInput in = toInput(c, {});
    const int plates = in.plates;
    const double H = D::webShearPlateHeight(in);
    const double W = D::webShearPlateLength(in);
    const double tp = in.plateThickness, tw = in.webThickness;
    const double d = in.beamDepth > 0.0 ? in.beamDepth : H + 90.0, bf = in.beamBf > 0.0 ? in.beamBf : 150.0, tf = in.beamTf > 0.0 ? in.beamTf : 10.0;
    const auto cope = D::webShearCope(in);

    // apoio (referencia)
    if (in.support == D::WebShearInput::ColumnFlange) {
        const double tfc = std::max(in.supTf, in.supportThickness);
        out.push_back(sb::box(Role::Concrete, -tfc / 2.0, 0.0, 0.0, tfc, std::max(d + 120.0, H + 120.0), std::max(in.supBf, bf + 80.0)));
    } else if (in.support == D::WebShearInput::ColumnWeb) {
        const double dc = std::max(in.supD, bf + 2.0 * std::max(in.supTf, 10.0) + 40.0), tfc = std::max(in.supTf, 8.0), twc = std::max(in.supTw, in.supportThickness);
        const double bfc = std::max(in.supBf, 120.0);
        out.push_back(sb::box(Role::Concrete, -twc / 2.0, 0.0, 0.0, twc, std::max(d + 120.0, H + 120.0), dc - 2.0 * tfc));
        for (const double side : {-1.0, 1.0}) {
            out.push_back(sb::box(Role::Concrete, -twc / 2.0, 0.0, side * (dc - tfc) / 2.0, bfc, std::max(d + 120.0, H + 120.0), tfc));
        }
    } else {
        const double dm = std::max(in.supD, d + 40.0), tfm = std::max(in.supTf, 10.0), twm = std::max(in.supTw, in.supportThickness), bfm = std::max(in.supBf, 140.0);
        out.push_back(sb::box(Role::Concrete, -twm / 2.0, 0.0, 0.0, twm, dm - 2.0 * tfm, std::max(bf + 200.0, 300.0)));
        for (const double side : {-1.0, 1.0}) {
            out.push_back(sb::box(Role::Concrete, -bfm / 2.0 + twm / 2.0 - twm, side * (dm - tfm) / 2.0, 0.0, bfm, tfm, std::max(bf + 200.0, 300.0)));
        }
    }

    // talas
    const double u0 = tw / 2.0;
    for (int i = 0; i < plates; ++i) {
        const double side = plates == 1 ? 1.0 : (i == 0 ? 1.0 : -1.0);
        const double ua = side > 0 ? u0 : -u0 - tp, ub = side > 0 ? u0 + tp : -u0;
        sb::addPlate(out, 0.0, W, H, ua, ub);
        // solda tala-apoio (dois filetes verticais, um de cada lado na face do apoio, representados por um bloco)
        out.push_back(sb::box(Role::Weld, in.weldLegSupport / 2.0, 0.0, side * (u0 + tp + in.weldLegSupport / 2.0), in.weldLegSupport, H, in.weldLegSupport));
        if (in.webWelded) {
            const double lw = std::max(in.plateLength, 20.0);
            for (const double edge : {-1.0, 1.0}) {
                out.push_back(sb::box(Role::Weld, in.gap + lw / 2.0, edge * H / 2.0, side * (u0 + tp + in.weldLegWeb / 2.0), lw, in.weldLegWeb, in.weldLegWeb));
            }
        } else {
            for (int r = 0; r < in.rows; ++r) {
                for (int l = 0; l < in.lines; ++l) {
                    if (i == 0) {
                        const double ub0 = -tw / 2.0 - (plates == 2 ? tp : 0.0) - 3.0, ub1 = tw / 2.0 + tp + 3.0;
                        sb::addBolt(out, {in.boltLineToSupport + l * in.gauge, (r - (in.rows - 1) / 2.0) * in.pitch, 0.0}, 2, ub0, ub1, in.db);
                    }
                }
            }
        }
    }

    if (context) {
        const double span = 520.0;
        if (in.support == D::WebShearInput::BeamWeb && cope.length > 0.0) {
            beamWithCopes(out, in.gap, in.gap + span, d, bf, tf, tw, cope.length, true, cope.both);
        } else {
            beamWithCopes(out, in.gap, in.gap + span, d, bf, tf, tw, 0.0, false, false);
        }
    }
}

std::vector<Cat::Dim> webShearDims(const NodeConnection &c)
{
    const D::WebShearInput in = toInput(c, {});
    std::vector<Cat::Dim> dims;
    const double H = D::webShearPlateHeight(in), W = D::webShearPlateLength(in);
    auto text = [](const char *label, double v) {
        char buffer[64];
        std::snprintf(buffer, sizeof buffer, "%s = %.0f", label, v);
        for (char &ch : buffer) {
            if (ch == '.') {
                ch = ',';
            }
        }
        return std::string(buffer);
    };
    dims.push_back({W + 15.0, -H / 2.0, W + 15.0, H / 2.0, 14.0, text("H", H)});
    dims.push_back({0.0, -H / 2.0 - 15.0, W, -H / 2.0 - 15.0, -12.0, text("L", W)});
    if (!in.webWelded) {
        dims.push_back({0.0, (in.rows - 1) / 2.0 * in.pitch + 20.0, in.boltLineToSupport, (in.rows - 1) / 2.0 * in.pitch + 20.0, -10.0, text("a", in.boltLineToSupport)});
        if (in.rows > 1) {
            dims.push_back({in.boltLineToSupport - 30.0, (in.rows - 1) / 2.0 * in.pitch, in.boltLineToSupport - 30.0, (in.rows - 3) / 2.0 * in.pitch, 8.0, text("p", in.pitch)});
        }
        dims.push_back({W - in.edgeH, -(in.rows - 1) / 2.0 * in.pitch, W, -(in.rows - 1) / 2.0 * in.pitch, 8.0, text("eh", in.edgeH)});
    } else {
        dims.push_back({in.gap, H / 2.0 + 12.0, in.gap + std::max(in.plateLength, 20.0), H / 2.0 + 12.0, -10.0, text("Lw", std::max(in.plateLength, 20.0))});
    }
    return dims;
}

std::vector<std::string> webShearNotes(const NodeConnection &c)
{
    const D::WebShearInput in = toInput(c, {});
    std::vector<std::string> notes;
    char buffer[220];
    std::snprintf(buffer, sizeof buffer, "%s DE ALMA t = %.1f mm; SOLDA TALA-APOIO: FILETE %.0f mm (2 FILETES POR TALA).", in.plates == 2 ? "DUPLA TALA" : "TALA SIMPLES", in.plateThickness, in.weldLegSupport);
    notes.push_back(buffer);
    if (in.webWelded) {
        std::snprintf(buffer, sizeof buffer, "TALA SOLDADA A ALMA DA VIGA: FILETE %.0f mm, COMPRIMENTO %.0f mm%s.", in.weldLegWeb, in.plateLength, in.weldEnd ? " COM CONTORNO EM C" : "");
    } else {
        std::snprintf(buffer, sizeof buffer, "PARAFUSOS d = %.0f mm %s: %d x %d; FUROS PADRÃO.", in.db, c.grade.toUtf8().constData(), in.rows, in.lines);
    }
    notes.push_back(buffer);
    const auto cope = D::webShearCope(in);
    if (cope.length > 0.0) {
        std::snprintf(buffer, sizeof buffer, "RECORTE NA(S) MESA(S) DA VIGA LIGADA: %.0f x %.0f mm (%s).", cope.length, cope.depth, cope.both ? "MESA SUPERIOR E INFERIOR" : "MESA SUPERIOR");
        notes.push_back(buffer);
    }
    notes.push_back(in.support == D::WebShearInput::ColumnWeb ? "LIGAÇÃO NA ALMA DO PILAR: MESAS DA VIGA ENTRE AS MESAS DO PILAR (VER FOLGAS)." : in.support == D::WebShearInput::ColumnFlange ? "LIGAÇÃO NA MESA DO PILAR." : "LIGAÇÃO NA ALMA DE VIGA PRINCIPAL.");
    return notes;
}

QPair<QString, QString> webShearSteelKeys()
{
    return {QStringLiteral("plateFy"), QStringLiteral("plateFu")};
}

} // namespace Structura::App::Services::catalog
