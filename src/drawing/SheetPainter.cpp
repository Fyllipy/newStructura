#include "drawing/SheetPainter.h"

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QTextStream>

#include <cmath>
#include <map>

namespace Structura::Drawing {

namespace {

QPen penFor(const Layer &layer, double pxPerMm, double minPenPx)
{
    QPen pen(QColor(layer.rgb[0], layer.rgb[1], layer.rgb[2]));
    pen.setWidthF(std::max(minPenPx, layer.weight * pxPerMm));
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    switch (layer.style) {
    case LineStyle::Continuous: pen.setStyle(Qt::SolidLine); break;
    case LineStyle::Dashed: pen.setStyle(Qt::CustomDashLine); pen.setDashPattern({6.0, 3.0}); break;
    case LineStyle::DashDot: pen.setStyle(Qt::CustomDashLine); pen.setDashPattern({10.0, 2.5, 1.5, 2.5}); break;
    case LineStyle::Dotted: pen.setStyle(Qt::CustomDashLine); pen.setDashPattern({1.0, 3.0}); break;
    }
    return pen;
}

} // namespace

void paintSheet(QPainter &painter, const Sheet &sheet, double k, QPointF origin, double minPenPx)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    auto map = [&](const Point &p) { return QPointF(origin.x() + p.x * k, origin.y() + (sheet.height - p.y) * k); };
    // folha
    painter.fillRect(QRectF(origin, QSizeF(sheet.width * k, sheet.height * k)), Qt::white);
    for (const Entity &entity : sheet.entities) {
        if (const auto *l = std::get_if<Line>(&entity)) {
            const Layer &layer = sheet.layers[static_cast<std::size_t>(l->layer)];
            painter.setPen(penFor(layer, k, minPenPx));
            painter.drawLine(map(l->a), map(l->b));
        } else if (const auto *p = std::get_if<Polyline>(&entity)) {
            const Layer &layer = sheet.layers[static_cast<std::size_t>(p->layer)];
            QPolygonF polygon;
            for (const Point &pt : p->points) {
                polygon << map(pt);
            }
            painter.setPen(penFor(layer, k, minPenPx));
            if (p->filled && p->closed) {
                painter.setBrush(QColor(layer.rgb[0], layer.rgb[1], layer.rgb[2]));
            } else {
                painter.setBrush(Qt::NoBrush);
            }
            if (p->closed) {
                painter.drawPolygon(polygon);
            } else {
                painter.drawPolyline(polygon);
            }
        } else if (const auto *c = std::get_if<Circle>(&entity)) {
            const Layer &layer = sheet.layers[static_cast<std::size_t>(c->layer)];
            painter.setPen(penFor(layer, k, minPenPx));
            painter.setBrush(c->filled ? QBrush(QColor(layer.rgb[0], layer.rgb[1], layer.rgb[2])) : QBrush(Qt::NoBrush));
            painter.drawEllipse(map(c->center), c->radius * k, c->radius * k);
        } else if (const auto *t = std::get_if<Text>(&entity)) {
            const Layer &layer = sheet.layers[static_cast<std::size_t>(t->layer)];
            QFont font(QStringLiteral("Arial"));
            font.setPixelSize(std::max(1, static_cast<int>(std::lround(t->height * k * 1.35))));
            font.setBold(t->bold);
            painter.setFont(font);
            painter.setPen(QColor(layer.rgb[0], layer.rgb[1], layer.rgb[2]));
            const QString text = QString::fromStdString(t->text);
            const QFontMetricsF metrics(font);
            double dx = 0.0;
            if (t->align == TextAlign::Center) {
                dx = -metrics.horizontalAdvance(text) / 2.0;
            } else if (t->align == TextAlign::Right) {
                dx = -metrics.horizontalAdvance(text);
            }
            painter.save();
            painter.translate(map(t->position));
            painter.rotate(-t->rotationDeg);
            painter.drawText(QPointF(dx, 0.0), text);
            painter.restore();
        }
    }
    painter.restore();
}

