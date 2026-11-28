// S-03.3: comprimento efetivo minimo de filete no SupportWeld (NBR 8800:2024, 6.2.6.2.3): "nao pode ser inferior a quatro vezes seu tamanho da perna
// e a 40 mm ou, entao, para efeito de calculo, a perna nao pode ser considerada maior que 25 % de seu comprimento". Interpretacao adotada: o limite de 40 mm
// nao tem alternativa de calculo; a alternativa de 25 % substitui apenas a relacao com 4 x perna. A regra dos filetes exclusivamente longitudinais em ligacoes
// extremas de chapas tracionadas nao se aplica a solda viga-apoio. Trechos: face externa da mesa (bf), duas faces internas ((bf - tw)/2), dois filetes da alma (d - 2 tf).
// Os resultados dos casos que atendem NAO mudam (nenhuma verificacao e acrescentada); so ha verificacao nova quando ha desvio ou quando a perna efetiva e acionada.
#include "design/SupportWeld.h"

#include <QtTest/QtTest>

#include <algorithm>
#include <cmath>

namespace C = Structura::Design::Connections;

namespace {

C::SupportWeldInput base()
{
    C::SupportWeldInput in;
    in.depth = 300.0;
    in.flangeWidth = 150.0;
    in.flangeThickness = 10.0;
    in.webThickness = 6.0;
    in.flangeFullPenetration = false;
    in.flangeLeg = 8.0;
    in.webLeg = 6.0;
    in.supportThickness = 12.0;
    in.shear = 120000.0;
    in.moment = 60.0e6;
    in.axial = 20000.0;
    return in;
}

const C::Check *find(const std::vector<C::Check> &checks, const std::string &part)
{
    for (const auto &c : checks) {
        if (c.name.find(part) != std::string::npos) {
            return &c;
        }
    }
    return nullptr;
}

} // namespace

class TestSupportWeldLengths : public QObject
{
    Q_OBJECT
private slots:
    void runsRecordGeometricAndEffectiveLegSeparately()
    {
        const auto runs = C::supportWeldRuns(base());
        QCOMPARE(runs.flange.size(), std::size_t(2));
        QCOMPARE(runs.web.size(), std::size_t(1));
        QCOMPARE(runs.flange[0].length, 150.0);
        QCOMPARE(runs.flange[1].length, (150.0 - 6.0) / 2.0);
        QCOMPARE(runs.flange[1].count, 2);
        QCOMPARE(runs.web[0].length, 300.0 - 2.0 * 10.0);
        for (const auto &r : runs.flange) {
            QCOMPARE(r.geometricLeg, 8.0);
            QCOMPARE(r.effectiveLeg, 8.0);   // comprimentos suficientes: perna efetiva = geometrica
            QVERIFY(!r.belowFourLegs && !r.belowMinimumLength);
        }
        QCOMPARE(runs.web[0].geometricLeg, 6.0);
        QCOMPARE(runs.web[0].effectiveLeg, 6.0);
    }

    void sufficientLengthAddsNoCheckAndKeepsResults()
    {
        const auto checks = C::checkSupportWeld(base());
        QVERIFY(find(checks, "comprimento efetivo minimo") == nullptr);
        QVERIFY(find(checks, "perna efetiva") == nullptr);
        for (const auto &c : checks) {
            QVERIFY2(c.clause != "6.2.6.2.3", c.name.c_str());
        }
    }

    void fullPenetrationFlangesHaveNoFlangeRuns()
    {
        auto in = base();
        in.flangeFullPenetration = true;
        const auto runs = C::supportWeldRuns(in);
        QVERIFY(runs.flange.empty());
        QCOMPARE(runs.web.size(), std::size_t(1));
    }

    void boundaryFourLegsExactlyIsNotReduced()
    {
        auto in = base();
        in.flangeWidth = 85.0;   // trecho interno (85 - 5)/2 = 40 mm
        in.webThickness = 5.0;
        in.flangeLeg = 10.0;     // 4 x 10 = 40 = L
        const auto runs = C::supportWeldRuns(in);
        QCOMPARE(runs.flange[1].length, 40.0);
        QVERIFY(!runs.flange[1].belowFourLegs);
        QVERIFY(!runs.flange[1].belowMinimumLength);
        QCOMPARE(runs.flange[1].effectiveLeg, 10.0);
        QVERIFY(find(C::checkSupportWeld(in), "perna efetiva (mesas)") == nullptr);
    }

    void justBelowFourLegsActivatesTheEffectiveLeg()
    {
        auto in = base();
        in.flangeWidth = 101.0;   // trecho interno (101 - 5)/2 = 48 mm
        in.webThickness = 5.0;
        in.flangeLeg = 12.5;      // 4 x 12,5 = 50 > 48
        const auto runs = C::supportWeldRuns(in);
        QVERIFY(runs.flange[1].belowFourLegs);
        QVERIFY(!runs.flange[1].belowMinimumLength);
        QCOMPARE(runs.flange[1].geometricLeg, 12.5);
        QCOMPARE(runs.flange[1].effectiveLeg, 12.0);   // 48/4
        const auto checks = C::checkSupportWeld(in);
        const C::Check *eff = find(checks, "perna efetiva (mesas)");
        QVERIFY(eff != nullptr);
        QCOMPARE(eff->status, C::CheckStatus::Information);   // informativo: nao reprova
        QCOMPARE(eff->clause, std::string("6.2.6.2.3"));
        QCOMPARE(eff->demand, 12.0);       // perna efetiva
        QCOMPARE(eff->resistance, 12.5);   // perna geometrica
        QVERIFY(eff->ratio() == 0.0);
        QVERIFY(find(checks, "comprimento efetivo minimo") == nullptr);   // 48 mm >= 40 mm: sem reprovacao
    }

