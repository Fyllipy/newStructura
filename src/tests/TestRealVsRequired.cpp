// Incremento 4: classificação normativa da ligação EFETIVAMENTE configurada (NBR 8800:2024, 6.1.2, via JointClassification) e comparação com o comportamento
// requerido do nó. Cobre fronteiras dos limites (0,5 e 25 EIv/Lv), Kv/Kp (6.1.2.2), dados ausentes, os quatro comportamentos requeridos, os quatro resultados
// (Atende / Não atende / Indeterminado / Não aplicável), a integração com a resposta M–θ real do Incremento 3 e a apresentação na janela do nó.
// Comparação independente: a classe esperada é recomputada aqui direto de `JointClass::classify`. Não é validação experimental.
#include "app/services/ConnectionMechanicalResponseService.h"
#include "app/services/ConnectionRequirementService.h"
#include "app/services/ConnectionService.h"
#include "design/JointClassification.h"
#include "design/JointComponents.h"
#include "design/jointengine/RealVsRequired.h"
#include "ui/NodeBehaviorDialog.h"

#include <QtTest/QtTest>

#include <cmath>
#include <limits>

namespace JE = Structura::Design::JointEngine;
namespace JS = Structura::Design::JointClass;
namespace AS = Structura::App::Services;
using NC = Structura::App::ProjectData::NodeConnection;
using JE::ComparisonOutcome;
using JE::RequiredKind;

namespace {

constexpr double kRef = 1000.0;   // E Iv / Lv (kN.m/rad) do item sintetico: rotulada <= 500; rigida >= 25000
constexpr double kMj = 100.0;     // Mj,Rd (kN.m)

JE::RequiredBehaviorItem itemOf(RequiredKind required, double kvKp = 0.5, bool storeyOk = true)
{
    JE::RequiredBehaviorItem i;
    i.beamId = 5;
    i.otherId = 7;
    i.kind = JS::JointKind::BeamColumn;
    i.required = required;
    i.basis = required == RequiredKind::Pinned ? JE::RequirementBasis::ModelRelease : (required == RequiredKind::SemiRigid ? JE::RequirementBasis::ModelSpring : JE::RequirementBasis::ModelImplicitContinuity);
    i.status = JE::ValidityStatus::ValidWithCaveats;
    i.systemConsistency = JE::SystemConsistency::Consistent;
    i.limitsAvailable = true;
    i.limits = JS::limitsFor(1000.0, 1.0, 1.0);   // E Iv/Lv = 1000
    i.referenceKnm = i.limits.referenceStiffness;
    i.pinnedMaxKnm = i.limits.sMin;
    i.rigidMinKnm = i.limits.sMax;
    i.storey.status = storeyOk ? JS::StoreyStatus::Ok : JS::StoreyStatus::Pending;
    i.storey.ratio = kvKp;
    i.storey.reason = storeyOk ? std::string() : std::string("sem pilares no andar");
    i.kvKp = storeyOk ? kvKp : JE::kNaN;
    if (required == RequiredKind::SemiRigid) {
        i.springKnm = 5000.0;
    }
    return i;
}

JE::MechanicalResponse responseWith(double sIni, double mj = kMj)
{
    JE::MechanicalResponse r;
    r.kind = JE::ResponseKind::MomentRotation;
    r.status = JE::ValidityStatus::Valid;
    JE::Curve c;
    c.actionDof = "M";
    for (const auto &p : Structura::Design::JointComponents::momentRotationCurve(sIni, mj, 2.7, 40)) {
        c.points.emplace_back(p.theta, p.moment);
    }
    r.curves.push_back(c);
    JE::Stiffness s;
    s.value = sIni;
    s.unit = "kN.m/rad";
    r.initialStiffness = s;
    JE::ResistanceItem item;
    item.name = "Mj,Rd";
    item.value = mj;
    item.unit = "kN.m";
    r.resistances.push_back(item);
    r.provenance.modelId = "EnComponentMethod";
    return r;
}

ComparisonOutcome outcome(RequiredKind required, double sIni, double kvKp = 0.5, bool storeyOk = true)
{
    const auto response = responseWith(sIni);
    return JE::compareRealWithRequired(itemOf(required, kvKp, storeyOk), &response, {}).outcome;
}

constexpr double kUp = 1.000001;
constexpr double kDown = 0.999999;

} // namespace

