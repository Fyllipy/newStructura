// P1 (F-SOL/F-GUS): os wrappers de resistencia devem reproduzir EXATAMENTE (igualdade de bits) as verificacoes legadas SupportWeld, checkGusset e
// checkTubularJoint, e preservar a condicao de fundamentacao (nenhuma verificacao elegivel a aprovacao: nenhuma foi confrontada com o texto da norma).
// Nao e validacao normativa nem fisica.
#include "design/jointengine/ResistanceWrappers.h"

#include <QtTest/QtTest>

#include <cmath>
#include <cstring>

namespace C = Structura::Design::Connections;
namespace JE = Structura::Design::JointEngine;

namespace {

bool sameBits(double a, double b)
{
    return std::memcmp(&a, &b, sizeof(double)) == 0;
}

void expectSame(const C::Check &legacy, const JE::WrappedCheck &w)
{
    QCOMPARE(w.name, legacy.name);
    QCOMPARE(w.clause, legacy.clause);
    QCOMPARE(w.unit, legacy.unit);
    QVERIFY(sameBits(w.demand, legacy.demand));
    QVERIFY(sameBits(w.resistance, legacy.resistance));
    QVERIFY(sameBits(w.ratio, legacy.ratio()));
    QCOMPARE(w.status, legacy.status);
    QCOMPARE(w.reason, legacy.reason);
    QCOMPARE(w.lines, legacy.lines);
}

std::vector<C::SupportWeldInput> weldInputs()
{
    std::vector<C::SupportWeldInput> v;
    C::SupportWeldInput a;
    a.shear = 120000.0;
    a.moment = 60.0e6;
    a.axial = 20000.0;
    v.push_back(a);
    C::SupportWeldInput b = a;   // mesas em filete
    b.flangeFullPenetration = false;
    b.flangeLeg = 8.0;
    b.supportThickness = 12.0;
    v.push_back(b);
    C::SupportWeldInput c = b;
    c.moment = 0.0;
    c.axial = -50000.0;
    c.webLeg = 5.0;
    v.push_back(c);
    return v;
}

std::vector<C::GussetInput> gussetInputs()
{
    std::vector<C::GussetInput> v;
    C::GussetInput in;
    in.grade = C::boltGrades()[2];
    in.db = 20.0;
    in.rows = 3;
    in.lines = 1;
    in.pitch = 70.0;
    in.bars = 2;
    in.barArea = 770.0;
    in.barLegThickness = 6.35;
    in.barEccentricity = 18.0;
    in.bar = {250.0, 400.0};
    in.gusset = {250.0, 400.0};
    in.support = {250.0, 400.0};
    in.gussetThickness = 9.5;
    in.gussetWidth = 200.0;
    in.tension = 120000.0;
    in.compression = 90000.0;
    v.push_back(in);
    C::GussetInput two = in;
    two.lines = 2;
    two.gauge = 60.0;
    two.rows = 4;
    two.freeLength = 120.0;
    two.compression = 0.0;
    v.push_back(two);
    C::GussetInput comp = in;
    comp.tension = 0.0;
    comp.freeLength = 150.0;
    comp.gussetWidth = 120.0;   // largura de Whitmore limitada pela chapa
    v.push_back(comp);
    return v;
}

std::vector<C::TubularJointInput> tubularInputs()
{
    std::vector<C::TubularJointInput> v;
    C::TubularJointInput t;
    t.kind = C::TubularJointInput::TY;
    t.d0 = 168.3; t.t0 = 6.0; t.fy0 = 355; t.fu0 = 510;
    t.d1 = 88.9; t.t1 = 4.0; t.fy1 = 355; t.theta1 = 90.0; t.n1 = 50000.0;
    v.push_back(t);
    C::TubularJointInput k;
    k.kind = C::TubularJointInput::KGap;
    k.d0 = 219.1; k.t0 = 7.1; k.fy0 = 355; k.fu0 = 510;
    k.d1 = 139.7; k.t1 = 4.5; k.theta1 = 38.7; k.n1 = 200000.0;
    k.d2 = 88.9; k.t2 = 3.6; k.theta2 = 50.8; k.n2 = 150000.0;
    k.gap = 21.3;
    v.push_back(k);
    C::TubularJointInput o = k;
    o.kind = C::TubularJointInput::KOverlap;
    o.overlap = 50.0;
    v.push_back(o);
    return v;
}

} // namespace

