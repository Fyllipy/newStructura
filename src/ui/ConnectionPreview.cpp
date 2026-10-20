#include "ConnectionPreview.h"

#include "app/services/ConnectionCatalog.h"
#include "app/services/ConnectionService.h"
#include "app/services/ConnectionSolids.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>

namespace Structura::UI {

using Structura::App::ProjectData::NodeConnection;
using Structura::App::Services::ConnectionService;
using Structura::App::Services::ConnectionSolids;

ConnectionPreview::ConnectionPreview(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(260, 200);
    setToolTip(tr("Arraste para girar · roda do mouse: zoom"));
}

void ConnectionPreview::setConnection(const NodeConnection &connection, double eta)
{
    m_connection = connection;
    m_eta = eta;
    m_has = true;
    update();
}

void ConnectionPreview::clear()
{
    m_has = false;
    update();
}

void ConnectionPreview::paintConnection(QPainter &painter, const QRectF &area, const NodeConnection &connection, double eta, double yaw, double pitch, double zoom)
{
    ConnectionSolids::Frame frame;
    if (connection.type == NodeConnection::kBasePlate || Structura::App::Services::ConnectionCatalog::isVertical(connection.type)) {
        frame.s = {0, 0, 1};   // pilar para cima
        frame.t = {1, 0, 0};
        frame.u = {0, 1, 0};
    } else {
        frame.s = {1, 0, 0};
        frame.t = {0, 0, 1};
        frame.u = {0, 1, 0};
    }
    frame.scale = 1.0;
    ConnectionSolids::Mesh mesh;
    ConnectionSolids::append(mesh, ConnectionSolids::build(connection, true), frame, eta);
    if (mesh.triangles.empty()) {
        return;
    }
    const double cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
    auto rotate = [&](double x, double y, double z) {
        const double x1 = cy * x - sy * y, y1 = sy * x + cy * y;
        return std::array<double, 3> {x1, cp * y1 - sp * z, sp * y1 + cp * z};   // x, profundidade, altura na tela
    };
    std::vector<std::array<double, 3>> pts(mesh.points.size());
    double minX = 1e18, maxX = -1e18, minZ = 1e18, maxZ = -1e18;
    for (std::size_t i = 0; i < mesh.points.size(); ++i) {
        pts[i] = rotate(mesh.points[i][0], mesh.points[i][1], mesh.points[i][2]);
        minX = std::min(minX, pts[i][0]);
        maxX = std::max(maxX, pts[i][0]);
        minZ = std::min(minZ, pts[i][2]);
        maxZ = std::max(maxZ, pts[i][2]);
    }
    const double k = zoom * 0.92 * std::min(area.width() / std::max(maxX - minX, 1.0), area.height() / std::max(maxZ - minZ, 1.0));
    const QPointF center = area.center();
    const double mx = (minX + maxX) / 2.0, mz = (minZ + maxZ) / 2.0;
    auto screen = [&](const std::array<double, 3> &p) { return QPointF(center.x() + (p[0] - mx) * k, center.y() - (p[2] - mz) * k); };

    struct Face {
        int index;
        double depth;
        bool member;
    };
    const auto memberColor = ConnectionSolids::color(ConnectionSolids::Role::Member, 0.0);
    const auto concreteColor = ConnectionSolids::color(ConnectionSolids::Role::Concrete, 0.0);
    std::vector<Face> order;
    order.reserve(mesh.triangles.size());
    for (std::size_t i = 0; i < mesh.triangles.size(); ++i) {
        const auto &t = mesh.triangles[i];
        const auto n = rotate(mesh.normals[static_cast<std::size_t>(t[0])][0], mesh.normals[static_cast<std::size_t>(t[0])][1], mesh.normals[static_cast<std::size_t>(t[0])][2]);
        if (n[1] > 0.12) {
            continue;   // face de costas
        }
        order.push_back({static_cast<int>(i), (pts[static_cast<std::size_t>(t[0])][1] + pts[static_cast<std::size_t>(t[1])][1] + pts[static_cast<std::size_t>(t[2])][1]) / 3.0,
                         mesh.colors[static_cast<std::size_t>(t[0])] == memberColor || mesh.colors[static_cast<std::size_t>(t[0])] == concreteColor});
    }
    // pecas ligadas (translucidas) primeiro, depois a ligacao por cima, cada grupo de tras para frente
    std::sort(order.begin(), order.end(), [](const Face &a, const Face &b) { return a.member != b.member ? a.member : a.depth > b.depth; });
    painter.save();
    painter.setClipRect(area);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const std::array<double, 3> light {-0.35, -0.55, 0.75};
    for (const auto &face : order) {
        const auto &t = mesh.triangles[static_cast<std::size_t>(face.index)];
        const auto n = rotate(mesh.normals[static_cast<std::size_t>(t[0])][0], mesh.normals[static_cast<std::size_t>(t[0])][1], mesh.normals[static_cast<std::size_t>(t[0])][2]);
        const double lambert = std::max(0.0, n[0] * light[0] + n[1] * light[1] + n[2] * light[2]);
        const double shade = 0.5 + 0.5 * lambert;
        const auto &c = mesh.colors[static_cast<std::size_t>(t[0])];
        const QColor color(std::min(255, static_cast<int>(c[0] * shade + 12)), std::min(255, static_cast<int>(c[1] * shade + 12)), std::min(255, static_cast<int>(c[2] * shade + 12)),
                           face.member ? 95 : 255);
        painter.setPen(QPen(color, 0.6));
        painter.setBrush(color);
        painter.drawPolygon(QPolygonF({screen(pts[static_cast<std::size_t>(t[0])]), screen(pts[static_cast<std::size_t>(t[1])]), screen(pts[static_cast<std::size_t>(t[2])])}));
    }
    painter.restore();
}

void ConnectionPreview::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(QStringLiteral("#f4f6f9")));
    painter.setPen(QColor(QStringLiteral("#c9d3de")));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
    if (!m_has) {
        painter.setPen(QColor(QStringLiteral("#5a6472")));
        painter.drawText(rect(), Qt::AlignCenter, tr("Selecione uma ligação"));
        return;
    }
    paintConnection(painter, QRectF(rect()).adjusted(6, 6, -6, -6), m_connection, m_eta, m_yaw, m_pitch, m_zoom);
    painter.setPen(QColor(QStringLiteral("#5a6472")));
    painter.drawText(rect().adjusted(8, 4, -8, -4), Qt::AlignLeft | Qt::AlignBottom, tr("Desenho esquemático (arraste para girar)"));
}

