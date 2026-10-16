#include "ProfileLibrary.h"

#include "viz/structure/SectionCatalog.h"

#include <QHash>
#include <QObject>

namespace Structura::UI {

namespace {
using Structura::Visualization::SectionCatalog;
using Structura::Visualization::SectionShapes;

const QUuid kNamespace = QUuid::fromString(QStringLiteral("{6b0f6d1e-3c59-4c6e-9f0a-5a4d2f8e7b11}"));
} // namespace

namespace {
QVector<Structura::App::ProjectData::SectionInfo> g_userProfiles;
bool g_dirty = true;
} // namespace

const QVector<ProfileLibrary::Entry> &ProfileLibrary::entries()
{
    static QVector<Entry> list;
    if (g_dirty || list.isEmpty()) {
        list.clear();
        for (const auto &family : SectionShapes::families()) {
            for (const auto &profile : SectionCatalog::profiles(family.name)) {
                Entry entry;
                entry.family = family.name;
                entry.name = profile.name;
                entry.parameters = profile.parameters;
                entry.id = QUuid::createUuidV5(kNamespace, family.name + QLatin1Char('|') + profile.name);
                list.append(entry);
            }
        }
        for (const auto &section : g_userProfiles) {
            Entry entry;
            entry.user = true;
            entry.family = section.family;
            entry.name = section.name;
            entry.parameters = section.geometry;
            entry.propertiesOnly = section.family.isEmpty() || section.geometry.isEmpty();
            entry.area = section.area;
            entry.iz = section.iz;
            entry.iy = section.iy;
            entry.j = section.j;
            entry.id = QUuid::createUuidV5(kNamespace, QStringLiteral("user|") + section.family + QLatin1Char('|') + section.name);
            list.append(entry);
        }
        g_dirty = false;
    }
    return list;
}

void ProfileLibrary::setUserProfiles(const QVector<Structura::App::ProjectData::SectionInfo> &profiles)
{
    g_userProfiles = profiles;
    g_dirty = true;
}

const ProfileLibrary::Entry *ProfileLibrary::find(const QUuid &id)
{
    for (const auto &entry : entries()) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

QStringList ProfileLibrary::families()
{
    QStringList result;
    for (const auto &entry : entries()) {
        if (!result.contains(entry.family)) {
            result.append(entry.family);
        }
    }
    return result;
}

QList<ProfileLibrary::Category> ProfileLibrary::categories()
{
    return {Category::Laminado, Category::MoldadoAFrio, Category::Concreto, Category::SemGeometria};
}

QString ProfileLibrary::categoryName(Category category)
{
    switch (category) {
    case Category::Laminado: return QObject::tr("Aço laminado");
    case Category::MoldadoAFrio: return QObject::tr("Aço moldado a frio");
    case Category::Concreto: return QObject::tr("Concreto");
    case Category::SemGeometria: return QObject::tr("Só propriedades");
    }
    return {};
}

ProfileLibrary::Category ProfileLibrary::categoryOf(const QString &family)
{
    const auto *info = SectionShapes::findFamily(family);
    if (!info) {
        return Category::SemGeometria;
    }
    if (info->material == QLatin1String("Concreto")) {
        return Category::Concreto;
    }
    // Perfis "formados" e tubulares (conformados a frio) vs. laminados a quente (W, I, U, L, T e barras).
    if (family.endsWith(QLatin1String("Formado")) || family.startsWith(QLatin1String("Tubular"))) {
        return Category::MoldadoAFrio;
    }
    return Category::Laminado;
}

QStringList ProfileLibrary::familiesIn(Category category)
{
    QStringList result;
    for (const auto &family : SectionShapes::families()) {
        if (categoryOf(family.name) == category) {
            result.append(family.name);
        }
    }
    return result;
}

QString ProfileLibrary::typeLabel(const QString &family)
{
    QString label = family;
    for (const char *suffix : {" Laminado", " Formado"}) {
        if (label.endsWith(QLatin1String(suffix))) {
            label.chop(int(qstrlen(suffix)));
            break;
        }
    }
    return label;
}

Structura::App::ProjectData::SectionInfo ProfileLibrary::makeSection(const Entry &entry)
{
    Structura::App::ProjectData::SectionInfo info;
    info.uuid = entry.id;
    info.name = entry.name;
    info.family = entry.family;
    info.geometry = entry.parameters;
    if (entry.propertiesOnly) {
        info.geometry.clear();
        info.area = entry.area;
        info.iz = entry.iz;
        info.iy = entry.iy;
        info.j = entry.j;
        return info;
    }
    if (const auto props = SectionShapes::properties(entry.family, entry.parameters)) {
        info.area = props->area;
        info.iz = props->iz;
        info.iy = props->iy;
        info.j = props->j;
    }
    return info;
}

} // namespace Structura::UI
