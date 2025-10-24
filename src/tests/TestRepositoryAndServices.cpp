#include <QtTest/QtTest>
#include "../app/InMemoryModelRepository.h"
#include "../app/NodeService.h"
#include "../app/BarService.h"

using namespace Structura::App;
using namespace Structura::Model;

/**
 * @brief Unit tests for InMemoryModelRepository
 */
class TestInMemoryModelRepository : public QObject
{
    Q_OBJECT

private slots:
    void testNodeOperations()
    {
        InMemoryModelRepository repo;
        
        // Test adding nodes
        Node node1(QUuid::createUuid(), 1, 0.0, 0.0, 0.0);
        Node node2(QUuid::createUuid(), 2, 1.0, 2.0, 3.0);
        
        QVERIFY(repo.addNode(node1));
        QVERIFY(repo.addNode(node2));
        QCOMPARE(repo.nodeCount(), static_cast<size_t>(2));
        
        // Test duplicate ID
        QVERIFY(!repo.addNode(node1));
        
        // Test finding
        auto found = repo.findNode(node1.id());
        QVERIFY(found.has_value());
        QCOMPARE(found->id(), node1.id());
        
        // Test finding by external ID
        auto foundExt = repo.findNodeByExternalId(2);
        QVERIFY(foundExt.has_value());
        QCOMPARE(foundExt->externalId(), 2);
        
        // Test updating
        Node updatedNode = node1;
        updatedNode.setPosition(10.0, 20.0, 30.0);
        QVERIFY(repo.updateNode(updatedNode));
        
        auto updated = repo.findNode(node1.id());
        QCOMPARE(updated->x(), 10.0);
        
        // Test removing
        QVERIFY(repo.removeNode(node1.id()));
        QCOMPARE(repo.nodeCount(), static_cast<size_t>(1));
        QVERIFY(!repo.findNode(node1.id()).has_value());
        
        // Test clearNodes
        repo.clearNodes();
        QCOMPARE(repo.nodeCount(), static_cast<size_t>(0));
    }
    
    void testBarOperations()
    {
        InMemoryModelRepository repo;
        
        QUuid startId = QUuid::createUuid();
        QUuid endId = QUuid::createUuid();
        
        Bar bar1(QUuid::createUuid(), startId, endId);
        bar1.setExternalId(1);
        
        QVERIFY(repo.addBar(bar1));
        QCOMPARE(repo.barCount(), static_cast<size_t>(1));
        
        auto found = repo.findBar(bar1.id());
        QVERIFY(found.has_value());
        QCOMPARE(found->startNodeId(), startId);
        
        // Test findBarsConnectedToNode
        Bar bar2(QUuid::createUuid(), startId, QUuid::createUuid());
        repo.addBar(bar2);
        
        auto connected = repo.findBarsConnectedToNode(startId);
        QCOMPARE(connected.size(), static_cast<size_t>(2));
        
        repo.clearBars();
        QCOMPARE(repo.barCount(), static_cast<size_t>(0));
    }
    
    void testMaterialAndSectionOperations()
    {
        InMemoryModelRepository repo;
        
        Material mat(QUuid::createUuid(), 1, "Steel", 200000.0, 80000.0);
        Section sec(QUuid::createUuid(), 1, "IPE300", 5381.0, 8356.0, 603.8, 20.12);
        
        QVERIFY(repo.addMaterial(mat));
        QVERIFY(repo.addSection(sec));
        
        QCOMPARE(repo.materialCount(), static_cast<size_t>(1));
        QCOMPARE(repo.sectionCount(), static_cast<size_t>(1));
        
        auto foundMat = repo.findMaterial(mat.id());
        auto foundSec = repo.findSection(sec.id());
        
        QVERIFY(foundMat.has_value());
        QVERIFY(foundSec.has_value());
        QCOMPARE(foundMat->name(), QString("Steel"));
        QCOMPARE(foundSec->name(), QString("IPE300"));
    }
    
