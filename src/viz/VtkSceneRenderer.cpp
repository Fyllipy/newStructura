#include "VtkSceneRenderer.h"
#include "../LoadVisualization.h"
#include "../CustomInteractorStyle.h"

#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkAxesActor.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkCellArray.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkUnsignedCharArray.h>
#include <vtkCellPicker.h>
#include <vtkPointPicker.h>
#include <vtkCamera.h>
#include <vtkRenderWindowInteractor.h>

namespace Structura::Viz {

VtkSceneRenderer::VtkSceneRenderer(QObject *parent)
    : QObject(parent)
{
}

VtkSceneRenderer::~VtkSceneRenderer() = default;

void VtkSceneRenderer::initialize(QVTKOpenGLNativeWidget *widget)
{
    if (m_initialized) {
        return;
    }
    
    widget->setRenderWindow(m_renderWindow);
    m_renderWindow->AddRenderer(m_renderer);
    
    // Background similar to SolidWorks: soft light blue gradient
    m_renderer->GradientBackgroundOn();
    m_renderer->SetBackground(0.91, 0.94, 0.98);  // top
    m_renderer->SetBackground2(0.78, 0.85, 0.93); // bottom
    
    // Initialize VTK pipeline
    initializeVtkPipeline();
    
    // Reset camera to see the scene
    m_renderer->ResetCamera();
    
    // Setup orientation marker
    vtkNew<vtkAxesActor> axes;
    axes->AxisLabelsOn();
    axes->SetConeRadius(0.55);
    axes->SetShaftTypeToCylinder();
    axes->SetCylinderRadius(0.03);
    axes->SetTotalLength(1.0, 1.0, 1.0);
    
    m_orientationMarker = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
    m_orientationMarker->SetOrientationMarker(axes);
    
    if (auto *interactor = widget->interactor()) {
        m_orientationMarker->SetInteractor(interactor);
        m_orientationMarker->SetViewport(0.80, 0.80, 0.98, 0.98);
        m_orientationMarker->SetOutlineColor(0.9, 0.9, 0.9);
        m_orientationMarker->EnabledOn();
        m_orientationMarker->InteractiveOff();
        
        // Custom interactor: rotate with right button
        vtkSmartPointer<CustomInteractorStyle> style = vtkSmartPointer<CustomInteractorStyle>::New();
        interactor->SetInteractorStyle(style);
    }
    
    // Initial render
    m_renderWindow->Render();
    
    m_initialized = true;
}

void VtkSceneRenderer::initializeVtkPipeline()
{
    setupNodeRendering();
    setupBarRendering();
    setupGridRendering();
    setupLoadRendering();
    setupSupportRendering();
    setupBarLCSRendering();
}

void VtkSceneRenderer::setupNodeRendering()
{
    m_points = vtkSmartPointer<vtkPoints>::New();
    m_pointCloud = vtkSmartPointer<vtkPolyData>::New();
    m_vertices = vtkSmartPointer<vtkCellArray>::New();
    m_pointColors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    
    m_pointColors->SetNumberOfComponents(3);
    m_pointColors->SetName("Colors");
    
    m_pointCloud->SetPoints(m_points);
    m_pointCloud->SetVerts(m_vertices);
    m_pointCloud->GetPointData()->SetScalars(m_pointColors);
    
    m_pointMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_pointMapper->SetInputData(m_pointCloud);
    
    m_pointActor = vtkSmartPointer<vtkActor>::New();
    m_pointActor->SetMapper(m_pointMapper);
    m_pointActor->GetProperty()->SetPointSize(8.0);
    
    m_renderer->AddActor(m_pointActor);
    
    m_nodePicker = vtkSmartPointer<vtkPointPicker>::New();
    m_nodePicker->SetTolerance(0.01);
}

void VtkSceneRenderer::setupBarRendering()
{
    m_barData = vtkSmartPointer<vtkPolyData>::New();
    m_barLines = vtkSmartPointer<vtkCellArray>::New();
    m_barColors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    
    m_barColors->SetNumberOfComponents(3);
    m_barColors->SetName("Colors");
    
    vtkNew<vtkPoints> barPoints;
    m_barData->SetPoints(barPoints);
    m_barData->SetLines(m_barLines);
    m_barData->GetCellData()->SetScalars(m_barColors);
    
    m_barMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_barMapper->SetInputData(m_barData);
    
    m_barActor = vtkSmartPointer<vtkActor>::New();
    m_barActor->SetMapper(m_barMapper);
    m_barActor->GetProperty()->SetLineWidth(3.0);
    
    m_renderer->AddActor(m_barActor);
    
    m_barPicker = vtkSmartPointer<vtkCellPicker>::New();
    m_barPicker->SetTolerance(0.005);
}

void VtkSceneRenderer::setupGridRendering()
{
    m_gridPoints = vtkSmartPointer<vtkPoints>::New();
    m_gridData = vtkSmartPointer<vtkPolyData>::New();
    m_gridCells = vtkSmartPointer<vtkCellArray>::New();
    m_gridColors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    
    m_gridColors->SetNumberOfComponents(3);
    m_gridColors->SetName("Colors");
    
    m_gridData->SetPoints(m_gridPoints);
    m_gridData->SetLines(m_gridCells);
    m_gridData->GetCellData()->SetScalars(m_gridColors);
    
    m_gridMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_gridMapper->SetInputData(m_gridData);
    
    m_gridActor = vtkSmartPointer<vtkActor>::New();
    m_gridActor->SetMapper(m_gridMapper);
    m_gridActor->GetProperty()->SetLineWidth(1.0);
    m_gridActor->GetProperty()->SetOpacity(0.3);
    
    m_renderer->AddActor(m_gridActor);
    
    // Ghost line setup
    m_gridGhostPoints = vtkSmartPointer<vtkPoints>::New();
    m_gridGhostData = vtkSmartPointer<vtkPolyData>::New();
    m_gridGhostCells = vtkSmartPointer<vtkCellArray>::New();
    
    m_gridGhostData->SetPoints(m_gridGhostPoints);
    m_gridGhostData->SetLines(m_gridGhostCells);
    
    m_gridGhostMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_gridGhostMapper->SetInputData(m_gridGhostData);
    
    m_gridGhostActor = vtkSmartPointer<vtkActor>::New();
    m_gridGhostActor->SetMapper(m_gridGhostMapper);
    m_gridGhostActor->GetProperty()->SetColor(1.0, 0.8, 0.0);
    m_gridGhostActor->GetProperty()->SetLineWidth(2.0);
    m_gridGhostActor->GetProperty()->SetOpacity(0.7);
    m_gridGhostActor->SetVisibility(false);
    
    m_renderer->AddActor(m_gridGhostActor);
    
    m_picker = vtkSmartPointer<vtkCellPicker>::New();
    m_picker->SetTolerance(0.005);
}

void VtkSceneRenderer::setupLoadRendering()
{
    m_loadVisualization = std::make_unique<Structura::Visualization::LoadVisualization>();
    m_loadVisualization->initialize(m_renderer);
}

void VtkSceneRenderer::setupSupportRendering()
{
    m_supportData = vtkSmartPointer<vtkPolyData>::New();
    m_supportMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_supportMapper->SetInputData(m_supportData);
    
    m_supportActor = vtkSmartPointer<vtkActor>::New();
    m_supportActor->SetMapper(m_supportMapper);
    m_supportActor->GetProperty()->SetColor(0.0, 0.8, 0.0);
    m_supportActor->GetProperty()->SetLineWidth(2.0);
    
    m_renderer->AddActor(m_supportActor);
}

void VtkSceneRenderer::setupBarLCSRendering()
{
    m_lcsPoints = vtkSmartPointer<vtkPoints>::New();
    m_lcsData = vtkSmartPointer<vtkPolyData>::New();
    m_lcsCells = vtkSmartPointer<vtkCellArray>::New();
    m_lcsColors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    
    m_lcsColors->SetNumberOfComponents(3);
    m_lcsColors->SetName("Colors");
    
    m_lcsData->SetPoints(m_lcsPoints);
    m_lcsData->SetLines(m_lcsCells);
    m_lcsData->GetCellData()->SetScalars(m_lcsColors);
    
    m_lcsMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_lcsMapper->SetInputData(m_lcsData);
    
    m_lcsActor = vtkSmartPointer<vtkActor>::New();
    m_lcsActor->SetMapper(m_lcsMapper);
    m_lcsActor->GetProperty()->SetLineWidth(2.0);
    m_lcsActor->SetVisibility(false);
    
    m_renderer->AddActor(m_lcsActor);
}

void VtkSceneRenderer::renderSnapshot(const ModelSnapshot &snapshot)
{
    // Store current state
    m_currentNodes = snapshot.nodes;
    m_currentBars = snapshot.bars;
    m_currentGridLines = snapshot.gridLines;
    
    // Rebuild all geometry
    rebuildNodeGeometry(snapshot.nodes);
    rebuildBarGeometry(snapshot.bars, snapshot.nodes);
    rebuildGridGeometry(snapshot.gridLines);
    
    // Update other visuals
    updateLoads(snapshot.nodalLoads, snapshot.memberLoads);
    updateSupports(snapshot.supports);
    updateBarLCS(snapshot.barLCS, snapshot.showBarLCS);
    
    refresh();
}

void VtkSceneRenderer::rebuildNodeGeometry(const std::vector<ModelSnapshot::NodeData> &nodes)
{
    m_points->Reset();
    m_vertices->Reset();
    m_pointColors->Reset();
    m_nodeIndexById.clear();
    
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto &node = nodes[i];
        
        vtkIdType pid = m_points->InsertNextPoint(node.x, node.y, node.z);
        m_vertices->InsertNextCell(1, &pid);
        
        // Determine color
        const unsigned char *color = m_defaultNodeColor;
        if (node.isHighlighted) {
            color = m_hoverNodeColor;
        } else if (node.isSelected) {
            color = m_selectedNodeColor;
        }
        
        m_pointColors->InsertNextTypedTuple(color);
        m_nodeIndexById[node.id] = static_cast<int>(i);
    }
    
