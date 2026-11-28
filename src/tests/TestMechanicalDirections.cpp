// Incremento 5, Frente A: resposta mecânica nos DOIS sentidos do momento. Comparação independente: o sentido negativo é recomputado aqui montando JointComponents::ModelInput
// à mão (fileiras de parafusos abaixo da mesa inferior atuando como tracionadas) e chamando computeStiffness/computeResistance diretamente; a identidade de espelho
// (positivo de A = negativo do espelho de A) prova que o negativo não é o sinal trocado da curva positiva. Não é validação experimental.
#include "app/services/ConnectionMechanicalResponseService.h"
#include "app/services/ConnectionRequirementService.h"
#include "app/services/ConnectionService.h"
#include "app/services/JointComponentService.h"
#include "design/EndPlateConnection.h"
#include "design/JointComponents.h"
#include "design/jointengine/RealVsRequired.h"

#include <QtTest/QtTest>

#include <cmath>
#include <cstring>
#include <limits>

namespace AS = Structura::App::Services;
namespace JE = Structura::Design::JointEngine;
namespace JS = Structura::Design::JointClass;
namespace JC = Structura::Design::JointComponents;
namespace D = Structura::Design::Connections;
using NC = Structura::App::ProjectData::NodeConnection;
using Dir = JS::Direction;
using JE::ComparisonOutcome;

namespace {

constexpr double kE = 200000.0;
constexpr double kPi = 3.14159265358979323846;

AS::ConnectionService::BeamInfo beam()
{
    AS::ConnectionService::BeamInfo b;
    b.name = "IPE300";
    b.family = "W Laminado";
    b.geometry = {{"d", 300.0}, {"bf", 150.0}, {"tw", 7.1}, {"tf", 10.7}};
    b.depth = 300.0;
    b.bf = 150.0;
    b.tw = 7.1;
    b.tf = 10.7;
    return b;
}

void setColumn(NC &c, double d, double bf, double tf, double tw, double r)
{
    c.values["supD"] = d;
    c.values["supBf"] = bf;
    c.values["supTf"] = tf;
    c.values["supTw"] = tw;
    c.values["supR"] = r;
    c.values["supFy"] = 250.0;
    c.values["supFu"] = 400.0;
    c.values["supConfirmed"] = 1.0;
}

NC endPlate(int tRows, int sRows)
{
    NC c = AS::ConnectionService::makeDefault(NC::kEndPlate, beam(), 250.0, 400.0);
    c.id = 1;
    c.nodeId = 4;
    c.memberId = 5;
    setColumn(c, 260, 146, 12.7, 7.6, 10);
    c.values["tRows"] = tRows;
    c.values["sRows"] = sRows;
    return c;
}

/// Caso em que as fileiras limitam de forma diferente em cada sentido (pilar esbelto, chapa 16 mm, 3 fileiras acima e 1 abaixo).
NC slenderColumnCase()
{
    NC c = endPlate(3, 1);
    setColumn(c, 152, 160, 9, 6, 15);
    c.values["plateT"] = 16.0;
    return c;
}

bool sameBits(double a, double b) { return std::memcmp(&a, &b, sizeof(double)) == 0; }

/// Monta o ModelInput do SENTIDO NEGATIVO diretamente dos dados da ligação (fileiras abaixo da mesa inferior = tracionadas), sem passar pelo serviço sob teste.
JC::ModelInput negativeInput(const NC &c)
{
    D::EndPlateInput in = AS::ConnectionService::endPlateInput(c);
    const int above = std::max(1, in.tensionRows);
    const int below = std::max(1, in.shearRows);
    JC::ModelInput mi;
    mi.youngModulus = kE;
    mi.kind = JC::ModelInput::ExtendedEndPlate;
    mi.beamDepth = in.beamDepth;
    mi.beamBf = in.flangeWidth;
    mi.beamTf = in.flangeThickness;
    mi.beamTw = in.webThickness;
    mi.beamFy = in.beam.fy;
    mi.colDepth = in.colDepth;
    mi.colBf = in.colFlangeWidth;
    mi.colTf = in.colFlangeThickness;
    mi.colTw = in.colWebThickness;
    mi.colRadius = in.colRadius;
    mi.colFy = in.column.fy;
    mi.plateWidth = in.plateWidth;
    mi.plateThickness = in.plateThickness;
    mi.plateFy = in.plate.fy;
    mi.weldLeg = in.weldFlangeLeg;
    mi.flangeThroat = in.weldFlangeLeg / std::sqrt(2.0);
    mi.gauge = in.gauge;
    mi.tensionRows = below;                                                       // sentido negativo: as fileiras abaixo tracionam
    mi.firstRow = in.firstRow;
    mi.pitch = in.pitch;
    mi.edgeVertical = in.edgeVertical;
    mi.plateBelowFlange = in.firstRow + (above - 1) * in.pitch + in.edgeVertical;   // lado comprimido = a extensao acima
    mi.boltDiameter = in.db;
    const double ab = kPi * in.db * in.db / 4.0;
    mi.boltStressArea = 0.78 * ab;
    mi.boltTensionResistance = 0.75 * ab * in.grade.fub / in.gammaA2;
    mi.beta = in.beamOnBothSides ? 0.0 : 1.0;
    mi.webStiffened = in.continuityPlates;
    mi.doublerThickness = in.doublerThickness;
    mi.gammaYield = in.gammaA1;
    return mi;
}

JE::RequiredBehaviorItem itemWithRef(double referenceKnm, JE::RequiredKind required, double kvKp = 0.5)
{
    JE::RequiredBehaviorItem i;
    i.beamId = 5;
    i.otherId = 7;
    i.kind = JS::JointKind::BeamColumn;
    i.required = required;
    i.basis = JE::RequirementBasis::ModelSpring;
    i.status = JE::ValidityStatus::ValidWithCaveats;
    i.systemConsistency = JE::SystemConsistency::Consistent;
    i.limitsAvailable = true;
    i.limits = JS::limitsFor(referenceKnm, 1.0, 1.0);
    i.referenceKnm = referenceKnm;
    i.storey.status = JS::StoreyStatus::Ok;
    i.storey.ratio = kvKp;
    i.kvKp = kvKp;
    if (required == JE::RequiredKind::SemiRigid) {
        i.springKnm = 10000.0;
    }
    return i;
}

JE::ConnectionRequirementComparison withOutcome(ComparisonOutcome o)
{
    JE::ConnectionRequirementComparison c;
    c.outcome = o;
    return c;
}

} // namespace

