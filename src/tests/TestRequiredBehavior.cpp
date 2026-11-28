// Incremento 1: comportamento estrutural REQUERIDO do no, anterior a qualquer ligacao fisica. Tres camadas: (A) nucleo de decisao (fatos escritos a mao),
// (B) servico que extrai os fatos de um ModelData real (portico plano), (C) dialogo (somente leitura). Referencia independente: os limites 0,5 e 25 EIv/Lv e
// Kv/Kp >= 0,1 sao CALCULADOS AQUI a partir da definicao de 6.1.2 da NBR 8800:2024, nao obtidos do codigo testado. Nao e validacao profissional.
#include "app/SolverBridge.h"
#include "app/services/ConnectionCatalog.h"
#include "app/services/NodeBehaviorService.h"
#include "app/services/TypologyCapabilityService.h"
#include "design/jointengine/RequiredBehavior.h"
#include "ui/NodeBehaviorDialog.h"

#include <QtTest/QtTest>

#include <cmath>

namespace JE = Structura::Design::JointEngine;
namespace JS = Structura::Design::JointClass;
namespace AS = Structura::App::Services;
using SB = Structura::App::SolverBridge;

namespace {

JE::NodeFacts coreFacts(JE::RotationalCondition cond, double spring = JE::kNaN, double kvKp = 0.5, bool limits = true)
{
    JE::NodeFacts f;
    f.nodeId = 7;
    f.standardOrigin = "teste";
    JE::NodeMemberFacts beam;
    beam.memberId = 10;
    beam.role = JE::NodeMemberRole::Beam;
    beam.lengthM = 6.0;
    beam.inertiaM4 = 1e-4;
    beam.youngKnM2 = 2e8;
    beam.condition = cond;
    JE::NodeMemberFacts col;
    col.memberId = 11;
    col.role = JE::NodeMemberRole::Column;
    col.lengthM = 3.0;
    col.inertiaM4 = 1e-4;
    col.youngKnM2 = 2e8;
    col.condition = JE::RotationalCondition::ContinuousImplicit;
    f.members = {beam, col};
    JE::PairFacts p;
    p.beamId = 10;
    p.otherId = 11;
    p.kind = JS::JointKind::BeamColumn;
    p.beamCondition = cond;
    p.beamSpringKnm = spring;
    p.limitsAvailable = limits;
    p.limits = JS::limitsFor(2e8, 1e-4, 6.0);
    if (kvKp > 0.0) {
        p.storey.status = JS::StoreyStatus::Ok;
        p.storey.ratio = kvKp;
        p.storey.beams = 1;
        p.storey.columns = 2;
    } else {
        p.storey.status = JS::StoreyStatus::Pending;
        p.storey.reason = "sem pilares no andar";
    }
    f.pairs = {p};
    return f;
}

JE::NodeFacts withSystem(JE::NodeFacts f, int system, int bracing)
{
    f.structuralSystem = system;
    f.bracing = bracing;
    return f;
}

constexpr double kEIL = 2e8 * 1e-4 / 6.0;   // EIv/Lv = 3333,33 kN.m/rad

// portico plano XZ: pilar 1 (no 1 -> 2), viga 2 (no 2 -> 3), pilar 3 (no 4 -> 3)
SB::ModelData frame(double columnInertia = 1e-4, bool releaseBeamAtNode2 = false, double springAtNode2 = -1.0)
{
    SB::ModelData m;
    SB::MaterialData mat;
    mat.id = 1;
    mat.e = 2.0e11;   // N/m2
    m.materials.push_back(mat);
    SB::SectionData beamSec;
    beamSec.id = 1;
    beamSec.area = 1e-2;
    beamSec.iy = 1e-4;
    beamSec.iz = 1e-4;
    SB::SectionData colSec = beamSec;
    colSec.id = 2;
    colSec.iy = columnInertia;
    colSec.iz = columnInertia;
    m.sections = {beamSec, colSec};
    SB::NodeData n1;
    n1.id = 1;
    n1.restraints = {1, 1, 1, 1, 1, 1};
    SB::NodeData n2;
    n2.id = 2;
    n2.z = 3.0;
    SB::NodeData n3;
    n3.id = 3;
    n3.x = 6.0;
    n3.z = 3.0;
    SB::NodeData n4;
    n4.id = 4;
    n4.x = 6.0;
    n4.restraints = {1, 1, 1, 1, 1, 1};
    m.nodes = {n1, n2, n3, n4};
    SB::MemberData c1;
    c1.id = 1;
    c1.nodeI = 1;
    c1.nodeJ = 2;
    c1.materialId = 1;
    c1.sectionId = 2;
    SB::MemberData b;
    b.id = 2;
    b.nodeI = 2;
    b.nodeJ = 3;
    b.materialId = 1;
    b.sectionId = 1;
    if (releaseBeamAtNode2) {
        b.releases[4] = 1;   // ry e rz na extremidade I (o eixo normal ao plano depende da orientacao da secao: libera os dois)
        b.releases[5] = 1;
    }
    SB::MemberData c3;
    c3.id = 3;
    c3.nodeI = 4;
    c3.nodeJ = 3;
    c3.materialId = 1;
    c3.sectionId = 2;
    m.members = {c1, b, c3};
    if (springAtNode2 >= 0.0) {
        SB::EndSpringData s;
        s.memberId = 2;
        s.k[4] = springAtNode2;
        s.k[5] = springAtNode2;
        m.endSprings.push_back(s);
    }
    return m;
}

constexpr double kForceToKn = 1e-3;   // modelo em N, m

} // namespace

