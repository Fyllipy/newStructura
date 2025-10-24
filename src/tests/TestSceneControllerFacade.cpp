#include <QtTest>
#include "../app/SceneControllerFacade.h"
#include "../app/InMemoryModelRepository.h"
#include "../app/NodeService.h"
#include "../app/BarService.h"
#include "MockSceneRenderer.h"

using namespace Structura::App;
using namespace Structura::Core::Model;
using namespace Structura::Tests;

/**
 * @brief Unit tests for SceneControllerFacade
 * 
 * Tests verify that the facade correctly:
 * - Coordinates between services and renderer
 * - Translates domain events to rendering updates
 * - Manages selection and highlighting state
 * - Delegates operations appropriately
 */
class TestSceneControllerFacade : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    
    // Initialization tests
    void testInitialize();
    void testRefreshAll();
    
    // Node event handling
    void testNodeCreatedTriggersUpdate();
    void testNodeDeletedTriggersUpdate();
    void testNodeUpdatedTriggersUpdate();
    void testNodeDeletedClearsSelection();
    void testNodeDeletedClearsHighlight();
    
    // Bar event handling
    void testBarCreatedTriggersUpdate();
    void testBarDeletedTriggersUpdate();
    void testBarUpdatedTriggersUpdate();
    void testBarDeletedClearsSelection();
    
    // Selection management
    void testSetSelectedNodes();
    void testSetSelectedBars();
    void testClearSelection();
    void testSelectionPersistsAcrossUpdates();
    
    // Highlighting
    void testHighlightNode();
    void testHighlightGridLine();
    void testClearHighlight();
    void testHighlightingOneTypeClearsOthers();
    
    // Grid operations
    void testUpdateGridLines();
    void testShowHideGridGhostLine();
    
    // Camera operations
    void testResetCamera();
    void testZoomExtents();
    
    // Picking operations
    void testPickNode();
    void testPickBar();
    void testPickGridLine();
    void testPickWorldPoint();
    
    // Signal emission
    void testModelChangedSignal();
    void testSelectionChangedSignal();

private:
    InMemoryModelRepository* m_repository;
    NodeService* m_nodeService;
    BarService* m_barService;
    MockSceneRenderer* m_mockRenderer;
    SceneControllerFacade* m_facade;
};

void TestSceneControllerFacade::initTestCase()
{
    // One-time setup
}

void TestSceneControllerFacade::init()
{
    // Create fresh instances for each test
    m_repository = new InMemoryModelRepository();
    m_nodeService = new NodeService(m_repository, this);
    m_barService = new BarService(m_repository, this);
    m_mockRenderer = new MockSceneRenderer();
    m_facade = new SceneControllerFacade(m_repository, m_nodeService, m_barService, m_mockRenderer, this);
}

void TestSceneControllerFacade::cleanup()
{
    delete m_facade;
    delete m_mockRenderer;
    delete m_barService;
    delete m_nodeService;
    delete m_repository;
}

void TestSceneControllerFacade::testInitialize()
{
    m_facade->initialize();
    
    QVERIFY(m_mockRenderer->wasRenderSnapshotCalled());
    QCOMPARE(m_mockRenderer->renderSnapshotCallCount(), 1);
}

void TestSceneControllerFacade::testRefreshAll()
{
    // Add some data
    auto node1 = m_nodeService->createNode(Vector3{1, 2, 3});
    auto node2 = m_nodeService->createNode(Vector3{4, 5, 6});
    auto bar1 = m_barService->createBar(node1, node2);
    
    m_mockRenderer->reset();
    
    m_facade->refreshAll();
    
    QVERIFY(m_mockRenderer->wasRenderSnapshotCalled());
    QCOMPARE(m_mockRenderer->lastNodeCount(), 2);
    QCOMPARE(m_mockRenderer->lastBarCount(), 1);
}

void TestSceneControllerFacade::testNodeCreatedTriggersUpdate()
{
    m_mockRenderer->reset();
    
    auto nodeId = m_nodeService->createNode(Vector3{1, 2, 3});
    
    QVERIFY(m_mockRenderer->wasUpdateNodesCalled());
    QCOMPARE(m_mockRenderer->lastNodeCount(), 1);
}

