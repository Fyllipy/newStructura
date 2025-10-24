#include <QtTest/QtTest>
#include "../core/model/Vector3.h"
#include "../core/model/ModelEntities.h"

using namespace Structura::Model;

/**
 * @brief Unit tests for Vector3 class
 */
class TestVector3 : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultConstructor()
    {
        Vector3 v;
        QCOMPARE(v.x(), 0.0);
        QCOMPARE(v.y(), 0.0);
        QCOMPARE(v.z(), 0.0);
    }

    void testParameterizedConstructor()
    {
        Vector3 v(1.0, 2.0, 3.0);
        QCOMPARE(v.x(), 1.0);
        QCOMPARE(v.y(), 2.0);
        QCOMPARE(v.z(), 3.0);
    }

    void testIndexOperator()
    {
        Vector3 v(1.0, 2.0, 3.0);
        QCOMPARE(v[0], 1.0);
        QCOMPARE(v[1], 2.0);
        QCOMPARE(v[2], 3.0);
    }

    void testSetters()
    {
        Vector3 v;
        v.setX(5.0);
        v.setY(6.0);
        v.setZ(7.0);
        QCOMPARE(v.x(), 5.0);
        QCOMPARE(v.y(), 6.0);
        QCOMPARE(v.z(), 7.0);
    }

    void testLength()
    {
        Vector3 v(3.0, 4.0, 0.0);
        QCOMPARE(v.length(), 5.0);
    }

    void testLengthSquared()
    {
        Vector3 v(3.0, 4.0, 0.0);
        QCOMPARE(v.lengthSquared(), 25.0);
    }

    void testDistance()
    {
        Vector3 v1(0.0, 0.0, 0.0);
        Vector3 v2(3.0, 4.0, 0.0);
        QCOMPARE(v1.distanceTo(v2), 5.0);
    }

    void testNormalize()
    {
        Vector3 v(3.0, 4.0, 0.0);
        QVERIFY(v.normalize());
        QCOMPARE(v.x(), 0.6);
        QCOMPARE(v.y(), 0.8);
        QCOMPARE(v.z(), 0.0);
        QVERIFY(qAbs(v.length() - 1.0) < 1e-10);
    }

    void testDotProduct()
    {
        Vector3 v1(1.0, 2.0, 3.0);
        Vector3 v2(4.0, 5.0, 6.0);
        // 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
        QCOMPARE(v1.dot(v2), 32.0);
    }

    void testCrossProduct()
    {
        Vector3 v1(1.0, 0.0, 0.0);
        Vector3 v2(0.0, 1.0, 0.0);
        Vector3 result = v1.cross(v2);
        QCOMPARE(result.x(), 0.0);
        QCOMPARE(result.y(), 0.0);
        QCOMPARE(result.z(), 1.0);
    }

    void testAddition()
    {
        Vector3 v1(1.0, 2.0, 3.0);
        Vector3 v2(4.0, 5.0, 6.0);
        Vector3 result = v1 + v2;
        QCOMPARE(result.x(), 5.0);
        QCOMPARE(result.y(), 7.0);
        QCOMPARE(result.z(), 9.0);
    }

    void testSubtraction()
    {
        Vector3 v1(4.0, 5.0, 6.0);
        Vector3 v2(1.0, 2.0, 3.0);
        Vector3 result = v1 - v2;
        QCOMPARE(result.x(), 3.0);
        QCOMPARE(result.y(), 3.0);
        QCOMPARE(result.z(), 3.0);
    }

    void testScalarMultiplication()
    {
        Vector3 v(1.0, 2.0, 3.0);
        Vector3 result = v * 2.0;
        QCOMPARE(result.x(), 2.0);
        QCOMPARE(result.y(), 4.0);
        QCOMPARE(result.z(), 6.0);
    }

    void testEquality()
    {
        Vector3 v1(1.0, 2.0, 3.0);
        Vector3 v2(1.0, 2.0, 3.0);
        Vector3 v3(1.0, 2.0, 4.0);
        QVERIFY(v1 == v2);
        QVERIFY(v1 != v3);
    }
};

/**
 * @brief Unit tests for Node class
 */
