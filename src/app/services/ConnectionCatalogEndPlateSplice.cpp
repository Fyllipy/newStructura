#include "app/services/ConnectionCatalogInternal.h"

#include "design/ShearConnections.h"

#include <QObject>

#include <algorithm>
#include <cmath>

namespace Structura::App::Services::catalog {

namespace D = Structura::Design::Connections;
using Structura::App::ProjectData::NodeConnection;
using Cat = ConnectionCatalog;

namespace {

bool columnKey(const QString &key)
{
    return key.startsWith(QLatin1String("sup")) || key.startsWith(QLatin1String("cont")) || key == QLatin1String("doubler");
}

bool columnCheck(const std::string &name)
{
    return name.rfind("Apoio:", 0) == 0 || name.rfind("Painel de alma", 0) == 0 || name.find("nrijecedores") != std::string::npos;
}

} // namespace

QVector<Cat::FieldDef> endPlateSpliceFields(const Cat::Entry &)
{
    QVector<Cat::FieldDef> list;
    for (const auto &def : ConnectionService::fields(NodeConnection::kEndPlate)) {
        if (!columnKey(def.key)) {
            list.append(def);
        }
    }
    return list;
}

NodeConnection endPlateSpliceDefault(const Cat::Entry &entry, const Cat::BeamInfo &beam, double plateFy, double plateFu)
{
    NodeConnection c = ConnectionService::makeDefault(NodeConnection::kEndPlate, beam, plateFy, plateFu);
    c.values["supType"] = 0.0;   // sem verificacao local de pilar: a "mesa de apoio" e a outra chapa de extremidade
    c.values["supTf"] = c.values.value(QStringLiteral("plateT"), 22.4);
    c.values["supFy"] = plateFy;
    c.values["supFu"] = plateFu;
    c.values["cont"] = 0.0;
    c.values["doubler"] = 0.0;
    c.values["outside"] = entry.preset.value(QStringLiteral("outside"), 0.0);
    return c;
}

Cat::Evaluation endPlateSpliceEvaluate(const NodeConnection &c, const Cat::Forces &f)
{
    NodeConnection legacy = c;
    legacy.type = NodeConnection::kEndPlate;
    legacy.values["supTf"] = c.values.value(QStringLiteral("plateT"), 22.4);   // a outra chapa tem a mesma espessura
    Cat::Evaluation e = ConnectionService::evaluate(legacy, f);
    std::vector<D::Check> kept;
    for (auto &chk : e.checks) {
        if (!columnCheck(chk.name)) {
            if (chk.name.find("mesa do pilar") != std::string::npos) {
                const auto pos = chk.name.find("mesa do pilar");
                chk.name.replace(pos, 13, "outra chapa");
            }
            kept.push_back(std::move(chk));
        }
    }
    e.checks = std::move(kept);
    e.maxRatio = D::worstRatio(e.checks);
    e.governing.clear();
    if (const auto *g = D::governing(e.checks)) {
        e.governing = QString::fromStdString(g->name);
    }
    e.description = endPlateSpliceDescribe(c);
    return e;
}

QString endPlateSpliceDescribe(const NodeConnection &c)
{
    const auto v = [&c](const char *key) { return val(c, key, 0.0); };
    return QObject::tr("2 chapas de extremidade %1 × %2 mm, %3 parafusos d%4 %5, solda %6/%7 mm")
        .arg(v("plateT"), 0, 'f', 1).arg(v("plateW"), 0, 'f', 0).arg(2 * (static_cast<int>(v("tRows") + 0.5) + static_cast<int>(v("sRows") + 0.5))).arg(v("db"), 0, 'f', 0).arg(c.grade)
        .arg(v("weldF"), 0, 'f', 0).arg(v("weldW"), 0, 'f', 0);
}

void endPlateSpliceSolids(const NodeConnection &c, bool context, std::vector<ConnectionSolids::Primitive> &out)
{
    const double db = val(c, "db", 20.0), d = val(c, "beamD", 300.0), bfl = val(c, "bfl", 150.0), tfl = val(c, "tfl", 10.0), tw = val(c, "tw", 6.0);
    const double tp = val(c, "plateT", 22.4), width = val(c, "plateW", 160.0), gauge = val(c, "gauge", 90.0), pitch = val(c, "pitch", 70.0);
    const double firstRow = val(c, "firstRow", 40.0), edge = val(c, "edgeV", 35.0), wf = val(c, "weldF", 8.0);
    const int tRows = count(c, "tRows", 2.0), sRows = count(c, "sRows", 1.0);
    const double extTop = firstRow + (tRows - 1) * pitch + edge, extBot = firstRow + (sRows - 1) * pitch + edge;
    const double height = d + extTop + extBot, mid = (extTop - extBot) / 2.0;
    const bool outside = val(c, "outside", 0.0) != 0.0;
    const double reach = context ? 420.0 : 120.0;
    const double leftReach = outside ? 180.0 : reach;   // flangeada fora do no: o trecho entre a junta e o pilar e curto
    for (const double side : {-1.0, 1.0}) {
        const double r = side < 0.0 ? leftReach : reach;
        // chapa e trecho de viga de cada lado da junta (junta em s = 0)
        out.push_back(sb::box(Role::Plate, side * tp / 2.0, mid, 0.0, tp, height, width));
        for (const double flange : {-1.0, 1.0}) {
            out.push_back(sb::box(Role::Member, side * (tp + r / 2.0), flange * (d - tfl) / 2.0, 0.0, r, tfl, bfl));
            out.push_back(sb::box(Role::Weld, side * (tp + wf / 2.0), flange * (d / 2.0 - tfl / 2.0), 0.0, wf, tfl + 2.0 * wf, bfl + 2.0 * wf));
        }
        out.push_back(sb::box(Role::Member, side * (tp + r / 2.0), 0.0, 0.0, r, d - 2.0 * tfl, tw));
    }
    if (outside && context) {
        // pilar I a esquerda: a viga chega na mesa do pilar
        const double colD = 240.0, colBf = 160.0, colTf = 12.0, colTw = 8.0, colH = height + 200.0;
        const double face = -(tp + leftReach);
        const double cc = face - colD / 2.0;
        for (const double side : {-1.0, 1.0}) {
            out.push_back(sb::box(Role::Concrete, cc + side * (colD - colTf) / 2.0, mid, 0.0, colTf, colH, colBf));
        }
        out.push_back(sb::box(Role::Concrete, cc, mid, 0.0, colD - 2.0 * colTf, colH, colTw));
    }
    for (int i = 0; i < tRows; ++i) {
        for (const double col : {-1.0, 1.0}) {
            sb::addBolt(out, {0.0, d / 2.0 + firstRow + i * pitch, col * gauge / 2.0}, 0, -tp - 2.0, tp + 2.0, db);
        }
    }
    for (int j = 0; j < sRows; ++j) {
        for (const double col : {-1.0, 1.0}) {
            sb::addBolt(out, {0.0, -(d / 2.0 + firstRow + j * pitch), col * gauge / 2.0}, 0, -tp - 2.0, tp + 2.0, db);
        }
    }
}

QPair<QString, QString> endPlateSpliceSteelKeys()
{
    return {QStringLiteral("plateFy"), QStringLiteral("plateFu")};
}

} // namespace Structura::App::Services::catalog
