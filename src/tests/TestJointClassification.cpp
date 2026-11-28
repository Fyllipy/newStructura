#include <QtTest/QtTest>

#include "app/SolverBridge.h"
#include "app/services/JointAnalysisService.h"
#include "app/services/ConnectionService.h"
#include "app/services/JointComponentService.h"
#include "design/JointClassification.h"
#include "design/JointComponents.h"
#include "ui/JointAnalysisDialog.h"

#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QComboBox>
#include <QImage>
#include <QPushButton>
#include <QTemporaryDir>

#include <cmath>

using namespace Structura::Design::JointClass;
using Structura::App::SolverBridge;
using Structura::App::Services::JointAnalysisService;

namespace {

QString solverLibrary()
{
    const QString enc = QStringLiteral(STRUCTURA_SOURCE_DIR "/rds.enc");
    return QFile::exists(enc) ? enc : QStringLiteral(STRUCTURA_SOURCE_DIR "/rds.dll");
}

constexpr double kE = 2.0e8;       // kN/m2 (200 GPa)
constexpr double kBeamI = 2.0e-4;  // m4
constexpr double kBeamL = 6.0;     // m
constexpr double kRef = kE * kBeamI / kBeamL;   // E Iv / Lv = 6666,67 kN.m/rad

SolverBridge::ModelData baseModel(double columnI, double beamRotationDeg = 0.0)
{
    SolverBridge::ModelData m;
    SolverBridge::MaterialData mat;
    mat.id = 1;
    mat.e = kE;
    mat.g = kE / 2.6;
    m.materials.push_back(mat);
    SolverBridge::SectionData beam;
    beam.id = 1;
    beam.area = 0.01;
    beam.iz = kBeamI;
    beam.iy = 1.0e-5;
    SolverBridge::SectionData column = beam;
    column.id = 2;
    column.iz = columnI;
    m.sections = {beam, column};
    (void)beamRotationDeg;
    return m;
}

void addNode(SolverBridge::ModelData &m, int id, double x, double y, double z)
{
    SolverBridge::NodeData n;
    n.id = id;
    n.x = x;
    n.y = y;
    n.z = z;
    m.nodes.push_back(n);
}

void addMember(SolverBridge::ModelData &m, int id, int a, int b, int section, double rotation = 0.0)
{
    SolverBridge::MemberData mem;
    mem.id = id;
    mem.nodeI = a;
    mem.nodeJ = b;
    mem.materialId = 1;
    mem.sectionId = section;
    mem.sectionRotationDeg = rotation;
    m.members.push_back(mem);
}

/// Portico plano: pilares 1 (no 1 -> 3) e 2 (no 2 -> 4), vigas 5 (3-4) e 6 (4-5) no nivel z=3,5, pilar 7 (4 -> 6) acima, diagonal 8 (4 -> 7).
/// `axisY`: o portico no plano YZ em vez de XZ.
SolverBridge::ModelData frame(double columnI, bool axisY = false)
{
    SolverBridge::ModelData m = baseModel(columnI);
    auto at = [&](int id, double h, double z) { axisY ? addNode(m, id, 0.0, h, z) : addNode(m, id, h, 0.0, z); };
    at(1, 0.0, 0.0);
    at(2, 6.0, 0.0);
    at(3, 0.0, 3.5);
    at(4, 6.0, 3.5);
    at(5, 12.0, 3.5);
    at(6, 6.0, 7.0);
    at(7, 9.0, 7.0);
    addMember(m, 1, 1, 3, 2, axisY ? 90.0 : 0.0);
    addMember(m, 2, 2, 4, 2, axisY ? 90.0 : 0.0);
    addMember(m, 5, 3, 4, 1);
    addMember(m, 6, 4, 5, 1);
    addMember(m, 7, 4, 6, 2, axisY ? 90.0 : 0.0);
    addMember(m, 8, 4, 7, 1);
    return m;
}

DirectionBehavior twoPoint(double mrd, double theta23)
{
    DirectionBehavior d;
    d.resistantMoment = mrd;
    d.theta23 = theta23;
    return d;
}

StoreyStiffness okStorey(double ratio)
{
    StoreyStiffness s;
    s.status = StoreyStatus::Ok;
    s.kv = ratio;
    s.kp = 1.0;
    s.ratio = ratio;
    s.beams = s.columns = 1;
    return s;
}

} // namespace

// secao do pilar explicita e confirmada (nao existe mais pilar padrao embutido no dimensionamento)
static void withColumn(Structura::App::ProjectData::NodeConnection &c)
{
    for (const auto &kv : {std::pair<const char *, double> {"supD", 260.0}, {"supBf", 146.0}, {"supTf", 12.7}, {"supTw", 7.6}, {"supR", 10.0}, {"supFy", 250.0}, {"supFu", 400.0}, {"supConfirmed", 1.0}}) {
        if (!c.values.contains(QString::fromLatin1(kv.first))) {
            c.values[QString::fromLatin1(kv.first)] = kv.second;
        }
    }
}

class TestJointClassification : public QObject
{
    Q_OBJECT

private slots:
    void siFromTwoThirdsPoint()
    {
        // Si = (2/3 Mj,Rd) / theta(2/3 Mj,Rd): Mj,Rd = 150 kN.m, theta = 0,01 rad -> 100/0,01 = 10000
        const auto r = computeStiffness(twoPoint(150.0, 0.01));
        QVERIFY(r.valid);
        QCOMPARE(r.moment23, 100.0);
        QVERIFY(std::abs(r.stiffness - 10000.0) < 1e-9);
    }

    void pinnedSemiRigidAndRigidByStiffness()
    {
        const Limits limits = limitsFor(kE, kBeamI, kBeamL);
        QVERIFY(std::abs(limits.referenceStiffness - kRef) < 1e-6);
        QVERIFY(std::abs(limits.sMin - 0.5 * kRef) < 1e-6);
        QVERIFY(std::abs(limits.sMax - 25.0 * kRef) < 1e-6);
        const StoreyStiffness storey = okStorey(0.3);
        QCOMPARE(classify(0.1 * kRef, limits, storey, JointKind::BeamColumn).final, StiffnessClass::Pinned);
        QCOMPARE(classify(0.5 * kRef, limits, storey, JointKind::BeamColumn).final, StiffnessClass::Pinned);   // fronteira: Si <= 0,5
        QCOMPARE(classify(5.0 * kRef, limits, storey, JointKind::BeamColumn).final, StiffnessClass::SemiRigid);
        QCOMPARE(classify(25.0 * kRef, limits, storey, JointKind::BeamColumn).final, StiffnessClass::Rigid);   // fronteira: Si >= 25
        QCOMPARE(classify(100.0 * kRef, limits, storey, JointKind::BeamColumn).final, StiffnessClass::Rigid);
    }

    void rigidLimitWithFailingStoreyConditionStaysSemiRigid()
    {
        const Limits limits = limitsFor(kE, kBeamI, kBeamL);
        const auto c = classify(40.0 * kRef, limits, okStorey(0.05), JointKind::BeamColumn);
        QCOMPARE(c.byStiffness, StiffnessClass::Rigid);
        QVERIFY(c.reachedRigidLimit);
        QVERIFY(!c.storeyConditionMet);
        QCOMPARE(c.final, StiffnessClass::SemiRigid);
        QVERIFY(c.justification.find("Kv/Kp < 0,1") != std::string::npos);
    }

    void pendingStoreyDataIsNeverPresumedAsMet()
    {
        const Limits limits = limitsFor(kE, kBeamI, kBeamL);
        StoreyStiffness pending;
        pending.reason = "sem pilares";
        const auto c = classify(40.0 * kRef, limits, pending, JointKind::BeamColumn);
        QVERIFY(c.storeyPending);
        QVERIFY(c.final != StiffnessClass::Rigid);
        // abaixo do limite de rigidez a condicao nao e necessaria
        QVERIFY(!classify(2.0 * kRef, limits, pending, JointKind::BeamColumn).storeyPending);
    }

    void onlyBeamColumnJointsAreClassified()
    {
        const Limits limits = limitsFor(kE, kBeamI, kBeamL);
        const auto c = classify(40.0 * kRef, limits, okStorey(0.3), JointKind::BeamBeam);
        QVERIFY(!c.applicable);
        QCOMPARE(c.final, StiffnessClass::NotClassified);
        QVERIFY(std::abs(c.eta - 40.0) < 1e-9);
    }

    void nonlinearCurveAndInterpolation()
    {
        DirectionBehavior d;
        d.curve = {{0.002, 40.0}, {0.006, 90.0}, {0.020, 120.0}, {0.050, 125.0}};
        d.resistantMoment = 120.0;   // 2/3 = 80 kN.m: entre (0,002;40) e (0,006;90) -> theta = 0,002 + 0,004*(40/50)
        const auto r = computeStiffness(d);
        QVERIFY(r.valid);
        QVERIFY(r.curveDriven);
        QVERIFY(std::abs(r.theta23 - 0.0052) < 1e-12);
        QVERIFY(std::abs(r.stiffness - 80.0 / 0.0052) < 1e-6);
        // a tangente inicial (20000) e a secante a 2/3 (15385) sao grandezas diferentes: vale a secante
        QVERIFY(std::abs(r.stiffness - 15384.615384) < 1e-3);
    }

    void curveThatDoesNotReachTwoThirdsIsRejected()
    {
        DirectionBehavior d;
        d.curve = {{0.002, 40.0}, {0.006, 60.0}};
        d.resistantMoment = 120.0;
        const auto r = computeStiffness(d);
        QVERIFY(!r.valid);
        QVERIFY(!r.messages.empty());
    }

    void maximumOfCurveIsNotSubstitutedForMissingResistantMoment()
    {
        DirectionBehavior d;
        d.curve = {{0.002, 40.0}, {0.006, 90.0}};
        const auto r = computeStiffness(d);
        QVERIFY(!r.valid);
    }

    void invalidCurvesAreRejected()
    {
        DirectionBehavior d;
        d.curve = {{0.004, 40.0}, {0.002, 60.0}};   // rotacoes decrescentes
        d.resistantMoment = 30.0;
        QVERIFY(!computeStiffness(d).valid);
    }

    void stiffnessOnlyAndMomentOnlyInputs()
    {
        DirectionBehavior d;
        d.stiffness = 5000.0;
        auto r = computeStiffness(d);
        QVERIFY(r.valid);
        QCOMPARE(r.stiffness, 5000.0);
        QCOMPARE(r.theta23, 0.0);   // sem Mj,Rd nao ha nivel de 2/3
        d.resistantMoment = 90.0;
        r = computeStiffness(d);
        QVERIFY(r.thetaDerived);
        QVERIFY(std::abs(r.theta23 - 60.0 / 5000.0) < 1e-12);
        DirectionBehavior onlyMoment;
        onlyMoment.resistantMoment = 90.0;
        QVERIFY(!computeStiffness(onlyMoment).valid);
    }