class TestRequiredBehavior;
namespace NodeBehaviorContextHelper {
void run(QObject *test, bool pinned);
}

class TestRequiredBehavior : public QObject
{
    Q_OBJECT
private slots:
    // ================================================================================================ (A) nucleo
    void releasedBeamEndRequiresPinnedWithTheNbrTargets()
    {
        const auto r = JE::assessRequiredBehavior(coreFacts(JE::RotationalCondition::Released));
        QCOMPARE(r.items.size(), std::size_t(1));
        const auto &i = r.items[0];
        QCOMPARE(i.required, JE::RequiredKind::Pinned);
        QCOMPARE(i.basis, JE::RequirementBasis::ModelRelease);
        QCOMPARE(i.status, JE::ValidityStatus::ValidWithCaveats);
        QVERIFY(std::abs(i.referenceKnm - kEIL) < 1e-6);
        QVERIFY(std::abs(i.pinnedMaxKnm - 0.5 * kEIL) < 1e-6);
        QVERIFY(std::abs(i.rigidMinKnm - 25.0 * kEIL) < 1e-6);
        QVERIFY(QString::fromStdString(i.statement).contains("ROTULADO"));
        QVERIFY(QString::fromStdString(i.statement).contains("6.1.3"));
    }

    void springRequiresSemiRigidAndFlagsKInsideAnotherBand()
    {
        const auto mid = JE::assessRequiredBehavior(coreFacts(JE::RotationalCondition::Spring, 5000.0));   // entre 0,5 e 25 EIv/Lv
        QCOMPARE(mid.items[0].required, JE::RequiredKind::SemiRigid);
        QCOMPARE(mid.items[0].basis, JE::RequirementBasis::ModelSpring);
        QVERIFY(mid.items[0].notes.empty() || mid.items[0].notes.size() == 0);
        QCOMPARE(*mid.items[0].springKnm, 5000.0);
        const auto low = JE::assessRequiredBehavior(coreFacts(JE::RotationalCondition::Spring, 1000.0));   // <= 0,5 EIv/Lv
        QVERIFY(!low.items[0].notes.empty());
        QVERIFY(QString::fromStdString(low.items[0].notes.back()).contains("rotulada"));
        const auto high = JE::assessRequiredBehavior(coreFacts(JE::RotationalCondition::Spring, 100000.0));   // >= 25 EIv/Lv
        QVERIFY(!high.items[0].notes.empty());
        QVERIFY(QString::fromStdString(high.items[0].notes.back()).contains("rígida"));
    }

    void implicitContinuityIsNotPresentedAsADeclaredDecision()
    {
        const auto r = JE::assessRequiredBehavior(coreFacts(JE::RotationalCondition::ContinuousImplicit));
        const auto &i = r.items[0];
        QCOMPARE(i.required, JE::RequiredKind::Rigid);
        QCOMPARE(i.basis, JE::RequirementBasis::ModelImplicitContinuity);
        QCOMPARE(i.rigidAdmissibility, JE::RigidAdmissibility::Admissible);   // Kv/Kp = 0,5 >= 0,1
        QVERIFY(QString::fromStdString(i.statement).contains("IMPLÍCITA"));
        bool decisionGap = false;
        for (const auto &g : i.gaps) {
            decisionGap = decisionGap || QString::fromStdString(g).contains("DECISÃO DE PROJETO");
        }
        QVERIFY(decisionGap);   // o programa nao escolhe entre rotulada, semirrigida e rigida
    }

