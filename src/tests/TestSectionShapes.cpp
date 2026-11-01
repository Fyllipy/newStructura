#include <QtTest/QtTest>

#include <cmath>

#include "viz/structure/SectionCatalog.h"
#include "viz/structure/SectionShapes.h"

using Structura::Visualization::SectionCatalog;
using Structura::Visualization::SectionShapes;

class TestSectionShapes : public QObject
{
    Q_OBJECT

private:
    static SectionShapes::Parameters defaults(const QString &family)
    {
        SectionShapes::Parameters params;
        const auto *spec = SectionShapes::findFamily(family);
        if (spec) {
            for (const auto &parameter : spec->parameters) {
                params.insert(parameter.key, parameter.defaultValue);
            }
        }
        return params;
    }

private slots:
    void initTestCase()
    {
        Q_INIT_RESOURCE(resources);
    }

    void everyFamilyBuildsWithDefaults()
    {
        for (const auto &family : SectionShapes::families()) {
            QString error;
            const auto shape = SectionShapes::build(family.name, defaults(family.name), false, &error);
            QVERIFY2(shape.has_value(), qPrintable(family.name + QStringLiteral(": ") + error));
            QVERIFY(!shape->loops.empty());
            const auto props = SectionShapes::properties(family.name, defaults(family.name), &error);
            QVERIFY2(props.has_value(), qPrintable(family.name + QStringLiteral(": ") + error));
            QVERIFY2(props->area > 0.0, qPrintable(family.name));
            QVERIFY2(props->iz > 0.0 && props->iy > 0.0, qPrintable(family.name));
            QVERIFY2(props->j > 0.0, qPrintable(family.name));
        }
    }

    void everyCatalogProfileBuilds()
    {
        int total = 0;
        QStringList failures;
        for (const auto &family : SectionShapes::families()) {
            for (const auto &profile : SectionCatalog::profiles(family.name)) {
                ++total;
                auto params = defaults(family.name);
                // Opcionais do catalogo ficam no padrao.
                for (const auto &parameter : family.parameters) {
                    if (parameter.optional) {
                        params[parameter.key] = 0.0;
                    }
                }
                for (auto it = profile.parameters.constBegin(); it != profile.parameters.constEnd(); ++it) {
                    params[it.key()] = it.value();
                }
                QString error;
                if (!SectionShapes::properties(family.name, params, &error)) {
                    failures << profile.name + QStringLiteral(": ") + error;
                }
            }
        }
        QVERIFY2(total >= 170, "catalogo nao carregado");
        QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
    }

    void rectangleProperties()
    {
        SectionShapes::Parameters params {{QStringLiteral("b"), 200.0}, {QStringLiteral("h"), 400.0}};
        const auto props = SectionShapes::properties(QStringLiteral("Retangular"), params);
        QVERIFY(props);
        QCOMPARE_LE(std::abs(props->area - 0.2 * 0.4) / (0.2 * 0.4), 1e-9);
        // Iz (altura b) = b*h^3/12 ; Iy = h*b^3/12
        QCOMPARE_LE(std::abs(props->iz - 0.2 * std::pow(0.4, 3) / 12.0) / (0.2 * std::pow(0.4, 3) / 12.0), 1e-9);
        QCOMPARE_LE(std::abs(props->iy - 0.4 * std::pow(0.2, 3) / 12.0) / (0.4 * std::pow(0.2, 3) / 12.0), 1e-9);
    }

    void wProfileAreaIsPlausible()
    {
        const auto props = SectionShapes::properties(QStringLiteral("W Laminado"), defaults(QStringLiteral("W Laminado")));
        QVERIFY(props);
        // 2*bf*tf + (d-2tf)*tw + 4*(1-pi/4)*r^2 em mm2
        const double expected = (2.0 * 165 * 9.7 + (310 - 2 * 9.7) * 5.8 + 4.0 * (1.0 - 3.14159265358979 / 4.0) * 100.0) * 1e-6;
        QCOMPARE_LE(std::abs(props->area - expected) / expected, 0.01);
    }

    void tubeAreaMatchesFormula()
    {
        SectionShapes::Parameters params {{QStringLiteral("D"), 100.0}, {QStringLiteral("t"), 5.0}};
        const auto props = SectionShapes::properties(QStringLiteral("Tubular Circular"), params);
        QVERIFY(props);
        const double expected = 3.14159265358979 / 4.0 * (100.0 * 100.0 - 90.0 * 90.0) * 1e-6;
        // O circulo e poligonal (48 lados): tolerancia de 0.5 %
        QCOMPARE_LE(std::abs(props->area - expected) / expected, 0.005);
    }