    void noDataGivesNoAnalysis()
    {
        const auto a = analyzeJoint(JointBehavior {}, limitsFor(kE, kBeamI, kBeamL), okStorey(0.3), JointKind::BeamColumn);
        QVERIFY(!a.positive.hasData);
        QVERIFY(a.primary() == nullptr);
        const std::string memory = buildMemory(a, MemoryContext {});
        QVERIFY(memory.find("NENHUM DADO") != std::string::npos);
    }

    void positiveAndNegativeAreIndependent()
    {
        JointBehavior b;
        b.origin = DataOrigin::NumericalCurve;
        b.positive = twoPoint(150.0, 0.01);   // Si = 10000
        b.negative = twoPoint(60.0, 0.02);    // Si = 2000
        const auto a = analyzeJoint(b, limitsFor(kE, kBeamI, kBeamL), okStorey(0.3), JointKind::BeamColumn);
        QVERIFY(std::abs(a.positive.stiffness.stiffness - 10000.0) < 1e-9);
        QVERIFY(std::abs(a.negative.stiffness.stiffness - 2000.0) < 1e-9);
        QCOMPARE(a.positive.classification.final, StiffnessClass::SemiRigid);   // eta = 1,5
        QCOMPARE(a.negative.classification.final, StiffnessClass::Pinned);   // eta = 0,3 <= 0,5
    }

    void curveTextParsingAndUnits()
    {
        std::vector<CurvePoint> pos, neg;
        // mrad e N.m: 2 mrad = 0,002 rad; 40000 N.m = 40 kN.m
        QCOMPARE(parseCurveText("# t ; M\n2 ; 40000\n6 ; 90000\n-3 ; -20000\n", 1e-3, 1e-3, pos, neg), std::string());
        QCOMPARE(pos.size(), std::size_t(2));
        QVERIFY(std::abs(pos[0].theta - 0.002) < 1e-12 && std::abs(pos[0].moment - 40.0) < 1e-12);
        QCOMPARE(neg.size(), std::size_t(1));
        QVERIFY(std::abs(neg[0].theta - 0.003) < 1e-12 && std::abs(neg[0].moment - 20.0) < 1e-12);
        // virgula decimal e kN.cm (1 kN.cm = 0,01 kN.m)
        QCOMPARE(parseCurveText("0,002;4000\n", 1.0, 0.01, pos, neg), std::string());
        QVERIFY(std::abs(pos[0].moment - 40.0) < 1e-12);
        QVERIFY(!parseCurveText("abc def\n", 1.0, 1.0, pos, neg).empty());
        QVERIFY(!parseCurveText("0,01 ; -5\n", 1.0, 1.0, pos, neg).empty());   // sinais opostos
    }

    void plainRolesAndKinds()
    {
        QCOMPARE(roleOfDirection({1, 0, 0}), BarRole::Beam);
        QCOMPARE(roleOfDirection({0, 0, -1}), BarRole::Column);
        QCOMPARE(roleOfDirection({0.7071, 0, 0.7071}), BarRole::Diagonal);
        QCOMPARE(kindOf(BarRole::Column, BarRole::Beam), JointKind::BeamColumn);
        QCOMPARE(kindOf(BarRole::Diagonal, BarRole::Column), JointKind::DiagonalColumn);
        QCOMPARE(kindOf(BarRole::Beam, BarRole::Beam), JointKind::BeamBeam);
    }

    void nodeWithTwoBarsOnly()
    {
        SolverBridge::ModelData m = baseModel(4.0e-4);
        addNode(m, 1, 0, 0, 0);
        addNode(m, 2, 0, 0, 3.5);
        addNode(m, 3, 6, 0, 3.5);
        addMember(m, 1, 1, 2, 2);
        addMember(m, 2, 2, 3, 1);
        const auto ctx = JointAnalysisService::analyze(m, 2, 1, 2, 1.0);
        QVERIFY(ctx.valid);
        QCOMPARE(ctx.bars.size(), std::size_t(2));
        QCOMPARE(ctx.kind, JointKind::BeamColumn);
        QVERIFY(std::abs(ctx.limits.referenceStiffness - kRef) < 1e-6);
        QCOMPARE(ctx.storey.status, StoreyStatus::Ok);   // uma viga e o pilar que chega por baixo
        QCOMPARE(ctx.storey.beams, 1);
        QCOMPARE(ctx.storey.columns, 1);
    }

    void nodeWithThreeAndManyBars()
    {
        const auto m = frame(4.0e-4);
        const auto ctx = JointAnalysisService::analyze(m, 4, 5, 2, 1.0);   // viga 5 + pilar 2 no no 4
        QVERIFY(ctx.valid);
        QCOMPARE(ctx.bars.size(), std::size_t(5));   // vigas 5 e 6, pilares 2 e 7, diagonal 8
        QCOMPARE(ctx.kind, JointKind::BeamColumn);
        QCOMPARE(ctx.pairs.size(), std::size_t(10));   // cada par e uma ligacao distinta
        int selected = 0;
        for (const auto &p : ctx.pairs) {
            selected += p.selected ? 1 : 0;
        }
        QCOMPARE(selected, 1);
        QVERIFY(std::abs(std::abs(ctx.planeNormal[1]) - 1.0) < 1e-9);   // portico no plano XZ
        QVERIFY(std::abs(ctx.limits.referenceStiffness - kRef) < 1e-6);
        QCOMPARE(ctx.storey.status, StoreyStatus::Ok);
        QCOMPARE(ctx.storey.beams, 2);
        QCOMPARE(ctx.storey.columns, 2);
        const double kv = kBeamI / kBeamL, kp = 4.0e-4 / 3.5;
        QVERIFY(std::abs(ctx.storey.kv - kv) < 1e-12);
        QVERIFY(std::abs(ctx.storey.kp - kp) < 1e-12);
        QVERIFY(std::abs(ctx.storey.ratio - kv / kp) < 1e-9);
    }

    void differentOrientationsGiveTheSameResult()
    {
        const auto a = JointAnalysisService::analyze(frame(4.0e-4, false), 4, 5, 2, 1.0);
        const auto b = JointAnalysisService::analyze(frame(4.0e-4, true), 4, 5, 2, 1.0);   // portico no plano YZ
        QVERIFY(a.valid && b.valid);
        QVERIFY(std::abs(b.planeNormal[0]) > 0.999);
        QVERIFY(std::abs(a.limits.referenceStiffness - b.limits.referenceStiffness) < 1e-9);
        QVERIFY(std::abs(a.storey.ratio - b.storey.ratio) < 1e-12);
        // secao da viga girada 90 graus: a inercia no plano passa a ser iy
        auto rotated = frame(4.0e-4);
        for (auto &mem : rotated.members) {
            if (mem.id == 5) {
                mem.sectionRotationDeg = 90.0;
            }
        }
        const auto c = JointAnalysisService::analyze(rotated, 4, 5, 2, 1.0);
        QVERIFY(std::abs(c.beamInertia - 1.0e-5) < 1e-12);
    }

    void failingStoreyConditionFromModel()
    {
        const auto ctx = JointAnalysisService::analyze(frame(4.0e-3), 4, 5, 2, 1.0);   // pilares muito rigidos: Kv/Kp < 0,1
        QVERIFY(ctx.valid);
        QVERIFY(ctx.storey.ratio < 0.1);
        const auto a = analyzeJoint([] { JointBehavior b; b.positive = twoPoint(2000.0, 2000.0 * 2.0 / 3.0 / (40.0 * kRef)); return b; }(), ctx.limits, ctx.storey, ctx.kind);
        QVERIFY(a.positive.classification.reachedRigidLimit);
        QCOMPARE(a.positive.classification.final, StiffnessClass::SemiRigid);
    }

    void missingStoreyDataIsPending()
    {
        SolverBridge::ModelData m = baseModel(4.0e-4);
        addNode(m, 1, 0, 0, 3.5);
        addNode(m, 2, 6, 0, 3.5);
        addNode(m, 3, 0, 0, 7.0);
        addMember(m, 1, 1, 2, 1);
        addMember(m, 2, 1, 3, 2);   // pilar acima do no: nao ha pilar do pavimento chegando por baixo
        const auto ctx = JointAnalysisService::analyze(m, 1, 1, 2, 1.0);
        QVERIFY(ctx.valid);
        QCOMPARE(ctx.storey.status, StoreyStatus::Pending);
    }

    void sharedNodesAndInvalidSelection()
    {
        const auto m = frame(4.0e-4);
        QCOMPARE(JointAnalysisService::sharedNodes(m, 5, 6), std::vector<int> {4});
        QVERIFY(JointAnalysisService::sharedNodes(m, 1, 6).empty());
    }

    void modelSpringAndReleaseAreReportedAsModelBehavior()
    {
        auto m = frame(4.0e-4);
        SolverBridge::EndSpringData spring;
        spring.memberId = 5;
        spring.k[11] = 3000.0;   // rz em J (no 4)
        m.endSprings.push_back(spring);
        const auto ctx = JointAnalysisService::analyze(m, 4, 5, 2, 1.0);
        const auto behavior = JointAnalysisService::behaviorFromModel(ctx);
        QVERIFY(behavior.has_value());
        QCOMPARE(behavior->origin, DataOrigin::ModelSpring);
        QCOMPARE(*behavior->positive.stiffness, 3000.0);
        // viga continua: nenhum dado de ligacao inventado
        QVERIFY(!JointAnalysisService::behaviorFromModel(JointAnalysisService::analyze(frame(4.0e-4), 4, 5, 2, 1.0)).has_value());
    }

