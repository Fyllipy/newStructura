// Rigidez inicial do T-stub (Fase B da A3): benchmark PUBLICADO (exemplo SCIA, EN 1993-1-8, HEB140 + IPE220), propriedades mecanicas e dados invalidos.
// Os valores de referencia sao os publicados; nenhum e obtido de outra funcao do Structura. NAO e validacao profissional.
#include "design/TStubStiffness.h"

#include <QtTest/QtTest>

#include <cmath>
#include <limits>

namespace TS = Structura::Design::TStub;

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

// dados publicados do exemplo SCIA: parafuso M16 (As = 157 mm2), chapa 12 mm, mesa do pilar tf = 12 mm; Lb = 12 + 12 + 2 x 1,65 + (10 + 13)/2 = 38,8 mm
TS::BoltElongation sciaBolt()
{
    TS::BoltElongation b;
    b.grip = 12.0 + 12.0;
    b.washers = 2.0 * 1.65;
    b.headHeight = 10.0;
    b.nutHeight = 13.0;
    return b;
}

bool near(double value, double published, double relTol)
{
    return std::abs(value - published) <= relTol * std::abs(published);
}

TS::StiffnessInput base()
{
    TS::StiffnessInput in;
    in.m = 26.9;
    in.leff = 107.55;
    in.thickness = 12.0;
    in.boltStressArea = 157.0;
    in.bolt = sciaBolt();
    return in;
}

} // namespace

class TestTStubStiffness : public QObject
{
    Q_OBJECT
private slots:
    void publishedScia_componentCoefficients()
    {
        const TS::EnStiffnessCoefficients c;
        // tolerancia: valores publicados com 2 decimais (+-0,005 mm = 0,06 % de 8,6) e entradas publicadas arredondadas (leff, m): 0,2 % para k4 e k10, 0,5 % para k5 (m calculado de valores arredondados)
        const auto k4a = TS::flangeBending(26.9, 107.55, 12.0, c, "k4");
        const auto k4b = TS::flangeBending(26.9, 105.0, 12.0, c, "k4");
        QVERIFY(k4a.valid && k4b.valid);
        QVERIFY2(near(k4a.k, 8.59, 0.002), qPrintable(QString::number(k4a.k, 'f', 4)));
        QVERIFY2(near(k4b.k, 8.39, 0.002), qPrintable(QString::number(k4b.k, 'f', 4)));
        const double mx = 30.0 - 0.8 * std::sqrt(2.0) * 5.0;   // 24,34 mm (publicado)
        const auto k5a = TS::flangeBending(mx, 70.0, 12.0, c, "k5");
        const auto k5b = TS::flangeBending(33.66, 185.51, 12.0, c, "k5");
        QVERIFY2(near(k5a.k, 7.55, 0.005), qPrintable(QString::number(k5a.k, 'f', 4)));
        QVERIFY2(near(k5b.k, 7.57, 0.005), qPrintable(QString::number(k5b.k, 'f', 4)));
        const auto k10 = TS::boltsInTension(157.0, sciaBolt(), c);
        QVERIFY(k10.valid);
        QVERIFY2(near(sciaBolt().length(), 38.8, 1e-4), "Lb publicado = 38,8 mm");
        QVERIFY2(near(k10.k, 6.47, 0.002), qPrintable(QString::number(k10.k, 'f', 4)));   // 1,6 As/Lb, sem fator 2
        QVERIFY(!k10.lines.empty());
    }

    void publishedScia_seriesReproducesKeff()
    {
        // keff,r = 1 / (1/k3 + 1/k4 + 1/k5 + 1/k10) com os ki PUBLICADOS (k3 vem de outro componente, ainda nao implementado no nucleo)
        QVERIFY(near(TS::seriesCoefficient({5.73, 8.59, 7.55, 6.47}), 1.73, 0.01));   // fileira 1
        QVERIFY(near(TS::seriesCoefficient({5.59, 8.39, 7.57, 6.47}), 1.71, 0.01));   // fileira 2
    }

    void tStubStiffnessIsTheSeriesOfFlangeAndBolts()
    {
        const TS::EnStiffnessCoefficients c;
        const auto r = TS::tStubStiffness(base(), c, "k4", "Mesa do pilar");
        QVERIFY2(r.valid, r.error.c_str());
        QVERIFY(near(r.kFlange, 8.59, 0.002));
        QVERIFY(near(r.kBolt, 6.47, 0.002));
        QVERIFY(near(r.k, 1.0 / (1.0 / r.kFlange + 1.0 / r.kBolt), 1e-12));
        QVERIFY(r.k < std::min(r.kFlange, r.kBolt));   // serie: sempre mais mole que o mais mole
        QVERIFY(r.lines.size() >= 4);
        // rigidez em forca: k x E (N/mm)
        QVERIFY(near(TS::forceStiffness(r.k, 210000.0), r.k * 210000.0, 1e-12));
        QCOMPARE(TS::forceStiffness(r.k, 0.0), 0.0);
        QCOMPARE(TS::forceStiffness(r.k, -1.0), 0.0);
        QCOMPARE(TS::forceStiffness(std::nan(""), 210000.0), 0.0);
    }