    void rigidTargetNotAdmissibleWhenKvKpBelowOneTenth()
    {
        const auto r = JE::assessRequiredBehavior(coreFacts(JE::RotationalCondition::ContinuousImplicit, JE::kNaN, 0.05));
        QCOMPARE(r.items[0].rigidAdmissibility, JE::RigidAdmissibility::NotAdmissible);
        QVERIFY(QString::fromStdString(r.items[0].statement).contains("SEMIRRÍGIDA"));
        QVERIFY(QString::fromStdString(r.items[0].statement).contains("6.1.2.2"));
        // fronteira: exatamente 0,1 e admitido ("Kv/Kp >= 0,1")
        QCOMPARE(JE::assessRequiredBehavior(coreFacts(JE::RotationalCondition::ContinuousImplicit, JE::kNaN, 0.1)).items[0].rigidAdmissibility, JE::RigidAdmissibility::Admissible);
        QCOMPARE(JE::assessRequiredBehavior(coreFacts(JE::RotationalCondition::ContinuousImplicit, JE::kNaN, 0.0999)).items[0].rigidAdmissibility, JE::RigidAdmissibility::NotAdmissible);
    }

    void pendingStoreyIsAnExplicitGapNotAGuess()
    {
        const auto r = JE::assessRequiredBehavior(coreFacts(JE::RotationalCondition::ContinuousImplicit, JE::kNaN, 0.0));
        QCOMPARE(r.items[0].rigidAdmissibility, JE::RigidAdmissibility::Unknown);
        QVERIFY(std::isnan(r.items[0].kvKp));
        bool gap = false;
        for (const auto &g : r.items[0].gaps) {
            gap = gap || QString::fromStdString(g).contains("Kv/Kp");
        }
        QVERIFY(gap);
    }

    void missingBeamDataGivesInsufficientDataAndNoTargets()
    {
        auto f = coreFacts(JE::RotationalCondition::Released, JE::kNaN, 0.5, false);
        const auto r = JE::assessRequiredBehavior(f);
        QCOMPARE(r.items[0].status, JE::ValidityStatus::InsufficientData);
        QVERIFY(std::isnan(r.items[0].pinnedMaxKnm) && std::isnan(r.items[0].rigidMinKnm));
        QCOMPARE(r.status, JE::ValidityStatus::InsufficientData);
        QVERIFY(!r.items[0].gaps.empty());
    }

    void unknownRotationConditionIsUndetermined()
    {
        const auto r = JE::assessRequiredBehavior(coreFacts(JE::RotationalCondition::Unknown));
        QCOMPARE(r.items[0].required, JE::RequiredKind::Undetermined);
        QCOMPARE(r.items[0].status, JE::ValidityStatus::InsufficientData);
    }

    void nonBeamColumnPairsAreOutOfTheDomainOfClause6_1_2()
    {
        auto f = coreFacts(JE::RotationalCondition::Released);
        f.pairs[0].kind = JS::JointKind::BeamBeam;
        const auto r = JE::assessRequiredBehavior(f);
        QCOMPARE(r.items[0].basis, JE::RequirementBasis::NotApplicable);
        QCOMPARE(r.items[0].status, JE::ValidityStatus::OutOfDomain);
        QCOMPARE(r.status, JE::ValidityStatus::OutOfDomain);
        QVERIFY(std::isnan(r.items[0].pinnedMaxKnm));   // sem metas para o que nao e viga-pilar
    }

    void unsupportedStandardGivesNoNormativeNumbers()
    {
        auto f = coreFacts(JE::RotationalCondition::Released);
        f.standard = "EN 1993-1-8";
        f.edition = "2005";
        const auto r = JE::assessRequiredBehavior(f);
        QCOMPARE(r.items[0].status, JE::ValidityStatus::Unsupported);
        QVERIFY(std::isnan(r.items[0].pinnedMaxKnm));
        QVERIFY(!r.nodeGaps.empty());
    }

