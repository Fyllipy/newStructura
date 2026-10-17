#include "LineChartWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace Structura::UI {

namespace {

double niceStep(double span, int targetTicks)
{
    if (span <= 0.0) {
        return 1.0;
    }
    const double raw = span / std::max(1, targetTicks);
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    const double norm = raw / mag;
    const double step = norm < 1.5 ? 1.0 : norm < 3.5 ? 2.0 : norm < 7.5 ? 5.0 : 10.0;
    return step * mag;
}

QString formatTick(double v, double step)
{
    const int decimals = std::clamp(static_cast<int>(std::ceil(-std::log10(step))) + 0, 0, 6);
    if (std::abs(v) >= 1e5 || (std::abs(v) < 1e-3 && v != 0.0)) {
        return QString::number(v, 'g', 3);
    }
    return QString::number(v, 'f', decimals);
}

} // namespace

LineChartWidget::LineChartWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(360, 220);
    setMouseTracking(true);
}

void LineChartWidget::setTitles(const QString &title, const QString &xLabel, const QString &yLabel)
{
    m_title = title;
    m_xLabel = xLabel;
    m_yLabel = yLabel;
    update();
}

void LineChartWidget::setSeries(const QVector<Series> &series)
{
    m_series = series;
    computeRanges();
    update();
}

void LineChartWidget::clear()
{
    m_series.clear();
    m_markers.clear();
    update();
}

void LineChartWidget::setVerticalMarkers(const QVector<double> &x)
{
    m_markers = x;
    computeRanges();
    update();
}

QRectF LineChartWidget::plotRect() const
{
    return QRectF(58, m_title.isEmpty() ? 12 : 30, width() - 58 - 14, height() - (m_title.isEmpty() ? 12 : 30) - 44);
}

void LineChartWidget::computeRanges()
{
    double xlo = 1e300, xhi = -1e300, ylo = 1e300, yhi = -1e300;
    for (const auto &s : m_series) {
        for (const QPointF &p : s.points) {
            xlo = std::min(xlo, p.x());
            xhi = std::max(xhi, p.x());
            ylo = std::min(ylo, p.y());
            yhi = std::max(yhi, p.y());
        }
    }
    if (xlo > xhi) {
        xlo = 0.0;
        xhi = 1.0;
        ylo = 0.0;
        yhi = 1.0;
    }
    for (const double m : m_markers) {
        xlo = std::min(xlo, m);
        xhi = std::max(xhi, m);
    }
    if (ylo > 0.0) {
        ylo = std::min(ylo, 0.0);
    }
    if (yhi < 0.0) {
        yhi = 0.0;
    }
    if (xhi - xlo < 1e-12) {
        xhi = xlo + 1.0;
    }
    if (yhi - ylo < 1e-12) {
        yhi = ylo + 1.0;
    }
    const double pad = 0.05 * (yhi - ylo);
    m_x = {xlo, xhi};
    m_y = {ylo < 0.0 ? ylo - pad : ylo, yhi + pad};
}

QPointF LineChartWidget::toScreen(const QPointF &p) const
{
    const QRectF r = plotRect();
    return {r.left() + (p.x() - m_x.lo) / (m_x.hi - m_x.lo) * r.width(), r.bottom() - (p.y() - m_y.lo) / (m_y.hi - m_y.lo) * r.height()};
}

void LineChartWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), palette().window());
    const QRectF r = plotRect();
    painter.fillRect(r, QColor(QStringLiteral("#fbfcfe")));
    const QColor text = palette().text().color();
    QFont small = font();
    small.setPointSizeF(std::max(7.0, font().pointSizeF() - 1.0));
    painter.setFont(small);

    // grade e graduacao
    const double sx = niceStep(m_x.hi - m_x.lo, 8), sy = niceStep(m_y.hi - m_y.lo, 6);
    painter.setPen(QPen(QColor(220, 225, 232), 1));
    for (double x = std::ceil(m_x.lo / sx) * sx; x <= m_x.hi + 1e-9; x += sx) {
        const double px = toScreen({x, 0}).x();
        painter.drawLine(QPointF(px, r.top()), QPointF(px, r.bottom()));
    }
    for (double y = std::ceil(m_y.lo / sy) * sy; y <= m_y.hi + 1e-9; y += sy) {
        const double py = toScreen({0, y}).y();
        painter.drawLine(QPointF(r.left(), py), QPointF(r.right(), py));
    }
    painter.setPen(text);
    for (double x = std::ceil(m_x.lo / sx) * sx; x <= m_x.hi + 1e-9; x += sx) {
        const double px = toScreen({x, 0}).x();
        painter.drawText(QRectF(px - 30, r.bottom() + 3, 60, 14), Qt::AlignCenter, formatTick(x, sx));
    }
    for (double y = std::ceil(m_y.lo / sy) * sy; y <= m_y.hi + 1e-9; y += sy) {
        const double py = toScreen({0, y}).y();
        painter.drawText(QRectF(0, py - 8, r.left() - 5, 16), Qt::AlignRight | Qt::AlignVCenter, formatTick(y, sy));
    }
    // eixo zero
    if (m_y.lo < 0.0 && m_y.hi > 0.0) {
        painter.setPen(QPen(QColor(120, 130, 145), 1));
        const double py = toScreen({0, 0}).y();
        painter.drawLine(QPointF(r.left(), py), QPointF(r.right(), py));
    }
    painter.setPen(QPen(QColor(90, 100, 115), 1));
    painter.drawRect(r);

    painter.save();
    painter.setClipRect(r.adjusted(-1, -1, 1, 1));
    for (const double m : m_markers) {
        painter.setPen(QPen(QColor(200, 120, 40, 150), 1, Qt::DashLine));
        const double px = toScreen({m, 0}).x();
        painter.drawLine(QPointF(px, r.top()), QPointF(px, r.bottom()));
    }
    for (const auto &s : m_series) {
        QPen pen(s.color, 1.8);
        if (s.dashed) {
            pen.setStyle(Qt::DashLine);
        }
        painter.setPen(pen);
        if (s.markers) {
            painter.setBrush(s.color);
            for (const QPointF &p : s.points) {
                painter.drawEllipse(toScreen(p), 3.5, 3.5);
            }
            painter.setBrush(Qt::NoBrush);
        } else if (s.points.size() > 1) {
            QPainterPath path;
            path.moveTo(toScreen(s.points.front()));
            // reduz a quantidade de segmentos para series longas
            const int stride = std::max<int>(1, s.points.size() / static_cast<int>(std::max(1.0, r.width() * 2.0)));
            for (int i = stride; i < s.points.size(); i += stride) {
                path.lineTo(toScreen(s.points[i]));
            }
            path.lineTo(toScreen(s.points.back()));
            painter.drawPath(path);
        }
    }
    painter.restore();

    painter.setPen(text);
    if (!m_title.isEmpty()) {
        QFont bold = small;
        bold.setBold(true);
        painter.setFont(bold);
        painter.drawText(QRectF(r.left(), 4, r.width(), 20), Qt::AlignLeft | Qt::AlignVCenter, m_title);
        painter.setFont(small);
    }
    painter.drawText(QRectF(r.left(), height() - 18, r.width(), 16), Qt::AlignCenter, m_xLabel);
    painter.save();
    painter.translate(12, r.center().y());
    painter.rotate(-90);
    painter.drawText(QRectF(-r.height() / 2, -8, r.height(), 16), Qt::AlignCenter, m_yLabel);
    painter.restore();

    // legenda
    double lx = r.right() - 6;
    for (int i = m_series.size() - 1; i >= 0; --i) {
        const auto &s = m_series[i];
        if (s.name.isEmpty()) {
            continue;
        }
        const double w = QFontMetricsF(small).horizontalAdvance(s.name) + 24;
        lx -= w;
        painter.setPen(QPen(s.color, 2));
        painter.drawLine(QPointF(lx, r.top() + 10), QPointF(lx + 14, r.top() + 10));
        painter.setPen(text);
        painter.drawText(QPointF(lx + 18, r.top() + 14), s.name);
    }

    // leitura sob o mouse: ponto mais proximo em x da serie sob o cursor
    if (m_hasHover && r.contains(m_hover)) {
        double hx = m_x.lo + (m_hover.x() - r.left()) / r.width() * (m_x.hi - m_x.lo);
        QStringList lines;
        lines << QStringLiteral("x = %1").arg(hx, 0, 'g', 5);
        for (const auto &s : m_series) {
            if (s.points.isEmpty()) {
                continue;
            }
            const QPointF *best = &s.points.front();
            for (const QPointF &p : s.points) {
                if (std::abs(p.x() - hx) < std::abs(best->x() - hx)) {
                    best = &p;
                }
            }
            lines << QStringLiteral("%1: %2").arg(s.name.isEmpty() ? QStringLiteral("y") : s.name).arg(best->y(), 0, 'g', 5);
        }
        painter.setPen(QPen(QColor(90, 100, 115, 160), 1, Qt::DotLine));
        painter.drawLine(QPointF(m_hover.x(), r.top()), QPointF(m_hover.x(), r.bottom()));
        const QFontMetricsF fm(small);
        double w = 0.0;
        for (const QString &l : lines) {
            w = std::max(w, fm.horizontalAdvance(l));
        }
        QRectF box(m_hover.x() + 10, m_hover.y() + 10, w + 12, lines.size() * fm.height() + 8);
        if (box.right() > r.right()) {
            box.moveLeft(m_hover.x() - 10 - box.width());
        }
        if (box.bottom() > r.bottom()) {
            box.moveTop(r.bottom() - box.height());
        }
        painter.setPen(QPen(QColor(90, 100, 115), 1));
        painter.setBrush(QColor(255, 255, 255, 235));
        painter.drawRoundedRect(box, 3, 3);
        painter.setPen(text);
        for (int i = 0; i < lines.size(); ++i) {
            painter.drawText(QPointF(box.left() + 6, box.top() + 4 + (i + 1) * fm.height() - fm.descent()), lines[i]);
        }
    }
}

void LineChartWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_hover = event->position();
    m_hasHover = true;
    update();
}

void LineChartWidget::leaveEvent(QEvent *)
{
    m_hasHover = false;
    update();
}

} // namespace Structura::UI