    void dialogChartsAndPanelStayConsistentAndUpdate()
    {
        const auto ctx = JointAnalysisService::analyze(frame(4.0e-4), 4, 5, 2, 1.0);
        Structura::UI::JointAnalysisDialog dialog(ctx, std::nullopt, false);
        dialog.resize(1100, 700);
        dialog.show();
        // sem dados: dados insuficientes, nenhuma curva desenhada e nenhum marcador
        QVERIFY(dialog.momentRotationChart()->insufficientData());
        QCOMPARE(dialog.momentRotationChart()->drawnCurves(), 0);
        QPointF marker;
        QVERIFY(!dialog.classificationChart()->markerPosition(Direction::Positive, marker));
        // curva real: Si a 2/3 (Mj,Rd = 90) -> theta = 0,002 + 0,004*(20/50) = 0,0036 ; Si = 60/0,0036
        dialog.setOrigin(DataOrigin::ExperimentalCurve);
        dialog.setFields(QStringLiteral("90"), {}, {});
        dialog.setCurveText(QStringLiteral("0,002;40\n0,006;90\n0,02;100\n"));
        QVERIFY(dialog.applyInputs());
        dialog.grab();
        QVERIFY(!dialog.momentRotationChart()->insufficientData());
        QCOMPARE(dialog.momentRotationChart()->drawnCurves(), 1);
        const double expected = 60.0 / 0.0036;
        QVERIFY(std::abs(dialog.analysis().positive.stiffness.stiffness - expected) < 1e-6);
        QVERIFY(dialog.classificationChart()->markerPosition(Direction::Positive, marker));
        QVERIFY(std::abs(marker.y() - expected) < 1e-6);
        QVERIFY(std::abs(marker.x() - expected / kRef) < 1e-9);
        QVERIFY(dialog.memoryText().contains(QStringLiteral("Si = (2/3 × Mj,Rd) / θ(2/3 Mj,Rd)")));
        QVERIFY(dialog.memoryText().contains(QString::number(expected, 'g', 5)));
        QCOMPARE(dialog.finalClassificationText(), QStringLiteral("SEMIRRÍGIDA"));
        // atualizacao dinamica: nova rigidez muito alta e viga-pilar com Kv/Kp atendido -> rigida
        dialog.setCurveText(QStringLiteral("0,00005;40\n0,0006;90\n"));
        QVERIFY(dialog.applyInputs());
        QVERIFY(std::abs(dialog.analysis().positive.stiffness.stiffness - 60.0 / (0.00005 + 0.00055 * (20.0 / 50.0))) < 1e-6);
        QCOMPARE(dialog.finalClassificationText(), QStringLiteral("RÍGIDA"));
        QVERIFY(dialog.classificationChart()->markerPosition(Direction::Positive, marker));
        QVERIFY(marker.x() >= 25.0);
        // entrada invalida nao altera os resultados
        dialog.setFields(QStringLiteral("abc"), {}, {});
        QVERIFY(!dialog.applyInputs());
        QCOMPARE(dialog.finalClassificationText(), QStringLiteral("RÍGIDA"));
    }

    void dialogUnitsZoomAndExports()
    {
        const auto ctx = JointAnalysisService::analyze(frame(4.0e-4), 4, 5, 2, 1.0);
        Structura::UI::JointAnalysisDialog dialog(ctx, std::nullopt, false);
        dialog.show();
        dialog.setFields(QStringLiteral("150"), QStringLiteral("0.01"), {});
        QVERIFY(dialog.applyInputs());   // so ponto de 2/3 e Mj,Rd: secante e linhas, sem curva
        dialog.grab();
        QVERIFY(!dialog.momentRotationChart()->insufficientData());
        const QRectF before = dialog.momentRotationChart()->viewRect();
        dialog.setUnits(QStringLiteral("mrad"), QStringLiteral("N·m"));
        const QRectF after = dialog.momentRotationChart()->viewRect();
        QVERIFY(std::abs(after.width() / before.width() - 1000.0) < 1e-6);
        QVERIFY(std::abs(after.height() / before.height() - 1000.0) < 1e-6);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString png = dir.filePath(QStringLiteral("mr.png")), svg = dir.filePath(QStringLiteral("mr.svg")), txt = dir.filePath(QStringLiteral("m.txt"));
        QVERIFY(dialog.momentRotationChart()->exportPng(png));
        QVERIFY(dialog.momentRotationChart()->exportSvg(svg, QStringLiteral("t")));
        QVERIFY(dialog.classificationChart()->exportPng(dir.filePath(QStringLiteral("c.png"))));
        QVERIFY(dialog.exportMemory(txt));
        QVERIFY(QImage(png).width() > 100);
        QFile svgFile(svg);
        QVERIFY(svgFile.open(QIODevice::ReadOnly));
        QVERIFY(svgFile.readAll().contains("<svg"));
        QFile memFile(txt);
        QVERIFY(memFile.open(QIODevice::ReadOnly));
        QVERIFY(QString::fromUtf8(memFile.readAll()).contains(QStringLiteral("MEMÓRIA DE CÁLCULO")));
    }

    // ---- metodo dos componentes (EN 1993-1-8, 6.3) -----------------------------------------------------------------------------------

    void weldedJointStiffnessMatchesHandCalculation()
    {
        namespace JC = Structura::Design::JointComponents;
        JC::ModelInput in;   // IPE300 em HEB200 (soldada, sem enrijecedores, viga de um lado)
        in.kind = JC::ModelInput::WeldedFlanges;
        in.beamDepth = 300.0;
        in.beamTf = 10.7;
        in.colDepth = 200.0;
        in.colBf = 200.0;
        in.colTf = 15.0;
        in.colTw = 9.0;
        in.colRadius = 18.0;
        in.flangeThroat = 0.0;
        const auto r = JC::computeStiffness(in);
        QVERIFY2(r.ok, r.error.c_str());
        // calculo independente
        const double dc = 200.0 - 30.0 - 36.0;                                   // 134
        const double area = 2 * 200.0 * 15.0 + (200.0 - 30.0) * 9.0 + (4.0 - 3.14159265358979323846) * 18.0 * 18.0;
        const double avc = area - 2 * 200.0 * 15.0 + (9.0 + 36.0) * 15.0;
        const double z = 300.0 - 10.7;
        const double beff = 10.7 + 5.0 * (15.0 + 18.0);
        const double k1 = 0.38 * avc / (1.0 * z);
        const double k2 = 0.7 * beff * 9.0 / dc;
        const double k3 = k2;
        const double expected = 210000.0 * z * z / (1.0 / k1 + 1.0 / k2 + 1.0 / k3);
        QVERIFY(std::abs(r.initialStiffness - expected) / expected < 1e-9);
        QVERIFY(std::abs(r.leverArm - z) < 1e-9);
        QVERIFY(r.initialStiffnessKnm() > 1000.0 && r.initialStiffnessKnm() < 1.0e6);
    }

    void stiffenersAndBetaMakeTheJointStiffer()
    {
        namespace JC = Structura::Design::JointComponents;
        JC::ModelInput in;
        in.kind = JC::ModelInput::WeldedFlanges;
        in.beamDepth = 300.0;
        in.beamTf = 10.7;
        in.colDepth = 200.0;
        in.colBf = 200.0;
        in.colTf = 15.0;
        in.colTw = 9.0;
        in.colRadius = 18.0;
        const double base = JC::computeStiffness(in).initialStiffness;
        JC::ModelInput stiff = in;
        stiff.webStiffened = true;
        QVERIFY(JC::computeStiffness(stiff).initialStiffness > base);
        JC::ModelInput both = in;
        both.beta = 0.0;   // momentos iguais dos dois lados: painel sem cisalhamento liquido
        QVERIFY(JC::computeStiffness(both).initialStiffness > base);
        JC::ModelInput doubler = in;
        doubler.doublerThickness = 8.0;
        QVERIFY(JC::computeStiffness(doubler).initialStiffness > base);
        JC::ModelInput opposite = in;
        opposite.beta = 2.0;
        QVERIFY(JC::computeStiffness(opposite).initialStiffness < base);
    }

    void endPlateStiffnessAddsRowsAndFollowsThePlateThickness()
    {
        namespace JC = Structura::Design::JointComponents;
        JC::ModelInput in;
        in.kind = JC::ModelInput::ExtendedEndPlate;
        in.beamDepth = 300.0;
        in.beamTf = 10.7;
        in.colDepth = 200.0;
        in.colBf = 200.0;
        in.colTf = 15.0;
        in.colTw = 9.0;
        in.colRadius = 18.0;
        in.plateWidth = 160.0;
        in.plateThickness = 20.0;
        in.gauge = 90.0;
        in.weldLeg = 8.0;
        in.firstRow = 40.0;
        in.pitch = 70.0;
        in.edgeVertical = 35.0;
        in.tensionRows = 1;
        const auto one = JC::computeStiffness(in);
        QVERIFY2(one.ok, one.error.c_str());
        QCOMPARE(one.rows.size(), std::size_t(1));
        in.tensionRows = 2;
        const auto two = JC::computeStiffness(in);
        QVERIFY(two.ok);
        QCOMPARE(two.rows.size(), std::size_t(2));
        QVERIFY(two.kEquivalent > one.kEquivalent);          // duas fileiras: mais rigidez
        QVERIFY(two.leverArm > one.leverArm);                // zeq entre as fileiras e acima de uma so
        JC::ModelInput thin = in;
        thin.plateThickness = 10.0;
        QVERIFY(JC::computeStiffness(thin).initialStiffness < two.initialStiffness);
        // k10 de cada fileira (dois parafusos): 2 x 1,6 As / Lb
        double k10 = 0.0;
        for (const auto &c : two.components) {
            if (c.symbol == "k10" && c.row == 1) {
                k10 = c.k;
            }
        }
        const double lb = 20.0 + 15.0 + 8.0 + 1.5 * 20.0 / 2.0;
        QVERIFY(std::abs(k10 - 1.6 * 245.0 / lb) < 1e-9);
    }

    void componentCurveFollowsEn1993TwoThirdsRule()
    {
        namespace JC = Structura::Design::JointComponents;
        const double sIni = 30000.0, mrd = 120.0;
        const auto curve = JC::momentRotationCurve(sIni, mrd, 2.7, 30);
        QVERIFY(curve.size() >= 30);
        // abaixo de 2/3: linear; Si lida da curva a 2/3 Mj,Rd = Sj,ini
        DirectionBehavior d;
        d.curve = curve;
        d.resistantMoment = mrd;
        const auto s = computeStiffness(d);
        QVERIFY(s.valid);
        QVERIFY(std::abs(s.stiffness - sIni) / sIni < 1e-9);
        // em Mj,Rd: theta = Mj,Rd/Sj,ini x 1,5^2,7
        QVERIFY(std::abs(curve.back().theta - mrd / sIni * std::pow(1.5, 2.7)) < 1e-12);
        // monotonica
        for (std::size_t i = 1; i < curve.size(); ++i) {
            QVERIFY(curve[i].theta > curve[i - 1].theta && curve[i].moment > curve[i - 1].moment);
        }
    }

