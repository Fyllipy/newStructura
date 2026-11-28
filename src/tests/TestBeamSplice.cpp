// Incremento 5, Frente B: modelo mecânico da EMENDA DE VIGA com duas chapas de extremidade parafusadas (catálogo 135), pelo método dos componentes da EN 1993-1-8:2005
// (Tabela 6.10 e 6.3.3.1(5) para a rigidez; 6.2.7.2(10) para a resistência). Comparação independente: os valores esperados vêm de uma implementação INDEPENDENTE em Python
// (somente biblioteca padrão; docs/validacao_emenda/splice_independent.py) a partir das equações da EN. É REPRODUÇÃO DE EQUAÇÕES e verificação numérica;
// NÃO é validação experimental nem profissional (não há ensaio nem MEF desta emenda no projeto).
#include "app/services/ConnectionCatalog.h"
#include "app/services/ConnectionMechanicalResponseService.h"
#include "app/services/ConnectionRequirementService.h"
#include "app/services/ConnectionService.h"
#include "app/services/JointComponentService.h"
#include "app/services/TypologyCapabilityService.h"

#include <QtTest/QtTest>

#include <cmath>
#include <cstring>

namespace AS = Structura::App::Services;
namespace JE = Structura::Design::JointEngine;
namespace JS = Structura::Design::JointClass;
using NC = Structura::App::ProjectData::NodeConnection;
using Dir = JS::Direction;

namespace {

constexpr double kE = 200000.0;

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

NC splice(int tRows, int sRows)
{
    NC c = AS::ConnectionCatalog::makeDefault(135, beam(), 250.0, 400.0);
    c.id = 1;
    c.nodeId = 4;
    c.memberId = 5;
    c.values["tRows"] = tRows;
    c.values["sRows"] = sRows;
    return c;
}

NC outside(int tRows, int sRows)
{
    NC c = AS::ConnectionCatalog::makeDefault(147, beam(), 250.0, 400.0);
    c.id = 1;
    c.nodeId = 4;
    c.memberId = 5;
    c.values["tRows"] = tRows;
    c.values["sRows"] = sRows;
    c.values["supD"] = 260;
    c.values["supBf"] = 146;
    c.values["supTf"] = 12.7;
    c.values["supTw"] = 7.6;
    c.values["supR"] = 10;
    c.values["supFy"] = 250;
    c.values["supFu"] = 400;
    c.values["supConfirmed"] = 1;
    return c;
}

bool sameBits(double a, double b) { return std::memcmp(&a, &b, sizeof(double)) == 0; }

bool near(double a, double b, double rel) { return std::abs(a - b) <= rel * std::max(std::abs(b), 1e-300); }

struct Row {
    double h, keff, k5, k10, individual, effective;
};
struct Reference {
    int rows;
    double sini, mj, z;
    std::vector<Row> detail;
};

// Valores de splice_independent.py (Python, so biblioteca padrao). Unidades: kN.m/rad, kN.m, mm, mm, kN.
std::vector<Reference> pythonReference()
{
    return {{1, 87832.4726, 45.90029, 334.6500, {{334.6500, 3.921421, 24.365984, 5.782755, 137.15909, 137.15909}}},
            {2, 99961.8416, 84.05505, 341.8251, {{404.6500, 0.370381, 0.791455, 5.782755, 94.29077, 94.29077}, {334.6500, 3.921421, 24.365984, 5.782755, 137.15909, 137.15909}}},
            {3, 103662.7514, 110.68020, 345.2746, {{474.6500, 0.082136, 0.166638, 5.782755, 56.09428, 56.09428}, {404.6500, 0.370381, 0.791455, 5.782755, 94.29077, 94.29077}, {334.6500, 3.921421, 24.365984, 5.782755, 137.15909, 137.15909}}}};
}

} // namespace

