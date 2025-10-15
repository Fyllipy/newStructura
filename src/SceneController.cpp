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
#include <vtkCellData.h>
#include <QVector3D>
#include <QtGlobal>
#include <optional>
#include <limits>
#include <algorithm>
#include "CustomInteractorStyle.h"

namespace {
constexpr double kCoordEpsilon = 1e-6;
}

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
    , m_barColors(vtkSmartPointer<vtkUnsignedCharArray>::New())
    , m_gridData(vtkSmartPointer<vtkPolyData>::New())
    , m_gridMapper(vtkSmartPointer<vtkPolyDataMapper>::New())
    , m_gridActor(vtkSmartPointer<vtkActor>::New())
    , m_gridPoints(vtkSmartPointer<vtkPoints>::New())
    , m_gridCells(vtkSmartPointer<vtkCellArray>::New())
    , m_gridColors(vtkSmartPointer<vtkUnsignedCharArray>::New())
    , m_gridGhostData(vtkSmartPointer<vtkPolyData>::New())
    , m_gridGhostPoints(vtkSmartPointer<vtkPoints>::New())
    , m_gridGhostCells(vtkSmartPointer<vtkCellArray>::New())
    , m_gridGhostMapper(vtkSmartPointer<vtkPolyDataMapper>::New())
    , m_gridGhostActor(vtkSmartPointer<vtkActor>::New())
    , m_picker(vtkSmartPointer<vtkCellPicker>::New())
    , m_nodePicker(vtkSmartPointer<vtkPointPicker>::New())
    , m_barPicker(vtkSmartPointer<vtkCellPicker>::New())
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
    m_barColors->SetNumberOfComponents(3);
    m_barColors->SetName("BarColors");
    m_barData->GetCellData()->SetScalars(m_barColors);
    m_barMapper->SetInputData(m_barData);
    m_barMapper->ScalarVisibilityOn();
    m_barMapper->SetColorModeToDirectScalars();
    m_barMapper->SetScalarModeToUseCellData();
    m_barActor->SetMapper(m_barMapper);
    m_barActor->GetProperty()->SetLineWidth(2.0);
    m_barActor->GetProperty()->LightingOff();
    m_barActor->PickableOn();

    // Grid default visuals
    m_gridData->SetPoints(m_gridPoints);
    m_gridData->SetLines(m_gridCells);
    m_gridColors->SetNumberOfComponents(3);
    m_gridColors->SetName("GridColors");
    m_gridData->GetCellData()->SetScalars(m_gridColors);
    m_gridMapper->SetInputData(m_gridData);
    m_gridMapper->ScalarVisibilityOn();
    m_gridMapper->SetColorModeToDirectScalars();
    m_gridMapper->SetScalarModeToUseCellData();
    m_gridActor->SetMapper(m_gridMapper);
    m_gridActor->GetProperty()->SetColor(0.55, 0.60, 0.68);
    m_gridActor->GetProperty()->SetOpacity(0.55);
    m_gridActor->GetProperty()->SetLineWidth(1.0);
    m_gridActor->PickableOn();
    m_gridActor->SetVisibility(false);

    // Ghost grid line visuals
    m_gridGhostData->SetPoints(m_gridGhostPoints);
    m_gridGhostData->SetLines(m_gridGhostCells);
    m_gridGhostMapper->SetInputData(m_gridGhostData);
    m_gridGhostActor->SetMapper(m_gridGhostMapper);
    m_gridGhostActor->GetProperty()->SetColor(0.98, 0.45, 0.15);
    m_gridGhostActor->GetProperty()->SetOpacity(0.35);
    m_gridGhostActor->GetProperty()->SetLineWidth(2.0);
    m_gridGhostActor->PickableOff();
    m_gridGhostActor->SetVisibility(false);

    // Picker tolerance suitable for thin lines
    m_picker->SetTolerance(0.005);

    m_nodePicker->SetTolerance(0.01);
    m_nodePicker->SetPickFromList(true);
    m_nodePicker->AddPickList(m_pointActor);

    m_barPicker->SetTolerance(0.005);
    m_barPicker->SetPickFromList(true);
    m_barPicker->AddPickList(m_barActor);
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
    m_renderer->AddActor(m_gridGhostActor);
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

    const vtkIdType pointId = m_points->InsertNextPoint(x, y, z);
    m_vertices->InsertNextCell(1);
    m_vertices->InsertCellPoint(pointId);

    QUuid nodeId = QUuid::createUuid();
    Node node(nodeId, externalId, x, y, z);
    m_nodes.push_back(node);
    m_nodePointIds.push_back(pointId);

    if (static_cast<std::size_t>(pointId + 1) > m_pointIdToNodeId.size()) {
        m_pointIdToNodeId.resize(static_cast<std::size_t>(pointId) + 1);
    }
    m_pointIdToNodeId[static_cast<std::size_t>(pointId)] = nodeId;
    m_nodeIndexById.insert(nodeId, static_cast<int>(m_nodes.size()) - 1);

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

    return nodeId;
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

    m_gridLines.clear();
    m_gridLineIndexById.clear();
    m_gridCellToLineIndex.clear();
    m_highlightGridLineId = QUuid();

    hideGridGhostLine();

    m_xCoords.clear();
    m_yCoords.clear();
    m_zCoords.clear();

    if (nx > 0 && dx > 0.0) {
        m_xCoords.reserve(nx);
        for (int i = 0; i < nx; ++i) {
            m_xCoords.append(static_cast<double>(i) * dx);
        }
    }
    if (ny > 0 && dy > 0.0) {
        m_yCoords.reserve(ny);
        for (int j = 0; j < ny; ++j) {
            m_yCoords.append(static_cast<double>(j) * dy);
        }
    }
    if (nz > 0 && dz > 0.0) {
        m_zCoords.reserve(nz);
        for (int k = 0; k < nz; ++k) {
            m_zCoords.append(static_cast<double>(k) * dz);
        }
    }

    rebuildGridFromCoordinates();
    m_gridActor->SetVisibility(!m_gridLines.isEmpty());
    m_renderWindow->Render();
}