    m_pointCloud->Modified();
}

void VtkSceneRenderer::rebuildBarGeometry(const std::vector<ModelSnapshot::BarData> &bars,
                                         const std::vector<ModelSnapshot::NodeData> &nodes)
{
    auto barPoints = m_barData->GetPoints();
    barPoints->Reset();
    m_barLines->Reset();
    m_barColors->Reset();
    m_barIndexById.clear();
    
    // Build node lookup
    QHash<QUuid, const ModelSnapshot::NodeData*> nodeById;
    for (const auto &node : nodes) {
        nodeById[node.id] = &node;
    }
    
    for (size_t i = 0; i < bars.size(); ++i) {
        const auto &bar = bars[i];
        
        auto startNode = nodeById.value(bar.startNodeId, nullptr);
        auto endNode = nodeById.value(bar.endNodeId, nullptr);
        
        if (!startNode || !endNode) {
            continue; // Skip if nodes not found
        }
        
        vtkIdType p0 = barPoints->InsertNextPoint(startNode->x, startNode->y, startNode->z);
        vtkIdType p1 = barPoints->InsertNextPoint(endNode->x, endNode->y, endNode->z);
        
        vtkIdType lineIds[2] = {p0, p1};
        m_barLines->InsertNextCell(2, lineIds);
        
        // Determine color
        const unsigned char *color = bar.isSelected ? m_selectedBarColor : m_defaultBarColor;
        m_barColors->InsertNextTypedTuple(color);
        
        m_barIndexById[bar.id] = static_cast<int>(i);
    }
    
    m_barData->Modified();
}

