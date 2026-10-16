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

const QVector<CatalogProfile> &SectionCatalog::profiles(const QString &family)
{
    static const QHash<QString, QVector<CatalogProfile>> catalog = loadCatalog();
    static const QVector<CatalogProfile> empty;
    const auto it = catalog.constFind(family);
    return it == catalog.constEnd() ? empty : it.value();
}

} // namespace Structura::Visualization