class TestJointEngineResistanceWrappers : public QObject
{
    Q_OBJECT
private slots:
    void descriptorsAreCompleteAndResistanceOnly()
    {
        for (const JE::ModelDescriptor *d : {&JE::WeldSupportResistance::descriptor(), &JE::GussetResistance::descriptor(), &JE::TubularResistance::descriptor()}) {
            QVERIFY2(JE::validateDescriptor(*d).empty(), d->id.c_str());
            QVERIFY(!d->nonlinear && !d->hasInternalState);
            QVERIFY(!d->limitations.empty());
        }
    }

    void weldWrapperEqualsLegacyBitForBit()
    {
        int n = 0;
        for (const auto &in : weldInputs()) {
            const auto legacy = C::checkSupportWeld(in);
            const auto r = JE::WeldSupportResistance::evaluate(in);
            QCOMPARE(r.checks.size(), legacy.size());
            QVERIFY(!legacy.empty());
            for (std::size_t i = 0; i < legacy.size(); ++i) {
                expectSame(legacy[i], r.checks[i]);
            }
            ++n;
        }
        QCOMPARE(n, 3);
    }

    void gussetWrapperEqualsLegacyBitForBit()
    {
        for (const auto &in : gussetInputs()) {
            const auto legacy = C::checkGusset(in);
            const auto r = JE::GussetResistance::evaluate(in);
            QCOMPARE(r.checks.size(), legacy.size());
            QVERIFY(!legacy.empty());
            for (std::size_t i = 0; i < legacy.size(); ++i) {
                expectSame(legacy[i], r.checks[i]);
            }
            QVERIFY(sameBits(r.parameters.at("whitmoreWidth").value, C::whitmoreWidth(in)));
            QVERIFY(sameBits(r.parameters.at("bucklingLength").value, in.freeLength));
            QVERIFY(sameBits(r.parameters.at("widthLimit").value, in.gussetWidth));
        }
    }

    void tubularWrapperIsASubsetOfLegacyWithExclusionsListed()
    {
        for (const auto &in : tubularInputs()) {
            const auto legacy = C::checkTubularJoint(in);
            const auto r = JE::TubularResistance::evaluate(in);
            QCOMPARE(r.checks.size() + r.excluded.size(), legacy.size());   // nada some em silencio
            std::size_t j = 0;
            for (const auto &w : r.checks) {   // mesma ordem, sem alteracao
                while (j < legacy.size() && legacy[j].name != w.name) {
                    ++j;
                }
                QVERIFY(j < legacy.size());
                expectSame(legacy[j], w);
                ++j;
            }
            for (const auto &w : r.checks) {
                QVERIFY2(w.clause.rfind("CIDECT eq. ", 0) == 0 || w.clause.find("(validade)") != std::string::npos || w.clause == "CIDECT 1.2.1", w.clause.c_str());
            }
        }
        // o caso K com sobreposicao tem verificacoes sem referencia unica: devem aparecer como excluidas com motivo
        const auto r = JE::TubularResistance::evaluate(tubularInputs()[2]);
        QVERIFY(!r.excluded.empty());
    }

