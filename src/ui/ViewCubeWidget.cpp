#include "ViewCubeWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QSignalBlocker>
#include <QToolButton>

#include <algorithm>
#include <cmath>

namespace Structura::UI {

namespace {

using V3 = std::array<double, 3>;

V3 sub(const V3 &a, const V3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double dot(const V3 &a, const V3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
V3 cross(const V3 &a, const V3 &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
V3 unit(const V3 &a)
{
    const double n = std::sqrt(dot(a, a));
    return n > 1e-12 ? V3 {a[0] / n, a[1] / n, a[2] / n} : V3 {0, 0, 1};
}

constexpr double kCube = 36.0;   // meia aresta em px

} // namespace

ViewCubeWidget::ViewCubeWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(sizeHint());
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    const QString style = QStringLiteral("QToolButton { background: rgba(255,255,255,120); border: 1px solid rgba(120,138,165,160); border-radius: 4px; color: #1e232b; padding: 2px 6px; font-size: 11px; }"
                                         "QToolButton:hover { background: rgba(228,241,250,235); border-color: #0d7fb9; }"
                                         "QToolButton::menu-indicator { image: none; }");
    m_iso = new QToolButton(this);
    m_iso->setText(tr("Vista iso"));
    m_iso->setStyleSheet(style);
    m_iso->setToolTip(tr("Orienta a câmera em uma vista isométrica (a projeção é escolhida no botão abaixo)"));
    m_projection = new QToolButton(this);
    m_projection->setCheckable(true);
    m_projection->setStyleSheet(style);
    m_projection->setToolTip(tr("Alterna entre projeção isométrica (paralela, sem convergência) e perspectiva"));
    m_views = new QToolButton(this);
    m_views->setText(tr("Vistas ▾"));
    m_views->setStyleSheet(style);
    m_views->setPopupMode(QToolButton::InstantPopup);
    m_views->setToolTip(tr("Vistas salvas e comandos para salvar a vista atual"));
    layoutButtons();
    setPerspective(false);
    connect(m_projection, &QToolButton::clicked, this, [this](bool checked) {
        setPerspective(checked);
        emit perspectiveToggled(checked);
    });
    connect(m_iso, &QToolButton::clicked, this, [this]() { emit viewRequested(1.0, -1.0, 0.8, 0.0, 0.0, 1.0); });
}

void ViewCubeWidget::setPerspective(bool perspective)
{
    QSignalBlocker block(m_projection);
    m_projection->setChecked(perspective);
    m_projection->setText(perspective ? tr("Perspectiva") : tr("Isométrica"));
}

void ViewCubeWidget::layoutButtons()
{
    m_iso->setGeometry(6, 104, width() - 12, 22);
    m_projection->setGeometry(6, 129, width() - 12, 22);
    m_views->setGeometry(6, 154, width() - 12, 22);
}

void ViewCubeWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    double pos[3] = {0, -1, 0}, target[3] = {0, 0, 0}, up[3] = {0, 0, 1};
    if (m_camera) {
        m_camera(pos, target, up);
    }
    const V3 forward = unit(sub({target[0], target[1], target[2]}, {pos[0], pos[1], pos[2]}));
    const V3 right = unit(cross(forward, {up[0], up[1], up[2]}));
    const V3 trueUp = cross(right, forward);

    static const std::array<Face, 6> faces {{
        {{0, 0, 1}, QStringLiteral("TOPO"), {0, 1, 0}},
        {{0, 0, -1}, QStringLiteral("BASE"), {0, -1, 0}},
        {{0, -1, 0}, QStringLiteral("FRENTE"), {0, 0, 1}},
        {{0, 1, 0}, QStringLiteral("FUNDOS"), {0, 0, 1}},
        {{1, 0, 0}, QStringLiteral("DIREITA"), {0, 0, 1}},
        {{-1, 0, 0}, QStringLiteral("ESQ."), {0, 0, 1}},
    }};
    const QPointF center(width() / 2.0, 51.0);
    auto project = [&](const V3 &v) { return QPointF(center.x() + dot(v, right) * kCube * 0.9, center.y() - dot(v, trueUp) * kCube * 0.9); };

    struct Item {
        int index;
        double depth;
    };
    std::vector<Item> order;
    for (int i = 0; i < 6; ++i) {
        m_visible[static_cast<std::size_t>(i)] = dot(faces[static_cast<std::size_t>(i)].normal, forward) < -1e-6;   // voltada para o olho
        order.push_back({i, dot(faces[static_cast<std::size_t>(i)].normal, forward)});
    }
    std::sort(order.begin(), order.end(), [](const Item &a, const Item &b) { return a.depth > b.depth; });
    QFont font = p.font();
    font.setPointSizeF(6.5);
    font.setBold(true);
    p.setFont(font);
    for (const Item &item : order) {
        const std::size_t i = static_cast<std::size_t>(item.index);
        if (!m_visible[i]) {
            continue;
        }
        const V3 n = faces[i].normal;
        // dois eixos do plano da face
        V3 a = std::abs(n[2]) > 0.5 ? V3 {1, 0, 0} : V3 {0, 0, 1};
        V3 b = cross(n, a);
        a = cross(b, n);
        std::array<QPointF, 4> poly;
        const double s[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (int k = 0; k < 4; ++k) {
            const V3 v {n[0] + a[0] * s[k][0] + b[0] * s[k][1], n[1] + a[1] * s[k][0] + b[1] * s[k][1], n[2] + a[2] * s[k][0] + b[2] * s[k][1]};
            poly[static_cast<std::size_t>(k)] = project(v);
        }
        m_polygons[i] = poly;
        const bool hot = m_hover == item.index;
        const double light = 0.82 + 0.18 * std::abs(dot(n, forward));
        QColor fill = hot ? QColor(QStringLiteral("#7cc4ee")) : QColor::fromHsvF(0.58, 0.08, std::min(1.0, light));
        p.setBrush(fill);
        p.setPen(QPen(QColor(QStringLiteral("#6b7c93")), 1.2));
        p.drawPolygon(QPolygonF(QList<QPointF>(poly.begin(), poly.end())));
        p.setPen(QColor(QStringLiteral("#1e232b")));
        QPointF c(0, 0);
        for (const QPointF &q : poly) {
            c += q / 4.0;
        }
        p.drawText(QRectF(c.x() - 28, c.y() - 7, 56, 14), Qt::AlignCenter, faces[i].label);
    }
}

int ViewCubeWidget::faceAt(const QPointF &pt) const
{
    for (int i = 0; i < 6; ++i) {
        if (!m_visible[static_cast<std::size_t>(i)]) {
            continue;
        }
        const auto &poly = m_polygons[static_cast<std::size_t>(i)];
        if (QPolygonF(QList<QPointF>(poly.begin(), poly.end())).containsPoint(pt, Qt::OddEvenFill)) {
            return i;
        }
    }
    return -1;
}

void ViewCubeWidget::mouseMoveEvent(QMouseEvent *event)
{
    const int face = faceAt(event->position());
    if (face != m_hover) {
        m_hover = face;
        setCursor(face >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void ViewCubeWidget::leaveEvent(QEvent *)
{
    m_hover = -1;
    update();
}

void ViewCubeWidget::mousePressEvent(QMouseEvent *event)
{
    const int face = faceAt(event->position());
    if (face < 0 || event->button() != Qt::LeftButton) {
        return;
    }
    static const double dirs[6][6] = {
        {0, 0, 1, 0, 1, 0},    // topo: olho em +Z, Y para cima na tela
        {0, 0, -1, 0, -1, 0},  // base
        {0, -1, 0, 0, 0, 1},   // frente (olho em -Y)
        {0, 1, 0, 0, 0, 1},    // fundos
        {1, 0, 0, 0, 0, 1},    // direita (olho em +X)
        {-1, 0, 0, 0, 0, 1},   // esquerda
    };
    const double *d = dirs[face];
    emit viewRequested(d[0], d[1], d[2], d[3], d[4], d[5]);
}

} // namespace Structura::UI