void VtkSceneRenderer::rebuildGridGeometry(const std::vector<ModelSnapshot::GridLineData> &gridLines)
{
    m_gridPoints->Reset();
    m_gridCells->Reset();
    m_gridColors->Reset();
    m_gridLineIndexById.clear();
    m_gridCellToLineIndex.clear();
    
    for (size_t i = 0; i < gridLines.size(); ++i) {
        const auto &line = gridLines[i];
        
        vtkIdType id0 = m_gridPoints->InsertNextPoint(line.startPoint.data());
        vtkIdType id1 = m_gridPoints->InsertNextPoint(line.endPoint.data());
        
        vtkIdType lineIds[2] = {id0, id1};
        vtkIdType cellId = m_gridCells->InsertNextCell(2, lineIds);
        
        const unsigned char *color = line.isHighlighted ? m_highlightGridColor : m_defaultGridColor;
        m_gridColors->InsertNextTypedTuple(color);
        
        m_gridLineIndexById[line.id] = static_cast<int>(i);
        m_gridCellToLineIndex[cellId] = static_cast<int>(i);
    }
    
    m_gridData->Modified();
}

void VtkSceneRenderer::updateNodes(const std::vector<ModelSnapshot::NodeData> &nodes)
{
    rebuildNodeGeometry(nodes);
    m_currentNodes = nodes;
    refresh();
}

void VtkSceneRenderer::updateBars(const std::vector<ModelSnapshot::BarData> &bars)
{
    rebuildBarGeometry(bars, m_currentNodes);
    m_currentBars = bars;
    refresh();
}