class TestBeamSplice : public QObject
{
    Q_OBJECT
private slots:
    void matchesTheIndependentPythonReproductionOfTheEnEquations()
    {
        for (const Reference &ref : pythonReference()) {
            const auto r = AS::ConnectionMechanicalResponseService::evaluate(splice(ref.rows, 1), kE, 40, Dir::Positive);
            QVERIFY2(r.available, r.message.c_str());
            QVERIFY2(near(r.initialStiffnessKnm, ref.sini, 2e-8), "Sj,ini diferente do Python independente");
            QVERIFY2(near(r.resistanceKnm, ref.mj, 2e-7), "Mj,Rd diferente do Python independente");
            QVERIFY2(near(r.leverArmMm, ref.z, 2e-7), "zeq diferente do Python independente");
            QCOMPARE(r.rows.size(), ref.detail.size());
            for (std::size_t i = 0; i < ref.detail.size(); ++i) {
                QVERIFY(near(r.rows[i].leverMm, ref.detail[i].h, 1e-7));
                QVERIFY(near(r.rows[i].keffMm, ref.detail[i].keff, 2e-5));   // referencia impressa com 6 casas decimais
                QVERIFY(near(r.rows[i].individualKn, ref.detail[i].individual, 2e-7));
                QVERIFY(near(r.rows[i].effectiveKn, ref.detail[i].effective, 2e-7));
            }
            // k5 e k10 por componente (cada fileira: k5 da chapa 1, k5 da chapa 2 e k10)
            int k5 = 0, k10 = 0;
            for (const auto &c : r.components) {
                k5 += c.symbol == "k5" ? 1 : 0;
                k10 += c.symbol == "k10" ? 1 : 0;
                QVERIFY2(c.symbol == "k5" || c.symbol == "k10", "emenda: nao ha k1 a k4 (sem pilar)");
            }
            QCOMPARE(k5, 2 * ref.rows);
            QCOMPARE(k10, ref.rows);
            for (std::size_t i = 0; i < ref.detail.size(); ++i) {   // k5 e k10 por fileira
                for (const auto &c : r.components) {
                    if (c.row == r.rows[i].row && c.symbol == "k5") {
                        QVERIFY(near(c.stiffnessMm, ref.detail[i].k5, 2e-5));
                    }
                    if (c.row == r.rows[i].row && c.symbol == "k10") {
                        QVERIFY(near(c.stiffnessMm, ref.detail[i].k10, 2e-5));
                    }
                }
            }
        }
    }

    void theSpliceHasItsOwnCurveWithTheStandardShape()
    {
        const auto r = AS::ConnectionMechanicalResponseService::evaluate(splice(2, 1), kE, 40, Dir::Positive);
        QVERIFY(r.available);
        QVERIFY(r.response.consistent());
        QCOMPARE(r.response.kind, JE::ResponseKind::MomentRotation);
        QVERIFY(!r.response.deformationCapacity);   // capacidade de rotacao indisponivel (EN 6.4 nao implementada)
        QVERIFY(r.family.find("emenda") != std::string::npos);
        // eq. 6.28 recalculada: theta = M/S (M <= 2/3 Mj) e theta = M/S (1,5 M/Mj)^2,7 acima
        for (const auto &p : r.response.curves[0].points) {
            const double m = p.second;
            const double theta = m <= 2.0 / 3.0 * r.resistanceKnm ? m / r.initialStiffnessKnm : m / r.initialStiffnessKnm * std::pow(1.5 * m / r.resistanceKnm, 2.7);
            QVERIFY(near(p.first, theta, 1e-9));
        }
        QVERIFY(near(r.response.curves[0].points.back().second, r.resistanceKnm, 1e-9));
        // sem pilar: o conjunto de fileiras e toda a flexibilidade
        QVERIFY(near(r.rowsShareOfTotal, 1.0, 1e-12));
        QVERIFY(r.governingContribution.find("fileiras") != std::string::npos);
        bool noColumnHypothesis = false;
        for (const auto &h : r.hypotheses) {
            noColumnHypothesis = noColumnHypothesis || (h.find("Emenda de viga") != std::string::npos && h.find("não há pilar") != std::string::npos);
        }
        QVERIFY(noColumnHypothesis);
    }