class TestRealVsRequired : public QObject
{
    Q_OBJECT
private slots:
    // ------------------------------------------------------------------------------------------------------------------ fronteiras e requeridos
    void pinnedRequiredAroundTheHalfEivLvBoundary()
    {
        QCOMPARE(outcome(RequiredKind::Pinned, 500.0 * kDown), ComparisonOutcome::Meets);
        QCOMPARE(outcome(RequiredKind::Pinned, 500.0 * kUp), ComparisonOutcome::DoesNotMeet);
        QCOMPARE(outcome(RequiredKind::Pinned, 5000.0), ComparisonOutcome::DoesNotMeet);
        QCOMPARE(outcome(RequiredKind::Pinned, 5.0e6), ComparisonOutcome::DoesNotMeet);    // muito rígida
    }

    void rigidRequiredAroundThe25EivLvBoundaryAndStoreyCondition()
    {
        QCOMPARE(outcome(RequiredKind::Rigid, 25000.0 * kUp), ComparisonOutcome::Meets);
        QCOMPARE(outcome(RequiredKind::Rigid, 25000.0 * kDown), ComparisonOutcome::DoesNotMeet);
        QCOMPARE(outcome(RequiredKind::Rigid, 600.0), ComparisonOutcome::DoesNotMeet);
        QCOMPARE(outcome(RequiredKind::Rigid, 100.0), ComparisonOutcome::DoesNotMeet);
        // 6.1.2.2: Kv/Kp < 0,1 => semirrígida mesmo com Si >= 25 EIv/Lv
        QCOMPARE(outcome(RequiredKind::Rigid, 1.0e6, 0.0999), ComparisonOutcome::DoesNotMeet);
        QCOMPARE(outcome(RequiredKind::Rigid, 1.0e6, 0.1), ComparisonOutcome::Meets);   // limite inclusivo: Kv/Kp >= 0,1
        QCOMPARE(outcome(RequiredKind::Rigid, 1.0e6, 0.2), ComparisonOutcome::Meets);
    }

    void semiRigidRequiredInsideAndOutsideTheBand()
    {
        QCOMPARE(outcome(RequiredKind::SemiRigid, 500.0 * kUp), ComparisonOutcome::Meets);
        QCOMPARE(outcome(RequiredKind::SemiRigid, 5000.0), ComparisonOutcome::Meets);
        QCOMPARE(outcome(RequiredKind::SemiRigid, 25000.0 * kDown), ComparisonOutcome::Meets);
        QCOMPARE(outcome(RequiredKind::SemiRigid, 500.0 * kDown), ComparisonOutcome::DoesNotMeet);     // rotulada
        QCOMPARE(outcome(RequiredKind::SemiRigid, 25000.0 * kUp), ComparisonOutcome::DoesNotMeet);     // rígida (Kv/Kp ok)
        QCOMPARE(outcome(RequiredKind::SemiRigid, 25000.0 * kUp, 0.05), ComparisonOutcome::Meets);     // 6.1.2.2: considerada semirrígida
    }

    void storeyPendingMakesTheRigidClassPendingNotInvented()
    {
        QCOMPARE(outcome(RequiredKind::Rigid, 1.0e6, 0.0, false), ComparisonOutcome::Indeterminate);
        QCOMPARE(outcome(RequiredKind::SemiRigid, 1.0e6, 0.0, false), ComparisonOutcome::Indeterminate);
        QCOMPARE(outcome(RequiredKind::Pinned, 1.0e6, 0.0, false), ComparisonOutcome::DoesNotMeet);     // Si > 0,5 EIv/Lv basta, sem Kv/Kp
        QCOMPARE(outcome(RequiredKind::Pinned, 100.0, 0.0, false), ComparisonOutcome::Meets);
        QCOMPARE(outcome(RequiredKind::Rigid, 5000.0, 0.0, false), ComparisonOutcome::DoesNotMeet);     // semirrígida pela rigidez, Kv/Kp irrelevante
        QCOMPARE(outcome(RequiredKind::SemiRigid, 5000.0, 0.0, false), ComparisonOutcome::Meets);
        const auto response = responseWith(1.0e6);
        const auto c = JE::compareRealWithRequired(itemOf(RequiredKind::Rigid, 0.0, false), &response, {});
        QVERIFY(c.real.storeyPending);
        QVERIFY(c.real.classLabel.find("pendente") != std::string::npos);
        QVERIFY(!c.gaps.empty());
        QCOMPARE(c.status, JE::ValidityStatus::InsufficientData);
    }

