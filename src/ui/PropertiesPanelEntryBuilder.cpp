#include "PropertiesPanelEntryBuilder.h"

#include "viz/SceneController.h"

#include <QHash>
#include <QVector3D>

namespace Structura::UI {

QVector<PropertiesPanel::NodeEntry> PropertiesPanelEntryBuilder::buildNodeEntries(const SceneController *sceneController,
                                                                                   const QSet<QUuid> &nodeIds,
                                                                                   const QVector<NodeSupport> &supports,
                                                                                   const QVector<NodalLoad> &nodalLoads)
{
    QVector<PropertiesPanel::NodeEntry> entries;
    if (!sceneController) {
        return entries;
    }

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

        for (const auto &support : supports) {
            if (support.nodeId == entry.externalId) {
                for (int i = 0; i < 6; ++i) {
                    entry.restraints[static_cast<std::size_t>(i)] = support.restraints[i];
                }
                break;
            }
        }

        int loadCount = 0;
        for (const auto &load : nodalLoads) {
            if (load.nodeId == entry.externalId) {
                ++loadCount;
            }
        }
        entry.loadCount = loadCount;

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

    QHash<QUuid, MaterialInfo> materialById;
    materialById.reserve(materials.size());
    for (const auto &mat : materials) {
        materialById.insert(mat.uuid, mat);
    }

    QHash<QUuid, SectionInfo> sectionById;
    sectionById.reserve(sections.size());
    for (const auto &sec : sections) {
        sectionById.insert(sec.uuid, sec);
    }

    const auto nodeInfos = sceneController->nodeInfos();
    QHash<QUuid, SceneController::NodeInfo> nodeMap;
    nodeMap.reserve(static_cast<int>(nodeInfos.size()));
    for (const auto &info : nodeInfos) {
        nodeMap.insert(info.id, info);
    }

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

        int distributedLoads = 0;
        for (const auto &load : memberLoads) {
            if (load.memberId == entry.externalId) {
                ++distributedLoads;
            }
        }
        entry.distributedLoadCount = distributedLoads;

        entries.append(entry);
    }

    return entries;
}

} // namespace Structura::UI