    void testClearAll()
    {
        InMemoryModelRepository repo;
        
        repo.addNode(Node(QUuid::createUuid(), 1, 0, 0, 0));
        repo.addBar(Bar(QUuid::createUuid(), QUuid::createUuid(), QUuid::createUuid()));
        repo.addMaterial(Material(QUuid::createUuid(), 1, "Test", 1000, 500));
        
        QVERIFY(!repo.isEmpty());
        
        repo.clearAll();
        
        QVERIFY(repo.isEmpty());
        QCOMPARE(repo.nodeCount(), static_cast<size_t>(0));
        QCOMPARE(repo.barCount(), static_cast<size_t>(0));
        QCOMPARE(repo.materialCount(), static_cast<size_t>(0));
    }
};

/**
 * @brief Unit tests for NodeService
 */
class TestNodeService : public QObject
{
    Q_OBJECT

private slots:
    void testCreateNode()
    {
        InMemoryModelRepository repo;
        NodeService service(&repo);
        
        Vector3 pos(1.0, 2.0, 3.0);
        QUuid id = service.createNode(pos);
        
        QVERIFY(!id.isNull());
        QCOMPARE(repo.nodeCount(), static_cast<size_t>(1));
        
        auto node = service.findNode(id);
        QVERIFY(node.has_value());
        QCOMPARE(node->x(), 1.0);
        QCOMPARE(node->y(), 2.0);
        QCOMPARE(node->z(), 3.0);
        QCOMPARE(node->externalId(), 1);
    }
    
    void testCreateMultipleNodes()
    {
        InMemoryModelRepository repo;
        NodeService service(&repo);
        
        QUuid id1 = service.createNode(Vector3(0, 0, 0));
        QUuid id2 = service.createNode(Vector3(1, 1, 1));
        
        QCOMPARE(service.nodeCount(), static_cast<size_t>(2));
        
        auto node1 = service.findNode(id1);
        auto node2 = service.findNode(id2);
        
        QCOMPARE(node1->externalId(), 1);
        QCOMPARE(node2->externalId(), 2);
    }
    
    void testDeleteNode()
    {
        InMemoryModelRepository repo;
        NodeService service(&repo);
        
        QUuid id = service.createNode(Vector3(0, 0, 0));
        QVERIFY(service.nodeExists(id));
        
        QVERIFY(service.deleteNode(id));
        QVERIFY(!service.nodeExists(id));
        QCOMPARE(service.nodeCount(), static_cast<size_t>(0));
    }
    
    void testSetNodePosition()
    {
        InMemoryModelRepository repo;
        NodeService service(&repo);
        
        QUuid id = service.createNode(Vector3(0, 0, 0));
        Vector3 newPos(10, 20, 30);
        
        QVERIFY(service.setNodePosition(id, newPos));
        
        auto node = service.findNode(id);
        QCOMPARE(node->x(), 10.0);
        QCOMPARE(node->y(), 20.0);
        QCOMPARE(node->z(), 30.0);
    }
    
    void testSetNodeRestraints()
    {
        InMemoryModelRepository repo;
        NodeService service(&repo);
        
        QUuid id = service.createNode(Vector3(0, 0, 0));
        std::array<bool, 6> restraints = {true, true, false, false, false, false};
        
        QVERIFY(service.setNodeRestraints(id, restraints));
        
        auto node = service.findNode(id);
        auto nodeRestraints = node->restraints();
        QVERIFY(nodeRestraints[0]);
        QVERIFY(nodeRestraints[1]);
        QVERIFY(!nodeRestraints[2]);
    }
    
    void testNextExternalId()
    {
        InMemoryModelRepository repo;
        NodeService service(&repo);
        
        QCOMPARE(service.nextExternalId(), 1);
        
        service.createNode(Vector3(0, 0, 0));
        QCOMPARE(service.nextExternalId(), 2);
        
        service.createNode(Vector3(1, 1, 1));
        QCOMPARE(service.nextExternalId(), 3);
    }
};

/**
 * @brief Unit tests for BarService
 */
class TestBarService : public QObject
{
    Q_OBJECT

private slots:
    void testCreateBar()
    {
        InMemoryModelRepository repo;
        NodeService nodeService(&repo);
        BarService barService(&repo);
        
        QUuid node1 = nodeService.createNode(Vector3(0, 0, 0));
        QUuid node2 = nodeService.createNode(Vector3(3, 4, 0));
        
        QUuid barId = barService.createBar(node1, node2);
        
        QVERIFY(!barId.isNull());
        QCOMPARE(barService.barCount(), static_cast<size_t>(1));
        
        auto bar = barService.findBar(barId);
        QVERIFY(bar.has_value());
        QCOMPARE(bar->startNodeId(), node1);
        QCOMPARE(bar->endNodeId(), node2);
        QCOMPARE(bar->externalId(), 1);
    }
    