    // ------------------------------------------------------------------------------------------------------------------ dados ausentes e domínio
    void missingDataIsIndeterminateWithTheGapDeclared()
    {
        const auto missing = JE::compareRealWithRequired(itemOf(RequiredKind::Rigid), nullptr, "tipologia sem modelo mecânico");
        QCOMPARE(missing.outcome, ComparisonOutcome::Indeterminate);
        QVERIFY(missing.summary.find("sem modelo mecânico") != std::string::npos);
        QVERIFY(!missing.real.available && !missing.gaps.empty());

        JE::RequiredBehaviorItem noLimits = itemOf(RequiredKind::Rigid);
        noLimits.limitsAvailable = false;
        const auto response = responseWith(1.0e6);
        QCOMPARE(JE::compareRealWithRequired(noLimits, &response, {}).outcome, ComparisonOutcome::Indeterminate);

        JE::RequiredBehaviorItem undetermined = itemOf(RequiredKind::Rigid);
        undetermined.required = RequiredKind::Undetermined;
        QCOMPARE(JE::compareRealWithRequired(undetermined, &response, {}).outcome, ComparisonOutcome::Indeterminate);

        JE::RequiredBehaviorItem unsupported = itemOf(RequiredKind::Rigid);
        unsupported.status = JE::ValidityStatus::Unsupported;
        QCOMPARE(JE::compareRealWithRequired(unsupported, &response, {}).outcome, ComparisonOutcome::Indeterminate);

        JE::MechanicalResponse stiffnessOnly;   // só rigidez: sem Mj,Rd não há Si a 2/3 Mj,Rd
        stiffnessOnly.kind = JE::ResponseKind::StiffnessOnly;
        stiffnessOnly.status = JE::ValidityStatus::ValidWithCaveats;
        JE::Stiffness s;
        s.value = 1.0e6;
        s.unit = "kN.m/rad";
        stiffnessOnly.initialStiffness = s;
        QCOMPARE(JE::compareRealWithRequired(itemOf(RequiredKind::Rigid), &stiffnessOnly, {}).outcome, ComparisonOutcome::Indeterminate);

        JE::MechanicalResponse noMj = responseWith(1.0e6);
        noMj.resistances.clear();   // curva sem Mj,Rd informado: o módulo exige o dado (não o supre pelo máximo da curva)
        QCOMPARE(JE::compareRealWithRequired(itemOf(RequiredKind::Rigid), &noMj, {}).outcome, ComparisonOutcome::Indeterminate);

        JE::MechanicalResponse blocked = responseWith(1.0e6);
        blocked.status = JE::ValidityStatus::InsufficientData;
        blocked.curves.clear();
        blocked.initialStiffness.reset();
        blocked.resistances.clear();
        QCOMPARE(JE::compareRealWithRequired(itemOf(RequiredKind::Rigid), &blocked, "E ausente").outcome, ComparisonOutcome::Indeterminate);
    }

    void pairsThatAreNotBeamColumnAreNotApplicable()
    {
        JE::RequiredBehaviorItem beamBeam = itemOf(RequiredKind::Rigid);
        beamBeam.kind = JS::JointKind::BeamBeam;
        beamBeam.basis = JE::RequirementBasis::NotApplicable;
        beamBeam.status = JE::ValidityStatus::OutOfDomain;
        const auto response = responseWith(1.0e6);
        const auto c = JE::compareRealWithRequired(beamBeam, &response, {});
        QCOMPARE(c.outcome, ComparisonOutcome::NotApplicable);
        QCOMPARE(c.status, JE::ValidityStatus::OutOfDomain);
        QVERIFY(!c.real.available);   // nenhuma classe é inventada para o par fora do domínio
        QCOMPARE(QString::fromStdString(JE::comparisonOutcomeName(c.outcome)), QStringLiteral("Não aplicável"));
    }

    void fourOutcomesHaveDistinctNames()
    {
        QStringList names;
        for (const auto o : {ComparisonOutcome::Meets, ComparisonOutcome::DoesNotMeet, ComparisonOutcome::Indeterminate, ComparisonOutcome::NotApplicable}) {
            names << QString::fromStdString(JE::comparisonOutcomeName(o));
        }
        QCOMPARE(names, (QStringList {QStringLiteral("Atende"), QStringLiteral("Não atende"), QStringLiteral("Indeterminado"), QStringLiteral("Não aplicável")}));
    }