class TestMechanicalDirections : public QObject
{
    Q_OBJECT
private slots:
    // ------------------------------------------------------------------------------------------------------------- simetria e assimetria
    void symmetryIsDemonstratedByTheConfigurationAndBothDirectionsAreCalculated()
    {
        const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(endPlate(2, 2), kE, 40);
        QVERIFY(b.positive.available && b.negative.available);
        QVERIFY(b.symmetryDemonstrated);
        QVERIFY(sameBits(b.positive.initialStiffnessKnm, b.negative.initialStiffnessKnm));
        QVERIFY(sameBits(b.positive.resistanceKnm, b.negative.resistanceKnm));
        QCOMPARE(b.positive.response.curves[0].points, b.negative.response.curves[0].points);
        QVERIFY(b.note.find("DEMONSTRADA") != std::string::npos);
        QVERIFY(b.negative.hypotheses.back().find("DEMONSTRADA") != std::string::npos);
        for (const int n : {1, 3}) {
            QVERIFY(AS::ConnectionMechanicalResponseService::evaluateBoth(endPlate(n, n), kE, 40).symmetryDemonstrated);
        }
    }

    void defaultGeometryIsAsymmetricAndTheNegativeIsNotTheSignFlippedPositive()
    {
        const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(endPlate(2, 1), kE, 40);   // padrão do cadastro: 2 fileiras acima, 1 abaixo
        QVERIFY(b.positive.available && b.negative.available);
        QVERIFY(!b.symmetryDemonstrated);
        QVERIFY(b.note.find("ASSIMÉTRICOS") != std::string::npos);
        QVERIFY(b.positive.initialStiffnessKnm > b.negative.initialStiffnessKnm * 1.05);   // mais fileiras tracionadas: mais rígida
        QVERIFY(b.positive.resistanceKnm > b.negative.resistanceKnm * 1.05);
        QCOMPARE(b.positive.rows.size(), std::size_t(2));
        QCOMPARE(b.negative.rows.size(), std::size_t(1));
        QVERIFY(b.positive.participants.find("mesa superior") != std::string::npos && b.positive.participants.find("2 fileira") != std::string::npos);
        QVERIFY(b.negative.participants.find("mesa inferior") != std::string::npos && b.negative.participants.find("1 fileira") != std::string::npos);
        QCOMPARE(b.positive.direction, Dir::Positive);
        QCOMPARE(b.negative.direction, Dir::Negative);
    }

