#include "viz/SceneController.h"
#include "viz/LocalCoordinateSystem.h"
#include "viz/construction/ConstructionScene.h"
#include "viz/structure/SupportMeshBuilder.h"
#include "viz/loads/LoadActionRenderer.h"
#include "viz/structure/SolidMemberRenderer.h"

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
    if (auto *renderWindow = m_renderWindow.GetPointer()) {
        renderWindow->Render();
    }
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
    m_construction->setSnapGeometry(std::move(nodes), std::move(midpoints));
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
        members.push_back(action);
    }

    LoadActionRenderer::Visibility visibility;
    visibility.nodeForces = m_loadVisibility.nodeForces;
    visibility.nodeMoments = m_loadVisibility.nodeMoments;
    visibility.memberForces = m_loadVisibility.memberForces;
    visibility.memberMoments = m_loadVisibility.memberMoments;
    m_loadRenderer->render(nodes, members, visibility);
    emit loadLabelsChanged();
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
    m_solidRenderer->setMembers(list);
    requestRender();
}

void SceneController::setElementAppearance(const Structura::Visualization::AppearanceMap &nodes,
                                           const Structura::Visualization::AppearanceMap &bars)
{
    m_structureRenderer->setNodeAppearance(nodes);
    m_structureRenderer->setBarAppearance(bars);
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

LoadLabelData SceneController::loadLabels() const
{
    LoadLabelData result;
    if (!m_loadRenderer) {
        return result;
    }
    const auto &labels = m_loadRenderer->labels();
    result.positions = labels.positions;
    result.texts = labels.texts;
    return result;
}

bool SceneController::pickWorldPoint(int displayX, int displayY, double &x, double &y, double &z) const
{
    if (!m_renderer || !m_renderWindow) return false;
    if (!m_picker) return false;
    // Perform pick against scene (grid and points are both pickable)
    if (m_picker->Pick(displayX, displayY, 0, m_renderer)) {
        double p[3];
        m_picker->GetPickPosition(p);
        x = p[0]; y = p[1]; z = p[2];
        return true;
    }
    return false;
}

int SceneController::viewportHeight() const
{
    if (!m_renderWindow) return 0;
    int* size = m_renderWindow->GetSize();
    return size ? size[1] : 0;
}

bool SceneController::worldPointOnPlaneZ0(int displayX, int displayY, double &x, double &y, double &z) const
{
    if (!m_renderer) return false;

    // 1) Try ray from display near/far to Z=0
    double p0w[4], p1w[4];
    m_renderer->SetDisplayPoint(displayX, displayY, 0.0);
    m_renderer->DisplayToWorld();
    m_renderer->GetWorldPoint(p0w);
    if (std::abs(p0w[3]) > 1e-14) { p0w[0]/=p0w[3]; p0w[1]/=p0w[3]; p0w[2]/=p0w[3]; }

    m_renderer->SetDisplayPoint(displayX, displayY, 1.0);
    m_renderer->DisplayToWorld();
    m_renderer->GetWorldPoint(p1w);
    if (std::abs(p1w[3]) > 1e-14) { p1w[0]/=p1w[3]; p1w[1]/=p1w[3]; p1w[2]/=p1w[3]; }

    double dx = p1w[0] - p0w[0];
    double dy = p1w[1] - p0w[1];
    double dz = p1w[2] - p0w[2];
    const double eps = 1e-14;
    if (std::abs(dz) < eps) {
        // 2) Fallback: build a ray from camera position through far world point
        auto *cam = m_renderer->GetActiveCamera();
        if (!cam) return false;
        double cpos[3]; cam->GetPosition(cpos);
        dx = p1w[0] - cpos[0];
        dy = p1w[1] - cpos[1];
        dz = p1w[2] - cpos[2];
        if (std::abs(dz) < eps) return false;
        const double t = -cpos[2] / dz;
        x = cpos[0] + t * dx;
        y = cpos[1] + t * dy;
        z = 0.0;
        return std::isfinite(x) && std::isfinite(y);
    }

    const double t = -p0w[2] / dz; // z=0 => p0.z + t*dz = 0
    x = p0w[0] + t * dx;
    y = p0w[1] + t * dy;
    z = 0.0;
    return std::isfinite(x) && std::isfinite(y);
}

bool SceneController::worldPointOnViewPlane(int displayX, int displayY, double &x, double &y, double &z) const
{
    if (!m_renderer) return false;

    // Camera view plane: passes through focal point and is orthogonal to view direction
    auto *cam = m_renderer->GetActiveCamera();
    if (!cam) return false;
    double fpt[3];
    cam->GetFocalPoint(fpt);
    double dop[3];
    cam->GetDirectionOfProjection(dop); // normalized in VTK

    // Build ray from display near/far
    double p0w[4], p1w[4];
    m_renderer->SetDisplayPoint(displayX, displayY, 0.0);
    m_renderer->DisplayToWorld();
    m_renderer->GetWorldPoint(p0w);
    if (std::abs(p0w[3]) > 1e-14) { p0w[0]/=p0w[3]; p0w[1]/=p0w[3]; p0w[2]/=p0w[3]; }
    m_renderer->SetDisplayPoint(displayX, displayY, 1.0);
    m_renderer->DisplayToWorld();
    m_renderer->GetWorldPoint(p1w);
    if (std::abs(p1w[3]) > 1e-14) { p1w[0]/=p1w[3]; p1w[1]/=p1w[3]; p1w[2]/=p1w[3]; }

    // Intersect ray p(t) = p0 + t*(p1-p0) with plane (n·(p - f)) = 0 where n=dop
    const double rdx = p1w[0]-p0w[0];
    const double rdy = p1w[1]-p0w[1];
    const double rdz = p1w[2]-p0w[2];
    const double ndotdir = dop[0]*rdx + dop[1]*rdy + dop[2]*rdz;
    if (std::abs(ndotdir) < 1e-12) return false; // Ray parallel to view plane
    const double vx = fpt[0]-p0w[0];
    const double vy = fpt[1]-p0w[1];
    const double vz = fpt[2]-p0w[2];
    const double t = (dop[0]*vx + dop[1]*vy + dop[2]*vz) / ndotdir;
    x = p0w[0] + t*rdx;
    y = p0w[1] + t*rdy;
    z = p0w[2] + t*rdz;
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
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
    requestRender();
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
        // Cubo modular cinza com arestas escuras (representacao dos apoios).
        m_supportActor->GetProperty()->SetColor(0xa8 / 255.0, 0xb0 / 255.0, 0xb9 / 255.0);
        m_supportActor->GetProperty()->EdgeVisibilityOn();
        m_supportActor->GetProperty()->SetEdgeColor(0x6e / 255.0, 0x77 / 255.0, 0x81 / 255.0);
        m_supportActor->GetProperty()->SetLineWidth(1.5);
        m_supportActor->PickableOff();
        
        m_renderer->AddActor(m_supportActor);
    }

    std::vector<SupportMeshBuilder::Support> supports;
    supports.reserve(static_cast<std::size_t>(m_supportVisuals.size()));

    for (const auto &visual : m_supportVisuals) {
        SupportMeshBuilder::Support support;
        support.position = {visual.position.x(), visual.position.y(), visual.position.z()};
        for (int i = 0; i < 6; ++i) {
            support.restraints[static_cast<std::size_t>(i)] = visual.restraints[static_cast<std::size_t>(i)];
        }
        supports.push_back(support);
    }

    if (m_supportMapper) {
        auto polyData = SupportMeshBuilder::build(supports);
        m_supportMapper->SetInputData(polyData);
        m_supportActor->SetVisibility(!supports.empty() && m_showSupports);
    }
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
                vtkIdType pBase = m_lcsPoints->InsertNextPoint(baseX, baseY, baseZ);
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
            
        } catch (const std::exception &e) {
            // Skip bars that fail LCS computation
            continue;
        }
    }
    
    m_lcsData->Modified();
}
