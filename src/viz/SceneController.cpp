#include "viz/SceneController.h"
#include "viz/LocalCoordinateSystem.h"
#include "viz/construction/ConstructionScene.h"
#include "viz/structure/SupportMeshBuilder.h"
#include "viz/loads/LoadActionRenderer.h"
#include "viz/structure/SolidMemberRenderer.h"
#include <vtkSphereSource.h>

#include <QVTKOpenGLNativeWidget.h>

#include <vtkActor.h>
#include <vtkAxesActor.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkPointData.h>
#include <vtkUnsignedCharArray.h>
#include <vtkPointPicker.h>
#include <cmath>
#include <vtkCellPicker.h>
#include <vtkCellData.h>
#include <vtkGlyph3D.h>
#include <vtkArrowSource.h>
#include <vtkDoubleArray.h>
#include <vtkDataObject.h>
#include <vtkBillboardTextActor3D.h>
#include <vtkCallbackCommand.h>
#include <vtkMatrix4x4.h>
#include <vtkTextProperty.h>
#include <QVector3D>
#include <QtGlobal>
#include <QString>
#include <optional>
#include <limits>
#include <algorithm>
#include "viz/CustomInteractorStyle.h"

namespace {
#ifndef M_PI
constexpr double M_PI = 3.14159265358979323846;
#endif
}


void SceneController::initializeBarLcsRendering()
{
    m_lcsData = vtkSmartPointer<vtkPolyData>::New();
    m_lcsPoints = vtkSmartPointer<vtkPoints>::New();
    m_lcsCells = vtkSmartPointer<vtkCellArray>::New();
    m_lcsColors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    m_lcsMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_lcsActor = vtkSmartPointer<vtkActor>::New();

    m_lcsColors->SetNumberOfComponents(3);
    m_lcsColors->SetName("LCSColors");
    m_lcsData->SetPoints(m_lcsPoints);
    m_lcsData->SetLines(m_lcsCells);
    m_lcsData->GetCellData()->SetScalars(m_lcsColors);
    m_lcsMapper->SetInputData(m_lcsData);
    m_lcsMapper->ScalarVisibilityOn();
    m_lcsMapper->SetColorModeToDirectScalars();
    m_lcsMapper->SetScalarModeToUseCellData();
    m_lcsActor->SetMapper(m_lcsMapper);

    if (auto *property = m_lcsActor->GetProperty()) {
        property->SetLineWidth(2.5);
        property->LightingOff();
    }

    m_lcsActor->PickableOff();
    m_lcsActor->SetVisibility(false);
}

void SceneController::initializePickers()
{
    if (m_picker) {
        m_picker->SetTolerance(0.005);
    }

    if (m_nodePicker) {
        m_nodePicker->SetTolerance(0.01);
        m_nodePicker->SetPickFromList(true);
        m_nodePicker->AddPickList(m_structureRenderer->getNodeActor());
    }

    if (m_barPicker) {
        m_barPicker->SetTolerance(0.005);
        m_barPicker->SetPickFromList(true);
        m_barPicker->AddPickList(m_structureRenderer->getBarActor());
    }
}

void SceneController::requestRender()
{
    // Pedidos de renderizacao feitos em sequencia (criar barras, trocar aparencia, mover nos...) viram um unico Render().
    if (m_renderPending) {
        return;
    }
    m_renderPending = true;
    QMetaObject::invokeMethod(this, [this]() {
        m_renderPending = false;
        if (auto *renderWindow = m_renderWindow.GetPointer()) {
            renderWindow->Render();
        }
    }, Qt::QueuedConnection);
}

void SceneController::clearNodes()
{
    m_nodes.clear();
    m_nodeIndexById.clear();
    m_highlightNodeId = QUuid();
    m_selectedNodeIds.clear();
    m_nextNodeExternalId = 1;

    m_structureRenderer->clear();
}

void SceneController::clearBars()
{
    m_bars.clear();
    m_barIndexById.clear();
    m_selectedBarIds.clear();
    
    // As linhas também são apagadas via m_structureRenderer->clear()
    // Mas clearBars isolado pode precisar de tratamento (TODO: se necessário)
}

void SceneController::clearLoads()
{
    m_nodalLoadVisuals.clear();
    m_memberActionVisuals.clear();
    updateLoadVisuals();
}

void SceneController::clearSupports()
{
    m_supportVisuals.clear();

    if (m_supportData) {
        m_supportData->Initialize();
        m_supportData->Modified();
    }
}

SceneController::SceneController(QObject *parent)
    : QObject(parent)
    , m_orientationMarker(vtkSmartPointer<vtkOrientationMarkerWidget>::New())
    , m_picker(vtkSmartPointer<vtkCellPicker>::New())
    , m_nodePicker(vtkSmartPointer<vtkPointPicker>::New())
    , m_barPicker(vtkSmartPointer<vtkCellPicker>::New())
    , m_structureRenderer(std::make_unique<Structura::Visualization::StructureRenderer>())
    , m_construction(std::make_unique<Structura::Construction::ConstructionScene>())
{
    // Initialize component renderers
    initializeBarLcsRendering();
    initializePickers();
}

SceneController::~SceneController() = default;

void SceneController::initialize(QVTKOpenGLNativeWidget *vtkWidget)
{
    if (!vtkWidget) {
        return;
    }

    vtkWidget->setRenderWindow(m_renderWindow);
    m_renderer->GetActiveCamera()->ParallelProjectionOn();   // isometrica (paralela) e o padrao; a perspectiva e opcional
    m_renderWindow->LineSmoothingOn();
    
    m_renderWindow->AddRenderer(m_renderer);
    // Background similar to SolidWorks: soft light blue gradient
    m_renderer->GradientBackgroundOn();
    m_renderer->SetBackground(0.91, 0.94, 0.98);  // top
    m_renderer->SetBackground2(0.78, 0.85, 0.93); // bottom
    m_structureRenderer->addToRenderer(m_renderer);
    m_renderer->AddActor(m_lcsActor);
    m_construction->attach(m_renderer, [this]() { requestRender(); });

    {
        auto cameraObserver = vtkSmartPointer<vtkCallbackCommand>::New();
        cameraObserver->SetClientData(this);
        cameraObserver->SetCallback([](vtkObject *, unsigned long, void *clientData, void *) {
            auto *self = static_cast<SceneController *>(clientData);
            emit self->cameraChanged();
        });
        m_renderer->GetActiveCamera()->AddObserver(vtkCommand::ModifiedEvent, cameraObserver);
    }
    
    m_renderer->ResetCamera();

    auto axes = vtkSmartPointer<vtkAxesActor>::New();
    axes->AxisLabelsOn();
    axes->SetConeRadius(0.55);
    axes->SetShaftTypeToCylinder();
    axes->SetCylinderRadius(0.03);
    axes->SetTotalLength(1.0, 1.0, 1.0);

    m_orientationMarker->SetOrientationMarker(axes);
    if (auto *interactor = vtkWidget->interactor()) {
        m_orientationMarker->SetInteractor(interactor);
        m_orientationMarker->SetViewport(0.80, 0.80, 0.98, 0.98);
        m_orientationMarker->SetOutlineColor(0.9, 0.9, 0.9);
        m_orientationMarker->EnabledOn();
        m_orientationMarker->InteractiveOff();

        // Custom interactor: rotate with right button
        vtkSmartPointer<CustomInteractorStyle> style = vtkSmartPointer<CustomInteractorStyle>::New();
        // sem geometria sob o cursor, o giro usa o centro do modelo
        style->setFallbackPivot([this](double *pivot) {
            if (m_nodes.empty() || !m_structureRenderer) {
                return false;
            }
            double bounds[6];
            m_structureRenderer->getNodeActor()->GetBounds(bounds);
            for (int i = 0; i < 3; ++i) {
                pivot[i] = 0.5 * (bounds[2 * i] + bounds[2 * i + 1]);
            }
            return true;
        });
        interactor->SetInteractorStyle(style);
    }

    requestRender();
}