    void memoryStatesTheDistinctionAndTheGapsAndIsDeterministic()
    {
        const auto r = JE::assessRequiredBehavior(coreFacts(JE::RotationalCondition::ContinuousImplicit));
        const QString m = QString::fromStdString(r.memory());
        QVERIFY(m.contains("NÃO classifica uma ligação real"));
        QVERIFY(m.contains("NÃO declarados nas propriedades do projeto"));
        QVERIFY(m.contains("6.1.2.1"));
        QVERIFY(m.contains("6.1.2.2"));
        QVERIFY(m.contains("6.1.2.3"));
        QCOMPARE(m, QString::fromStdString(JE::assessRequiredBehavior(coreFacts(JE::RotationalCondition::ContinuousImplicit)).memory()));
    }

    // ================================================================================================ (B) servico sobre ModelData
    void serviceIdentifiesTheElementsConnectedToTheNode()
    {
        const auto facts = AS::NodeBehaviorService::collectFacts(frame(), 2, kForceToKn, {});
        QCOMPARE(facts.members.size(), std::size_t(2));   // pilar 1 e viga 2 chegam ao no 2
        int beams = 0, columns = 0;
        for (const auto &m : facts.members) {
            beams += m.role == JE::NodeMemberRole::Beam;
            columns += m.role == JE::NodeMemberRole::Column;
            QVERIFY(m.lengthM > 0.0 && m.inertiaM4 > 0.0 && m.youngKnM2 > 0.0);
        }
        QCOMPARE(beams, 1);
        QCOMPARE(columns, 1);
        QCOMPARE(facts.pairs.size(), std::size_t(1));
        QCOMPARE(facts.pairs[0].kind, JS::JointKind::BeamColumn);
        QCOMPARE(facts.pairs[0].beamId, 2);
        QCOMPARE(facts.pairs[0].otherId, 1);
    }

    void serviceDefaultModelGivesRigidByImplicitContinuityWithIndependentTargets()
    {
        const auto r = AS::NodeBehaviorService::assess(frame(), 2, kForceToKn, {});
        QCOMPARE(r.status, JE::ValidityStatus::ValidWithCaveats);
        QCOMPARE(r.items.size(), std::size_t(1));
        const auto &i = r.items[0];
        QCOMPARE(i.required, JE::RequiredKind::Rigid);
        QCOMPARE(i.basis, JE::RequirementBasis::ModelImplicitContinuity);
        // EIv/Lv calculado independentemente: E = 2e8 kN/m2, I = 1e-4 m4, L = 6 m
        QVERIFY(std::abs(i.referenceKnm - kEIL) < 1e-6 * kEIL);
        QVERIFY(std::abs(i.pinnedMaxKnm - 0.5 * kEIL) < 1e-6 * kEIL);
        QVERIFY(std::abs(i.rigidMinKnm - 25.0 * kEIL) < 1e-6 * kEIL);
        // Kv/Kp: vigas no andar (1): Iv/Lv = 1e-4/6; pilares que chegam por baixo (2): Ip/Lp = 1e-4/3 -> 0,5
        QVERIFY(std::abs(i.kvKp - 0.5) < 1e-9);
        QCOMPARE(i.rigidAdmissibility, JE::RigidAdmissibility::Admissible);
    }

    void serviceReadsADeclaredReleaseAsPinned()
    {
        const auto r = AS::NodeBehaviorService::assess(frame(1e-4, true), 2, kForceToKn, {});
        QCOMPARE(r.items[0].required, JE::RequiredKind::Pinned);
        QCOMPARE(r.items[0].basis, JE::RequirementBasis::ModelRelease);
    }

    void serviceReadsADeclaredSpringAsSemiRigid()
    {
        const auto r = AS::NodeBehaviorService::assess(frame(1e-4, false, 5.0e6), 2, kForceToKn, {});   // 5e6 N.m/rad = 5000 kN.m/rad
        QCOMPARE(r.items[0].required, JE::RequiredKind::SemiRigid);
        QCOMPARE(r.items[0].basis, JE::RequirementBasis::ModelSpring);
        QVERIFY(std::abs(*r.items[0].springKnm - 5000.0) < 1e-6);
    }

    void serviceStiffColumnsMakeTheRigidTargetNotAdmissible()
    {
        const auto r = AS::NodeBehaviorService::assess(frame(1e-2), 2, kForceToKn, {});   // Kp 100x maior: Kv/Kp = 0,005
        QVERIFY(r.items[0].kvKp < 0.1);
        QCOMPARE(r.items[0].rigidAdmissibility, JE::RigidAdmissibility::NotAdmissible);
    }