void ConnectionPreview::mousePressEvent(QMouseEvent *event)
{
    m_last = event->pos();
}

void ConnectionPreview::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        m_yaw += (event->pos().x() - m_last.x()) * 0.01;
        m_pitch = std::clamp(m_pitch + (event->pos().y() - m_last.y()) * 0.01, -1.5, 1.5);
        m_last = event->pos();
        update();
    }
}

void ConnectionPreview::wheelEvent(QWheelEvent *event)
{
    m_zoom = std::clamp(m_zoom * std::pow(1.15, event->angleDelta().y() / 120.0), 0.4, 6.0);
    update();
    event->accept();
}

QPixmap ConnectionPreview::example(int type, const QSize &size)
{
    ConnectionService::BeamInfo beam;
    beam.depth = 310.0;
    beam.bf = 165.0;
    beam.tf = 9.7;
    beam.tw = 5.8;
    beam.area = 4000.0;
    beam.fy = 345.0;
    beam.fu = 450.0;
    NodeConnection connection = type == NodeConnection::kTubular ? ConnectionService::makeTubular(beam, {}) : ConnectionService::makeDefault(type, beam, 250.0, 400.0);
    connection.type = type;
    if (type == NodeConnection::kTubular) {
        connection.values[QStringLiteral("kindT")] = 2.0;
        connection.values[QStringLiteral("d0")] = 140.0;
        connection.values[QStringLiteral("d1")] = connection.values[QStringLiteral("d2")] = 80.0;
        connection.values[QStringLiteral("th1")] = connection.values[QStringLiteral("th2")] = 45.0;
        connection.values[QStringLiteral("gap")] = 30.0;
    } else if (Structura::App::Services::ConnectionCatalog::baseType(type) == NodeConnection::kTubular) {
        // miniatura de uma variante tubular: dimensoes de exemplo; tipo, angulos e afastamento vem da variante
        connection.values[QStringLiteral("d0")] = 140.0;
        connection.values[QStringLiteral("d1")] = connection.values[QStringLiteral("d2")] = 80.0;
    }
    const qreal ratio = 2.0;
    QPixmap pixmap(size * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(QColor(QStringLiteral("#f4f6f9")));
    QPainter painter(&pixmap);
    paintConnection(painter, QRectF(QPointF(2, 2), QSizeF(size) - QSizeF(4, 4)), connection, 0.0, -0.75, 0.45, 1.0);
    return pixmap;
}

} // namespace Structura::UI