bool SceneController::hasGrid() const
{
    return !m_xCoords.isEmpty() && !m_yCoords.isEmpty() && !m_zCoords.isEmpty();
}

void SceneController::snapToGrid(double &x, double &y, double &z) const
{
    if (!hasGrid()) return;
    x = nearestCoordinate(m_xCoords, x);
    y = nearestCoordinate(m_yCoords, y);
    z = nearestCoordinate(m_zCoords, z);
}

void SceneController::gridSpacing(double &dx, double &dy, double &dz) const
{
    dx = computeMinSpacing(m_xCoords);
    dy = computeMinSpacing(m_yCoords);
    dz = computeMinSpacing(m_zCoords);
}

void SceneController::gridCounts(int &nx, int &ny, int &nz) const
{
    nx = m_xCoords.size();
    ny = m_yCoords.size();
    nz = m_zCoords.size();
}

bool SceneController::insertCoordinate(QVector<double> &coords, double value) const
{
    if (!std::isfinite(value)) {
        return false;
    }
    for (double current : coords) {
        if (std::abs(current - value) <= kCoordEpsilon) {
            return false;
        }
    }
    auto it = std::lower_bound(coords.begin(), coords.end(), value);
    coords.insert(it, value);
    return true;
}

bool SceneController::removeCoordinate(QVector<double> &coords, double value)
{
    for (int i = 0; i < coords.size(); ++i) {
        if (std::abs(coords[i] - value) <= kCoordEpsilon) {
            coords.removeAt(i);
            return true;
        }
    }
    return false;
}

double SceneController::nearestCoordinate(const QVector<double> &coords, double value)
{
    if (coords.isEmpty()) {
        return value;
    }
    auto it = std::lower_bound(coords.begin(), coords.end(), value);
    if (it == coords.begin()) {
        return *it;
    }
    if (it == coords.end()) {
        return coords.back();
    }
    double upper = *it;
    double lower = *(it - 1);
    return (std::abs(upper - value) < std::abs(value - lower)) ? upper : lower;
}

double SceneController::computeMinSpacing(const QVector<double> &coords)
{
    if (coords.size() < 2) {
        return 0.0;
    }
    double minSpacing = std::numeric_limits<double>::max();
    for (int i = 1; i < coords.size(); ++i) {
        const double diff = std::abs(coords[i] - coords[i - 1]);
        if (diff < minSpacing) {
            minSpacing = diff;
        }
    }
    return (minSpacing == std::numeric_limits<double>::max()) ? 0.0 : minSpacing;
}