QUuid SceneController::addPoint(double x, double y, double z)
{
    return addPointWithId(x, y, z, m_nextNodeExternalId++);
}

QUuid SceneController::addPointWithId(double x, double y, double z, int externalId)
{
    if (externalId <= 0) {
        externalId = m_nextNodeExternalId++;
    } else {
        if (externalId >= m_nextNodeExternalId) {
            m_nextNodeExternalId = externalId + 1;
        }
    }

    QUuid nodeId = QUuid::createUuid();
    Node node(nodeId, externalId, x, y, z);
    m_nodes.push_back(node);
    m_nodeIndexById.insert(nodeId, static_cast<int>(m_nodes.size()) - 1);

    m_structureRenderer->addNode(nodeId, x, y, z);

    updateBounds();
    requestRender();

    return nodeId;
}

void SceneController::resetCamera()
{
    m_renderer->ResetCamera();
    requestRender();
}

void SceneController::zoomExtents()
{
    if (m_nodes.empty()) {
        resetCamera();
        return;
    }

    m_renderer->ResetCamera(m_structureRenderer->getNodeActor()->GetBounds());
    requestRender();
}

void SceneController::focusOnBars(const QSet<QUuid> &barIds)
{
    if (!m_renderer || barIds.isEmpty()) {
        return;
    }
    QHash<QUuid, std::array<double, 3>> position;
    for (const auto &node : nodeInfos()) {
        position.insert(node.id, {node.x, node.y, node.z});
    }
    double bounds[6] = {1e300, -1e300, 1e300, -1e300, 1e300, -1e300};
    bool any = false;
    for (const auto &bar : bars()) {
        if (!barIds.contains(bar.id)) {
            continue;
        }
        for (const QUuid &nodeId : {bar.startNodeId, bar.endNodeId}) {
            const auto it = position.constFind(nodeId);
            if (it == position.constEnd()) {
                continue;
            }
            for (int k = 0; k < 3; ++k) {
                bounds[2 * k] = std::min(bounds[2 * k], (*it)[k]);
                bounds[2 * k + 1] = std::max(bounds[2 * k + 1], (*it)[k]);
            }
            any = true;
        }
    }
    if (!any) {
        return;
    }
    // margem: 1,5x a maior dimensao em cada eixo (no minimo 1 m) para manter o contexto da estrutura
    double size = 1.0;
    for (int k = 0; k < 3; ++k) {
        size = std::max(size, bounds[2 * k + 1] - bounds[2 * k]);
    }
    for (int k = 0; k < 3; ++k) {
        bounds[2 * k] -= 1.5 * size;
        bounds[2 * k + 1] += 1.5 * size;
    }
    m_renderer->ResetCamera(bounds);
    requestRender();
}

void SceneController::updateBounds()
{
    scheduleConstructionRefresh();
}

QMatrix4x4 SceneController::compositeMatrix() const
{
    QMatrix4x4 result;
    if (!m_renderer) {
        return result;
    }
    vtkMatrix4x4 *matrix = m_renderer->GetActiveCamera()->GetCompositeProjectionTransformMatrix(
        m_renderer->GetTiledAspectRatio(), -1.0, 1.0);
    float values[16];
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            values[row * 4 + column] = static_cast<float>(matrix->GetElement(row, column));
        }
    }
    return QMatrix4x4(values);
}

vtkRenderer *SceneController::renderer() const
{
    return m_renderer.GetPointer();
}

vtkRenderWindow *SceneController::renderWindow() const
{
    return m_renderWindow.GetPointer();
}

void SceneController::scheduleConstructionRefresh()
{
    if (m_constructionRefreshPending) {
        return;
    }
    m_constructionRefreshPending = true;
    QMetaObject::invokeMethod(this, [this]() {
        m_constructionRefreshPending = false;
        refreshConstructionGeometry();
    }, Qt::QueuedConnection);
}

void SceneController::refreshConstructionGeometry()
{
    if (!m_construction) {
        return;
    }
    std::vector<Structura::Construction::Vec3> nodes;
    nodes.reserve(m_nodes.size());
    for (const auto &node : m_nodes) {
        nodes.push_back({node.x(), node.y(), node.z()});
    }
    for (const auto &p : m_extraSnapPoints) {
        nodes.push_back({p[0], p[1], p[2]});
    }
    std::vector<Structura::Construction::Vec3> midpoints;
    midpoints.reserve(m_bars.size());
    for (const auto &bar : m_bars) {
        const int startIndex = nodeIndex(bar.startNodeId());
        const int endIndex = nodeIndex(bar.endNodeId());
        if (startIndex < 0 || endIndex < 0) {
            continue;
        }
        const auto &a = m_nodes[static_cast<std::size_t>(startIndex)];
        const auto &b = m_nodes[static_cast<std::size_t>(endIndex)];
        midpoints.push_back({(a.x() + b.x()) / 2.0, (a.y() + b.y()) / 2.0, (a.z() + b.z()) / 2.0});
    }
    std::vector<std::pair<Structura::Construction::Vec3, Structura::Construction::Vec3>> segments;
    segments.reserve(m_bars.size());
    for (const auto &bar : m_bars) {
        const int startIndex = nodeIndex(bar.startNodeId());
        const int endIndex = nodeIndex(bar.endNodeId());
        if (startIndex < 0 || endIndex < 0) {
            continue;
        }
        const auto &a = m_nodes[static_cast<std::size_t>(startIndex)];
        const auto &b = m_nodes[static_cast<std::size_t>(endIndex)];
        segments.push_back({{a.x(), a.y(), a.z()}, {b.x(), b.y(), b.z()}});
    }
    m_construction->setSnapGeometry(std::move(nodes), std::move(midpoints), std::move(segments));
    emit geometryRefreshed();
}