    // ------------------------------------------------------------------------------------------------------------------ independência e contexto
    void classIsTheOneOfJointClassificationRecomputedIndependently()
    {
        for (const double s : {100.0, 499.0, 501.0, 5000.0, 24999.0, 25001.0, 1.0e6}) {
            for (const double kv : {0.05, 0.5}) {
                const auto response = responseWith(s);
                const auto c = JE::compareRealWithRequired(itemOf(RequiredKind::Rigid, kv), &response, {});
                QVERIFY(c.real.available);
                const JS::Classification direct = JS::classify(s, itemOf(RequiredKind::Rigid, kv).limits, itemOf(RequiredKind::Rigid, kv).storey, JS::JointKind::BeamColumn);
                QCOMPARE(c.real.finalClass, direct.final);
                QCOMPARE(c.real.byStiffness, direct.byStiffness);
                QVERIFY(std::abs(c.real.stiffnessKnm - s) <= 1e-6 * s);          // Si lido da curva = Sj,ini informado
                QVERIFY(std::abs(c.real.eta - s / kRef) <= 1e-6 * s / kRef);
            }
        }
    }

    void contextTravelsWithTheResultWithoutChangingIt()
    {
        const auto response = responseWith(1.0e6);
        JE::RequiredBehaviorItem implicit = itemOf(RequiredKind::Rigid);
        const auto c = JE::compareRealWithRequired(implicit, &response, {});
        QCOMPARE(c.outcome, ComparisonOutcome::Meets);
        QCOMPARE(c.status, JE::ValidityStatus::ValidWithCaveats);   // continuidade implícita: ressalva
        bool implicitNote = false;
        for (const auto &x : c.caveats) {
            implicitNote = implicitNote || x.find("CONTINUIDADE IMPLÍCITA") != std::string::npos;
        }
        QVERIFY(implicitNote);

        JE::RequiredBehaviorItem attention = itemOf(RequiredKind::Pinned);
        attention.systemConsistency = JE::SystemConsistency::NeedsAttention;
        attention.systemNote = "ligação rotulada em estrutura não contraventada";
        const auto pinnedResponse = responseWith(100.0);
        const auto a = JE::compareRealWithRequired(attention, &pinnedResponse, {});
        QCOMPARE(a.outcome, ComparisonOutcome::Meets);   // o contexto estrutural é ressalva, não altera a classe
        bool sys = false;
        for (const auto &x : a.caveats) {
            sys = sys || x.find("não contraventada") != std::string::npos;
        }
        QVERIFY(sys);

        JE::RequiredBehaviorItem undeclared = itemOf(RequiredKind::Pinned);
        undeclared.systemConsistency = JE::SystemConsistency::Undetermined;
        const auto u = JE::compareRealWithRequired(undeclared, &pinnedResponse, {});
        QCOMPARE(u.outcome, ComparisonOutcome::Meets);
        QVERIFY(!u.gaps.empty());   // sistema não declarado: lacuna explícita, nada presumido
    }

    void exactlyConsistentMeetingResultIsValidWithoutCaveats()
    {
        const auto response = responseWith(100.0);
        JE::RequiredBehaviorItem pinned = itemOf(RequiredKind::Pinned);   // liberação declarada + sistema consistente + sem lacunas
        const auto c = JE::compareRealWithRequired(pinned, &response, {});
        QCOMPARE(c.outcome, ComparisonOutcome::Meets);
        QCOMPARE(c.status, JE::ValidityStatus::Valid);
        QVERIFY(c.caveats.empty() && c.gaps.empty());
    }

    void semiRigidRequiredReportsTheSpringRatioWithoutInventingATolerance()
    {
        const auto response = responseWith(2500.0);
        const auto c = JE::compareRealWithRequired(itemOf(RequiredKind::SemiRigid), &response, {});   // k do modelo = 5000
        QCOMPARE(c.outcome, ComparisonOutcome::Meets);
        QVERIFY(std::abs(c.stiffnessRatioToSpring - 0.5) < 1e-6);
        bool tol = false;
        for (const auto &x : c.caveats) {
            tol = tol || x.find("não fixa tolerância") != std::string::npos;
        }
        QVERIFY(tol);
    }

    void doesNotMeetCarriesTheActionHintAndTheMemoryIsComplete()
    {
        const auto response = responseWith(5000.0);
        const auto c = JE::compareRealWithRequired(itemOf(RequiredKind::Pinned), &response, {});
        QCOMPARE(c.outcome, ComparisonOutcome::DoesNotMeet);
        const QString mem = QString::fromStdString(c.memory());
        for (const char *part : {"Resultado: Não atende", "Requerido: rotulado", "Limites (6.1.2.1)", "Justificativas", "Hipóteses", "6.1.2.1", "Ressalvas"}) {
            QVERIFY2(mem.contains(QString::fromUtf8(part)), part);
        }
    }