std::pair<double, double> SceneController::minMaxAlongAxis(GridLine::Axis axis) const
{
    const QVector<double> *source = nullptr;
    switch (axis) {
    case GridLine::Axis::X:
        source = &m_xCoords;
        break;
    case GridLine::Axis::Y:
        source = &m_yCoords;
        break;
    case GridLine::Axis::Z:
        source = &m_zCoords;
        break;
    }
    if (!source || source->isEmpty()) {
        return {0.0, 0.0};
    }
    return {source->front(), source->back()};
}

QString SceneController::gridLineKey(GridLine::Axis axis, double coord1, double coord2) const
{
    return QStringLiteral("%1|%2|%3")
        .arg(static_cast<int>(axis))
        .arg(QString::number(coord1, 'f', 6))
        .arg(QString::number(coord2, 'f', 6));
}

SceneController::GridLine *SceneController::findGridLine(const QUuid &id)
{
    const int idx = gridLineIndex(id);
    if (idx < 0 || idx >= m_gridLines.size()) {
        return nullptr;
    }
    return &m_gridLines[idx];
}

const SceneController::GridLine *SceneController::findGridLine(const QUuid &id) const
{
    return const_cast<SceneController *>(this)->findGridLine(id);
}

SceneController::LineEndpoints SceneController::makeLineEndpoints(GridLine::Axis axis,
                                                                  double coordinate1,
                                                                  double coordinate2) const
{
    LineEndpoints endpoints;
    const auto [minX, maxX] = minMaxAlongAxis(GridLine::Axis::X);
    const auto [minY, maxY] = minMaxAlongAxis(GridLine::Axis::Y);
    const auto [minZ, maxZ] = minMaxAlongAxis(GridLine::Axis::Z);

    switch (axis) {
    case GridLine::Axis::X:
        endpoints.start = { {minX, coordinate1, coordinate2} };
        endpoints.end = { {maxX, coordinate1, coordinate2} };
        break;
    case GridLine::Axis::Y:
        endpoints.start = { {coordinate1, minY, coordinate2} };
        endpoints.end = { {coordinate1, maxY, coordinate2} };
        break;
    case GridLine::Axis::Z:
        endpoints.start = { {coordinate1, coordinate2, minZ} };
        endpoints.end = { {coordinate1, coordinate2, maxZ} };
        break;
    }
    return endpoints;
}

void SceneController::rebuildGridFromCoordinates()
{
    if (!m_gridPoints || !m_gridCells || !m_gridColors) {
        return;
    }

    m_gridPoints->Reset();
    m_gridCells->Reset();
    m_gridColors->Reset();
    m_gridColors->SetNumberOfComponents(3);
    m_gridColors->SetName("GridColors");
    QHash<QString, GridLine> previous;
    previous.reserve(m_gridLines.size());
    for (const auto &line : m_gridLines) {
        previous.insert(gridLineKey(line.axis(), line.coordinate1(), line.coordinate2()), line);
    }

    m_gridLines.clear();
    m_gridLineIndexById.clear();
    m_gridCellToLineIndex.clear();

    auto appendLine = [&](GridLine::Axis axis, double coord1, double coord2) {
        const QString key = gridLineKey(axis, coord1, coord2);
        GridLine line;
        if (previous.contains(key)) {
            line = previous.value(key);
        } else {
            line = GridLine(QUuid::createUuid(), axis, coord1, static_cast<int>(m_gridLines.size()), coord1, coord2);
        }
        line.setAxis(axis);
        line.setOffset(coord1);
        line.setCoordinate1(coord1);
        line.setCoordinate2(coord2);

        const auto endpoints = makeLineEndpoints(axis, coord1, coord2);
        line.setEndpoints(endpoints.start[0], endpoints.start[1], endpoints.start[2],
                          endpoints.end[0], endpoints.end[1], endpoints.end[2]);
        line.setIndex(static_cast<int>(m_gridLines.size()));

        const auto &start = line.startPoint();
        const auto &end = line.endPoint();
        vtkIdType id0 = m_gridPoints->InsertNextPoint(start.data());
        vtkIdType id1 = m_gridPoints->InsertNextPoint(end.data());
        vtkIdType lineIds[2] = { id0, id1 };
        vtkIdType cellId = m_gridCells->InsertNextCell(2, lineIds);

        m_gridCellToLineIndex.insert(cellId, m_gridLines.size());
        m_gridLineIndexById.insert(line.id(), m_gridLines.size());
        const unsigned char *color = line.isHighlighted() ? m_highlightGridColor : m_defaultGridColor;
        m_gridColors->InsertNextTypedTuple(color);
        m_gridLines.append(line);
    };

    for (double y : m_yCoords) {
        for (double z : m_zCoords) {
            appendLine(GridLine::Axis::X, y, z);
        }
    }
    for (double x : m_xCoords) {
        for (double z : m_zCoords) {
            appendLine(GridLine::Axis::Y, x, z);
        }
    }
    for (double x : m_xCoords) {
        for (double y : m_yCoords) {
            appendLine(GridLine::Axis::Z, x, y);
        }
    }

    if (!m_highlightGridLineId.isNull()) {
        if (gridLineIndex(m_highlightGridLineId) < 0) {
            m_highlightGridLineId = QUuid();
        }
    }

    m_nx = m_xCoords.size();
    m_ny = m_yCoords.size();
    m_nz = m_zCoords.size();

    m_gridPoints->Modified();
    m_gridCells->Modified();
    m_gridColors->Modified();
    m_gridData->GetCellData()->SetScalars(m_gridColors);
    m_gridData->Modified();
}