    void serviceSupportedNodeIsNotedAndNodeWithOneBarIsInsufficient()
    {
        const auto supported = AS::NodeBehaviorService::assess(frame(), 1, kForceToKn, {});   // no 1: so o pilar chega, apoiado
        QCOMPARE(supported.status, JE::ValidityStatus::InsufficientData);
        QVERIFY(!supported.nodeGaps.empty());
        const auto missing = AS::NodeBehaviorService::assess(frame(), 99, kForceToKn, {});
        QCOMPARE(missing.status, JE::ValidityStatus::InsufficientData);
        QVERIFY(QString::fromStdString(missing.nodeGaps.back()).contains("não encontrado"));
    }

    void serviceIsIndependentOfAnyPhysicalConnectionAndReproducible()
    {
        // ModelData nao contem nenhuma ligacao cadastrada; duas avaliacoes identicas dao a mesma memoria
        const auto a = AS::NodeBehaviorService::assess(frame(), 2, kForceToKn, {});
        const auto b = AS::NodeBehaviorService::assess(frame(), 2, kForceToKn, {});
        QCOMPARE(QString::fromStdString(a.memory()), QString::fromStdString(b.memory()));
        QVERIFY(QString::fromStdString(a.memory()).contains("NÃO classifica uma ligação real"));
    }

    void serviceBeamBeamNodeIsNotApplicable()
    {
        auto m = frame();
        // viga 4 colinear a viga 2 passando pelo no 3 (no 3 passa a ter pilar 3 + vigas)
        SB::NodeData n5;
        n5.id = 5;
        n5.x = 12.0;
        n5.z = 3.0;
        m.nodes.push_back(n5);
        SB::MemberData b2;
        b2.id = 4;
        b2.nodeI = 3;
        b2.nodeJ = 5;
        b2.materialId = 1;
        b2.sectionId = 1;
        m.members.push_back(b2);
        const auto r = AS::NodeBehaviorService::assess(m, 3, kForceToKn, {});
        bool beamBeam = false, beamColumn = false;
        for (const auto &i : r.items) {
            beamBeam = beamBeam || (i.kind == JS::JointKind::BeamBeam && i.basis == JE::RequirementBasis::NotApplicable);
            beamColumn = beamColumn || (i.kind == JS::JointKind::BeamColumn && i.required != JE::RequiredKind::Undetermined);
        }
        QVERIFY(beamBeam);     // par viga-viga: criterio de 6.1.2 nao se aplica
        QVERIFY(beamColumn);   // par viga-pilar do mesmo no segue avaliado
        QCOMPARE(r.items.size(), std::size_t(3));   // tres pares: pilar-viga 2, pilar-viga 4, viga 2-viga 4
    }

    // ================================================================================================ Incremento 2: sistema/contraventamento declarados
    void undeclaredSystemLeavesItInsufficientWithoutADefault()
    {
        const auto r = JE::assessRequiredBehavior(coreFacts(JE::RotationalCondition::Released));   // sistema = 0, contraventamento = 0
        QCOMPARE(r.items[0].systemConsistency, JE::SystemConsistency::Undetermined);
        QCOMPARE(r.items[0].status, JE::ValidityStatus::ValidWithCaveats);   // nunca Valid sem a declaracao
        bool gap = false;
        for (const auto &g : r.items[0].gaps) {
            gap = gap || QString::fromStdString(g).contains("Condição de contraventamento não declarada");
        }
        QVERIFY(gap);
        QVERIFY(QString::fromStdString(r.nodeGaps.front()).contains("NÃO declarados"));
    }

    void pinnedInAnUnbracedStructureNeedsAttention()
    {
        const auto r = JE::assessRequiredBehavior(withSystem(coreFacts(JE::RotationalCondition::Released), 1, 2));
        QCOMPARE(r.items[0].systemConsistency, JE::SystemConsistency::NeedsAttention);
        QVERIFY(QString::fromStdString(r.items[0].systemNote).contains("NÃO contraventada"));
        QCOMPARE(r.items[0].status, JE::ValidityStatus::ValidWithCaveats);
    }

