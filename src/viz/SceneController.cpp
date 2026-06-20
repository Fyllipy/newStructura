#include "viz/SceneController.h"
#include "viz/LoadVisualization.h"
#include "viz/LocalCoordinateSystem.h"

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

bool SceneController::nearlyEqual(double lhs, double rhs, double epsilon)
{
    return std::abs(lhs - rhs) <= epsilon;
}

void SceneController::populateAxisCoordinates(QVector<double> &coords, double step, int count)
{
    coords.clear();
    if (count <= 0 || step <= 0.0) {
        return;
    }

    coords.reserve(count);
    for (int i = 0; i < count; ++i) {
        coords.append(static_cast<double>(i) * step);
    }
}


void SceneController::initializeGridRendering()
{
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

    if (auto *property = m_gridActor->GetProperty()) {
        property->SetColor(0.55, 0.60, 0.68);
        property->SetOpacity(0.55);
        property->SetLineWidth(1.0);
    }

    m_gridActor->PickableOn();
    m_gridActor->SetVisibility(false);
}

void SceneController::initializeGridGhostRendering()
{
    m_gridGhostData->SetPoints(m_gridGhostPoints);
    m_gridGhostData->SetLines(m_gridGhostCells);
    m_gridGhostMapper->SetInputData(m_gridGhostData);
    m_gridGhostActor->SetMapper(m_gridGhostMapper);

    if (auto *property = m_gridGhostActor->GetProperty()) {
        property->SetColor(0.98, 0.45, 0.15);
        property->SetOpacity(0.35);
        property->SetLineWidth(2.0);
    }

    m_gridGhostActor->PickableOff();
    m_gridGhostActor->SetVisibility(false);
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

void SceneController::clearGrid()
{
    m_gridLines.clear();
    m_gridLineIndexById.clear();
    m_gridCellToLineIndex.clear();
    m_highlightGridLineId = QUuid();

    m_dx = 0.0;
    m_dy = 0.0;
    m_dz = 0.0;
    m_nx = 0;
    m_ny = 0;
    m_nz = 0;

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

    if (m_gridData) {
        m_gridData->GetCellData()->SetScalars(m_gridColors);
        m_gridData->Modified();
    }

    m_gridActor->SetVisibility(false);
    hideGridGhostLine();
}

void SceneController::clearLoads()
{
    m_nodalLoadVisuals.clear();
    m_memberLoadVisuals.clear();

    if (m_nodalLoadActor) {
        m_nodalLoadActor->SetVisibility(false);
    }
    if (m_distributedLoadActor) {
        m_distributedLoadActor->SetVisibility(false);
    }
    if (m_momentActor) {
        m_momentActor->SetVisibility(false);
    }
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
    , m_structureRenderer(std::make_unique<Structura::Visualization::StructureRenderer>())
{
    // Initialize component renderers
    initializeGridRendering();
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
    m_renderer->AddActor(m_gridActor);
    m_renderer->AddActor(m_gridGhostActor);
    m_renderer->AddActor(m_lcsActor);
    
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

void SceneController::updateBounds()
{
}

void SceneController::createGrid(double dx, double dy, double dz, int nx, int ny, int nz)
{
    clearGrid();

    m_dx = dx;
    m_dy = dy;
    m_dz = dz;
    m_nx = nx;
    m_ny = ny;
    m_nz = nz;

    populateAxisCoordinates(m_xCoords, dx, nx);
    populateAxisCoordinates(m_yCoords, dy, ny);
    populateAxisCoordinates(m_zCoords, dz, nz);

    rebuildGridFromCoordinates();
    m_gridActor->SetVisibility(!m_gridLines.isEmpty());
    requestRender();
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
        if (nearlyEqual(current, value)) {
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
        if (nearlyEqual(coords[i], value)) {
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
        vtkIdType id0 = m_gridPoints->InsertNextPoint(start.x(), start.y(), start.z());
        vtkIdType id1 = m_gridPoints->InsertNextPoint(end.x(), end.y(), end.z());
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
    requestRender();

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
        if (!nearlyEqual(line.coordinate1(), canonicalCoord1)) {
            continue;
        }
        if (!nearlyEqual(line.coordinate2(), canonicalCoord2)) {
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
    requestRender();
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
    requestRender();
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
    requestRender();
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
        requestRender();
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

void SceneController::setNodalLoadVisuals(const QVector<NodalLoadVisual> &visuals)
{
    m_nodalLoadVisuals = visuals;
    updateLoadVisuals();
    requestRender();
}

void SceneController::setMemberLoadVisuals(const QVector<MemberLoadVisual> &visuals)
{
    m_memberLoadVisuals = visuals;
    updateLoadVisuals();
    requestRender();
}

void SceneController::updateLoadVisuals()
{
    using namespace Structura::Visualization;

    // Inicialização sob demanda dos atores
    if (!m_nodalLoadMapper && m_renderer) {
        // Inicializar apenas os atores (os PolyDatas são gerados pelo factory)
        m_nodalLoadMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        m_nodalLoadActor = vtkSmartPointer<vtkActor>::New();
        m_nodalLoadActor->SetMapper(m_nodalLoadMapper);
        m_nodalLoadActor->GetProperty()->SetColor(0.90, 0.15, 0.20);
        m_nodalLoadActor->GetProperty()->LightingOff();
        m_renderer->AddActor(m_nodalLoadActor);

        m_distributedLoadMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        m_distributedLoadActor = vtkSmartPointer<vtkActor>::New();
        m_distributedLoadActor->SetMapper(m_distributedLoadMapper);
        m_distributedLoadActor->GetProperty()->SetColor(0.15, 0.58, 0.32);
        m_distributedLoadActor->GetProperty()->LightingOff();
        m_renderer->AddActor(m_distributedLoadActor);

        m_momentMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        m_momentActor = vtkSmartPointer<vtkActor>::New();
        m_momentActor->SetMapper(m_momentMapper);
        m_momentActor->GetProperty()->SetColor(0.65, 0.20, 0.82);
        m_momentActor->GetProperty()->SetLineWidth(2.5);
        m_momentActor->GetProperty()->LightingOff();
        m_renderer->AddActor(m_momentActor);
    }

    // Preparar dados para as forças nodais e momentos
    std::vector<LoadGlyphFactory::NodalLoadData> nodalLoads;
    std::vector<LoadGlyphFactory::NodalMomentData> nodalMoments;
    
    for (const auto& visual : m_nodalLoadVisuals) {
        if (visual.force.lengthSquared() > 1e-6f) {
            LoadGlyphFactory::NodalLoadData nld;
            nld.position = {visual.position.x(), visual.position.y(), visual.position.z()};
            QVector3D dir = visual.force.normalized();
            nld.direction = {dir.x(), dir.y(), dir.z()};
            nld.magnitude = visual.force.length();
            nld.arrowSize = 0.5;
            nodalLoads.push_back(nld);
        }
        if (visual.moment.lengthSquared() > 1e-6f) {
            LoadGlyphFactory::NodalMomentData nmd;
            nmd.position = {visual.position.x(), visual.position.y(), visual.position.z()};
            QVector3D dir = visual.moment.normalized();
            nmd.axis = {dir.x(), dir.y(), dir.z()};
            nmd.magnitude = visual.moment.length();
            nmd.radius = 0.4;
            nodalMoments.push_back(nmd);
        }
    }

    // Preparar dados para cargas distribuídas
    std::vector<LoadGlyphFactory::DistributedLoadData> distLoads;
    for (const auto& visual : m_memberLoadVisuals) {
        const QVector3D barHalfVector = visual.barVector * 0.5f;
        QVector3D startP = visual.position - barHalfVector;
        QVector3D endP = visual.position + barHalfVector;
        
        LoadGlyphFactory::DistributedLoadData dld;
        dld.node1 = {startP.x(), startP.y(), startP.z()};
        dld.node2 = {endP.x(), endP.y(), endP.z()};
        QVector3D dir = visual.force.normalized();
        dld.loadDir = {dir.x(), dir.y(), dir.z()};
        dld.magnitude = visual.force.length();
        dld.arrowSize = 0.3;
        distLoads.push_back(dld);
    }

    if (m_nodalLoadMapper) {
        m_nodalLoadMapper->SetInputData(LoadGlyphFactory::buildNodalLoads(nodalLoads));
        m_nodalLoadActor->SetVisibility(!nodalLoads.empty());
    }
    if (m_distributedLoadMapper) {
        m_distributedLoadMapper->SetInputData(LoadGlyphFactory::buildDistributedLoads(distLoads));
        m_distributedLoadActor->SetVisibility(!distLoads.empty());
    }
    if (m_momentMapper) {
        m_momentMapper->SetInputData(LoadGlyphFactory::buildNodalMoments(nodalMoments));
        m_momentActor->SetVisibility(!nodalMoments.empty());
    }
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
}

void SceneController::clearAll()
{
    clearNodes();
    clearBars();
    clearGrid();
    clearLoads();
    clearSupports();

    rebuildBarLCSVisuals();

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
        m_supportActor->GetProperty()->SetColor(0.15, 0.58, 0.32); // Greenish
        m_supportActor->GetProperty()->LightingOff(); // Draw technical 2D flat
        m_supportActor->GetProperty()->SetLineWidth(2.0);
        
        m_renderer->AddActor(m_supportActor);
    }

    std::vector<SupportGlyphFactory::SupportData> supports;
    supports.reserve(m_supportVisuals.size());

    for (const auto &visual : m_supportVisuals) {
        SupportGlyphFactory::SupportData sd;
        sd.position = {visual.position.x(), visual.position.y(), visual.position.z()};
        for (int i=0; i<6; ++i) sd.restraints[i] = visual.restraints[i];
        sd.glyphSize = 0.5;
        supports.push_back(sd);
    }

    if (m_supportMapper) {
        auto polyData = SupportGlyphFactory::build(supports);
        m_supportMapper->SetInputData(polyData);
        m_supportActor->SetVisibility(!supports.empty());
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
            auto lcs = lcsProvider.computeLCS(pointA, pointB, kPointArray);
            
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
