#include "QuantityService.h"

#include "viz/structure/SectionShapes.h"

#include <QMap>
#include <QObject>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace Structura::App::Services {

namespace {

constexpr double kGravity = 9.80665;

double perimeterMeters(const Structura::App::ProjectData::SectionInfo &section)
{
    if (section.family.isEmpty()) {
        return 0.0;
    }
    const auto shape = Structura::Visualization::SectionShapes::build(section.family, section.geometry, section.mirrored);
    if (!shape) {
        return 0.0;
    }
    double total = 0.0;
    for (const auto &loop : shape->loops) {
        for (std::size_t i = 0; i < loop.size(); ++i) {
            const auto &a = loop[i];
            const auto &b = loop[(i + 1) % loop.size()];
            total += std::hypot(b[0] - a[0], b[1] - a[1]);
        }
    }
    return total / 1000.0;   // mm -> m
}

QString num(double v, int decimals)
{
    return QString::number(v, 'f', decimals).replace(QLatin1Char('.'), QLatin1Char(','));
}

} // namespace

QuantityService::Summary QuantityService::compute(const QVector<Bar> &bars,
                                                  const QVector<Structura::App::ProjectData::SectionInfo> &sections,
                                                  const QVector<Structura::App::ProjectData::MaterialInfo> &materials)
{
    Summary summary;
    QMap<QString, Row> rows;
    QSet<QString> warned;
    for (const auto &bar : bars) {
        const Structura::App::ProjectData::SectionInfo *section = nullptr;
        const Structura::App::ProjectData::MaterialInfo *material = nullptr;
        for (const auto &s : sections) {
            if (s.uuid == bar.sectionId) {
                section = &s;
                break;
            }
        }
        for (const auto &m : materials) {
            if (m.uuid == bar.materialId) {
                material = &m;
                break;
            }
        }
        if (!section || !material) {
            const QString key = QStringLiteral("bar%1").arg(bar.id);
            if (!warned.contains(key)) {
                warned.insert(key);
                summary.warnings << QObject::tr("Barra %1 sem seção ou material: ignorada.").arg(bar.id);
            }
            continue;
        }
        // kN/m3 -> kg/m3 (ou N/m3 -> kg/m3 se o projeto estiver em N)
        const bool newtons = material->youngModulus > 1e10;
        const double kgPerM3 = material->unitWeight * (newtons ? 1.0 : 1000.0) / kGravity;
        if (material->unitWeight <= 0.0 && !warned.contains(material->name)) {
            warned.insert(material->name);
            summary.warnings << QObject::tr("Material \"%1\" sem peso específico: massa nula.").arg(material->name);
        }
        const QString key = section->name + QLatin1Char('|') + material->name;
        Row &row = rows[key];
        if (row.count == 0) {
            row.section = section->name;
            row.family = section->family;
            row.material = material->name;
            row.massPerMeter = section->area * kgPerM3;
        }
        ++row.count;
        row.totalLength += bar.length;
        row.totalMass += bar.length * row.massPerMeter;
        row.paintArea += bar.length * perimeterMeters(*section);
        row.bars.append(bar.id);
    }
    for (auto it = rows.begin(); it != rows.end(); ++it) {
        summary.rows.append(it.value());
        summary.totalLength += it->totalLength;
        summary.totalMass += it->totalMass;
        summary.totalPaintArea += it->paintArea;
        summary.barCount += it->count;
    }
    std::sort(summary.rows.begin(), summary.rows.end(), [](const Row &a, const Row &b) { return a.totalMass > b.totalMass; });
    return summary;
}

QString QuantityService::toCsv(const Summary &summary)
{
    QString out = QStringLiteral("Perfil;Familia;Material;Qtd;Comprimento total (m);Massa linear (kg/m);Massa total (kg);Area de pintura (m2)\r\n");
    for (const auto &r : summary.rows) {
        out += QStringLiteral("%1;%2;%3;%4;%5;%6;%7;%8\r\n")
                   .arg(r.section, r.family, r.material)
                   .arg(r.count)
                   .arg(num(r.totalLength, 2), num(r.massPerMeter, 2), num(r.totalMass, 1), num(r.paintArea, 1));
    }
    out += QStringLiteral("TOTAL;;;%1;%2;;%3;%4\r\n").arg(summary.barCount).arg(num(summary.totalLength, 2), num(summary.totalMass, 1), num(summary.totalPaintArea, 1));
    return out;
}

} // namespace Structura::App::Services