    void deterministic()
    {
        const auto response = responseWith(5000.0);
        const auto a = JE::compareRealWithRequired(itemOf(RequiredKind::Rigid), &response, {});
        const auto b = JE::compareRealWithRequired(itemOf(RequiredKind::Rigid), &response, {});
        QCOMPARE(QString::fromStdString(a.memory()), QString::fromStdString(b.memory()));
    }

    // ------------------------------------------------------------------------------------------------------------------ integração com o Incremento 3
    void realEndPlateResponseIsClassifiedAndComparedForEachRequiredBehavior()
    {
        NC c = connection();
        const auto mech = AS::ConnectionMechanicalResponseService::evaluate(c, 200000.0, 40);
        QVERIFY2(mech.available, mech.message.c_str());
        const double sIni = mech.initialStiffnessKnm;
        struct Case { double ref; RequiredKind required; ComparisonOutcome expected; JS::StiffnessClass cls; };
        const Case cases[] = {
            {sIni / 10.0, RequiredKind::SemiRigid, ComparisonOutcome::Meets, JS::StiffnessClass::SemiRigid},       // eta = 10
            {sIni / 10.0, RequiredKind::Pinned, ComparisonOutcome::DoesNotMeet, JS::StiffnessClass::SemiRigid},
            {sIni / 10.0, RequiredKind::Rigid, ComparisonOutcome::DoesNotMeet, JS::StiffnessClass::SemiRigid},
            {sIni / 30.0, RequiredKind::Rigid, ComparisonOutcome::Meets, JS::StiffnessClass::Rigid},               // eta = 30
            {sIni / 30.0, RequiredKind::SemiRigid, ComparisonOutcome::DoesNotMeet, JS::StiffnessClass::Rigid},
            {sIni * 3.0, RequiredKind::Pinned, ComparisonOutcome::Meets, JS::StiffnessClass::Pinned},              // eta = 0,33
            {sIni * 3.0, RequiredKind::Rigid, ComparisonOutcome::DoesNotMeet, JS::StiffnessClass::Pinned},
        };
        for (const Case &k : cases) {
            JE::RequiredBehaviorAssessment assessment;
            assessment.nodeId = 4;
            JE::RequiredBehaviorItem item = itemOf(k.required);
            item.limits = JS::limitsFor(k.ref, 1.0, 1.0);
            item.referenceKnm = k.ref;
            assessment.items.push_back(item);
            const auto r = AS::ConnectionRequirementService::evaluate(c, assessment, mech);
            QVERIFY2(r.outcome == k.expected, r.summary.c_str());
            QCOMPARE(r.real.finalClass, k.cls);
            QVERIFY(std::abs(r.real.stiffnessKnm - sIni) <= 1e-6 * sIni);   // a rigidez classificada é a calculada no Incremento 3
        }
    }

    void pairSelectionNeverGuesses()
    {
        NC c = connection();   // viga 5, nó 4
        const auto mech = AS::ConnectionMechanicalResponseService::evaluate(c, 200000.0, 40);
        QVERIFY(mech.available);
        JE::RequiredBehaviorAssessment assessment;
        JE::RequiredBehaviorItem a = itemOf(RequiredKind::Rigid);   // 5–7
        JE::RequiredBehaviorItem b = itemOf(RequiredKind::Rigid);
        b.otherId = 9;                                              // 5–9
        assessment.items = {a, b};
        // sem barra de apoio vinculada e dois pares: ambíguo
        c.values.remove(QStringLiteral("supMember"));
        QCOMPARE(AS::ConnectionRequirementService::evaluate(c, assessment, mech).outcome, ComparisonOutcome::Indeterminate);
        // barra de apoio que não é de nenhum par
        c.values["supMember"] = 99.0;
        QCOMPARE(AS::ConnectionRequirementService::evaluate(c, assessment, mech).outcome, ComparisonOutcome::Indeterminate);
        // barra de apoio vinculada identifica o par
        c.values["supMember"] = 9.0;
        const auto ok = AS::ConnectionRequirementService::evaluate(c, assessment, mech);
        QCOMPARE(ok.otherId, 9);
        QVERIFY(ok.outcome != ComparisonOutcome::Indeterminate || !ok.real.available);
        // viga fora do nó
        NC other = connection();
        other.memberId = 123;
        QCOMPARE(AS::ConnectionRequirementService::evaluate(other, assessment, mech).outcome, ComparisonOutcome::Indeterminate);
        // um único par e ligação sem apoio vinculado: adotado com hipótese registrada
        JE::RequiredBehaviorAssessment single;
        single.items = {a};
        NC unlinked = connection();
        unlinked.values.remove(QStringLiteral("supMember"));
        const auto one = AS::ConnectionRequirementService::evaluate(unlinked, single, mech);
        bool noted = false;
        for (const auto &h : one.hypotheses) {
            noted = noted || h.find("único par avaliado") != std::string::npos;
        }
        QVERIFY(noted);
    }

