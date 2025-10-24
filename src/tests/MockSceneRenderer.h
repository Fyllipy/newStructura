#pragma once

#include "../viz/ISceneRenderer.h"
#include <QSet>
#include <QUuid>
#include <vector>

namespace Structura::Tests {

/**
 * @brief Mock implementation of ISceneRenderer for unit testing
 * 
 * This test double records all method calls and provides inspection
 * capabilities to verify that the facade correctly coordinates with
 * the renderer.
 * 
 * Usage in tests:
 * @code
 * MockSceneRenderer mockRenderer;
 * SceneControllerFacade facade(&repo, &nodeService, &barService, &mockRenderer);
 * 
 * // Perform operations
 * nodeService.createNode(Vector3{1,2,3});
 * 
 * // Verify
 * QVERIFY(mockRenderer.wasUpdateNodesCalled());
 * QCOMPARE(mockRenderer.lastNodeCount(), 1);
 * @endcode
 */
class MockSceneRenderer : public Structura::Viz::ISceneRenderer
{
public:
    MockSceneRenderer() { reset(); }
    ~MockSceneRenderer() override = default;

    // ISceneRenderer interface implementation
    void initialize(QVTKOpenGLNativeWidget* widget) override {
        m_initializeCalled = true;
        m_widget = widget;
    }

    void renderSnapshot(const Structura::Viz::ModelSnapshot& snapshot) override {
        m_renderSnapshotCalled = true;
        m_renderSnapshotCallCount++;
        m_lastSnapshot = snapshot;
    }

    void updateNodes(const std::vector<Structura::Viz::ModelSnapshot::NodeData>& nodes) override {
        m_updateNodesCalled = true;
        m_updateNodesCallCount++;
        m_lastNodes = nodes;
    }

    void updateBars(const std::vector<Structura::Viz::ModelSnapshot::BarData>& bars) override {
        m_updateBarsCalled = true;
        m_updateBarsCallCount++;
        m_lastBars = bars;
    }

    void updateGridLines(const std::vector<Structura::Viz::ModelSnapshot::GridLineData>& gridLines) override {
        m_updateGridLinesCalled = true;
        m_updateGridLinesCallCount++;
        m_lastGridLines = gridLines;
    }

    void highlightNode(const QUuid& nodeId) override {
        m_highlightNodeCalled = true;
        m_lastHighlightedNodeId = nodeId;
    }

    void setSelectedNodes(const QSet<QUuid>& nodeIds) override {
        m_setSelectedNodesCalled = true;
        m_lastSelectedNodeIds = nodeIds;
    }

    void setSelectedBars(const QSet<QUuid>& barIds) override {
        m_setSelectedBarsCalled = true;
        m_lastSelectedBarIds = barIds;
    }

    void highlightGridLine(const QUuid& lineId) override {
        m_highlightGridLineCalled = true;
        m_lastHighlightedGridLineId = lineId;
    }

    void showGridGhostLine(int axis, const std::array<double, 3>& startPoint,
                          const std::array<double, 3>& endPoint) override {
        m_showGridGhostLineCalled = true;
        m_lastGhostLineAxis = axis;
        m_lastGhostLineStart = startPoint;
        m_lastGhostLineEnd = endPoint;
    }

    void hideGridGhostLine() override {
        m_hideGridGhostLineCalled = true;
    }

    void clearAll() override {
        m_clearAllCalled = true;
    }

    void resetCamera() override {
        m_resetCameraCalled = true;
    }

    void zoomExtents() override {
        m_zoomExtentsCalled = true;
    }

    void refresh() override {
        m_refreshCalled = true;
        m_refreshCallCount++;
    }

    QUuid pickNode(int displayX, int displayY) const override {
        m_pickNodeCalled = true;
        m_lastPickX = displayX;
        m_lastPickY = displayY;
        return m_mockPickedNodeId;
    }