    void bothDirectionsUseTheRowsThatExistOnEachSide()
    {
        const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(splice(3, 1), kE, 40);
        QVERIFY(b.positive.available && b.negative.available);
        QVERIFY(!b.symmetryDemonstrated);
        QCOMPARE(b.positive.rows.size(), std::size_t(3));
        QCOMPARE(b.negative.rows.size(), std::size_t(1));
        // o negativo (1 fileira abaixo) e igual ao positivo do cadastro com 1 fileira (Python independente)
        QVERIFY(near(b.negative.initialStiffnessKnm, 87832.4726, 2e-8));
        QVERIFY(near(b.negative.resistanceKnm, 45.90029, 2e-7));
        QVERIFY(b.positive.resistanceKnm > 2.0 * b.negative.resistanceKnm);
        // identidade de espelho
        const auto mirror = AS::ConnectionMechanicalResponseService::evaluateBoth(splice(1, 3), kE, 40);
        QVERIFY(sameBits(b.positive.initialStiffnessKnm, mirror.negative.initialStiffnessKnm));
        QVERIFY(sameBits(b.positive.resistanceKnm, mirror.negative.resistanceKnm));
        QVERIFY(sameBits(b.negative.initialStiffnessKnm, mirror.positive.initialStiffnessKnm));
        // simetria demonstrada quando as fileiras dos dois lados sao iguais
        const auto sym = AS::ConnectionMechanicalResponseService::evaluateBoth(splice(2, 2), kE, 40);
        QVERIFY(sym.symmetryDemonstrated);
        QVERIFY(sameBits(sym.positive.initialStiffnessKnm, sym.negative.initialStiffnessKnm));
        QVERIFY(sameBits(sym.positive.resistanceKnm, sym.negative.resistanceKnm));
        QVERIFY(sym.positive.analysisFingerprint != sym.negative.analysisFingerprint);
        QCOMPARE(sym.positive.configurationFingerprint, sym.negative.configurationFingerprint);
        QVERIFY(b.negative.hypotheses.back().find("sentido NEGATIVO") != std::string::npos || b.negative.participants.find("mesa inferior") != std::string::npos);
    }

    void physicalSensitivitiesAndSeriesBounds()
    {
        // k5 cresce com t^3: chapa mais espessa => rigidez inicial e resistencia nao menores
        double previousS = 0.0, previousM = 0.0;
        for (const double tp : {12.0, 16.0, 19.0, 25.0}) {
            NC c = splice(2, 1);
            c.values["plateT"] = tp;
            const auto r = AS::ConnectionMechanicalResponseService::evaluate(c, kE, 40, Dir::Positive);
            QVERIFY2(r.available, r.message.c_str());
            QVERIFY(r.initialStiffnessKnm > previousS);
            QVERIFY(r.resistanceKnm >= previousM - 1e-9);
            previousS = r.initialStiffnessKnm;
            previousM = r.resistanceKnm;
        }
        // E: Sj,ini proporcional a E (os componentes ki sao geometricos); Mj,Rd independente de E
        const auto a = AS::ConnectionMechanicalResponseService::evaluate(splice(2, 1), kE, 40, Dir::Positive);
        const auto b = AS::ConnectionMechanicalResponseService::evaluate(splice(2, 1), 2.0 * kE, 40, Dir::Positive);
        QVERIFY(near(b.initialStiffnessKnm, 2.0 * a.initialStiffnessKnm, 1e-12));
        QVERIFY(sameBits(a.resistanceKnm, b.resistanceKnm));
        // serie: keff,r < k5/2 e < k10 (a associacao em serie e sempre mais flexivel que qualquer parcela)
        for (const auto &row : a.rows) {
            for (const auto &c : a.components) {
                if (c.row == row.row) {
                    QVERIFY(row.keffMm < (c.symbol == "k5" ? c.stiffnessMm / 2.0 : c.stiffnessMm) + 1e-12);
                }
            }
        }
        // parafuso de maior diametro: k10 e Ft,Rd crescem
        NC bigBolt = splice(2, 1);
        bigBolt.values["db"] = 24.0;
        const auto big = AS::ConnectionMechanicalResponseService::evaluate(bigBolt, kE, 40, Dir::Positive);
        QVERIFY(big.available);
        QVERIFY(big.initialStiffnessKnm != a.initialStiffnessKnm);
    }

