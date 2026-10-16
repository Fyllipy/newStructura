#include "PlacementOverlay.h"

#include <QFont>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QVector4D>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace Structura::UI {

// ── SnapMarkerWidget ──────────────────────────────────────────────────────

SnapMarkerWidget::SnapMarkerWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(22, 22);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    hide();
}

void SnapMarkerWidget::setKind(Kind kind)
{
    if (kind == m_kind) {
        return;
    }
    m_kind = kind;
    update();
}

void SnapMarkerWidget::placeAt(const QPointF &screen)
{
    move(static_cast<int>(std::lround(screen.x() - width() / 2.0)),
         static_cast<int>(std::lround(screen.y() - height() / 2.0)));
    show();
    raise();
}

void SnapMarkerWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const double scale = width() / 24.0;
    painter.scale(scale, scale);
    painter.setBrush(Qt::NoBrush);

    // Cada marcador e desenhado duas vezes: um contorno branco (legibilidade
    // sobre qualquer fundo) e o tracado azul do software por cima.
    auto draw = [&](auto &&shape) {
        QPen halo(Qt::white);
        halo.setWidthF(4.6);
        halo.setCapStyle(Qt::RoundCap);
        halo.setJoinStyle(Qt::RoundJoin);
        painter.setPen(halo);
        shape();
        QPen pen(QColor(QStringLiteral("#0d7fb9")));
        pen.setWidthF(2.2);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        shape();
    };

    switch (m_kind) {
    case Kind::Grid:
        draw([&]() {
            painter.drawEllipse(QPointF(12, 12), 5.5, 5.5);
            painter.drawLine(QPointF(12, 2.5), QPointF(12, 21.5));
            painter.drawLine(QPointF(2.5, 12), QPointF(21.5, 12));
        });
        break;
    case Kind::Endpoint:
        draw([&]() {
            painter.drawRect(QRectF(5, 5, 14, 14));
            painter.drawPoint(QPointF(12, 12));
        });
        break;
    case Kind::Center:
        draw([&]() {
            QPolygonF diamond;
            diamond << QPointF(12, 3.5) << QPointF(20.5, 12) << QPointF(12, 20.5) << QPointF(3.5, 12);
            painter.drawPolygon(diamond);
        });
        break;
    }
}

// ── CoordinateReadoutWidget ───────────────────────────────────────────────

CoordinateReadoutWidget::CoordinateReadoutWidget(QWidget *parent)
    : QObject(parent)
    , m_parent(parent)
    , m_frame(new QFrame(parent))
    , m_angleFrame(new QFrame(parent))
{
    const QString frameStyle = QStringLiteral(
        "QFrame#placementReadout, QFrame#placementAngle { background: #ffffff; border: 1px solid #cbd4e2;"
        " border-left: 4px solid #0d7fb9; border-radius: 6px; }"
        "QLabel { color: #1e232b; background: transparent; border: 0; padding: 0; font-size: 11px; }"
        "QLabel#axisBadgeX, QLabel#axisBadgeY, QLabel#axisBadgeZ { color: #ffffff; font-weight: 700; border-radius: 3px; }"
        "QLabel#axisBadgeX { background: #d64545; }"
        "QLabel#axisBadgeY { background: #2e9c5a; }"
        "QLabel#axisBadgeZ { background: #0d7fb9; }"
        "QLabel#angleBadge { color: #0d7fb9; font-weight: 700; font-size: 13px; }"
        "QLineEdit { color: #1e232b; background: transparent; border: 1px solid transparent; border-radius: 3px; padding: 0 2px; font-size: 11px; }"
        "QLineEdit:!read-only { background: #e6f2f9; border: 1px solid #0d7fb9; }");
    m_frame->setObjectName(QStringLiteral("placementReadout"));
    m_angleFrame->setObjectName(QStringLiteral("placementAngle"));
    m_frame->setStyleSheet(frameStyle);
    m_angleFrame->setStyleSheet(frameStyle);
    m_frame->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_angleFrame->setAttribute(Qt::WA_TransparentForMouseEvents);

    constexpr int kValueWidth = 78;
    auto *layout = new QVBoxLayout(m_frame);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(1);
    const char *names[3] = {"X", "Y", "Z"};
    for (int i = 0; i < 3; ++i) {
        auto *row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);
        auto *label = new QLabel(QString::fromLatin1(names[i]), m_frame);
        label->setObjectName(QStringLiteral("axisBadge") + QString::fromLatin1(names[i]));
        label->setAlignment(Qt::AlignCenter);
        label->setFixedSize(16, 16);
        row->addWidget(label);
        auto *edit = new QLineEdit(QStringLiteral("0.000 m"), m_frame);
        edit->setReadOnly(true);
        edit->setFixedWidth(kValueWidth);
        row->addWidget(edit);
        layout->addLayout(row);
        m_values[static_cast<std::size_t>(i)] = edit;
    }

    auto *angleLayout = new QHBoxLayout(m_angleFrame);
    angleLayout->setContentsMargins(7, 4, 8, 4);
    angleLayout->setSpacing(3);
    auto *angleIcon = new QLabel(QStringLiteral("∠"), m_angleFrame);
    angleIcon->setObjectName(QStringLiteral("angleBadge"));
    angleLayout->addWidget(angleIcon);
    m_angleEditor = new QLineEdit(QStringLiteral("0.000°"), m_angleFrame);
    m_angleEditor->setReadOnly(true);
    m_angleEditor->setFixedWidth(kValueWidth);
    angleLayout->addWidget(m_angleEditor);

    equalizeWidths();
    hideAll();
}

