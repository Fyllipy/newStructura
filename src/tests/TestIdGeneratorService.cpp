#include <QtTest/QtTest>

#include "app/ISceneModelGateway.h"
#include "app/services/IdGeneratorService.h"

using namespace Structura::App;
using namespace Structura::App::Services;

namespace {

class FakeSceneModelGateway : public ISceneModelGateway
{
public:
    std::vector<SceneNodeInfo> nodeInfos() const override
    {
        return {};
    }

    std::vector<SceneBarInfo> bars() const override
    {
        return m_bars;
    }

    QUuid addPointWithId(double, double, double, int) override
    {
        return QUuid::createUuid();
    }

    Structura::Model::Node *findNode(const QUuid &) override
    {
        return nullptr;
    }

    QUuid addBar(const QUuid &startNodeId,
                 const QUuid &endNodeId,
                 const QUuid &materialId,
                 const QUuid &sectionId) override
    {
        SceneBarInfo bar;
        bar.id = QUuid::createUuid();
        bar.startNodeId = startNodeId;
        bar.endNodeId = endNodeId;
        bar.materialId = materialId;
        bar.sectionId = sectionId;
        bar.externalId = 0;
        m_bars.push_back(bar);
        return bar.id;
    }

    void setBarExternalId(const QUuid &barId, int externalId) override
    {
        for (auto &bar : m_bars) {
            if (bar.id == barId) {
                bar.externalId = externalId;
                return;
            }
        }
    }

    void setBars(const std::vector<SceneBarInfo> &bars)
    {
        m_bars = bars;
    }

private:
    std::vector<SceneBarInfo> m_bars;
};

} // namespace

class TestIdGeneratorService : public QObject
{
    Q_OBJECT

private slots:
    void testNextMaterialExternalIdUsesMaxPlusOne()
    {
        QVector<IdGeneratorService::MaterialInfo> materials;
        IdGeneratorService::MaterialInfo a;
        a.externalId = 10;
        IdGeneratorService::MaterialInfo b;
        b.externalId = 3;
        materials.append(a);
        materials.append(b);

        QCOMPARE(IdGeneratorService::nextMaterialExternalId(materials), 11);
    }

    void testEnsureMaterialExternalIdsOnlyFillsMissing()
    {
        QVector<IdGeneratorService::MaterialInfo> materials;
        IdGeneratorService::MaterialInfo a;
        a.externalId = 5;
        IdGeneratorService::MaterialInfo b;
        b.externalId = 0;
        IdGeneratorService::MaterialInfo c;
        c.externalId = -1;
        materials.append(a);
        materials.append(b);
        materials.append(c);

        IdGeneratorService::ensureMaterialExternalIds(materials);

        QCOMPARE(materials[0].externalId, 5);
        QCOMPARE(materials[1].externalId, 6);
        QCOMPARE(materials[2].externalId, 7);
    }

    void testEnsureSectionExternalIdsOnlyFillsMissing()
    {
        QVector<IdGeneratorService::SectionInfo> sections;
        IdGeneratorService::SectionInfo a;
        a.externalId = 8;
        IdGeneratorService::SectionInfo b;
        b.externalId = 0;
        sections.append(a);
        sections.append(b);

        IdGeneratorService::ensureSectionExternalIds(sections);

        QCOMPARE(sections[0].externalId, 8);
        QCOMPARE(sections[1].externalId, 9);
    }

    void testEnsureBarExternalIdsOnlyFillsMissing()
    {
        FakeSceneModelGateway gateway;

        SceneBarInfo barA;
        barA.id = QUuid::createUuid();
        barA.externalId = 4;

        SceneBarInfo barB;
        barB.id = QUuid::createUuid();
        barB.externalId = 0;

        SceneBarInfo barC;
        barC.id = QUuid::createUuid();
        barC.externalId = -10;

        gateway.setBars({barA, barB, barC});

        IdGeneratorService::ensureBarExternalIds(&gateway);

        const auto bars = gateway.bars();
        QCOMPARE(bars[0].externalId, 4);
        QCOMPARE(bars[1].externalId, 5);
        QCOMPARE(bars[2].externalId, 6);
    }
};

QTEST_APPLESS_MAIN(TestIdGeneratorService)
#include "TestIdGeneratorService.moc"