void VtkSceneRenderer::updateGridLines(const std::vector<ModelSnapshot::GridLineData> &gridLines)
{
    rebuildGridGeometry(gridLines);
    m_currentGridLines = gridLines;
    refresh();
}

void VtkSceneRenderer::highlightNode(const QUuid &nodeId)
{
    // Clear previous highlight
    if (!m_highlightedNodeId.isNull() && m_highlightedNodeId != nodeId) {
        int prevIndex = findNodeIndex(m_highlightedNodeId);
        if (prevIndex >= 0) {
            bool isSelected = m_selectedNodeIds.contains(m_highlightedNodeId);
            const unsigned char *color = isSelected ? m_selectedNodeColor : m_defaultNodeColor;
            applyNodeColor(prevIndex, color);
        }
    }
    
    m_highlightedNodeId = nodeId;
    
    // Apply new highlight
    if (!nodeId.isNull()) {
        int index = findNodeIndex(nodeId);
        if (index >= 0) {
            applyNodeColor(index, m_hoverNodeColor);
        }
    }
    
    refresh();
}

void VtkSceneRenderer::setSelectedNodes(const QSet<QUuid> &nodeIds)
{
    // Clear previous selection colors
    for (const auto &id : m_selectedNodeIds) {
        int index = findNodeIndex(id);
        if (index >= 0 && id != m_highlightedNodeId) {
            applyNodeColor(index, m_defaultNodeColor);
        }
    }
    
    m_selectedNodeIds = nodeIds;
    
    // Apply new selection colors
    for (const auto &id : m_selectedNodeIds) {
        int index = findNodeIndex(id);
        if (index >= 0 && id != m_highlightedNodeId) {
            applyNodeColor(index, m_selectedNodeColor);
        }
    }
    
    refresh();
}

void VtkSceneRenderer::setSelectedBars(const QSet<QUuid> &barIds)
{
    // Clear previous selection colors
    for (const auto &id : m_selectedBarIds) {
        int index = findBarIndex(id);
        if (index >= 0) {
            applyBarColor(index, m_defaultBarColor);
        }
    }
    
    m_selectedBarIds = barIds;
    
    // Apply new selection colors
    for (const auto &id : m_selectedBarIds) {
        int index = findBarIndex(id);
        if (index >= 0) {
            applyBarColor(index, m_selectedBarColor);
        }
    }
    
    refresh();
}

void VtkSceneRenderer::highlightGridLine(const QUuid &lineId)
{
    // Clear previous highlight
    if (!m_highlightedGridLineId.isNull() && m_highlightedGridLineId != lineId) {
        int prevIndex = findGridLineIndex(m_highlightedGridLineId);
        if (prevIndex >= 0) {
            applyGridLineColor(prevIndex, m_defaultGridColor);
        }
    }
    
    m_highlightedGridLineId = lineId;
    
    // Apply new highlight
    if (!lineId.isNull()) {
        int index = findGridLineIndex(lineId);
        if (index >= 0) {
            applyGridLineColor(index, m_highlightGridColor);
        }
    }
    
    refresh();
}

void VtkSceneRenderer::showGridGhostLine(int axis,
                                        const std::array<double, 3> &startPoint,
                                        const std::array<double, 3> &endPoint)
{
    m_gridGhostPoints->Reset();
    m_gridGhostCells->Reset();
    
    vtkIdType id0 = m_gridGhostPoints->InsertNextPoint(startPoint.data());
    vtkIdType id1 = m_gridGhostPoints->InsertNextPoint(endPoint.data());
    
    vtkIdType lineIds[2] = {id0, id1};
    m_gridGhostCells->InsertNextCell(2, lineIds);
    
    m_gridGhostData->Modified();
    m_gridGhostActor->SetVisibility(true);
    
    refresh();
}

void VtkSceneRenderer::hideGridGhostLine()
{
    m_gridGhostActor->SetVisibility(false);
    refresh();
}