void SceneController::updateGridColors()
{
    if (!m_gridColors) {
        return;
    }
    const vtkIdType tupleCount = m_gridColors->GetNumberOfTuples();
    if (tupleCount != static_cast<vtkIdType>(m_gridLines.size())) {
        rebuildGridFromCoordinates();
        return;
    }
    for (int i = 0; i < m_gridLines.size(); ++i) {
        const GridLine &line = m_gridLines[i];
        const unsigned char *color = line.isHighlighted() ? m_highlightGridColor : m_defaultGridColor;
        m_gridColors->SetTypedTuple(i, color);
    }
    m_gridColors->Modified();
    m_gridData->Modified();
}

QUuid SceneController::addGridLine(GridLine::Axis axis, double coordinate1, double coordinate2)
{
    if (!hasGrid()) {
        return QUuid();
    }

    switch (axis) {
    case GridLine::Axis::X:
        insertCoordinate(m_yCoords, coordinate1);
        insertCoordinate(m_zCoords, coordinate2);
        break;
    case GridLine::Axis::Y:
        insertCoordinate(m_xCoords, coordinate1);
        insertCoordinate(m_zCoords, coordinate2);
        break;
    case GridLine::Axis::Z:
        insertCoordinate(m_xCoords, coordinate1);
        insertCoordinate(m_yCoords, coordinate2);
        break;
    }

    rebuildGridFromCoordinates();
    m_gridActor->SetVisibility(!m_gridLines.isEmpty());
    m_renderWindow->Render();

    double canonicalCoord1 = coordinate1;
    double canonicalCoord2 = coordinate2;
    switch (axis) {
    case GridLine::Axis::X:
        canonicalCoord1 = nearestCoordinate(m_yCoords, coordinate1);
        canonicalCoord2 = nearestCoordinate(m_zCoords, coordinate2);
        break;
    case GridLine::Axis::Y:
        canonicalCoord1 = nearestCoordinate(m_xCoords, coordinate1);
        canonicalCoord2 = nearestCoordinate(m_zCoords, coordinate2);
        break;
    case GridLine::Axis::Z:
        canonicalCoord1 = nearestCoordinate(m_xCoords, coordinate1);
        canonicalCoord2 = nearestCoordinate(m_yCoords, coordinate2);
        break;
    }

    for (const auto &line : m_gridLines) {
        if (line.axis() != axis) {
            continue;
        }
        if (std::abs(line.coordinate1() - canonicalCoord1) > kCoordEpsilon) {
            continue;
        }
        if (std::abs(line.coordinate2() - canonicalCoord2) > kCoordEpsilon) {
            continue;
        }
        return line.id();
    }
    return QUuid();
}