    void fingerprintsAndDeterminism()
    {
        const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(splice(2, 1), kE, 40);
        const auto again = AS::ConnectionMechanicalResponseService::evaluateBoth(splice(2, 1), kE, 40);
        QCOMPARE(b.positive.analysisFingerprint, again.positive.analysisFingerprint);
        QCOMPARE(b.negative.analysisFingerprint, again.negative.analysisFingerprint);
        QVERIFY(b.positive.analysisFingerprint != b.negative.analysisFingerprint);
        NC thicker = splice(2, 1);
        thicker.values["plateT"] = 25.0;
        const auto changed = AS::ConnectionMechanicalResponseService::evaluateBoth(thicker, kE, 40);
        QVERIFY(changed.positive.configurationFingerprint != b.positive.configurationFingerprint);
        QVERIFY(changed.positive.analysisFingerprint != b.positive.analysisFingerprint);
        // o fingerprint da emenda e distinto do de uma chapa de extremidade viga-pilar de mesmos valores
        NC endPlate = AS::ConnectionService::makeDefault(NC::kEndPlate, beam(), 250.0, 400.0);
        QVERIFY(endPlate.type != splice(2, 1).type);
    }

    void typologiesWithoutAModelRemainUnavailable()
    {
        for (const int type : {143, NC::kSplice}) {   // 143 = perfis T; kSplice = emenda legada (147 passou a ter modelo no Incremento 6)
            NC c = AS::ConnectionCatalog::makeDefault(type, beam(), 250.0, 400.0);
            c.nodeId = 4;
            c.memberId = 5;
            const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(c, kE, 40);
            QVERIFY2(!b.positive.available && !b.negative.available, std::to_string(type).c_str());
            QVERIFY(b.positive.response.curves.empty() && !b.positive.response.initialStiffness);
        }
    }

    void invalidGeometryGivesNoNumbers()
    {
        NC c = splice(2, 1);
        c.values["weldF"] = 60.0;   // 0,8 x perna > primeira fileira => m <= 0 na chapa
        const auto r = AS::ConnectionMechanicalResponseService::evaluate(c, kE, 40, Dir::Positive);
        QVERIFY(!r.available);
        QVERIFY(r.response.curves.empty() && !r.response.initialStiffness);
        QVERIFY(!r.message.empty());
        const auto none = AS::ConnectionMechanicalResponseService::evaluate(splice(2, 1), 0.0, 40, Dir::Positive);   // sem E: nada
        QVERIFY(!none.available);
        QCOMPARE(none.status, JE::ValidityStatus::InsufficientData);
    }

    void nbrLevelIsEvaluatedForTheSpliceItselfAndNeverChangesTheCurve()
    {
        const auto r = AS::ConnectionMechanicalResponseService::evaluate(splice(2, 1), kE, 40, Dir::Positive);
        QVERIFY(r.available);
        QVERIFY(r.nbrResistanceKnm > 0.0);   // verificacoes da NBR da propria emenda (as de pilar sao descartadas)
        QVERIFY(near(r.resistanceKnm, 84.05505, 2e-7));   // a curva continua a da EN (nivel A)
        QCOMPARE(r.nbrBelowCurve, r.nbrResistanceKnm < r.resistanceKnm * (1.0 - 1e-9));
    }

    void capabilityMatrixNowListsTheSplice()
    {
        const auto row = AS::TypologyCapabilityService::row(135);
        QVERIFY(row.momentRotation);
        QVERIFY(row.mechanical == JE::ValidityStatus::Valid || row.mechanical == JE::ValidityStatus::ValidWithCaveats);
        QVERIFY(AS::TypologyCapabilityService::row(147).momentRotation);   // Incremento 6: composicao em serie (no Incremento 5 ainda sem modelo)
        QVERIFY(!AS::TypologyCapabilityService::row(143).momentRotation);
    }

