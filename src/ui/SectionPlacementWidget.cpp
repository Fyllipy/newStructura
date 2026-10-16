#include "SectionPlacementWidget.h"

#include "viz/structure/SectionShapes.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

namespace Structura::UI {

namespace {

constexpr double kPi = 3.14159265358979323846;
const QColor kAccent(0x0d, 0x7f, 0xb9);
const QColor kOrange(0xf2, 0x99, 0x2e);
const QColor kInk(0x1e, 0x23, 0x2b);
const QColor kMuted(0x5a, 0x65, 0x74);

} // namespace

SectionPlacementWidget::SectionPlacementWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(160, 140);
}

QSize SectionPlacementWidget::sizeHint() const
{
    return m_compact ? QSize(190, 150) : QSize(330, 250);
}

QSize SectionPlacementWidget::minimumSizeHint() const
{
    return m_compact ? QSize(120, 100) : QSize(240, 190);
}

void SectionPlacementWidget::setInteractive(bool interactive)
{
    m_interactive = interactive;
    setCursor(interactive ? Qt::ArrowCursor : Qt::ArrowCursor);
    update();
}

void SectionPlacementWidget::setCompact(bool compact)
{
    m_compact = compact;
    updateGeometry();
    update();
}

void SectionPlacementWidget::setSection(const SectionInfo &section)
{
    m_section = section;
    rebuildGeometry();
    updateScale(true);
    update();
}

void SectionPlacementWidget::clearSection()
{
    m_section = SectionInfo();
    m_geometry = Geometry();
    update();
}

void SectionPlacementWidget::setPlacement(double rotationDeg, int reference, double offsetA, double offsetB)
{
    if (m_dragging) {
        return;
    }
    m_rotationDeg = rotationDeg;
    m_reference = std::clamp(reference, 0, 8);
    m_offsetA = offsetA;
    m_offsetB = offsetB;
    updateScale(true);
    update();
}

void SectionPlacementWidget::rebuildGeometry()
{
    m_geometry = Geometry();
    if (m_section.family.isEmpty()) {
        return;
    }
    const auto shape = Structura::Visualization::SectionShapes::build(m_section.family, m_section.geometry, m_section.mirrored);
    if (!shape) {
        return;
    }
    constexpr double kMmToM = 1e-3;
    for (const auto &loop : shape->loops) {
        QPolygonF polygon;
        for (const auto &point : loop) {
            polygon << QPointF(point[0] * kMmToM, point[1] * kMmToM);
        }
        m_geometry.loops.append(polygon);
    }
    m_geometry.minA = shape->bounds[0] * kMmToM;
    m_geometry.maxA = shape->bounds[1] * kMmToM;
    m_geometry.minB = shape->bounds[2] * kMmToM;
    m_geometry.maxB = shape->bounds[3] * kMmToM;
    m_geometry.valid = !m_geometry.loops.isEmpty();
}

QPointF SectionPlacementWidget::referenceCoordinate(int reference) const
{
    const auto &g = m_geometry;
    switch (reference) {
    case 1: return {g.minA, g.minB};
    case 2: return {g.maxA, g.minB};
    case 3: return {g.minA, g.maxB};
    case 4: return {g.maxA, g.maxB};
    case 5: return {0.0, g.minB};
    case 6: return {g.maxA, 0.0};
    case 7: return {0.0, g.maxB};
    case 8: return {g.minA, 0.0};
    default: return {0.0, 0.0};
    }
}

void SectionPlacementWidget::updateScale(bool force)
{
    if (m_scaleFrozen && !force) {
        return;
    }
    if (!m_geometry.valid) {
        m_scale = 1.0;
        return;
    }
    // Raio (m) do que precisa caber centrado no no: todos os cantos da secao na posicao atual + o proprio no.
    double radius = 0.02;
    const QPointF ref = referenceCoordinate(m_reference);
    const double corners[4][2] = {{m_geometry.minA, m_geometry.minB}, {m_geometry.maxA, m_geometry.minB},
                                  {m_geometry.minA, m_geometry.maxB}, {m_geometry.maxA, m_geometry.maxB}};
    const double c = std::cos(m_rotationDeg * kPi / 180.0);
    const double s = std::sin(m_rotationDeg * kPi / 180.0);
    for (const auto &corner : corners) {
        const double a = corner[0] - ref.x() + m_offsetA;
        const double b = corner[1] - ref.y() + m_offsetB;
        const double x = a * c + b * s;
        const double y = -a * s + b * c;
        radius = std::max(radius, std::hypot(x, y));
    }
    const double margin = m_compact ? 6.0 : 26.0;
    const double available = std::max(20.0, std::min(width(), height()) / 2.0 - margin);
    m_scale = available / radius;
}

void SectionPlacementWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateScale(true);
}

QPointF SectionPlacementWidget::toScreen(const QPointF &p) const
{
    return QPointF(width() / 2.0 + p.x() * m_scale, height() / 2.0 - p.y() * m_scale);
}

QPointF SectionPlacementWidget::sectionToScreen(double a, double b) const
{
    const QPointF ref = referenceCoordinate(m_reference);
    const double ua = a - ref.x() + m_offsetA;
    const double ub = b - ref.y() + m_offsetB;
    const double c = std::cos(m_rotationDeg * kPi / 180.0);
    const double s = std::sin(m_rotationDeg * kPi / 180.0);
    return toScreen(QPointF(ua * c + ub * s, -ua * s + ub * c));
}

int SectionPlacementWidget::handleAt(const QPointF &pos) const
{
    if (!m_geometry.valid) {
        return -1;
    }
    int best = -1;
    double bestDistance = 11.0;
    for (int i = 0; i < 9; ++i) {
        const QPointF p = referenceCoordinate(i);
        const double distance = QLineF(sectionToScreen(p.x(), p.y()), pos).length();
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

bool SectionPlacementWidget::insideSection(const QPointF &pos) const
{
    if (!m_geometry.valid) {
        return false;
    }
    QPainterPath path;
    path.setFillRule(Qt::OddEvenFill);
    for (const auto &loop : m_geometry.loops) {
        QPolygonF screen;
        for (const QPointF &point : loop) {
            screen << sectionToScreen(point.x(), point.y());
        }
        path.addPolygon(screen);
        path.closeSubpath();
    }
    return path.contains(pos);
}

void SectionPlacementWidget::drawArrow(QPainter &painter, const QPointF &from, const QPointF &to, const QColor &color, const QString &label) const
{
    painter.setPen(QPen(color, 1.5));
    painter.drawLine(from, to);
    const QLineF line(from, to);
    const double angle = std::atan2(-line.dy(), line.dx());
    const double size = 6.0;
    QPolygonF head;
    head << to
         << QPointF(to.x() - size * std::cos(angle - 0.4), to.y() + size * std::sin(angle - 0.4))
         << QPointF(to.x() - size * std::cos(angle + 0.4), to.y() + size * std::sin(angle + 0.4));
    painter.setBrush(color);
    painter.drawPolygon(head);
    painter.setBrush(Qt::NoBrush);
    if (!label.isEmpty()) {
        painter.drawText(QPointF(to.x() + 4, to.y() - 2), label);
    }
}

void SectionPlacementWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Fundo e moldura
    painter.setPen(QPen(QColor(0xcb, 0xd4, 0xe2), 1.0));
    painter.setBrush(QColor(0xff, 0xff, 0xff));
    painter.drawRoundedRect(QRectF(0.5, 0.5, width() - 1.0, height() - 1.0), 6, 6);

    if (!m_geometry.valid) {
        painter.setPen(kMuted);
        painter.drawText(rect().adjusted(10, 10, -10, -10), Qt::AlignCenter | Qt::TextWordWrap,
                         m_section.name.isEmpty() ? tr("Selecione uma barra com seção definida.")
                                                  : tr("A seção \"%1\" não tem geometria paramétrica:\nso o deslocamento adicional se aplica.").arg(m_section.name));
        return;
    }

    const QPointF origin = toScreen(QPointF(0.0, 0.0));

    // Reticulado de referencia no no (eixos y0/z0 fixos)
    painter.setPen(QPen(QColor(0xe3, 0xe8, 0xf0), 1.0));
    painter.drawLine(QPointF(8, origin.y()), QPointF(width() - 8, origin.y()));
    painter.drawLine(QPointF(origin.x(), 8), QPointF(origin.x(), height() - 8));

    // Contorno da secao
    QPainterPath section;
    section.setFillRule(Qt::OddEvenFill);
    for (const auto &loop : m_geometry.loops) {
        QPolygonF screen;
        for (const QPointF &point : loop) {
            screen << sectionToScreen(point.x(), point.y());
        }
        section.addPolygon(screen);
        section.closeSubpath();
    }
    painter.setPen(QPen(kAccent, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(0x13, 0x93, 0xd6, 70));
    painter.drawPath(section);

    // Retangulo envolvente
    const QPointF c1 = sectionToScreen(m_geometry.minA, m_geometry.minB);
    const QPointF c2 = sectionToScreen(m_geometry.maxA, m_geometry.minB);
    const QPointF c3 = sectionToScreen(m_geometry.maxA, m_geometry.maxB);
    const QPointF c4 = sectionToScreen(m_geometry.minA, m_geometry.maxB);
    painter.setPen(QPen(kMuted, 1.0, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawPolygon(QPolygonF({c1, c2, c3, c4}));

    // Centroide
    const QPointF centroid = sectionToScreen(0.0, 0.0);
    painter.setPen(QPen(kAccent, 1.4));
    painter.drawLine(centroid + QPointF(-5, 0), centroid + QPointF(5, 0));
    painter.drawLine(centroid + QPointF(0, -5), centroid + QPointF(0, 5));

    // Pontos de referencia
    const bool showHandles = m_interactive && !m_compact;
    for (int i = 0; i < 9; ++i) {
        const QPointF p = referenceCoordinate(i);
        const QPointF screen = sectionToScreen(p.x(), p.y());
        const bool selected = (i == m_reference);
        if (!showHandles && !selected) {
            continue;
        }
        const bool hover = (i == m_hoverHandle);
        painter.setPen(QPen(selected ? kOrange.darker(115) : kMuted, 1.2));
        painter.setBrush(selected ? kOrange : (hover ? QColor(0xff, 0xe2, 0xb8) : QColor(0xff, 0xff, 0xff)));
        const double radius = selected ? 5.0 : (hover ? 5.0 : 3.5);
        painter.drawEllipse(screen, radius, radius);
    }

    // No (alvo) no centro
    painter.setPen(QPen(kOrange.darker(120), 1.8));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(origin, 7.0, 7.0);
    painter.drawLine(origin + QPointF(-10, 0), origin + QPointF(10, 0));
    painter.drawLine(origin + QPointF(0, -10), origin + QPointF(0, 10));

    if (!m_compact) {
        QFont font = painter.font();
        font.setPointSizeF(8.0);
        painter.setFont(font);
        // Eixos de referencia fixos (y0 para cima, z0 para a direita)
        const QPointF corner(18.0, height() - 16.0);
        drawArrow(painter, corner, corner + QPointF(26, 0), kMuted, QStringLiteral("z"));
        drawArrow(painter, corner, corner + QPointF(0, -26), kMuted, QStringLiteral("y"));
        painter.setPen(kMuted);
        painter.drawText(QRectF(8, 6, width() - 16, 14), Qt::AlignRight | Qt::AlignVCenter,
                         tr("vista do nó I para o nó J"));
        painter.setPen(kOrange.darker(130));
        painter.drawText(origin + QPointF(10, -9), tr("nó"));
    }
}

void SectionPlacementWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_interactive || event->button() != Qt::LeftButton || !m_geometry.valid) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QPointF pos = event->position();
    const int handle = handleAt(pos);
    if (handle >= 0) {
        if (handle != m_reference) {
            m_reference = handle;
            update();
            emit referenceChanged(handle);
        }
        return;
    }
    if (insideSection(pos)) {
        m_dragging = true;
        m_scaleFrozen = true;
        m_dragStart = pos;
        m_dragOffsetA0 = m_offsetA;
        m_dragOffsetB0 = m_offsetB;
        setCursor(Qt::ClosedHandCursor);
    }
}

void SectionPlacementWidget::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF pos = event->position();
    if (m_dragging) {
        const double dxs = (pos.x() - m_dragStart.x()) / m_scale;
        const double dys = -(pos.y() - m_dragStart.y()) / m_scale;
        const double c = std::cos(m_rotationDeg * kPi / 180.0);
        const double s = std::sin(m_rotationDeg * kPi / 180.0);
        const double da = dxs * c - dys * s;     // R(beta)^-1 aplicado ao deslocamento do mouse
        const double db = dxs * s + dys * c;
        auto snap = [](double value) { return std::round(value * 1000.0) / 1000.0; };
        m_offsetA = snap(m_dragOffsetA0 + da);
        m_offsetB = snap(m_dragOffsetB0 + db);
        update();
        emit offsetChanged(m_offsetA, m_offsetB);
        return;
    }
    if (!m_interactive) {
        return;
    }
    const int handle = handleAt(pos);
    if (handle != m_hoverHandle) {
        m_hoverHandle = handle;
        update();
    }
    setCursor(handle >= 0 ? Qt::PointingHandCursor : (insideSection(pos) ? Qt::OpenHandCursor : Qt::ArrowCursor));
}

void SectionPlacementWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        m_scaleFrozen = false;
        updateScale(true);
        setCursor(Qt::ArrowCursor);
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void SectionPlacementWidget::leaveEvent(QEvent *event)
{
    if (m_hoverHandle != -1) {
        m_hoverHandle = -1;
        update();
    }
    QWidget::leaveEvent(event);
}

} // namespace Structura::UI