void TestSceneControllerFacade::testNodeDeletedTriggersUpdate()
{
    auto nodeId = m_nodeService->createNode(Vector3{1, 2, 3});
    m_mockRenderer->reset();
    
    m_nodeService->deleteNode(nodeId);
    
    QVERIFY(m_mockRenderer->wasUpdateNodesCalled());
    QCOMPARE(m_mockRenderer->lastNodeCount(), 0);
}

void TestSceneControllerFacade::testNodeUpdatedTriggersUpdate()
{
    auto nodeId = m_nodeService->createNode(Vector3{1, 2, 3});
    m_mockRenderer->reset();
    
    m_nodeService->setNodePosition(nodeId, Vector3{10, 20, 30});
    
    QVERIFY(m_mockRenderer->wasUpdateNodesCalled());
    QCOMPARE(m_mockRenderer->lastNodeCount(), 1);
    
    // Verify position was updated
    const auto& nodes = m_mockRenderer->lastNodes();
    QCOMPARE(nodes[0].x, 10.0);
    QCOMPARE(nodes[0].y, 20.0);
    QCOMPARE(nodes[0].z, 30.0);
}

void TestSceneControllerFacade::testNodeDeletedClearsSelection()
{
    auto node1 = m_nodeService->createNode(Vector3{1, 2, 3});
    auto node2 = m_nodeService->createNode(Vector3{4, 5, 6});
    
    QSet<QUuid> selection;
    selection.insert(node1);
    selection.insert(node2);
    m_facade->setSelectedNodes(selection);
    
    m_nodeService->deleteNode(node1);
    
    // Verify node1 was removed from selection
    const auto& lastSelection = m_mockRenderer->lastSelectedNodeIds();
    QVERIFY(!lastSelection.contains(node1));
    QVERIFY(lastSelection.contains(node2));
}

void TestSceneControllerFacade::testNodeDeletedClearsHighlight()
{
    auto nodeId = m_nodeService->createNode(Vector3{1, 2, 3});
    
    m_facade->highlightNode(nodeId);
    QVERIFY(m_mockRenderer->wasHighlightNodeCalled());
    
    m_nodeService->deleteNode(nodeId);
    
    // After deletion, highlight should be cleared (null UUID)
    QVERIFY(m_mockRenderer->lastHighlightedNodeId().isNull());
}

void TestSceneControllerFacade::testBarCreatedTriggersUpdate()
{
    auto node1 = m_nodeService->createNode(Vector3{0, 0, 0});
    auto node2 = m_nodeService->createNode(Vector3{1, 1, 1});
    
    m_mockRenderer->reset();
    
    auto barId = m_barService->createBar(node1, node2);
    
    QVERIFY(m_mockRenderer->wasUpdateBarsCalled());
    QCOMPARE(m_mockRenderer->lastBarCount(), 1);
}

void TestSceneControllerFacade::testBarDeletedTriggersUpdate()
{
    auto node1 = m_nodeService->createNode(Vector3{0, 0, 0});
    auto node2 = m_nodeService->createNode(Vector3{1, 1, 1});
    auto barId = m_barService->createBar(node1, node2);
    
    m_mockRenderer->reset();
    
    m_barService->deleteBar(barId);
    
    QVERIFY(m_mockRenderer->wasUpdateBarsCalled());
    QCOMPARE(m_mockRenderer->lastBarCount(), 0);
}

void TestSceneControllerFacade::testBarUpdatedTriggersUpdate()
{
    auto node1 = m_nodeService->createNode(Vector3{0, 0, 0});
    auto node2 = m_nodeService->createNode(Vector3{1, 1, 1});
    auto barId = m_barService->createBar(node1, node2);
    
    m_mockRenderer->reset();
    
    // Trigger an update (e.g., by setting external ID)
    auto bar = m_repository->findBar(barId);
    QVERIFY(bar.has_value());
    bar->setExternalId(100);
    m_repository->updateBar(*bar);
    
    QVERIFY(m_mockRenderer->wasUpdateBarsCalled());
}

void TestSceneControllerFacade::testBarDeletedClearsSelection()
{
    auto node1 = m_nodeService->createNode(Vector3{0, 0, 0});
    auto node2 = m_nodeService->createNode(Vector3{1, 1, 1});
    auto bar1 = m_barService->createBar(node1, node2);
    auto bar2 = m_barService->createBar(node2, node1);
    
    QSet<QUuid> selection;
    selection.insert(bar1);
    selection.insert(bar2);
    m_facade->setSelectedBars(selection);
    
    m_barService->deleteBar(bar1);
    
    const auto& lastSelection = m_mockRenderer->lastSelectedBarIds();
    QVERIFY(!lastSelection.contains(bar1));
    QVERIFY(lastSelection.contains(bar2));
}