void VtkSceneRenderer::updateLoads(const std::vector<ModelSnapshot::NodalLoadData> &nodalLoads,
                                  const std::vector<ModelSnapshot::MemberLoadData> &memberLoads)
{
    if (!m_loadVisualization) {
        return;
    }
    
    // Convert to LoadVisualization format
    QVector<Structura::Visualization::LoadVisualization::NodalLoad> nodalLoadVec;
    for (const auto &load : nodalLoads) {
        Structura::Visualization::LoadVisualization::NodalLoad nl;
        nl.position = QVector3D(load.position[0], load.position[1], load.position[2]);
        nl.force = QVector3D(load.force[0], load.force[1], load.force[2]);
        nl.moment = QVector3D(load.moment[0], load.moment[1], load.moment[2]);
        nodalLoadVec.append(nl);
    }
    
    QVector<Structura::Visualization::LoadVisualization::DistributedLoad> memberLoadVec;
    for (const auto &load : memberLoads) {
        Structura::Visualization::LoadVisualization::DistributedLoad dl;
        dl.startPoint = QVector3D(load.position[0], load.position[1], load.position[2]);
        // For member loads, we need more info (end point). This is simplified for now.
        dl.endPoint = dl.startPoint + QVector3D(load.barVector[0], load.barVector[1], load.barVector[2]);
        dl.loadVector = QVector3D(load.force[0], load.force[1], load.force[2]);
        dl.isLocalSystem = load.localSystem;
        memberLoadVec.append(dl);
    }
    
    m_loadVisualization->setNodalLoads(nodalLoadVec);
    m_loadVisualization->setDistributedLoads(memberLoadVec);
    
    refresh();
}

void VtkSceneRenderer::updateSupports(const std::vector<ModelSnapshot::SupportData> &supports)
{
    // TODO: Implement support visualization
    // This would require porting the support visualization logic from SceneController
    refresh();
}

void VtkSceneRenderer::updateBarLCS(const std::vector<ModelSnapshot::BarLCSData> &barLCS, bool visible)
{
    // TODO: Implement bar LCS visualization
    // This would require porting the LCS visualization logic from SceneController
    m_lcsActor->SetVisibility(visible);
    refresh();
}

void VtkSceneRenderer::clearAll()
{
    m_currentNodes.clear();
    m_currentBars.clear();
    m_currentGridLines.clear();
    
    m_points->Reset();
    m_vertices->Reset();
    m_pointColors->Reset();
    
    auto barPoints = m_barData->GetPoints();
    barPoints->Reset();
    m_barLines->Reset();
    m_barColors->Reset();
    
    m_gridPoints->Reset();
    m_gridCells->Reset();
    m_gridColors->Reset();
    
    m_nodeIndexById.clear();
    m_barIndexById.clear();
    m_gridLineIndexById.clear();
    
    m_highlightedNodeId = QUuid();
    m_highlightedGridLineId = QUuid();
    m_selectedNodeIds.clear();
    m_selectedBarIds.clear();
    
    refresh();
}

void VtkSceneRenderer::resetCamera()
{
    m_renderer->ResetCamera();
    refresh();
}

void VtkSceneRenderer::zoomExtents()
{
    m_renderer->ResetCamera();
    refresh();
}

void VtkSceneRenderer::refresh()
{
    if (m_initialized) {
        m_renderWindow->Render();
    }
}

// Picking operations

QUuid VtkSceneRenderer::pickNode(int displayX, int displayY) const
{
    if (!m_nodePicker || !m_initialized) {
        return QUuid();
    }
    
    // Note: VTK picks work with display coordinates directly from Qt
    // No need to adjust Y coordinate
    m_nodePicker->Pick(displayX, displayY, 0, m_renderer);
    vtkIdType pointId = m_nodePicker->GetPointId();
    
    if (pointId >= 0 && pointId < static_cast<vtkIdType>(m_currentNodes.size())) {
        return m_currentNodes[pointId].id;
    }
    
    return QUuid();
}

QUuid VtkSceneRenderer::pickBar(int displayX, int displayY) const
{
    if (!m_barPicker || !m_initialized) {
        return QUuid();
    }
    
    // Note: VTK picks work with display coordinates directly from Qt
    // No need to adjust Y coordinate
    m_barPicker->AddPickList(m_barActor);
    m_barPicker->PickFromListOn();
    
    m_barPicker->Pick(displayX, displayY, 0, m_renderer);
    vtkIdType cellId = m_barPicker->GetCellId();
    
    m_barPicker->PickFromListOff();
    
    if (cellId >= 0 && cellId < static_cast<vtkIdType>(m_currentBars.size())) {
        return m_currentBars[cellId].id;
    }
    
    return QUuid();
}

