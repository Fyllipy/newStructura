#include "PlacementController.h"

#include "PlacementOverlay.h"
#include "viz/SceneController.h"
#include "viz/construction/ConstructionScene.h"
#include "viz/construction/PlacementSolver.h"

#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace Structura::UI {

namespace {

constexpr double kPlacementDragTolerancePixels = 4.0;
constexpr double kPi = 3.14159265358979323846;

QString formatCoordinate(double value)
{
    if (std::abs(value) < 0.0005) {
        value = 0.0;
    }
    return QStringLiteral("%1 m").arg(value, 0, 'f', 3);
}

std::optional<std::pair<double, bool>> parseEditorValue(const QLineEdit *editor)
{
    QString text = editor->text().trimmed().toLower();
    text.remove(QLatin1Char('m'));
    text.remove(QChar(0x00b0));
    const bool relative = text.startsWith(QLatin1Char('@'));
    if (relative) {
        text = text.mid(1).trimmed();
    }
    text.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double value = text.toDouble(&ok);
    if (!ok) {
        return std::nullopt;
    }
    return std::make_pair(value, relative);
}

} // namespace

PlacementController::PlacementController(SceneController *scene, QWidget *viewport, QObject *parent)
    : QObject(parent)
    , m_scene(scene)
    , m_viewport(viewport)
    , m_solver(std::make_unique<Structura::Construction::PlacementSolver>(
          scene->renderer(), scene->renderWindow(), scene->constructionScene()))
    , m_marker(new SnapMarkerWidget(viewport))
    , m_readout(new CoordinateReadoutWidget(viewport))
{
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(0);
    connect(&m_refreshTimer, &QTimer::timeout, this, &PlacementController::refresh);

    for (int i = 0; i < 3; ++i) {
        m_readout->coordinateEditor(i)->installEventFilter(this);
    }
    m_readout->angleEditor()->installEventFilter(this);
}

PlacementController::~PlacementController() = default;

void PlacementController::start(Mode mode)
{
    m_mode = mode;
    m_solver->setBarSnap(mode == Mode::Node || mode == Mode::Measure);
    m_start.reset();
    m_press.reset();
    cancelCoordinateEdit(false);
    if (mode == Mode::None) {
        hideVisuals();
        return;
    }
    refresh();
    if (onStateChanged) {
        onStateChanged();
    }
}

void PlacementController::stop()
{
    m_mode = Mode::None;
    m_solver->setBarSnap(false);
    m_ghostSource.clear();
    m_start.reset();
    m_press.reset();
    cancelCoordinateEdit(false);
    hideVisuals();
    if (onStateChanged) {
        onStateChanged();
    }
}

void PlacementController::hideVisuals()
{
    m_marker->hide();
    m_readout->hideAll();
    m_readout->setAllReadOnly();
    if (auto *construction = m_scene->constructionScene()) {
        construction->hidePlacementVisuals();
    }
    if (onHoverChanged) {
        onHoverChanged(false, Vec3 {});
    }
    m_scene->render();
}

void PlacementController::scheduleRefresh()
{
    if (active() && !m_refreshTimer.isActive()) {
        m_refreshTimer.start();
    }
}

std::optional<PlacementController::Vec3> PlacementController::currentTarget(Structura::Construction::SnapKind *kind) const
{
    if (kind) {
        *kind = Structura::Construction::SnapKind::None;
    }
    if (m_manualTarget) {
        return m_manualTarget;
    }
    if (!m_pointer) {
        return std::nullopt;
    }
    Vec3 cursor {};
    if (!m_solver->cursorOnPlane(*m_pointer, cursor)) {
        return std::nullopt;
    }
    const auto target = m_solver->resolve(*m_pointer, cursor, m_start);
    if (kind) {
        *kind = target.kind;
    }
    return target.point;
}

