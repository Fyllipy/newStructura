#include "app/services/ConnectionCatalogInternal.h"

#include "design/FlexEndPlate.h"

#include <QObject>

#include <algorithm>
#include <cmath>

namespace Structura::App::Services::catalog {

namespace D = Structura::Design::Connections;
using Structura::App::ProjectData::NodeConnection;
using Cat = ConnectionCatalog;

namespace {

D::FlexEndPlateInput toInput(const NodeConnection &c, double shear, double axial)
{
    D::FlexEndPlateInput in;
    in.support = static_cast<int>(val(c, "support", 2.0) + 0.5);
    in.grade = gradeByName(c.grade);
    in.db = val(c, "db", 20.0);
    in.threadInShearPlane = val(c, "thread", 1.0) != 0.0;
    in.rows = count(c, "rows", 3.0);
    in.pitch = val(c, "pitch", 70.0);
    in.gauge = val(c, "gauge", 90.0);
    in.edgeV = val(c, "edgeV", 35.0);
    in.edgeH = val(c, "edgeH", 35.0);
    in.plate = {val(c, "plateFy", 250.0), val(c, "plateFu", 400.0)};
    in.plateThickness = val(c, "plateT", 9.5);
    in.plateHeight = val(c, "plateH", 0.0);
    in.weldLeg = val(c, "weldWeb", 6.0);
    in.weldFw = val(c, "weldFw", 485.0);
    in.web = {val(c, "webFy", 250.0), val(c, "webFu", 400.0)};
    in.webThickness = val(c, "webT", 6.0);
    in.beamDepth = val(c, "beamD", 0.0);
    in.beamTf = val(c, "beamTf", 0.0);
    in.beamFillet = val(c, "beamR", 0.0);
    in.gap = val(c, "gap", 12.0);
    in.support_ = {val(c, "supFy", 250.0), val(c, "supFu", 400.0)};
    in.supportThickness = val(c, "supT", 12.0);
    in.shear = std::abs(shear);
    in.axial = axial;
    return in;
}

} // namespace

QVector<Cat::FieldDef> flexEndPlateFields(const Cat::Entry &entry)
{
    QVector<Cat::FieldDef> list;
    QString group;
    auto section = [&group](const QString &title) { group = title; };
    auto add = [&](const char *key, const QString &label, double min, double max, int decimals, const QString &suffix, const QStringList &options = {}, const QString &tip = {}) {
        list.append({QString::fromLatin1(key), label, min, max, decimals, suffix, group, options, tip});
    };
    const QString mm = QObject::tr(" mm"), mpa = QObject::tr(" MPa");
    const int support = static_cast<int>(entry.preset.value(QStringLiteral("support")) + 0.5);
    section(QObject::tr("Chapa de extremidade"));
    add("plateT", QObject::tr("Espessura da chapa"), 3, 50, 1, mm);
    add("plateFy", QObject::tr("fy da chapa"), 150, 800, 0, mpa);
    add("plateFu", QObject::tr("fu da chapa"), 250, 1000, 0, mpa);
    add("plateH", QObject::tr("Altura da chapa (0 = automática)"), 0, 1500, 0, mm, {}, QObject::tr("Altura parcial, limitada ao trecho reto da alma da viga; automática = (n − 1) p + 2 ev."));
    section(QObject::tr("Parafusos ao apoio (duas colunas simétricas em relação à alma)"));
    add("db", QObject::tr("Diâmetro do parafuso"), 12, 40, 1, mm);
    add("thread", QObject::tr("Plano de corte"), 0, 1, 0, QString(), {QObject::tr("Fora da rosca"), QObject::tr("Na rosca")});
    add("rows", QObject::tr("Parafusos por coluna"), 2, 12, 0, QString());
    add("pitch", QObject::tr("Espaçamento vertical"), 30, 300, 0, mm);
    add("gauge", QObject::tr("Entre as duas colunas (g)"), 40, 400, 0, mm);
    add("edgeV", QObject::tr("Furo extremo – borda superior/inferior"), 15, 200, 0, mm);
    add("edgeH", QObject::tr("Coluna de parafusos – borda lateral"), 15, 200, 0, mm);
    section(QObject::tr("Soldas"));
    add("weldWeb", QObject::tr("Perna dos filetes chapa–alma (dois lados)"), 3, 25, 1, mm);
    add("weldFw", QObject::tr("fw (eletrodo)"), 300, 700, 0, mpa);
    section(QObject::tr("Viga ligada"));
    add("beamD", QObject::tr("Altura da viga"), 80, 2000, 0, mm);
    add("beamBf", QObject::tr("Largura da mesa"), 40, 600, 0, mm);
    add("beamTf", QObject::tr("Espessura da mesa"), 3, 60, 1, mm);
    add("webT", QObject::tr("Espessura da alma"), 3, 40, 1, mm);
    add("beamR", QObject::tr("Raio de concordância alma–mesa"), 0, 40, 1, mm);
    add("webFy", QObject::tr("fy da viga"), 150, 800, 0, mpa);
    add("webFu", QObject::tr("fu da viga"), 250, 1000, 0, mpa);
    add("gap", QObject::tr("Folga entre a viga e o apoio"), 0, 60, 0, mm);
    section(support == 1 ? QObject::tr("Pilar de apoio (ligação na alma)") : QObject::tr("Pilar de apoio (ligação na mesa)"));
    add("supT", support == 1 ? QObject::tr("Espessura da alma do pilar") : QObject::tr("Espessura da mesa do pilar"), 3, 60, 1, mm);
    add("supFy", QObject::tr("fy do apoio"), 150, 800, 0, mpa);
    add("supFu", QObject::tr("fu do apoio"), 250, 1000, 0, mpa);
    add("supD", QObject::tr("Altura do perfil de apoio"), 0, 2000, 0, mm);
    add("supBf", QObject::tr("Largura da mesa do apoio"), 0, 600, 0, mm);
    add("supTf", QObject::tr("Espessura da mesa do apoio"), 0, 60, 1, mm);
    add("supTw", QObject::tr("Espessura da alma do apoio"), 0, 40, 1, mm);
    add("supR", QObject::tr("Raio de concordância do apoio"), 0, 40, 1, mm);
    add("supLb", QObject::tr("Comprimento destravado do apoio (0 = contido)"), 0, 20000, 0, mm);
    add("supEnd", QObject::tr("Ligação na extremidade do apoio"), 0, 1, 0, QString(), {QObject::tr("Não (apoio contínuo)"), QObject::tr("Sim")});
    return list;
}

NodeConnection flexEndPlateDefault(const Cat::Entry &entry, const Cat::BeamInfo &beam, double plateFy, double plateFu)
{
    NodeConnection c;
    c.grade = defaultBoltGradeName();
    const int support = static_cast<int>(entry.preset.value(QStringLiteral("support"), 2.0) + 0.5);
    const double d = beam.depth > 0.0 ? beam.depth : 300.0;
    const double tf = beam.tf > 0.0 ? beam.tf : 10.0;
    const double r = beam.geometry.value(QStringLiteral("r"), 0.0);
    const double clear = std::max(60.0, d - 2.0 * tf - 2.0 * r);
    const double pitch = 70.0, edgeV = 35.0;
    const int rows = std::clamp(static_cast<int>(std::floor((0.9 * clear - 2.0 * edgeV) / pitch)) + 1, 2, 8);
    const double tw = beam.tw > 0.0 ? beam.tw : 6.0;
    c.values = {{"support", static_cast<double>(support)}, {"supType", support == 2 ? 1.0 : 2.0},
                {"db", 20.0}, {"thread", 1.0}, {"rows", static_cast<double>(rows)}, {"pitch", pitch}, {"gauge", std::max(90.0, std::round(tw * 10.0 + 60.0))}, {"edgeV", edgeV}, {"edgeH", 35.0},
                {"plateT", 9.5}, {"plateFy", plateFy}, {"plateFu", plateFu}, {"plateH", 0.0}, {"weldWeb", 6.0}, {"weldFw", 485.0},
                {"beamD", d}, {"beamBf", beam.bf > 0.0 ? beam.bf : 150.0}, {"beamTf", tf}, {"webT", tw}, {"beamR", r}, {"webFy", beam.fy}, {"webFu", beam.fu}, {"gap", 12.0},
                {"supT", 12.0}, {"supFy", plateFy}, {"supFu", plateFu}, {"supD", 0.0}, {"supBf", 0.0}, {"supTf", 0.0}, {"supTw", 0.0}, {"supR", 0.0}, {"supLb", 0.0}, {"supEnd", 1.0}};
    return c;
}

Cat::Evaluation flexEndPlateEvaluate(const NodeConnection &c, const Cat::Forces &f)
{
    Cat::Evaluation e;
    const D::FlexEndPlateInput in = toInput(c, f.shear, f.axial);
    e.checks = D::checkFlexEndPlate(in);
    e.parts = D::partsFlexEndPlate(in);
    const double hp = D::flexEndPlateHeight(in);
    ConnectionService::appendSupportLocal(e, c, f, std::max(0.5 * hp, 1.0), in.plateThickness + 2.0 * in.weldLeg, std::max(f.axial, 0.0), std::max(-f.axial, 0.0));
    e.maxRatio = D::worstRatio(e.checks);
    if (const auto *g = D::governing(e.checks)) {
        e.governing = QString::fromStdString(g->name);
    }
    e.description = flexEndPlateDescribe(c);
    return e;
}

QString flexEndPlateDescribe(const NodeConnection &c)
{
    return QObject::tr("Chapa de extremidade flexível t = %1 mm, %2 parafusos d%3 %4 em duas colunas, filete %5 mm à alma")
        .arg(val(c, "plateT"), 0, 'f', 1).arg(2 * count(c, "rows", 3)).arg(val(c, "db"), 0, 'f', 0).arg(c.grade).arg(val(c, "weldWeb"), 0, 'f', 0);
}

void flexEndPlateSolids(const NodeConnection &c, bool context, std::vector<Cat::Primitive> &out)
{
    const D::FlexEndPlateInput in = toInput(c, 0.0, 0.0);
    const double hp = D::flexEndPlateHeight(in), wp = D::flexEndPlateWidth(in), tp = in.plateThickness, tw = in.webThickness;
    const double d = in.beamDepth > 0.0 ? in.beamDepth : hp + 90.0, bf = std::max(val(c, "beamBf", 150.0), 40.0), tf = in.beamTf > 0.0 ? in.beamTf : 10.0;
    const double gap = in.gap;
    // apoio
    if (in.support == D::WebShearInput::ColumnWeb) {
        const double dc = std::max(val(c, "supD", 0.0), bf + 2.0 * std::max(val(c, "supTf", 0.0), 10.0) + 40.0), tfc = std::max(val(c, "supTf", 0.0), 8.0), twc = std::max(in.supportThickness, 8.0);
        const double bfc = std::max(val(c, "supBf", 0.0), 120.0);
        out.push_back(sb::box(Role::Concrete, -twc / 2.0, 0.0, 0.0, twc, std::max(d + 120.0, hp + 120.0), dc - 2.0 * tfc));
        for (const double side : {-1.0, 1.0}) {
            out.push_back(sb::box(Role::Concrete, -twc / 2.0, 0.0, side * (dc - tfc) / 2.0, bfc, std::max(d + 120.0, hp + 120.0), tfc));
        }
    } else {
        const double tfc = std::max(in.supportThickness, 8.0);
        out.push_back(sb::box(Role::Concrete, -tfc / 2.0, 0.0, 0.0, tfc, std::max(d + 120.0, hp + 120.0), std::max(val(c, "supBf", 0.0), bf + 80.0)));
    }
    // chapa entre a viga e o apoio; solda filete dos dois lados da alma
    const double sPlate = gap;
    out.push_back(sb::box(Role::Plate, sPlate + tp / 2.0, 0.0, 0.0, tp, hp, wp));
    for (const double side : {-1.0, 1.0}) {
        out.push_back(sb::box(Role::Weld, sPlate + tp + in.weldLeg / 2.0, 0.0, side * (tw / 2.0 + in.weldLeg / 2.0), in.weldLeg, hp, in.weldLeg));
    }
    for (int r = 0; r < in.rows; ++r) {
        for (const double col : {-1.0, 1.0}) {
            sb::addBolt(out, {0.0, (r - (in.rows - 1) / 2.0) * in.pitch, col * in.gauge / 2.0}, 0, -3.0, sPlate + tp + 3.0, in.db);
        }
    }
    if (context) {
        const double s0 = sPlate + tp, s1 = s0 + 520.0;
        for (const double side : {-1.0, 1.0}) {
            out.push_back(sb::box(Role::Member, (s0 + s1) / 2.0, side * (d - tf) / 2.0, 0.0, s1 - s0, tf, bf));
        }
        out.push_back(sb::box(Role::Member, (s0 + s1) / 2.0, 0.0, 0.0, s1 - s0, d - 2.0 * tf, tw));
    }
}

std::vector<Cat::Dim> flexEndPlateDims(const NodeConnection &c)
{
    const D::FlexEndPlateInput in = toInput(c, 0.0, 0.0);
    std::vector<Cat::Dim> dims;
    const double hp = D::flexEndPlateHeight(in), tp = in.plateThickness;
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
    dims.push_back({in.gap + tp + 20.0, -hp / 2.0, in.gap + tp + 20.0, hp / 2.0, 14.0, text("H", hp)});
    if (in.rows > 1) {
        dims.push_back({in.gap - 25.0, (in.rows - 1) / 2.0 * in.pitch, in.gap - 25.0, (in.rows - 3) / 2.0 * in.pitch, 8.0, text("p", in.pitch)});
    }
    return dims;
}

std::vector<std::string> flexEndPlateNotes(const NodeConnection &c)
{
    const D::FlexEndPlateInput in = toInput(c, 0.0, 0.0);
    std::vector<std::string> notes;
    char buffer[220];
    std::snprintf(buffer, sizeof buffer, "CHAPA DE EXTREMIDADE FLEXÍVEL t = %.1f mm x %.0f x %.0f mm, SOLDADA À ALMA DA VIGA: FILETE %.0f mm DOS DOIS LADOS.", in.plateThickness, D::flexEndPlateWidth(in), D::flexEndPlateHeight(in), in.weldLeg);
    notes.push_back(buffer);
    std::snprintf(buffer, sizeof buffer, "PARAFUSOS d = %.0f mm %s: 2 COLUNAS x %d; FUROS PADRÃO.", in.db, c.grade.toUtf8().constData(), in.rows);
    notes.push_back(buffer);
    return notes;
}

QPair<QString, QString> flexEndPlateSteelKeys()
{
    return {QStringLiteral("plateFy"), QStringLiteral("plateFu")};
}

} // namespace Structura::App::Services::catalog