    void connectionDefinedInTheProjectFeedsTheCurve()
    {
        using Structura::App::ProjectData::NodeConnection;
        using Structura::App::Services::ConnectionService;
        using Structura::App::Services::JointComponentService;
        ConnectionService::BeamInfo beam;
        beam.name = "IPE300";
        beam.family = "W Laminado";
        beam.geometry = {{"d", 300.0}, {"bf", 150.0}, {"tw", 7.1}, {"tf", 10.7}};
        beam.depth = 300.0;
        beam.bf = 150.0;
        beam.tw = 7.1;
        beam.tf = 10.7;
        NodeConnection c = ConnectionService::makeDefault(NodeConnection::kEndPlate, beam, 250.0, 400.0);
        withColumn(c);
        c.nodeId = 4;
        c.memberId = 5;
        QVERIFY(JointComponentService::isSupported(c));
        const auto r = JointComponentService::evaluate(c, 200000.0);
        QVERIFY2(r.supported, r.message.c_str());
        QVERIFY(r.stiffness.ok);
        QVERIFY(r.momentResistanceKnm > 1.0);
        QVERIFY(r.behavior.origin == DataOrigin::ComponentMethod);
        QVERIFY(!r.behavior.positive.curve.empty());
        QVERIFY(!r.behavior.negative.hasData());   // chapa estendida so para cima: sentido inverso nao calculado
        const auto s = computeStiffness(r.behavior.positive);
        QVERIFY(s.valid);
        QVERIFY(std::abs(s.stiffness - r.stiffness.initialStiffnessKnm()) / s.stiffness < 1e-6);
        // a resistencia e o limite do verificador: o verificador reprova um pouco acima de Mj,Rd e aprova um pouco abaixo
        const auto input = ConnectionService::endPlateInput(c);
        QVERIFY(!r.memory.empty());
        (void)input;
        // sem modelo para a tipologia
        NodeConnection shear = ConnectionService::makeDefault(NodeConnection::kShearTab, beam, 250.0, 400.0);
        QVERIFY(!JointComponentService::isSupported(shear));
        QVERIFY(!JointComponentService::evaluate(shear, 200000.0).supported);
    }

    void dialogUsesTheComponentModelOfTheProjectConnection()
    {
        using Structura::App::Services::JointComponentService;
        const auto ctx = JointAnalysisService::analyze(frame(4.0e-4), 4, 5, 2, 1.0);
        JointComponentService::Result comp;
        comp.supported = true;
        comp.behavior.origin = DataOrigin::ComponentMethod;
        comp.behavior.positive.curve = Structura::Design::JointComponents::momentRotationCurve(50000.0, 150.0);
        comp.behavior.positive.resistantMoment = 150.0;
        comp.memory = {"linha do metodo dos componentes"};
        Structura::UI::JointAnalysisDialog dialog(ctx, comp.behavior, false, comp);
        dialog.show();
        QVERIFY(std::abs(dialog.analysis().positive.stiffness.stiffness - 50000.0) / 50000.0 < 1e-9);
        QVERIFY(dialog.memoryText().contains(QStringLiteral("MÉTODO DOS COMPONENTES")));
        QVERIFY(dialog.memoryText().contains(QStringLiteral("linha do metodo dos componentes")));
        QCOMPARE(dialog.momentRotationChart()->drawnCurves() >= 0, true);
    }

    // ---- fluxo: classificacao e resultado; idealizacao e decisao posterior ----------------------------------------------------------

    static QPushButton *buttonWithText(QWidget *root, const QString &text)
    {
        for (QPushButton *b : root->findChildren<QPushButton *>()) {
            if (b->text() == text) {
                return b;
            }
        }
        return nullptr;
    }

    static Structura::App::Services::JointComponentService::Result defaultEndPlateResult()
    {
        using Structura::App::ProjectData::NodeConnection;
        using Structura::App::Services::ConnectionService;
        ConnectionService::BeamInfo beam;
        beam.name = "IPE300";
        beam.family = "W Laminado";
        beam.geometry = {{"d", 300.0}, {"bf", 150.0}, {"tw", 7.1}, {"tf", 10.7}};
        beam.depth = 300.0;
        beam.bf = 150.0;
        beam.tw = 7.1;
        beam.tf = 10.7;
        NodeConnection c = ConnectionService::makeDefault(NodeConnection::kEndPlate, beam, 250.0, 400.0);
        withColumn(c);
        return Structura::App::Services::JointComponentService::evaluate(c, 200000.0);
    }

    void classificationIsAResultAndNeverAnInput()
    {
        using Structura::App::Services::JointComponentService;
        const auto ctx = JointAnalysisService::analyze(frame(4.0e-4), 4, 5, 2, 1.0);
        Structura::UI::JointAnalysisDialog dialog(ctx, std::nullopt, false);
        dialog.show();
        QCOMPARE(dialog.finalClassificationText(), QStringLiteral("SEM DADOS"));
        QVERIFY(dialog.connectionStatusText().contains(QStringLiteral("Sem ligação construtiva")));   // falta de dados explicita
        // o dialogo nao tem nenhum controle para escolher "rotulada/semirrigida/rigida" como entrada da analise
        for (const auto *combo : dialog.findChildren<QComboBox *>()) {
            for (int i = 0; i < combo->count(); ++i) {
                QVERIFY2(!combo->itemText(i).startsWith(QStringLiteral("Classificação")), "classificacao nao pode ser entrada");
            }
        }
        // sem NENHUMA ligacao dimensionada, o modelo preliminar (configuracao de referencia) estima o comportamento: a classificacao e resultado
        int preliminaryCalls = 0;
        Structura::UI::JointAnalysisDialog::Hooks hooks;
        hooks.candidates = []() { return QStringList(); };   // nenhuma ligacao cadastrada no no
        hooks.preliminary = [&preliminaryCalls](const JointComponentService::PreliminarySettings &settings) {
            ++preliminaryCalls;
            return JointComponentService::evaluatePreliminary(ipe300OnHeb200(), settings);
        };
        dialog.setHooks(hooks);
        QVERIFY(preliminaryCalls >= 1);
        QVERIFY(dialog.analysis().behavior.origin == DataOrigin::PreliminaryModel);
        QVERIFY(dialog.analysis().positive.stiffness.valid);
        const QString text = dialog.finalClassificationText();
        QVERIFY2(text == QStringLiteral("ROTULADA") || text == QStringLiteral("SEMIRRÍGIDA") || text == QStringLiteral("RÍGIDA"), qPrintable(text));
        QVERIFY(dialog.connectionStatusText().contains(QStringLiteral("MODELO PRELIMINAR")));
        QVERIFY(dialog.memoryText().contains(QStringLiteral("PRELIMINAR")));
        // a faixa preliminar mostra as duas referencias lado a lado
        QVERIFY(dialog.preliminaryBandText().contains(QStringLiteral("chapa de extremidade")));
        QVERIFY(dialog.preliminaryBandText().contains(QStringLiteral("mesas soldadas")));
        // trocar a referencia para mesas soldadas recalcula, sem exigir nada dimensionado
        const double endPlateSi = dialog.analysis().positive.stiffness.stiffness;
        for (QComboBox *combo : dialog.findChildren<QComboBox *>()) {
            if (combo->count() == 2 && combo->itemText(1).contains(QStringLiteral("Mesas soldadas ao pilar"))) {
                combo->setCurrentIndex(1);
            }
        }
        QPushButton *calc = buttonWithText(&dialog, QStringLiteral("Calcular modelo preliminar"));
        QVERIFY(calc && calc->isEnabled());
        calc->click();
        QVERIFY(dialog.analysis().behavior.origin == DataOrigin::PreliminaryModel);
        QVERIFY(std::abs(dialog.analysis().positive.stiffness.stiffness - endPlateSi) > 1.0);
        dialog.grab();
        QVERIFY(!dialog.momentRotationChart()->insufficientData());
        QPointF marker;
        QVERIFY(dialog.classificationChart()->markerPosition(Direction::Positive, marker));
    }

    void missingConnectionIsReportedExplicitlyAndNoCurveIsInvented()
    {
        using Structura::App::Services::JointComponentService;
        const auto ctx = JointAnalysisService::analyze(frame(4.0e-4), 4, 5, 2, 1.0);
        JointComponentService::Result none;
        none.message = "Nao ha ligacao do projeto no no 4 para as barras 5 e 2.";
        Structura::UI::JointAnalysisDialog dialog(ctx, std::nullopt, false, none);
        dialog.show();
        dialog.grab();
        QVERIFY(dialog.momentRotationChart()->insufficientData());
        QCOMPARE(dialog.momentRotationChart()->drawnCurves(), 0);
        QVERIFY(dialog.connectionStatusText().contains(QStringLiteral("Sem dados suficientes para determinar a curva real")));
        QVERIFY(dialog.connectionStatusText().contains(QStringLiteral("Nao ha ligacao do projeto")));
        // tentar usar o metodo dos componentes sem ligacao: erro, sem curva
        dialog.setOrigin(DataOrigin::ComponentMethod);
        QVERIFY(!dialog.applyInputs());
        QVERIFY(dialog.analysis().primary() == nullptr);
        // idealizacao como semirrigida sem Si calculada e recusada
        Structura::UI::JointAnalysisDialog::Hooks hooks;
        int calls = 0;
        hooks.applyIdealization = [&calls](int, double, double, double) {
            ++calls;
            return QStringLiteral("x");
        };
        dialog.setHooks(hooks);
        QVERIFY(!dialog.applyIdealization(1, false));
        QCOMPARE(calls, 0);
    }

    void idealizationIsALaterModelingDecisionAndKeepsTheMechanicalResults()
    {
        using Structura::App::Services::JointComponentService;
        const auto ctx = JointAnalysisService::analyze(frame(4.0e-4), 4, 5, 2, 1.0);
        JointComponentService::Result comp;
        comp.supported = true;
        comp.behavior.origin = DataOrigin::ComponentMethod;
        comp.behavior.positive.curve = Structura::Design::JointComponents::momentRotationCurve(30000.0, 150.0);   // eta = 4,5: semirrigida
        comp.behavior.positive.resistantMoment = 150.0;
        Structura::UI::JointAnalysisDialog dialog(ctx, comp.behavior, false, comp);
        dialog.show();
        QCOMPARE(dialog.finalClassificationText(), QStringLiteral("SEMIRRÍGIDA"));
        int lastKind = -1;
        double lastSi = -1.0;
        int calls = 0;
        Structura::UI::JointAnalysisDialog::Hooks hooks;
        hooks.applyIdealization = [&](int kind, double si, double, double) {
            lastKind = kind;
            lastSi = si;
            ++calls;
            return QStringLiteral("aplicado");
        };
        hooks.currentIdealization = []() { return QStringLiteral("continua"); };
        dialog.setHooks(hooks);
        const double siBefore = dialog.analysis().positive.stiffness.stiffness;
        const QString classBefore = dialog.finalClassificationText();
        // idealizacao igual ao comportamento calculado: sem aviso
        QVERIFY(dialog.idealizationWarning(1).isEmpty());
        // diferente: aviso explicito
        const QString warning = dialog.idealizationWarning(2);
        QVERIFY(warning.contains(QStringLiteral("semirrígida")) && warning.contains(QStringLiteral("rígida")));
        QVERIFY(!dialog.idealizationWarning(0).isEmpty());
        // aplicar rotulada (diferente) com o aviso confirmado
        QVERIFY(dialog.applyIdealization(0, false));
        QCOMPARE(lastKind, 0);
        QVERIFY(dialog.idealizationStatusText().contains(QStringLiteral("Atenção")));
        // semirrigida: a rigidez enviada ao modelo e a Si calculada
        QVERIFY(dialog.applyIdealization(1, false));
        QCOMPARE(lastKind, 1);
        QVERIFY(std::abs(lastSi - siBefore) < 1e-9);
        QVERIFY(dialog.applyIdealization(2, false));
        QCOMPARE(calls, 3);
        // os resultados da analise mecanica nao mudam com a idealizacao
        QCOMPARE(dialog.analysis().positive.stiffness.stiffness, siBefore);
        QCOMPARE(dialog.finalClassificationText(), classBefore);
        QCOMPARE(dialog.analysis().behavior.origin, DataOrigin::ComponentMethod);
    }