std::optional<double> PlacementController::previewAngleDegrees(const Vec3 &endpoint) const
{
    if (!m_start) {
        return std::nullopt;
    }
    const auto axes = Structura::Construction::planeAxes(m_scene->constructionScene()->planeMode());
    const double du = endpoint[static_cast<std::size_t>(axes.u)] - (*m_start)[static_cast<std::size_t>(axes.u)];
    const double dv = endpoint[static_cast<std::size_t>(axes.v)] - (*m_start)[static_cast<std::size_t>(axes.v)];
    if (std::hypot(du, dv) <= 1e-9) {
        return std::nullopt;
    }
    double angle = std::atan2(dv, du) * 180.0 / kPi;
    angle = std::fmod(angle, 360.0);
    if (angle < 0.0) {
        angle += 360.0;
    }
    return angle;
}

void PlacementController::refresh()
{
    auto *construction = m_scene->constructionScene();
    if (!active() || !construction) {
        return;
    }
    m_solver->setWidgetSize(m_viewport->size());

    if (!m_pointer || !m_viewport->underMouse()) {
        m_marker->hide();
        m_readout->hideAll();
        construction->hidePlacementVisuals();
        if (onHoverChanged) {
            onHoverChanged(false, Vec3 {});
        }
        m_scene->render();
        return;
    }

    Structura::Construction::SnapKind kind = Structura::Construction::SnapKind::None;
    const auto target = currentTarget(&kind);
    if (!target) {
        m_marker->hide();
        m_readout->hideAll();
        construction->hidePlacementVisuals();
        if (onHoverChanged) {
            onHoverChanged(false, Vec3 {});
        }
        m_scene->render();
        return;
    }

    const Vec3 display = *target;
    m_lastDisplay = display;

    construction->setGuides(m_start);
    if (m_start) {
        construction->setPreviewLine(std::make_pair(*m_start, display));
    } else {
        construction->setPreviewLine(std::nullopt);
    }
    if ((m_mode == Mode::Copy || m_mode == Mode::Move) && m_start && !m_ghostSource.empty()) {
        const Vec3 delta {display[0] - (*m_start)[0], display[1] - (*m_start)[1], display[2] - (*m_start)[2]};
        std::vector<std::pair<Vec3, Vec3>> moved;
        moved.reserve(m_ghostSource.size());
        for (const auto &segment : m_ghostSource) {
            moved.push_back({Vec3 {segment.first[0] + delta[0], segment.first[1] + delta[1], segment.first[2] + delta[2]},
                             Vec3 {segment.second[0] + delta[0], segment.second[1] + delta[1], segment.second[2] + delta[2]}});
        }
        construction->setGhostSegments(moved);
    } else {
        construction->setGhostSegments({});
    }

    using Structura::Construction::SnapKind;
    QPointF markerScreen;
    if ((kind == SnapKind::Grid || kind == SnapKind::Endpoint || kind == SnapKind::Center || kind == SnapKind::OnBar)
        && m_solver->worldToWidget(display, markerScreen)) {
        m_marker->setKind(kind == SnapKind::Grid ? SnapMarkerWidget::Kind::Grid
                          : kind == SnapKind::Endpoint ? SnapMarkerWidget::Kind::Endpoint
                          : kind == SnapKind::OnBar ? SnapMarkerWidget::Kind::OnBar
                                                    : SnapMarkerWidget::Kind::Center);
        m_marker->placeAt(markerScreen);
    } else {
        m_marker->hide();
    }

    const auto angle = previewAngleDegrees(display);
    if (!angle) {
        if (m_manualIndex == 3) {
            m_manualIndex = -1;
        }
    } else if (m_manualIndex != 3) {
        m_readout->setAngleText(QStringLiteral("%1°").arg(*angle, 0, 'f', 3));
        m_readout->angleEditor()->setReadOnly(true);
    }
    for (int i = 0; i < 3; ++i) {
        if (m_manualIndex != i) {
            m_readout->setCoordinateText(i, formatCoordinate(display[static_cast<std::size_t>(i)]));
        }
    }
    m_readout->showNear(*m_pointer, angle.has_value());

    if (onHoverChanged) {
        onHoverChanged(true, display);
    }
    m_scene->render();
}