    void pinnedInABracedStructureIsConsistentAndFullyDeterminedIsValid()
    {
        const auto r = JE::assessRequiredBehavior(withSystem(coreFacts(JE::RotationalCondition::Released), 1, 1));
        QCOMPARE(r.items[0].systemConsistency, JE::SystemConsistency::Consistent);
        QVERIFY(r.items[0].gaps.empty());
        QCOMPARE(r.items[0].status, JE::ValidityStatus::Valid);   // declarado, com metas, sem lacunas
        QVERIFY(r.nodeGaps.size() == 0 || !QString::fromStdString(r.nodeGaps.front()).contains("NÃO declarados"));
    }

    void rigidOrSemiRigidNeverBecomeFullyValidWhileTheDecisionIsImplicit()
    {
        const auto rigid = JE::assessRequiredBehavior(withSystem(coreFacts(JE::RotationalCondition::ContinuousImplicit), 1, 2));
        QCOMPARE(rigid.items[0].systemConsistency, JE::SystemConsistency::Consistent);
        QCOMPARE(rigid.items[0].status, JE::ValidityStatus::ValidWithCaveats);   // a decisao de projeto continua pendente
        const auto semi = JE::assessRequiredBehavior(withSystem(coreFacts(JE::RotationalCondition::Spring, 5000.0), 1, 1));
        QCOMPARE(semi.items[0].systemConsistency, JE::SystemConsistency::Consistent);
        QCOMPARE(semi.items[0].status, JE::ValidityStatus::Valid);   // mola declarada + sistema declarado + sem lacunas
    }

    void nonFramedSystemsAreNotJudgedByBracing()
    {
        const auto r = JE::assessRequiredBehavior(withSystem(coreFacts(JE::RotationalCondition::Released), 3, 1));   // trelica
        QCOMPARE(r.items[0].systemConsistency, JE::SystemConsistency::Undetermined);
        bool gap = false;
        for (const auto &g : r.items[0].gaps) {
            gap = gap || QString::fromStdString(g).contains("não é porticado");
        }
        QVERIFY(gap);
    }

    void bracingAloneWithoutSystemIsStillReportedAsIncompleteAtNodeLevel()
    {
        const auto r = JE::assessRequiredBehavior(withSystem(coreFacts(JE::RotationalCondition::Released), 0, 1));
        QVERIFY(QString::fromStdString(r.nodeGaps.front()).contains("NÃO declarados"));
        QCOMPARE(r.items[0].systemConsistency, JE::SystemConsistency::Consistent);   // so o contraventamento e usado pela regra
    }

    void serviceForwardsTheProjectDeclarations()
    {
        AS::NodeBehaviorService::Options options;
        options.structuralSystem = 1;
        options.bracing = 2;
        const auto r = AS::NodeBehaviorService::assess(frame(1e-4, true), 2, kForceToKn, options);   // viga rotulada em estrutura nao contraventada
        QCOMPARE(r.structuralSystem, 1);
        QCOMPARE(r.bracing, 2);
        QCOMPARE(r.items[0].systemConsistency, JE::SystemConsistency::NeedsAttention);
    }

    void declarationsAreProjectSettingsAndNeverDefaulted()
    {
        Structura::App::ProjectData::DesignSettings ds;
        QCOMPARE(ds.structuralSystem, 0);   // padrao: NAO declarado
        QCOMPARE(ds.bracingCondition, 0);
        QVERIFY(ds.isDefault());
        ds.bracingCondition = 1;
        QVERIFY2(!ds.isDefault(), "uma declaracao isolada precisa ser gravada com o projeto");
        ds.bracingCondition = 0;
        ds.structuralSystem = 2;
        QVERIFY(!ds.isDefault());
    }

    void statusLabelsAreForTheUserNotEnumNames()
    {
        QCOMPARE(QString::fromStdString(JE::statusLabelPt(JE::ValidityStatus::ValidWithCaveats)), QString("determinado, com ressalvas"));
        QCOMPARE(QString::fromStdString(JE::statusLabelPt(JE::ValidityStatus::InsufficientData)), QString("dados insuficientes"));
        const auto r = JE::assessRequiredBehavior(coreFacts(JE::RotationalCondition::Released));
        QVERIFY(!QString::fromStdString(r.memory()).contains("ValidWithCaveats"));
    }