    void weldedFamiliesMatchClosedFormProperties()
    {
        // I soldado simetrico 400 x 200 x 8 x 12,5 (mm): A = 2 bf tf + (d - 2 tf) tw; Ix = soma dos retangulos; J = soma de b t^3 / 3
        SectionShapes::Parameters ip {{QStringLiteral("d"), 400}, {QStringLiteral("bf"), 200}, {QStringLiteral("tw"), 8}, {QStringLiteral("tf"), 12.5}};
        const auto iProps = SectionShapes::properties(QStringLiteral("I Soldado"), ip);
        QVERIFY(iProps);
        const double area = 2.0 * 200.0 * 12.5 + 375.0 * 8.0;
        QVERIFY(std::abs(iProps->area * 1e6 - area) < 1e-6 * area);
        const double ixExpected = 2.0 * (200.0 * std::pow(12.5, 3) / 12.0 + 200.0 * 12.5 * std::pow(193.75, 2)) + 8.0 * std::pow(375.0, 3) / 12.0;
        QVERIFY(std::abs(iProps->iz * 1e12 - ixExpected) < 1e-6 * ixExpected);   // iz = inercia no plano da altura (eixo horizontal)
        const double jExpected = (2.0 * 200.0 * std::pow(12.5, 3) + (400.0 - 12.5) * std::pow(8.0, 3)) / 3.0;
        QVERIFY(std::abs(iProps->j * 1e12 - jExpected) < 1e-6 * jExpected);
        // mesa inferior diferente: centroide deslocado para a mesa maior (a maior fica no topo: b do centroide acima do meio)
        ip.insert(QStringLiteral("bfi"), 120);
        ip.insert(QStringLiteral("tfi"), 10);
        const auto shape = SectionShapes::build(QStringLiteral("I Soldado"), ip, false);
        QVERIFY(shape);
        QVERIFY(shape->bounds[3] < -shape->bounds[2]);   // topo mais perto do centroide que a base: mesa superior (maior) puxa o centroide para cima
        const auto mono = SectionShapes::properties(QStringLiteral("I Soldado"), ip);
        QVERIFY(mono);
        QVERIFY(std::abs(mono->area * 1e6 - (200.0 * 12.5 + 120.0 * 10.0 + (400.0 - 22.5) * 8.0)) < 1e-6 * area);
        // caixao 400 x 300 x 10 x 16
        SectionShapes::Parameters box {{QStringLiteral("d"), 400}, {QStringLiteral("bf"), 300}, {QStringLiteral("tw"), 10}, {QStringLiteral("tf"), 16}};
        const auto boxProps = SectionShapes::properties(QStringLiteral("Caixao Soldado"), box);
        QVERIFY(boxProps);
        QVERIFY(std::abs(boxProps->area * 1e6 - (300.0 * 400.0 - 280.0 * 368.0)) < 1e-6 * 17000.0);
        const double jBox = 4.0 * std::pow(290.0 * 384.0, 2) / (2.0 * 290.0 / 16.0 + 2.0 * 384.0 / 10.0);   // Bredt
        QVERIFY(std::abs(boxProps->j * 1e12 - jBox) < 1e-6 * jBox);
        // tubo soldado: pi/4 (D^2 - (D - 2t)^2)
        SectionShapes::Parameters tube {{QStringLiteral("D"), 508}, {QStringLiteral("t"), 9.5}};
        const auto tubeProps = SectionShapes::properties(QStringLiteral("Tubo Soldado"), tube);
        QVERIFY(tubeProps);
        const double areaTube = 3.14159265358979 / 4.0 * (508.0 * 508.0 - 489.0 * 489.0);
        QVERIFY(std::abs(tubeProps->area * 1e6 - areaTube) < 0.01 * areaTube);   // contorno poligonal de 48 lados: erro < 1%
        // Z enrijecido: o enrijecedor acrescenta area
        SectionShapes::Parameters z {{QStringLiteral("d"), 200}, {QStringLiteral("bf"), 75}, {QStringLiteral("t"), 2.65}};
        const auto plain = SectionShapes::properties(QStringLiteral("Z Formado"), z);
        z.insert(QStringLiteral("c"), 25);
        const auto lipped = SectionShapes::properties(QStringLiteral("Z Formado"), z);
        QVERIFY(plain && lipped);
        QVERIFY(std::abs((lipped->area - plain->area) * 1e6 - 2.0 * 25.0 * 2.65) < 0.06 * 2.0 * 25.0 * 2.65 + 4.0);
        QVERIFY(lipped->iz > plain->iz && lipped->iy > plain->iy);
    }

    void invalidDimensionsAreRejected()
    {
        QString error;
        const auto shape = SectionShapes::build(QStringLiteral("W Laminado"),
                                                {{QStringLiteral("d"), 10.0}, {QStringLiteral("bf"), 50.0},
                                                 {QStringLiteral("tw"), 5.0}, {QStringLiteral("tf"), 8.0}, {QStringLiteral("r"), 10.0}},
                                                false, &error);
        QVERIFY(!shape.has_value());
        QVERIFY(!error.isEmpty());
    }

    void mirroringFlipsWidthAxis()
    {
        const auto a = SectionShapes::build(QStringLiteral("L Laminado"), defaults(QStringLiteral("L Laminado")), false);
        const auto b = SectionShapes::build(QStringLiteral("L Laminado"), defaults(QStringLiteral("L Laminado")), true);
        QVERIFY(a && b);
        QCOMPARE_LE(std::abs(a->bounds[0] + b->bounds[1]), 1e-9);
    }
};

QTEST_APPLESS_MAIN(TestSectionShapes)
#include "TestSectionShapes.moc"
