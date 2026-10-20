#include "ConnectionService.h"
#include "ConnectionCatalog.h"

#include "design/EndPlateConnection.h"
#include "design/SupportChecks.h"
#include "design/BasePlate.h"
#include "design/ColdFormedConnections.h"
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

// ---- perfil formado a frio ------------------------------------------------------------------------------------------

D::ColdFormedInput toColdFormed(const NodeConnection &c, const ConnectionService::Forces &f)
{
    D::ColdFormedInput in;
    in.mode = static_cast<int>(value(c, "mode", 0.0) + 0.5);
    in.force = std::max({f.tension, f.compression, f.shear, std::abs(f.axial)});
    in.memberResistance = value(c, "memberNRd", 0.0) * 1000.0;
    in.t1 = value(c, "t1", 2.0);
    in.fy1 = value(c, "fy1", 345.0);
    in.fu1 = value(c, "fu1", 450.0);
    in.t2 = value(c, "t2", 4.75);
    in.fy2 = value(c, "fy2", 250.0);
    in.fu2 = value(c, "fu2", 400.0);
    in.d = value(c, "d", 12.0);
    in.n = count(c, "n", 2);
    in.lines = count(c, "lines", 1);
    in.pitch = value(c, "pitch", 40.0);
    in.gauge = value(c, "gauge", 40.0);
    in.end1 = value(c, "end1", 30.0);
    in.end2 = value(c, "end2", 30.0);
    in.edge = value(c, "edge", 25.0);
    in.fub = value(c, "fub", 400.0);
    in.thread = value(c, "thread", 1.0) != 0.0;
    in.planes = count(c, "planes", 1);
    in.leg = value(c, "leg", 3.0);
    in.lengthLong = value(c, "lLong", 0.0);
    in.lengthTrans = value(c, "lTrans", 0.0);
    in.fw = value(c, "fw", 485.0);
    in.curvedRe = value(c, "cRe", 6.0);
    in.curvedTwoSurfaces = value(c, "cTwo", 0.0) != 0.0;
    in.curvedSalient = value(c, "cSalient", 0.0) != 0.0;
    in.curvedW1 = value(c, "cW1", 3.0);
    in.curvedW2 = value(c, "cW2", 3.0);
    in.curvedStiffenerHeight = value(c, "cH", 0.0);
    in.curvedParallel = value(c, "cParallel", 1.0) != 0.0;
    in.fssRk = value(c, "fss", 0.0) * 1000.0;
    in.ftsRk = value(c, "fts", 0.0) * 1000.0;
    in.headDiameter = value(c, "dh", 8.0);
    in.washerDiameter = value(c, "dw", 0.0);
    in.washerThickness = value(c, "tw", 0.0);
    in.tension = value(c, "tens", 0.0) * 1000.0;
    return in;
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
    in.webPlates = std::clamp(static_cast<int>(value(c, "wN", 2.0) + 0.5), 1, 2);
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
    in.ribs = static_cast<int>(value(c, "ribs", 0.0) + 0.5);
    in.ribThickness = value(c, "ribT", 12.5);
    in.ribHeight = value(c, "ribH", 150.0);
    in.ribLength = value(c, "ribL", 100.0);
    in.ribWeldLeg = value(c, "ribW", 6.0);
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

// ---- ligacao rigida por chapa de extremidade ------------------------------------------------------------------------

D::EndPlateInput toEndPlate(const NodeConnection &c, double shear, double axial, double moment, double panelShear = -1.0, double columnAxial = 0.0)
{
    D::EndPlateInput in;
    in.grade = gradeByName(c.grade);
    in.db = value(c, "db", 20.0);
    in.threadInShearPlane = value(c, "thread", 1.0) != 0.0;
    in.beamDepth = value(c, "beamD", 300.0);
    in.flangeWidth = value(c, "bfl", 150.0);
    in.flangeThickness = value(c, "tfl", 10.0);
    in.webThickness = value(c, "tw", 6.0);
    in.beam = {value(c, "beamFy", 250.0), value(c, "beamFu", 400.0)};
    in.plateWidth = value(c, "plateW", 160.0);
    in.plateThickness = value(c, "plateT", 19.0);
    in.plate = {value(c, "plateFy", 250.0), value(c, "plateFu", 400.0)};
    in.tensionRows = count(c, "tRows", 2);
    in.shearRows = count(c, "sRows", 1);
    in.pitch = value(c, "pitch", 70.0);
    in.gauge = value(c, "gauge", 90.0);
    in.firstRow = value(c, "firstRow", 40.0);
    in.edgeVertical = value(c, "edgeV", 35.0);
    in.weldFlangeLeg = value(c, "weldF", 8.0);
    in.weldWebLeg = value(c, "weldW", 6.0);
    in.weldFw = value(c, "weldFw", 485.0);
    auto positive = [&c](const char *key, double fallback) {
        const double v = value(c, key, 0.0);
        return v > 0.0 ? v : fallback;
    };
    in.colDepth = positive("supD", 260.0);
    in.colFlangeWidth = positive("supBf", 146.0);
    in.colFlangeThickness = positive("supTf", 12.7);
    in.colWebThickness = positive("supTw", 7.6);
    in.colRadius = value(c, "supR", 10.0);
    in.column = {value(c, "supFy", 250.0), value(c, "supFu", 400.0)};
    in.colUnbraced = value(c, "supLb", 0.0);
    in.continuityPlates = value(c, "cont", 0.0) != 0.0;
    in.stiffenerThickness = value(c, "contT", 9.5);
    in.stiffenerWidth = value(c, "contW", 60.0);
    in.doublerThickness = value(c, "doubler", 0.0);
    in.shear = shear;
    in.axial = axial;
    in.moment = moment;
    in.panelShear = panelShear;
    in.colAxial = panelShear >= 0.0 ? columnAxial : value(c, "colN", 0.0) * 1000.0;
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
    case NodeConnection::kEndPlate:
        return D::checkEndPlate(toEndPlate(c, fc.shear, fc.axial, fc.moment, fc.panelShear, fc.columnAxial));
    case NodeConnection::kSplice: {
        const auto in = toSplice(c, fc.shear, fc.axial, fc.moment);
        auto checks = D::checkSplice(in);
        const int slip = static_cast<int>(value(c, "slip", 0.0) + 0.5);
        if (slip > 0) {
            // atrito (6.3.4): mesas com a forca M/(d - tf) + parte do axial; alma com o cortante, duas talas (dois planos)
            const double flangeShare = in.beamArea > 0.0 ? in.flangeWidth * in.flangeThickness / in.beamArea : 0.5;
            D::SlipInput flange;
            flange.grade = in.grade;
            flange.db = in.db;
            flange.surfaceClass = static_cast<int>(value(c, "slipClass", 0.0) + 0.5);
            flange.fillers = value(c, "fillers", 0.0) != 0.0;
            flange.oversized = slip == 2;
            flange.bolts = in.flangeRows * in.flangeLines;
            flange.planes = in.innerPlateThickness > 0.0 ? 2 : 1;
            flange.shear = std::abs(in.moment) / std::max(in.beamDepth - in.flangeThickness, 1.0) + std::abs(in.axial) * flangeShare;
            for (auto check : D::checkSlip(flange)) {
                check.name += QObject::tr(" - talas das mesas").toStdString();
                checks.push_back(std::move(check));
            }
            D::SlipInput web = flange;
            web.bolts = in.webRows * in.webCols;
            web.planes = 2;
            web.shear = std::abs(in.shear);
            for (auto check : D::checkSlip(web)) {
                check.name += QObject::tr(" - talas da alma").toStdString();
                checks.push_back(std::move(check));
            }
        }
        return checks;
    }
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
    if (const auto *entry = ConnectionCatalog::find(type)) {
        return entry->name;
    }
    switch (type) {
    case NodeConnection::kDoubleAngle: return QObject::tr("Cantoneira dupla");
    case NodeConnection::kGusset: return QObject::tr("Chapa de nó (gusset)");
    case NodeConnection::kSplice: return QObject::tr("Emenda de viga");
    case NodeConnection::kBasePlate: return QObject::tr("Base de pilar");
    case NodeConnection::kTubular: return QObject::tr("Ligação tubular (CHS)");
    case NodeConnection::kColdFormed: return QObject::tr("Perfil formado a frio (NBR 14762)");
    case NodeConnection::kEndPlate: return QObject::tr("Ligação rígida viga–pilar (chapa de extremidade)");
    default: return QObject::tr("Chapa de cisalhamento");
    }
}

QString ConnectionService::typeSummary(int type)
{
    if (const auto *entry = ConnectionCatalog::find(type)) {
        return entry->summary;
    }
    switch (type) {
    case NodeConnection::kShearTab: return QObject::tr("Viga apoiada em pilar ou viga principal por chapa soldada ao apoio e parafusada na alma (ligação flexível).");
    case NodeConnection::kDoubleAngle: return QObject::tr("Viga apoiada por duas cantoneiras parafusadas na alma e no apoio.");
    case NodeConnection::kGusset: return QObject::tr("Diagonal, montante ou contraventamento (cantoneira simples/dupla ou chapa) parafusado em chapa de nó soldada ao banzo ou pilar: parafusos, diagonal, secção de Whitmore e solda.");
    case NodeConnection::kSplice: return QObject::tr("Continuidade de viga I/H com talas de mesa e de alma parafusadas: transmite momento, cortante e axial.");
    case NodeConnection::kBasePlate: return QObject::tr("Pilar I/H sobre bloco de concreto com chumbadores (NBR 8800:2024, 6.7): placa de base, chumbadores, pressão no concreto e atrito.");
    case NodeConnection::kEndPlate: return QObject::tr("Viga I/H soldada a chapa de extremidade parafusada na mesa do pilar: transmite momento e cortante (ligação rígida). Parafusos tracionados com efeito de alavanca, parafusos de cortante, soldas, mesa e alma do pilar (NBR 8800:2024, 5.7 e 6.3), enrijecedores e painel de alma.");
    case NodeConnection::kColdFormed: return QObject::tr("Perfil formado a frio (t ≤ 4,75 mm) ligado a chapa por parafusos, solda de filete ou parafusos autoatarraxantes (NBR 14762:2010, seção 10): rasgamento, pressão de contato, cisalhamento, solda e colapso por rasgamento.");
    default: return QObject::tr("Diagonais de seção circular soldadas ao banzo tubular (T, Y, X, K, N), ou chapa transversal: plastificação do banzo, punção e esmagamento (CIDECT DG1).");
    }
}

bool ConnectionService::usesBolts(int type)
{
    if (const auto *entry = ConnectionCatalog::find(type)) {
        return entry->usesBolts;
    }
    return type != NodeConnection::kTubular && type != NodeConnection::kBasePlate && type != NodeConnection::kColdFormed;
}

QPair<QString, QString> ConnectionService::steelKeys(int type)
{
    if (ConnectionCatalog::isCatalog(type)) {
        return ConnectionCatalog::steelKeys(type);
    }
    switch (type) {
    case NodeConnection::kDoubleAngle: return {QStringLiteral("angleFy"), QStringLiteral("angleFu")};
    case NodeConnection::kGusset: return {QStringLiteral("gFy"), QStringLiteral("gFu")};
    case NodeConnection::kColdFormed: return {QStringLiteral("fy2"), QStringLiteral("fu2")};
    case NodeConnection::kEndPlate:
    case NodeConnection::kSplice:
    case NodeConnection::kBasePlate:
    case NodeConnection::kShearTab: return {QStringLiteral("plateFy"), QStringLiteral("plateFu")};
    default: return {};
    }
}

bool ConnectionService::hasProposal(int type)
{
    if (ConnectionCatalog::isCatalog(type)) {
        return ConnectionCatalog::hasProposal(type);
    }
    return type != NodeConnection::kTubular && type != NodeConnection::kColdFormed && type != NodeConnection::kEndPlate;
}

QVector<ConnectionService::FieldDef> ConnectionService::fields(int type)
{
    if (ConnectionCatalog::isCatalog(type)) {
        return ConnectionCatalog::fields(type);
    }
    QVector<FieldDef> list;
    QString group;
    auto section = [&group](const QString &title) { group = title; };
    auto add = [&](const char *key, const QString &label, double min, double max, int decimals, const QString &suffix, const QStringList &options = {}, const QString &tip = {}) {
        FieldDef def {QString::fromLatin1(key), label, min, max, decimals, suffix, group, options, tip};
        list.append(def);
    };
    const QString mm = QObject::tr(" mm"), mpa = QObject::tr(" MPa"), deg = QObject::tr(" °"), mm2 = QObject::tr(" mm²");
    auto supportMember = [&]() {
        section(QObject::tr("Perfil de apoio (verificação local da mesa/alma)"));
        add("supType", QObject::tr("Chapa soldada na"), 0, 2, 0, QString(), {QObject::tr("(não verificar)"), QObject::tr("Face da mesa"), QObject::tr("Face da alma")},
            QObject::tr("Pilar ou viga principal de perfil I/H onde a chapa está soldada. Verifica flexão local da mesa, escoamento local e enrugamento da alma (NBR 8800, 5.7)."));
        add("supD", QObject::tr("Altura do perfil de apoio d"), 0, 2000, 1, mm);
        add("supBf", QObject::tr("Largura da mesa bf"), 0, 1000, 1, mm);
        add("supTf", QObject::tr("Espessura da mesa tf"), 0, 100, 2, mm);
        add("supTw", QObject::tr("Espessura da alma tw"), 0, 60, 2, mm);
        add("supR", QObject::tr("Raio de concordância mesa–alma"), 0, 60, 1, mm);
        add("supEnd", QObject::tr("Posição da ligação no perfil de apoio"), 0, 1, 0, QString(), {QObject::tr("Intermediária (apoio contínuo)"), QObject::tr("Extremidade do perfil")});
        add("supLb", QObject::tr("Comprimento destravado do apoio ℓ (0 = mesas contidas)"), 0, 20000, 0, mm, {},
            QObject::tr("NBR 8800:2024, 5.7.5: flambagem lateral da alma sob compressão localizada. Informe o maior comprimento destravado das mesas na seção da força; 0 dispensa a verificação."));
    };
    switch (type) {
    case NodeConnection::kGusset: {
        section(QObject::tr("Parafusos"));
        add("db", QObject::tr("Diâmetro do parafuso"), 10, 40, 1, mm);
        add("rows", QObject::tr("Parafusos ao longo da força"), 1, 12, 0, QString());
        add("lines", QObject::tr("Linhas de parafusos"), 1, 2, 0, QString());
        add("slip", QObject::tr("Ligação por atrito (6.3.4)"), 0, 2, 0, QString(),
            {QObject::tr("Não (contato)"), QObject::tr("Sim – furos-padrão (ELS)"), QObject::tr("Sim – furos alargados/alongados (ELU)")},
            QObject::tr("Exige parafusos ASTM F3125 A325/A490 com protensão (Tabela 19). A verificação de deslizamento acrescenta-se às de contato."));
        add("slipClass", QObject::tr("Superfície de contato (μ)"), 0, 2, 0, QString(), {QObject::tr("Classe A (0,30)"), QObject::tr("Classe B – jateada (0,50)"), QObject::tr("Galvanizada (0,20)")});
        add("fillers", QObject::tr("Chapas de enchimento (Ce)"), 0, 1, 0, QString(), {QObject::tr("Nenhuma ou uma"), QObject::tr("Duas ou mais (0,85)")});
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
        supportMember();
        break;
    }
    case NodeConnection::kSplice: {
        section(QObject::tr("Parafusos"));
        add("db", QObject::tr("Diâmetro do parafuso"), 10, 40, 1, mm);
        add("slip", QObject::tr("Ligação por atrito (6.3.4)"), 0, 2, 0, QString(),
            {QObject::tr("Não (contato)"), QObject::tr("Sim – furos-padrão (ELS)"), QObject::tr("Sim – furos alargados/alongados (ELU)")},
            QObject::tr("Exige parafusos ASTM F3125 A325/A490 com protensão (Tabela 19). O deslizamento é verificado nas talas das mesas (M/(d−tf) + parcela do axial) e da alma (cortante)."));
        add("slipClass", QObject::tr("Superfície de contato (μ)"), 0, 2, 0, QString(), {QObject::tr("Classe A (0,30)"), QObject::tr("Classe B – jateada (0,50)"), QObject::tr("Galvanizada (0,20)")});
        add("fillers", QObject::tr("Chapas de enchimento (Ce)"), 0, 1, 0, QString(), {QObject::tr("Nenhuma ou uma"), QObject::tr("Duas ou mais (0,85)")});
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
    case NodeConnection::kEndPlate: {
        section(QObject::tr("Parafusos"));
        add("db", QObject::tr("Diâmetro do parafuso"), 12, 40, 1, mm);
        add("tRows", QObject::tr("Fileiras tracionadas (2 parafusos cada)"), 1, 3, 0, QString());
        add("sRows", QObject::tr("Fileiras de cortante, zona comprimida (2 parafusos cada)"), 1, 3, 0, QString());
        add("pitch", QObject::tr("Espaçamento entre fileiras"), 30, 300, 1, mm);
        add("gauge", QObject::tr("Gabarito entre as colunas de parafusos"), 40, 400, 1, mm);
        add("firstRow", QObject::tr("Face externa da mesa – 1ª fileira (b + solda)"), 20, 200, 1, mm);
        add("edgeV", QObject::tr("Última fileira – borda da chapa"), 15, 150, 1, mm);
        section(QObject::tr("Chapa de extremidade"));
        add("plateW", QObject::tr("Largura da chapa"), 60, 600, 1, mm);
        add("plateT", QObject::tr("Espessura da chapa"), 6, 63, 1, mm);
        add("plateFy", QObject::tr("fy da chapa"), 150, 800, 0, mpa);
        add("plateFu", QObject::tr("fu da chapa"), 250, 1000, 0, mpa);
        section(QObject::tr("Viga"));
        add("beamD", QObject::tr("Altura da viga"), 100, 2000, 1, mm);
        add("bfl", QObject::tr("Largura da mesa"), 50, 600, 1, mm);
        add("tfl", QObject::tr("Espessura da mesa"), 4, 60, 1, mm);
        add("tw", QObject::tr("Espessura da alma"), 3, 40, 2, mm);
        add("beamFy", QObject::tr("fy da viga"), 150, 800, 0, mpa);
        add("beamFu", QObject::tr("fu da viga"), 250, 1000, 0, mpa);
        section(QObject::tr("Soldas viga–chapa"));
        add("weldF", QObject::tr("Perna do filete das mesas"), 3, 25, 1, mm);
        add("weldW", QObject::tr("Perna do filete da alma"), 3, 25, 1, mm);
        add("weldFw", QObject::tr("fw do metal da solda"), 415, 550, 0, mpa);
        section(QObject::tr("Pilar: enrijecedores e reforço (5.7.7 e 5.7.9)"));
        add("cont", QObject::tr("Enrijecedores horizontais alinhados com as mesas da viga"), 0, 1, 0, QString(), {QObject::tr("Não"), QObject::tr("Sim")},
            QObject::tr("Com enrijecedores, as verificações locais da mesa e da alma (5.7.2 a 5.7.5) são substituídas pelo dimensionamento dos enrijecedores (5.7.9)."));
        add("contT", QObject::tr("Espessura do enrijecedor"), 4, 40, 1, mm);
        add("contW", QObject::tr("Largura de cada enrijecedor"), 20, 300, 1, mm);
        add("doubler", QObject::tr("Chapa de reforço da alma do pilar (0 = nenhuma)"), 0, 30, 1, mm);
        add("supFy", QObject::tr("fy do pilar"), 150, 800, 0, mpa);
        add("supFu", QObject::tr("fu do pilar"), 250, 1000, 0, mpa);
        supportMember();
        break;
    }
    case NodeConnection::kColdFormed: {
        const QString kn = QObject::tr(" kN");
        section(QObject::tr("Meio de ligação"));
        add("mode", QObject::tr("Meio de ligação"), 0, 3, 0, QString(), {QObject::tr("Parafusos comuns (10.3)"), QObject::tr("Solda de filete (10.2)"), QObject::tr("Parafusos autoatarraxantes (10.5)"), QObject::tr("Filete em superfície curva (10.2.4)")},
            QObject::tr("Perfil formado a frio sobreposto a uma chapa de ligação (t ≤ 4,75 mm). As distâncias são medidas a partir do centro do furo."));
        add("memberNRd", QObject::tr("Resistência de cálculo da barra (0 = sem mínimo de 50 %)"), 0, 100000, 1, kn, {},
            QObject::tr("10.1-a: a ligação de barra axialmente solicitada é dimensionada no mínimo para 50 % da força axial resistente de cálculo."));
        section(QObject::tr("Perfil formado a frio"));
        add("t1", QObject::tr("Espessura do perfil t1"), 0.5, 4.75, 2, mm);
        add("fy1", QObject::tr("fy do perfil"), 150, 800, 0, mpa);
        add("fu1", QObject::tr("fu do perfil"), 250, 1000, 0, mpa);
        section(QObject::tr("Chapa de ligação"));
        add("t2", QObject::tr("Espessura da chapa t2"), 0.5, 4.75, 2, mm);
        add("fy2", QObject::tr("fy da chapa"), 150, 800, 0, mpa);
        add("fu2", QObject::tr("fu da chapa"), 250, 1000, 0, mpa);
        section(QObject::tr("Parafusos (comuns ou autoatarraxantes)"));
        add("d", QObject::tr("Diâmetro nominal d"), 2, 36, 2, mm);
        add("n", QObject::tr("Parafusos ao longo da força"), 1, 20, 0, QString());
        add("lines", QObject::tr("Linhas de parafusos"), 1, 4, 0, QString());
        add("pitch", QObject::tr("Espaçamento ao longo da força"), 6, 300, 1, mm);
        add("gauge", QObject::tr("Espaçamento entre linhas"), 6, 300, 1, mm);
        add("end1", QObject::tr("Furo extremo – extremidade do perfil"), 5, 200, 1, mm);
        add("end2", QObject::tr("Furo extremo – extremidade da chapa"), 5, 200, 1, mm);
        add("edge", QObject::tr("Furo – borda transversal"), 5, 200, 1, mm);
        add("fub", QObject::tr("fub do parafuso comum (Tabela 16)"), 300, 1100, 0, mpa);
        add("thread", QObject::tr("Plano de corte do parafuso comum"), 0, 1, 0, QString(), {QObject::tr("Fora da rosca (0,5)"), QObject::tr("Pela rosca (0,4)")});
        add("planes", QObject::tr("Planos de corte do parafuso comum"), 1, 2, 0, QString());
        add("fss", QObject::tr("F_ss,Rk do autoatarraxante (fabricante; 0 = não informado)"), 0, 100, 2, kn);
        add("fts", QObject::tr("F_ts,Rk do autoatarraxante (fabricante; 0 = não informado)"), 0, 100, 2, kn);
        add("dh", QObject::tr("Diâmetro da cabeça dh (autoatarraxante)"), 4, 30, 1, mm);
        add("dw", QObject::tr("Diâmetro da arruela dw (0 = sem)"), 0, 30, 1, mm);
        add("tw", QObject::tr("Espessura da arruela"), 0, 6, 2, mm);
        add("tens", QObject::tr("Tração total nos parafusos"), 0, 10000, 1, kn);
        section(QObject::tr("Solda de filete"));
        add("leg", QObject::tr("Perna do filete"), 1.5, 10, 1, mm);
        add("lLong", QObject::tr("Comprimento de filetes paralelos à força"), 0, 2000, 0, mm);
        add("lTrans", QObject::tr("Comprimento de filetes normais à força"), 0, 2000, 0, mm);
        add("fw", QObject::tr("fw do metal da solda"), 415, 550, 0, mpa);
        section(QObject::tr("Filete em superfície curva (10.2.4; usa L// ou L⟂ acima)"));
        add("cParallel", QObject::tr("Solicitação em relação ao eixo da solda"), 0, 1, 0, QString(), {QObject::tr("Normal ao eixo (Fig. 7a)"), QObject::tr("Paralela ao eixo (Fig. 7b a 7g)")});
        add("cRe", QObject::tr("Raio externo de dobramento re"), 0.5, 40, 1, mm);
        add("cTwo", QObject::tr("Superfícies curvas ligadas"), 0, 1, 0, QString(), {QObject::tr("Uma"), QObject::tr("Duas")});
        add("cSalient", QObject::tr("Face externa do filete"), 0, 1, 0, QString(), {QObject::tr("Rente ao metal-base (tef = 0,3 re ou 0,5 re)"), QObject::tr("Saliente (tef = 0,7 w)")});
        add("cW1", QObject::tr("Perna w1 (face saliente)"), 1, 20, 1, mm);
        add("cW2", QObject::tr("Perna w2 (face saliente)"), 1, 20, 1, mm);
        add("cH", QObject::tr("Altura h do enrijecedor (0 = menor que L)"), 0, 500, 0, mm);
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
        add("slip", QObject::tr("Ligação por atrito (6.3.4)"), 0, 2, 0, QString(),
            {QObject::tr("Não (contato)"), QObject::tr("Sim – furos-padrão (ELS)"), QObject::tr("Sim – furos alargados/alongados (ELU)")},
            QObject::tr("Exige parafusos ASTM F3125 A325/A490 com protensão (Tabela 19). A verificação de deslizamento acrescenta-se às de contato."));
        add("slipClass", QObject::tr("Superfície de contato (μ)"), 0, 2, 0, QString(), {QObject::tr("Classe A (0,30)"), QObject::tr("Classe B – jateada (0,50)"), QObject::tr("Galvanizada (0,20)")});
        add("fillers", QObject::tr("Chapas de enchimento (Ce)"), 0, 1, 0, QString(), {QObject::tr("Nenhuma ou uma"), QObject::tr("Duas ou mais (0,85)")});
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
        supportMember();
        break;
    }
    }
    return list;
}

NodeConnection ConnectionService::makeDefault(int type, const BeamInfo &beam, double plateFy, double plateFu)
{
    if (ConnectionCatalog::isCatalog(type)) {
        return ConnectionCatalog::makeDefault(type, beam, plateFy, plateFu);
    }
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
    if (type == NodeConnection::kEndPlate) {
        const double bf = beam.bf > 0 ? beam.bf : 150.0;
        const double tfl = beam.tf > 0 ? beam.tf : 10.0;
        const double db = 20.0;
        c.values = {{"db", db}, {"thread", 1.0}, {"tRows", 2.0}, {"sRows", 1.0}, {"pitch", 70.0}, {"gauge", std::max(3.0 * db, std::round(bf * 0.6))}, {"firstRow", 40.0}, {"edgeV", 35.0},
                    {"plateW", std::round(bf + 20.0)}, {"plateT", 22.4}, {"plateFy", plateFy}, {"plateFu", plateFu},
                    {"beamD", beam.depth}, {"bfl", bf}, {"tfl", tfl}, {"tw", beam.tw}, {"beamFy", beam.fy}, {"beamFu", beam.fu},
                    {"weldF", std::min(12.0, std::max(5.0, std::round(tfl * 0.8)))}, {"weldW", 6.0}, {"weldFw", 485.0},
                    {"cont", 0.0}, {"contT", 9.5}, {"contW", 60.0}, {"doubler", 0.0},
                    {"supFy", plateFy}, {"supFu", plateFu}};
        return c;
    }
    if (type == NodeConnection::kColdFormed) {
        const double t = std::min(4.75, beam.geometry.value(QStringLiteral("t"), beam.tw > 0 ? beam.tw : 2.0));
        c.values = {{"mode", 0.0}, {"memberNRd", std::round(beam.area * beam.fy / 1.20 / 100.0) / 10.0}, {"t1", t}, {"fy1", beam.fy}, {"fu1", beam.fu}, {"t2", 4.75},
                    {"fy2", plateFy}, {"fu2", plateFu}, {"d", 12.0}, {"n", 2.0}, {"lines", 1.0}, {"pitch", 40.0}, {"gauge", 40.0}, {"end1", 30.0}, {"end2", 30.0},
                    {"edge", 25.0}, {"fub", 400.0}, {"thread", 1.0}, {"planes", 1.0}, {"fss", 0.0}, {"fts", 0.0}, {"dh", 8.0}, {"dw", 0.0}, {"tw", 0.0}, {"tens", 0.0},
                    {"leg", std::min(t, 3.0)}, {"lLong", 80.0}, {"lTrans", 0.0}, {"fw", 485.0}, {"cParallel", 1.0}, {"cRe", 6.0}, {"cTwo", 0.0}, {"cSalient", 0.0}, {"cW1", 3.0}, {"cW2", 3.0}, {"cH", 0.0}};
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

std::pair<double, double> ConnectionService::supportDemand(const NodeConnection &c, const Forces &f)
{
    if (c.type == NodeConnection::kGusset) {
        // forca da diagonal tratada como normal a face (conservador)
        return {std::max(f.tension, std::max(f.axial, 0.0)), std::max(f.compression, std::max(-f.axial, 0.0))};
    }
    if (c.type != NodeConnection::kShearTab && c.type != NodeConnection::kDoubleAngle) {
        return {0.0, 0.0};
    }
    const double height = (count(c, "rows", 3) - 1) * value(c, "pitch", 70.0) + 2.0 * value(c, "edgeV", 35.0);
    const double eccentricity = c.type == NodeConnection::kDoubleAngle ? value(c, "gaugeWeb", 40.0) : value(c, "a", 60.0);
    const double couple = height > 0.0 ? 1.5 * f.shear * eccentricity / height : 0.0;   // resultante da zona tracionada/comprimida pelo momento V.e
    return {std::max(f.axial, 0.0) + couple, std::max(-f.axial, 0.0) + couple};
}

/// Verificacao local do perfil de apoio (mesa ou alma onde a chapa esta soldada), quando a ligacao informa o perfil.
static void appendSupportChecks(ConnectionService::Evaluation &e, const NodeConnection &c, const ConnectionService::Forces &f)
{
    double along = 0.0, transverse = 0.0;
    if (c.type == NodeConnection::kGusset) {
        along = value(c, "gWidth", 200.0);
        transverse = value(c, "gT", 9.5) + 2.0 * value(c, "weldLeg", 6.0);
    } else {
        const double height = (count(c, "rows", 3) - 1) * value(c, "pitch", 70.0) + 2.0 * value(c, "edgeV", 35.0);
        along = std::max(0.5 * height, 1.0);
        const double plate = c.type == NodeConnection::kDoubleAngle ? 2.0 * value(c, "angleT", 9.5) : value(c, "plateT", 9.5);
        transverse = plate + 2.0 * value(c, "weldLeg", 6.0);
    }
    const auto own = ConnectionService::supportDemand(c, f);
    ConnectionService::appendSupportLocal(e, c, f, along, transverse, own.first, own.second);
}

void ConnectionService::appendSupportLocal(Evaluation &e, const NodeConnection &c, const Forces &f, double lengthAlong, double lengthTransverse, double tension, double compression)
{
    const int face = static_cast<int>(value(c, "supType", 0.0) + 0.5);
    if (face != 1 && face != 2) {
        return;
    }
    D::SupportInput in;
    in.face = face;
    in.d = value(c, "supD", 0.0);
    in.bf = value(c, "supBf", 0.0);
    in.tf = value(c, "supTf", 0.0);
    in.tw = value(c, "supTw", 0.0);
    in.k = value(c, "supTf", 0.0) + value(c, "supR", 0.0);
    in.fy = value(c, "supFy", 250.0);
    in.fu = value(c, "supFu", 400.0);
    in.atMemberEnd = value(c, "supEnd", 1.0) != 0.0;
    in.unbracedLength = value(c, "supLb", 0.0);
    in.lengthAlong = lengthAlong;
    in.lengthTransverse = lengthTransverse;
    // as demais ligacoes que chegam a mesma barra de apoio no mesmo no carregam o mesmo perfil: somam-se (conservador)
    in.tension = tension + f.hostTension;
    in.compression = compression + f.hostCompression;
    for (auto &check : D::checkSupportMember(in)) {
        if (f.hostConnections > 0) {
            check.name += QObject::tr(" (com mais %1 ligação(ões) na barra de apoio)").arg(f.hostConnections).toStdString();
        }
        e.checks.push_back(std::move(check));
    }
}

ConnectionService::Evaluation ConnectionService::evaluate(const NodeConnection &c, const Forces &f)
{
    if (ConnectionCatalog::isCatalog(c.type)) {
        return ConnectionCatalog::evaluate(c, f);
    }
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
    case NodeConnection::kColdFormed: {
        const auto in = toColdFormed(c, f);
        e.checks = D::checkColdFormed(in);
        e.parts = D::partsColdFormed(in);
        break;
    }
    case NodeConnection::kEndPlate:
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
        if (c.type == NodeConnection::kEndPlate) {
            e.parts = D::partsEndPlate(toEndPlate(c, 0, 0, 0));
        } else if (c.type == NodeConnection::kSplice) {
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
    if (c.type == NodeConnection::kShearTab || c.type == NodeConnection::kDoubleAngle || c.type == NodeConnection::kGusset) {
        appendSupportChecks(e, c, f);
        const int slip = static_cast<int>(value(c, "slip", 0.0) + 0.5);
        if (slip > 0) {
            D::SlipInput in;
            in.grade = gradeByName(c.grade);
            in.db = value(c, "db", 20.0);
            in.surfaceClass = static_cast<int>(value(c, "slipClass", 0.0) + 0.5);
            in.fillers = value(c, "fillers", 0.0) != 0.0;
            in.oversized = slip == 2;
            if (c.type == NodeConnection::kGusset) {
                in.bolts = count(c, "rows", 3) * count(c, "lines", 1);
                in.planes = count(c, "bars", 1);
                in.shear = std::max(f.tension, f.compression);
            } else {
                in.bolts = count(c, "rows", 3);
                in.planes = c.type == NodeConnection::kDoubleAngle ? 2 : 1;
                in.shear = f.shear;
                in.tension = std::max(f.axial, 0.0);
            }
            for (auto &check : D::checkSlip(in)) {
                e.checks.push_back(std::move(check));
            }
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
    if (ConnectionCatalog::isCatalog(c.type)) {
        return ConnectionCatalog::propose(c, f);
    }
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
    case NodeConnection::kEndPlate:
    case NodeConnection::kColdFormed:
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
    if (ConnectionCatalog::isCatalog(c.type)) {
        return ConnectionCatalog::describe(c);
    }
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
    case NodeConnection::kEndPlate:
        return QObject::tr("Chapa %1 × %2 mm, %3 parafusos d%4 %5, solda %6/%7 mm%8").arg(v("plateT"), 0, 'f', 1).arg(v("plateW"), 0, 'f', 0)
            .arg(2 * (static_cast<int>(v("tRows") + 0.5) + static_cast<int>(v("sRows") + 0.5))).arg(v("db"), 0, 'f', 0).arg(c.grade)
            .arg(v("weldF"), 0, 'f', 0).arg(v("weldW"), 0, 'f', 0).arg(v("cont") != 0.0 ? QObject::tr(", enrijecedores no pilar") : QString());
    case NodeConnection::kColdFormed: {
        const int mode = static_cast<int>(v("mode") + 0.5);
        const int total = static_cast<int>(v("n") + 0.5) * std::max(1, static_cast<int>(v("lines") + 0.5));
        if (mode == 3) {
            return QObject::tr("Perfil t = %1 mm, filete em superfície curva re = %2 mm").arg(v("t1"), 0, 'f', 2).arg(v("cRe"), 0, 'f', 1);
        }
        if (mode == 1) {
            return QObject::tr("Perfil t = %1 mm soldado à chapa t = %2 mm, filete %3 mm").arg(v("t1"), 0, 'f', 2).arg(v("t2"), 0, 'f', 2).arg(v("leg"), 0, 'f', 1);
        }
        return QObject::tr("Perfil t = %1 mm e chapa t = %2 mm, %3 %4 d%5").arg(v("t1"), 0, 'f', 2).arg(v("t2"), 0, 'f', 2).arg(total)
            .arg(mode == 2 ? QObject::tr("parafusos autoatarraxantes") : QObject::tr("parafusos comuns")).arg(v("d"), 0, 'f', 1);
    }
    case NodeConnection::kTubular:
        return QObject::tr("%1: banzo Ø%2×%3, diagonal Ø%4×%5").arg(kindName(static_cast<int>(v("kindT") + 0.5))).arg(v("d0"), 0, 'f', 1).arg(v("t0"), 0, 'f', 1).arg(v("d1"), 0, 'f', 1).arg(v("t1"), 0, 'f', 1);
    default:
        return QObject::tr("Chapa t = %1 mm, %2, solda %3 mm").arg(v("plateT"), 0, 'f', 1).arg(bolts).arg(v("weldLeg"), 0, 'f', 0);
    }
}

} // namespace Structura::App::Services