    // ================================================================================================ Incremento 2: capacidades por tipologia
    void typologyListIsTheSameAsTheConfiguratorAndNothingIsFiltered()
    {
        const auto rows = AS::TypologyCapabilityService::list();
        QCOMPARE(rows.size(), std::size_t(8) + std::size_t(AS::ConnectionCatalog::entries().size()));   // 8 tipos legados + catalogo
        for (const auto &r : rows) {
            QVERIFY2(r.configurable, r.name.toUtf8().constData());
            QVERIFY2(r.normativeDesign, r.name.toUtf8().constData());   // todas tem verificacoes de resistencia
        }
    }

    void fourCapabilitiesAreIndependentAndMomentRotationOnlyWhereTheMechanicalModelExists()
    {
        const auto rows = AS::TypologyCapabilityService::list();
        int withCurve = 0, configurableWithoutModel = 0;
        for (const auto &r : rows) {
            if (r.momentRotation) {
                ++withCurve;
                QCOMPARE(r.mechanicalModelId, std::string("EnComponentMethod"));
                QVERIFY(JE::allowsNumbers(r.mechanical));
            } else if (r.mechanicalModelId == "NbrResistanceOnly") {
                // Incremento 6: perfis T (143) = SO resistencia (nivel C, NBR); nao ha curva M-theta e a lacuna de formulacao e explicada
                QCOMPARE(r.type, 143);
                QVERIFY(!r.justification.empty() && !r.missingFoundations.empty());
                ++configurableWithoutModel;
            } else {
                QCOMPARE(r.mechanical, JE::ValidityStatus::NotYetAvailable);
                QVERIFY2(!r.justification.empty() && !r.missingFoundations.empty(), r.name.toUtf8().constData());   // a indisponibilidade e explicada
                QVERIFY(r.mechanicalModelId.empty());
                ++configurableWithoutModel;
            }
        }
        QVERIFY(withCurve >= 1);
        QVERIFY(configurableWithoutModel > withCurve);   // a maioria e configuravel e dimensionavel, mas SEM curva M-theta
    }

    void typologiesWithAUsableMechanicalModelAreExactlyTheMomentRotationRows()
    {
        // lista objetiva: quais tipologias (legadas e do catalogo) tem modelo mecanico utilizavel (curva M-theta) — mostrada no log do teste
        QStringList names;
        int legacy = 0, catalog = 0;
        for (const auto &r : AS::TypologyCapabilityService::list()) {
            if (r.momentRotation) {
                names << QString("%1 [%2] %3").arg(r.type).arg(r.type >= 100 ? "catalogo" : "legado").arg(r.name);
                (r.type >= 100 ? catalog : legacy)++;
            }
        }
        qInfo().noquote() << "tipologias com curva M-theta:" << names.join(" | ");
        QCOMPARE(legacy, 1);     // chapa de extremidade rigida (tipo legado kEndPlate)
        QCOMPARE(catalog, 5);    // catalogo: 135 (emenda de viga com chapas de extremidade, Incremento 5), 144 (mesas soldadas), 145 e 146 (mesas soldadas com tala de alma); 143 (perfis T) NAO tem modelo
        QVERIFY(names.join(" ").contains("135 ["));
        QVERIFY(names.join(" ").contains("147 ["));   // Incremento 6: viga-pilar flangeada fora do no (composicao em serie)
        QVERIFY(!names.join(" ").contains("143 ["));
        QCOMPARE(legacy + catalog, int(names.size()));
    }

    void unknownTypologyIsNotConfigurable()
    {
        const auto r = AS::TypologyCapabilityService::row(9999);
        QVERIFY(!r.configurable);
        QCOMPARE(r.mechanical, JE::ValidityStatus::Unsupported);
    }