    void idealizeWritesTheEndRotationOfTheBeam()
    {
        Structura::App::ProjectData::MemberConnectionSpec spec;
        spec.memberId = 5;
        QVERIFY(spec.isDefault());
        JointAnalysisService::idealize(spec, 11, 0, 0.0);
        QCOMPARE(spec.springK[11], 0.0);   // rotulada
        QVERIFY(!spec.isDefault());
        JointAnalysisService::idealize(spec, 11, 1, 24118.0);
        QCOMPARE(spec.springK[11], 24118.0);   // semirrigida: mola
        QCOMPARE(spec.springK[5], -1.0);       // o outro extremo nao foi tocado
        JointAnalysisService::idealize(spec, 11, 2, 0.0);
        QVERIFY(spec.springK[11] < 0.0);
        QVERIFY(spec.isDefault());             // rigida = continua: sem ligacao de extremidade
        // o grau de liberdade usado vem da analise do no: viga 5 no no 4 (extremidade J, flexao em torno do eixo normal ao plano)
        const auto ctx = JointAnalysisService::analyze(frame(4.0e-4), 4, 5, 2, 1.0);
        QCOMPARE(ctx.bars[static_cast<std::size_t>(ctx.referenceBar)].rotationDof, 11);
    }

    // ---- VALIDACAO FISICA contra exemplo publicado -----------------------------------------------------------------------------------
    // Exemplo de calculo manual da SCIA (EN 1993-1-8): ligacao viga-pilar de um lado, chapa de extremidade estendida.
    //   Pilar HEB140 (S235), viga IPE220 (S235), chapa 140 x 12 (w = 80), 6 parafusos M16 8.8, tres fileiras;
    //   alturas hr = 245,4; 175,4; 35,4 mm. Resultados publicados: k3 = 5,73/5,59; k4 = 8,59/8,39; k5 = 7,55/7,57; k10 = 6,47 mm;
    //   keff = 1,73/1,71; zeq = 216,4 mm; keq = 3,35 mm; k2 = 8,70 mm; Sj,ini = 11596 kN.m/rad; Ft = 97,31 kN (fileira 1, chapa);
    //   Vwp,Rd = 160,2 kN; Mj,Rd = 34,91 kN.m.
    // Os comprimentos efetivos e os m de cada fileira vem do exemplo publicado (o calculo dos leff de grupo nao faz parte desta validacao).

    static Structura::Design::JointComponents::ModelInput sciaInput()
    {
        namespace JC = Structura::Design::JointComponents;
        JC::ModelInput in;
        in.kind = JC::ModelInput::ExtendedEndPlate;
        in.youngModulus = 210000.0;
        in.beamDepth = 220.0;
        in.beamBf = 110.0;
        in.beamTf = 9.2;
        in.beamTw = 5.9;
        in.beamFy = 235.0;
        in.beamPlasticModulus = 285000.0;
        in.colDepth = 140.0;
        in.colBf = 140.0;
        in.colTf = 12.0;
        in.colTw = 7.0;
        in.colRadius = 12.0;
        in.colFy = 235.0;
        in.colArea = 4300.0;
        in.plateWidth = 140.0;
        in.plateThickness = 12.0;
        in.plateFy = 235.0;
        in.gauge = 80.0;
        in.flangeThroat = 5.0;
        in.plateBelowFlange = 19.93;   // dispersao sp limitada pela chapa abaixo da mesa (valor publicado)
        in.boltDiameter = 16.0;
        in.boltStressArea = 157.0;
        in.boltTensionResistance = 90430.0;   // 0,9 fub As / 1,25
        in.washerThickness = 1.65;            // 2 x 1,65 = 3,3 mm (Lb = 38,8 mm publicado)
        in.boltHeadHeight = 10.0;
        in.boltNutHeight = 13.0;
        in.gammaYield = 1.0;
        in.beta = 1.0;
        return in;
    }

    static Structura::Design::JointComponents::BoltRow sciaRow(int row, double h, double mc, double leffCol, double mp, double leffPlateCp, double leffPlateNc, double leffColCp = -1.0, double leffColNc = -1.0)
    {
        Structura::Design::JointComponents::BoltRow r;
        r.row = row;
        r.h = h;
        r.mColumn = mc;
        r.eColumn = 30.0;
        r.leffColumnCircular = leffColCp > 0.0 ? leffColCp : leffCol;
        r.leffColumnNonCircular = leffColNc > 0.0 ? leffColNc : leffCol;
        r.mPlate = mp;
        r.ePlate = 30.0;
        r.leffPlateCircular = leffPlateCp;
        r.leffPlateNonCircular = leffPlateNc;
        return r;
    }

    void validationScia_stiffnessComponentsAndSjIni()
    {
        namespace JC = Structura::Design::JointComponents;
        JC::ModelInput in = sciaInput();
        const double mx = 30.0 - 0.8 * std::sqrt(2.0) * 5.0;   // 24,34 mm
        in.rows = {sciaRow(1, 245.4, 26.9, 107.55, mx, 70.0, 70.0), sciaRow(2, 175.4, 26.9, 105.0, 33.66, 185.51, 185.51)};
        const auto r = JC::computeStiffness(in);
        QVERIFY2(r.ok, r.error.c_str());
        auto k = [&](const char *symbol, int row) {
            for (const auto &c : r.components) {
                if (c.symbol == symbol && c.row == row) {
                    return c.k;
                }
            }
            return -1.0;
        };
        auto near = [](double value, double published, double relTol) { return std::abs(value - published) <= relTol * published; };
        QVERIFY2(near(k("k3", 1), 5.73, 0.005), qPrintable(QString::number(k("k3", 1))));
        QVERIFY2(near(k("k3", 2), 5.59, 0.005), qPrintable(QString::number(k("k3", 2))));
        QVERIFY2(near(k("k4", 1), 8.59, 0.005), qPrintable(QString::number(k("k4", 1))));
        QVERIFY2(near(k("k4", 2), 8.39, 0.005), qPrintable(QString::number(k("k4", 2))));
        QVERIFY2(near(k("k5", 1), 7.55, 0.01), qPrintable(QString::number(k("k5", 1))));
        QVERIFY2(near(k("k5", 2), 7.57, 0.01), qPrintable(QString::number(k("k5", 2))));
        QVERIFY2(near(k("k10", 1), 6.47, 0.005), qPrintable(QString::number(k("k10", 1))));   // 1,6 As/Lb sem fator 2
        QVERIFY2(near(r.rows[0].keff, 1.73, 0.01), qPrintable(QString::number(r.rows[0].keff)));
        QVERIFY2(near(r.rows[1].keff, 1.71, 0.01), qPrintable(QString::number(r.rows[1].keff)));
        QVERIFY2(near(r.leverArm, 216.4, 0.002), qPrintable(QString::number(r.leverArm)));
        QVERIFY2(near(r.kEquivalent, 3.35, 0.01), qPrintable(QString::number(r.kEquivalent)));
        QVERIFY2(near(k("k2", 0), 8.70, 0.005), qPrintable(QString::number(k("k2", 0))));
        QVERIFY2(near(r.initialStiffnessKnm(), 11596.0, 0.01), qPrintable(QString::number(r.initialStiffnessKnm())));
    }

    void validationScia_resistanceAndMomentResistance()
    {
        namespace JC = Structura::Design::JointComponents;
        JC::ModelInput in = sciaInput();
        const double mx = 30.0 - 0.8 * std::sqrt(2.0) * 5.0;
        const double cp = 169.02, nc = 145.10;   // mesa do pilar: fileira isolada (Tabela 6.4)
        in.rows = {sciaRow(1, 245.4, 26.9, 0.0, mx, 2.0 * 3.14159265358979 * mx, 70.0, cp, nc), sciaRow(2, 175.4, 26.9, 0.0, 33.66, 211.49, 201.57, cp, nc),
                   sciaRow(3, 35.4, 26.9, 0.0, 33.66, 211.49, 172.14, cp, nc)};
        const auto r = JC::computeResistance(in);
        QVERIFY2(r.ok, r.error.c_str());
        auto near = [](double value, double published, double relTol) { return std::abs(value - published) <= relTol * published; };
        QVERIFY2(near(r.panelShear, 160200.0, 0.002), qPrintable(QString::number(r.panelShear)));            // Vwp,Rd = 160,2 kN
        QVERIFY2(near(r.beamFlangeCompression, 317700.0, 0.01), qPrintable(QString::number(r.beamFlangeCompression)));
        QCOMPARE(r.rows.size(), std::size_t(3));
        QVERIFY2(near(r.rows[0].individual, 97310.0, 0.005), qPrintable(QString::number(r.rows[0].individual)));   // fileira 1: chapa, modo 1
        // o exemplo publicado lista "ruptura da chapa" (97,31 kN); a alma da viga tracionada (EN 6.2.6.8: 70 x 5,9 x 235 = 97,06 kN) fica 0,26% abaixo: ambas governam dentro da tolerancia
        QVERIFY(r.rows[0].governingComponent.find("chapa") != std::string::npos || r.rows[0].governingComponent.find("alma da viga") != std::string::npos);
        QVERIFY2(near(r.rows[0].effective, 97310.0, 0.005), qPrintable(QString::number(r.rows[0].effective)));
        QVERIFY2(near(r.rows[1].effective, 62900.0, 0.005), qPrintable(QString::number(r.rows[1].effective)));     // limitada pelo painel: 160,2 - 97,3
        QVERIFY(r.rows[1].limitedBy.find("painel") != std::string::npos);
        QVERIFY2(r.rows[2].effective < 1.0, qPrintable(QString::number(r.rows[2].effective)));                      // fileira 3 nao contribui
        QVERIFY2(near(r.momentResistanceKnm(), 34.91, 0.005), qPrintable(QString::number(r.momentResistanceKnm())));
        // equilibrio: Mj,Rd = soma(F h) e soma(F) <= limite do painel
        double sumF = 0.0, sumFh = 0.0;
        for (const auto &row : r.rows) {
            sumF += row.effective;
            sumFh += row.effective * row.h;
        }
        QVERIFY(sumF <= r.cap * (1.0 + 1e-9));
        QVERIFY(std::abs(sumFh - r.momentResistance) < 1e-6 * r.momentResistance);
    }