    void rowsParticipatingInEachDirectionFollowTheRealBoltRows()
    {
        const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(endPlate(1, 3), kE, 40);
        QCOMPARE(b.positive.rows.size(), std::size_t(1));
        QCOMPARE(b.negative.rows.size(), std::size_t(3));
        QVERIFY(b.negative.initialStiffnessKnm > b.positive.initialStiffnessKnm);
        QVERIFY(b.negative.resistanceKnm > b.positive.resistanceKnm);
        QVERIFY(b.negative.components.size() > b.positive.components.size());
    }

    // ------------------------------------------------------------------------------------------------------------- independência dos cálculos
    void mirrorIdentityPositiveOfAEqualsNegativeOfTheMirror()
    {
        for (const auto &ab : {std::make_pair(3, 1), std::make_pair(1, 3), std::make_pair(2, 1), std::make_pair(1, 2), std::make_pair(3, 2)}) {
            const auto a = AS::ConnectionMechanicalResponseService::evaluateBoth(endPlate(ab.first, ab.second), kE, 40);
            const auto m = AS::ConnectionMechanicalResponseService::evaluateBoth(endPlate(ab.second, ab.first), kE, 40);
            QVERIFY(a.positive.available && m.negative.available);
            QVERIFY2(sameBits(a.positive.initialStiffnessKnm, m.negative.initialStiffnessKnm), "Sj,ini: positivo de A != negativo do espelho");
            QVERIFY2(sameBits(a.positive.resistanceKnm, m.negative.resistanceKnm), "Mj,Rd: positivo de A != negativo do espelho");
            QCOMPARE(a.positive.response.curves[0].points, m.negative.response.curves[0].points);
            QCOMPARE(a.positive.components.size(), m.negative.components.size());
            QVERIFY(sameBits(a.negative.initialStiffnessKnm, m.positive.initialStiffnessKnm));
            QVERIFY(sameBits(a.negative.resistanceKnm, m.positive.resistanceKnm));
        }
    }

    void negativeEqualsAnIndependentRecomputationFromTheComponentMethod()
    {
        for (const NC &c : {endPlate(2, 1), endPlate(1, 3), slenderColumnCase()}) {
            const JC::ModelInput mi = negativeInput(c);
            const JC::StiffnessResult st = JC::computeStiffness(mi);
            const JC::Resistance rs = JC::computeResistance(mi);
            QVERIFY(st.ok && rs.ok);
            const auto r = AS::ConnectionMechanicalResponseService::evaluate(c, kE, 40, Dir::Negative);
            QVERIFY2(r.available, r.message.c_str());
            QVERIFY(sameBits(r.initialStiffnessKnm, st.initialStiffnessKnm()));
            QVERIFY(sameBits(r.resistanceKnm, rs.momentResistanceKnm()));
            QCOMPARE(r.rows.size(), st.rows.size());
            const auto curve = JC::momentRotationCurve(st.initialStiffnessKnm(), rs.momentResistanceKnm(), JC::kPsiWeldedAndEndPlate, 40);
            QCOMPARE(r.response.curves[0].points.size(), curve.size());
            for (std::size_t i = 0; i < curve.size(); ++i) {
                QVERIFY(sameBits(r.response.curves[0].points[i].first, curve[i].theta) && sameBits(r.response.curves[0].points[i].second, curve[i].moment));
            }
        }
    }

    void governingRowMechanismAndResistanceChangeBetweenDirections()
    {
        const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(slenderColumnCase(), kE, 40);
        QVERIFY(b.positive.available && b.negative.available);
        QVERIFY(b.positive.initialStiffnessKnm > b.negative.initialStiffnessKnm * 1.05);   // rigidezes diferentes
        QVERIFY(b.positive.resistanceKnm > b.negative.resistanceKnm * 1.3);                // resistências diferentes
        // a fileira inferior do sentido positivo é limitada pelo painel de alma (distribuição 6.2.7.2); no sentido negativo há uma só fileira, sem limitação
        bool positiveLimited = false, negativeLimited = false;
        for (const auto &row : b.positive.rows) {
            positiveLimited = positiveLimited || !row.limitedBy.empty();
        }
        for (const auto &row : b.negative.rows) {
            negativeLimited = negativeLimited || !row.limitedBy.empty();
        }
        QVERIFY(positiveLimited);
        QVERIFY(!negativeLimited);
        // Limite honesto: o componente de teto (menor entre painel, alma comprimida e mesa comprimida) coincidiu nos dois sentidos em todas as 972 geometrias amostradas
        // (varredura de perfis de pilar, espessuras de chapa e fileiras); o que muda entre os sentidos é a limitação por fileira, Sj,ini e Mj,Rd.
    }