    QUuid pickBar(int displayX, int displayY) const override {
        m_pickBarCalled = true;
        m_lastPickX = displayX;
        m_lastPickY = displayY;
        return m_mockPickedBarId;
    }

    QUuid pickGridLine(int displayX, int displayY) const override {
        m_pickGridLineCalled = true;
        m_lastPickX = displayX;
        m_lastPickY = displayY;
        return m_mockPickedGridLineId;
    }

    bool pickWorldPoint(int displayX, int displayY, double& x, double& y, double& z) const override {
        m_pickWorldPointCalled = true;
        m_lastPickX = displayX;
        m_lastPickY = displayY;
        x = m_mockWorldX;
        y = m_mockWorldY;
        z = m_mockWorldZ;
        return m_mockPickWorldPointSuccess;
    }

    // Test inspection methods
    void reset() {
        m_initializeCalled = false;
        m_renderSnapshotCalled = false;
        m_updateNodesCalled = false;
        m_updateBarsCalled = false;
        m_updateGridLinesCalled = false;
        m_highlightNodeCalled = false;
        m_setSelectedNodesCalled = false;
        m_setSelectedBarsCalled = false;
        m_highlightGridLineCalled = false;
        m_showGridGhostLineCalled = false;
        m_hideGridGhostLineCalled = false;
        m_clearAllCalled = false;
        m_resetCameraCalled = false;
        m_zoomExtentsCalled = false;
        m_refreshCalled = false;
        m_pickNodeCalled = false;
        m_pickBarCalled = false;
        m_pickGridLineCalled = false;
        m_pickWorldPointCalled = false;
        
        m_renderSnapshotCallCount = 0;
        m_updateNodesCallCount = 0;
        m_updateBarsCallCount = 0;
        m_updateGridLinesCallCount = 0;
        m_refreshCallCount = 0;
        
        m_lastNodes.clear();
        m_lastBars.clear();
        m_lastGridLines.clear();
        m_lastSelectedNodeIds.clear();
        m_lastSelectedBarIds.clear();
        m_lastHighlightedNodeId = QUuid();
        m_lastHighlightedGridLineId = QUuid();
    }

    // Query methods for test verification
    bool wasInitializeCalled() const { return m_initializeCalled; }
    bool wasRenderSnapshotCalled() const { return m_renderSnapshotCalled; }
    bool wasUpdateNodesCalled() const { return m_updateNodesCalled; }
    bool wasUpdateBarsCalled() const { return m_updateBarsCalled; }
    bool wasUpdateGridLinesCalled() const { return m_updateGridLinesCalled; }
    bool wasHighlightNodeCalled() const { return m_highlightNodeCalled; }
    bool wasSetSelectedNodesCalled() const { return m_setSelectedNodesCalled; }
    bool wasSetSelectedBarsCalled() const { return m_setSelectedBarsCalled; }
    bool wasHighlightGridLineCalled() const { return m_highlightGridLineCalled; }
    bool wasShowGridGhostLineCalled() const { return m_showGridGhostLineCalled; }
    bool wasHideGridGhostLineCalled() const { return m_hideGridGhostLineCalled; }
    bool wasClearAllCalled() const { return m_clearAllCalled; }
    bool wasResetCameraCalled() const { return m_resetCameraCalled; }
    bool wasZoomExtentsCalled() const { return m_zoomExtentsCalled; }
    bool wasRefreshCalled() const { return m_refreshCalled; }
    
    int renderSnapshotCallCount() const { return m_renderSnapshotCallCount; }
    int updateNodesCallCount() const { return m_updateNodesCallCount; }
    int updateBarsCallCount() const { return m_updateBarsCallCount; }
    int updateGridLinesCallCount() const { return m_updateGridLinesCallCount; }
    int refreshCallCount() const { return m_refreshCallCount; }
    