    void testCreateBarWithInvalidNodes()
    {
        InMemoryModelRepository repo;
        BarService barService(&repo);
        
        QUuid fakeId1 = QUuid::createUuid();
        QUuid fakeId2 = QUuid::createUuid();
        
        QUuid barId = barService.createBar(fakeId1, fakeId2);
        QVERIFY(barId.isNull());
    }
    
    void testCreateBarSameNode()
    {
        InMemoryModelRepository repo;
        NodeService nodeService(&repo);
        BarService barService(&repo);
        
        QUuid node1 = nodeService.createNode(Vector3(0, 0, 0));
        
        QUuid barId = barService.createBar(node1, node1);
        QVERIFY(barId.isNull());
    }
    
    void testDeleteBar()
    {
        InMemoryModelRepository repo;
        NodeService nodeService(&repo);
        BarService barService(&repo);
        
        QUuid node1 = nodeService.createNode(Vector3(0, 0, 0));
        QUuid node2 = nodeService.createNode(Vector3(1, 1, 1));
        QUuid barId = barService.createBar(node1, node2);
        
        QVERIFY(barService.barExists(barId));
        QVERIFY(barService.deleteBar(barId));
        QVERIFY(!barService.barExists(barId));
    }
    
    void testAssignProperties()
    {
        InMemoryModelRepository repo;
        NodeService nodeService(&repo);
        BarService barService(&repo);
        
        QUuid node1 = nodeService.createNode(Vector3(0, 0, 0));
        QUuid node2 = nodeService.createNode(Vector3(1, 1, 1));
        QUuid barId = barService.createBar(node1, node2);
        
        QUuid matId = QUuid::createUuid();
        QUuid secId = QUuid::createUuid();
        
        QVERIFY(barService.assignProperties(barId, matId, secId));
        
        auto bar = barService.findBar(barId);
        QCOMPARE(bar->materialId(), matId);
        QCOMPARE(bar->sectionId(), secId);
    }
    
    void testCalculateBarLength()
    {
        InMemoryModelRepository repo;
        NodeService nodeService(&repo);
        BarService barService(&repo);
        
        QUuid node1 = nodeService.createNode(Vector3(0, 0, 0));
        QUuid node2 = nodeService.createNode(Vector3(3, 4, 0));
        QUuid barId = barService.createBar(node1, node2);
        
        double length = barService.calculateBarLength(barId);
        QCOMPARE(length, 5.0);
    }
    
    void testFindBarsConnectedToNode()
    {
        InMemoryModelRepository repo;
        NodeService nodeService(&repo);
        BarService barService(&repo);
        
        QUuid node1 = nodeService.createNode(Vector3(0, 0, 0));
        QUuid node2 = nodeService.createNode(Vector3(1, 1, 1));
        QUuid node3 = nodeService.createNode(Vector3(2, 2, 2));
        
        QUuid bar1 = barService.createBar(node1, node2);
        QUuid bar2 = barService.createBar(node1, node3);
        
        auto connected = barService.findBarsConnectedToNode(node1);
        QCOMPARE(connected.size(), static_cast<size_t>(2));
    }
    
    void testSetKPoint()
    {
        InMemoryModelRepository repo;
        NodeService nodeService(&repo);
        BarService barService(&repo);
        
        QUuid node1 = nodeService.createNode(Vector3(0, 0, 0));
        QUuid node2 = nodeService.createNode(Vector3(1, 0, 0));
        QUuid barId = barService.createBar(node1, node2);
        
        Vector3 kp(0, 1, 0);
        QVERIFY(barService.setKPoint(barId, kp));
        
        auto bar = barService.findBar(barId);
        QVERIFY(bar->hasKPoint());
        QVERIFY(bar->isLCSDirty());
    }
};

// Main function to run all tests
QTEST_MAIN(TestInMemoryModelRepository)
#include "TestRepositoryAndServices.moc"