void SceneController::setNodesVisible(bool visible)
{
    m_showNodes = visible;
    if (m_structureRenderer && m_structureRenderer->getNodeActor()) {
        m_structureRenderer->getNodeActor()->SetVisibility(visible ? 1 : 0);
    }
    requestRender();
}

void SceneController::setSupportsVisible(bool visible)
{
    m_showSupports = visible;
    if (m_supportActor) {
        m_supportActor->SetVisibility(visible && !m_supportVisuals.isEmpty() ? 1 : 0);
    }
    requestRender();
}

void SceneController::setNodalLoadVisuals(const QVector<NodalLoadVisual> &visuals)
{
    m_nodalLoadVisuals = visuals;
    updateLoadVisuals();
    requestRender();
}

void SceneController::setMemberActionVisuals(const QVector<MemberActionVisual> &visuals)
{
    m_memberActionVisuals = visuals;
    updateLoadVisuals();
    requestRender();
}

void SceneController::setLoadVisibility(bool nodeForces, bool nodeMoments, bool memberForces, bool memberMoments)
{
    m_loadVisibility = {nodeForces, nodeMoments, memberForces, memberMoments};
    updateLoadVisuals();
    requestRender();
}

void SceneController::setAreaPanels(const std::vector<Structura::Visualization::AreaPanelRenderer::Panel> &panels)
{
    m_areaPanels = panels;
    setAreaPanelsVisible(m_areaPanelsVisible);
}

bool SceneController::cameraState(double position[3], double target[3], double up[3], double &viewAngleDeg, bool &parallel) const
{
    if (!m_renderer) {
        return false;
    }
    auto *camera = m_renderer->GetActiveCamera();
    if (!camera) {
        return false;
    }
    camera->GetPosition(position);
    camera->GetFocalPoint(target);
    camera->GetViewUp(up);
    viewAngleDeg = camera->GetViewAngle();
    parallel = camera->GetParallelProjection() != 0;
    return true;
}

void SceneController::setBarTypeMarkers(const std::vector<Structura::Visualization::BarTypeRenderer::Marker> &markers)
{
    if (!m_barTypeRenderer) {
        m_barTypeRenderer = std::make_unique<Structura::Visualization::BarTypeRenderer>();
        m_barTypeRenderer->attach(m_renderer);
    }
    m_barTypeRenderer->render(markers, true);
    requestRender();
}

void SceneController::setModeShape(const std::vector<std::vector<std::array<double, 3>>> &lines)
{
    if (!m_modeShapeRenderer) {
        m_modeShapeRenderer = std::make_unique<Structura::Visualization::ModeShapeRenderer>();
        m_modeShapeRenderer->attach(m_renderer);
    }
    m_modeShapeRenderer->render(lines, true);
    requestRender();
}

void SceneController::setAreaPanelsVisible(bool visible)
{
    m_areaPanelsVisible = visible;
    if (!m_areaPanelRenderer) {
        m_areaPanelRenderer = std::make_unique<Structura::Visualization::AreaPanelRenderer>();
        m_areaPanelRenderer->attach(m_renderer);
    }
    m_areaPanelRenderer->render(m_areaPanels, visible);
    requestRender();
}

void SceneController::updateLoadVisuals()
{
    using Structura::Visualization::LoadActionRenderer;
    if (!m_loadRenderer) {
        m_loadRenderer = std::make_unique<LoadActionRenderer>();
        m_loadRenderer->attach(m_renderer);
    }

    std::vector<LoadActionRenderer::NodeAction> nodes;
    nodes.reserve(static_cast<std::size_t>(m_nodalLoadVisuals.size()));
    for (const auto &visual : m_nodalLoadVisuals) {
        LoadActionRenderer::NodeAction action;
        action.position = {visual.position.x(), visual.position.y(), visual.position.z()};
        action.force = {visual.force.x(), visual.force.y(), visual.force.z()};
        action.moment = {visual.moment.x(), visual.moment.y(), visual.moment.z()};
        action.key = {visual.sourceKind, visual.sourceIndex};
        nodes.push_back(action);
    }

    std::vector<LoadActionRenderer::MemberAction> members;
    members.reserve(static_cast<std::size_t>(m_memberActionVisuals.size()));
    auto toVec = [](const QVector3D &v) { return LoadActionRenderer::Vec3 {v.x(), v.y(), v.z()}; };
    for (const auto &visual : m_memberActionVisuals) {
        LoadActionRenderer::MemberAction action;
        action.kind = visual.moment ? LoadActionRenderer::MemberKind::Moment : LoadActionRenderer::MemberKind::Force;
        action.start = toVec(visual.start);
        action.end = toVec(visual.end);
        action.basis = {toVec(visual.xAxis), toVec(visual.yAxis), toVec(visual.zAxis)};
        action.values = toVec(visual.values);
        action.local = visual.local;
        action.selfWeight = visual.selfWeight;
        action.key = {visual.sourceKind, visual.sourceIndex};
        members.push_back(action);
    }

    LoadActionRenderer::Visibility visibility;
    visibility.nodeForces = m_loadVisibility.nodeForces;
    visibility.nodeMoments = m_loadVisibility.nodeMoments;
    visibility.memberForces = m_loadVisibility.memberForces;
    visibility.memberMoments = m_loadVisibility.memberMoments;
    m_loadRenderer->render(nodes, members, visibility, {m_highlightedLoad.kind, m_highlightedLoad.index});
    emit loadLabelsChanged();
}

void SceneController::setHighlightedLoad(const LoadRef &load)
{
    if (m_highlightedLoad == load) {
        return;
    }
    m_highlightedLoad = load;
    updateLoadVisuals();
    requestRender();
}