void TestSceneControllerFacade::testSetSelectedNodes()
{
    auto node1 = m_nodeService->createNode(Vector3{1, 2, 3});
    auto node2 = m_nodeService->createNode(Vector3{4, 5, 6});
    
    QSet<QUuid> selection;
    selection.insert(node1);
    selection.insert(node2);
    
    m_facade->setSelectedNodes(selection);
    
    QVERIFY(m_mockRenderer->wasSetSelectedNodesCalled());
    QCOMPARE(m_mockRenderer->lastSelectedNodeIds().size(), 2);
    QVERIFY(m_mockRenderer->lastSelectedNodeIds().contains(node1));
    QVERIFY(m_mockRenderer->lastSelectedNodeIds().contains(node2));
}

void TestSceneControllerFacade::testSetSelectedBars()
{
    auto node1 = m_nodeService->createNode(Vector3{0, 0, 0});
    auto node2 = m_nodeService->createNode(Vector3{1, 1, 1});
    auto bar1 = m_barService->createBar(node1, node2);
    
    QSet<QUuid> selection;
    selection.insert(bar1);
    
    m_facade->setSelectedBars(selection);
    
    QVERIFY(m_mockRenderer->wasSetSelectedBarsCalled());
    QCOMPARE(m_mockRenderer->lastSelectedBarIds().size(), 1);
    QVERIFY(m_mockRenderer->lastSelectedBarIds().contains(bar1));
}

void TestSceneControllerFacade::testClearSelection()
{
    auto node1 = m_nodeService->createNode(Vector3{1, 2, 3});
    
    QSet<QUuid> selection;
    selection.insert(node1);
    m_facade->setSelectedNodes(selection);
    
    m_mockRenderer->reset();
    
    m_facade->clearSelection();
    
    QVERIFY(m_mockRenderer->wasSetSelectedNodesCalled());
    QVERIFY(m_mockRenderer->wasSetSelectedBarsCalled());
    QCOMPARE(m_mockRenderer->lastSelectedNodeIds().size(), 0);
    QCOMPARE(m_mockRenderer->lastSelectedBarIds().size(), 0);
}

void TestSceneControllerFacade::testSelectionPersistsAcrossUpdates()
{
    auto node1 = m_nodeService->createNode(Vector3{1, 2, 3});
    auto node2 = m_nodeService->createNode(Vector3{4, 5, 6});
    
    QSet<QUuid> selection;
    selection.insert(node1);
    m_facade->setSelectedNodes(selection);
    
    // Update a node position
    m_nodeService->setNodePosition(node2, Vector3{10, 20, 30});
    
    // Selection should still be marked in the updated data
    const auto& nodes = m_mockRenderer->lastNodes();
    bool node1Selected = false;
    bool node2Selected = false;
    
    for (const auto& node : nodes) {
        if (node.id == node1) node1Selected = node.isSelected;
        if (node.id == node2) node2Selected = node.isSelected;
    }
    
    QVERIFY(node1Selected);
    QVERIFY(!node2Selected);
}

void TestSceneControllerFacade::testHighlightNode()
{
    auto nodeId = m_nodeService->createNode(Vector3{1, 2, 3});
    
    m_facade->highlightNode(nodeId);
    
    QVERIFY(m_mockRenderer->wasHighlightNodeCalled());
    QCOMPARE(m_mockRenderer->lastHighlightedNodeId(), nodeId);
}

void TestSceneControllerFacade::testHighlightGridLine()
{
    QUuid lineId = QUuid::createUuid();
    
    m_facade->highlightGridLine(lineId);
    
    QVERIFY(m_mockRenderer->wasHighlightGridLineCalled());
    QCOMPARE(m_mockRenderer->lastHighlightedGridLineId(), lineId);
}

void TestSceneControllerFacade::testClearHighlight()
{
    auto nodeId = m_nodeService->createNode(Vector3{1, 2, 3});
    m_facade->highlightNode(nodeId);
    
    m_mockRenderer->reset();
    
    m_facade->clearHighlight();
    
    QVERIFY(m_mockRenderer->wasHighlightNodeCalled());
    QVERIFY(m_mockRenderer->lastHighlightedNodeId().isNull());
}