    void typologyWithoutMechanicalModelIsIndeterminateNotInvented()
    {
        NC c = AS::ConnectionService::makeDefault(NC::kShearTab, beam(), 250.0, 400.0);
        c.nodeId = 4;
        c.memberId = 5;
        const auto mech = AS::ConnectionMechanicalResponseService::evaluate(c, 200000.0, 40);
        QVERIFY(!mech.available);
        JE::RequiredBehaviorAssessment assessment;
        assessment.items = {itemOf(RequiredKind::Pinned)};
        const auto r = AS::ConnectionRequirementService::evaluate(c, assessment, mech);
        QCOMPARE(r.outcome, ComparisonOutcome::Indeterminate);
        QVERIFY(!r.real.available);
        QVERIFY(!std::isfinite(r.real.stiffnessKnm));
    }

    // ------------------------------------------------------------------------------------------------------------------ interface
    void dialogShowsRequiredStiffnessCriterionClassResultAndWarnsOnFailure()
    {
        JE::RequiredBehaviorAssessment assessment;
        assessment.nodeId = 4;
        assessment.status = JE::ValidityStatus::ValidWithCaveats;
        assessment.items = {itemOf(RequiredKind::Pinned)};
        const auto response = responseWith(5000.0);   // semirrígida: não atende ao rotulado
        Structura::UI::NodeBehaviorDialog::Context context;
        Structura::UI::NodeBehaviorDialog::MechanicalEntry entry;
        entry.connectionId = 1;
        entry.memberId = 5;
        entry.typeName = QStringLiteral("Chapa de extremidade");
        entry.result.available = true;
        entry.result.status = JE::ValidityStatus::Valid;
        entry.result.response = response;
        entry.result.initialStiffnessKnm = 5000.0;
        entry.result.resistanceKnm = kMj;
        entry.comparison = JE::compareRealWithRequired(assessment.items.front(), &response, {});
        context.mechanical.push_back(entry);
        AS::DefinedConnection defined;
        defined.id = 1;
        defined.memberId = 5;
        defined.typeName = entry.typeName;
        context.defined.push_back(defined);
        Structura::UI::NodeBehaviorDialog dialog(assessment, context);
        QVERIFY(dialog.alertText().contains(QStringLiteral("NÃO atende"), Qt::CaseInsensitive));
        QVERIFY(dialog.alertText().contains(QStringLiteral("ATENÇÃO")));
        QCOMPARE(dialog.resultRowCount(), 1);
        QVERIFY(dialog.resultCell(0, 8).contains(QStringLiteral("NÃO ATENDE")));
        QVERIFY(dialog.resultCell(0, 8).contains(QStringLiteral("semirrígida")));
        QVERIFY(dialog.definedCell(0, 7).contains(QStringLiteral("NÃO ATENDE")));
        const QString box = dialog.comparisonText();
        for (const char *part : {"Comportamento requerido", "rotulado", "Rigidez inicial", "Critério", "NBR 8800", "6.1.2.1", "Classificação da ligação", "semirrígida", "Resultado da comparação", "NÃO ATENDE"}) {
            QVERIFY2(box.contains(QString::fromUtf8(part)), part);
        }
        const QString notes = dialog.comparisonNotesText();
        for (const char *part : {"JUSTIFICATIVAS", "RESSALVAS", "HIPÓTESES DA CLASSIFICAÇÃO", "HIPÓTESES DA RESPOSTA MECÂNICA"}) {
            QVERIFY2(notes.contains(QString::fromUtf8(part)), part);
        }
    }