    void eligibilityRequiresResistanceDemandConditionsAndNbrBasis()
    {
        // solda: resistencia confrontada com a NBR, mas demanda idealizada e condicoes nao verificadas => NENHUMA elegivel
        const auto weld = JE::WeldSupportResistance::evaluate(weldInputs()[1]);
        int resistanceConfronted = 0;
        for (const auto &c : weld.checks) {
            QVERIFY2(!c.approvalEligible, c.name.c_str());
            QVERIFY2(!c.confrontedWithOwnSource, c.name.c_str());
            QCOMPARE(c.basis, JE::StandardBasis::Nbr8800_2024);
            QVERIFY(!c.eligibilityReason.empty() && !c.demandNote.empty() && !c.conditionsNote.empty());
            const bool leg = c.name.find("perna") != std::string::npos;
            QCOMPARE(c.demandOrigin, leg ? JE::DemandOrigin::ProvidedInput : JE::DemandOrigin::IdealizedByLegacy);
            if (c.provenance == JE::FormulationProvenance::ConfrontedWithStandard) {
                ++resistanceConfronted;   // formula conferida, mas a verificacao inteira NAO e elegivel
                if (leg) {
                    QVERIFY(c.eligibilityReason.find("condicoes") != std::string::npos);   // nota a da Tabela 11 (um passe) nao verificavel
                } else {
                    QVERIFY(c.eligibilityReason.find("demanda idealizada") != std::string::npos);
                }
            }
        }
        QVERIFY(resistanceConfronted >= 6);   // todas as verificacoes de solda tem a resistencia/limite confrontados com a NBR
        // gusset: Whitmore/pseudo-coluna e metodo complementar (AISC), nunca conformidade NBR
        const auto gusset = JE::GussetResistance::evaluate(gussetInputs()[0]);
        int whitmore = 0;
        for (const auto &c : gusset.checks) {
            QVERIFY2(!c.approvalEligible, c.name.c_str());
            if (c.name.find("Whitmore") != std::string::npos) {
                QCOMPARE(c.basis, JE::StandardBasis::ComplementaryAisc);
                ++whitmore;
            }
        }
        QVERIFY(whitmore >= 2);
        // tubular CHS: confrontado com a PROPRIA fonte (CIDECT DG1, Tabela 4.1), mas NAO e conformidade NBR => nunca approvalEligible
        const auto kgap = JE::TubularResistance::evaluate(tubularInputs()[1]);
        int own = 0;
        for (const auto &c : kgap.checks) {
            QVERIFY2(!c.approvalEligible, c.name.c_str());
            QCOMPARE(c.basis, JE::StandardBasis::ComplementaryCidect);
            const bool tab41 = c.clause == "CIDECT eq. 4.1" || c.clause == "CIDECT eq. 4.2" || c.clause == "CIDECT 4.1 (validade)";
            QCOMPARE(c.confrontedWithOwnSource, tab41);
            own += c.confrontedWithOwnSource ? 1 : 0;
        }
        QVERIFY(own >= 4);
        // fy0 > 355: nem a propria fonte foi confrontada (nota (**) nao lida)
        auto high = tubularInputs()[1];
        high.fy0 = 420.0;
        high.fy1 = 420.0;
        high.fu0 = 520.0;
        for (const auto &c : JE::TubularResistance::evaluate(high).checks) {
            QVERIFY2(!c.confrontedWithOwnSource, c.name.c_str());
        }
        // fora da faixa de validade (gap < t1 + t2): condicoes nao verificadas
        auto bad = tubularInputs()[1];
        bad.gap = 5.0;
        for (const auto &c : JE::TubularResistance::evaluate(bad).checks) {
            QVERIFY2(!c.confrontedWithOwnSource, c.name.c_str());
            QVERIFY2(!c.conditionsVerified, c.name.c_str());
        }
        QVERIFY(!weld.sourcesRead.empty() && !kgap.sourcesRead.empty());
    }

    void weldWrapperRecordsGeometricAndEffectiveLegSeparately()
    {
        auto in = weldInputs()[1];
        in.flangeWidth = 101.0;
        in.webThickness = 5.0;
        in.flangeLeg = 12.5;   // trecho interno de 48 mm < 4 x 12,5
        const auto r = JE::WeldSupportResistance::evaluate(in);
        QVERIFY(r.parameters.count("geometricLeg.flange") && r.parameters.count("effectiveLeg.flange") && r.parameters.count("shortestRun.flange"));
        QCOMPARE(r.parameters.at("geometricLeg.flange").value, 12.5);
        QCOMPARE(r.parameters.at("effectiveLeg.flange").value, 12.0);
        QCOMPARE(r.parameters.at("shortestRun.flange").value, 48.0);
        bool lengthCheck = false;
        for (const auto &c : r.checks) {
            if (c.clause == "6.2.6.2.3") {
                lengthCheck = true;
                QCOMPARE(c.provenance, JE::FormulationProvenance::ConfrontedWithStandard);
                QCOMPARE(c.demandOrigin, JE::DemandOrigin::IdealizedByLegacy);   // comprimentos de perna plena: idealizacao
                QVERIFY(!c.approvalEligible);
            }
        }
        QVERIFY(lengthCheck);
        // equivalencia com o legado preservada tambem com a perna efetiva
        const auto legacy = C::checkSupportWeld(in);
        QCOMPARE(r.checks.size(), legacy.size());
        for (std::size_t i = 0; i < legacy.size(); ++i) {
            expectSame(legacy[i], r.checks[i]);
        }
    }