QUuid VtkSceneRenderer::pickGridLine(int displayX, int displayY) const
{
    if (!m_picker || !m_initialized) {
        return QUuid();
    }
    
    // Note: VTK picks work with display coordinates directly from Qt
    // No need to adjust Y coordinate
    m_picker->AddPickList(m_gridActor);
    m_picker->PickFromListOn();
    
    m_picker->Pick(displayX, displayY, 0, m_renderer);
    vtkIdType cellId = m_picker->GetCellId();
    
    m_picker->PickFromListOff();
    
    if (m_gridCellToLineIndex.contains(cellId)) {
        int lineIndex = m_gridCellToLineIndex[cellId];
        if (lineIndex >= 0 && lineIndex < static_cast<int>(m_currentGridLines.size())) {
            return m_currentGridLines[lineIndex].id;
        }
    }
    
    return QUuid();
}

bool VtkSceneRenderer::pickWorldPoint(int displayX, int displayY,
                                     double &x, double &y, double &z) const
{
    if (!m_renderer || !m_initialized) {
        return false;
    }
    
    // Use renderer's display to world conversion
    // This matches the old SceneController behavior
    double p0w[4], p1w[4];
    m_renderer->SetDisplayPoint(displayX, displayY, 0.0);
    m_renderer->DisplayToWorld();
    m_renderer->GetWorldPoint(p0w);
    if (std::abs(p0w[3]) > 1e-14) { 
        p0w[0] /= p0w[3]; 
        p0w[1] /= p0w[3]; 
        p0w[2] /= p0w[3]; 
    }
    
    m_renderer->SetDisplayPoint(displayX, displayY, 1.0);
    m_renderer->DisplayToWorld();
    m_renderer->GetWorldPoint(p1w);
    if (std::abs(p1w[3]) > 1e-14) { 
        p1w[0] /= p1w[3]; 
        p1w[1] /= p1w[3]; 
        p1w[2] /= p1w[3]; 
    }
    
    // Intersect ray with Z=0 plane
    double dx = p1w[0] - p0w[0];
    double dy = p1w[1] - p0w[1];
    double dz = p1w[2] - p0w[2];
    
    const double eps = 1e-14;
    if (std::abs(dz) < eps) {
        // Ray parallel to Z=0, use fallback
        auto *cam = m_renderer->GetActiveCamera();
        if (!cam) return false;
        
        double cpos[3]; 
        cam->GetPosition(cpos);
        dx = p1w[0] - cpos[0];
        dy = p1w[1] - cpos[1];
        dz = p1w[2] - cpos[2];
        
        if (std::abs(dz) < eps) {
            return false;
        }
        
        double t = (0.0 - cpos[2]) / dz;
        x = cpos[0] + t * dx;
        y = cpos[1] + t * dy;
        z = 0.0;
        return true;
    }
    
    double t = (0.0 - p0w[2]) / dz;
    x = p0w[0] + t * dx;
    y = p0w[1] + t * dy;
    z = 0.0;
    
    return true;
}

int VtkSceneRenderer::viewportHeight() const
{
    if (!m_initialized || !m_renderWindow) {
        return 0;
    }
    
    int *size = m_renderWindow->GetSize();
    return size ? size[1] : 0;
}

// Helper methods

void VtkSceneRenderer::applyNodeColor(int nodeIndex, const unsigned char color[3])
{
    if (nodeIndex >= 0 && nodeIndex < m_pointColors->GetNumberOfTuples()) {
        m_pointColors->SetTypedTuple(nodeIndex, color);
        m_pointColors->Modified();
    }
}

void VtkSceneRenderer::applyBarColor(int barIndex, const unsigned char color[3])
{
    if (barIndex >= 0 && barIndex < m_barColors->GetNumberOfTuples()) {
        m_barColors->SetTypedTuple(barIndex, color);
        m_barColors->Modified();
    }
}

void VtkSceneRenderer::applyGridLineColor(int lineIndex, const unsigned char color[3])
{
    if (lineIndex >= 0 && lineIndex < m_gridColors->GetNumberOfTuples()) {
        m_gridColors->SetTypedTuple(lineIndex, color);
        m_gridColors->Modified();
    }
}

int VtkSceneRenderer::findNodeIndex(const QUuid &id) const
{
    return m_nodeIndexById.value(id, -1);
}

int VtkSceneRenderer::findBarIndex(const QUuid &id) const
{
    return m_barIndexById.value(id, -1);
}

int VtkSceneRenderer::findGridLineIndex(const QUuid &id) const
{
    return m_gridLineIndexById.value(id, -1);
}

} // namespace Structura::Viz