    void definedConnectionsAreDescribedByTheMotorWithoutCopyingGeometry()
    {
        using NC = Structura::App::ProjectData::NodeConnection;
        NC c;
        c.id = 1;
        c.nodeId = 2;
        c.memberId = 2;
        c.type = NC::kEndPlate;
        c.values.insert(QStringLiteral("tp"), 19.0);
        NC other = c;
        other.id = 2;
        other.nodeId = 3;   // outro no: nao entra
        QVector<NC> all = {c, other};
        const auto d = AS::TypologyCapabilityService::describeAtNode(all, 2, {1, 2}, 200000.0);
        QCOMPARE(d.size(), std::size_t(1));
        QCOMPARE(d[0].memberId, 2);
        QVERIFY(d[0].momentRotation);   // chapa de extremidade rigida: modelo EN disponivel
        QCOMPARE(d[0].topologyStatus, std::string("Partial"));   // lacunas declaradas, nada inferido
        QVERIFY(d[0].topologyGaps > 0);
        QCOMPARE(d[0].fingerprint.size(), std::size_t(12));
        all[0].values[QStringLiteral("tp")] = 20.0;
        QVERIFY(AS::TypologyCapabilityService::describeAtNode(all, 2, {1, 2}, 200000.0)[0].fingerprint != d[0].fingerprint);   // alterar a ligacao muda o fingerprint
        NC shear = c;
        shear.type = NC::kShearTab;
        const auto s = AS::TypologyCapabilityService::describeAtNode({shear}, 2, {1, 2}, 200000.0);
        QVERIFY(!s[0].momentRotation);   // tipologia sem modelo mecanico nao apresenta curva
    }

    // ================================================================================================ dialogo: definir ligacao
    void dialogListsEveryTypologyRegardlessOfTheRequiredBehavior()
    {
        NodeBehaviorContextHelper::run(this, true);
        NodeBehaviorContextHelper::run(this, false);
    }

    // ================================================================================================ (C) dialogo
    void dialogShowsTheAssessment()
    {
        const auto r = AS::NodeBehaviorService::assess(frame(1e-4, true), 2, kForceToKn, {});
        Structura::UI::NodeBehaviorDialog dialog(r);
        QCOMPARE(dialog.resultRowCount(), 1);
        QCOMPARE(dialog.resultCell(0, 2), QString("Rotulado"));
        QVERIFY(dialog.resultCell(0, 3).contains("liberação"));
        QVERIFY(dialog.memoryText().contains("COMPORTAMENTO ESTRUTURAL REQUERIDO DO NÓ 2"));
        QVERIFY(dialog.memoryText().contains("NÃO classifica uma ligação real"));
        QVERIFY(dialog.summaryText().contains("ANTES de definir a ligação"));
    }
};

namespace NodeBehaviorContextHelper {
void run(QObject *, bool pinned)
{
    const auto r = AS::NodeBehaviorService::assess(frame(1e-4, pinned), 2, kForceToKn, {});
    Structura::UI::NodeBehaviorDialog::Context context;
    context.typologies = AS::TypologyCapabilityService::list();
    Structura::UI::NodeBehaviorDialog dialog(r, context);
    QVERIFY(dialog.typologyRowCount() == static_cast<int>(context.typologies.size()));   // o requerido NAO filtra as tipologias
    bool endPlate = false, shearTab = false;
    for (int row = 0; row < dialog.typologyRowCount(); ++row) {
        endPlate = endPlate || dialog.typologyCell(row, 0).contains("Viga–pilar rígida") || dialog.typologyCell(row, 1).contains("rígida");
        shearTab = shearTab || dialog.typologyCell(row, 1).contains("Viga–apoio");
    }
    QVERIFY(endPlate && shearTab);   // tanto tipologias rigidas quanto de cisalhamento aparecem, rotulado ou rigido
    // colunas das quatro capacidades preenchidas
    for (int row = 0; row < dialog.typologyRowCount(); ++row) {
        QVERIFY(dialog.typologyCell(row, 2) == "Sim");
        QVERIFY(!dialog.typologyCell(row, 3).isEmpty() && !dialog.typologyCell(row, 4).isEmpty() && !dialog.typologyCell(row, 5).isEmpty());
    }
    // pedido de definicao: nao cria ligacao, so devolve o par e a tipologia ao configurador existente
    QVERIFY(!dialog.request().valid);
    const int endPlateType = Structura::App::ProjectData::NodeConnection::kEndPlate;
    QVERIFY(dialog.requestDefinition(0, endPlateType));
    QVERIFY(dialog.request().valid);
    QCOMPARE(dialog.request().nodeId, 2);
    QCOMPARE(dialog.request().beamId, 2);
    QCOMPARE(dialog.request().otherId, 1);
    QCOMPARE(dialog.request().type, endPlateType);
    Structura::UI::NodeBehaviorDialog again(r, context);
    QVERIFY(!again.requestDefinition(0, 123456));   // tipologia inexistente
    QVERIFY(!again.request().valid);
}
} // namespace NodeBehaviorContextHelper

QTEST_MAIN(TestRequiredBehavior)
#include "TestRequiredBehavior.moc"