SceneController::LoadRef SceneController::pickLoad(const QPointF &pos, double width, double height, double tolerancePx) const
{
    if (!m_loadRenderer || width <= 0.0 || height <= 0.0) {
        return {};
    }
    const QMatrix4x4 matrix = compositeMatrix();
    constexpr float kNear = 1e-4f;
    auto clipOf = [&](const std::array<double, 3> &p) {
        return matrix * QVector4D(static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2]), 1.0f);
    };
    auto toScreen = [&](const QVector4D &c) { return QPointF((c.x() / c.w() + 1.0) * width / 2.0, (1.0 - c.y() / c.w()) * height / 2.0); };
    auto distanceToSegment = [&](QPointF a, QPointF b) {
        const QPointF d = b - a;
        const double l2 = d.x() * d.x() + d.y() * d.y();
        double t = l2 > 1e-12 ? ((pos.x() - a.x()) * d.x() + (pos.y() - a.y()) * d.y()) / l2 : 0.0;
        t = std::clamp(t, 0.0, 1.0);
        const QPointF q = a + d * t;
        return std::hypot(pos.x() - q.x(), pos.y() - q.y());
    };
    // segmento do espaco (recortado no plano proximo) -> distancia em pixels ao cursor
    auto segmentDistance = [&](const std::array<double, 3> &p0, const std::array<double, 3> &p1) {
        QVector4D c0 = clipOf(p0), c1 = clipOf(p1);
        const bool in0 = c0.w() > kNear, in1 = c1.w() > kNear;
        if (!in0 && !in1) {
            return 1e18;
        }
        if (!in0) {
            c0 = c0 + (c1 - c0) * ((kNear - c0.w()) / (c1.w() - c0.w()));
        } else if (!in1) {
            c1 = c1 + (c0 - c1) * ((kNear - c1.w()) / (c0.w() - c1.w()));
        }
        return distanceToSegment(toScreen(c0), toScreen(c1));
    };
    LoadRef best;
    double bestDistance = tolerancePx;
    for (const auto &item : m_loadRenderer->pickItems()) {
        double distance = 1e18;
        for (const auto &segment : item.segments) {
            distance = std::min(distance, segmentDistance(segment[0], segment[1]));
        }
        for (const auto &quad : item.quads) {
            // arestas livres da faixa (a aresta 0-1 e a propria barra e fica de fora)
            distance = std::min({distance, segmentDistance(quad[1], quad[2]), segmentDistance(quad[2], quad[3]), segmentDistance(quad[3], quad[0])});
            // interior da faixa, afastado da barra
            std::array<QVector4D, 4> clip {clipOf(quad[0]), clipOf(quad[1]), clipOf(quad[2]), clipOf(quad[3])};
            if (std::all_of(clip.begin(), clip.end(), [&](const QVector4D &c) { return c.w() > kNear; })) {
                const std::array<QPointF, 4> s {toScreen(clip[0]), toScreen(clip[1]), toScreen(clip[2]), toScreen(clip[3])};
                bool inside = false;
                for (int i = 0, j = 3; i < 4; j = i++) {
                    if (((s[static_cast<std::size_t>(i)].y() > pos.y()) != (s[static_cast<std::size_t>(j)].y() > pos.y()))
                        && (pos.x() < (s[static_cast<std::size_t>(j)].x() - s[static_cast<std::size_t>(i)].x()) * (pos.y() - s[static_cast<std::size_t>(i)].y())
                                              / (s[static_cast<std::size_t>(j)].y() - s[static_cast<std::size_t>(i)].y()) + s[static_cast<std::size_t>(i)].x())) {
                        inside = !inside;
                    }
                }
                if (inside && distanceToSegment(s[0], s[1]) > tolerancePx * 0.8) {
                    distance = 0.0;
                }
            }
        }
        if (distance < bestDistance) {
            bestDistance = distance;
            best = {item.key.kind, item.key.index};
        }
    }
    return best;
}

void SceneController::setSolidMembers(const QVector<SolidMemberVisual> &members)
{
    using Structura::Visualization::SolidMemberRenderer;
    if (!m_solidRenderer) {
        m_solidRenderer = std::make_unique<SolidMemberRenderer>();
        m_solidRenderer->attach(m_renderer);
    }
    auto toVec = [](const QVector3D &v) { return SolidMemberRenderer::Vec3 {v.x(), v.y(), v.z()}; };
    std::vector<SolidMemberRenderer::Member> list;
    list.reserve(static_cast<std::size_t>(members.size()));
    for (const auto &visual : members) {
        SolidMemberRenderer::Member member;
        member.start = toVec(visual.start);
        member.end = toVec(visual.end);
        member.xAxis = toVec(visual.xAxis);
        member.yAxis = toVec(visual.yAxis);
        member.zAxis = toVec(visual.zAxis);
        member.family = visual.family;
        member.parameters = visual.parameters;
        member.mirrored = visual.mirrored;
        member.color = {{visual.color[0], visual.color[1], visual.color[2]}};
        member.dimmed = visual.dimmed;
        list.push_back(std::move(member));
    }
    m_solidBarIds.clear();
    for (const auto &visual : members) {
        m_solidBarIds.append(visual.barId);
    }
    m_solidRenderer->setMembers(list);
    applySolidSelection();
    requestRender();
}

void SceneController::setElementAppearance(const Structura::Visualization::AppearanceMap &nodes,
                                           const Structura::Visualization::AppearanceMap &bars)
{
    m_structureRenderer->setNodeAppearance(nodes);
    m_structureRenderer->setBarAppearance(bars);
    requestRender();
}

void SceneController::lookFromDirection(double dx, double dy, double dz, double ux, double uy, double uz)
{
    auto *camera = m_renderer->GetActiveCamera();
    double focal[3];
    camera->GetFocalPoint(focal);
    if (!m_nodes.empty()) {
        double bounds[6];
        m_structureRenderer->getNodeActor()->GetBounds(bounds);
        for (int i = 0; i < 3; ++i) {
            focal[i] = 0.5 * (bounds[2 * i] + bounds[2 * i + 1]);
        }
    }
    const double norm = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (norm < 1e-12) {
        return;
    }
    camera->SetFocalPoint(focal);
    camera->SetPosition(focal[0] + dx / norm, focal[1] + dy / norm, focal[2] + dz / norm);
    camera->SetViewUp(ux, uy, uz);
    m_renderer->ResetCamera();
    emit cameraChanged();
    requestRender();
}

void SceneController::setCameraState(const double position[3], const double target[3], const double up[3], double viewAngleDeg, bool parallel)
{
    auto *camera = m_renderer->GetActiveCamera();
    camera->SetPosition(position[0], position[1], position[2]);
    camera->SetFocalPoint(target[0], target[1], target[2]);
    camera->SetViewUp(up[0], up[1], up[2]);
    camera->SetViewAngle(viewAngleDeg);
    camera->SetParallelProjection(parallel ? 1 : 0);
    m_renderer->ResetCameraClippingRange();
    emit cameraChanged();
    requestRender();
}

bool SceneController::isPerspective() const
{
    return m_renderer && m_renderer->GetActiveCamera() && m_renderer->GetActiveCamera()->GetParallelProjection() == 0;
}