    void effectiveLegReducesTheFilletResistanceByTheNormativeAlternative()
    {
        auto in = base();
        in.flangeWidth = 101.0;
        in.webThickness = 5.0;
        in.flangeLeg = 12.5;
        in.beamFy = 1.0e6;   // metal-base nao governa: isola a resistencia do metal da solda
        in.beamFu = 1.0e6;
        in.moment = 5.0e7;
        const auto checks = C::checkSupportWeld(in);
        const C::Check *flange = find(checks, "mesas (filete)");
        QVERIFY(flange != nullptr);
        // pernas efetivas: face externa (L = 101: 4a = 50 <= 101) a = 12,5; faces internas (L = 48) a = 12
        const double throatLength = 1 * 0.707 * 12.5 * 101.0 + 2 * 0.707 * 12.0 * 48.0;
        const double expected = 0.60 * throatLength * in.fw * 1.5 / in.gammaW2 / 1000.0;
        QVERIFY(std::abs(flange->resistance - expected) < 1e-9);
        // sem a alternativa (perna geometrica em toda parte) a resistencia seria maior
        const double unreduced = 0.60 * (0.707 * 12.5 * (2.0 * 101.0 - 5.0)) * in.fw * 1.5 / in.gammaW2 / 1000.0;
        QVERIFY(flange->resistance < unreduced);
    }

    void minimumLength40mmBoundary_data()
    {
        QTest::addColumn<double>("innerLength");   // (bf - tw)/2
        QTest::addColumn<bool>("fails");
        QTest::newRow("39.99") << 39.99 << true;
        QTest::newRow("40.00") << 40.0 << false;
        QTest::newRow("40.01") << 40.01 << false;
        QTest::newRow("20") << 20.0 << true;
    }
    void minimumLength40mmBoundary()
    {
        QFETCH(double, innerLength);
        QFETCH(bool, fails);
        auto in = base();
        in.webThickness = 4.0;
        in.flangeWidth = 4.0 + 2.0 * innerLength;
        in.flangeLeg = 3.0;   // 4a = 12 < 40: so o limite de 40 mm governa
        const auto checks = C::checkSupportWeld(in);
        const C::Check *c = find(checks, "comprimento efetivo minimo (mesas)");
        QCOMPARE(c != nullptr, fails);
        if (c) {
            QCOMPARE(c->clause, std::string("6.2.6.2.3"));
            QVERIFY(!c->ok());
            QVERIFY(c->ratio() > 1.0);
            QCOMPARE(c->resistance, 40.0);
        }
    }

    void shortFilletBelow40mmHasNoNormativeAlternative()
    {
        // mesmo com a perna efetiva (L/4), L < 40 mm reprova: a alternativa dos 25 % so substitui a relacao com 4 x perna
        auto in = base();
        in.webThickness = 4.0;
        in.flangeWidth = 4.0 + 2.0 * 30.0;   // trecho interno de 30 mm
        in.flangeLeg = 6.0;                  // 4a = 24 <= 30: perna efetiva = geometrica
        const auto checks = C::checkSupportWeld(in);
        const C::Check *c = find(checks, "comprimento efetivo minimo (mesas)");
        QVERIFY(c != nullptr);
        QVERIFY(!c->ok());
    }

    void webRunsFollowTheSameRule()
    {
        auto in = base();
        in.depth = 59.0;            // hw = 59 - 20 = 39 < 40 mm
        in.flangeThickness = 10.0;
        in.webLeg = 3.0;
        in.flangeFullPenetration = true;
        const auto checks = C::checkSupportWeld(in);
        const C::Check *c = find(checks, "comprimento efetivo minimo (alma)");
        QVERIFY(c != nullptr);
        QVERIFY(!c->ok());
        in.depth = 60.0;            // hw = 40: atende
        QVERIFY(find(C::checkSupportWeld(in), "comprimento efetivo minimo (alma)") == nullptr);
    }

    void webEffectiveLegWhenLengthBelowFourLegs()
    {
        auto in = base();
        in.depth = 20.0 + 48.0;   // hw = 48
        in.webLeg = 12.5;         // 4a = 50 > 48
        in.flangeFullPenetration = true;
        const auto runs = C::supportWeldRuns(in);
        QCOMPARE(runs.web[0].effectiveLeg, 12.0);
        const auto checks = C::checkSupportWeld(in);
        QVERIFY(find(checks, "perna efetiva (alma)") != nullptr);
        QVERIFY(find(checks, "comprimento efetivo minimo (alma)") == nullptr);
    }

    void legLimitChecksUseTheGeometricLegNotTheEffectiveOne()
    {
        auto in = base();
        in.flangeWidth = 101.0;
        in.webThickness = 5.0;
        in.flangeLeg = 12.5;
        const auto checks = C::checkSupportWeld(in);
        const C::Check *lo = find(checks, "perna minima das mesas");
        QVERIFY(lo != nullptr);
        QCOMPARE(lo->demand, 12.5);   // perna adotada (geometrica)
    }
};

QTEST_APPLESS_MAIN(TestSupportWeldLengths)
#include "TestSupportWeldLengths.moc"
