#include "PropertiesPanelEntryBuilder.h"

#include "app/LookupUtils.h"

#include "viz/SceneController.h"

#include <QHash>
#include <QVector3D>

namespace Structura::UI {

namespace {

QHash<int, PropertiesPanelEntryBuilder::NodeSupport> buildSupportsByNodeId(
    const QVector<PropertiesPanelEntryBuilder::NodeSupport> &supports)
{
    QHash<int, PropertiesPanelEntryBuilder::NodeSupport> map;
    map.reserve(supports.size());
    for (const auto &support : supports) {
        if (!map.contains(support.nodeId)) {
            map.insert(support.nodeId, support);
        }
    }
    return map;
}

} // namespace

QVector<PropertiesPanel::NodeEntry> PropertiesPanelEntryBuilder::buildNodeEntries(const SceneController *sceneController,
                                                                                   const QSet<QUuid> &nodeIds,
                                                                                   const QVector<NodeSupport> &supports,
                                                                                   const QVector<NodalLoad> &nodalLoads)
{
    QVector<PropertiesPanel::NodeEntry> entries;
    if (!sceneController) {
        return entries;
    }

    const QHash<int, NodeSupport> supportsByNodeId = buildSupportsByNodeId(supports);
    const QHash<int, int> nodalLoadsByNodeId = Structura::App::buildCounts<int>(
        nodalLoads,
        [](const NodalLoad &load) { return load.nodeId; });

    for (const QUuid &id : nodeIds) {
        const SceneController::Node *node = sceneController->findNode(id);
        if (!node) {
            continue;
        }

        PropertiesPanel::NodeEntry entry;
        entry.id = id;
        entry.externalId = node->externalId();
        const auto pos = node->position();
        entry.x = pos[0];
        entry.y = pos[1];
        entry.z = pos[2];
        entry.restraints.fill(false);

        const auto supportIt = supportsByNodeId.constFind(entry.externalId);
        if (supportIt != supportsByNodeId.constEnd()) {
            for (int i = 0; i < 6; ++i) {
                entry.restraints[static_cast<std::size_t>(i)] = supportIt->restraints[i];
            }
        }

        entry.loadCount = nodalLoadsByNodeId.value(entry.externalId, 0);

        entries.append(entry);
    }

    return entries;
}

QVector<PropertiesPanel::BarEntry> PropertiesPanelEntryBuilder::buildBarEntries(const SceneController *sceneController,
                                                                                 const QSet<QUuid> &barIds,
                                                                                 const QVector<MaterialInfo> &materials,
                                                                                 const QVector<SectionInfo> &sections,
                                                                                 const QVector<MemberLoad> &memberLoads,
                                                                                 const QString &noMaterialLabel,
                                                                                 const QString &noSectionLabel)
{
    QVector<PropertiesPanel::BarEntry> entries;
    if (!sceneController) {
        return entries;
    }

    const QHash<QUuid, MaterialInfo> materialById = Structura::App::buildLookup<QUuid, MaterialInfo>(
        materials,
        [](const MaterialInfo &mat) { return mat.uuid; });

    const QHash<QUuid, SectionInfo> sectionById = Structura::App::buildLookup<QUuid, SectionInfo>(
        sections,
        [](const SectionInfo &sec) { return sec.uuid; });

    const auto nodeInfos = sceneController->nodeInfos();
    const QHash<QUuid, SceneController::NodeInfo> nodeMap =
        Structura::App::buildLookup<QUuid, SceneController::NodeInfo>(
            nodeInfos,
            [](const SceneController::NodeInfo &info) { return info.id; });

    const QHash<int, int> memberLoadsByBarExternalId = Structura::App::buildCounts<int>(
        memberLoads,
        [](const MemberLoad &load) { return load.memberId; });

    for (const QUuid &id : barIds) {
        const SceneController::Bar *bar = sceneController->findBar(id);
        if (!bar) {
            continue;
        }

        PropertiesPanel::BarEntry entry;
        entry.id = id;
        entry.externalId = bar->externalId();
        entry.materialId = bar->materialId();
        entry.sectionId = bar->sectionId();

        const auto materialInfo = materialById.constFind(entry.materialId);
        entry.materialName = materialInfo != materialById.constEnd() ? materialInfo->name : noMaterialLabel;
        const auto sectionInfo = sectionById.constFind(entry.sectionId);
        entry.sectionName = sectionInfo != sectionById.constEnd() ? sectionInfo->name : noSectionLabel;

        QVector3D startPos;
        QVector3D endPos;
        bool hasStart = false;
        bool hasEnd = false;

        auto startIt = nodeMap.constFind(bar->startNodeId());
        if (startIt != nodeMap.constEnd()) {
            entry.nodeI = startIt.value().externalId;
            startPos = QVector3D(startIt.value().x, startIt.value().y, startIt.value().z);
            hasStart = true;
        }

        auto endIt = nodeMap.constFind(bar->endNodeId());
        if (endIt != nodeMap.constEnd()) {
            entry.nodeJ = endIt.value().externalId;
            endPos = QVector3D(endIt.value().x, endIt.value().y, endIt.value().z);
            hasEnd = true;
        }

        entry.length = (hasStart && hasEnd) ? (startPos - endPos).length() : 0.0;

        entry.distributedLoadCount = memberLoadsByBarExternalId.value(entry.externalId, 0);

        entries.append(entry);
    }

    return entries;
}

} // namespace Structura::UI
