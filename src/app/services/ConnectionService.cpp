#include "ConnectionService.h"

#include "design/BasePlate.h"
#include "design/TrussConnections.h"
#include "design/TubularJoints.h"

#include <QObject>
#include <QPair>

#include <algorithm>
#include <cmath>

namespace Structura::App::Services {

namespace D = Structura::Design::Connections;
using Structura::App::ProjectData::NodeConnection;

namespace {

D::BoltGrade gradeByName(const QString &name)
{
    for (const auto &g : D::boltGrades()) {
        if (QString::fromStdString(g.name) == name) {
            return g;
        }
    }
    return D::boltGrades()[2];   // A325
}

double value(const NodeConnection &c, const char *key, double fallback = 0.0)
{
    return c.values.value(QString::fromLatin1(key), fallback);
}

int count(const NodeConnection &c, const char *key, int fallback)
{
    return static_cast<int>(value(c, key, fallback) + 0.5);
}

D::ShearTabInput toShearTab(const NodeConnection &c, const ConnectionService::Forces &f)
{
    D::ShearTabInput in;
    in.grade = gradeByName(c.grade);
    in.db = value(c, "db", 20.0);
    in.threadInShearPlane = value(c, "thread", 1.0) != 0.0;
    in.rows = count(c, "rows", 3);
    in.pitch = value(c, "pitch", 70.0);
    in.edgeV = value(c, "edgeV", 35.0);
    in.edgeH = value(c, "edgeH", 35.0);
    in.boltLineToWeld = value(c, "a", 60.0);
    in.beamSetback = value(c, "setback", 12.0);
    in.plate = {value(c, "plateFy", 250.0), value(c, "plateFu", 400.0)};
    in.plateThickness = value(c, "plateT", 9.5);
    in.web = {value(c, "webFy", 250.0), value(c, "webFu", 400.0)};
    in.webThickness = value(c, "webT", 6.0);
    in.beamDepth = value(c, "beamDepth", 0.0);
    in.weldLeg = value(c, "weldLeg", 6.0);
    in.weldFw = value(c, "weldFw", 485.0);
    in.supportThickness = value(c, "supT", 12.0);
    in.support = {value(c, "supFy", 250.0), value(c, "supFu", 400.0)};
    in.shear = f.shear;
    in.axial = f.axial;
    return in;
}

void fromShearTab(NodeConnection &c, const D::ShearTabInput &in)
{
    c.values["db"] = in.db;
    c.values["rows"] = in.rows;
    c.values["pitch"] = in.pitch;
    c.values["edgeV"] = in.edgeV;
    c.values["edgeH"] = in.edgeH;
    c.values["a"] = in.boltLineToWeld;
    c.values["plateT"] = in.plateThickness;
    c.values["weldLeg"] = in.weldLeg;
}

D::DoubleAngleInput toDoubleAngle(const NodeConnection &c, const ConnectionService::Forces &f)
{
    D::DoubleAngleInput in;
    in.grade = gradeByName(c.grade);
    in.db = value(c, "db", 20.0);
    in.threadInShearPlane = value(c, "thread", 1.0) != 0.0;
    in.rows = count(c, "rows", 3);
    in.pitch = value(c, "pitch", 70.0);
    in.edgeV = value(c, "edgeV", 35.0);
    in.gaugeWeb = value(c, "gaugeWeb", 50.0);
    in.gaugeSupport = value(c, "gaugeSupport", 50.0);
    in.webEndDistance = value(c, "webEnd", 30.0);
    in.supportEdgeDistance = value(c, "supEdge", 35.0);
    in.angle = {value(c, "angleFy", 250.0), value(c, "angleFu", 400.0)};
    in.angleThickness = value(c, "angleT", 9.5);
    in.web = {value(c, "webFy", 250.0), value(c, "webFu", 400.0)};
    in.webThickness = value(c, "webT", 6.0);
    in.support = {value(c, "supFy", 250.0), value(c, "supFu", 400.0)};
    in.supportThickness = value(c, "supT", 12.0);
    in.beamDepth = value(c, "beamDepth", 0.0);
    in.shear = f.shear;
    in.axial = f.axial;
    return in;
}

void fromDoubleAngle(NodeConnection &c, const D::DoubleAngleInput &in)
{
    c.values["db"] = in.db;
    c.values["rows"] = in.rows;
    c.values["pitch"] = in.pitch;
    c.values["edgeV"] = in.edgeV;
    c.values["gaugeWeb"] = in.gaugeWeb;
    c.values["gaugeSupport"] = in.gaugeSupport;
    c.values["webEnd"] = in.webEndDistance;
    c.values["supEdge"] = in.supportEdgeDistance;
    c.values["angleT"] = in.angleThickness;
}

// ---- chapa de no ---------------------------------------------------------------------------------------------------

D::GussetInput toGusset(const NodeConnection &c, const ConnectionService::Forces &f)
{
    D::GussetInput in;
    in.grade = gradeByName(c.grade);
    in.db = value(c, "db", 20.0);
    in.threadInShearPlane = value(c, "thread", 1.0) != 0.0;
    in.rows = count(c, "rows", 3);
    in.lines = count(c, "lines", 1);
    in.pitch = value(c, "pitch", 70.0);
    in.gauge = value(c, "gauge", 60.0);
    in.endDistLeg = value(c, "endLeg", 35.0);
    in.edgeDistLeg = value(c, "edgeLeg", 30.0);
    in.endDistGusset = value(c, "endG", 35.0);
    in.edgeDistGusset = value(c, "edgeG", 40.0);
    in.bars = count(c, "bars", 2);
    in.barArea = value(c, "barArea", 0.0);
    in.barLegThickness = value(c, "legT", 6.35);
    in.barEccentricity = value(c, "ec", 20.0);
    in.bar = {value(c, "barFy", 250.0), value(c, "barFu", 400.0)};
    in.gussetThickness = value(c, "gT", 9.5);
    in.gussetWidth = value(c, "gWidth", 200.0);
    in.freeLength = value(c, "freeL", 80.0);
    in.gusset = {value(c, "gFy", 250.0), value(c, "gFu", 400.0)};
    in.weldLeg = value(c, "weldLeg", 6.0);
    in.weldFw = value(c, "weldFw", 485.0);
    in.weldAngleDeg = value(c, "weldAngle", 90.0);
    in.supportThickness = value(c, "supT", 12.0);
    in.support = {value(c, "supFy", 250.0), value(c, "supFu", 400.0)};
    in.tension = f.tension;
    in.compression = f.compression;
    return in;
}

void fromGusset(NodeConnection &c, const D::GussetInput &in)
{
    c.values["db"] = in.db;
    c.values["rows"] = in.rows;
    c.values["lines"] = in.lines;
    c.values["pitch"] = in.pitch;
    c.values["gauge"] = in.gauge;
    c.values["endLeg"] = in.endDistLeg;
    c.values["edgeLeg"] = in.edgeDistLeg;
    c.values["endG"] = in.endDistGusset;
    c.values["edgeG"] = in.edgeDistGusset;
    c.values["gT"] = in.gussetThickness;
    c.values["gWidth"] = in.gussetWidth;
    c.values["weldLeg"] = in.weldLeg;
}

// ---- emenda de viga ------------------------------------------------------------------------------------------------

D::SpliceInput toSplice(const NodeConnection &c, double shear, double axial, double moment)
{
    D::SpliceInput in;
    in.grade = gradeByName(c.grade);
    in.db = value(c, "db", 20.0);
    in.threadInShearPlane = value(c, "thread", 1.0) != 0.0;
    in.beamDepth = value(c, "beamD", 300.0);
    in.flangeWidth = value(c, "bfl", 150.0);
    in.flangeThickness = value(c, "tfl", 10.0);
    in.webThickness = value(c, "tw", 6.0);
    in.beamArea = value(c, "beamArea", 0.0);
    in.beam = {value(c, "beamFy", 250.0), value(c, "beamFu", 400.0)};
    in.gap = value(c, "gap", 10.0);
    in.flangeRows = count(c, "fRows", 3);
    in.flangeLines = count(c, "fLines", 2);
    in.flangePitch = value(c, "fPitch", 70.0);
    in.flangeGauge = value(c, "fGauge", 80.0);
    in.flangeEnd = value(c, "fEnd", 40.0);
    in.flangeEdge = value(c, "fEdge", 35.0);
    in.outerPlateWidth = value(c, "opW", 150.0);
    in.outerPlateThickness = value(c, "opT", 12.5);
    in.innerPlateThickness = value(c, "ipT", 0.0);
    in.innerPlateWidth = value(c, "ipW", 55.0);
    in.plates = {value(c, "plateFy", 250.0), value(c, "plateFu", 400.0)};
    in.webRows = count(c, "wRows", 3);
    in.webCols = count(c, "wCols", 1);
    in.webPitch = value(c, "wPitch", 70.0);
    in.webGauge = value(c, "wGauge", 60.0);
    in.webEdgeH = value(c, "wEdgeH", 40.0);
    in.webEdgeV = value(c, "wEdgeV", 35.0);
    in.webPlateThickness = value(c, "wpT", 6.3);
    in.shear = shear;
    in.axial = axial;
    in.moment = moment;
    return in;
}

void fromSplice(NodeConnection &c, const D::SpliceInput &in)
{
    c.values["db"] = in.db;
    c.values["fRows"] = in.flangeRows;
    c.values["fPitch"] = in.flangePitch;
    c.values["fGauge"] = in.flangeGauge;
    c.values["fEnd"] = in.flangeEnd;
    c.values["fEdge"] = in.flangeEdge;
    c.values["opW"] = in.outerPlateWidth;
    c.values["opT"] = in.outerPlateThickness;
    c.values["ipT"] = in.innerPlateThickness;
    c.values["wRows"] = in.webRows;
    c.values["wPitch"] = in.webPitch;
    c.values["wGauge"] = in.webGauge;
    c.values["wEdgeH"] = in.webEdgeH;
    c.values["wEdgeV"] = in.webEdgeV;
    c.values["wpT"] = in.webPlateThickness;
}

// ---- base de pilar -------------------------------------------------------------------------------------------------

D::BasePlateInput toBasePlate(const NodeConnection &c, double shear, double axial, double moment)
{
    D::BasePlateInput in;
    in.d = value(c, "colD", 300.0);
    in.bf = value(c, "colBf", 150.0);
    in.tw = value(c, "colTw", 6.0);
    in.tf = value(c, "colTf", 10.0);
    in.column = {value(c, "colFy", 345.0), value(c, "colFu", 450.0)};
    in.anchorDiameter = value(c, "anchorD", 25.0);
    in.anchors = count(c, "anchors", 4);
    in.anchorFub = value(c, "anchorFub", 400.0);
    in.plateThickness = value(c, "plateT", 25.0);
    in.plate = {value(c, "plateFy", 250.0), value(c, "plateFu", 400.0)};
    in.fck = value(c, "fck", 25.0);
    in.gammaC = value(c, "gammaC", 1.4);
    in.blockN = value(c, "blockN", 0.0);
    in.blockB = value(c, "blockB", 0.0);
    in.frictionCoefficient = value(c, "mu", 0.45);
    in.weldLeg = value(c, "weldLeg", 6.0);
    in.weldFw = value(c, "weldFw", 485.0);
    in.shear = shear;
    in.axial = axial;
    in.moment = moment;
    return in;
}

void fromBasePlate(NodeConnection &c, const D::BasePlateInput &in)
{
    c.values["anchorD"] = in.anchorDiameter;
    c.values["anchors"] = in.anchors;
    c.values["plateT"] = in.plateThickness;
    c.values["weldLeg"] = in.weldLeg;
}

// ---- ligacao tubular -----------------------------------------------------------------------------------------------

D::TubularJointInput toTubular(const NodeConnection &c, const ConnectionService::ForceCase &fc)
{
    D::TubularJointInput in;
    in.kind = count(c, "kindT", D::TubularJointInput::KGap);
    in.braceKind = count(c, "braceKind", 0);
    in.d0 = value(c, "d0", 114.3);
    in.t0 = value(c, "t0", 6.0);
    in.fy0 = value(c, "fy0", 355.0);
    in.fu0 = value(c, "fu0", 490.0);
    in.d1 = value(c, "d1", 60.3);
    in.t1 = value(c, "t1", 4.0);
    in.fy1 = value(c, "fy1", 355.0);
    in.theta1 = value(c, "th1", 45.0);
    in.d2 = value(c, "d2", 60.3);
    in.t2 = value(c, "t2", 4.0);
    in.fy2 = value(c, "fy2", 355.0);
    in.theta2 = value(c, "th2", 45.0);
    in.gap = value(c, "gap", 20.0);
    in.overlap = value(c, "overlap", 50.0);
    in.hiddenToeWelded = count(c, "hidden", 0) != 0;
    in.eccentricity = value(c, "ecc", 0.0);
    in.chordAxial = fc.axial;
    in.chordMoment = fc.moment;
    in.n1 = fc.braceAxial.size() > 0 ? fc.braceAxial[0] : 0.0;
    in.n2 = fc.braceAxial.size() > 1 ? fc.braceAxial[1] : 0.0;
    return in;
}

// ---- avaliacao por combinacao ---------------------------------------------------------------------------------------

QVector<ConnectionService::ForceCase> casesOf(const ConnectionService::Forces &f)
{
    if (!f.cases.isEmpty()) {
        return f.cases;
    }
    ConnectionService::ForceCase single;
    single.name = QObject::tr("envoltória");
    single.shear = f.shear;
    single.axial = f.axial;
    single.moment = f.moment;
    return {single};
}

std::vector<D::Check> checksOf(const NodeConnection &c, const ConnectionService::ForceCase &fc)
{
    switch (c.type) {
    case NodeConnection::kSplice:
        return D::checkSplice(toSplice(c, fc.shear, fc.axial, fc.moment));
    case NodeConnection::kBasePlate:
        return D::checkBasePlate(toBasePlate(c, fc.shear, fc.axial, fc.moment));
    default:
        return D::checkTubularJoint(toTubular(c, fc));
    }
}

QString kindName(int kind)
{
    switch (kind) {
    case D::TubularJointInput::TY: return QObject::tr("T/Y");
    case D::TubularJointInput::X: return QObject::tr("X");
    case D::TubularJointInput::KGap: return QObject::tr("K com afastamento");
    default: return QObject::tr("K com sobreposição");
    }
}

} // namespace

QString ConnectionService::typeName(int type)
{
    switch (type) {
    case NodeConnection::kDoubleAngle: return QObject::tr("Cantoneira dupla");
    case NodeConnection::kGusset: return QObject::tr("Chapa de nó (gusset)");
    case NodeConnection::kSplice: return QObject::tr("Emenda de viga");
    case NodeConnection::kBasePlate: return QObject::tr("Base de pilar");
    case NodeConnection::kTubular: return QObject::tr("Ligação tubular (CHS)");
    default: return QObject::tr("Chapa de cisalhamento");
    }
}

QString ConnectionService::typeSummary(int type)
{
    switch (type) {
    case NodeConnection::kShearTab: return QObject::tr("Viga apoiada em pilar ou viga principal por chapa soldada ao apoio e parafusada na alma (ligação flexível).");
    case NodeConnection::kDoubleAngle: return QObject::tr("Viga apoiada por duas cantoneiras parafusadas na alma e no apoio.");
    case NodeConnection::kGusset: return QObject::tr("Diagonal, montante ou contraventamento (cantoneira simples/dupla ou chapa) parafusado em chapa de nó soldada ao banzo ou pilar: parafusos, diagonal, secção de Whitmore e solda.");
    case NodeConnection::kSplice: return QObject::tr("Continuidade de viga I/H com talas de mesa e de alma parafusadas: transmite momento, cortante e axial.");
    case NodeConnection::kBasePlate: return QObject::tr("Pilar I/H sobre bloco de concreto com chumbadores (NBR 8800:2024, 6.7): placa de base, chumbadores, pressão no concreto e atrito.");
    default: return QObject::tr("Diagonais de seção circular soldadas ao banzo tubular (T, Y, X, K, N), ou chapa transversal: plastificação do banzo, punção e esmagamento (CIDECT DG1).");
    }
}

bool ConnectionService::usesBolts(int type)
{
    return type != NodeConnection::kTubular && type != NodeConnection::kBasePlate;
}

QPair<QString, QString> ConnectionService::steelKeys(int type)
{
    switch (type) {
    case NodeConnection::kDoubleAngle: return {QStringLiteral("angleFy"), QStringLiteral("angleFu")};
    case NodeConnection::kGusset: return {QStringLiteral("gFy"), QStringLiteral("gFu")};
    case NodeConnection::kSplice:
    case NodeConnection::kBasePlate:
    case NodeConnection::kShearTab: return {QStringLiteral("plateFy"), QStringLiteral("plateFu")};
    default: return {};
    }
}

bool ConnectionService::hasProposal(int type)
{
    return type != NodeConnection::kTubular;
}

QVector<ConnectionService::FieldDef> ConnectionService::fields(int type)
{
    QVector<FieldDef> list;
    QString group;
    auto section = [&group](const QString &title) { group = title; };
    auto add = [&](const char *key, const QString &label, double min, double max, int decimals, const QString &suffix, const QStringList &options = {}, const QString &tip = {}) {
        FieldDef def {QString::fromLatin1(key), label, min, max, decimals, suffix, group, options, tip};
        list.append(def);
    };
    const QString mm = QObject::tr(" mm"), mpa = QObject::tr(" MPa"), deg = QObject::tr(" °"), mm2 = QObject::tr(" mm²");
    switch (type) {
    case NodeConnection::kGusset: {
        section(QObject::tr("Parafusos"));
        add("db", QObject::tr("Diâmetro do parafuso"), 10, 40, 1, mm);
        add("rows", QObject::tr("Parafusos ao longo da força"), 1, 12, 0, QString());
        add("lines", QObject::tr("Linhas de parafusos"), 1, 2, 0, QString());
        add("pitch", QObject::tr("Espaçamento ao longo da força"), 30, 300, 1, mm);
        add("gauge", QObject::tr("Espaçamento entre linhas"), 30, 300, 1, mm);
        add("endLeg", QObject::tr("Furo extremo – extremidade da diagonal"), 15, 150, 1, mm);
        add("edgeLeg", QObject::tr("Linha de furos – borda da aba da diagonal"), 15, 150, 1, mm);
        add("endG", QObject::tr("Furo extremo – ponta da chapa"), 15, 150, 1, mm);
        add("edgeG", QObject::tr("Linha extrema – borda lateral da chapa"), 15, 150, 1, mm);
        section(QObject::tr("Diagonal"));
        add("bars", QObject::tr("Peças da diagonal (1 simples, 2 com a chapa entre elas)"), 1, 2, 0, QString());
        add("barArea", QObject::tr("Área bruta de uma peça"), 10, 100000, 0, mm2);
        add("legT", QObject::tr("Espessura da aba ligada"), 2, 40, 2, mm);
        add("ec", QObject::tr("Excentricidade da ligação ec (centroide – face)"), 0, 200, 1, mm);
        add("barFy", QObject::tr("fy da diagonal"), 150, 800, 0, mpa);
        add("barFu", QObject::tr("fu da diagonal"), 250, 1000, 0, mpa);
        section(QObject::tr("Chapa de nó"));
        add("gT", QObject::tr("Espessura da chapa"), 3, 50, 1, mm);
        add("gWidth", QObject::tr("Largura da chapa na solda ao apoio"), 30, 2000, 0, mm);
        add("freeL", QObject::tr("Distância do apoio aos parafusos mais próximos"), 10, 1000, 0, mm);
        add("gFy", QObject::tr("fy da chapa"), 150, 800, 0, mpa);
        add("gFu", QObject::tr("fu da chapa"), 250, 1000, 0, mpa);
        section(QObject::tr("Solda ao apoio"));
        add("weldLeg", QObject::tr("Perna do filete"), 3, 25, 1, mm);
        add("weldFw", QObject::tr("fw do metal da solda"), 415, 550, 0, mpa);
        add("weldAngle", QObject::tr("Ângulo entre a força e a solda (90° = perpendicular)"), 0, 90, 0, deg);
        add("supT", QObject::tr("Espessura do apoio (banzo/pilar)"), 3, 60, 1, mm);
        add("supFy", QObject::tr("fy do apoio"), 150, 800, 0, mpa);
        add("supFu", QObject::tr("fu do apoio"), 250, 1000, 0, mpa);
        break;
    }
    case NodeConnection::kSplice: {
        section(QObject::tr("Parafusos"));
        add("db", QObject::tr("Diâmetro do parafuso"), 10, 40, 1, mm);
        section(QObject::tr("Viga"));
        add("beamD", QObject::tr("Altura da viga"), 50, 2000, 1, mm);
        add("bfl", QObject::tr("Largura da mesa"), 30, 600, 1, mm);
        add("tfl", QObject::tr("Espessura da mesa"), 3, 60, 1, mm);
        add("tw", QObject::tr("Espessura da alma"), 2, 40, 2, mm);
        add("beamArea", QObject::tr("Área bruta da viga"), 100, 200000, 0, mm2);
        add("beamFy", QObject::tr("fy da viga"), 150, 800, 0, mpa);
        add("beamFu", QObject::tr("fu da viga"), 250, 1000, 0, mpa);
        add("gap", QObject::tr("Folga entre as extremidades"), 0, 40, 1, mm);
        section(QObject::tr("Talas de mesa"));
        add("fRows", QObject::tr("Parafusos ao longo da viga (cada lado)"), 1, 10, 0, QString());
        add("fLines", QObject::tr("Parafusos na largura"), 1, 4, 0, QString());
        add("fPitch", QObject::tr("Espaçamento ao longo da viga"), 30, 300, 1, mm);
        add("fGauge", QObject::tr("Espaçamento na largura"), 30, 400, 1, mm);
        add("fEnd", QObject::tr("Furo extremo – extremidade da tala"), 15, 150, 1, mm);
        add("fEdge", QObject::tr("Furo – borda lateral"), 15, 150, 1, mm);
        add("opW", QObject::tr("Largura da tala externa"), 30, 600, 1, mm);
        add("opT", QObject::tr("Espessura da tala externa"), 3, 50, 1, mm);
        add("ipT", QObject::tr("Espessura das talas internas (0 = sem)"), 0, 50, 1, mm);
        add("ipW", QObject::tr("Largura de cada tala interna"), 20, 300, 1, mm);
        add("plateFy", QObject::tr("fy das talas"), 150, 800, 0, mpa);
        add("plateFu", QObject::tr("fu das talas"), 250, 1000, 0, mpa);
        section(QObject::tr("Talas de alma (duas)"));
        add("wRows", QObject::tr("Parafusos na vertical"), 1, 12, 0, QString());
        add("wCols", QObject::tr("Colunas de parafusos (cada lado)"), 1, 3, 0, QString());
        add("wPitch", QObject::tr("Espaçamento vertical"), 30, 300, 1, mm);
        add("wGauge", QObject::tr("Espaçamento entre colunas"), 30, 300, 1, mm);
        add("wEdgeH", QObject::tr("Coluna extrema – borda da tala"), 15, 150, 1, mm);
        add("wEdgeV", QObject::tr("Furo extremo – borda superior/inferior"), 15, 150, 1, mm);
        add("wpT", QObject::tr("Espessura de cada tala de alma"), 3, 40, 1, mm);
        break;
    }
    case NodeConnection::kBasePlate: {
        section(QObject::tr("Pilar"));
        add("colD", QObject::tr("Altura do perfil d"), 50, 1200, 1, mm);
        add("colBf", QObject::tr("Largura da mesa bf"), 30, 600, 1, mm);
        add("colTw", QObject::tr("Espessura da alma tw"), 2, 40, 2, mm);
        add("colTf", QObject::tr("Espessura da mesa tf"), 3, 60, 1, mm);
        add("colFy", QObject::tr("fy do pilar"), 150, 800, 0, mpa);
        add("colFu", QObject::tr("fu do pilar"), 250, 1000, 0, mpa);
        section(QObject::tr("Chumbadores (ASTM A36, Tabela 18)"));
        add("anchorD", QObject::tr("Diâmetro do chumbador"), 19, 50, 1, mm, {}, QObject::tr("Os demais dados (a1, a2, ancoragem, arruelas) vêm da Tabela 18 da NBR 8800."));
        add("anchors", QObject::tr("Número de chumbadores (4 a 8)"), 4, 8, 0, QString());
        add("anchorFub", QObject::tr("fub do chumbador"), 300, 1000, 0, mpa);
        section(QObject::tr("Placa de base"));
        add("plateT", QObject::tr("Espessura da placa"), 6, 100, 1, mm);
        add("plateFy", QObject::tr("fy da placa"), 150, 800, 0, mpa);
        add("plateFu", QObject::tr("fu da placa"), 250, 1000, 0, mpa);
        section(QObject::tr("Concreto e argamassa (NBR 6118)"));
        add("fck", QObject::tr("fck do concreto"), 15, 90, 0, mpa);
        add("gammaC", QObject::tr("γc"), 1.0, 2.0, 2, QString());
        add("blockN", QObject::tr("Comprimento do bloco Nb (0 = mínimo da Tabela 18)"), 0, 5000, 0, mm);
        add("blockB", QObject::tr("Largura do bloco Bb (0 = mínimo)"), 0, 5000, 0, mm);
        add("mu", QObject::tr("Coeficiente de atrito placa–argamassa"), 0, 1, 2, QString());
        section(QObject::tr("Solda pilar–placa"));
        add("weldLeg", QObject::tr("Perna do filete"), 3, 25, 1, mm);
        add("weldFw", QObject::tr("fw do metal da solda"), 415, 550, 0, mpa);
        break;
    }
    case NodeConnection::kTubular: {
        section(QObject::tr("Tipologia"));
        add("kindT", QObject::tr("Tipo de ligação"), 0, 3, 0, QString(),
            {QObject::tr("T / Y"), QObject::tr("X"), QObject::tr("K / N com afastamento"), QObject::tr("K / N com sobreposição")});
        add("braceKind", QObject::tr("Elemento ligado ao banzo"), 0, 1, 0, QString(), {QObject::tr("Tubo circular"), QObject::tr("Chapa transversal (T/X)")});
        section(QObject::tr("Banzo (CHS)"));
        add("d0", QObject::tr("Diâmetro externo d0"), 20, 1000, 1, mm);
        add("t0", QObject::tr("Espessura t0"), 1, 60, 2, mm);
        add("fy0", QObject::tr("fy do banzo"), 150, 800, 0, mpa);
        add("fu0", QObject::tr("fu do banzo"), 250, 1000, 0, mpa);
        section(QObject::tr("Diagonal 1 (a que se sobrepõe, em K com sobreposição)"));
        add("d1", QObject::tr("Diâmetro d1 (ou largura da chapa)"), 10, 1000, 1, mm);
        add("t1", QObject::tr("Espessura t1"), 1, 60, 2, mm);
        add("fy1", QObject::tr("fy da diagonal 1"), 150, 800, 0, mpa);
        add("th1", QObject::tr("Ângulo θ1 com o banzo"), 5, 90, 1, deg);
        section(QObject::tr("Diagonal 2 (K / N)"));
        add("d2", QObject::tr("Diâmetro d2"), 10, 1000, 1, mm);
        add("t2", QObject::tr("Espessura t2"), 1, 60, 2, mm);
        add("fy2", QObject::tr("fy da diagonal 2"), 150, 800, 0, mpa);
        add("th2", QObject::tr("Ângulo θ2 com o banzo"), 5, 90, 1, deg);
        section(QObject::tr("Geometria do nó"));
        add("gap", QObject::tr("Afastamento g (K com afastamento)"), -500, 500, 1, mm);
        add("overlap", QObject::tr("Sobreposição Ov (K com sobreposição)"), 0, 100, 0, QObject::tr(" %"));
        add("hidden", QObject::tr("Pé oculto da diagonal sobreposta soldado"), 0, 1, 0, QString(), {QObject::tr("Não (Ov,limite 60%)"), QObject::tr("Sim (Ov,limite 80%)")});
        add("ecc", QObject::tr("Excentricidade e do nó"), -500, 500, 1, mm);
        break;
    }
    default: {
        section(QObject::tr("Parafusos"));
        add("db", QObject::tr("Diâmetro do parafuso"), 10, 40, 1, mm);
        add("rows", QObject::tr("Parafusos na coluna"), 1, 12, 0, QString());
        add("pitch", QObject::tr("Espaçamento vertical"), 30, 300, 1, mm);
        add("edgeV", QObject::tr("Furo extremo – extremidade (vertical)"), 15, 150, 1, mm);
        if (type == NodeConnection::kDoubleAngle) {
            section(QObject::tr("Cantoneiras"));
            add("gaugeWeb", QObject::tr("Linha de furos – aresta, aba da alma"), 20, 150, 1, mm);
            add("gaugeSupport", QObject::tr("Linha de furos – aresta, aba do apoio"), 20, 150, 1, mm);
            add("webEnd", QObject::tr("Linha de furos da alma – extremidade da viga"), 15, 150, 1, mm);
            add("supEdge", QObject::tr("Linha de furos do apoio – borda da aba"), 15, 150, 1, mm);
            add("angleT", QObject::tr("Espessura da cantoneira"), 3, 32, 1, mm);
            add("angleFy", QObject::tr("fy da cantoneira"), 150, 800, 0, mpa);
            add("angleFu", QObject::tr("fu da cantoneira"), 250, 1000, 0, mpa);
        } else {
            section(QObject::tr("Chapa e solda"));
            add("edgeH", QObject::tr("Linha de furos – borda livre da chapa"), 15, 150, 1, mm);
            add("a", QObject::tr("Linha de furos – face do apoio (excentricidade)"), 20, 250, 1, mm);
            add("setback", QObject::tr("Folga da extremidade da viga"), 0, 40, 1, mm);
            add("plateT", QObject::tr("Espessura da chapa"), 3, 40, 1, mm);
            add("plateFy", QObject::tr("fy da chapa"), 150, 800, 0, mpa);
            add("plateFu", QObject::tr("fu da chapa"), 250, 1000, 0, mpa);
            add("weldLeg", QObject::tr("Perna do filete de solda"), 3, 25, 1, mm);
            add("weldFw", QObject::tr("fw do metal da solda"), 415, 550, 0, mpa);
        }
        section(QObject::tr("Viga e apoio"));
        add("webT", QObject::tr("Espessura da alma da viga"), 3, 40, 2, mm);
        add("webFy", QObject::tr("fy da viga"), 150, 800, 0, mpa);
        add("webFu", QObject::tr("fu da viga"), 250, 1000, 0, mpa);
        add("beamDepth", QObject::tr("Altura da viga (0 = não verifica)"), 0, 2000, 0, mm);
        add("supT", QObject::tr("Espessura da peça de apoio no ponto da ligação"), 3, 60, 1, mm);
        add("supFy", QObject::tr("fy do apoio"), 150, 800, 0, mpa);
        add("supFu", QObject::tr("fu do apoio"), 250, 1000, 0, mpa);
        break;
    }
    }
    return list;
}

NodeConnection ConnectionService::makeDefault(int type, const BeamInfo &beam, double plateFy, double plateFu)
{
    NodeConnection c;
    c.type = type;
    c.grade = QString::fromStdString(D::boltGrades()[2].name);
    if (type == NodeConnection::kGusset) {
        const bool angle = beam.family.startsWith(QLatin1String("L "));
        double legT = beam.geometry.value(QStringLiteral("t"), beam.tf > 0 ? beam.tf : 6.35);
        double ec = 0.25 * std::max(beam.depth, beam.geometry.value(QStringLiteral("b"), beam.bf));
        if (angle) {
            const double b = beam.geometry.value(QStringLiteral("b"), beam.geometry.value(QStringLiteral("bf"), beam.geometry.value(QStringLiteral("d"), 60.0)));
            ec = (b * b + b * legT - legT * legT) / (2.0 * (2.0 * b - legT));
        }
        c.values = {{"db", 20.0}, {"thread", 1.0}, {"rows", 3.0}, {"lines", 1.0}, {"pitch", 70.0}, {"gauge", 60.0}, {"endLeg", 35.0}, {"edgeLeg", 30.0},
                    {"endG", 35.0}, {"edgeG", 40.0}, {"bars", angle ? 2.0 : 1.0}, {"barArea", beam.area}, {"legT", legT}, {"ec", ec}, {"barFy", beam.fy},
                    {"barFu", beam.fu}, {"gT", 9.5}, {"gWidth", 200.0}, {"freeL", 80.0}, {"gFy", plateFy}, {"gFu", plateFu}, {"weldLeg", 6.0}, {"weldFw", 485.0},
                    {"weldAngle", 90.0}, {"supT", 12.0}, {"supFy", plateFy}, {"supFu", plateFu}};
        return c;
    }
    if (type == NodeConnection::kSplice) {
        const double flangeW = beam.bf > 0 ? beam.bf : 150.0;
        c.values = {{"db", 20.0}, {"thread", 1.0}, {"beamD", beam.depth}, {"bfl", flangeW}, {"tfl", beam.tf}, {"tw", beam.tw}, {"beamArea", beam.area},
                    {"beamFy", beam.fy}, {"beamFu", beam.fu}, {"gap", 10.0}, {"fRows", 3.0}, {"fLines", 2.0}, {"fPitch", 70.0}, {"fGauge", std::max(60.0, flangeW * 0.5)},
                    {"fEnd", 40.0}, {"fEdge", 35.0}, {"opW", flangeW}, {"opT", 12.5}, {"ipT", 0.0}, {"ipW", std::max(20.0, (flangeW - beam.tw) / 2.0 - 15.0)},
                    {"plateFy", plateFy}, {"plateFu", plateFu}, {"wRows", 3.0}, {"wCols", 1.0}, {"wPitch", 70.0}, {"wGauge", 60.0}, {"wEdgeH", 40.0}, {"wEdgeV", 35.0},
                    {"wpT", 6.3}};
        return c;
    }
    if (type == NodeConnection::kBasePlate) {
        c.values = {{"colD", beam.depth}, {"colBf", beam.bf > 0 ? beam.bf : 150.0}, {"colTw", beam.tw}, {"colTf", beam.tf}, {"colFy", beam.fy}, {"colFu", beam.fu},
                    {"anchorD", 25.0}, {"anchors", 4.0}, {"anchorFub", 400.0}, {"plateT", 25.0}, {"plateFy", plateFy}, {"plateFu", plateFu}, {"fck", 25.0},
                    {"gammaC", 1.4}, {"blockN", 0.0}, {"blockB", 0.0}, {"mu", 0.45}, {"weldLeg", 6.0}, {"weldFw", 485.0}};
        return c;
    }
    c.values = {{"db", 20.0}, {"rows", 3.0}, {"pitch", 70.0}, {"edgeV", 35.0}, {"thread", 1.0},
                {"webT", beam.tw}, {"webFy", beam.fy}, {"webFu", beam.fu}, {"beamDepth", beam.depth},
                {"supT", 12.0}, {"supFy", plateFy}, {"supFu", plateFu}};
    if (type == NodeConnection::kDoubleAngle) {
        c.values.insert("gaugeWeb", 50.0);
        c.values.insert("gaugeSupport", 50.0);
        c.values.insert("webEnd", 35.0);
        c.values.insert("supEdge", 35.0);
        c.values.insert("angleT", 9.5);
        c.values.insert("angleFy", plateFy);
        c.values.insert("angleFu", plateFu);
    } else {
        c.values.insert("edgeH", 35.0);
        c.values.insert("a", 60.0);
        c.values.insert("setback", 12.0);
        c.values.insert("plateT", 9.5);
        c.values.insert("plateFy", plateFy);
        c.values.insert("plateFu", plateFu);
        c.values.insert("weldLeg", 6.0);
        c.values.insert("weldFw", 485.0);
    }
    return c;
}

NodeConnection ConnectionService::makeTubular(const BeamInfo &chord, const QVector<BraceInfo> &braces)
{
    NodeConnection c;
    c.type = NodeConnection::kTubular;
    const double d0 = chord.geometry.value(QStringLiteral("D"), chord.geometry.value(QStringLiteral("d"), chord.depth > 0 ? chord.depth : 114.3));
    const double t0 = chord.geometry.value(QStringLiteral("t"), 6.0);
    int kind = D::TubularJointInput::TY;
    if (braces.size() >= 2) {
        kind = braces[1].sameSideAsFirst ? D::TubularJointInput::KGap : D::TubularJointInput::X;
    }
    BraceInfo b1 = braces.value(0), b2 = braces.value(1);
    if (b1.d <= 0.0) {
        b1.d = 60.3;
        b1.t = 4.0;
        b1.angleDeg = 90.0;
    }
    if (b2.d <= 0.0) {
        b2 = b1;
    }
    double gap = 20.0;
    if (kind == D::TubularJointInput::KGap) {
        const double s1 = std::sin(b1.angleDeg * 3.14159265358979 / 180.0), s2 = std::sin(b2.angleDeg * 3.14159265358979 / 180.0);
        const double s12 = std::sin((b1.angleDeg + b2.angleDeg) * 3.14159265358979 / 180.0);
        gap = (d0 / 2.0) * s1 * s2 / std::max(1e-6, s12) - b1.d / (2.0 * s1) - b2.d / (2.0 * s2);
        if (gap < b1.t + b2.t) {
            kind = D::TubularJointInput::KOverlap;
        }
    }
    c.values = {{"kindT", kind}, {"braceKind", 0.0}, {"d0", d0}, {"t0", t0}, {"fy0", chord.fy}, {"fu0", chord.fu}, {"d1", b1.d}, {"t1", b1.t}, {"fy1", b1.fy},
                {"th1", b1.angleDeg}, {"d2", b2.d}, {"t2", b2.t}, {"fy2", b2.fy}, {"th2", b2.angleDeg}, {"gap", std::max(gap, b1.t + b2.t)}, {"overlap", 50.0},
                {"hidden", 0.0}, {"ecc", 0.0}};
    return c;
}

ConnectionService::Evaluation ConnectionService::evaluate(const NodeConnection &c, const Forces &f)
{
    Evaluation e;
    switch (c.type) {
    case NodeConnection::kDoubleAngle: {
        const auto in = toDoubleAngle(c, f);
        e.checks = D::checkDoubleAngle(in);
        e.parts = D::partsDoubleAngle(in);
        break;
    }
    case NodeConnection::kGusset: {
        const auto in = toGusset(c, f);
        e.checks = D::checkGusset(in);
        e.parts = D::partsGusset(in);
        break;
    }
    case NodeConnection::kSplice:
    case NodeConnection::kBasePlate:
    case NodeConnection::kTubular: {
        // avalia cada combinacao e fica com a mais desfavoravel
        double worst = -1.0;
        for (const ForceCase &fc : casesOf(f)) {
            auto checks = checksOf(c, fc);
            const double ratio = D::worstRatio(checks);
            if (ratio > worst) {
                worst = ratio;
                e.checks = std::move(checks);
                e.governingCase = fc.name;
            }
        }
        if (c.type == NodeConnection::kSplice) {
            e.parts = D::partsSplice(toSplice(c, 0, 0, 0));
        } else if (c.type == NodeConnection::kBasePlate) {
            e.parts = D::partsBasePlate(toBasePlate(c, 0, 0, 0));
        } else {
            e.parts = D::partsTubularJoint(toTubular(c, ForceCase {}));
        }
        break;
    }
    default: {
        const auto in = toShearTab(c, f);
        e.checks = D::checkShearTab(in);
        e.parts = D::partsShearTab(in);
        break;
    }
    }
    e.maxRatio = D::worstRatio(e.checks);
    if (const auto *g = D::governing(e.checks)) {
        e.governing = QString::fromStdString(g->name);
    }
    e.description = describe(c);
    return e;
}

bool ConnectionService::propose(NodeConnection &c, const Forces &f)
{
    switch (c.type) {
    case NodeConnection::kDoubleAngle: {
        const auto result = D::proposeDoubleAngle(toDoubleAngle(c, f));
        if (!result.found) {
            return false;
        }
        fromDoubleAngle(c, result.input);
        return true;
    }
    case NodeConnection::kGusset: {
        const auto result = D::proposeGusset(toGusset(c, f));
        if (!result.found) {
            return false;
        }
        fromGusset(c, result.input);
        return true;
    }
    case NodeConnection::kSplice:
    case NodeConnection::kBasePlate: {
        // cada combinacao pode governar: propoe para a mais desfavoravel e reavalia ate todas atenderem
        for (int iteration = 0; iteration < 6; ++iteration) {
            const QVector<ForceCase> cases = casesOf(f);
            const ForceCase *worstCase = nullptr;
            double worst = -1.0;
            for (const ForceCase &fc : cases) {
                const double ratio = D::worstRatio(checksOf(c, fc));
                if (ratio > worst) {
                    worst = ratio;
                    worstCase = &fc;
                }
            }
            if (!worstCase || worst <= 1.0 + 1e-9) {
                return worstCase != nullptr;
            }
            if (c.type == NodeConnection::kSplice) {
                const auto result = D::proposeSplice(toSplice(c, worstCase->shear, worstCase->axial, worstCase->moment));
                if (!result.found) {
                    return false;
                }
                fromSplice(c, result.input);
            } else {
                const auto result = D::proposeBasePlate(toBasePlate(c, worstCase->shear, worstCase->axial, worstCase->moment));
                if (!result.found) {
                    return false;
                }
                fromBasePlate(c, result.input);
            }
        }
        return false;
    }
    case NodeConnection::kTubular:
        return false;
    default: {
        const auto result = D::proposeShearTab(toShearTab(c, f));
        if (!result.found) {
            return false;
        }
        fromShearTab(c, result.input);
        return true;
    }
    }
}

QString ConnectionService::describe(const NodeConnection &c)
{
    const int rows = static_cast<int>(c.values.value(QStringLiteral("rows"), 0.0) + 0.5);
    const QString bolts = QObject::tr("%1 parafuso(s) d%2 %3").arg(rows).arg(c.values.value(QStringLiteral("db")), 0, 'f', 0).arg(c.grade);
    auto v = [&c](const char *key) { return c.values.value(QString::fromLatin1(key), 0.0); };
    switch (c.type) {
    case NodeConnection::kDoubleAngle:
        return QObject::tr("2 cantoneiras t = %1 mm, %2").arg(v("angleT"), 0, 'f', 1).arg(bolts);
    case NodeConnection::kGusset: {
        const int lines = std::max(1, static_cast<int>(v("lines") + 0.5));
        return QObject::tr("Chapa de nó t = %1 mm, %2 parafusos d%3 %4, solda %5 mm").arg(v("gT"), 0, 'f', 1).arg(rows * lines).arg(v("db"), 0, 'f', 0).arg(c.grade).arg(v("weldLeg"), 0, 'f', 0);
    }
    case NodeConnection::kSplice: {
        const int fr = static_cast<int>(v("fRows") + 0.5), fl = static_cast<int>(v("fLines") + 0.5), wr = static_cast<int>(v("wRows") + 0.5), wc = static_cast<int>(v("wCols") + 0.5);
        return QObject::tr("Talas de mesa t = %1 mm%2 (%3 parafusos por lado), 2 talas de alma t = %4 mm (%5 parafusos por lado), d%6 %7")
            .arg(v("opT"), 0, 'f', 1).arg(v("ipT") > 0 ? QObject::tr(" + internas") : QString()).arg(fr * fl).arg(v("wpT"), 0, 'f', 1).arg(wr * wc).arg(v("db"), 0, 'f', 0).arg(c.grade);
    }
    case NodeConnection::kBasePlate:
        return QObject::tr("Placa %1 mm, %2 chumbadores d%3 mm, fck %4 MPa").arg(v("plateT"), 0, 'f', 1).arg(static_cast<int>(v("anchors") + 0.5)).arg(v("anchorD"), 0, 'f', 0).arg(v("fck"), 0, 'f', 0);
    case NodeConnection::kTubular:
        return QObject::tr("%1: banzo Ø%2×%3, diagonal Ø%4×%5").arg(kindName(static_cast<int>(v("kindT") + 0.5))).arg(v("d0"), 0, 'f', 1).arg(v("t0"), 0, 'f', 1).arg(v("d1"), 0, 'f', 1).arg(v("t1"), 0, 'f', 1);
    default:
        return QObject::tr("Chapa t = %1 mm, %2, solda %3 mm").arg(v("plateT"), 0, 'f', 1).arg(bolts).arg(v("weldLeg"), 0, 'f', 0);
    }
}

} // namespace Structura::App::Services