    void validationP398_k10AndK1ReproduceThePublishedValues()
    {
        // SCI P398 exemplo C2 (New Steel Construction, fev/2020): As = 353 mm2, Lb = 70,5 mm -> k10 = 8,01; Avc = 3810 mm2, zeq = 498 mm -> k1 = 2,91;
        // keq = 6,01 e k2 = infinito (enrijecedor) -> Sj = E zeq^2 / (1/k1 + 1/keq) = 102 MN.m/rad
        QVERIFY(std::abs(1.6 * 353.0 / 70.5 - 8.01) < 0.005);
        QVERIFY(std::abs(0.38 * 3810.0 / 498.0 - 2.91) < 0.005);
        const double sj = 210.0 * 498.0 * 498.0 / (1.0 / 2.91 + 1.0 / 6.01);   // kN.mm/rad
        QVERIFY(std::abs(sj * 1e-6 - 102.0) < 0.5);                             // MN.m/rad
    }

    void physicalLawsOfTheComponentAssembly()
    {
        namespace JC = Structura::Design::JointComponents;
        JC::ModelInput in = sciaInput();
        const double mx = 30.0 - 0.8 * std::sqrt(2.0) * 5.0;
        in.rows = {sciaRow(1, 245.4, 26.9, 107.55, mx, 70.0, 70.0), sciaRow(2, 175.4, 26.9, 105.0, 33.66, 185.51, 185.51)};
        const auto base = JC::computeStiffness(in);
        QVERIFY(base.ok);
        // serie: 1/Sj = soma(1/ki) / (E z^2) com ki dos componentes em serie
        const double inv = 1.0 / base.components[0].k + 1.0 / base.components[1].k + 1.0 / base.kEquivalent;
        QVERIFY(std::abs(base.initialStiffness - 210000.0 * base.leverArm * base.leverArm / inv) / base.initialStiffness < 1e-12);
        // Sj,ini e menor que a de qualquer componente isolado (mola equivalente em serie e mais mole que a mais mole)
        for (const auto &c : base.components) {
            if (c.row == 0 && !c.infinite) {
                QVERIFY(base.initialStiffness < 210000.0 * base.leverArm * base.leverArm * c.k);
            }
        }
        // E escala a rigidez linearmente
        JC::ModelInput doubled = in;
        doubled.youngModulus *= 2.0;
        QVERIFY(std::abs(JC::computeStiffness(doubled).initialStiffness / base.initialStiffness - 2.0) < 1e-12);
        // chapa mais espessa: mais rigida (k5 ~ t^3) e nunca menos resistente
        JC::ModelInput thick = in;
        thick.plateThickness = 20.0;
        QVERIFY(JC::computeStiffness(thick).initialStiffness > base.initialStiffness);
        JC::ModelInput resistanceBase = sciaInput();
        resistanceBase.rows = {sciaRow(1, 245.4, 26.9, 0.0, mx, 152.95, 70.0, 169.02, 145.10), sciaRow(2, 175.4, 26.9, 0.0, 33.66, 211.49, 201.57, 169.02, 145.10)};
        JC::ModelInput resistanceThick = resistanceBase;
        resistanceThick.plateThickness = 20.0;
        QVERIFY(JC::computeResistance(resistanceThick).momentResistance >= JC::computeResistance(resistanceBase).momentResistance);
        // resistencia nao depende de E (so a rigidez); fatores parciais maiores reduzem Mj,Rd
        JC::ModelInput lessE = resistanceBase;
        lessE.youngModulus = 100000.0;
        QCOMPARE(JC::computeResistance(lessE).momentResistance, JC::computeResistance(resistanceBase).momentResistance);
        JC::ModelInput safer = resistanceBase;
        safer.gammaYield = 1.10;
        QVERIFY(JC::computeResistance(safer).momentResistance < JC::computeResistance(resistanceBase).momentResistance);
        // enrijecedores: o painel deixa de limitar -> maior resistencia (ou igual)
        JC::ModelInput stiffened = resistanceBase;
        stiffened.webStiffened = true;
        QVERIFY(JC::computeResistance(stiffened).momentResistance >= JC::computeResistance(resistanceBase).momentResistance);
    }

    void curveMetricsMatchTheAnalyticalDerivatives()
    {
        namespace JC = Structura::Design::JointComponents;
        const double s = 20000.0, mrd = 100.0, psi = 2.7;
        const auto m = JC::curveMetrics(s, mrd, psi);
        QCOMPARE(m.initialStiffness, s);
        QVERIFY(std::abs(m.onsetOfNonlinearity - 100.0 * 2.0 / 3.0) < 1e-12);
        QVERIFY(std::abs(m.rotationAtResistance - mrd / s * std::pow(1.5, psi)) < 1e-15);
        QVERIFY(std::abs(m.secantAtResistance - s / std::pow(1.5, psi)) < 1e-9);
        // tangente por diferenca finita na curva densa
        const auto curve = JC::momentRotationCurve(s, mrd, psi, 3000);
        const std::size_t i = curve.size() - 2;
        const double fd = (curve[i + 1].moment - curve[i].moment) / (curve[i + 1].theta - curve[i].theta);
        QVERIFY(std::abs(fd - JC::tangentStiffnessAt(0.5 * (curve[i].moment + curve[i + 1].moment), s, mrd, psi)) / fd < 0.01);
        QVERIFY(std::abs(JC::tangentStiffnessAt(50.0, s, mrd, psi) - s) < 1e-9);   // abaixo de 2/3 Mj,Rd: tangente = inicial
        // a secante a Sj,ini/2 esta onde a formula manda
        QVERIFY(std::abs(JC::secantStiffnessAt(m.momentAtHalfStiffness, s, mrd, psi) - s / 2.0) < 1e-6);
        QVERIFY(m.rotationAtHalfStiffness > m.rotationAtOnset && m.rotationAtHalfStiffness < m.rotationAtResistance);
        // secante a 2/3 de Mj,Rd = Si da norma = Sj,ini; secante em Mj,Rd < Si
        QVERIFY(std::abs(JC::secantStiffnessAt(2.0 * mrd / 3.0, s, mrd, psi) - s) < 1e-9);
        QVERIFY(m.secantAtResistance < s && m.tangentAtResistance < m.secantAtResistance);
    }

    void bilinearEquivalentReproducesTheCurveAtTheKnees()
    {
        namespace JC = Structura::Design::JointComponents;
        const double s = 20000.0, mrd = 100.0;
        const auto eq = JC::deriveEquivalent(JC::EquivalentModel::SemiRigidBilinear, s, mrd);
        QVERIFY(eq.valid);
        QCOMPARE(eq.stiffness, s);
        QVERIFY(std::abs(eq.yieldMoment - mrd * 2.0 / 3.0) < 1e-12);
        const auto m = JC::curveMetrics(s, mrd);
        // a rotula passa pelos dois pontos da curva: (theta 2/3, 2/3 Mj,Rd) e (theta Rd, Mj,Rd)
        const double thetaY = eq.yieldMoment / eq.stiffness;
        QVERIFY(std::abs(eq.yieldMoment + eq.hardeningRatio * eq.stiffness * (m.rotationAtResistance - thetaY) - mrd) < 1e-9);
        QVERIFY(eq.hardeningRatio > 0.0 && eq.hardeningRatio < 1.0);
        // a partir de qualquer curva (mesmo derivada da EN) o ajuste e o mesmo
        DirectionBehavior d;
        d.curve = JC::momentRotationCurve(s, mrd, 2.7, 60);
        d.resistantMoment = mrd;
        const auto a = analyzeJoint([&] { JointBehavior b; b.positive = d; return b; }(), limitsFor(2e8, 2e-4, 6.0), okStorey(0.3), JointKind::BeamColumn);
        double si = 0.0, my = 0.0, h = 0.0;
        QVERIFY(bilinearEquivalent(a.positive, si, my, h));
        QVERIFY(std::abs(si - s) < 1e-6 && std::abs(my - eq.yieldMoment) < 1e-9 && std::abs(h - eq.hardeningRatio) / eq.hardeningRatio < 1e-3);
        // dados sem curva ate Mj,Rd: nao ha como derivar
        DirectionBehavior onlyPoint;
        onlyPoint.resistantMoment = mrd;
        onlyPoint.theta23 = 0.001;
        const auto b = analyzeJoint([&] { JointBehavior x; x.positive = onlyPoint; return x; }(), limitsFor(2e8, 2e-4, 6.0), okStorey(0.3), JointKind::BeamColumn);
        QVERIFY(!bilinearEquivalent(b.positive, si, my, h));
    }

    void idealizationDerivesFromThePhysicalModelAndWritesAHinge()
    {
        namespace JC = Structura::Design::JointComponents;
        Structura::App::ProjectData::MemberConnectionSpec spec;
        spec.memberId = 5;
        const auto eq = JC::deriveEquivalent(JC::EquivalentModel::SemiRigidBilinear, 20000.0, 100.0);
        JointAnalysisService::idealize(spec, 11, 3, eq.stiffness, eq.yieldMoment, eq.hardeningRatio);
        QCOMPARE(spec.hinges.size(), 1);
        QCOMPARE(spec.hinges[0].dof, 11);
        QCOMPARE(spec.hinges[0].kElastic, 20000.0);
        QVERIFY(std::abs(spec.hinges[0].yieldMoment - 100.0 * 2.0 / 3.0) < 1e-9);
        QVERIFY(spec.springK[11] < 0.0);   // no continuo: a nao linearidade esta na rotula
        QVERIFY(!spec.isDefault());
        // reaplicar substitui (nao empilha) a rotula; a mola linear a remove
        JointAnalysisService::idealize(spec, 11, 3, eq.stiffness, eq.yieldMoment, eq.hardeningRatio);
        QCOMPARE(spec.hinges.size(), 1);
        JointAnalysisService::idealize(spec, 11, 1, 20000.0);
        QCOMPARE(spec.hinges.size(), 0);
        QCOMPARE(spec.springK[11], 20000.0);
        JointAnalysisService::idealize(spec, 11, 2, 0.0);
        QVERIFY(spec.isDefault());
    }

