#include "app/services/ConnectionCatalogInternal.h"

#include "design/PinConnection.h"

#include <QObject>

#include <algorithm>
#include <cmath>

namespace Structura::App::Services::catalog {

namespace D = Structura::Design::Connections;
using Structura::App::ProjectData::NodeConnection;
using Cat = ConnectionCatalog;

namespace {

D::PinPlateInput toInput(const NodeConnection &c, double tension)
{
    D::PinPlateInput in;
    in.plateThickness = val(c, "plateT", 19.0);
    in.plateWidth = val(c, "plateB", 120.0);
    in.endDistance = val(c, "plateA", 80.0);
    in.plate = {val(c, "plateFy", 250.0), val(c, "plateFu", 400.0)};
    in.pinDiameter = val(c, "pinD", 40.0);
    in.holeDiameter = val(c, "holeD", 40.5);
    in.rotationAllowed = val(c, "rot", 0.0) != 0.0;
    in.machinedHole = val(c, "machined", 1.0) != 0.0;
    in.pin = {val(c, "pinFy", 345.0), val(c, "pinFu", 450.0)};
    in.shearPlanes = std::clamp(static_cast<int>(val(c, "planes", 2.0) + 0.5), 1, 2);
    in.otherThickness = val(c, "otherT", 12.5);
    in.gap = val(c, "gap", 2.0);
    in.tension = std::abs(tension);
    return in;
}

} // namespace

QVector<Cat::FieldDef> pinFields(const Cat::Entry &entry)
{
    QVector<Cat::FieldDef> list;
    QString group;
    auto section = [&group](const QString &title) { group = title; };
    auto add = [&](const char *key, const QString &label, double min, double max, int decimals, const QString &suffix, const QStringList &options = {}, const QString &tip = {}) {
        list.append({QString::fromLatin1(key), label, min, max, decimals, suffix, group, options, tip});
    };
    const QString mm = QObject::tr(" mm"), mpa = QObject::tr(" MPa");
    const bool doubleShear = entry.preset.value(QStringLiteral("planes"), 2.0) > 1.5;
    section(QObject::tr("Chapa ligada por pino (NBR 8800:2024, 5.2.6)"));
    add("plateT", QObject::tr("Espessura da chapa t"), 3, 100, 1, mm);
    add("plateB", QObject::tr("Largura da chapa b"), 30, 800, 0, mm, {}, QObject::tr("O furo fica na meia distância das bordas; b ≥ 2 bef + dp."));
    add("plateA", QObject::tr("Distância do furo à extremidade a"), 10, 400, 0, mm, {}, QObject::tr("Borda do furo à extremidade da barra, paralela à força; a ≥ 1,33 bef."));
    add("plateFy", QObject::tr("fy da chapa"), 150, 800, 0, mpa);
    add("plateFu", QObject::tr("fu da chapa"), 250, 1000, 0, mpa);
    section(QObject::tr("Pino e furo (6.4 e 6.6.2)"));
    add("pinD", QObject::tr("Diâmetro do pino dp"), 10, 250, 1, mm);
    add("holeD", QObject::tr("Diâmetro do furo dh"), 10, 255, 1, mm);
    add("rot", QObject::tr("Função do pino"), 0, 1, 0, QString(), {QObject::tr("Só transmite força"), QObject::tr("Também permite rotação (dh − dp ≤ 1 mm)")});
    add("machined", QObject::tr("Furo"), 0, 1, 0, QString(), {QObject::tr("Não usinado (6.6.3)"), QObject::tr("Mandrilado ou broqueado (6.6.2)")});
    add("pinFy", QObject::tr("fy do pino"), 150, 1100, 0, mpa);
    add("pinFu", QObject::tr("fu do pino"), 250, 1300, 0, mpa);
    section(doubleShear ? QObject::tr("Chapas externas (corte duplo)") : QObject::tr("Outra chapa (corte simples)"));
    add("otherT", doubleShear ? QObject::tr("Espessura de cada chapa externa") : QObject::tr("Espessura da outra chapa"), 3, 100, 1, mm);
    add("gap", QObject::tr("Folga entre chapas adjacentes"), 0, 30, 1, mm);
    return list;
}

NodeConnection pinDefault(const Cat::Entry &entry, const Cat::BeamInfo &beam, double plateFy, double plateFu)
{
    NodeConnection c;
    c.grade = defaultBoltGradeName();
    const bool doubleShear = entry.preset.value(QStringLiteral("planes"), 2.0) > 1.5;
    // chapa dimensionada para a area da barra ligada: t b >= A (aproximadamente)
    const double area = beam.area > 0.0 ? beam.area : 2000.0;
    const double t = std::clamp(std::ceil(std::sqrt(area / 6.0) / 2.5) * 2.5, 12.5, 50.0);
    const double b = std::max(80.0, std::ceil(area / t / 5.0) * 5.0);
    const double pin = std::clamp(std::ceil(b / 3.0 / 5.0) * 5.0, 25.0, 120.0);
    c.values = {{"planes", doubleShear ? 2.0 : 1.0}, {"plateT", t}, {"plateB", b}, {"plateA", std::ceil(1.6 * std::min(2.0 * t + 16.0, (b - pin) / 2.0) / 5.0) * 5.0}, {"plateFy", plateFy},
                {"plateFu", plateFu}, {"pinD", pin}, {"holeD", pin + 0.5}, {"rot", 0.0}, {"machined", 1.0}, {"pinFy", 345.0}, {"pinFu", 450.0},
                {"otherT", doubleShear ? std::max(9.5, std::ceil(t / 2.0 / 2.5) * 2.5) : t}, {"gap", 2.0}};
    return c;
}

Cat::Evaluation pinEvaluate(const NodeConnection &c, const Cat::Forces &f)
{
    Cat::Evaluation e;
    const double tension = std::max(f.tension, std::max(f.axial, 0.0));
    const D::PinPlateInput in = toInput(c, tension);
    e.checks = D::checkPinPlate(in);
    e.parts = D::partsPinPlate(in);
    if (tension <= 0.0) {
        D::Check note;
        note.name = "Ligacao por pino - sem tracao nas combinacoes";
        note.clause = "5.2.6";
        note.demand = 0.0;
        note.resistance = 1.0;
        note.lines.push_back("A chapa ligada por pino e verificada a tracao: nao ha tracao nas combinacoes analisadas (compressao nao e coberta por 5.2.6).");
        e.checks.push_back(note);
    }
    e.maxRatio = D::worstRatio(e.checks);
    if (const auto *g = D::governing(e.checks)) {
        e.governing = QString::fromStdString(g->name);
    }
    e.description = pinDescribe(c);
    return e;
}

QString pinDescribe(const NodeConnection &c)
{
    return QObject::tr("Pino d = %1 mm em chapa %2 x %3 mm, corte %4")
        .arg(val(c, "pinD"), 0, 'f', 0).arg(val(c, "plateT"), 0, 'f', 1).arg(val(c, "plateB"), 0, 'f', 0).arg(val(c, "planes", 2.0) > 1.5 ? QObject::tr("duplo") : QObject::tr("simples"));
}

void pinSolids(const NodeConnection &c, bool context, std::vector<Cat::Primitive> &out)
{
    const D::PinPlateInput in = toInput(c, 0.0);
    const double t = in.plateThickness, b = in.plateWidth, dp = in.pinDiameter, t2 = in.otherThickness, g = in.gap;
    const double end = in.endDistance + dp / 2.0;
    // chapa da barra: do eixo do pino para -s (a barra); a extremidade fica a `end` do centro do furo
    out.push_back(sb::box(Role::Plate, -(end + 300.0) / 2.0 + end / 2.0, 0.0, 0.0, end + 300.0, b, t));
    // chapas do garfo (corte duplo) ou outra chapa (corte simples), para +s
    const double offset = t / 2.0 + g + t2 / 2.0;
    const double forkLength = 260.0;
    if (in.shearPlanes >= 2) {
        for (const double side : {-1.0, 1.0}) {
            out.push_back(sb::box(Role::Member, (forkLength - end) / 2.0, 0.0, side * offset, forkLength + end, std::max(b, 2.0 * dp), t2));
        }
    } else {
        out.push_back(sb::box(Role::Member, (forkLength - end) / 2.0, 0.0, offset, forkLength + end, std::max(b, 2.0 * dp), t2));
    }
    const double reach = in.shearPlanes >= 2 ? offset + t2 / 2.0 + 6.0 : offset + t2 / 2.0 + 6.0;
    const double lo = in.shearPlanes >= 2 ? -reach : -(t / 2.0 + 6.0);
    out.push_back(sb::cylinder(Role::Bolt, {0.0, 0.0, lo}, {0.0, 0.0, reach}, dp / 2.0));
    out.push_back(sb::cylinder(Role::Bolt, {0.0, 0.0, lo - 6.0}, {0.0, 0.0, lo}, 0.6 * dp));
    out.push_back(sb::cylinder(Role::Bolt, {0.0, 0.0, reach}, {0.0, 0.0, reach + 6.0}, 0.6 * dp));
    if (context) {
        out.push_back(sb::box(Role::Member, -(end + 300.0) - 150.0, 0.0, 0.0, 300.0, b * 0.9, t * 0.9));
    }
}

std::vector<std::string> pinNotes(const NodeConnection &c)
{
    const D::PinPlateInput in = toInput(c, 0.0);
    std::vector<std::string> notes;
    char buffer[220];
    std::snprintf(buffer, sizeof buffer, "PINO d = %.0f mm, FURO %.1f mm %s; CHAPA t = %.1f x %.0f mm, a = %.0f mm.", in.pinDiameter, in.holeDiameter,
                  in.machinedHole ? "MANDRILADO/BROQUEADO" : "BRUTO", in.plateThickness, in.plateWidth, in.endDistance);
    notes.push_back(buffer);
    return notes;
}

QPair<QString, QString> pinSteelKeys()
{
    return {QStringLiteral("plateFy"), QStringLiteral("plateFu")};
}

} // namespace Structura::App::Services::catalog