    void dialogDistinguishesMeetingFromIndeterminateAndWithoutConnection()
    {
        JE::RequiredBehaviorAssessment assessment;
        assessment.nodeId = 4;
        assessment.items = {itemOf(RequiredKind::Pinned)};
        {   // sem ligação definida
            Structura::UI::NodeBehaviorDialog dialog(assessment, {});
            QVERIFY(dialog.alertText().isEmpty());
            QVERIFY(dialog.resultCell(0, 8).contains(QStringLiteral("sem ligação definida")));
        }
        {   // atende
            const auto response = responseWith(100.0);
            Structura::UI::NodeBehaviorDialog::Context context;
            Structura::UI::NodeBehaviorDialog::MechanicalEntry entry;
            entry.memberId = 5;
            entry.result.available = true;
            entry.result.status = JE::ValidityStatus::Valid;
            entry.result.response = response;
            entry.comparison = JE::compareRealWithRequired(assessment.items.front(), &response, {});
            context.mechanical.push_back(entry);
            Structura::UI::NodeBehaviorDialog dialog(assessment, context);
            QVERIFY(!dialog.alertText().contains(QStringLiteral("NÃO atende"), Qt::CaseInsensitive));
            QVERIFY(dialog.resultCell(0, 8).startsWith(QStringLiteral("Atende")));
        }
        {   // indeterminado (sem resposta)
            Structura::UI::NodeBehaviorDialog::Context context;
            Structura::UI::NodeBehaviorDialog::MechanicalEntry entry;
            entry.memberId = 5;
            entry.result.message = "tipologia sem modelo mecânico";
            entry.comparison = JE::compareRealWithRequired(assessment.items.front(), nullptr, entry.result.message);
            context.mechanical.push_back(entry);
            Structura::UI::NodeBehaviorDialog dialog(assessment, context);
            QVERIFY(dialog.resultCell(0, 8).startsWith(QStringLiteral("Indeterminado")));
            QVERIFY(dialog.comparisonText().contains(QStringLiteral("indisponíveis")));
            QVERIFY(dialog.comparisonNotesText().contains(QStringLiteral("LACUNAS")));
        }
    }

    // ------------------------------------------------------------------------------------------------------------------ Incremento 5: dois sentidos na janela
    void dialogShowsBothDirectionsWithIndependentIndicatorsAndGlobalResult()
    {
        NC c = connection();
        c.values["tRows"] = 3;
        c.values["sRows"] = 1;
        c.values["supMember"] = 7.0;
        const auto both = AS::ConnectionMechanicalResponseService::evaluateBoth(c, 200000.0, 40);
        QVERIFY(both.positive.available && both.negative.available);
        JE::RequiredBehaviorAssessment assessment;
        assessment.nodeId = 4;
        JE::RequiredBehaviorItem item = itemOf(RequiredKind::Rigid);
        const double ref = 0.5 * (both.positive.initialStiffnessKnm + both.negative.initialStiffnessKnm) / 25.0;   // positivo rígido, negativo semirrígido
        item.limits = JS::limitsFor(ref, 1.0, 1.0);
        item.referenceKnm = ref;
        assessment.items = {item};
        const auto directional = AS::ConnectionRequirementService::evaluateBoth(c, assessment, both);
        Structura::UI::NodeBehaviorDialog::Context context;
        Structura::UI::NodeBehaviorDialog::MechanicalEntry entry;
        entry.connectionId = 1;
        entry.memberId = 5;
        entry.typeName = QStringLiteral("Chapa de extremidade");
        entry.result = both.positive;
        entry.negative = both.negative;
        entry.comparison = directional.positive;
        entry.comparisonNegative = directional.negative;
        entry.directional = directional;
        entry.hasNegative = true;
        entry.symmetryNote = QString::fromStdString(both.note);
        context.mechanical.push_back(entry);
        Structura::UI::NodeBehaviorDialog dialog(assessment, context);
        QCOMPARE(dialog.directionRowCount(), 2);
        QVERIFY(dialog.directionCell(0, 0).contains(QStringLiteral("Positivo")) && dialog.directionCell(1, 0).contains(QStringLiteral("Negativo")));
        QVERIFY(dialog.directionCell(0, 2) != dialog.directionCell(1, 2));   // Sj,ini diferentes
        QVERIFY(dialog.directionCell(0, 3) != dialog.directionCell(1, 3));   // Mj,Rd diferentes
        QVERIFY(dialog.directionCell(0, 5).contains(QStringLiteral("3 fileira")) && dialog.directionCell(1, 5).contains(QStringLiteral("1 fileira")));
        QVERIFY(dialog.directionCell(0, 6).contains(QStringLiteral("rígida")) && dialog.directionCell(1, 6).contains(QStringLiteral("semirrígida")));
        QVERIFY(dialog.directionCell(0, 7).startsWith(QStringLiteral("Atende")) && dialog.directionCell(1, 7).contains(QStringLiteral("NÃO ATENDE")));
        QVERIFY(dialog.directionalText().contains(QStringLiteral("NÃO ATENDE")) && dialog.directionalText().contains(QStringLiteral("conservadora")));
        QVERIFY(dialog.directionalText().contains(QStringLiteral("Não há média")));
        QVERIFY(dialog.alertText().contains(QStringLiteral("ATENÇÃO")));
        QVERIFY(dialog.resultCell(0, 8).contains(QStringLiteral("NÃO ATENDE")) && dialog.resultCell(0, 8).contains(QStringLiteral("+: Atende")) && dialog.resultCell(0, 8).contains(QStringLiteral("−: Não atende")) == false);
        QVERIFY(dialog.comparisonText().contains(QStringLiteral("POSITIVO")));
        dialog.selectDirection(1);
        QVERIFY(dialog.comparisonText().contains(QStringLiteral("NEGATIVO")));
        QVERIFY(dialog.comparisonNotesText().contains(QStringLiteral("RESULTADO GLOBAL DOS DOIS SENTIDOS")));
        QVERIFY(dialog.mechanicalSummary(0).contains(QStringLiteral("NEGATIVO")));
        QVERIFY(dialog.mechanicalLevelRows() == 5 && dialog.mechanicalComponentRows() == static_cast<int>(both.negative.components.size()));   // detalhes do sentido selecionado
        dialog.selectDirection(0);
        QVERIFY(dialog.mechanicalComponentRows() == static_cast<int>(both.positive.components.size()));
    }