void SceneController::setPerspective(bool perspective)
{
    auto *camera = m_renderer->GetActiveCamera();
    if (!camera || perspective == isPerspective()) {
        return;
    }
    constexpr double kPi = 3.14159265358979323846;
    const double halfAngle = 0.5 * camera->GetViewAngle() * kPi / 180.0;
    if (!perspective) {
        // perspectiva -> paralela: a escala paralela reproduz a largura visivel no plano do alvo
        camera->SetParallelScale(camera->GetDistance() * std::tan(halfAngle));
        camera->ParallelProjectionOn();
    } else {
        // paralela -> perspectiva: afasta a camera ate a distancia que enquadra a mesma altura
        double focal[3], dop[3];
        camera->GetFocalPoint(focal);
        camera->GetDirectionOfProjection(dop);
        const double distance = camera->GetParallelScale() / std::max(1e-6, std::tan(halfAngle));
        camera->ParallelProjectionOff();
        camera->SetPosition(focal[0] - dop[0] * distance, focal[1] - dop[1] * distance, focal[2] - dop[2] * distance);
    }
    m_renderer->ResetCameraClippingRange();
    emit cameraChanged();
    requestRender();
}

void SceneController::lookAtPlane(int normalAxis, double level)
{
    auto *camera = m_renderer->GetActiveCamera();
    double focal[3];
    camera->GetFocalPoint(focal);
    if (!m_nodes.empty()) {
        double bounds[6];
        m_structureRenderer->getNodeActor()->GetBounds(bounds);
        for (int i = 0; i < 3; ++i) {
            focal[i] = 0.5 * (bounds[2 * i] + bounds[2 * i + 1]);
        }
    }
    focal[normalAxis] = level;
    // Z: vista de cima (X para a direita); Y: vista frontal; X: vista lateral (Y para a direita).
    double direction[3] = {0.0, 0.0, 0.0};
    double up[3] = {0.0, 0.0, 1.0};
    if (normalAxis == 2) {
        direction[2] = 1.0;
        up[1] = 1.0;
        up[2] = 0.0;
    } else if (normalAxis == 1) {
        direction[1] = -1.0;
    } else {
        direction[0] = 1.0;
    }
    camera->SetFocalPoint(focal);
    camera->SetPosition(focal[0] + direction[0], focal[1] + direction[1], focal[2] + direction[2]);
    camera->SetViewUp(up);
    m_renderer->ResetCamera();
    emit cameraChanged();
    requestRender();
}

void SceneController::setSolidsVisible(bool visible)
{
    using Structura::Visualization::SolidMemberRenderer;
    if (!m_solidRenderer) {
        m_solidRenderer = std::make_unique<SolidMemberRenderer>();
        m_solidRenderer->attach(m_renderer);
    }
    m_solidRenderer->setVisible(visible);
    requestRender();
}

void SceneController::setConnectionMesh(const Structura::Visualization::ConnectionRenderer::Mesh &mesh)
{
    if (!m_connectionRenderer) {
        m_connectionRenderer = std::make_unique<Structura::Visualization::ConnectionRenderer>();
        m_connectionRenderer->attach(m_renderer);
    }
    m_connectionRenderer->setMesh(mesh);
    requestRender();
}

void SceneController::setNodeHighlight(const std::optional<QVector3D> &position)
{
    if (!m_nodeHighlight) {
        auto sphere = vtkSmartPointer<vtkSphereSource>::New();
        sphere->SetThetaResolution(28);
        sphere->SetPhiResolution(20);
        sphere->SetRadius(1.0);
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(sphere->GetOutputPort());
        m_nodeHighlight = vtkSmartPointer<vtkActor>::New();
        m_nodeHighlight->SetMapper(mapper);
        m_nodeHighlight->PickableOff();
        m_nodeHighlight->GetProperty()->SetColor(1.0, 0.55, 0.0);
        m_nodeHighlight->GetProperty()->SetOpacity(0.55);
        m_nodeHighlight->GetProperty()->SetAmbient(0.8);
        m_renderer->AddActor(m_nodeHighlight);
    }
    if (!position) {
        m_nodeHighlight->SetVisibility(0);
        requestRender();
        return;
    }
    // raio proporcional ao tamanho do modelo
    double lo[3] = {1e18, 1e18, 1e18}, hi[3] = {-1e18, -1e18, -1e18};
    for (const auto &node : nodeInfos()) {
        const double p[3] = {node.x, node.y, node.z};
        for (int i = 0; i < 3; ++i) {
            lo[i] = std::min(lo[i], p[i]);
            hi[i] = std::max(hi[i], p[i]);
        }
    }
    const double diagonal = std::sqrt((hi[0] - lo[0]) * (hi[0] - lo[0]) + (hi[1] - lo[1]) * (hi[1] - lo[1]) + (hi[2] - lo[2]) * (hi[2] - lo[2]));
    const double radius = std::max(0.08, 0.025 * diagonal);
    m_nodeHighlight->SetPosition(position->x(), position->y(), position->z());
    m_nodeHighlight->SetScale(radius);
    m_nodeHighlight->SetVisibility(1);
    requestRender();
}

void SceneController::setShellMesh(const Structura::Shell::RenderMesh &mesh)
{
    if (!m_shellRenderer) {
        m_shellRenderer = std::make_unique<Structura::Visualization::ShellRenderer>();
        m_shellRenderer->attach(m_renderer);
    }
    m_shellRenderer->setMesh(mesh);
    requestRender();
}

int SceneController::pickShellQuad(int displayX, int displayY) const
{
    return m_shellRenderer ? m_shellRenderer->pickQuad(displayX, displayY) : -1;
}

void SceneController::setExtraSnapPoints(std::vector<std::array<double, 3>> points)
{
    m_extraSnapPoints = std::move(points);
    refreshConstructionGeometry();
}

void SceneController::setShellVisible(bool visible)
{
    if (!m_shellRenderer) {
        m_shellRenderer = std::make_unique<Structura::Visualization::ShellRenderer>();
        m_shellRenderer->attach(m_renderer);
    }
    m_shellRenderer->setVisible(visible);
    requestRender();
}

void SceneController::setConnectionsVisible(bool visible)
{
    if (!m_connectionRenderer) {
        m_connectionRenderer = std::make_unique<Structura::Visualization::ConnectionRenderer>();
        m_connectionRenderer->attach(m_renderer);
    }
    m_connectionRenderer->setVisible(visible);
    requestRender();
}

LoadLabelData SceneController::loadLabels() const
{
    LoadLabelData result;
    if (!m_loadRenderer) {
        return result;
    }
    const auto &labels = m_loadRenderer->labels();
    result.positions = labels.positions;
    result.texts = labels.texts;
    result.colors = labels.colors;
    result.weights = labels.weights;
    return result;
}

void SceneController::placeOrientationMarker(int widgetWidth, int widgetHeight, int sizePx, int marginPx)
{
    if (!m_orientationMarker || widgetWidth < 50 || widgetHeight < 50) {
        return;
    }
    const double w = widgetWidth, h = widgetHeight;
    m_orientationMarker->SetViewport(marginPx / w, 1.0 - (marginPx + sizePx) / h, (marginPx + sizePx) / w, 1.0 - marginPx / h);
}

