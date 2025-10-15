#include "SceneController.h"

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
#include <vtkLine.h>
#include "CustomInteractorStyle.h"

SceneController::SceneController(QObject *parent)
    : QObject(parent)
    , m_orientationMarker(vtkSmartPointer<vtkOrientationMarkerWidget>::New())
    , m_points(vtkSmartPointer<vtkPoints>::New())
    , m_pointCloud(vtkSmartPointer<vtkPolyData>::New())
    , m_vertices(vtkSmartPointer<vtkCellArray>::New())
    , m_pointMapper(vtkSmartPointer<vtkPolyDataMapper>::New())
    , m_pointActor(vtkSmartPointer<vtkActor>::New())
    , m_pointColors(vtkSmartPointer<vtkUnsignedCharArray>::New())
    , m_barData(vtkSmartPointer<vtkPolyData>::New())
    , m_barLines(vtkSmartPointer<vtkCellArray>::New())
    , m_barMapper(vtkSmartPointer<vtkPolyDataMapper>::New())
    , m_barActor(vtkSmartPointer<vtkActor>::New())
    , m_gridData(vtkSmartPointer<vtkPolyData>::New())
    , m_gridMapper(vtkSmartPointer<vtkPolyDataMapper>::New())
    , m_gridActor(vtkSmartPointer<vtkActor>::New())
    , m_picker(vtkSmartPointer<vtkCellPicker>::New())
    , m_nodePicker(vtkSmartPointer<vtkPointPicker>::New())
{
    m_pointCloud->SetPoints(m_points);
    m_pointCloud->SetVerts(m_vertices);

    m_pointMapper->SetInputData(m_pointCloud);
    m_pointActor->SetMapper(m_pointMapper);
    m_pointActor->GetProperty()->SetColor(0.95, 0.32, 0.18);
    m_pointActor->GetProperty()->SetPointSize(10.0);
    m_pointActor->GetProperty()->SetRenderPointsAsSpheres(true);
    m_pointColors->SetNumberOfComponents(3);
    m_pointColors->SetName("NodeColors");
    m_pointCloud->GetPointData()->SetScalars(m_pointColors);
    m_pointMapper->ScalarVisibilityOn();

    // Bars share the same point set
    m_barData->SetPoints(m_points);
    m_barData->SetLines(m_barLines);
    m_barMapper->SetInputData(m_barData);
    m_barMapper->ScalarVisibilityOff();
    m_barActor->SetMapper(m_barMapper);
    m_barActor->GetProperty()->SetColor(0.28, 0.32, 0.40);
    m_barActor->GetProperty()->SetLineWidth(2.0);
    m_barActor->PickableOn();

    // Grid default visuals
    m_gridMapper->SetInputData(m_gridData);
    m_gridActor->SetMapper(m_gridMapper);
    m_gridActor->GetProperty()->SetColor(0.55, 0.60, 0.68);
    m_gridActor->GetProperty()->SetOpacity(0.55);
    m_gridActor->GetProperty()->SetLineWidth(1.0);
    m_gridActor->PickableOn();

    // Picker tolerance suitable for thin lines
    m_picker->SetTolerance(0.005);

    m_nodePicker->SetTolerance(0.01);
    m_nodePicker->SetPickFromList(true);
    m_nodePicker->AddPickList(m_pointActor);
}

SceneController::~SceneController() = default;

void SceneController::initialize(QVTKOpenGLNativeWidget *vtkWidget)
{
    if (!vtkWidget) {
        return;
    }

    vtkWidget->setRenderWindow(m_renderWindow);
    m_renderWindow->AddRenderer(m_renderer);
    // Background similar to SolidWorks: soft light blue gradient
    m_renderer->GradientBackgroundOn();
    m_renderer->SetBackground(0.91, 0.94, 0.98);  // top
    m_renderer->SetBackground2(0.78, 0.85, 0.93); // bottom
    m_renderer->AddActor(m_barActor);
    m_renderer->AddActor(m_pointActor);
    m_renderer->AddActor(m_gridActor);
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

    m_renderWindow->Render();
}

void SceneController::addPoint(double x, double y, double z)
{
    addPointWithId(x, y, z, m_nextNodeExternalId++);
}