bool SceneController::removeGridLine(const QUuid &lineId)
{
    const int idx = gridLineIndex(lineId);
    if (idx < 0 || idx >= m_gridLines.size()) {
        return false;
    }
    const GridLine line = m_gridLines[idx];

    bool removed = false;
    switch (line.axis()) {
    case GridLine::Axis::X:
        removed = removeCoordinate(m_yCoords, line.coordinate1());
        if (!removed) {
            removed = removeCoordinate(m_zCoords, line.coordinate2());
        }
        break;
    case GridLine::Axis::Y:
        removed = removeCoordinate(m_xCoords, line.coordinate1());
        if (!removed) {
            removed = removeCoordinate(m_zCoords, line.coordinate2());
        }
        break;
    case GridLine::Axis::Z:
        removed = removeCoordinate(m_xCoords, line.coordinate1());
        if (!removed) {
            removed = removeCoordinate(m_yCoords, line.coordinate2());
        }
        break;
    }

    if (!removed) {
        return false;
    }

    m_gridLineIndexById.remove(lineId);
    if (m_highlightGridLineId == lineId) {
        m_highlightGridLineId = QUuid();
    }

    rebuildGridFromCoordinates();
    m_gridActor->SetVisibility(!m_gridLines.isEmpty());
    m_renderWindow->Render();
    return true;
}

QUuid SceneController::pickGridLine(int displayX, int displayY) const
{
    if (!m_renderer || !m_picker || m_gridLines.isEmpty()) {
        return QUuid();
    }

    if (!m_picker->Pick(displayX, displayY, 0, m_renderer)) {
        return QUuid();
    }

    vtkIdType cellId = m_picker->GetCellId();
    if (cellId < 0) {
        return QUuid();
    }

    if (m_picker->GetActor() != m_gridActor) {
        return QUuid();
    }

    const auto it = m_gridCellToLineIndex.constFind(cellId);
    if (it == m_gridCellToLineIndex.constEnd()) {
        return QUuid();
    }
    const int idx = it.value();
    if (idx < 0 || idx >= m_gridLines.size()) {
        return QUuid();
    }
    return m_gridLines[idx].id();
}

void SceneController::setHighlightedGridLine(const QUuid &lineId)
{
    if (lineId == m_highlightGridLineId) {
        return;
    }

    if (!m_highlightGridLineId.isNull()) {
        if (auto *line = findGridLine(m_highlightGridLineId)) {
            line->setHighlighted(false);
        }
    }

    m_highlightGridLineId = lineId;
    if (!lineId.isNull()) {
        if (auto *line = findGridLine(lineId)) {
            line->setHighlighted(true);
        }
    }

    updateGridColors();
    m_renderWindow->Render();
}

void SceneController::clearHighlightedGridLine()
{
    setHighlightedGridLine(QUuid());
}

void SceneController::showGridGhostLine(GridLine::Axis axis, double coordinate1, double coordinate2)
{
    if (!m_gridGhostPoints || !m_gridGhostCells) {
        return;
    }

    m_ghostAxis = axis;
    LineEndpoints endpoints = makeLineEndpoints(axis, coordinate1, coordinate2);

    m_gridGhostPoints->Reset();
    m_gridGhostCells->Reset();

    vtkIdType id0 = m_gridGhostPoints->InsertNextPoint(endpoints.start.data());
    vtkIdType id1 = m_gridGhostPoints->InsertNextPoint(endpoints.end.data());
    vtkIdType lineIds[2] = { id0, id1 };
    m_gridGhostCells->InsertNextCell(2, lineIds);

    m_gridGhostPoints->Modified();
    m_gridGhostCells->Modified();
    m_gridGhostData->Modified();

    if (!m_gridGhostActor->GetVisibility()) {
        m_gridGhostActor->SetVisibility(true);
    }
    m_renderWindow->Render();
}

void SceneController::hideGridGhostLine()
{
    if (!m_gridGhostActor) {
        return;
    }
    if (m_gridGhostPoints) {
        m_gridGhostPoints->Reset();
        m_gridGhostPoints->Modified();
    }
    if (m_gridGhostCells) {
        m_gridGhostCells->Reset();
        m_gridGhostCells->Modified();
    }
    if (m_gridGhostData) {
        m_gridGhostData->Modified();
    }
    if (m_gridGhostActor->GetVisibility()) {
        m_gridGhostActor->SetVisibility(false);
        m_renderWindow->Render();
    }
}