class TestNode : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultConstructor()
    {
        Node node;
        QVERIFY(!node.id().isNull());
        QCOMPARE(node.externalId(), 0);
        QCOMPARE(node.x(), 0.0);
        QCOMPARE(node.y(), 0.0);
        QCOMPARE(node.z(), 0.0);
        QVERIFY(!node.isSelected());
        QVERIFY(!node.hasRestraints());
    }

    void testParameterizedConstructor()
    {
        QUuid id = QUuid::createUuid();
        Node node(id, 42, 1.0, 2.0, 3.0);
        QCOMPARE(node.id(), id);
        QCOMPARE(node.externalId(), 42);
        QCOMPARE(node.x(), 1.0);
        QCOMPARE(node.y(), 2.0);
        QCOMPARE(node.z(), 3.0);
    }

    void testSetPosition()
    {
        Node node;
        node.setPosition(10.0, 20.0, 30.0);
        QCOMPARE(node.x(), 10.0);
        QCOMPARE(node.y(), 20.0);
        QCOMPARE(node.z(), 30.0);
    }

    void testMoveTo()
    {
        Node node;
        Vector3 newPos(5.0, 6.0, 7.0);
        node.moveTo(newPos);
        QCOMPARE(node.position(), newPos);
    }

    void testDistanceTo()
    {
        Node node1(QUuid::createUuid(), 1, 0.0, 0.0, 0.0);
        Node node2(QUuid::createUuid(), 2, 3.0, 4.0, 0.0);
        QCOMPARE(node1.distanceTo(node2), 5.0);
    }

    void testRestraints()
    {
        Node node;
        QVERIFY(!node.hasRestraints());
        
        node.setRestraint(0, true);  // UX
        node.setRestraint(2, true);  // UZ
        
        QVERIFY(node.hasRestraints());
        auto restraints = node.restraints();
        QVERIFY(restraints[0]);
        QVERIFY(!restraints[1]);
        QVERIFY(restraints[2]);
        
        node.clearRestraints();
        QVERIFY(!node.hasRestraints());
    }

    void testSelection()
    {
        Node node;
        QVERIFY(!node.isSelected());
        node.setSelected(true);
        QVERIFY(node.isSelected());
    }
};

/**
 * @brief Unit tests for Bar class
 */
class TestBar : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultConstructor()
    {
        Bar bar;
        QVERIFY(!bar.id().isNull());
        QCOMPARE(bar.externalId(), 0);
        QVERIFY(!bar.isSelected());
        QVERIFY(!bar.hasKPoint());
        QVERIFY(bar.isLCSDirty());
    }

    void testParameterizedConstructor()
    {
        QUuid id = QUuid::createUuid();
        QUuid startId = QUuid::createUuid();
        QUuid endId = QUuid::createUuid();
        QUuid matId = QUuid::createUuid();
        QUuid secId = QUuid::createUuid();
        
        Bar bar(id, startId, endId, matId, secId);
        QCOMPARE(bar.id(), id);
        QCOMPARE(bar.startNodeId(), startId);
        QCOMPARE(bar.endNodeId(), endId);
        QCOMPARE(bar.materialId(), matId);
        QCOMPARE(bar.sectionId(), secId);
    }

    void testKPoint()
    {
        Bar bar;
        QVERIFY(!bar.hasKPoint());
        
        Vector3 kp(1.0, 2.0, 3.0);
        bar.setKPoint(kp);
        
        QVERIFY(bar.hasKPoint());
        QVERIFY(bar.isLCSDirty());
        QCOMPARE(bar.kPoint()->x(), 1.0);
        QCOMPARE(bar.kPoint()->y(), 2.0);
        QCOMPARE(bar.kPoint()->z(), 3.0);
        
        bar.clearKPoint();
        QVERIFY(!bar.hasKPoint());
    }

    void testCalculateLength()
    {
        Vector3 start(0.0, 0.0, 0.0);
        Vector3 end(3.0, 4.0, 0.0);
        double length = Bar::calculateLength(start, end);
        QCOMPARE(length, 5.0);
    }

    void testSetNodeIds()
    {
        Bar bar;
        QUuid newStart = QUuid::createUuid();
        QUuid newEnd = QUuid::createUuid();
        
        bar.setStartNodeId(newStart);
        bar.setEndNodeId(newEnd);
        
        QCOMPARE(bar.startNodeId(), newStart);
        QCOMPARE(bar.endNodeId(), newEnd);
    }
};

/**
 * @brief Unit tests for Material class
 */
