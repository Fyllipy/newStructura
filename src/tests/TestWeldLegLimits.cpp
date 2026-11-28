// S-03.1: fronteiras do limite de espessura da Tabela 11 (perna minima) e de 6.2.6.2.2 (perna maxima) da NBR 8800:2024, aplicadas LITERALMENTE.
// Tabela 11: "abaixo de 6,3 e ate 6,3: 3 mm; acima de 6,3 ate 12,5: 5 mm; acima de 12,5 ate 19: 6 mm; acima de 19: 8 mm".
// 6.2.6.2.2: perna maxima ao longo de bordas: t se t < 6,3 mm; t - 1,5 mm se t >= 6,3 mm.
// (Antes do S-03.1 o SupportWeld usava 6,35 mm; os pontos de divergencia estavam como QEXPECT_FAIL e agora sao testes normais.)
// Consequencia: parte de 6,35 mm (1/4 pol) passa a exigir perna minima de 5 mm, como nos demais modulos (checkFilletWeld, ShearConnections, BeamExtras).
#include "design/Connections.h"
#include "design/SupportWeld.h"

#include <QtTest/QtTest>

namespace C = Structura::Design::Connections;

namespace {

double nbrMin(double t)
{
    if (t <= 6.3) return 3.0;
    if (t <= 12.5) return 5.0;
    if (t <= 19.0) return 6.0;
    return 8.0;
}

double nbrMax(double t) { return t < 6.3 ? t : t - 1.5; }

} // namespace

class TestWeldLegLimits : public QObject
{
    Q_OBJECT
private slots:
    void minimumLegTable11_data()
    {
        QTest::addColumn<double>("t");
        QTest::addColumn<double>("expected");
        for (const double t : {2.0, 6.0, 6.29, 6.30, 6.31, 6.32, 6.33, 6.34, 6.35, 6.36, 6.4, 8.0, 12.49, 12.5, 12.51, 16.0, 18.99, 19.0, 19.01, 25.0, 40.0}) {
            QTest::newRow(qPrintable(QString::number(t, 'f', 2))) << t << nbrMin(t);
        }
    }
    void minimumLegTable11()
    {
        QFETCH(double, t);
        QFETCH(double, expected);
        QCOMPARE(C::minimumFilletLeg(t), expected);
    }

    void minimumLegKeyBoundaries()
    {
        QCOMPARE(C::minimumFilletLeg(6.29), 3.0);
        QCOMPARE(C::minimumFilletLeg(6.30), 3.0);   // "ate 6,3": inclusive
        QCOMPARE(C::minimumFilletLeg(6.31), 5.0);   // "acima de 6,3"
        QCOMPARE(C::minimumFilletLeg(6.35), 5.0);   // 1/4 pol passa a exigir 5 mm
        QCOMPARE(C::minimumFilletLeg(6.36), 5.0);
        QCOMPARE(C::minimumFilletLeg(12.5), 5.0);
        QCOMPARE(C::minimumFilletLeg(12.51), 6.0);
        QCOMPARE(C::minimumFilletLeg(19.0), 6.0);
        QCOMPARE(C::minimumFilletLeg(19.01), 8.0);
    }

    void maximumLeg6_2_6_2_2_data()
    {
        QTest::addColumn<double>("t");
        QTest::addColumn<double>("expected");
        for (const double t : {2.0, 6.0, 6.29, 6.30, 6.31, 6.34, 6.35, 6.36, 6.4, 8.0, 12.5, 19.0}) {
            QTest::newRow(qPrintable(QString::number(t, 'f', 2))) << t << nbrMax(t);
        }
    }
    void maximumLeg6_2_6_2_2()
    {
        QFETCH(double, t);
        QFETCH(double, expected);
        QCOMPARE(C::maximumFilletLeg(t), expected);
    }

    void maximumLegKeyBoundaries()
    {
        QCOMPARE(C::maximumFilletLeg(6.29), 6.29);        // t < 6,3: perna maxima = t
        QCOMPARE(C::maximumFilletLeg(6.30), 6.30 - 1.5);  // t >= 6,3: t - 1,5 mm
        QCOMPARE(C::maximumFilletLeg(6.35), 6.35 - 1.5);
    }

    void supportWeldChecksUseTheCorrectedLimits()
    {
        C::SupportWeldInput in;
        in.flangeFullPenetration = false;
        in.flangeThickness = 6.35;
        in.webThickness = 6.35;
        in.depth = 300.0;
        in.flangeWidth = 150.0;
        in.flangeLeg = 3.0;
        in.webLeg = 3.0;
        in.supportThickness = 0.0;
        in.shear = 10000.0;
        in.moment = 1.0e6;
        int found = 0;
        for (const auto &c : C::checkSupportWeld(in)) {
            if (c.name.find("perna minima") != std::string::npos) {
                QCOMPARE(c.resistance, 5.0);   // parte de 6,35 mm exige 5 mm
                QVERIFY2(!c.ok(), c.name.c_str());   // perna de 3 mm reprova
                ++found;
            }
        }
        QCOMPARE(found, 2);
    }

    void otherModulesAgreeWithSupportWeld()
    {
        // checkFilletWeld (Connections.cpp) ja aplicava a Tabela 11 com 6,3 mm: agora os modulos concordam em 6,35 mm
        C::WeldInput w;
        w.plateThickness = 6.35;
        w.leg = 3.0;
        w.length = 100.0;
        w.force = 1000.0;
        bool found = false;
        for (const auto &c : C::checkFilletWeld(w)) {
            if (c.name == "Perna minima do filete") {
                found = true;
                QCOMPARE(c.resistance, C::minimumFilletLeg(6.35));
            }
        }
        QVERIFY(found);
    }
};

QTEST_APPLESS_MAIN(TestWeldLegLimits)
#include "TestWeldLegLimits.moc"