    void negativeNbrLevelUsesTheSwappedZonesToo()
    {
        const NC c = endPlate(2, 1);
        const NC mirror = endPlate(1, 2);
        const auto neg = AS::JointComponentService::evaluate(c, kE, Dir::Negative);
        const auto pos = AS::JointComponentService::evaluate(mirror, kE, Dir::Positive);
        QVERIFY(neg.supported && pos.supported);
        QVERIFY(sameBits(neg.momentResistanceNbrKnm, pos.momentResistanceNbrKnm));
        QCOMPARE(neg.nbrBelowCurve, pos.nbrBelowCurve);
    }

    // ------------------------------------------------------------------------------------------------------------- regressão do sentido positivo
    void positiveDirectionIsBitIdenticalToTheIncrement3Response()
    {
        for (const NC &c : {endPlate(2, 1), endPlate(3, 2), slenderColumnCase()}) {
            const auto old = AS::ConnectionMechanicalResponseService::evaluate(c, kE, 40);
            const auto both = AS::ConnectionMechanicalResponseService::evaluateBoth(c, kE, 40);
            QVERIFY(old.available && both.positive.available);
            QVERIFY(sameBits(old.initialStiffnessKnm, both.positive.initialStiffnessKnm));
            QVERIFY(sameBits(old.resistanceKnm, both.positive.resistanceKnm));
            QCOMPARE(old.response.curves[0].points, both.positive.response.curves[0].points);
            QCOMPARE(old.analysisFingerprint, both.positive.analysisFingerprint);   // AFP do sentido positivo preservado
            QCOMPARE(old.configurationFingerprint, both.positive.configurationFingerprint);
            QCOMPARE(old.memory, both.positive.memory);
            const auto legacy = AS::JointComponentService::evaluate(c, kE);   // serviço legado inalterado: sem dados no sentido inverso
            QVERIFY(!legacy.behavior.negative.hasData());
            QVERIFY(sameBits(legacy.stiffness.initialStiffnessKnm(), old.initialStiffnessKnm));
        }
    }

    // ------------------------------------------------------------------------------------------------------------- fingerprints e proveniência
    void fingerprintsAreIndependentPerDirection()
    {
        const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(endPlate(2, 2), kE, 40);   // até a ligação simétrica tem AFP distinto por sentido
        QCOMPARE(b.positive.configurationFingerprint, b.negative.configurationFingerprint);   // a configuração é uma só
        QVERIFY(b.positive.analysisFingerprint != b.negative.analysisFingerprint);            // a análise é uma por sentido
        QVERIFY(b.positive.response.provenance.parameters.count("direction") == 0);
        QVERIFY(b.negative.response.provenance.parameters.count("direction") == 1);
        const auto again = AS::ConnectionMechanicalResponseService::evaluateBoth(endPlate(2, 2), kE, 40);
        QCOMPARE(b.negative.analysisFingerprint, again.negative.analysisFingerprint);   // reprodutível
        const auto changed = AS::ConnectionMechanicalResponseService::evaluateBoth(endPlate(2, 1), kE, 40);
        QVERIFY(changed.negative.analysisFingerprint != b.negative.analysisFingerprint);
        QVERIFY(changed.positive.configurationFingerprint != b.positive.configurationFingerprint);
        QVERIFY(b.negative.response.consistent() && b.positive.response.consistent());
        const auto neg = AS::JointComponentService::evaluate(endPlate(2, 2), kE, Dir::Negative);
        const auto pos = AS::JointComponentService::evaluate(endPlate(2, 2), kE, Dir::Positive);
        QVERIFY(neg.fingerprint != pos.fingerprint);
    }

    void eachDirectionKeepsItsOwnHypothesesAndLimitations()
    {
        const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(endPlate(2, 1), kE, 40);
        auto has = [](const std::vector<std::string> &v, const char *text) {
            for (const auto &s : v) {
                if (s.find(text) != std::string::npos) {
                    return true;
                }
            }
            return false;
        };
        QVERIFY(has(b.positive.hypotheses, "sentido POSITIVO"));
        QVERIFY(has(b.negative.hypotheses, "sentido NEGATIVO"));
        QVERIFY(has(b.negative.limitations, "Sentido negativo"));
        QVERIFY(!has(b.positive.limitations, "Sentido negativo"));
        QVERIFY(!has(b.positive.limitations, "sentido inverso") && !has(b.negative.limitations, "sentido inverso"));   // o aviso legado de "não calculado" saiu
        QVERIFY(!QString::fromStdString(b.positive.message).contains("Sentido inverso"));
    }