    void spliceIsABeamBeamJointSoTheNbrStiffnessClassificationIsNotApplicable()
    {
        const auto both = AS::ConnectionMechanicalResponseService::evaluateBoth(splice(2, 1), kE, 40);
        JE::RequiredBehaviorAssessment assessment;
        JE::RequiredBehaviorItem beamColumn;   // 5-7 (viga-pilar)
        beamColumn.beamId = 5;
        beamColumn.otherId = 7;
        beamColumn.kind = JS::JointKind::BeamColumn;
        JE::RequiredBehaviorItem beamBeam;     // 5-9 (viga-viga)
        beamBeam.beamId = 5;
        beamBeam.otherId = 9;
        beamBeam.kind = JS::JointKind::BeamBeam;
        beamBeam.basis = JE::RequirementBasis::NotApplicable;
        beamBeam.status = JE::ValidityStatus::OutOfDomain;
        assessment.items = {beamColumn, beamBeam};
        const auto d = AS::ConnectionRequirementService::evaluateBoth(splice(2, 1), assessment, both);
        QCOMPARE(d.positive.outcome, JE::ComparisonOutcome::NotApplicable);
        QCOMPARE(d.negative.outcome, JE::ComparisonOutcome::NotApplicable);
        QCOMPARE(d.overall, JE::ComparisonOutcome::NotApplicable);
        QCOMPARE(d.positive.otherId, 9);   // escolheu o par viga-viga, nao o viga-pilar
        QVERIFY(!d.positive.real.available);   // nenhuma classe de rigidez e inventada para a emenda
        // mas a resposta mecanica da emenda existe e e utilizavel
        QVERIFY(both.positive.available && both.negative.available);
    }

    // ------------------------------------------------------------------------------------------------------------- INCREMENTO 6: viga-pilar flangeada fora do no (147)
    void outsideJointMatchesTheIndependentPythonSeriesComposition()
    {
        struct Ref { int t, s; Dir dir; double sini, mj; };
        // splice_independent.py: parte soldada A (Sini 16863.5060, Mj 55.84295) em serie com a emenda B; Mj = min
        const Ref refs[] = {{1, 1, Dir::Positive, 14147.2810, 45.90029}, {2, 1, Dir::Positive, 14429.2925, 55.84295}, {2, 1, Dir::Negative, 14147.2810, 45.90029}};
        for (const Ref &ref : refs) {
            const auto r = AS::ConnectionMechanicalResponseService::evaluate(outside(ref.t, ref.s), kE, 40, ref.dir);
            QVERIFY2(r.available, r.message.c_str());
            QVERIFY2(near(r.initialStiffnessKnm, ref.sini, 2e-8), "Sj,ini da composicao diferente do Python independente");
            QVERIFY2(near(r.resistanceKnm, ref.mj, 2e-7), "Mj,Rd da composicao diferente do Python independente");
        }
        const auto detail = AS::JointComponentService::evaluate(outside(2, 1), kE, Dir::Positive);
        QCOMPARE(detail.parts.size(), std::size_t(2));
        QVERIFY(near(detail.parts[0].stiffnessKnm, 16863.5060, 2e-8) && near(detail.parts[0].resistanceKnm, 55.84295, 2e-7));   // parte A (soldada ao pilar)
        QVERIFY(near(detail.parts[1].stiffnessKnm, 99961.8416, 2e-8) && near(detail.parts[1].resistanceKnm, 84.05505, 2e-7));   // parte B (emenda, 2 fileiras)
        QVERIFY(detail.parts[0].label.find("soldadas") != std::string::npos && detail.parts[1].label.find("emenda") != std::string::npos);
    }