int SceneController::viewportHeight() const
{
    if (!m_renderWindow) return 0;
    int* size = m_renderWindow->GetSize();
    return size ? size[1] : 0;
}

int SceneController::nodeCount() const
{
    return static_cast<int>(m_nodes.size());
}

std::vector<SceneController::NodeInfo> SceneController::nodeInfos() const
{
    std::vector<NodeInfo> result;
    result.reserve(m_nodes.size());
    for (const auto &node : m_nodes) {
        const auto pos = node.position();
        result.push_back(NodeInfo{ node.id(), node.externalId(), pos[0], pos[1], pos[2] });
    }
    return result;
}

const SceneController::Node *SceneController::findNode(const QUuid &id) const
{
    const int index = nodeIndex(id);
    if (index < 0) {
        return nullptr;
    }
    return &m_nodes[static_cast<std::size_t>(index)];
}

SceneController::Node *SceneController::findNode(const QUuid &id)
{
    const int index = nodeIndex(id);
    if (index < 0) {
        return nullptr;
    }
    return &m_nodes[static_cast<std::size_t>(index)];
}

QUuid SceneController::pickNode(int displayX, int displayY) const
{
    if (!m_nodePicker || !m_renderer) {
        return {};
    }
    if (m_nodePicker->Pick(displayX, displayY, 0.0, m_renderer)) {
        vtkIdType pid = m_nodePicker->GetPointId();
        if (pid >= 0 && pid < static_cast<vtkIdType>(m_nodes.size())) {
            return m_nodes[static_cast<std::size_t>(pid)].id();
        }
    }
    return {};
}

QUuid SceneController::pickBar(int displayX, int displayY) const
{
    if (!m_barPicker || !m_renderer) {
        return {};
    }
    if (m_barPicker->Pick(displayX, displayY, 0.0, m_renderer)) {
        vtkIdType cid = m_barPicker->GetCellId();
        if (cid >= 0 && cid < static_cast<vtkIdType>(m_bars.size())) {
            return m_bars[static_cast<std::size_t>(cid)].id();
        }
    }
    return {};
}

void SceneController::setHighlightedNode(const QUuid &nodeId)
{
    m_structureRenderer->setHoveredNode(nodeId);
    requestRender();
}

void SceneController::clearHighlightedNode()
{
    m_structureRenderer->clearHoveredNode();
    requestRender();
}

void SceneController::setSelectedNodes(const QSet<QUuid> &nodeIds)
{
    m_structureRenderer->setSelectedNodes(nodeIds);
    requestRender();
}

bool SceneController::updateNodePosition(const QUuid &nodeId, double x, double y, double z)
{
    QVector<QUuid> ids;
    ids.append(nodeId);
    QVector<QVector3D> positions;
    positions.append(QVector3D(x, y, z));
    return updateNodePositions(ids, positions);
}

bool SceneController::updateNodePositions(const QVector<QUuid> &nodeIds, const QVector<QVector3D> &positions)
{
    if (nodeIds.size() != positions.size()) {
        return false;
    }
    bool changed = false;
    for (int i = 0; i < nodeIds.size(); ++i) {
        const QUuid &id = nodeIds.at(i);
        const int idx = nodeIndex(id);
        if (idx < 0) {
            continue;
        }
        Node &node = m_nodes[static_cast<std::size_t>(idx)];
        const QVector3D pos = positions.at(i);
        const auto current = node.position();
        if (qFuzzyCompare(current[0] + 1.0, pos.x() + 1.0) &&
            qFuzzyCompare(current[1] + 1.0, pos.y() + 1.0) &&
            qFuzzyCompare(current[2] + 1.0, pos.z() + 1.0)) {
            continue;
        }

        m_structureRenderer->updateNodePosition(id, pos.x(), pos.y(), pos.z());
        node.setPosition(pos.x(), pos.y(), pos.z());
        changed = true;
    }

    if (changed) {
        m_structureRenderer->endUpdate();
        updateBounds();
        requestRender();
    }
    return changed;
}

QUuid SceneController::addBar(const QUuid &startNodeId,
                              const QUuid &endNodeId,
                              const QUuid &materialId,
                              const QUuid &sectionId)
{
    const int startIndex = nodeIndex(startNodeId);
    const int endIndex = nodeIndex(endNodeId);
    if (startIndex < 0 || endIndex < 0 || startIndex == endIndex) {
        return {};
    }
    const QUuid barId = QUuid::createUuid();
    Bar bar(barId, startNodeId, endNodeId, materialId, sectionId);
    m_bars.push_back(bar);
    m_barIndexById.insert(barId, static_cast<int>(m_bars.size()) - 1);
    
    m_structureRenderer->addBar(barId, startNodeId, endNodeId);

    scheduleConstructionRefresh();
    updateBarLCSVisuals();
    requestRender();
    return barId;
}

void SceneController::assignBarProperties(const std::vector<QUuid> &barIds,
                                          const std::optional<QUuid> &materialId,
                                          const std::optional<QUuid> &sectionId)
{
    bool changed = false;
    for (const QUuid &id : barIds) {
        const int idx = barIndex(id);
        if (idx < 0) {
            continue;
        }
        Bar &bar = m_bars[static_cast<std::size_t>(idx)];
        if (materialId.has_value()) {
            const QUuid newMat = materialId.value();
            if (bar.materialId() != newMat) {
                bar.setMaterialId(newMat);
                changed = true;
            }
        }
        if (sectionId.has_value()) {
            const QUuid newSec = sectionId.value();
            if (bar.sectionId() != newSec) {
                bar.setSectionId(newSec);
                changed = true;
            }
        }
    }
    if (changed) {
        scheduleConstructionRefresh();
        requestRender();
    }
}

std::vector<SceneController::BarInfo> SceneController::bars() const
{
    std::vector<BarInfo> result;
    result.reserve(m_bars.size());
    for (const auto &bar : m_bars) {
        result.push_back(BarInfo{
            bar.id(),
            bar.startNodeId(),
            bar.endNodeId(),
            bar.materialId(),
            bar.sectionId(),
            bar.externalId()
        });
    }
    return result;
}

const SceneController::Bar *SceneController::findBar(const QUuid &id) const
{
    const int idx = barIndex(id);
    if (idx < 0) {
        return nullptr;
    }
    return &m_bars[static_cast<std::size_t>(idx)];
}

SceneController::Bar *SceneController::findBar(const QUuid &id)
{
    const int idx = barIndex(id);
    if (idx < 0) {
        return nullptr;
    }
    return &m_bars[static_cast<std::size_t>(idx)];
}

void SceneController::setSelectedBars(const QSet<QUuid> &barIds)
{
    m_structureRenderer->setSelectedBars(barIds);
    m_solidSelection = barIds;
    applySolidSelection();
    requestRender();
}

