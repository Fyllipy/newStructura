#include <QtTest/QtTest>
#include "../app/InMemoryModelRepository.h"
#include "../app/NodeService.h"
#include "../app/BarService.h"

using namespace Structura::App;
using namespace Structura::Model;

namespace {

struct NodeServiceFixture {
    InMemoryModelRepository repo;
    NodeService service;

    NodeServiceFixture()
        : service(&repo)
    {
    }
};

struct BarServiceFixture {
    InMemoryModelRepository repo;
    NodeService nodeService;
    BarService barService;

    BarServiceFixture()
        : nodeService(&repo)
        , barService(&repo)
    {
    }

    QUuid createNode(double x, double y, double z)
    {
        return nodeService.createNode(Vector3(x, y, z));
    }

    QUuid createBarBetween(const Vector3 &a, const Vector3 &b)
    {
        const QUuid node1 = createNode(a.x(), a.y(), a.z());
        const QUuid node2 = createNode(b.x(), b.y(), b.z());
        return barService.createBar(node1, node2);
    }
};

} // namespace

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
        NodeServiceFixture fixture;
        
        Vector3 pos(1.0, 2.0, 3.0);
        QUuid id = fixture.service.createNode(pos);
        
        QVERIFY(!id.isNull());
        QCOMPARE(fixture.repo.nodeCount(), static_cast<size_t>(1));
        
        auto node = fixture.service.findNode(id);
        QVERIFY(node.has_value());
        QCOMPARE(node->x(), 1.0);
        QCOMPARE(node->y(), 2.0);
        QCOMPARE(node->z(), 3.0);
        QCOMPARE(node->externalId(), 1);
    }
    
    void testCreateMultipleNodes()
    {
        NodeServiceFixture fixture;
        
        QUuid id1 = fixture.service.createNode(Vector3(0, 0, 0));
        QUuid id2 = fixture.service.createNode(Vector3(1, 1, 1));
        
        QCOMPARE(fixture.service.nodeCount(), static_cast<size_t>(2));
        
        auto node1 = fixture.service.findNode(id1);
        auto node2 = fixture.service.findNode(id2);
        
        QCOMPARE(node1->externalId(), 1);
        QCOMPARE(node2->externalId(), 2);
    }
    
    void testDeleteNode()
    {
        NodeServiceFixture fixture;
        
        QUuid id = fixture.service.createNode(Vector3(0, 0, 0));
        QVERIFY(fixture.service.nodeExists(id));
        
        QVERIFY(fixture.service.deleteNode(id));
        QVERIFY(!fixture.service.nodeExists(id));
        QCOMPARE(fixture.service.nodeCount(), static_cast<size_t>(0));
    }
    
    void testSetNodePosition()
    {
        NodeServiceFixture fixture;
        
        QUuid id = fixture.service.createNode(Vector3(0, 0, 0));
        Vector3 newPos(10, 20, 30);
        
        QVERIFY(fixture.service.setNodePosition(id, newPos));
        
        auto node = fixture.service.findNode(id);
        QCOMPARE(node->x(), 10.0);
        QCOMPARE(node->y(), 20.0);
        QCOMPARE(node->z(), 30.0);
    }
    
    void testSetNodeRestraints()
    {
        NodeServiceFixture fixture;
        
        QUuid id = fixture.service.createNode(Vector3(0, 0, 0));
        std::array<bool, 6> restraints = {true, true, false, false, false, false};
        
        QVERIFY(fixture.service.setNodeRestraints(id, restraints));
        
        auto node = fixture.service.findNode(id);
        auto nodeRestraints = node->restraints();
        QVERIFY(nodeRestraints[0]);
        QVERIFY(nodeRestraints[1]);
        QVERIFY(!nodeRestraints[2]);
    }
    
    void testNextExternalId()
    {
        NodeServiceFixture fixture;
        
        QCOMPARE(fixture.service.nextExternalId(), 1);
        
        fixture.service.createNode(Vector3(0, 0, 0));
        QCOMPARE(fixture.service.nextExternalId(), 2);
        
        fixture.service.createNode(Vector3(1, 1, 1));
        QCOMPARE(fixture.service.nextExternalId(), 3);
    }

    void testCreateNodeWithExternalId()
    {
        NodeServiceFixture fixture;

        QUuid id = fixture.service.createNodeWithExternalId(Vector3(1.0, 2.0, 3.0), 42);
        QVERIFY(!id.isNull());

        auto node = fixture.service.findNode(id);
        QVERIFY(node.has_value());
        QCOMPARE(node->externalId(), 42);
    }

    void testCreateNodeWithExternalIdConflict()
    {
        NodeServiceFixture fixture;

        QUuid firstId = fixture.service.createNodeWithExternalId(Vector3(0.0, 0.0, 0.0), 7);
        QVERIFY(!firstId.isNull());

        QUuid conflictId = fixture.service.createNodeWithExternalId(Vector3(1.0, 1.0, 1.0), 7);
        QVERIFY(conflictId.isNull());
        QCOMPARE(fixture.service.nodeCount(), static_cast<size_t>(1));
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
        BarServiceFixture fixture;
        
        QUuid node1 = fixture.createNode(0, 0, 0);
        QUuid node2 = fixture.createNode(3, 4, 0);
        
        QUuid barId = fixture.barService.createBar(node1, node2);
        
        QVERIFY(!barId.isNull());
        QCOMPARE(fixture.barService.barCount(), static_cast<size_t>(1));
        
        auto bar = fixture.barService.findBar(barId);
        QVERIFY(bar.has_value());
        QCOMPARE(bar->startNodeId(), node1);
        QCOMPARE(bar->endNodeId(), node2);
        QCOMPARE(bar->externalId(), 1);
    }
    
    void testCreateBarWithInvalidNodes()
    {
        BarServiceFixture fixture;
        
        QUuid fakeId1 = QUuid::createUuid();
        QUuid fakeId2 = QUuid::createUuid();
        
        QUuid barId = fixture.barService.createBar(fakeId1, fakeId2);
        QVERIFY(barId.isNull());
    }
    
    void testCreateBarSameNode()
    {
        BarServiceFixture fixture;
        
        QUuid node1 = fixture.createNode(0, 0, 0);
        
        QUuid barId = fixture.barService.createBar(node1, node1);
        QVERIFY(barId.isNull());
    }
    
    void testDeleteBar()
    {
        BarServiceFixture fixture;
        
        QUuid node1 = fixture.createNode(0, 0, 0);
        QUuid node2 = fixture.createNode(1, 1, 1);
        QUuid barId = fixture.barService.createBar(node1, node2);
        
        QVERIFY(fixture.barService.barExists(barId));
        QVERIFY(fixture.barService.deleteBar(barId));
        QVERIFY(!fixture.barService.barExists(barId));
    }
    
    void testAssignProperties()
    {
        BarServiceFixture fixture;
        
        QUuid barId = fixture.createBarBetween(Vector3(0, 0, 0), Vector3(1, 1, 1));
        
        QUuid matId = QUuid::createUuid();
        QUuid secId = QUuid::createUuid();
        
        QVERIFY(fixture.barService.assignProperties(barId, matId, secId));
        
        auto bar = fixture.barService.findBar(barId);
        QCOMPARE(bar->materialId(), matId);
        QCOMPARE(bar->sectionId(), secId);
    }
    
    void testCalculateBarLength()
    {
        BarServiceFixture fixture;
        
        QUuid barId = fixture.createBarBetween(Vector3(0, 0, 0), Vector3(3, 4, 0));
        
        double length = fixture.barService.calculateBarLength(barId);
        QCOMPARE(length, 5.0);
    }
    
    void testFindBarsConnectedToNode()
    {
        BarServiceFixture fixture;
        
        QUuid node1 = fixture.createNode(0, 0, 0);
        QUuid node2 = fixture.createNode(1, 1, 1);
        QUuid node3 = fixture.createNode(2, 2, 2);
        
        QUuid bar1 = fixture.barService.createBar(node1, node2);
        QUuid bar2 = fixture.barService.createBar(node1, node3);
        Q_UNUSED(bar1);
        Q_UNUSED(bar2);
        
        auto connected = fixture.barService.findBarsConnectedToNode(node1);
        QCOMPARE(connected.size(), static_cast<size_t>(2));
    }
    
    void testSetKPoint()
    {
        BarServiceFixture fixture;
        
        QUuid barId = fixture.createBarBetween(Vector3(0, 0, 0), Vector3(1, 0, 0));
        
        Vector3 kp(0, 1, 0);
        QVERIFY(fixture.barService.setKPoint(barId, kp));
        
        auto bar = fixture.barService.findBar(barId);
        QVERIFY(bar->hasKPoint());
        QVERIFY(bar->isLCSDirty());
    }

    void testAssignMaterialPreservesSection()
    {
        BarServiceFixture fixture;
        QUuid barId = fixture.createBarBetween(Vector3(0, 0, 0), Vector3(1, 0, 0));

        QUuid originalSection = QUuid::createUuid();
        QUuid originalMaterial = QUuid::createUuid();
        QVERIFY(fixture.barService.assignProperties(barId, originalMaterial, originalSection));

        QUuid newMaterial = QUuid::createUuid();
        QVERIFY(fixture.barService.assignMaterial(barId, newMaterial));

        auto bar = fixture.barService.findBar(barId);
        QVERIFY(bar.has_value());
        QCOMPARE(bar->materialId(), newMaterial);
        QCOMPARE(bar->sectionId(), originalSection);
    }

    void testAssignSectionPreservesMaterial()
    {
        BarServiceFixture fixture;
        QUuid barId = fixture.createBarBetween(Vector3(0, 0, 0), Vector3(1, 0, 0));

        QUuid originalMaterial = QUuid::createUuid();
        QUuid originalSection = QUuid::createUuid();
        QVERIFY(fixture.barService.assignProperties(barId, originalMaterial, originalSection));

        QUuid newSection = QUuid::createUuid();
        QVERIFY(fixture.barService.assignSection(barId, newSection));

        auto bar = fixture.barService.findBar(barId);
        QVERIFY(bar.has_value());
        QCOMPARE(bar->materialId(), originalMaterial);
        QCOMPARE(bar->sectionId(), newSection);
    }
};

int main(int argc, char *argv[])
{
    int status = 0;

    TestInMemoryModelRepository repositoryTests;
    status |= QTest::qExec(&repositoryTests, argc, argv);

    TestNodeService nodeServiceTests;
    status |= QTest::qExec(&nodeServiceTests, argc, argv);

    TestBarService barServiceTests;
    status |= QTest::qExec(&barServiceTests, argc, argv);

    return status;
}

#include "TestRepositoryAndServices.moc"
