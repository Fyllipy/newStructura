#include <QtTest/QtTest>

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
