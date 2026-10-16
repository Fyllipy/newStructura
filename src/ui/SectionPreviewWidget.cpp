#include "SectionPreviewWidget.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace Structura::UI {

using Structura::Visualization::SectionShape;
using Structura::Visualization::SectionShapes;

namespace {

QPainterPath shapePath(const SectionShape &shape, const QRectF &target, double margin, QPointF *centroid)
{
    const auto &b = shape.bounds;
    const double w = std::max(1e-6, b[1] - b[0]);
    const double h = std::max(1e-6, b[3] - b[2]);
    const double scale = std::min((target.width() - 2 * margin) / w, (target.height() - 2 * margin) / h);
    const double cx = (b[0] + b[1]) / 2.0;
    const double cy = (b[2] + b[3]) / 2.0;
    const QPointF middle = target.center();

    QPainterPath path;
    path.setFillRule(Qt::OddEvenFill);
    for (const auto &loop : shape.loops) {
        QPolygonF polygon;
        for (const auto &p : loop) {
            polygon << QPointF(middle.x() + (p[0] - cx) * scale, middle.y() - (p[1] - cy) * scale);
        }
        path.addPolygon(polygon);
        path.closeSubpath();
    }
    if (centroid) {
        *centroid = QPointF(middle.x() + (shape.centroid[0] - cx) * scale,
                            middle.y() - (shape.centroid[1] - cy) * scale);
    }
    return path;
}

} // namespace

SectionPreviewWidget::SectionPreviewWidget(QWidget *parent)
    : QWidget(parent)
    , m_placeholder(tr("Sem contorno para exibir"))
{
    setMinimumSize(200, 180);
}

bool SectionPreviewWidget::setSection(const QString &family,
                                      const SectionShapes::Parameters &parameters,
                                      bool mirrored,
                                      QString *error)
{
    m_shape.reset();
    bool ok = false;
    if (!family.isEmpty()) {
        QString message;
        if (auto shape = SectionShapes::build(family, parameters, mirrored, &message)) {
            m_shape = std::move(*shape);
            ok = true;
        } else if (error) {
            *error = message;
        }
    }
    update();
    return ok;
}

void SectionPreviewWidget::clear()
{
    m_shape.reset();
    update();
}

void SectionPreviewWidget::setPlaceholder(const QString &text)
{
    m_placeholder = text;
    update();
}

QPixmap SectionPreviewWidget::thumbnail(const QString &family,
                                        const SectionShapes::Parameters &parameters,
                                        const QSize &size,
                                        bool mirrored)
{
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    const auto shape = SectionShapes::build(family, parameters, mirrored);
    if (!shape) {
        return pixmap;
    }
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPainterPath path = shapePath(*shape, QRectF(QPointF(0, 0), QSizeF(size)), 3.0, nullptr);
    painter.setPen(QPen(QColor(QStringLiteral("#0d7fb9")), 1.2));
    painter.setBrush(QColor(13, 127, 185, 90));
    painter.drawPath(path);
    return pixmap;
}

void SectionPreviewWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(QStringLiteral("#f2f5fa")));
    painter.setPen(QColor(QStringLiteral("#cbd4e2")));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
    if (!m_shape) {
        painter.setPen(QColor(QStringLiteral("#6e7781")));
        painter.drawText(rect().adjusted(12, 12, -12, -12), Qt::AlignCenter | Qt::TextWordWrap, m_placeholder);
        return;
    }
    QPointF centroid;
    const QPainterPath path = shapePath(*m_shape, QRectF(rect()), 24.0, &centroid);
    painter.setPen(QPen(QColor(QStringLiteral("#0d7fb9")), 1.6));
    painter.setBrush(QColor(13, 127, 185, 70));
    painter.drawPath(path);
    painter.setPen(QPen(QColor(QStringLiteral("#f2992e")), 1.4));
    painter.drawLine(centroid + QPointF(-6, 0), centroid + QPointF(6, 0));
    painter.drawLine(centroid + QPointF(0, -6), centroid + QPointF(0, 6));
}

} // namespace Structura::UI