void SceneController::applySolidSelection()
{
    if (!m_solidRenderer) {
        return;
    }
    std::vector<bool> selected(static_cast<std::size_t>(m_solidBarIds.size()), false);
    for (int i = 0; i < m_solidBarIds.size(); ++i) {
        selected[static_cast<std::size_t>(i)] = m_solidSelection.contains(m_solidBarIds[i]);
    }
    m_solidRenderer->setSelected(selected);
}

void SceneController::setBarExternalId(const QUuid &barId, int externalId)
{
    Bar *bar = findBar(barId);
    if (!bar) {
        return;
    }
    bar->setExternalId(externalId);
    scheduleConstructionRefresh();
}

SceneController::GeometrySnapshot SceneController::captureGeometry() const
{
    GeometrySnapshot g;
    g.nodes.reserve(m_nodes.size());
    for (const auto &node : m_nodes) {
        g.nodes.push_back({node.id(), node.externalId(), node.x(), node.y(), node.z()});
    }
    g.bars.reserve(m_bars.size());
    for (const auto &bar : m_bars) {
        g.bars.push_back({bar.id(), bar.startNodeId(), bar.endNodeId(), bar.materialId(), bar.sectionId(), bar.externalId()});
    }
    return g;
}

void SceneController::restoreGeometry(const GeometrySnapshot &g)
{
    m_structureRenderer->clear();
    m_nodes.clear();
    m_nodeIndexById.clear();
    m_bars.clear();
    m_barIndexById.clear();
    m_highlightNodeId = QUuid();
    m_selectedNodeIds.clear();
    m_selectedBarIds.clear();
    for (const auto &n : g.nodes) {
        m_nodes.push_back(Node(n.id, n.externalId, n.x, n.y, n.z));
        m_nodeIndexById.insert(n.id, static_cast<int>(m_nodes.size()) - 1);
        m_structureRenderer->addNode(n.id, n.x, n.y, n.z);
        m_nextNodeExternalId = std::max(m_nextNodeExternalId, n.externalId + 1);
    }
    for (const auto &b : g.bars) {
        if (nodeIndex(b.start) < 0 || nodeIndex(b.end) < 0) {
            continue;
        }
        Bar bar(b.id, b.start, b.end, b.material, b.section);
        bar.setExternalId(b.externalId);
        m_bars.push_back(bar);
        m_barIndexById.insert(b.id, static_cast<int>(m_bars.size()) - 1);
        m_structureRenderer->addBar(b.id, b.start, b.end);
    }
    updateBounds();
    scheduleConstructionRefresh();
    updateBarLCSVisuals();
    requestRender();
}

void SceneController::clearAll()
{
    clearNodes();
    clearBars();
    clearLoads();
    clearSupports();

    rebuildBarLCSVisuals();
    scheduleConstructionRefresh();

    if (m_renderer) {
        m_renderer->ResetCamera();
    }
    requestRender();
}


void SceneController::setSupportVisuals(const QVector<SupportVisual> &visuals)
{
    m_supportVisuals = visuals;
    updateSupportVisuals();
    requestRender();
}

void SceneController::updateSupportVisuals()
{
    using namespace Structura::Visualization;

    if (!m_supportMapper && m_renderer) {
        m_supportMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        m_supportActor = vtkSmartPointer<vtkActor>::New();
        m_supportActor->SetMapper(m_supportMapper);
        // apoios: cinza com arestas escuras
        m_supportActor->GetProperty()->SetColor(0xa8 / 255.0, 0xb0 / 255.0, 0xb9 / 255.0);
        m_supportActor->GetProperty()->EdgeVisibilityOn();
        m_supportActor->GetProperty()->SetEdgeColor(0x6e / 255.0, 0x77 / 255.0, 0x81 / 255.0);
        m_supportActor->GetProperty()->SetLineWidth(1.5);
        m_supportActor->PickableOff();
        m_renderer->AddActor(m_supportActor);

        // restricoes nodais: laranja, para nao se confundirem com apoios
        m_constraintMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        m_constraintActor = vtkSmartPointer<vtkActor>::New();
        m_constraintActor->SetMapper(m_constraintMapper);
        m_constraintActor->GetProperty()->SetColor(0.93, 0.52, 0.12);
        m_constraintActor->GetProperty()->EdgeVisibilityOn();
        m_constraintActor->GetProperty()->SetEdgeColor(0.55, 0.28, 0.04);
        m_constraintActor->GetProperty()->SetLineWidth(1.0);
        m_constraintActor->PickableOff();
        m_renderer->AddActor(m_constraintActor);
    }

    std::vector<SupportMeshBuilder::Support> all, supports, constraints;
    all.reserve(static_cast<std::size_t>(m_supportVisuals.size()));
    for (const auto &visual : m_supportVisuals) {
        SupportMeshBuilder::Support support;
        support.position = {visual.position.x(), visual.position.y(), visual.position.z()};
        for (int i = 0; i < 6; ++i) {
            support.restraints[static_cast<std::size_t>(i)] = visual.restraints[static_cast<std::size_t>(i)];
        }
        (SupportMeshBuilder::isSupport(support.restraints) ? supports : constraints).push_back(support);
    }
    // tamanho dos simbolos proporcional ao modelo (minimo de 0,30 m)
    double unit = SupportMeshBuilder::kSupportSize;
    if (!m_nodes.empty()) {
        double lo[3] = {1e18, 1e18, 1e18}, hi[3] = {-1e18, -1e18, -1e18};
        for (const auto &node : m_nodes) {
            const double p[3] = {node.x(), node.y(), node.z()};
            for (int i = 0; i < 3; ++i) {
                lo[i] = std::min(lo[i], p[i]);
                hi[i] = std::max(hi[i], p[i]);
            }
        }
        const double diagonal = std::sqrt((hi[0] - lo[0]) * (hi[0] - lo[0]) + (hi[1] - lo[1]) * (hi[1] - lo[1]) + (hi[2] - lo[2]) * (hi[2] - lo[2]));
        unit = std::max(unit, 0.02 * diagonal);
    }
    if (m_supportMapper) {
        m_supportMapper->SetInputData(SupportMeshBuilder::buildSupports(supports, unit));
        m_supportActor->SetVisibility(!supports.empty() && m_showSupports);
        m_constraintMapper->SetInputData(SupportMeshBuilder::buildConstraints(constraints, unit));
        m_constraintActor->SetVisibility(!constraints.empty() && m_showConstraints);
    }
}

void SceneController::setConstraintsVisible(bool visible)
{
    m_showConstraints = visible;
    updateSupportVisuals();
    requestRender();
}