    void outsideJointComponentsSharesAndSeriesBounds()
    {
        const auto r = AS::ConnectionMechanicalResponseService::evaluate(outside(2, 1), kE, 40, Dir::Positive);
        QVERIFY(r.available);
        int k1 = 0, k2 = 0, k3 = 0, k5 = 0, k10 = 0;
        double nodeShares = 0.0;
        for (const auto &c : r.components) {
            k1 += c.symbol == "k1";
            k2 += c.symbol == "k2";
            k3 += c.symbol == "k3";
            k5 += c.symbol == "k5";
            k10 += c.symbol == "k10";
            if (c.row == 0) {
                nodeShares += c.shareOfTotal;
                QVERIFY(c.name.find("Parte soldada ao pilar") != std::string::npos);
            }
        }
        QVERIFY(k1 == 1 && k2 == 1 && k3 == 1);
        QCOMPARE(k5, 4);    // 2 fileiras x 2 chapas
        QCOMPARE(k10, 2);
        QVERIFY2(near(nodeShares + r.rowsShareOfTotal, 1.0, 1e-12), "as fracoes de ROTACAO das duas partes devem somar 1");
        QVERIFY(r.initialStiffnessKnm < 16863.5060 && r.initialStiffnessKnm < 99961.8416);   // serie: mais flexivel que qualquer parte
        QVERIFY(r.governingContribution.find("k1") != std::string::npos);                   // painel de alma do pilar domina a flexibilidade
        QVERIFY(r.family.find("fora do nó") != std::string::npos);
        bool series = false;
        for (const auto &h : r.hypotheses) {
            series = series || h.find("SÉRIE") != std::string::npos;
        }
        QVERIFY(series);
        QVERIFY(r.response.consistent());
        QVERIFY(!r.response.deformationCapacity);
    }

    void outsideJointResistanceIsTheMinimumOfTheTwoParts()
    {
        const auto weak = AS::ConnectionMechanicalResponseService::evaluate(outside(1, 1), kE, 40, Dir::Positive);   // a emenda (1 fileira) governa
        QVERIFY(weak.available);
        QVERIFY(near(weak.resistanceKnm, 45.90029, 2e-7));
        QVERIFY(weak.response.governingMechanism.find("emenda") != std::string::npos);
        const auto strong = AS::ConnectionMechanicalResponseService::evaluate(outside(2, 1), kE, 40, Dir::Positive);   // a parte soldada ao pilar governa
        QVERIFY(near(strong.resistanceKnm, 55.84295, 2e-7));
        QVERIFY(strong.response.governingMechanism.find("ligação soldada ao pilar") != std::string::npos);
        QVERIFY(strong.nbrResistanceKnm > 0.0);   // nivel C: verificacoes da NBR da emenda e da solda viga-apoio
    }

    void outsideJointDirectionsMirrorAndSymmetry()
    {
        const auto a = AS::ConnectionMechanicalResponseService::evaluateBoth(outside(2, 1), kE, 40);
        const auto m = AS::ConnectionMechanicalResponseService::evaluateBoth(outside(1, 2), kE, 40);
        QVERIFY(a.positive.available && a.negative.available);
        QVERIFY(!a.symmetryDemonstrated);
        QVERIFY(sameBits(a.positive.initialStiffnessKnm, m.negative.initialStiffnessKnm));
        QVERIFY(sameBits(a.positive.resistanceKnm, m.negative.resistanceKnm));
        QVERIFY(sameBits(a.negative.initialStiffnessKnm, m.positive.initialStiffnessKnm));
        QVERIFY(AS::ConnectionMechanicalResponseService::evaluateBoth(outside(2, 2), kE, 40).symmetryDemonstrated);
        QVERIFY(a.positive.analysisFingerprint != a.negative.analysisFingerprint);
        QCOMPARE(a.positive.configurationFingerprint, a.negative.configurationFingerprint);
    }

    void outsideJointNeedsTheColumnAndGivesNoNumbersWithout()
    {
        NC c = outside(2, 1);
        c.values["supConfirmed"] = 0.0;   // pilar digitado e nao confirmado, sem barra vinculada
        const auto r = AS::ConnectionMechanicalResponseService::evaluate(c, kE, 40, Dir::Positive);
        QVERIFY(!r.available);
        QVERIFY(r.response.curves.empty() && !r.response.initialStiffness);
        QVERIFY(!r.message.empty());
        NC thin = outside(2, 1);
        thin.values["weldF"] = 60.0;   // m <= 0 na chapa: emenda invalida
        QVERIFY(!AS::ConnectionMechanicalResponseService::evaluate(thin, kE, 40, Dir::Positive).available);
        QVERIFY(!AS::ConnectionMechanicalResponseService::evaluate(outside(2, 1), 0.0, 40, Dir::Positive).available);
    }