int SceneController::addPointWithId(double x, double y, double z, int externalId)
{
    if (externalId <= 0) {
        externalId = m_nextNodeExternalId++;
    } else {
        if (externalId >= m_nextNodeExternalId) {
            m_nextNodeExternalId = externalId + 1;
        }
    }

    const vtkIdType pointId = m_points->InsertNextPoint(x, y, z);
    m_vertices->InsertNextCell(1);
    m_vertices->InsertCellPoint(pointId);

    NodeRecord node;
    node.pointId = pointId;
    node.position[0] = x;
    node.position[1] = y;
    node.position[2] = z;
    node.externalId = externalId;
    m_nodes.push_back(node);

    if (m_pointColors) {
        m_pointColors->InsertNextTypedTuple(m_defaultNodeColor);
        m_pointColors->Modified();
        m_pointCloud->GetPointData()->SetScalars(m_pointColors);
    }

    m_points->Modified();
    m_vertices->Modified();
    m_pointCloud->Modified();
    m_barData->Modified();

    updateBounds();
    m_renderWindow->Render();

    return static_cast<int>(m_nodes.size()) - 1;
}

void SceneController::resetCamera()
{
    m_renderer->ResetCamera();
    m_renderWindow->Render();
}

void SceneController::zoomExtents()
{
    if (m_points->GetNumberOfPoints() == 0) {
        resetCamera();
        return;
    }

    m_renderer->ResetCamera(m_pointActor->GetBounds());
    m_renderWindow->Render();
}

void SceneController::updateBounds()
{
    if (m_points->GetNumberOfPoints() == 0) {
        return;
    }

    m_pointCloud->GetBounds(); // ensures bounds computation
}

void SceneController::createGrid(double dx, double dy, double dz, int nx, int ny, int nz)
{
    m_dx = dx; m_dy = dy; m_dz = dz;
    m_nx = nx; m_ny = ny; m_nz = nz;

    auto gridPoints = vtkSmartPointer<vtkPoints>::New();
    auto gridLines = vtkSmartPointer<vtkCellArray>::New();

    // Build a 3D lattice of axis-aligned lines from origin
    const double xmax = (nx > 0 ? (nx - 1) * dx : 0.0);
    const double ymax = (ny > 0 ? (ny - 1) * dy : 0.0);
    const double zmax = (nz > 0 ? (nz - 1) * dz : 0.0);

    auto addLine = [&](double x0, double y0, double z0, double x1, double y1, double z1) {
        vtkIdType id0 = gridPoints->InsertNextPoint(x0, y0, z0);
        vtkIdType id1 = gridPoints->InsertNextPoint(x1, y1, z1);
        vtkIdType lineIds[2] = { id0, id1 };
        gridLines->InsertNextCell(2, lineIds);
    };

    // Lines parallel to X (vary y,z)
    for (int j = 0; j < ny; ++j) {
        for (int k = 0; k < nz; ++k) {
            const double y = j * dy;
            const double z = k * dz;
            addLine(0.0, y, z, xmax, y, z);
        }
    }
    // Lines parallel to Y (vary x,z)
    for (int i = 0; i < nx; ++i) {
        for (int k = 0; k < nz; ++k) {
            const double x = i * dx;
            const double z = k * dz;
            addLine(x, 0.0, z, x, ymax, z);
        }
    }
    // Lines parallel to Z (vary x,y)
    for (int i = 0; i < nx; ++i) {
        for (int j = 0; j < ny; ++j) {
            const double x = i * dx;
            const double y = j * dy;
            addLine(x, y, 0.0, x, y, zmax);
        }
    }

    m_gridData->SetPoints(gridPoints);
    m_gridData->SetLines(gridLines);
    m_gridData->Modified();
    m_renderWindow->Render();
}

bool SceneController::hasGrid() const
{
    return (m_nx > 0 && m_ny > 0 && m_nz > 0 && m_dx > 0 && m_dy > 0 && m_dz > 0);
}

static inline double roundToStepClamp(double v, double step, int n)
{
    if (step <= 0 || n <= 0) return 0.0;
    double maxv = (n - 1) * step;
    double t = std::round(v / step) * step;
    if (t < 0.0) t = 0.0;
    if (t > maxv) t = maxv;
    return t;
}