void CoordinateReadoutWidget::equalizeWidths()
{
    for (QFrame *frame : {m_frame, m_angleFrame}) {
        frame->setMinimumWidth(0);
        frame->setMaximumWidth(16777215);
        frame->adjustSize();
    }
    const int width = std::max(m_frame->sizeHint().width(), m_angleFrame->sizeHint().width());
    m_frame->setFixedWidth(width);
    m_angleFrame->setFixedWidth(width);
    m_frame->adjustSize();
    m_angleFrame->adjustSize();
}

void CoordinateReadoutWidget::setCoordinateText(int index, const QString &text)
{
    m_values[static_cast<std::size_t>(index)]->setText(text);
}

void CoordinateReadoutWidget::setAngleText(const QString &text)
{
    m_angleEditor->setText(text);
}

void CoordinateReadoutWidget::setEditing(int index, bool editing)
{
    QLineEdit *edit = index == 3 ? m_angleEditor : m_values[static_cast<std::size_t>(index)];
    edit->setReadOnly(!editing);
    if (editing) {
        // Os campos devem receber teclado: os frames deixam de ser transparentes ao mouse.
        edit->setFocus(Qt::OtherFocusReason);
        edit->selectAll();
    }
}

void CoordinateReadoutWidget::setAllReadOnly()
{
    for (QLineEdit *edit : m_values) {
        edit->setReadOnly(true);
    }
    m_angleEditor->setReadOnly(true);
}

void CoordinateReadoutWidget::hideAll()
{
    m_frame->hide();
    m_angleFrame->hide();
}

void CoordinateReadoutWidget::showNear(const QPointF &cursor, bool angleVisible)
{
    constexpr int kOffset = 16;
    const int readoutWidth = std::max(m_frame->width(), m_angleFrame->width());
    int readoutHeight = m_frame->height();
    if (angleVisible) {
        readoutHeight += 6 + m_angleFrame->height();
    }
    int left = static_cast<int>(cursor.x()) + kOffset;
    int top = static_cast<int>(cursor.y()) + kOffset;
    if (left + readoutWidth > m_parent->width()) {
        left = static_cast<int>(cursor.x()) - readoutWidth - kOffset;
    }
    if (top + readoutHeight > m_parent->height()) {
        top = static_cast<int>(cursor.y()) - readoutHeight - kOffset;
    }
    m_frame->move(std::max(0, std::min(left, m_parent->width() - readoutWidth)),
                  std::max(0, std::min(top, m_parent->height() - readoutHeight)));
    m_frame->show();
    m_frame->raise();
    if (angleVisible) {
        m_angleFrame->move(std::max(0, std::min(left, m_parent->width() - m_angleFrame->width())),
                           std::max(0, std::min(top + m_frame->height() + 6,
                                                m_parent->height() - m_angleFrame->height())));
        m_angleFrame->show();
        m_angleFrame->raise();
    } else {
        m_angleFrame->hide();
    }
}

// ── ViewportLabelOverlay ──────────────────────────────────────────────────

ViewportLabelOverlay::ViewportLabelOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    resizeToParent();
    show();
    raise();
}

void ViewportLabelOverlay::resizeToParent()
{
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
    }
    raise();
}

void ViewportLabelOverlay::setGroup(const QString &kind,
                                    const std::vector<std::array<double, 3>> &positions,
                                    const QVector<QString> &labels,
                                    bool visible)
{
    Group group;
    group.positions = positions;
    group.labels = labels;
    group.screen.assign(positions.size(), QPointF());
    group.onScreen.assign(positions.size(), false);
    group.visible = visible;
    m_groups.insert(kind, std::move(group));
}

void ViewportLabelOverlay::setGroupVisible(const QString &kind, bool visible)
{
    auto it = m_groups.find(kind);
    if (it != m_groups.end()) {
        it->visible = visible;
    }
    update();
}

void ViewportLabelOverlay::clearAll()
{
    m_groups.clear();
    update();
}

void ViewportLabelOverlay::sync(const QMatrix4x4 &matrix)
{
    const double w = width();
    const double h = height();
    for (auto it = m_groups.begin(); it != m_groups.end(); ++it) {
        Group &group = it.value();
        for (std::size_t i = 0; i < group.positions.size(); ++i) {
            const auto &p = group.positions[i];
            const QVector4D clip = matrix * QVector4D(static_cast<float>(p[0]), static_cast<float>(p[1]),
                                                       static_cast<float>(p[2]), 1.0f);
            const double cw = clip.w();
            if (cw <= 1e-12) {
                group.onScreen[i] = false;
                continue;
            }
            const double nx = clip.x() / cw;
            const double ny = clip.y() / cw;
            const double nz = clip.z() / cw;
            group.onScreen[i] = std::abs(nz) <= 1.0 && std::abs(nx) <= 1.0 && std::abs(ny) <= 1.0;
            group.screen[i] = QPointF((nx + 1.0) * w / 2.0, (1.0 - ny) * h / 2.0);
        }
    }
    raise();
    update();
}

void ViewportLabelOverlay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    QFont font = painter.font();
    font.setPixelSize(11);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor(QStringLiteral("#24292f")));
    const QFontMetricsF metrics(font);
    for (auto it = m_groups.constBegin(); it != m_groups.constEnd(); ++it) {
        const Group &group = it.value();
        if (!group.visible) {
            continue;
        }
        for (std::size_t i = 0; i < group.positions.size() && static_cast<int>(i) < group.labels.size(); ++i) {
            if (!group.onScreen[i]) {
                continue;
            }
            const QString &text = group.labels[static_cast<int>(i)];
            const QSizeF size(metrics.horizontalAdvance(text), metrics.height());
            painter.drawText(QPointF(group.screen[i].x() - size.width() / 2.0,
                                     group.screen[i].y() + metrics.ascent() - size.height() / 2.0),
                             text);
        }
    }
}

} // namespace Structura::UI