void PlacementController::placeAt(const Vec3 &point)
{
    cancelCoordinateEdit(false);
    if (m_mode == Mode::Rotate || m_mode == Mode::Node) {
        if (onSinglePoint) {
            onSinglePoint(m_mode, point);
        }
    } else if (m_mode == Mode::Bar || m_mode == Mode::Copy || m_mode == Mode::Move || m_mode == Mode::Mirror || m_mode == Mode::Measure) {
        if (!m_start) {
            m_start = point;
        } else {
            const Mode mode = m_mode;
            bool done = false;
            if (mode == Mode::Bar) {
                done = onBarPlaced ? onBarPlaced(*m_start, point) : false;
            } else {
                done = onDisplacement ? onDisplacement(mode, *m_start, point) : false;
            }
            if (done) {
                m_start.reset();
            }
        }
    }
    if (onStateChanged) {
        onStateChanged();
    }
    scheduleRefresh();
}

// ── Edicao manual de coordenadas ──────────────────────────────────────────

std::vector<int> PlacementController::editableFields() const
{
    std::vector<int> fields {0, 1, 2};
    const auto endpoint = m_manualTarget ? m_manualTarget : m_lastDisplay;
    if (m_start && endpoint && previewAngleDegrees(*endpoint)) {
        fields.push_back(3);
    }
    return fields;
}

void PlacementController::commitCoordinateEditor(int index)
{
    if (!m_manualTarget) {
        return;
    }
    QLineEdit *editor = index == 3 ? m_readout->angleEditor() : m_readout->coordinateEditor(index);
    const auto parsed = parseEditorValue(editor);
    if (!parsed) {
        return;
    }
    double value = parsed->first;
    const bool relative = parsed->second;
    Vec3 &target = *m_manualTarget;

    if (index < 3) {
        if (relative) {
            if (!m_start) {
                return;
            }
            double direction = 1.0;
            if (m_manualDirection) {
                const double component = (*m_manualDirection)[static_cast<std::size_t>(index)];
                if (std::abs(component) > 1e-9) {
                    direction = component > 0.0 ? 1.0 : -1.0;
                }
            }
            value = (*m_start)[static_cast<std::size_t>(index)] + value * direction;
        }
        target[static_cast<std::size_t>(index)] = value;
        return;
    }

    if (!m_start) {
        return;
    }
    const auto axes = Structura::Construction::planeAxes(m_scene->constructionScene()->planeMode());
    const auto u = static_cast<std::size_t>(axes.u);
    const auto v = static_cast<std::size_t>(axes.v);
    const double du = target[u] - (*m_start)[u];
    const double dv = target[v] - (*m_start)[v];
    const double planarLength = std::hypot(du, dv);
    if (planarLength <= 1e-9) {
        return;
    }
    const double radians = value * kPi / 180.0;
    target[u] = (*m_start)[u] + planarLength * std::cos(radians);
    target[v] = (*m_start)[v] + planarLength * std::sin(radians);
}

void PlacementController::cycleCoordinateEdit()
{
    if (!active()) {
        return;
    }
    if (!m_manualTarget) {
        if (!m_lastDisplay) {
            refresh();
        }
        if (!m_lastDisplay) {
            return;
        }
        m_manualTarget = *m_lastDisplay;
        if (m_start) {
            m_manualDirection = Structura::Construction::operator-(*m_manualTarget, *m_start);
        } else {
            m_manualDirection.reset();
        }
    }

    const int current = m_manualIndex;
    if (current >= 0) {
        commitCoordinateEditor(current);
    }
    m_manualIndex = -1;
    m_manualConfirmable = false;
    refresh();

    const auto fields = editableFields();
    if (fields.empty()) {
        return;
    }
    std::size_t nextPosition = 0;
    if (current >= 0) {
        const auto it = std::find(fields.begin(), fields.end(), current);
        nextPosition = it == fields.end() ? 0 : (static_cast<std::size_t>(it - fields.begin()) + 1) % fields.size();
    }
    m_manualIndex = fields[nextPosition];
    m_readout->setEditing(m_manualIndex, true);
}

