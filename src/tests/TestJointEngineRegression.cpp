// J2 do Joint Mechanical Engine: os WRAPPERS (EnComponentMethodModel, NbrClassifier) devem reproduzir EXATAMENTE (igualdade de double, sem tolerancia)
// os resultados das funcoes originais `JointComponents` e `JointClassification`. A regressao do servico completo continua em TestMomentRotationRegression
// (golden textual, nao regravado). Nenhum calculo e reescrito: o teste compara o wrapper com a chamada direta.
#include "design/JointClassification.h"
#include "design/JointComponents.h"
#include "design/jointengine/EnComponentMethodModel.h"
#include "design/jointengine/NbrClassifier.h"

#include <QtTest/QtTest>

#include <cmath>
#include <cstring>

namespace JC = Structura::Design::JointComponents;
namespace JS = Structura::Design::JointClass;
namespace JE = Structura::Design::JointEngine;

namespace {

bool sameBits(double a, double b)
{
    return std::memcmp(&a, &b, sizeof(double)) == 0;
}

std::vector<JC::ModelInput> inputs()
{
    std::vector<JC::ModelInput> v;
    JC::ModelInput welded;
    welded.kind = JC::ModelInput::WeldedFlanges;
    v.push_back(welded);
    JC::ModelInput plate1;
    plate1.kind = JC::ModelInput::ExtendedEndPlate;
    plate1.tensionRows = 1;
    v.push_back(plate1);
    JC::ModelInput plate2 = plate1;
    plate2.tensionRows = 2;
    v.push_back(plate2);
    JC::ModelInput plate3 = plate1;
    plate3.tensionRows = 3;
    plate3.plateThickness = 22.0;
    plate3.beamDepth = 400.0;
    plate3.beamBf = 180.0;
    plate3.beamTf = 13.5;
    plate3.beamTw = 8.6;
    v.push_back(plate3);
    JC::ModelInput heavy = welded;
    heavy.colDepth = 300.0;
    heavy.colBf = 300.0;
    heavy.colTf = 19.0;
    heavy.colTw = 11.0;
    heavy.colRadius = 27.0;
    heavy.colFy = 355.0;
    v.push_back(heavy);
    return v;
}

} // namespace

class TestJointEngineRegression : public QObject
{
    Q_OBJECT
private slots:
    void descriptorIsComplete()
    {
        QVERIFY(JE::validateDescriptor(JE::EnComponentMethodModel::descriptor()).empty());
        QCOMPARE(JE::EnComponentMethodModel::descriptor().resultKind, JE::ResultKind::NormativeResistance);
    }

    void wrapperEqualsOriginalFunctionsBitForBit()
    {
        int n = 0;
        for (const JC::ModelInput &in : inputs()) {
            const auto stiff = JC::computeStiffness(in);
            const auto res = JC::computeResistance(in);
            QVERIFY2(stiff.ok && res.ok, "entrada de teste deve ser calculavel");
            const double sIni = stiff.initialStiffnessKnm();
            const double mj = res.momentResistanceKnm();
            const auto metrics = JC::curveMetrics(sIni, mj, JC::kPsiWeldedAndEndPlate);
            const auto curve = JC::momentRotationCurve(sIni, mj, JC::kPsiWeldedAndEndPlate, 40);

            const JE::MechanicalResponse r = JE::EnComponentMethodModel::respond(in, 40);
            QVERIFY(r.consistent());
            QVERIFY(JE::allowsNumbers(r.status));
            QCOMPARE(r.kind, JE::ResponseKind::MomentRotation);
            QVERIFY(r.initialStiffness && sameBits(r.initialStiffness->value, sIni));
            QVERIFY(r.secantStiffness && sameBits(r.secantStiffness->value, metrics.secantAtResistance));
            QVERIFY(r.tangentStiffness && sameBits(r.tangentStiffness->value, metrics.tangentAtResistance));
            QCOMPARE(r.resistances.size(), std::size_t(1));
            QVERIFY(sameBits(r.resistances[0].value, mj));
            QCOMPARE(r.resistances[0].kind, JE::ResultKind::NormativeResistance);
            QCOMPARE(r.curves.size(), std::size_t(1));
            QCOMPARE(r.curves[0].points.size(), curve.size());
            for (std::size_t i = 0; i < curve.size(); ++i) {
                QVERIFY(sameBits(r.curves[0].points[i].first, curve[i].theta));
                QVERIFY(sameBits(r.curves[0].points[i].second, curve[i].moment));
            }
            QVERIFY(!r.deformationCapacity);   // capacidade de rotacao NAO e calculada pelo modelo EN existente
            QVERIFY(!r.limitations.empty());
            ++n;
        }
        QCOMPARE(n, 5);
    }