    // ------------------------------------------------------------------------------------------------------------- sentidos não suportados e dados insuficientes
    void unsupportedTypologiesHaveNoNumbersInEitherDirection()
    {
        for (const int type : {NC::kShearTab, NC::kDoubleAngle, NC::kGusset, NC::kBasePlate, NC::kSplice, NC::kTubular}) {   // 143 (perfis T) = so resistencia desde o Incremento 6: ver TestBeamSplice
            NC c = AS::ConnectionService::makeDefault(type, beam(), 250.0, 400.0);
            c.nodeId = 4;
            c.memberId = 5;
            setColumn(c, 260, 146, 12.7, 7.6, 10);
            const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(c, kE, 40);
            for (const auto *r : {&b.positive, &b.negative}) {
                QVERIFY2(!r->available && !r->partial, std::to_string(type).c_str());
                QVERIFY(r->response.curves.empty() && !r->response.initialStiffness && r->response.resistances.empty());
                QVERIFY(r->response.consistent());
                QVERIFY(!r->message.empty());
            }
            QVERIFY(!b.symmetryDemonstrated);
        }
    }

    void insufficientDataGivesNoNumbersInBothDirections()
    {
        for (const double e : {0.0, -5.0, std::numeric_limits<double>::quiet_NaN()}) {
            const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(endPlate(2, 1), e, 40);
            for (const auto *r : {&b.positive, &b.negative}) {
                QVERIFY(!r->available);
                QCOMPARE(r->status, JE::ValidityStatus::InsufficientData);
                QVERIFY(r->response.curves.empty());
            }
            QVERIFY(b.negative.directionLabel.find("negativo") != std::string::npos);   // o sentido fica identificado mesmo sem resultado
        }
        NC noColumn = AS::ConnectionService::makeDefault(NC::kEndPlate, beam(), 250.0, 400.0);   // pilar não informado
        noColumn.nodeId = 4;
        noColumn.memberId = 5;
        const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(noColumn, kE, 40);
        QVERIFY(!b.positive.available && !b.negative.available);
        QVERIFY(b.note.empty());
    }

    void weldedFlangesAreSymmetricByTheirOwnData()
    {
        NC c = AS::ConnectionService::makeDefault(144, beam(), 250.0, 400.0);
        c.nodeId = 4;
        c.memberId = 5;
        setColumn(c, 260, 146, 12.7, 7.6, 10);
        const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(c, kE, 40);
        QVERIFY2(b.positive.available && b.negative.available, b.negative.message.c_str());
        QVERIFY(b.symmetryDemonstrated);
        QVERIFY(sameBits(b.positive.initialStiffnessKnm, b.negative.initialStiffnessKnm));
        QVERIFY(sameBits(b.positive.resistanceKnm, b.negative.resistanceKnm));
        QVERIFY(b.positive.analysisFingerprint != b.negative.analysisFingerprint);
    }