    void basisAndDemandNames()
    {
        QCOMPARE(JE::basisName(JE::StandardBasis::ComplementaryCidect), std::string("ComplementaryCidect"));
        QCOMPARE(JE::demandOriginName(JE::DemandOrigin::IdealizedByLegacy), std::string("IdealizedByLegacy"));
    }

    void whitmoreParametersAreRecordedNotChanged()
    {
        const auto in = gussetInputs()[0];
        const auto r = JE::GussetResistance::evaluate(in);
        QVERIFY(r.parameters.count("K"));
        const auto &k = r.parameters.at("K");
        QCOMPARE(k.value, 0.65);                                   // valor do legado, preservado
        QCOMPARE(*k.origin, JE::Origin::ModelAssumption);          // origem: hipotese, nao norma
        QVERIFY(k.source.find("NAO ajustado") != std::string::npos);
        // sem compressao nao ha K a registrar (nada e inventado)
        const auto two = JE::GussetResistance::evaluate(gussetInputs()[1]);
        QVERIFY(!two.parameters.count("K"));
        QVERIFY(!two.limitations.empty());
    }

    void partiallyDocumentedChecksStatePreciselyWhatWasAndWasNotChecked()
    {
        const auto r = JE::GussetResistance::evaluate(gussetInputs()[0]);
        bool found = false;
        for (const auto &c : r.checks) {
            if (c.provenance == JE::FormulationProvenance::PartiallyDocumented && c.name.find("compressao") != std::string::npos) {
                found = true;
                QVERIFY(c.provenanceNote.find("Thornton (1984), p. 147") != std::string::npos);
                QVERIFY(c.provenanceNote.find("K = 0,50") != std::string::npos);
                QVERIFY(c.provenanceNote.find("DEPENDE das condicoes de vinculo") != std::string::npos);
                QVERIFY(c.provenanceNote.find("0,5 a 1,2") != std::string::npos);
            }
        }
        QVERIFY(found);
        QVERIFY(!r.sourcesRead.empty());
    }

    void wrappersAreDeterministic()
    {
        const auto a = JE::GussetResistance::evaluate(gussetInputs()[0]);
        const auto b = JE::GussetResistance::evaluate(gussetInputs()[0]);
        QCOMPARE(a.checks.size(), b.checks.size());
        for (std::size_t i = 0; i < a.checks.size(); ++i) {
            QVERIFY(sameBits(a.checks[i].resistance, b.checks[i].resistance));
        }
    }

    void provenanceNames()
    {
        QCOMPARE(JE::provenanceName(JE::FormulationProvenance::ReproducedFromCode), std::string("ReproducedFromCode"));
        QVERIFY(JE::approvalEligible(JE::FormulationProvenance::ConfrontedWithStandard));
        QVERIFY(JE::approvalEligible(JE::FormulationProvenance::DomainVerified));
        QVERIFY(!JE::approvalEligible(JE::FormulationProvenance::PartiallyDocumented));
        QVERIFY(!JE::approvalEligible(JE::FormulationProvenance::ReproducedFromCode));
        QVERIFY(!JE::approvalEligible(JE::FormulationProvenance::Unavailable));
    }
};

QTEST_APPLESS_MAIN(TestJointEngineResistanceWrappers)
#include "TestJointEngineResistanceWrappers.moc"