    void outsideJointIsBeamColumnSoItIsClassifiedAndComparedPerDirection()
    {
        NC c = outside(2, 1);
        c.values["supMember"] = 7.0;
        c.values["supType"] = 1.0;
        const auto both = AS::ConnectionMechanicalResponseService::evaluateBoth(c, kE, 40);
        QVERIFY(both.positive.available && both.negative.available);
        JE::RequiredBehaviorAssessment assessment;
        JE::RequiredBehaviorItem item;
        item.beamId = 5;
        item.otherId = 7;
        item.kind = JS::JointKind::BeamColumn;
        item.required = JE::RequiredKind::SemiRigid;
        item.basis = JE::RequirementBasis::ModelSpring;
        item.status = JE::ValidityStatus::ValidWithCaveats;
        item.systemConsistency = JE::SystemConsistency::Consistent;
        item.limitsAvailable = true;
        item.limits = JS::limitsFor(both.positive.initialStiffnessKnm / 10.0, 1.0, 1.0);   // eta = 10: semirrigida nos dois sentidos
        item.referenceKnm = item.limits.referenceStiffness;
        item.storey.status = JS::StoreyStatus::Ok;
        item.storey.ratio = 0.5;
        item.kvKp = 0.5;
        JE::RequiredBehaviorItem beamBeam = item;   // um par viga-viga da mesma viga NAO deve ser escolhido (a ligacao e viga-pilar)
        beamBeam.otherId = 9;
        beamBeam.kind = JS::JointKind::BeamBeam;
        beamBeam.basis = JE::RequirementBasis::NotApplicable;
        assessment.items = {beamBeam, item};
        const auto d = AS::ConnectionRequirementService::evaluateBoth(c, assessment, both);
        QCOMPARE(d.positive.otherId, 7);
        QVERIFY(d.positive.real.available && d.negative.real.available);
        QCOMPARE(d.positive.real.finalClass, JS::StiffnessClass::SemiRigid);
        QCOMPARE(d.positive.outcome, JE::ComparisonOutcome::Meets);
        QCOMPARE(d.overall, JE::ComparisonOutcome::Meets);
        QVERIFY(AS::TypologyCapabilityService::row(147).momentRotation);
    }