void TestSceneControllerFacade::testHighlightingOneTypeClearsOthers()
{
    auto nodeId = m_nodeService->createNode(Vector3{1, 2, 3});
    QUuid lineId = QUuid::createUuid();
    
    // Highlight node first
    m_facade->highlightNode(nodeId);
    QCOMPARE(m_mockRenderer->lastHighlightedNodeId(), nodeId);
    
    // Highlight grid line should clear node highlight
    m_facade->highlightGridLine(lineId);
    QVERIFY(m_mockRenderer->lastHighlightedNodeId().isNull());
    QCOMPARE(m_mockRenderer->lastHighlightedGridLineId(), lineId);
}

void TestSceneControllerFacade::testUpdateGridLines()
{
    std::vector<GridLine> gridLines;
    GridLine line1(0, 5.0);
    line1.setStartPoint(Vector3{0, 0, 0});
    line1.setEndPoint(Vector3{10, 0, 0});
    gridLines.push_back(line1);
    
    m_facade->updateGridLines(gridLines);
    
    QVERIFY(m_mockRenderer->wasUpdateGridLinesCalled());
    QCOMPARE(m_mockRenderer->lastGridLineCount(), 1);
}

void TestSceneControllerFacade::testShowHideGridGhostLine()
{
    Vector3 start{0, 0, 0};
    Vector3 end{10, 0, 0};
    
    m_facade->showGridGhostLine(0, start, end);
    QVERIFY(m_mockRenderer->wasShowGridGhostLineCalled());
    
    m_facade->hideGridGhostLine();
    QVERIFY(m_mockRenderer->wasHideGridGhostLineCalled());
}

void TestSceneControllerFacade::testResetCamera()
{
    m_facade->resetCamera();
    QVERIFY(m_mockRenderer->wasResetCameraCalled());
}

void TestSceneControllerFacade::testZoomExtents()
{
    m_facade->zoomExtents();
    QVERIFY(m_mockRenderer->wasZoomExtentsCalled());
}

void TestSceneControllerFacade::testPickNode()
{
    auto nodeId = m_nodeService->createNode(Vector3{1, 2, 3});
    m_mockRenderer->setMockPickedNodeId(nodeId);
    
    QUuid picked = m_facade->pickNode(100, 200);
    
    QCOMPARE(picked, nodeId);
}

void TestSceneControllerFacade::testPickBar()
{
    auto node1 = m_nodeService->createNode(Vector3{0, 0, 0});
    auto node2 = m_nodeService->createNode(Vector3{1, 1, 1});
    auto barId = m_barService->createBar(node1, node2);
    
    m_mockRenderer->setMockPickedBarId(barId);
    
    QUuid picked = m_facade->pickBar(100, 200);
    
    QCOMPARE(picked, barId);
}

void TestSceneControllerFacade::testPickGridLine()
{
    QUuid lineId = QUuid::createUuid();
    m_mockRenderer->setMockPickedGridLineId(lineId);
    
    QUuid picked = m_facade->pickGridLine(100, 200);
    
    QCOMPARE(picked, lineId);
}

void TestSceneControllerFacade::testPickWorldPoint()
{
    m_mockRenderer->setMockWorldPoint(1.5, 2.5, 3.5, true);
    
    double x, y, z;
    bool success = m_facade->pickWorldPoint(100, 200, x, y, z);
    
    QVERIFY(success);
    QCOMPARE(x, 1.5);
    QCOMPARE(y, 2.5);
    QCOMPARE(z, 3.5);
}

void TestSceneControllerFacade::testModelChangedSignal()
{
    QSignalSpy spy(m_facade, &SceneControllerFacade::modelChanged);
    
    m_nodeService->createNode(Vector3{1, 2, 3});
    
    QCOMPARE(spy.count(), 1);
}

void TestSceneControllerFacade::testSelectionChangedSignal()
{
    QSignalSpy spy(m_facade, &SceneControllerFacade::selectionChanged);
    
    auto nodeId = m_nodeService->createNode(Vector3{1, 2, 3});
    
    QSet<QUuid> selection;
    selection.insert(nodeId);
    m_facade->setSelectedNodes(selection);
    
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestSceneControllerFacade)
#include "TestSceneControllerFacade.moc"