    void mechanicalScalingLaws()
    {
        const TS::EnStiffnessCoefficients c;
        const double k0 = TS::flangeBending(26.9, 107.55, 12.0, c, "k4").k;
        QVERIFY(near(TS::flangeBending(26.9, 107.55, 24.0, c, "k4").k, 8.0 * k0, 1e-12));    // t^3
        QVERIFY(near(TS::flangeBending(26.9, 215.10, 12.0, c, "k4").k, 2.0 * k0, 1e-12));    // leff
        QVERIFY(near(TS::flangeBending(53.8, 107.55, 12.0, c, "k4").k, k0 / 8.0, 1e-12));    // m^-3
        const auto b0 = TS::boltsInTension(157.0, sciaBolt(), c);
        QVERIFY(near(TS::boltsInTension(314.0, sciaBolt(), c).k, 2.0 * b0.k, 1e-12));         // As
        TS::BoltElongation longer = sciaBolt();
        longer.grip = 2.0 * 38.8 - longer.washers - 0.5 * (longer.headHeight + longer.nutHeight);   // Lb dobrado
        QVERIFY(near(longer.length(), 77.6, 1e-12));
        QVERIFY(near(TS::boltsInTension(157.0, longer, c).k, 0.5 * b0.k, 1e-12));
        // monotonicidade: engrossar a mesa nunca reduz a rigidez do T-stub; parafuso maior nunca reduz
        double previous = 0.0;
        for (double t = 8.0; t <= 40.0; t += 2.0) {
            TS::StiffnessInput in = base();
            in.thickness = t;
            const auto r = TS::tStubStiffness(in, c, "k4", "x");
            QVERIFY(r.valid);
            QVERIFY(r.k > previous);
            QVERIFY(r.k < r.kBolt);   // limitado pelo parafuso quando a mesa e muito rigida
            previous = r.k;
        }
        // limite: mesa muito rigida -> k_T tende a k10
        TS::StiffnessInput stiff = base();
        stiff.thickness = 400.0;
        const auto s = TS::tStubStiffness(stiff, c, "k4", "x");
        QVERIFY(near(s.k, s.kBolt, 1e-3));
    }

    void seriesWithInfiniteAndInvalidComponents()
    {
        QVERIFY(near(TS::seriesCoefficient({4.0, kInf}), 4.0, 1e-12));     // componente enrijecido contribui 0
        QVERIFY(near(TS::seriesCoefficient({4.0, 4.0}), 2.0, 1e-12));
        QVERIFY(std::isinf(TS::seriesCoefficient({kInf, kInf})));
        QCOMPARE(TS::seriesCoefficient({}), 0.0);
        QCOMPARE(TS::seriesCoefficient({4.0, 0.0}), 0.0);                  // invalido nunca vira rigidez
        QCOMPARE(TS::seriesCoefficient({4.0, -1.0}), 0.0);
        QCOMPARE(TS::seriesCoefficient({4.0, std::nan("")}), 0.0);
    }

    void missingOrInvalidDataIsNeverDefaulted()
    {
        const TS::EnStiffnessCoefficients c;
        // nada informado (NaN) -> invalido com causa; nenhum Lb, As ou comprimento assumido
        TS::StiffnessInput empty;
        const auto none = TS::tStubStiffness(empty, c, "k4", "T");
        QVERIFY(!none.valid);
        QVERIFY(!none.error.empty());
        QCOMPARE(none.k, 0.0);
        QVERIFY(!TS::BoltElongation().valid());
        QVERIFY(!TS::boltsInTension(157.0, TS::BoltElongation(), c).valid);
        // cada campo obrigatorio ausente/invalido
        for (int field = 0; field < 8; ++field) {
            TS::StiffnessInput in = base();
            switch (field) {
            case 0: in.m = 0.0; break;
            case 1: in.leff = -1.0; break;
            case 2: in.thickness = std::nan(""); break;
            case 3: in.boltStressArea = 0.0; break;
            case 4: in.bolt.grip = std::nan(""); break;
            case 5: in.bolt.washers = std::nan(""); break;   // arruela nao informada nao vira 0
            case 6: in.bolt.headHeight = 0.0; break;
            case 7: in.bolt.nutHeight = kInf; break;
            }
            const auto r = TS::tStubStiffness(in, c, "k4", "T");
            QVERIFY2(!r.valid, qPrintable(QString::number(field)));
            QVERIFY(!r.error.empty());
        }
        // arruela explicitamente 0 e valida (sem arruela)
        TS::StiffnessInput noWasher = base();
        noWasher.bolt.washers = 0.0;
        QVERIFY(TS::tStubStiffness(noWasher, c, "k4", "T").valid);
        TS::StiffnessInput negativeWasher = base();
        negativeWasher.bolt.washers = -0.1;
        QVERIFY(!TS::tStubStiffness(negativeWasher, c, "k4", "T").valid);
        // coeficientes invalidos
        TS::EnStiffnessCoefficients bad;
        bad.flangeBending = 0.0;
        QVERIFY(!TS::flangeBending(26.9, 107.55, 12.0, bad, "k4").valid);
        bad = TS::EnStiffnessCoefficients();
        bad.boltTension = -1.6;
        QVERIFY(!TS::boltsInTension(157.0, sciaBolt(), bad).valid);
    }

    void coefficientsAreEnOnlyAndNamedByTable()
    {
        const TS::EnStiffnessCoefficients c;
        QCOMPARE(c.flangeBending, 0.9);   // EN 1993-1-8, Tabela 6.11
        QCOMPARE(c.boltTension, 1.6);
        // a mesma geometria, plastica x elastica: a rigidez NAO depende de fy nem de gammaM (nao e entrada)
        const auto r = TS::flangeBending(26.9, 107.55, 12.0, c, "k5");
        QVERIFY(r.lines.front().rfind("k5 = ", 0) == 0);
        QVERIFY(r.lines.front().find("Tabela 6.11") != std::string::npos);
    }
};

QTEST_MAIN(TestTStubStiffness)
#include "TestTStubStiffness.moc"