    // ------------------------------------------------------------------------------------------------------------- INCREMENTO 6: perfis T (143) = SO resistencia
    void tStubJointGivesOnlyTheNbrResistanceAndNothingElse()
    {
        NC c = AS::ConnectionCatalog::makeDefault(143, beam(), 250.0, 400.0);
        c.nodeId = 4;
        c.memberId = 5;
        c.values["supD"] = 260;
        c.values["supBf"] = 146;
        c.values["supTf"] = 12.7;
        c.values["supTw"] = 7.6;
        c.values["supR"] = 10;
        c.values["supFy"] = 250;
        c.values["supFu"] = 400;
        c.values["supConfirmed"] = 1;
        const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(c, kE, 40);
        for (const auto *r : {&b.positive, &b.negative}) {
            QVERIFY(r->resistanceOnly);
            QVERIFY(!r->available && !r->partial);
            QCOMPARE(r->response.kind, JE::ResponseKind::ResistanceOnly);
            QVERIFY(r->response.curves.empty() && !r->response.initialStiffness && !r->response.tangentStiffness && !r->response.secantStiffness);
            QCOMPARE(r->response.resistances.size(), std::size_t(1));
            QVERIFY(r->response.resistances[0].name.find("NBR") != std::string::npos);
            QVERIFY(r->resistanceKnm > 0.0 && r->nbrResistanceKnm > 0.0);
            QVERIFY(sameBits(r->resistanceKnm, r->nbrResistanceKnm));
            QVERIFY(r->components.empty() && r->levels.empty());     // nenhum componente nem nivel de momento inventado
            QVERIFY(r->response.consistent());
            bool stiffnessUnavailable = false;
            for (const auto &l : r->limitations) {
                stiffnessUnavailable = stiffnessUnavailable || l.find("Rigidez inicial Sj,ini INDISPONÍVEL") != std::string::npos;
            }
            QVERIFY(stiffnessUnavailable);
        }
        QVERIFY(b.symmetryDemonstrated);
        QVERIFY(sameBits(b.positive.resistanceKnm, b.negative.resistanceKnm));
        QVERIFY(b.positive.analysisFingerprint != b.negative.analysisFingerprint);
        // capacidade: configuravel, so resistencia, SEM curva M-theta
        const auto row = AS::TypologyCapabilityService::row(143);
        QVERIFY(!row.momentRotation);
        QCOMPARE(row.mechanicalModelId, std::string("NbrResistanceOnly"));
        // a classificacao de rigidez fica indeterminada (nao ha Sj,ini) e a comparacao nao inventa classe
        JE::RequiredBehaviorAssessment assessment;
        JE::RequiredBehaviorItem item;
        item.beamId = 5;
        item.otherId = 7;
        item.kind = JS::JointKind::BeamColumn;
        item.required = JE::RequiredKind::Rigid;
        item.basis = JE::RequirementBasis::ModelImplicitContinuity;
        item.status = JE::ValidityStatus::ValidWithCaveats;
        item.limitsAvailable = true;
        item.limits = JS::limitsFor(1000.0, 1.0, 1.0);
        item.storey.status = JS::StoreyStatus::Ok;
        item.storey.ratio = 0.5;
        assessment.items = {item};
        c.values["supMember"] = 7.0;
        const auto d = AS::ConnectionRequirementService::evaluateBoth(c, assessment, b);
        QCOMPARE(d.positive.outcome, JE::ComparisonOutcome::Indeterminate);
        QCOMPARE(d.overall, JE::ComparisonOutcome::Indeterminate);
        QVERIFY(!d.positive.real.available);
        QVERIFY(d.positive.summary.find("só a resistência") != std::string::npos);
    }

    void tStubJointWithoutColumnOrWithBadDataGivesNoNumbers()
    {
        NC c = AS::ConnectionCatalog::makeDefault(143, beam(), 250.0, 400.0);
        c.nodeId = 4;
        c.memberId = 5;   // pilar nao informado
        const auto r = AS::ConnectionMechanicalResponseService::evaluate(c, kE, 40, Dir::Positive);
        QVERIFY(!r.resistanceOnly && !r.available);
        QVERIFY(r.response.resistances.empty() && r.response.curves.empty());
        QVERIFY(!r.message.empty());
    }

    void weldedAndTStubFamiliesNeverFallBackToADefaultColumn()
    {
        for (const int type : {143, 144, 145, 146}) {
            NC c = AS::ConnectionCatalog::makeDefault(type, beam(), 250.0, 400.0);   // pilar NAO informado
            c.nodeId = 4;
            c.memberId = 5;
            const auto b = AS::ConnectionMechanicalResponseService::evaluateBoth(c, kE, 40);
            for (const auto *r : {&b.positive, &b.negative}) {
                QVERIFY2(!r->available && !r->resistanceOnly, std::to_string(type).c_str());
                QVERIFY(r->message.find("Nenhum pilar padrão") != std::string::npos);
                QVERIFY(r->response.curves.empty() && r->response.resistances.empty());
            }
            NC unconfirmed = c;   // geometria digitada e NAO confirmada
            unconfirmed.values["supD"] = 260;
            unconfirmed.values["supBf"] = 146;
            unconfirmed.values["supTf"] = 12.7;
            unconfirmed.values["supTw"] = 7.6;
            QVERIFY(!AS::ConnectionMechanicalResponseService::evaluate(unconfirmed, kE, 40, Dir::Positive).available);
            unconfirmed.values["supConfirmed"] = 1.0;   // confirmada: calcula
            const auto ok = AS::ConnectionMechanicalResponseService::evaluate(unconfirmed, kE, 40, Dir::Positive);
            QVERIFY2(ok.available || ok.resistanceOnly, std::to_string(type).c_str());
        }
    }
};

QTEST_MAIN(TestBeamSplice)
#include "TestBeamSplice.moc"
