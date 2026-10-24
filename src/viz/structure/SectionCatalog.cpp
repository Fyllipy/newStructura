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

const QVector<CatalogProfile> &SectionCatalog::profiles(const QString &family)
{
    static const QHash<QString, QVector<CatalogProfile>> catalog = [] {
        QHash<QString, QVector<CatalogProfile>> all = loadCatalog();
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