int SceneController::nodeIndex(const QUuid &id) const
{
    if (id.isNull()) {
        return -1;
    }
    const auto it = m_nodeIndexById.constFind(id);
    if (it == m_nodeIndexById.constEnd()) {
        return -1;
    }
    return it.value();
}

int SceneController::barIndex(const QUuid &id) const
{
    if (id.isNull()) {
        return -1;
    }
    const auto it = m_barIndexById.constFind(id);
    if (it == m_barIndexById.constEnd()) {
        return -1;
    }
    return it.value();
}

void SceneController::setShowBarLCS(bool show)
{
    m_showBarLCS = show;
    if (show) {
        rebuildBarLCSVisuals();
    }
    m_lcsActor->SetVisibility(show);
    requestRender();
}

void SceneController::updateBarLCSVisuals()
{
    if (m_showBarLCS) {
        rebuildBarLCSVisuals();
        requestRender();
    }
}

void SceneController::setBarSectionRotations(const QHash<int, double> &rotationsByExternalId)
{
    if (m_barSectionRotations == rotationsByExternalId) {
        return;
    }
    m_barSectionRotations = rotationsByExternalId;
    updateBarLCSVisuals();
}

void SceneController::rebuildBarLCSVisuals()
{
    m_lcsPoints->Reset();
    m_lcsCells->Reset();
    m_lcsColors->Reset();
    
    if (m_bars.empty()) {
        m_lcsData->Modified();
        return;
    }
    
    Structura::Geometry::DefaultLocalAxisProvider lcsProvider;
    
    // Colors for each axis: X' = red, Y' = green, Z' = blue
    unsigned char xColor[3] = {255, 0, 0};    // Red
    unsigned char yColor[3] = {0, 255, 0};    // Green
    unsigned char zColor[3] = {0, 0, 255};    // Blue
    
    // Arrow parameters
    constexpr double arrowLength = 0.15;        // Total arrow length
    constexpr double arrowHeadLength = 0.04;    // Length of arrow head
    constexpr double arrowHeadWidth = 0.02;     // Half-width of arrow head
    
    for (const auto &bar : m_bars) {
        // Get start and end node positions
        const Node *startNode = findNode(bar.startNodeId());
        const Node *endNode = findNode(bar.endNodeId());
        
        if (!startNode || !endNode) {
            continue;
        }
        
        std::array<double, 3> pointA = {startNode->x(), startNode->y(), startNode->z()};
        std::array<double, 3> pointB = {endNode->x(), endNode->y(), endNode->z()};
        
        // Compute bar length
        double dx = pointB[0] - pointA[0];
        double dy = pointB[1] - pointA[1];
        double dz = pointB[2] - pointA[2];
        double barLength = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        if (barLength < 1e-9) {
            continue; // Skip degenerate bars
        }
        
        // Compute LCS - convert Vector3 kPoint to array format if present
        std::optional<std::array<double, 3>> kPointArray;
        if (bar.kPoint().has_value()) {
            const auto &kp = bar.kPoint().value();
            kPointArray = std::array<double, 3>{{kp.x(), kp.y(), kp.z()}};
        }
        
        try {
            auto lcs = Structura::Geometry::rotatedAboutX(lcsProvider.computeLCS(pointA, pointB, kPointArray),
                                                          m_barSectionRotations.value(bar.externalId(), 0.0));
            
            // Origin point (midpoint of bar)
            const auto &origin = lcs.origin;
            
            // Helper lambda to create an arrow with line-based head
            auto createArrow = [&](const std::array<double, 3> &direction, 
                                   const unsigned char color[3]) {
                // Arrow tip point
                double tipX = origin[0] + direction[0] * arrowLength;
                double tipY = origin[1] + direction[1] * arrowLength;
                double tipZ = origin[2] + direction[2] * arrowLength;
                
                // Arrow base (where head starts)
                double baseX = origin[0] + direction[0] * (arrowLength - arrowHeadLength);
                double baseY = origin[1] + direction[1] * (arrowLength - arrowHeadLength);
                double baseZ = origin[2] + direction[2] * (arrowLength - arrowHeadLength);
                
                // Main shaft line (origin to base)
                vtkIdType p0 = m_lcsPoints->InsertNextPoint(origin[0], origin[1], origin[2]);
                vtkIdType pTip = m_lcsPoints->InsertNextPoint(tipX, tipY, tipZ);
                
                m_lcsCells->InsertNextCell(2);
                m_lcsCells->InsertCellPoint(p0);
                m_lcsCells->InsertCellPoint(pTip);
                m_lcsColors->InsertNextTypedTuple(color);
                
                // Create arrow head (two lines forming a V)
                // We need two perpendicular vectors to the arrow direction
                std::array<double, 3> perp1, perp2;
                
                // Find first perpendicular vector
                if (std::abs(direction[0]) < 0.9) {
                    perp1[0] = 0.0;
                    perp1[1] = -direction[2];
                    perp1[2] = direction[1];
                } else {
                    perp1[0] = -direction[1];
                    perp1[1] = direction[0];
                    perp1[2] = 0.0;
                }
                
                // Normalize perp1
                double len1 = std::sqrt(perp1[0]*perp1[0] + perp1[1]*perp1[1] + perp1[2]*perp1[2]);
                perp1[0] /= len1; perp1[1] /= len1; perp1[2] /= len1;
                
                // Second perpendicular (cross product)
                perp2[0] = direction[1]*perp1[2] - direction[2]*perp1[1];
                perp2[1] = direction[2]*perp1[0] - direction[0]*perp1[2];
                perp2[2] = direction[0]*perp1[1] - direction[1]*perp1[0];
                
                // Create 4 points around the base forming arrow head
                for (int i = 0; i < 4; ++i) {
                    double angle = i * M_PI / 2.0;
                    double px = baseX + (perp1[0] * std::cos(angle) + perp2[0] * std::sin(angle)) * arrowHeadWidth;
                    double py = baseY + (perp1[1] * std::cos(angle) + perp2[1] * std::sin(angle)) * arrowHeadWidth;
                    double pz = baseZ + (perp1[2] * std::cos(angle) + perp2[2] * std::sin(angle)) * arrowHeadWidth;
                    
                    vtkIdType pSide = m_lcsPoints->InsertNextPoint(px, py, pz);
                    
                    // Line from side point to tip
                    m_lcsCells->InsertNextCell(2);
                    m_lcsCells->InsertCellPoint(pSide);
                    m_lcsCells->InsertCellPoint(pTip);
                    m_lcsColors->InsertNextTypedTuple(color);
                }
            };
            
            // Create arrows for each axis
            createArrow(lcs.xPrime, xColor);
            createArrow(lcs.yPrime, yColor);
            createArrow(lcs.zPrime, zColor);
            
        } catch (const std::exception &) {
            // Skip bars that fail LCS computation
            continue;
        }
    }
    
    m_lcsData->Modified();
}