std::optional<QUuid> SceneController::nearestGridLineId(GridLine::Axis axis,
                                                        double coordinate1,
                                                        double coordinate2) const
{
    if (m_gridLines.isEmpty()) {
        return std::nullopt;
    }

    double bestDistance = std::numeric_limits<double>::max();
    QUuid bestId;

    for (const auto &line : m_gridLines) {
        if (line.axis() != axis) {
            continue;
        }
        const double d1 = line.coordinate1() - coordinate1;
        const double d2 = line.coordinate2() - coordinate2;
        const double distance = std::hypot(d1, d2);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestId = line.id();
        }
    }

    if (bestId.isNull()) {
        return std::nullopt;
    }
    return bestId;
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
        if (pid >= 0 && pid < static_cast<vtkIdType>(m_pointIdToNodeId.size())) {
            return m_pointIdToNodeId[static_cast<std::size_t>(pid)];
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
    if (!m_pointColors) {
        return;
    }

    if (nodeId == m_highlightNodeId) {
        return;
    }

    if (!m_highlightNodeId.isNull()) {
        if (m_selectedNodeIds.contains(m_highlightNodeId)) {
            applyNodeColor(m_highlightNodeId, m_selectedNodeColor);
        } else {
            applyNodeColor(m_highlightNodeId, m_defaultNodeColor);
        }
    }
    m_highlightNodeId = nodeId;
    if (!m_highlightNodeId.isNull()) {
        applyNodeColor(m_highlightNodeId, m_hoverNodeColor);
    }

    m_pointColors->Modified();
    m_pointCloud->Modified();
    m_renderWindow->Render();
}

void SceneController::clearHighlightedNode()
{
    setHighlightedNode(QUuid());
}

