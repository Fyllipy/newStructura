#include "viz/structure/SectionCatalog.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>

namespace Structura::Visualization {

namespace {

QHash<QString, QVector<CatalogProfile>> loadCatalog()
{
    QHash<QString, QVector<CatalogProfile>> catalog;
    QFile file(QStringLiteral(":/data/catalog_steel.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return catalog;
    }
    const QJsonObject families = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("families")).toObject();
    for (auto it = families.begin(); it != families.end(); ++it) {
        QVector<CatalogProfile> list;
        for (const QJsonValue &value : it.value().toArray()) {
            const QJsonObject object = value.toObject();
            CatalogProfile profile;
            profile.name = object.value(QStringLiteral("name")).toString();
            for (auto field = object.begin(); field != object.end(); ++field) {
                if (field.key() != QLatin1String("name") && field.value().isDouble()) {
                    profile.parameters.insert(field.key(), field.value().toDouble());
                }
            }
            list.append(std::move(profile));
        }
        catalog.insert(it.key(), std::move(list));
    }
    return catalog;
}

} // namespace

namespace {

/// Catalogo das barras compostas: cada perfil laminado do catalogo (L ou U) combinado em dupla, com o afastamento padrao de 10 mm
/// (chapa espacadora/de no usual) e o nome "2L ..." / "2U ... (costas)" / "2U ... (face a face)".
QHash<QString, QVector<CatalogProfile>> composedCatalog(const QHash<QString, QVector<CatalogProfile>> &simple)
{
    QHash<QString, QVector<CatalogProfile>> composed;
    constexpr double kDefaultGap = 10.0;
    auto derive = [&](const QString &composedFamily, const QString &baseFamily, const QString &prefix, const QString &suffix) {
        const auto base = simple.constFind(baseFamily);
        if (base == simple.constEnd()) {
            return;
        }
        QVector<CatalogProfile> list;
        for (const CatalogProfile &profile : base.value()) {
            CatalogProfile item;
            QString name = profile.name;
            name.remove(0, name.indexOf(QLatin1Char(' ')) + 1);   // "L 76 x 6.1" -> "76 x 6.1"
            item.name = prefix + QLatin1Char(' ') + name + suffix;
            item.parameters = profile.parameters;
            item.parameters.insert(QStringLiteral("e"), kDefaultGap);
            list.append(std::move(item));
        }
        composed.insert(composedFamily, std::move(list));
    };
    derive(QStringLiteral("Dupla Cantoneira T"), QStringLiteral("L Laminado"), QStringLiteral("2L"), QString());
    derive(QStringLiteral("Duplo U Costas"), QStringLiteral("U Laminado"), QStringLiteral("2U"), QStringLiteral(" (costas)"));
    derive(QStringLiteral("Duplo U Face a Face"), QStringLiteral("U Laminado"), QStringLiteral("2U"), QStringLiteral(" (face a face)"));
    return composed;
}

} // namespace

namespace {

/// Series tipicas de perfis armados (I soldado e caixao): chapas comerciais com proporcoes usuais; confira a serie e as espessuras com o fabricante.
QHash<QString, QVector<CatalogProfile>> weldedCatalog()
{
    QHash<QString, QVector<CatalogProfile>> out;
    auto number = [](double v) { return QString::number(v, 'g', 5); };
    QVector<CatalogProfile> iProfiles;
    for (const double d : {200.0, 250.0, 300.0, 350.0, 400.0, 450.0, 500.0, 600.0, 700.0, 800.0, 1000.0}) {
        for (const double bf : {100.0, 150.0, 200.0, 250.0, 300.0, 400.0}) {
            if (bf < d / 4.0 - 1.0 || bf > d * 0.9 + 1.0) {
                continue;
            }
            for (const double tf : {9.5, 12.5, 16.0, 19.0, 25.0}) {
                if (tf * 2.0 > d / 5.0 || bf / (2.0 * tf) > 14.0 || bf / (2.0 * tf) < 3.0) {
                    continue;
                }
                for (const double tw : {6.3, 8.0, 9.5, 12.5}) {
                    const double h = d - 2.0 * tf;
                    if (tw > tf || h / tw > 170.0 || h / tw < 40.0) {
                        continue;
                    }
                    CatalogProfile profile;
                    profile.name = QStringLiteral("I %1x%2x%3x%4").arg(number(d), number(bf), number(tw), number(tf));
                    profile.parameters = {{QStringLiteral("d"), d}, {QStringLiteral("bf"), bf}, {QStringLiteral("tw"), tw}, {QStringLiteral("tf"), tf}};
                    iProfiles.append(profile);
                }
            }
        }
    }
    out.insert(QStringLiteral("I Soldado"), iProfiles);
    QVector<CatalogProfile> boxes;
    for (const double d : {200.0, 300.0, 400.0, 500.0, 600.0, 800.0}) {
        for (const double bf : {200.0, 300.0, 400.0, 500.0}) {
            if (bf < d / 4.0 || bf > d * 1.6 + 1.0) {
                continue;
            }
            for (const double tf : {9.5, 12.5, 16.0, 25.0}) {
                for (const double tw : {8.0, 9.5, 12.5, 16.0}) {
                    if (tw > tf * 1.01 || (d - 2.0 * tf) / tw > 150.0 || (bf - 2.0 * tw) / tf > 70.0 || 2.0 * tf > d / 4.0) {
                        continue;
                    }
                    CatalogProfile profile;
                    profile.name = QStringLiteral("CX %1x%2x%3x%4").arg(number(d), number(bf), number(tw), number(tf));
                    profile.parameters = {{QStringLiteral("d"), d}, {QStringLiteral("bf"), bf}, {QStringLiteral("tw"), tw}, {QStringLiteral("tf"), tf}};
                    boxes.append(profile);
                }
            }
        }
    }
    out.insert(QStringLiteral("Caixao Soldado"), boxes);
    QVector<CatalogProfile> tubes;
    for (const double diameter : {323.9, 406.4, 508.0, 610.0, 711.0, 813.0, 914.0, 1016.0, 1219.0}) {
        for (const double thickness : {6.3, 8.0, 9.5, 12.5, 16.0, 19.0, 25.0}) {
            if (thickness > diameter / 20.0 || diameter / thickness > 150.0) {
                continue;
            }
            CatalogProfile profile;
            profile.name = QStringLiteral("TS %1x%2").arg(number(diameter), number(thickness));
            profile.parameters = {{QStringLiteral("D"), diameter}, {QStringLiteral("t"), thickness}};
            tubes.append(profile);
        }
    }
    out.insert(QStringLiteral("Tubo Soldado"), tubes);
    return out;
}

} // namespace

const QVector<CatalogProfile> &SectionCatalog::profiles(const QString &family)
{
    static const QHash<QString, QVector<CatalogProfile>> catalog = [] {
        QHash<QString, QVector<CatalogProfile>> all = loadCatalog();
        // Z enrijecido (terca usual): serie d x bf x c da NBR 6355, espessuras comerciais; mesma familia "Z Formado" com enrijecedor c > 0
        {
            auto &zList = all[QStringLiteral("Z Formado")];
            static const double sizes[][3] = {{100, 40, 17}, {127, 50, 17}, {150, 60, 20}, {200, 75, 25}, {250, 85, 25}, {300, 85, 25}, {350, 100, 25}, {400, 100, 25}};
            for (const auto &sz : sizes) {
                for (const double thickness : {1.5, 1.95, 2.25, 2.65, 3.0, 3.35, 3.75, 4.25, 4.75}) {
                    if (sz[0] <= 127 && thickness > 3.0) {
                        continue;
                    }
                    CatalogProfile profile;
                    profile.name = QStringLiteral("Zs %1x%2x%3x%4").arg(sz[0]).arg(sz[1]).arg(sz[2]).arg(thickness);
                    profile.parameters = {{QStringLiteral("d"), sz[0]}, {QStringLiteral("bf"), sz[1]}, {QStringLiteral("c"), sz[2]}, {QStringLiteral("t"), thickness}};
                    zList.append(profile);
                }
            }
        }
        // cantoneira enrijecida (L com enrijecedor de borda): abas iguais d = bf com c
        {
            auto &lList = all[QStringLiteral("L Formado")];
            static const double sizes[][2] = {{40, 12}, {50, 15}, {60, 17}, {75, 20}, {100, 25}};
            for (const auto &sz : sizes) {
                for (const double thickness : {1.5, 1.95, 2.25, 2.65, 3.0}) {
                    CatalogProfile profile;
                    profile.name = QStringLiteral("Ls %1x%2x%3").arg(sz[0]).arg(sz[1]).arg(thickness);
                    profile.parameters = {{QStringLiteral("d"), sz[0]}, {QStringLiteral("bf"), sz[0]}, {QStringLiteral("c"), sz[1]}, {QStringLiteral("t"), thickness}};
                    lList.append(profile);
                }
            }
        }
        const auto welded = weldedCatalog();
        for (auto it = welded.begin(); it != welded.end(); ++it) {
            all.insert(it.key(), it.value());
        }
        const auto composed = composedCatalog(all);
        for (auto it = composed.begin(); it != composed.end(); ++it) {
            all.insert(it.key(), it.value());
        }
        return all;
    }();
    static const QVector<CatalogProfile> empty;
    const auto it = catalog.constFind(family);
    return it == catalog.constEnd() ? empty : it.value();
}

} // namespace Structura::Visualization
