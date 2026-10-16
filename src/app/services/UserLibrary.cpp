#include "UserLibrary.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace Structura::App::Services {

namespace {

QString g_directoryOverride;

QString filePath(const QString &name)
{
    return QDir(UserLibrary::directory()).filePath(name);
}

QJsonArray readArray(const QString &name)
{
    QFile file(filePath(name));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object().value(QStringLiteral("items")).toArray() : QJsonArray();
}

bool writeArray(const QString &name, const QJsonArray &items)
{
    QDir().mkpath(UserLibrary::directory());
    QSaveFile file(filePath(name));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    QJsonObject root;
    root.insert(QStringLiteral("schema"), 1);
    root.insert(QStringLiteral("items"), items);
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

QJsonObject toJson(const UserLibrary::SectionInfo &s)
{
    QJsonObject o;
    o.insert(QStringLiteral("name"), s.name);
    o.insert(QStringLiteral("family"), s.family);
    o.insert(QStringLiteral("mirrored"), s.mirrored);
    QJsonObject geometry;
    for (auto it = s.geometry.constBegin(); it != s.geometry.constEnd(); ++it) {
        geometry.insert(it.key(), it.value());
    }
    o.insert(QStringLiteral("geometry"), geometry);
    o.insert(QStringLiteral("area"), s.area);
    o.insert(QStringLiteral("iz"), s.iz);
    o.insert(QStringLiteral("iy"), s.iy);
    o.insert(QStringLiteral("j"), s.j);
    return o;
}

UserLibrary::SectionInfo sectionFromJson(const QJsonObject &o)
{
    UserLibrary::SectionInfo s;
    s.name = o.value(QStringLiteral("name")).toString();
    s.family = o.value(QStringLiteral("family")).toString();
    s.mirrored = o.value(QStringLiteral("mirrored")).toBool();
    const QJsonObject geometry = o.value(QStringLiteral("geometry")).toObject();
    for (auto it = geometry.constBegin(); it != geometry.constEnd(); ++it) {
        s.geometry.insert(it.key(), it.value().toDouble());
    }
    s.area = o.value(QStringLiteral("area")).toDouble();
    s.iz = o.value(QStringLiteral("iz")).toDouble();
    s.iy = o.value(QStringLiteral("iy")).toDouble();
    s.j = o.value(QStringLiteral("j")).toDouble();
    return s;
}

} // namespace

QString UserLibrary::directory()
{
    if (!g_directoryOverride.isEmpty()) {
        return g_directoryOverride;
    }
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("library"));
}

void UserLibrary::setDirectory(const QString &path)
{
    g_directoryOverride = path;
}

QVector<UserLibrary::SectionInfo> UserLibrary::loadProfiles()
{
    QVector<SectionInfo> result;
    for (const auto &value : readArray(QStringLiteral("profiles.json"))) {
        const SectionInfo s = sectionFromJson(value.toObject());
        if (!s.name.isEmpty()) {
            result.append(s);
        }
    }
    return result;
}

bool UserLibrary::saveProfile(const SectionInfo &section)
{
    QJsonArray items;
    bool replaced = false;
    for (const auto &value : readArray(QStringLiteral("profiles.json"))) {
        const QJsonObject o = value.toObject();
        if (o.value(QStringLiteral("name")).toString() == section.name && o.value(QStringLiteral("family")).toString() == section.family) {
            items.append(toJson(section));
            replaced = true;
        } else {
            items.append(o);
        }
    }
    if (!replaced) {
        items.append(toJson(section));
    }
    return writeArray(QStringLiteral("profiles.json"), items);
}

bool UserLibrary::removeProfile(const QString &family, const QString &name)
{
    QJsonArray items;
    for (const auto &value : readArray(QStringLiteral("profiles.json"))) {
        const QJsonObject o = value.toObject();
        if (!(o.value(QStringLiteral("name")).toString() == name && o.value(QStringLiteral("family")).toString() == family)) {
            items.append(o);
        }
    }
    return writeArray(QStringLiteral("profiles.json"), items);
}

QVector<MaterialLibrary::Entry> UserLibrary::loadMaterials()
{
    QVector<MaterialLibrary::Entry> result;
    for (const auto &value : readArray(QStringLiteral("materials.json"))) {
        const QJsonObject o = value.toObject();
        MaterialLibrary::Entry e;
        e.name = o.value(QStringLiteral("name")).toString();
        if (e.name.isEmpty()) {
            continue;
        }
        e.category = static_cast<MaterialLibrary::Category>(o.value(QStringLiteral("category")).toInt(static_cast<int>(MaterialLibrary::Category::Custom)));
        e.fy = o.value(QStringLiteral("fy")).toDouble();
        e.fu = o.value(QStringLiteral("fu")).toDouble();
        e.eGPa = o.value(QStringLiteral("eGPa")).toDouble(200.0);
        e.gGPa = o.value(QStringLiteral("gGPa")).toDouble(77.0);
        e.unitWeight = o.value(QStringLiteral("unitWeight")).toDouble(78.5);
        e.note = o.value(QStringLiteral("note")).toString();
        e.user = true;
        result.append(e);
    }
    return result;
}

bool UserLibrary::saveMaterial(const MaterialLibrary::Entry &entry)
{
    QJsonObject o;
    o.insert(QStringLiteral("name"), entry.name);
    o.insert(QStringLiteral("category"), static_cast<int>(entry.category));
    o.insert(QStringLiteral("fy"), entry.fy);
    o.insert(QStringLiteral("fu"), entry.fu);
    o.insert(QStringLiteral("eGPa"), entry.eGPa);
    o.insert(QStringLiteral("gGPa"), entry.gGPa);
    o.insert(QStringLiteral("unitWeight"), entry.unitWeight);
    o.insert(QStringLiteral("note"), entry.note);
    QJsonArray items;
    bool replaced = false;
    for (const auto &value : readArray(QStringLiteral("materials.json"))) {
        const QJsonObject existing = value.toObject();
        if (existing.value(QStringLiteral("name")).toString().compare(entry.name, Qt::CaseInsensitive) == 0) {
            items.append(o);
            replaced = true;
        } else {
            items.append(existing);
        }
    }
    if (!replaced) {
        items.append(o);
    }
    return writeArray(QStringLiteral("materials.json"), items);
}

bool UserLibrary::removeMaterial(const QString &name)
{
    QJsonArray items;
    for (const auto &value : readArray(QStringLiteral("materials.json"))) {
        const QJsonObject o = value.toObject();
        if (o.value(QStringLiteral("name")).toString().compare(name, Qt::CaseInsensitive) != 0) {
            items.append(o);
        }
    }
    return writeArray(QStringLiteral("materials.json"), items);
}

} // namespace Structura::App::Services