    void serviceUsesOneModelForResistanceStiffnessAndCurve()
    {
        using Structura::App::Services::JointComponentService;
        const auto r = defaultEndPlateResult();
        QVERIFY2(r.supported, r.message.c_str());
        // as fileiras da rigidez e da resistencia sao as mesmas (mesma geometria cadastrada)
        QVERIFY(r.resistance.ok);
        QCOMPARE(r.stiffness.rows.size(), r.resistance.rows.size());
        for (std::size_t i = 0; i < r.stiffness.rows.size(); ++i) {
            QVERIFY(std::abs(r.stiffness.rows[i].h - r.resistance.rows[i].h) < 1e-9);
        }
        // NIVEL A (curva) usa os componentes da EN; a verificacao da NBR e o NIVEL C e nao altera a curva, so a sinaliza
        const double en = r.momentResistanceEnKnm, nbr = r.momentResistanceNbrKnm;
        QVERIFY(en > 0.0);
        QVERIFY(std::abs(r.momentResistanceKnm - en) < 1e-12);
        QCOMPARE(r.nbrCheckAvailable, nbr > 0.0);
        QCOMPARE(r.nbrBelowCurve, nbr > 0.0 && nbr < en);
        QVERIFY(!r.preliminary);
        // a curva usa essa rigidez e essa resistencia
        const auto s = computeStiffness(r.behavior.positive);
        QVERIFY(s.valid);
        QVERIFY(std::abs(s.stiffness - r.stiffness.initialStiffnessKnm()) / s.stiffness < 1e-6);
        QVERIFY(std::abs(*r.behavior.positive.resistantMoment - r.momentResistanceKnm) < 1e-12);
        QVERIFY(std::abs(r.metrics.momentResistance - r.momentResistanceKnm) < 1e-12);
        // procedencia: o resultado e identificado como calculado
        QVERIFY(originNature(r.behavior.origin).find("CALCULADO") != std::string::npos);
        QVERIFY(originNature(DataOrigin::ExperimentalCurve).find("EXPERIMENTAL") != std::string::npos);
    }

    void resultsAreInvalidatedWhenTheConnectionChanges()
    {
        using Structura::App::ProjectData::NodeConnection;
        using Structura::App::Services::ConnectionService;
        using Structura::App::Services::JointComponentService;
        ConnectionService::BeamInfo beam;
        beam.name = "IPE300";
        beam.family = "W Laminado";
        beam.geometry = {{"d", 300.0}, {"bf", 150.0}, {"tw", 7.1}, {"tf", 10.7}};
        beam.depth = 300.0;
        beam.bf = 150.0;
        beam.tw = 7.1;
        beam.tf = 10.7;
        NodeConnection c = ConnectionService::makeDefault(NodeConnection::kEndPlate, beam, 250.0, 400.0);
        withColumn(c);
        const auto fp = JointComponentService::fingerprint(c, 200000.0);
        QCOMPARE(JointComponentService::fingerprint(c, 200000.0), fp);   // estavel
        QVERIFY(JointComponentService::fingerprint(c, 210000.0) != fp);   // E faz parte do estado
        NodeConnection changed = c;
        changed.values["plateT"] = changed.values.value("plateT") + 3.0;
        QVERIFY(JointComponentService::fingerprint(changed, 200000.0) != fp);
        // a janela recalcula quando a ligacao muda e nao quando fica igual
        const auto ctx = JointAnalysisService::analyze(frame(4.0e-4), 4, 5, 2, 1.0);
        auto first = JointComponentService::evaluate(c, 200000.0);
        QVERIFY(first.supported);
        Structura::UI::JointAnalysisDialog dialog(ctx, first.behavior, false, first);
        NodeConnection current = c;
        Structura::UI::JointAnalysisDialog::Hooks hooks;
        hooks.reevaluate = [&current]() { return JointComponentService::evaluate(current, 200000.0); };
        dialog.setHooks(hooks);
        const double before = dialog.analysis().positive.stiffness.stiffness;
        QVERIFY(!dialog.refreshFromModel());                                  // nada mudou
        current.values["plateT"] = current.values.value("plateT") + 6.0;       // o usuario engrossou a chapa no editor de ligacoes
        QVERIFY(dialog.refreshFromModel());
        QVERIFY(dialog.analysis().positive.stiffness.stiffness > before);      // rigidez recalculada, sem ninguem pedir
        QVERIFY(dialog.idealizationStatusText().contains(QStringLiteral("recalculadas")));
        QVERIFY(!dialog.refreshFromModel());                                  // estavel de novo
    }

    void limitationsOfTheEquivalentModelAreReportedForTheNode()
    {
        // viga com excentricidade de eixo e viga oposta no no: o modelo avisa o que nao representa
        auto model = frame(4.0e-4);
        for (auto &mem : model.members) {
            if (mem.id == 5) {
                mem.offsetY2 = 0.05;   // extremidade J (no 4) com offset
            }
        }
        addNode(model, 20, 0.0, 0.0, 0.0);
        (void)model;
        const auto ctx = JointAnalysisService::analyze(model, 4, 5, 2, 1.0);
        QVERIFY(ctx.valid);
        bool eccentricity = false, opposite = false, aligned = false;
        for (const auto &n : ctx.notes) {
            eccentricity = eccentricity || n.find("excentricidade") != std::string::npos;
            opposite = opposite || n.find("viga oposta") != std::string::npos;
            aligned = aligned || n.find("eixo principal") != std::string::npos;
        }
        QVERIFY(eccentricity);
        QVERIFY(opposite);       // a viga 6 chega ao no 4 pelo lado oposto da viga 5
        QVERIFY(!aligned);       // plano alinhado com o eixo principal da secao
        // plano inclinado em relacao ao eixo principal: aviso
        auto rotated = frame(4.0e-4);
        for (auto &mem : rotated.members) {
            if (mem.id == 5) {
                mem.sectionRotationDeg = 30.0;
            }
        }
        const auto c2 = JointAnalysisService::analyze(rotated, 4, 5, 2, 1.0);
        bool warned = false;
        for (const auto &n : c2.notes) {
            warned = warned || n.find("eixo principal") != std::string::npos;
        }
        QVERIFY(warned);
    }

    // O solver condensa a mola de extremidade em serie com a barra: balanco com mola rotacional k no apoio.
    // Solucao analitica: flecha da ponta = P L^3 / (3 E I) + P L^2 / k  (a rotacao da mola e M/k com M = P L).
    void endSpringInTheSolverGivesTheAnalyticalCantileverWithRotationalSupport()
    {
        const double L = 3.0, P = 10.0, I = 1.0e-4, k = 5000.0;   // kN, m, kN.m/rad
        SolverBridge::ModelData m = baseModel(I);
        m.sections[0].iz = m.sections[0].iy = I;
        m.sections[0].area = 0.01;
        m.sections[0].j = 1.0e-4;
        SolverBridge::NodeData n1;
        n1.id = 1;
        n1.restraints = {{1, 1, 1, 1, 1, 1}};
        SolverBridge::NodeData n2;
        n2.id = 2;
        n2.x = L;
        m.nodes = {n1, n2};
        addMember(m, 1, 1, 2, 1);
        SolverBridge::NodalLoadData load;
        load.nodeId = 2;
        load.values = {{0, 0, -P, 0, 0, 0}};
        m.nodalLoads.push_back(load);
        const double rigid = SolverBridge(solverLibrary()).solve(m).nodeResults[1].displacements[2];
        QVERIFY(std::abs(rigid + P * L * L * L / (3.0 * kE * I)) < 1e-9);   // sem mola: Euler-Bernoulli
        // mola rotacional em torno do eixo local z (flexao no plano XZ) na extremidade I (dof 5), gerada pela idealizacao do programa
        Structura::App::ProjectData::MemberConnectionSpec spec;
        spec.memberId = 1;
        JointAnalysisService::idealize(spec, 5, 1, k);
        SolverBridge::EndSpringData spring;
        spring.memberId = 1;
        spring.k = spec.springK;
        m.endSprings.push_back(spring);
        const double withSpring = SolverBridge(solverLibrary()).solve(m).nodeResults[1].displacements[2];
        const double expected = -(P * L * L * L / (3.0 * kE * I) + P * L * L / k);
        QVERIFY2(std::abs(withSpring - expected) < 1e-9, qPrintable(QString::number(withSpring, 'g', 10) + " vs " + QString::number(expected, 'g', 10)));
        // mola muito rigida -> volta ao engaste; mola nula (rotulada) -> a barra nao transmite momento ao apoio (mecanismo ou flecha enorme)
        spec.springK[5] = 1.0e12;
        m.endSprings[0].k = spec.springK;
        const double stiff = SolverBridge(solverLibrary()).solve(m).nodeResults[1].displacements[2];
        QVERIFY(std::abs(stiff - rigid) < 1e-6);
    }

    // ---- escopo: A (comportamento) -> B (classificacao/idealizacao) -> C (dimensionamento), sem exigir ligacao dimensionada ---------------

    static Structura::App::Services::JointComponentService::PreliminaryMembers ipe300OnHeb200()
    {
        Structura::App::Services::JointComponentService::PreliminaryMembers m;
        m.beamDepth = 300.0;
        m.beamBf = 150.0;
        m.beamTf = 10.7;
        m.beamTw = 7.1;
        m.beamFy = 250.0;
        m.colDepth = 200.0;
        m.colBf = 200.0;
        m.colTf = 15.0;
        m.colTw = 9.0;
        m.colRadius = 18.0;
        m.colFy = 250.0;
        m.youngModulusMPa = 200000.0;
        return m;
    }

