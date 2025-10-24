#include "SceneControllerFacade.h"

namespace Structura::App {

SceneControllerFacade::SceneControllerFacade(
    IModelRepository* repository,
    NodeService* nodeService,
    BarService* barService,
    Structura::Viz::ISceneRenderer* renderer,
    QObject* parent)
    : QObject(parent)
    , m_repository(repository)
    , m_nodeService(nodeService)
    , m_barService(barService)
    , m_renderer(renderer)
{
    // Connect to service signals
    if (m_nodeService) {
        connect(m_nodeService, &NodeService::nodeCreated,
                this, &SceneControllerFacade::onNodeCreated);
        connect(m_nodeService, &NodeService::nodeDeleted,
                this, &SceneControllerFacade::onNodeDeleted);
        connect(m_nodeService, &NodeService::nodeUpdated,
                this, &SceneControllerFacade::onNodeUpdated);
    }
    
    if (m_barService) {
        connect(m_barService, &BarService::barCreated,
                this, &SceneControllerFacade::onBarCreated);
        connect(m_barService, &BarService::barDeleted,
                this, &SceneControllerFacade::onBarDeleted);
        connect(m_barService, &BarService::barUpdated,
                this, &SceneControllerFacade::onBarUpdated);
    }
}

SceneControllerFacade::~SceneControllerFacade() = default;

void SceneControllerFacade::initialize()
{
    // Initial rendering of the entire model
    refreshAll();
}

void SceneControllerFacade::refreshAll()
{
    if (!m_renderer) {
        return;
    }
    
    Structura::Viz::ModelSnapshot snapshot;
    buildModelSnapshot(snapshot);
    m_renderer->renderSnapshot(snapshot);
    
    emit modelChanged();
}

void SceneControllerFacade::buildModelSnapshot(Structura::Viz::ModelSnapshot& snapshot) const
{
    if (!m_repository) {
        return;
    }
    
    // Convert nodes
    auto nodes = m_repository->allNodes();
    snapshot.nodes.reserve(nodes.size());
    for (const auto& node : nodes) {
        auto nodeData = convertToNodeData(node);
        nodeData.isSelected = m_selectedNodeIds.contains(node.id());
        nodeData.isHighlighted = (m_highlightedNodeId == node.id());
        snapshot.nodes.push_back(nodeData);
    }
    
    // Convert bars
    auto bars = m_repository->allBars();
    snapshot.bars.reserve(bars.size());
    for (const auto& bar : bars) {
        auto barData = convertToBarData(bar);
        barData.isSelected = m_selectedBarIds.contains(bar.id());
        snapshot.bars.push_back(barData);
    }
    
    // Convert grid lines
    snapshot.gridLines.reserve(m_gridLines.size());
    for (const auto& line : m_gridLines) {
        auto lineData = convertToGridLineData(line);
        lineData.isHighlighted = (m_highlightedGridLineId == line.id());
        snapshot.gridLines.push_back(lineData);
    }
    
    // TODO: Add loads, supports, LCS when available
    snapshot.showBarLCS = false;
}

Structura::Viz::ModelSnapshot::NodeData 
SceneControllerFacade::convertToNodeData(const Structura::Model::Node& node) const
{
    Structura::Viz::ModelSnapshot::NodeData data;
    data.id = node.id();
    data.externalId = node.externalId();
    data.x = node.position().x();
    data.y = node.position().y();
    data.z = node.position().z();
    data.isSelected = false; // Set by caller
    data.isHighlighted = false; // Set by caller
    return data;
}

Structura::Viz::ModelSnapshot::BarData 
SceneControllerFacade::convertToBarData(const Structura::Model::Bar& bar) const
{
    Structura::Viz::ModelSnapshot::BarData data;
    data.id = bar.id();
    data.externalId = bar.externalId();
    data.startNodeId = bar.startNodeId();
    data.endNodeId = bar.endNodeId();
    data.isSelected = false; // Set by caller
    return data;
}

Structura::Viz::ModelSnapshot::GridLineData 
SceneControllerFacade::convertToGridLineData(const Structura::Model::GridLine& line) const
{
    Structura::Viz::ModelSnapshot::GridLineData data;
    data.id = line.id();
    data.axis = static_cast<int>(line.axis());
    data.offset = line.offset();
    data.startPoint = {{line.startPoint().x(), line.startPoint().y(), line.startPoint().z()}};
    data.endPoint = {{line.endPoint().x(), line.endPoint().y(), line.endPoint().z()}};
    data.isHighlighted = false; // Set by caller
    data.isGhost = false;
    return data;
}

// Selection management

void SceneControllerFacade::setSelectedNodes(const QSet<QUuid>& nodeIds)
{
    if (m_selectedNodeIds == nodeIds) {
        return;
    }
    
    m_selectedNodeIds = nodeIds;
    
    if (m_renderer) {
        m_renderer->setSelectedNodes(nodeIds);
    }
    
    emit selectionChanged();
}

void SceneControllerFacade::setSelectedBars(const QSet<QUuid>& barIds)
{
    if (m_selectedBarIds == barIds) {
        return;
    }
    
    m_selectedBarIds = barIds;
    
    if (m_renderer) {
        m_renderer->setSelectedBars(barIds);
    }
    
    emit selectionChanged();
}

void SceneControllerFacade::clearSelection()
{
    bool changed = !m_selectedNodeIds.isEmpty() || !m_selectedBarIds.isEmpty();
    
    m_selectedNodeIds.clear();
    m_selectedBarIds.clear();
    
    if (m_renderer) {
        m_renderer->setSelectedNodes(QSet<QUuid>());
        m_renderer->setSelectedBars(QSet<QUuid>());
    }
    
    if (changed) {
        emit selectionChanged();
    }
}

// Highlighting

void SceneControllerFacade::highlightNode(const QUuid& nodeId)
{
    if (m_highlightedNodeId == nodeId) {
        return;
    }
    
    m_highlightedNodeId = nodeId;
    m_highlightedBarId = QUuid();
    m_highlightedGridLineId = QUuid();
    
    if (m_renderer) {
        m_renderer->highlightNode(nodeId);
    }
}

void SceneControllerFacade::highlightBar(const QUuid& barId)
{
    if (m_highlightedBarId == barId) {
        return;
    }
    
    m_highlightedNodeId = QUuid();
    m_highlightedBarId = barId;
    m_highlightedGridLineId = QUuid();
    
    // Bar highlighting would need implementation in ISceneRenderer
    // For now, just update state
}

void SceneControllerFacade::highlightGridLine(const QUuid& lineId)
{
    if (m_highlightedGridLineId == lineId) {
        return;
    }
    
    m_highlightedNodeId = QUuid();
    m_highlightedBarId = QUuid();
    m_highlightedGridLineId = lineId;
    
    if (m_renderer) {
        m_renderer->highlightGridLine(lineId);
    }
}

void SceneControllerFacade::clearHighlight()
{
    m_highlightedNodeId = QUuid();
    m_highlightedBarId = QUuid();
    m_highlightedGridLineId = QUuid();
    
    if (m_renderer) {
        m_renderer->highlightNode(QUuid());
        m_renderer->highlightGridLine(QUuid());
    }
}

// Grid visualization

void SceneControllerFacade::updateGridLines(const std::vector<Structura::Model::GridLine>& gridLines)
{
    m_gridLines = gridLines;
    
    if (m_renderer) {
        std::vector<Structura::Viz::ModelSnapshot::GridLineData> gridData;
        gridData.reserve(gridLines.size());
        
        for (const auto& line : gridLines) {
            auto data = convertToGridLineData(line);
            data.isHighlighted = (m_highlightedGridLineId == line.id());
            gridData.push_back(data);
        }
        
        m_renderer->updateGridLines(gridData);
    }
    
    emit modelChanged();
}

void SceneControllerFacade::showGridGhostLine(int axis, const Vector3& start, const Vector3& end)
{
    if (m_renderer) {
        std::array<double, 3> startArr = {{start.x(), start.y(), start.z()}};
        std::array<double, 3> endArr = {{end.x(), end.y(), end.z()}};
        m_renderer->showGridGhostLine(axis, startArr, endArr);
    }
}

void SceneControllerFacade::hideGridGhostLine()
{
    if (m_renderer) {
        m_renderer->hideGridGhostLine();
    }
}

// Camera and view

void SceneControllerFacade::resetCamera()
{
    if (m_renderer) {
        m_renderer->resetCamera();
    }
}

void SceneControllerFacade::zoomExtents()
{
    if (m_renderer) {
        m_renderer->zoomExtents();
    }
}

// Query operations

QUuid SceneControllerFacade::pickNode(int displayX, int displayY) const
{
    if (m_renderer) {
        return m_renderer->pickNode(displayX, displayY);
    }
    return QUuid();
}

QUuid SceneControllerFacade::pickBar(int displayX, int displayY) const
{
    if (m_renderer) {
        return m_renderer->pickBar(displayX, displayY);
    }
    return QUuid();
}

QUuid SceneControllerFacade::pickGridLine(int displayX, int displayY) const
{
    if (m_renderer) {
        return m_renderer->pickGridLine(displayX, displayY);
    }
    return QUuid();
}

bool SceneControllerFacade::pickWorldPoint(int displayX, int displayY, 
                                          double& x, double& y, double& z) const
{
    if (m_renderer) {
        return m_renderer->pickWorldPoint(displayX, displayY, x, y, z);
    }
    return false;
}

// Service event handlers

void SceneControllerFacade::onNodeCreated(const QUuid& nodeId)
{
    updateNodeRendering();
    emit modelChanged();
}

void SceneControllerFacade::onNodeDeleted(const QUuid& nodeId)
{
    // Remove from selection if deleted
    m_selectedNodeIds.remove(nodeId);
    if (m_highlightedNodeId == nodeId) {
        m_highlightedNodeId = QUuid();
    }
    
    updateNodeRendering();
    emit modelChanged();
}

void SceneControllerFacade::onNodeUpdated(const QUuid& nodeId)
{
    updateNodeRendering();
    emit modelChanged();
}

void SceneControllerFacade::onBarCreated(const QUuid& barId)
{
    updateBarRendering();
    emit modelChanged();
}

void SceneControllerFacade::onBarDeleted(const QUuid& barId)
{
    // Remove from selection if deleted
    m_selectedBarIds.remove(barId);
    if (m_highlightedBarId == barId) {
        m_highlightedBarId = QUuid();
    }
    
    updateBarRendering();
    emit modelChanged();
}

void SceneControllerFacade::onBarUpdated(const QUuid& barId)
{
    updateBarRendering();
    emit modelChanged();
}

// Helper methods

void SceneControllerFacade::updateNodeRendering()
{
    if (!m_renderer || !m_repository) {
        return;
    }
    
    auto nodes = m_repository->allNodes();
    std::vector<Structura::Viz::ModelSnapshot::NodeData> nodeData;
    nodeData.reserve(nodes.size());
    
    for (const auto& node : nodes) {
        auto data = convertToNodeData(node);
        data.isSelected = m_selectedNodeIds.contains(node.id());
        data.isHighlighted = (m_highlightedNodeId == node.id());
        nodeData.push_back(data);
    }
    
    m_renderer->updateNodes(nodeData);
}

void SceneControllerFacade::updateBarRendering()
{
    if (!m_renderer || !m_repository) {
        return;
    }
    
    auto bars = m_repository->allBars();
    std::vector<Structura::Viz::ModelSnapshot::BarData> barData;
    barData.reserve(bars.size());
    
    for (const auto& bar : bars) {
        auto data = convertToBarData(bar);
        data.isSelected = m_selectedBarIds.contains(bar.id());
        barData.push_back(data);
    }
    
    m_renderer->updateBars(barData);
}

} // namespace Structura::App