    const Structura::Viz::ModelSnapshot& lastSnapshot() const { return m_lastSnapshot; }
    const std::vector<Structura::Viz::ModelSnapshot::NodeData>& lastNodes() const { return m_lastNodes; }
    const std::vector<Structura::Viz::ModelSnapshot::BarData>& lastBars() const { return m_lastBars; }
    const std::vector<Structura::Viz::ModelSnapshot::GridLineData>& lastGridLines() const { return m_lastGridLines; }
    
    size_t lastNodeCount() const { return m_lastNodes.size(); }
    size_t lastBarCount() const { return m_lastBars.size(); }
    size_t lastGridLineCount() const { return m_lastGridLines.size(); }
    
    const QSet<QUuid>& lastSelectedNodeIds() const { return m_lastSelectedNodeIds; }
    const QSet<QUuid>& lastSelectedBarIds() const { return m_lastSelectedBarIds; }
    QUuid lastHighlightedNodeId() const { return m_lastHighlightedNodeId; }
    QUuid lastHighlightedGridLineId() const { return m_lastHighlightedGridLineId; }
    
    // Mock return value setters (for pick operations)
    void setMockPickedNodeId(const QUuid& id) { m_mockPickedNodeId = id; }
    void setMockPickedBarId(const QUuid& id) { m_mockPickedBarId = id; }
    void setMockPickedGridLineId(const QUuid& id) { m_mockPickedGridLineId = id; }
    void setMockWorldPoint(double x, double y, double z, bool success = true) {
        m_mockWorldX = x;
        m_mockWorldY = y;
        m_mockWorldZ = z;
        m_mockPickWorldPointSuccess = success;
    }

private:
    // Call tracking flags
    mutable bool m_initializeCalled;
    mutable bool m_renderSnapshotCalled;
    mutable bool m_updateNodesCalled;
    mutable bool m_updateBarsCalled;
    mutable bool m_updateGridLinesCalled;
    mutable bool m_highlightNodeCalled;
    mutable bool m_setSelectedNodesCalled;
    mutable bool m_setSelectedBarsCalled;
    mutable bool m_highlightGridLineCalled;
    mutable bool m_showGridGhostLineCalled;
    mutable bool m_hideGridGhostLineCalled;
    mutable bool m_clearAllCalled;
    mutable bool m_resetCameraCalled;
    mutable bool m_zoomExtentsCalled;
    mutable bool m_refreshCalled;
    mutable bool m_pickNodeCalled;
    mutable bool m_pickBarCalled;
    mutable bool m_pickGridLineCalled;
    mutable bool m_pickWorldPointCalled;
    
    // Call counters
    int m_renderSnapshotCallCount;
    int m_updateNodesCallCount;
    int m_updateBarsCallCount;
    int m_updateGridLinesCallCount;
    int m_refreshCallCount;
    
    // Last call data
    QVTKOpenGLNativeWidget* m_widget = nullptr;
    Structura::Viz::ModelSnapshot m_lastSnapshot;
    std::vector<Structura::Viz::ModelSnapshot::NodeData> m_lastNodes;
    std::vector<Structura::Viz::ModelSnapshot::BarData> m_lastBars;
    std::vector<Structura::Viz::ModelSnapshot::GridLineData> m_lastGridLines;
    QSet<QUuid> m_lastSelectedNodeIds;
    QSet<QUuid> m_lastSelectedBarIds;
    QUuid m_lastHighlightedNodeId;
    QUuid m_lastHighlightedGridLineId;
    int m_lastGhostLineAxis;
    std::array<double, 3> m_lastGhostLineStart;
    std::array<double, 3> m_lastGhostLineEnd;
    mutable int m_lastPickX;
    mutable int m_lastPickY;
    
    // Mock return values
    QUuid m_mockPickedNodeId;
    QUuid m_mockPickedBarId;
    QUuid m_mockPickedGridLineId;
    double m_mockWorldX = 0.0;
    double m_mockWorldY = 0.0;
    double m_mockWorldZ = 0.0;
    bool m_mockPickWorldPointSuccess = true;
};

} // namespace Structura::Tests