class TestMaterial : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultConstructor()
    {
        Material mat;
        QVERIFY(!mat.id().isNull());
        QCOMPARE(mat.externalId(), 0);
        QVERIFY(mat.name().isEmpty());
        QCOMPARE(mat.youngModulus(), 0.0);
        QCOMPARE(mat.shearModulus(), 0.0);
        QVERIFY(!mat.isValid());
    }

    void testParameterizedConstructor()
    {
        QUuid id = QUuid::createUuid();
        Material mat(id, 1, "Steel", 200000.0, 80000.0);
        
        QCOMPARE(mat.id(), id);
        QCOMPARE(mat.externalId(), 1);
        QCOMPARE(mat.name(), QString("Steel"));
        QCOMPARE(mat.youngModulus(), 200000.0);
        QCOMPARE(mat.shearModulus(), 80000.0);
        QVERIFY(mat.isValid());
    }

    void testSetters()
    {
        Material mat;
        mat.setName("Concrete");
        mat.setYoungModulus(30000.0);
        mat.setShearModulus(12000.0);
        
        QCOMPARE(mat.name(), QString("Concrete"));
        QCOMPARE(mat.youngModulus(), 30000.0);
        QCOMPARE(mat.shearModulus(), 12000.0);
        QVERIFY(mat.isValid());
    }

    void testValidation()
    {
        Material mat(QUuid::createUuid(), 1, "Test", -100.0, 80000.0);
        QVERIFY(!mat.isValid());  // negative Young's modulus
        
        mat.setYoungModulus(200000.0);
        QVERIFY(mat.isValid());
    }
};

/**
 * @brief Unit tests for Section class
 */
class TestSection : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultConstructor()
    {
        Section sec;
        QVERIFY(!sec.id().isNull());
        QCOMPARE(sec.externalId(), 0);
        QVERIFY(sec.name().isEmpty());
        QVERIFY(!sec.isValid());
    }

    void testParameterizedConstructor()
    {
        QUuid id = QUuid::createUuid();
        Section sec(id, 1, "IPE300", 5381.0, 8356.0, 603.8, 20.12);
        
        QCOMPARE(sec.id(), id);
        QCOMPARE(sec.externalId(), 1);
        QCOMPARE(sec.name(), QString("IPE300"));
        QCOMPARE(sec.area(), 5381.0);
        QCOMPARE(sec.iz(), 8356.0);
        QCOMPARE(sec.iy(), 603.8);
        QCOMPARE(sec.torsionalConstant(), 20.12);
        QVERIFY(sec.isValid());
    }

    void testSetters()
    {
        Section sec;
        sec.setName("Custom");
        sec.setArea(100.0);
        sec.setIz(1000.0);
        sec.setIy(500.0);
        sec.setTorsionalConstant(50.0);
        
        QCOMPARE(sec.area(), 100.0);
        QCOMPARE(sec.iz(), 1000.0);
        QCOMPARE(sec.iy(), 500.0);
        QCOMPARE(sec.torsionalConstant(), 50.0);
        QVERIFY(sec.isValid());
    }
};

/**
 * @brief Unit tests for GridLine class
 */
class TestGridLine : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultConstructor()
    {
        GridLine line;
        QVERIFY(!line.id().isNull());
        QCOMPARE(line.axis(), GridLine::Axis::X);
        QCOMPARE(line.offset(), 0.0);
        QCOMPARE(line.index(), 0);
        QVERIFY(!line.isHighlighted());
        QVERIFY(!line.isGhost());
    }

    void testParameterizedConstructor()
    {
        QUuid id = QUuid::createUuid();
        GridLine line(id, GridLine::Axis::Y, 5.0, 2, 0.0, 10.0);
        
        QCOMPARE(line.id(), id);
        QCOMPARE(line.axis(), GridLine::Axis::Y);
        QCOMPARE(line.offset(), 5.0);
        QCOMPARE(line.index(), 2);
        QCOMPARE(line.coordinate1(), 0.0);
        QCOMPARE(line.coordinate2(), 10.0);
    }

    void testSetEndpoints()
    {
        GridLine line;
        line.setEndpoints(1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
        
        QCOMPARE(line.startPoint().x(), 1.0);
        QCOMPARE(line.startPoint().y(), 2.0);
        QCOMPARE(line.startPoint().z(), 3.0);
        QCOMPARE(line.endPoint().x(), 4.0);
        QCOMPARE(line.endPoint().y(), 5.0);
        QCOMPARE(line.endPoint().z(), 6.0);
    }

    void testHighlightAndGhost()
    {
        GridLine line;
        line.setHighlighted(true);
        line.setGhost(true);
        
        QVERIFY(line.isHighlighted());
        QVERIFY(line.isGhost());
    }
};

// Main function to run all tests
QTEST_MAIN(TestVector3)
#include "TestCoreModels.moc"