    void dialogDoesNotMirrorAnUnavailableDirection()
    {
        NC c = connection();
        c.values["supMember"] = 7.0;
        auto both = AS::ConnectionMechanicalResponseService::evaluateBoth(c, 200000.0, 40);
        both.negative = AS::ConnectionMechanicalResult();   // sentido negativo indisponível
        both.negative.direction = Structura::Design::JointClass::Direction::Negative;
        both.negative.directionLabel = "negativo (mesa inferior tracionada)";
        both.negative.message = "sentido não suportado pelo modelo";
        JE::RequiredBehaviorAssessment assessment;
        assessment.nodeId = 4;
        assessment.items = {itemOf(RequiredKind::Rigid)};
        const auto directional = AS::ConnectionRequirementService::evaluateBoth(c, assessment, both);
        Structura::UI::NodeBehaviorDialog::Context context;
        Structura::UI::NodeBehaviorDialog::MechanicalEntry entry;
        entry.memberId = 5;
        entry.result = both.positive;
        entry.negative = both.negative;
        entry.comparison = directional.positive;
        entry.comparisonNegative = directional.negative;
        entry.directional = directional;
        entry.hasNegative = true;
        context.mechanical.push_back(entry);
        Structura::UI::NodeBehaviorDialog dialog(assessment, context);
        QCOMPARE(dialog.directionRowCount(), 2);
        QVERIFY(dialog.directionCell(1, 1).contains(QStringLiteral("INDISPONÍVEL")));
        for (int col = 2; col <= 6; ++col) {
            QCOMPARE(dialog.directionCell(1, col), QStringLiteral("—"));   // nenhum número para o sentido indisponível
        }
        QVERIFY(dialog.directionCell(1, 7).startsWith(QStringLiteral("Indeterminado")));
        QVERIFY(!dialog.directionalText().contains(QStringLiteral("Atende:")));
        QVERIFY(dialog.directionalText().contains(QStringLiteral("Indeterminado")));
        dialog.selectDirection(1);
        QVERIFY(dialog.mechanicalSummary(0).contains(QStringLiteral("Sem números")));
        QCOMPARE(dialog.mechanicalLevelRows(), 0);
        QCOMPARE(dialog.mechanicalComponentRows(), 0);
    }

private:
    static AS::ConnectionService::BeamInfo beam()
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
    static NC connection()
    {
        NC c = AS::ConnectionService::makeDefault(NC::kEndPlate, beam(), 250.0, 400.0);
        c.id = 1;
        c.nodeId = 4;
        c.memberId = 5;
        c.values["supD"] = 260;
        c.values["supBf"] = 146;
        c.values["supTf"] = 12.7;
        c.values["supTw"] = 7.6;
        c.values["supR"] = 10;
        c.values["supFy"] = 250.0;
        c.values["supFu"] = 400.0;
        c.values["supConfirmed"] = 1.0;
        c.values["tRows"] = 2;
        c.values["supMember"] = 7.0;
        c.values["supType"] = 1.0;
        return c;
    }
};

QTEST_MAIN(TestRealVsRequired)
#include "TestRealVsRequired.moc"