void SceneController::setSelectedNodes(const QSet<QUuid> &nodeIds)
{
    if (!m_pointColors) {
        m_selectedNodeIds = nodeIds;
        return;
    }

    if (m_selectedNodeIds == nodeIds) {
        return;
    }

    for (const QUuid &id : m_selectedNodeIds) {
        if (nodeIds.contains(id)) {
            continue;
        }
        if (id == m_highlightNodeId) {
            continue;
        }
        applyNodeColor(id, m_defaultNodeColor);
    }

    for (const QUuid &id : nodeIds) {
        if (id == m_highlightNodeId) {
            continue;
        }
        if (!m_selectedNodeIds.contains(id)) {
            applyNodeColor(id, m_selectedNodeColor);
        }
    }

    m_selectedNodeIds = nodeIds;
    m_pointColors->Modified();
    m_pointCloud->Modified();
    m_renderWindow->Render();
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
        if (idx < 0 || static_cast<std::size_t>(idx) >= m_nodePointIds.size()) {
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

        const vtkIdType pointId = m_nodePointIds[static_cast<std::size_t>(idx)];
        m_points->SetPoint(pointId, pos.x(), pos.y(), pos.z());
        node.setPosition(pos.x(), pos.y(), pos.z());
        changed = true;
    }

    if (changed) {
        m_points->Modified();
        m_pointCloud->Modified();
        m_barData->Modified();
        updateBounds();
        m_renderWindow->Render();
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
    if (static_cast<std::size_t>(startIndex) >= m_nodePointIds.size()
        || static_cast<std::size_t>(endIndex) >= m_nodePointIds.size()) {
        return {};
    }

    vtkIdType ids[2] = {
        m_nodePointIds[static_cast<std::size_t>(startIndex)],
        m_nodePointIds[static_cast<std::size_t>(endIndex)]
    };
    m_barLines->InsertNextCell(2, ids);
    m_barLines->Modified();
    m_barData->SetLines(m_barLines);
    m_barData->Modified();

    const QUuid barId = QUuid::createUuid();
    Bar bar(barId, startNodeId, endNodeId, materialId, sectionId);
    m_bars.push_back(bar);
    m_barIndexById.insert(barId, static_cast<int>(m_bars.size()) - 1);
    if (m_barColors) {
        m_barColors->InsertNextTypedTuple(m_defaultBarColor);
        m_barColors->Modified();
        m_barData->GetCellData()->SetScalars(m_barColors);
    }

    m_renderWindow->Render();
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
        m_renderWindow->Render();
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
    if (!m_barColors) {
        m_selectedBarIds = barIds;
        return;
    }

    if (m_selectedBarIds == barIds) {
        return;
    }

    for (const QUuid &id : m_selectedBarIds) {
        if (barIds.contains(id)) {
            continue;
        }
        const int idx = barIndex(id);
        if (idx < 0) {
            continue;
        }
        applyBarColor(idx, m_defaultBarColor);
    }

    for (const QUuid &id : barIds) {
        if (m_selectedBarIds.contains(id)) {
            continue;
        }
        const int idx = barIndex(id);
        if (idx < 0) {
            continue;
        }
        applyBarColor(idx, m_selectedBarColor);
    }

    m_selectedBarIds = barIds;
    m_barColors->Modified();
    m_barData->Modified();
    m_renderWindow->Render();
}

void SceneController::setBarExternalId(const QUuid &barId, int externalId)
{
    Bar *bar = findBar(barId);
    if (!bar) {
        return;
    }
    bar->setExternalId(externalId);
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
    m_nodePointIds.clear();
    m_pointIdToNodeId.clear();
    m_nodeIndexById.clear();
    m_bars.clear();
    m_barIndexById.clear();
    m_highlightNodeId = QUuid();
    m_selectedNodeIds.clear();
    m_selectedBarIds.clear();
    if (m_barColors) {
        m_barColors->Reset();
        m_barColors->SetNumberOfComponents(3);
        m_barColors->SetName("BarColors");
        m_barData->GetCellData()->SetScalars(m_barColors);
    }
    m_nextNodeExternalId = 1;

    m_gridLines.clear();
    m_gridLineIndexById.clear();
    m_gridCellToLineIndex.clear();
    m_highlightGridLineId = QUuid();
    m_dx = 0.0; m_dy = 0.0; m_dz = 0.0;
    m_nx = 0; m_ny = 0; m_nz = 0;
    m_xCoords.clear();
    m_yCoords.clear();
    m_zCoords.clear();
    if (m_gridPoints) {
        m_gridPoints->Reset();
        m_gridPoints->Modified();
    }
    if (m_gridCells) {
        m_gridCells->Reset();
        m_gridCells->Modified();
    }
    if (m_gridColors) {
        m_gridColors->Reset();
        m_gridColors->SetNumberOfComponents(3);
        m_gridColors->SetName("GridColors");
        m_gridColors->Modified();
    }
    m_gridData->Modified();
    m_gridActor->SetVisibility(false);
    hideGridGhostLine();

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

void SceneController::applyNodeColor(const QUuid &id, const unsigned char color[3])
{
    if (!m_pointColors) {
        return;
    }
    if (id.isNull()) {
        return;
    }
    const int idx = nodeIndex(id);
    if (idx < 0) {
        return;
    }
    if (static_cast<std::size_t>(idx) >= m_nodePointIds.size()) {
        return;
    }
    const vtkIdType pointId = m_nodePointIds[static_cast<std::size_t>(idx)];
    if (pointId < 0) {
        return;
    }
    const vtkIdType tupleCount = m_pointColors->GetNumberOfTuples();
    if (pointId >= tupleCount) {
        return;
    }
    m_pointColors->SetTypedTuple(pointId, color);
}

void SceneController::applyBarColor(int barIndex, const unsigned char color[3])
{
    if (!m_barColors) {
        return;
    }
    if (barIndex < 0) {
        return;
    }
    const vtkIdType tupleCount = m_barColors->GetNumberOfTuples();
    if (barIndex >= tupleCount) {
        return;
    }
    m_barColors->SetTypedTuple(barIndex, color);
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

int SceneController::gridLineIndex(const QUuid &id) const
{
    if (id.isNull()) {
        return -1;
    }
    const auto it = m_gridLineIndexById.constFind(id);
    if (it == m_gridLineIndexById.constEnd()) {
        return -1;
    }
    return it.value();
}