    // ------------------------------------------------------------------------------------------------------------- classificação e comparação por sentido
    void eachDirectionIsClassifiedWithItsOwnStiffness()
    {
        const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(slenderColumnCase(), kE, 40);
        QVERIFY(b.positive.available && b.negative.available);
        const double sp = b.positive.initialStiffnessKnm, sn = b.negative.initialStiffnessKnm;
        const double ref = 0.5 * (sp + sn) / 25.0;   // limite rígido a meio caminho: o sentido positivo (maior) é rígido pela rigidez; o negativo (menor) é semirrígido
        JE::RequiredBehaviorAssessment assessment;
        assessment.items = {itemWithRef(ref, JE::RequiredKind::Rigid)};
        NC c = slenderColumnCase();
        c.values["supMember"] = 7.0;
        c.values["supType"] = 1.0;
        const auto d = AS::ConnectionRequirementService::evaluateBoth(c, assessment, b);
        QCOMPARE(d.positive.real.finalClass, JS::StiffnessClass::Rigid);
        QCOMPARE(d.negative.real.finalClass, JS::StiffnessClass::SemiRigid);
        QVERIFY(std::abs(d.positive.real.stiffnessKnm - sp) <= 1e-6 * sp);
        QVERIFY(std::abs(d.negative.real.stiffnessKnm - sn) <= 1e-6 * sn);
        QCOMPARE(d.positive.outcome, ComparisonOutcome::Meets);
        QCOMPARE(d.negative.outcome, ComparisonOutcome::DoesNotMeet);
        QCOMPARE(d.overall, ComparisonOutcome::DoesNotMeet);   // global conservador: um sentido não atende
        QVERIFY(d.summary.find("sentido negativo") != std::string::npos);
        QVERIFY(d.positive.directionLabel.find("positivo") != std::string::npos && d.negative.directionLabel.find("negativo") != std::string::npos);
        QVERIFY(d.rule.find("não define") != std::string::npos);   // a limitação normativa é explicada
        QVERIFY(d.rule.find("Não há média") != std::string::npos);
        QVERIFY(std::abs(d.negative.real.stiffnessKnm - 0.5 * (sp + sn)) > 1.0);   // nenhuma média: cada sentido usa a sua rigidez
    }

    void globalRuleTableOverAllSixteenCombinations()
    {
        using O = ComparisonOutcome;
        const O all[] = {O::Meets, O::DoesNotMeet, O::Indeterminate, O::NotApplicable};
        for (const O p : all) {
            for (const O n : all) {
                const auto d = JE::combineDirections(withOutcome(p), withOutcome(n));
                O expected = O::Indeterminate;
                if (p == O::DoesNotMeet || n == O::DoesNotMeet) {
                    expected = O::DoesNotMeet;
                } else if (p == O::NotApplicable && n == O::NotApplicable) {
                    expected = O::NotApplicable;
                } else if (p == O::Meets && n == O::Meets) {
                    expected = O::Meets;
                }
                QVERIFY2(d.overall == expected, (JE::comparisonOutcomeName(p) + " x " + JE::comparisonOutcomeName(n)).c_str());
            }
        }
    }

    void unavailableDirectionPreventsGlobalMeeting()
    {
        const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(endPlate(2, 2), kE, 40);
        AS::ConnectionMechanicalBoth partial = b;
        partial.negative = AS::ConnectionMechanicalResult();   // sentido negativo indisponível (sem resposta)
        partial.negative.direction = Dir::Negative;
        partial.negative.directionLabel = "negativo (mesa inferior tracionada)";
        partial.negative.message = "sem modelo";
        JE::RequiredBehaviorAssessment assessment;
        assessment.items = {itemWithRef(700.0, JE::RequiredKind::Rigid)};   // positivo da ligação 2x2 (S ~ 23683) é rígido => atende
        NC c = endPlate(2, 2);
        c.values["supMember"] = 7.0;
        const auto d = AS::ConnectionRequirementService::evaluateBoth(c, assessment, partial);
        QCOMPARE(d.positive.outcome, ComparisonOutcome::Meets);
        QCOMPARE(d.negative.outcome, ComparisonOutcome::Indeterminate);
        QCOMPARE(d.overall, ComparisonOutcome::Indeterminate);   // não se declara atendimento global com um sentido indisponível
        QVERIFY(d.summary.find("Não se declara atendimento global") != std::string::npos);
        QVERIFY(!d.negative.real.available);
        const QString mem = QString::fromStdString(d.memory());
        QVERIFY(mem.contains("RESULTADO GLOBAL") && mem.contains("sentido negativo") && mem.contains("sentido positivo"));
    }

    void bothDirectionsMeetingGivesGlobalMeeting()
    {
        const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(endPlate(2, 2), kE, 40);   // simétrica: S ~ 23683
        JE::RequiredBehaviorAssessment assessment;
        assessment.items = {itemWithRef(700.0, JE::RequiredKind::Rigid)};
        NC c = endPlate(2, 2);
        c.values["supMember"] = 7.0;
        const auto d = AS::ConnectionRequirementService::evaluateBoth(c, assessment, b);
        QCOMPARE(d.positive.outcome, ComparisonOutcome::Meets);
        QCOMPARE(d.negative.outcome, ComparisonOutcome::Meets);
        QCOMPARE(d.overall, ComparisonOutcome::Meets);
        QVERIFY(d.summary.find("dois sentidos") != std::string::npos);
    }
};

QTEST_MAIN(TestMechanicalDirections)
#include "TestMechanicalDirections.moc"