    void invalidInputsGiveStatusAndNoNumbers()
    {
        JC::ModelInput bad;
        bad.kind = JC::ModelInput::WeldedFlanges;
        bad.beamBf = 0.0;
        bad.beamDepth = 0.0;
        const auto direct = JC::computeStiffness(bad);
        const JE::MechanicalResponse r = JE::EnComponentMethodModel::respond(bad, 40);
        if (!direct.ok) {
            QCOMPARE(r.status, JE::ValidityStatus::InsufficientData);
            QVERIFY(r.curves.empty() && r.resistances.empty() && !r.initialStiffness);
            QVERIFY(!r.messages.empty());
            QVERIFY(r.consistent());
        }
        const JE::MechanicalResponse few = JE::EnComponentMethodModel::respond(inputs()[0], 1);
        QCOMPARE(few.status, JE::ValidityStatus::InsufficientData);
        QVERIFY(few.curves.empty());
    }

    void classifierEqualsAnalyzeJointBitForBit()
    {
        const JC::ModelInput in = inputs()[2];
        const auto stiff = JC::computeStiffness(in);
        const auto res = JC::computeResistance(in);
        const double sIni = stiff.initialStiffnessKnm();
        const double mj = res.momentResistanceKnm();

        JS::JointBehavior direct;
        direct.origin = JS::DataOrigin::ComponentMethod;
        direct.positive.curve = JC::momentRotationCurve(sIni, mj, JC::kPsiWeldedAndEndPlate, 40);
        direct.positive.resistantMoment = mj;
        const JS::Limits limits = JS::limitsFor(2.0e8, 8.356e-5, 6.0);
        JS::StoreyStiffness storey;
        const JS::JointAnalysis expected = JS::analyzeJoint(direct, limits, storey, JS::JointKind::BeamColumn);

        const JE::NbrClassifier classifier(limits, storey, JS::JointKind::BeamColumn);
        const JE::MechanicalResponse r = JE::EnComponentMethodModel::respond(in, 40);
        const JS::JointAnalysis got = classifier.analyze(r);
        QVERIFY(expected.primary() && got.primary());
        const auto &e = expected.primary()->classification;
        const auto &g = got.primary()->classification;
        QVERIFY(sameBits(e.stiffness, g.stiffness));
        QVERIFY(sameBits(e.eta, g.eta));
        QCOMPARE(e.byStiffness, g.byStiffness);
        QCOMPARE(e.final, g.final);
        QCOMPARE(e.justification, g.justification);
        QVERIFY(sameBits(expected.primary()->stiffness.stiffness, got.primary()->stiffness.stiffness));
    }

    void classifierWithoutDataIsNotVerifiableNeverApproved()
    {
        const JS::Limits limits = JS::limitsFor(2.0e8, 8.356e-5, 6.0);
        const JE::NbrClassifier classifier(limits, JS::StoreyStiffness{}, JS::JointKind::BeamColumn);
        JE::MechanicalResponse empty;   // InsufficientData, sem curva
        const JE::ClassificationResult res = classifier.classify(empty, {});
        QCOMPARE(res.verdict, JE::Verdict::NaoVerificavel);
        QVERIFY(res.classLabel.empty());
        QCOMPARE(res.standard, std::string("NBR 8800"));
    }

    void classificationResultCarriesStandardAndEdition()
    {
        const JS::Limits limits = JS::limitsFor(2.0e8, 8.356e-5, 6.0);
        const JE::NbrClassifier classifier(limits, JS::StoreyStiffness{}, JS::JointKind::BeamColumn);
        const JE::MechanicalResponse r = JE::EnComponentMethodModel::respond(inputs()[1], 40);
        const JE::ClassificationResult res = classifier.classify(r, {});
        QCOMPARE(res.edition, std::string("2024"));
        QVERIFY(!res.criteria.empty());
        QVERIFY(res.verdict != JE::Verdict::NaoAtende);   // o classificador nunca reprova: NaoAtende e da comparacao com o comportamento desejado
    }

    void wrapperIsDeterministic()
    {
        const auto a = JE::EnComponentMethodModel::respond(inputs()[3], 40);
        const auto b = JE::EnComponentMethodModel::respond(inputs()[3], 40);
        QCOMPARE(a.curves[0].points.size(), b.curves[0].points.size());
        for (std::size_t i = 0; i < a.curves[0].points.size(); ++i) {
            QVERIFY(sameBits(a.curves[0].points[i].second, b.curves[0].points[i].second));
        }
    }
};

QTEST_APPLESS_MAIN(TestJointEngineRegression)
#include "TestJointEngineRegression.moc"
