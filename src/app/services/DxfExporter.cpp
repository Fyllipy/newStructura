#include "DxfExporter.h"

#include <QSet>
#include <QTextStream>

#include <cmath>
#include <map>

namespace Structura::App::Services {

namespace {

struct P {
    double x, y, z;
};

P project(double x, double y, double z, DxfExporter::View view)
{
    switch (view) {
    case DxfExporter::View::PlanXY: return {x, y, 0.0};
    case DxfExporter::View::ElevationXZ: return {x, z, 0.0};
    case DxfExporter::View::ElevationYZ: return {y, z, 0.0};
    case DxfExporter::View::Model3D: break;
    }
    return {x, y, z};
}

QString f(double v)
{
    return QString::number(v, 'f', 6);
}

QString ascii(const QString &text)
{
    // DXF R12 e ASCII: remove acentos e simbolos nao ASCII
    QString out = text.normalized(QString::NormalizationForm_D);
    QString clean;
    for (const QChar c : out) {
        if (c.unicode() < 128) {
            clean += c;
        } else if (c.category() != QChar::Mark_NonSpacing) {
            clean += QLatin1Char('?');
        }
    }
    return clean;
}

} // namespace

QString DxfExporter::layerName(const QString &text)
{
    QString name = ascii(text).toUpper();
    for (QChar &c : name) {
        if (!(c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('-') || c == QLatin1Char('$'))) {
            c = QLatin1Char('_');
        }
    }
    return name.isEmpty() ? QStringLiteral("0") : name.left(31);
}

QString DxfExporter::build(const QVector<Node> &nodes, const QVector<Bar> &bars, const Options &o)
{
    QString out;
    QTextStream s(&out);
    auto pair = [&](int code, const QString &value) { s << code << "\r\n" << value << "\r\n"; };

    // camadas
    std::map<QString, int> layers;   // nome -> cor
    int color = 1;
    for (const auto &b : bars) {
        const QString layer = b.tie ? QStringLiteral("TIRANTES") : layerName(b.section);
        if (!layers.count(layer)) {
            layers[layer] = (color++ % 7) + 1;
        }
    }
    layers["ROTULOS"] = 7;
    layers["NOS"] = 8;

    pair(0, QStringLiteral("SECTION"));
    pair(2, QStringLiteral("HEADER"));
    pair(9, QStringLiteral("$ACADVER"));
    pair(1, QStringLiteral("AC1009"));
    pair(9, QStringLiteral("$INSUNITS"));
    pair(70, QStringLiteral("6"));
    pair(0, QStringLiteral("ENDSEC"));

    pair(0, QStringLiteral("SECTION"));
    pair(2, QStringLiteral("TABLES"));
    pair(0, QStringLiteral("TABLE"));
    pair(2, QStringLiteral("LAYER"));
    pair(70, QString::number(static_cast<int>(layers.size())));
    for (const auto &[name, col] : layers) {
        pair(0, QStringLiteral("LAYER"));
        pair(2, name);
        pair(70, QStringLiteral("0"));
        pair(62, QString::number(col));
        pair(6, QStringLiteral("CONTINUOUS"));
    }
    pair(0, QStringLiteral("ENDTAB"));
    pair(0, QStringLiteral("ENDSEC"));

    pair(0, QStringLiteral("SECTION"));
    pair(2, QStringLiteral("ENTITIES"));

    auto text = [&](const QString &layer, const P &p, const QString &value) {
        pair(0, QStringLiteral("TEXT"));
        pair(8, layer);
        pair(10, f(p.x));
        pair(20, f(p.y));
        pair(30, f(p.z));
        pair(40, f(o.textHeight));
        pair(1, ascii(value));
    };

    for (const auto &b : bars) {
        const QString layer = b.tie ? QStringLiteral("TIRANTES") : layerName(b.section);
        const P a = project(b.x1, b.y1, b.z1, o.view);
        const P c = project(b.x2, b.y2, b.z2, o.view);
        pair(0, QStringLiteral("LINE"));
        pair(8, layer);
        pair(10, f(a.x));
        pair(20, f(a.y));
        pair(30, f(a.z));
        pair(11, f(c.x));
        pair(21, f(c.y));
        pair(31, f(c.z));
        if (o.barLabels || o.sectionLabels) {
            const P m {(a.x + c.x) / 2.0, (a.y + c.y) / 2.0, (a.z + c.z) / 2.0};
            QString label;
            if (o.barLabels) {
                label = QStringLiteral("B%1").arg(b.id);
            }
            if (o.sectionLabels) {
                label += (label.isEmpty() ? QString() : QStringLiteral(" ")) + b.section;
            }
            text(QStringLiteral("ROTULOS"), {m.x, m.y + o.textHeight * 0.3, m.z}, label);
        }
    }
    for (const auto &n : nodes) {
        const P p = project(n.x, n.y, n.z, o.view);
        pair(0, QStringLiteral("POINT"));
        pair(8, QStringLiteral("NOS"));
        pair(10, f(p.x));
        pair(20, f(p.y));
        pair(30, f(p.z));
        if (o.nodeLabels) {
            text(QStringLiteral("NOS"), {p.x + o.textHeight * 0.3, p.y + o.textHeight * 0.3, p.z}, QStringLiteral("N%1").arg(n.id));
        }
    }
    pair(0, QStringLiteral("ENDSEC"));
    pair(0, QStringLiteral("EOF"));
    return out;
}

} // namespace Structura::App::Services