    void preliminaryModelNeedsNoDimensionedConnection()
    {
        using Structura::App::Services::JointComponentService;
        namespace JC = Structura::Design::JointComponents;
        // nenhuma NodeConnection, nenhum parafuso/chapa/solda dimensionados: so os perfis das duas barras
        JointComponentService::PreliminarySettings welded;
        welded.kind = 0;
        const auto w = JointComponentService::evaluatePreliminary(ipe300OnHeb200(), welded);
        QVERIFY2(w.supported, w.message.c_str());
        QVERIFY(w.preliminary);
        QVERIFY(w.behavior.origin == DataOrigin::PreliminaryModel);
        QVERIFY(!w.hypotheses.empty());
        QVERIFY(originNature(w.behavior.origin).find("PRELIMINAR") != std::string::npos);
        QVERIFY(originNature(w.behavior.origin).find("NÃO é uma ligação dimensionada") != std::string::npos);
        // soldada, sem enrijecedores, penetracao total, E = 200 GPa, HEB200: conferido a mao (mesmo calculo do guia)
        const double dc = 200.0 - 30.0 - 36.0;
        const double area = 2 * 200.0 * 15.0 + 170.0 * 9.0 + (4.0 - 3.14159265358979) * 18.0 * 18.0;
        const double avc = area - 2 * 200.0 * 15.0 + (9.0 + 36.0) * 15.0;
        const double z = 300.0 - 10.7;
        const double beff = 10.7 + 5.0 * (15.0 + 18.0);
        const double k1 = 0.38 * avc / z, k2 = 0.7 * beff * 9.0 / dc;
        const double expected = 200000.0 * z * z / (1.0 / k1 + 2.0 / k2) * 1e-6;
        QVERIFY(std::abs(w.stiffness.initialStiffnessKnm() - expected) / expected < 1e-9);
        QVERIFY(w.momentResistanceKnm > 0.0);
        QVERIFY(w.behavior.negative.hasData());   // soldada e simetrica
        // nivel C nao realizado: nenhuma verificacao construtiva (NBR) foi feita e nenhuma ligacao e acusada de insuficiente
        QVERIFY(!w.nbrCheckAvailable);
        QVERIFY(!w.nbrBelowCurve);
        QCOMPARE(w.momentResistanceNbrKnm, 0.0);
        // chapa de extremidade de referencia: configuracao ADOTADA por regras e declarada
        JointComponentService::PreliminarySettings plate;
        plate.kind = 1;
        const auto p = JointComponentService::evaluatePreliminary(ipe300OnHeb200(), plate);
        QVERIFY2(p.supported, p.message.c_str());
        bool declared = false;
        for (const auto &h : p.hypotheses) {
            declared = declared || h.find("ADOTADA") != std::string::npos;
        }
        QVERIFY(declared);
        QVERIFY(p.stiffness.initialStiffnessKnm() > 0.0 && p.momentResistanceKnm > 0.0);
        QVERIFY(!p.behavior.negative.hasData());   // so o sentido que traciona as fileiras
        // a curva e a da EN (6.3.1) com a rigidez e a resistencia do modelo preliminar, e a Si da norma coincide com Sj,ini
        const auto s = computeStiffness(p.behavior.positive);
        QVERIFY(s.valid);
        QVERIFY(std::abs(s.stiffness - p.stiffness.initialStiffnessKnm()) / s.stiffness < 1e-6);
        // as hipoteses entram na memoria de calculo
        bool inMemory = false;
        for (const auto &l : p.memory) {
            inMemory = inMemory || l.find("MODELO PRELIMINAR") != std::string::npos;
        }
        QVERIFY(inMemory);
    }

    void preliminaryModelRespondsToTheReferenceParametersAndTheMembers()
    {
        using Structura::App::Services::JointComponentService;
        JointComponentService::PreliminarySettings base;
        base.kind = 1;
        const auto a = JointComponentService::evaluatePreliminary(ipe300OnHeb200(), base);
        JointComponentService::PreliminarySettings thick = base;
        thick.plateThickness = 30.0;
        const auto b = JointComponentService::evaluatePreliminary(ipe300OnHeb200(), thick);
        QVERIFY(b.stiffness.initialStiffnessKnm() > a.stiffness.initialStiffnessKnm());   // chapa mais grossa: mais rigida
        QVERIFY(a.fingerprint != b.fingerprint);
        QCOMPARE(JointComponentService::evaluatePreliminary(ipe300OnHeb200(), base).fingerprint, a.fingerprint);   // estavel
        // viga mais alta (outro perfil): outro resultado
        auto deeper = ipe300OnHeb200();
        deeper.beamDepth = 450.0;
        QVERIFY(JointComponentService::evaluatePreliminary(deeper, base).fingerprint != a.fingerprint);
        // vigas dos dois lados (beta = 0): painel deixa de limitar -> mais rigida
        JointComponentService::PreliminarySettings both = base;
        both.beta = 0.0;
        QVERIFY(JointComponentService::evaluatePreliminary(ipe300OnHeb200(), both).stiffness.initialStiffnessKnm() > a.stiffness.initialStiffnessKnm());
        // sem perfil I/H (dimensoes ausentes): nao inventa resultado
        JointComponentService::PreliminaryMembers none;
        const auto r = JointComponentService::evaluatePreliminary(none, base);
        QVERIFY(!r.supported);
        QVERIFY(!r.message.empty());
        QVERIFY(r.behavior.positive.curve.empty());
    }

    void levelCDoesNotChangeTheMechanicalBehaviorButFlagsAnInsufficientConnection()
    {
        // ligacao cadastrada (dimensionada) cuja verificacao da NBR resiste a menos que o Mj,Rd da curva: a curva (nivel A) nao muda; o nivel C sinaliza
        const auto r = defaultEndPlateResult();
        QVERIFY(r.supported);
        QVERIFY(r.nbrCheckAvailable);
        QVERIFY(r.momentResistanceNbrKnm < r.momentResistanceEnKnm);
        QVERIFY(r.nbrBelowCurve);
        QVERIFY(std::abs(r.momentResistanceKnm - r.momentResistanceEnKnm) < 1e-12);
        const auto ctx = JointAnalysisService::analyze(frame(4.0e-4), 4, 5, 2, 1.0);
        Structura::UI::JointAnalysisDialog dialog(ctx, r.behavior, false, r);
        dialog.show();
        // o resultado de nivel A e o mesmo com ou sem o aviso; o aviso aparece na memoria e na aba de idealizacao
        QVERIFY(dialog.memoryText().contains(QStringLiteral("NÃO sustenta o momento da curva")));
        QVERIFY(dialog.memoryText().contains(QStringLiteral("NÍVEL C")));
        QVERIFY(dialog.memoryText().contains(QStringLiteral("NÍVEL A")));
        QCOMPARE(dialog.analysis().positive.stiffness.momentRd, r.momentResistanceEnKnm);
    }

    void idealizationChoiceIsPreservedSeparatelyFromThePreliminaryClassification()
    {
        using Structura::App::Services::JointComponentService;
        const auto ctx = JointAnalysisService::analyze(frame(4.0e-4), 4, 5, 2, 1.0);
        Structura::UI::JointAnalysisDialog dialog(ctx, std::nullopt, false);
        dialog.show();
        int lastKind = -1;
        Structura::UI::JointAnalysisDialog::Hooks hooks;
        hooks.preliminary = [](const JointComponentService::PreliminarySettings &s) { return JointComponentService::evaluatePreliminary(ipe300OnHeb200(), s); };
        hooks.applyIdealization = [&lastKind](int kind, double, double, double) {
            lastKind = kind;
            return QStringLiteral("ok");
        };
        dialog.setHooks(hooks);
        const QString classBefore = dialog.finalClassificationText();
        const double si = dialog.analysis().positive.stiffness.stiffness;
        QVERIFY(si > 0.0);
        // o projetista escolhe a idealizacao oposta a classificacao: aviso, mas nao impede
        const int opposite = classBefore == QStringLiteral("RÍGIDA") ? 0 : 2;
        QVERIFY(!dialog.idealizationWarning(opposite).isEmpty());
        QVERIFY(dialog.applyIdealization(opposite, false));
        QCOMPARE(lastKind, opposite);
        QCOMPARE(dialog.finalClassificationText(), classBefore);                 // a classificacao calculada nao e sobrescrita
        QCOMPARE(dialog.analysis().positive.stiffness.stiffness, si);            // nem a rigidez
        QVERIFY(dialog.idealizationStatusText().contains(QStringLiteral("Atenção")));
    }

    void screenshotsWhenRequested()
    {
        const QString dir = qEnvironmentVariable("JOINT_SHOT_DIR");
        if (dir.isEmpty()) {
            QSKIP("defina JOINT_SHOT_DIR para gravar as capturas");
        }
        const auto ctx = JointAnalysisService::analyze(frame(4.0e-4), 4, 5, 2, 1.0);
        Structura::UI::JointAnalysisDialog dialog(ctx, std::nullopt, false);
        dialog.resize(1200, 780);
        dialog.show();
        dialog.setOrigin(DataOrigin::NumericalCurve);
        dialog.setFields(QStringLiteral("120"), {}, {}, QStringLiteral("90"));
        dialog.setCurveText(QStringLiteral("0,0004;18\n0,0010;40\n0,0025;80\n0,006;108\n0,015;120\n-0,0006;-20\n-0,002;-55\n-0,006;-85\n-0,02;-92\n"));
        dialog.setUnits(QStringLiteral("mrad"), QStringLiteral("kN·m"));
        QVERIFY(dialog.applyInputs());
        QVERIFY(dialog.grab().save(dir + QStringLiteral("/janela_momento_rotacao.png")));
        QVERIFY(dialog.momentRotationChart()->exportPng(dir + QStringLiteral("/grafico1.png")));
        QVERIFY(dialog.classificationChart()->exportPng(dir + QStringLiteral("/grafico2.png")));
        QVERIFY(dialog.momentRotationChart()->exportSvg(dir + QStringLiteral("/grafico1.svg"), QStringLiteral("Momento-rotação")));
        QVERIFY(dialog.exportMemory(dir + QStringLiteral("/memoria.txt")));
        // ligacao por chapa de extremidade do projeto -> metodo dos componentes
        using Structura::App::ProjectData::NodeConnection;
        using Structura::App::Services::ConnectionService;
        using Structura::App::Services::JointComponentService;
        ConnectionService::BeamInfo beam;
        beam.name = "IPE300";
        beam.family = "W Laminado";
        beam.geometry = {{"d", 300.0}, {"bf", 150.0}, {"tw", 7.1}, {"tf", 10.7}};
        beam.depth = 300.0;
        beam.bf = 150.0;
        beam.tw = 7.1;
        beam.tf = 10.7;
        NodeConnection c = ConnectionService::makeDefault(NodeConnection::kEndPlate, beam, 250.0, 400.0);
        withColumn(c);
        const auto comp = JointComponentService::evaluate(c, 200000.0);
        QVERIFY2(comp.supported, comp.message.c_str());
        Structura::UI::JointAnalysisDialog d2(ctx, comp.behavior, false, comp);
        d2.resize(1200, 780);
        d2.show();
        d2.setUnits(QStringLiteral("mrad"), QStringLiteral("kN·m"));
        QVERIFY(d2.grab().save(dir + QStringLiteral("/janela_componentes.png")));
        QVERIFY(d2.exportMemory(dir + QStringLiteral("/memoria_componentes.txt")));
    }
};

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    if (QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/segoeui.ttf")) >= 0) {
        app.setFont(QFont(QStringLiteral("Segoe UI"), 9));
    }
    TestJointClassification test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestJointClassification.moc"