void SceneController::snapToGrid(double &x, double &y, double &z) const
{
    if (!hasGrid()) return;
    x = roundToStepClamp(x, m_dx, m_nx);
    y = roundToStepClamp(y, m_dy, m_ny);
    z = roundToStepClamp(z, m_dz, m_nz);
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

void SceneController::nodePosition(int index, double &x, double &y, double &z) const
{
    if (index < 0 || index >= nodeCount()) {
        x = y = z = 0.0;
        return;
    }
    const auto &node = m_nodes[static_cast<std::size_t>(index)];
    x = node.position[0];
    y = node.position[1];
    z = node.position[2];
}

int SceneController::nodeIndexByExternalId(int externalId) const
{
    for (std::size_t i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes[i].externalId == externalId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::vector<SceneController::NodeInfo> SceneController::nodeInfos() const
{
    std::vector<NodeInfo> result;
    result.reserve(m_nodes.size());
    for (const auto &node : m_nodes) {
        result.push_back(NodeInfo{ node.externalId, node.position[0], node.position[1], node.position[2] });
    }
    return result;
}

int SceneController::pickNode(int displayX, int displayY) const
{
    if (!m_nodePicker || !m_renderer) {
        return -1;
    }
    if (m_nodePicker->Pick(displayX, displayY, 0.0, m_renderer)) {
        vtkIdType pid = m_nodePicker->GetPointId();
        if (pid >= 0 && pid < static_cast<vtkIdType>(m_nodes.size())) {
            return static_cast<int>(pid);
        }
    }
    return -1;
}

void SceneController::setHighlightedNode(int nodeIndex)
{
    if (!m_pointColors) {
        return;
    }

    if (nodeIndex == m_highlightNode) {
        return;
    }

    const vtkIdType tupleCount = m_pointColors->GetNumberOfTuples();
    auto restoreColor = [&](int idx) {
        if (idx >= 0 && idx < static_cast<int>(tupleCount)) {
            m_pointColors->SetTypedTuple(idx, m_defaultNodeColor);
        }
    };
    auto applyHighlight = [&](int idx) {
        if (idx >= 0 && idx < static_cast<int>(tupleCount)) {
            m_pointColors->SetTypedTuple(idx, m_highlightNodeColor);
        }
    };

    restoreColor(m_highlightNode);
    m_highlightNode = nodeIndex;
    if (m_highlightNode >= 0) {
        applyHighlight(m_highlightNode);
    }

    m_pointColors->Modified();
    m_pointCloud->Modified();
    m_renderWindow->Render();
}

void SceneController::clearHighlightedNode()
{
    setHighlightedNode(-1);
}

int SceneController::addBar(int startNodeIndex, int endNodeIndex, const QUuid &materialId, const QUuid &sectionId)
{
    if (startNodeIndex < 0 || endNodeIndex < 0) {
        return -1;
    }
    if (startNodeIndex >= nodeCount() || endNodeIndex >= nodeCount()) {
        return -1;
    }
    if (startNodeIndex == endNodeIndex) {
        return -1;
    }

    vtkNew<vtkLine> line;
    line->GetPointIds()->SetId(0, m_nodes[static_cast<std::size_t>(startNodeIndex)].pointId);
    line->GetPointIds()->SetId(1, m_nodes[static_cast<std::size_t>(endNodeIndex)].pointId);
    m_barLines->InsertNextCell(line);
    m_barData->SetLines(m_barLines);
    m_barData->Modified();

    BarInfo info { startNodeIndex, endNodeIndex, materialId, sectionId, 0 };
    m_bars.push_back(info);

    m_renderWindow->Render();
    return static_cast<int>(m_bars.size()) - 1;
}

void SceneController::assignBarProperties(const std::vector<int> &barIndices, const QUuid &materialId, const QUuid &sectionId)
{
    for (int idx : barIndices) {
        if (idx >= 0 && idx < static_cast<int>(m_bars.size())) {
            m_bars[static_cast<std::size_t>(idx)].materialId = materialId;
            m_bars[static_cast<std::size_t>(idx)].sectionId = sectionId;
        }
    }
}

const std::vector<SceneController::BarInfo> &SceneController::bars() const
{
    return m_bars;
}

void SceneController::setBarExternalId(int barIndex, int externalId)
{
    if (barIndex < 0 || barIndex >= static_cast<int>(m_bars.size())) {
        return;
    }
    m_bars[static_cast<std::size_t>(barIndex)].externalId = externalId;
}
void SceneController::clearAll()
{
    m_points->Reset();
    m_vertices->Reset();
    m_pointColors->Reset();
    m_pointColors->SetNumberOfComponents(3);
    m_pointColors->SetName("NodeColors");
    m_barLines->Reset();
    m_barData->SetLines(m_barLines);

    m_nodes.clear();
    m_bars.clear();
    m_highlightNode = -1;
    m_nextNodeExternalId = 1;

    m_points->Modified();
    m_vertices->Modified();
    m_pointColors->Modified();
    m_barData->Modified();
    m_pointCloud->Modified();

    if (m_renderer) {
        m_renderer->ResetCamera();
    }
    m_renderWindow->Render();
}
