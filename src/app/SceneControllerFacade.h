#pragma once

#include "IModelRepository.h"
#include "NodeService.h"
#include "BarService.h"
#include "../viz/ISceneRenderer.h"
#include "../core/model/ModelEntities.h"

#include <QObject>
#include <QUuid>
#include <memory>

namespace Structura::App {

/**
 * @brief Facade that coordinates between domain services and the rendering layer
 * 
 * This class acts as an adapter/facade that:
 * - Listens to model change events from services (NodeService, BarService)
 * - Translates domain model changes to renderer updates via ISceneRenderer
 * - Maintains no VTK dependencies (depends only on abstractions)
 * - Provides high-level operations for the UI layer
 * 
 * Design Pattern: Facade + Observer
 * - Facade: Simplifies interaction between multiple subsystems
 * - Observer: Reacts to service events and propagates to renderer
 */
class SceneControllerFacade : public QObject
{
    Q_OBJECT

public:
    explicit SceneControllerFacade(
        IModelRepository* repository,
        NodeService* nodeService,
        BarService* barService,
        Structura::Viz::ISceneRenderer* renderer,
        QObject* parent = nullptr);
    
    ~SceneControllerFacade() override;

    // Initialization
    void initialize();
    
    // Full model synchronization
    void refreshAll();
    
    // Selection management
    void setSelectedNodes(const QSet<QUuid>& nodeIds);
    void setSelectedBars(const QSet<QUuid>& barIds);
    void clearSelection();
    
    // Highlighting (hover effects)
    void highlightNode(const QUuid& nodeId);
    void highlightBar(const QUuid& barId);
    void highlightGridLine(const QUuid& lineId);
    void clearHighlight();
    
    // Grid visualization
    void updateGridLines(const std::vector<Structura::Model::GridLine>& gridLines);
    void showGridGhostLine(int axis, const Vector3& start, const Vector3& end);
    void hideGridGhostLine();
    
    // Camera and view
    void resetCamera();
    void zoomExtents();
    
    // Query operations (delegated to renderer)
    QUuid pickNode(int displayX, int displayY) const;
    QUuid pickBar(int displayX, int displayY) const;
    QUuid pickGridLine(int displayX, int displayY) const;
    bool pickWorldPoint(int displayX, int displayY, double& x, double& y, double& z) const;

signals:
    // Forwarded from services for UI coordination
    void modelChanged();
    void selectionChanged();

private slots:
    // Service event handlers
    void onNodeCreated(const QUuid& nodeId);
    void onNodeDeleted(const QUuid& nodeId);
    void onNodeUpdated(const QUuid& nodeId);
    
    void onBarCreated(const QUuid& barId);
    void onBarDeleted(const QUuid& barId);
    void onBarUpdated(const QUuid& barId);

private:
    // Helper methods
    void buildModelSnapshot(Structura::Viz::ModelSnapshot& snapshot) const;
    void updateNodeRendering();
    void updateBarRendering();
    
    Structura::Viz::ModelSnapshot::NodeData convertToNodeData(const Structura::Model::Node& node) const;
    Structura::Viz::ModelSnapshot::BarData convertToBarData(const Structura::Model::Bar& bar) const;
    Structura::Viz::ModelSnapshot::GridLineData convertToGridLineData(const Structura::Model::GridLine& line) const;

private:
    // Dependencies (non-owning pointers)
    IModelRepository* m_repository;
    NodeService* m_nodeService;
    BarService* m_barService;
    Structura::Viz::ISceneRenderer* m_renderer;
    
    // Current selection state
    QSet<QUuid> m_selectedNodeIds;
    QSet<QUuid> m_selectedBarIds;
    
    // Current highlight state
    QUuid m_highlightedNodeId;
    QUuid m_highlightedBarId;
    QUuid m_highlightedGridLineId;
    
    // Grid lines (managed separately from repository)
    std::vector<Structura::Model::GridLine> m_gridLines;
};

} // namespace Structura::App