void PlacementController::finishCoordinateEdit()
{
    if (m_manualIndex >= 0) {
        commitCoordinateEditor(m_manualIndex);
    }
    m_manualIndex = -1;
    m_manualConfirmable = m_manualTarget.has_value();
    m_readout->setAllReadOnly();
    m_viewport->setFocus(Qt::OtherFocusReason);
    refresh();
}

void PlacementController::cancelCoordinateEdit(bool refreshAfter)
{
    m_manualTarget.reset();
    m_manualDirection.reset();
    m_manualIndex = -1;
    m_manualConfirmable = false;
    m_readout->setAllReadOnly();
    if (refreshAfter) {
        refresh();
    }
}

// ── Eventos ───────────────────────────────────────────────────────────────

bool PlacementController::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress && active()) {
        auto *key = static_cast<QKeyEvent *>(event);
        bool isReadoutEditor = watched == m_readout->angleEditor();
        for (int i = 0; i < 3 && !isReadoutEditor; ++i) {
            isReadoutEditor = watched == m_readout->coordinateEditor(i);
        }
        if (isReadoutEditor) {
            if (key->key() == Qt::Key_Tab) {
                cycleCoordinateEdit();
                return true;
            }
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                finishCoordinateEdit();
                return true;
            }
            if (key->key() == Qt::Key_Escape) {
                if (onCancelRequested) {
                    onCancelRequested();
                }
                return true;
            }
        }
    }
    return QObject::eventFilter(watched, event);
}

bool PlacementController::handleViewportEvent(QEvent *event)
{
    if (!active()) {
        return false;
    }

    switch (event->type()) {
    case QEvent::MouseMove: {
        auto *move = static_cast<QMouseEvent *>(event);
        m_pointer = QPointF(move->position());
        if (m_manualTarget) {
            cancelCoordinateEdit(false);
        }
        refresh();
        return false;
    }
    case QEvent::MouseButtonPress: {
        auto *press = static_cast<QMouseEvent *>(event);
        m_pointer = QPointF(press->position());
        if (press->button() == Qt::LeftButton) {
            m_press = QPointF(press->position());
        }
        return false;
    }
    case QEvent::MouseButtonRelease: {
        auto *release = static_cast<QMouseEvent *>(event);
        if (release->button() == Qt::LeftButton && m_press) {
            const QPointF pressed = *m_press;
            m_press.reset();
            m_pointer = QPointF(release->position());
            const QPointF delta = *m_pointer - pressed;
            if (std::hypot(delta.x(), delta.y()) <= kPlacementDragTolerancePixels) {
                m_solver->setWidgetSize(m_viewport->size());
                if (const auto target = currentTarget()) {
                    placeAt(*target);
                }
                return true;
            }
        }
        return false;
    }
    case QEvent::Wheel:
    case QEvent::Resize:
        scheduleRefresh();
        return false;
    case QEvent::Leave:
        m_pointer.reset();
        refresh();
        return false;
    case QEvent::KeyPress: {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Tab) {
            cycleCoordinateEdit();
            return true;
        }
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
            && m_manualTarget && m_manualIndex < 0 && m_manualConfirmable) {
            const Vec3 point = *m_manualTarget;
            placeAt(point);
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            if (onCancelRequested) {
                onCancelRequested();
            }
            return true;
        }
        return false;
    }
    default:
        return false;
    }
}

} // namespace Structura::UI