QString sheetToDxf(const Sheet &sheet)
{
    QString out;
    QTextStream s(&out);
    auto pair = [&](int code, const QString &value) { s << code << "\r\n" << value << "\r\n"; };
    auto f = [](double v) { return QString::number(v, 'f', 4); };
    auto layerName = [&](int index) {
        QString name = QString::fromStdString(sheet.layers[static_cast<std::size_t>(index)].name).toUpper();
        QString clean;
        for (const QChar c : name.normalized(QString::NormalizationForm_D)) {
            if (c.unicode() < 128 && (c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('-'))) {
                clean += c;
            } else if (c == QLatin1Char(' ')) {
                clean += QLatin1Char('_');
            }
        }
        return clean.isEmpty() ? QStringLiteral("0") : clean.left(31);
    };
    auto ascii = [](const QString &text) {
        QString clean;
        for (const QChar c : text.normalized(QString::NormalizationForm_D)) {
            if (c.unicode() < 128) {
                clean += c;
            } else if (c.category() != QChar::Mark_NonSpacing) {
                clean += QLatin1Char('?');
            }
        }
        return clean;
    };
    auto aciColor = [](const std::array<int, 3> &rgb) {
        // cor ACI aproximada
        const int r = rgb[0], g = rgb[1], b = rgb[2];
        if (r > 200 && g < 80 && b < 80) return 1;
        if (r > 200 && g > 200 && b < 80) return 2;
        if (r < 80 && g > 150 && b < 80) return 3;
        if (r < 80 && g > 150 && b > 150) return 4;
        if (r < 80 && g < 80 && b > 150) return 5;
        if (r > 150 && g < 80 && b > 150) return 6;
        if (r > 100 && g > 100 && b > 100) return 8;
        return 7;
    };

    pair(0, QStringLiteral("SECTION"));
    pair(2, QStringLiteral("HEADER"));
    pair(9, QStringLiteral("$ACADVER"));
    pair(1, QStringLiteral("AC1009"));
    pair(9, QStringLiteral("$INSUNITS"));
    pair(70, QStringLiteral("4"));
    pair(0, QStringLiteral("ENDSEC"));
    pair(0, QStringLiteral("SECTION"));
    pair(2, QStringLiteral("TABLES"));
    pair(0, QStringLiteral("TABLE"));
    pair(2, QStringLiteral("LAYER"));
    pair(70, QString::number(static_cast<int>(sheet.layers.size())));
    for (std::size_t i = 0; i < sheet.layers.size(); ++i) {
        const Layer &layer = sheet.layers[i];
        pair(0, QStringLiteral("LAYER"));
        pair(2, layerName(static_cast<int>(i)));
        pair(70, QStringLiteral("0"));
        pair(62, QString::number(aciColor(layer.rgb)));
        pair(6, layer.style == LineStyle::Continuous ? QStringLiteral("CONTINUOUS") : QStringLiteral("CONTINUOUS"));
    }
    pair(0, QStringLiteral("ENDTAB"));
    pair(0, QStringLiteral("ENDSEC"));
    pair(0, QStringLiteral("SECTION"));
    pair(2, QStringLiteral("ENTITIES"));
    auto emitLine = [&](const Point &a, const Point &b, int layer) {
        pair(0, QStringLiteral("LINE"));
        pair(8, layerName(layer));
        pair(10, f(a.x));
        pair(20, f(a.y));
        pair(30, QStringLiteral("0"));
        pair(11, f(b.x));
        pair(21, f(b.y));
        pair(31, QStringLiteral("0"));
    };
    for (const Entity &entity : sheet.entities) {
        if (const auto *l = std::get_if<Line>(&entity)) {
            emitLine(l->a, l->b, l->layer);
        } else if (const auto *p = std::get_if<Polyline>(&entity)) {
            for (std::size_t i = 0; i + 1 < p->points.size(); ++i) {
                emitLine(p->points[i], p->points[i + 1], p->layer);
            }
            if (p->closed && p->points.size() > 2) {
                emitLine(p->points.back(), p->points.front(), p->layer);
            }
        } else if (const auto *c = std::get_if<Circle>(&entity)) {
            pair(0, QStringLiteral("CIRCLE"));
            pair(8, layerName(c->layer));
            pair(10, f(c->center.x));
            pair(20, f(c->center.y));
            pair(30, QStringLiteral("0"));
            pair(40, f(c->radius));
        } else if (const auto *t = std::get_if<Text>(&entity)) {
            pair(0, QStringLiteral("TEXT"));
            pair(8, layerName(t->layer));
            pair(10, f(t->position.x));
            pair(20, f(t->position.y));
            pair(30, QStringLiteral("0"));
            pair(40, f(t->height));
            pair(1, ascii(QString::fromStdString(t->text)));
            if (t->rotationDeg != 0.0) {
                pair(50, f(t->rotationDeg));
            }
            if (t->align != TextAlign::Left) {
                pair(72, t->align == TextAlign::Center ? QStringLiteral("1") : QStringLiteral("2"));
                pair(11, f(t->position.x));
                pair(21, f(t->position.y));
                pair(31, QStringLiteral("0"));
            }
        }
    }
    pair(0, QStringLiteral("ENDSEC"));
    pair(0, QStringLiteral("EOF"));
    return out;
}

} // namespace Structura::Drawing
