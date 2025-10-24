#include "MainWindowPresenter.h"

#include "../SceneController.h"
#include "../SelectionModel.h"
#include "../app/UndoRedoService.h"

#include <QtGlobal>

namespace Structura::UI {

MainWindowPresenter::MainWindowPresenter(const Dependencies &deps, QObject *parent)
    : QObject(parent)
    , m_sceneController(deps.sceneController)
    , m_selectionModel(deps.selectionModel)
    , m_undoService(deps.undoService)
    , m_materials(deps.materials)
    , m_sections(deps.sections)
    , m_lastMaterialId(deps.lastMaterialId)
    , m_lastSectionId(deps.lastSectionId)
    , m_supports(deps.supports)
    , m_nodalLoads(deps.nodalLoads)
    , m_memberLoads(deps.memberLoads)
    , m_lastNodalPreset(deps.lastNodalPreset)
    , m_lastDistributedPreset(deps.lastDistributedPreset)
{
}

MainWindowPresenter::MaterialInfo *MainWindowPresenter::findMaterial(const QUuid &id)
{
    if (id.isNull()) {
        return nullptr;
    }
    if (!m_materials) {
        return nullptr;
    }
    for (auto &mat : *m_materials) {
        if (mat.uuid == id) {
            return &mat;
        }
    }
    return nullptr;
}

const MainWindowPresenter::MaterialInfo *MainWindowPresenter::findMaterial(const QUuid &id) const
{
    return const_cast<MainWindowPresenter *>(this)->findMaterial(id);
}

MainWindowPresenter::SectionInfo *MainWindowPresenter::findSection(const QUuid &id)
{
    if (id.isNull()) {
        return nullptr;
    }
    if (!m_sections) {
        return nullptr;
    }
    for (auto &sec : *m_sections) {
        if (sec.uuid == id) {
            return &sec;
        }
    }
    return nullptr;
}

const MainWindowPresenter::SectionInfo *MainWindowPresenter::findSection(const QUuid &id) const
{
    return const_cast<MainWindowPresenter *>(this)->findSection(id);
}

void MainWindowPresenter::handleNodeCoordinateEdited(const QVector<QUuid> &ids, char axis, double value)
{
    if (!m_sceneController || !m_undoService || ids.isEmpty()) {
        return;
    }

    QVector<QUuid> validIds;
    QVector<QVector3D> oldPositions;
    QVector<QVector3D> newPositions;

    for (const QUuid &id : ids) {
        const SceneController::Node *node = m_sceneController->findNode(id);
        if (!node) {
            continue;
        }
        const auto pos = node->position();
        QVector3D oldPos(pos[0], pos[1], pos[2]);
        QVector3D newPos = oldPos;
        switch (axis) {
        case 'x':
            newPos.setX(value);
            break;
        case 'y':
            newPos.setY(value);
            break;
        case 'z':
            newPos.setZ(value);
            break;
        default:
            return;
        }
        if (qFuzzyCompare(oldPos.x() + 1.0, newPos.x() + 1.0) &&
            qFuzzyCompare(oldPos.y() + 1.0, newPos.y() + 1.0) &&
            qFuzzyCompare(oldPos.z() + 1.0, newPos.z() + 1.0)) {
            continue;
        }
        validIds.append(id);
        oldPositions.append(oldPos);
        newPositions.append(newPos);
    }

    if (validIds.isEmpty()) {
        return;
    }

    m_undoService->pushMoveNodesCommand(m_sceneController, validIds, oldPositions, newPositions);
}

void MainWindowPresenter::handleBarMaterialEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &materialId)
{
    if (!m_sceneController || !m_undoService || ids.isEmpty() || !materialId.has_value()) {
        return;
    }

    QVector<QUuid> validIds;
    QVector<QUuid> oldMaterials;
    QVector<QUuid> oldSections;
    bool changed = false;
    const QUuid newMaterial = materialId.value();

    for (const QUuid &id : ids) {
        const SceneController::Bar *bar = m_sceneController->findBar(id);
        if (!bar) {
            continue;
        }
        validIds.append(id);
        const QUuid oldMat = bar->materialId();
        oldMaterials.append(oldMat);
        oldSections.append(bar->sectionId());
        if (oldMat != newMaterial) {
            changed = true;
        }
    }

    if (validIds.isEmpty() || !changed) {
        return;
    }

    m_undoService->pushSetBarPropertiesCommand(m_sceneController,
                                               validIds,
                                               oldMaterials,
                                               oldSections,
                                               materialId,
                                               std::nullopt);
    if (m_lastMaterialId) {
        *m_lastMaterialId = newMaterial;
    }
}

void MainWindowPresenter::handleBarSectionEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &sectionId)
{
    if (!m_sceneController || !m_undoService || ids.isEmpty() || !sectionId.has_value()) {
        return;
    }

    QVector<QUuid> validIds;
    QVector<QUuid> oldMaterials;
    QVector<QUuid> oldSections;
    bool changed = false;
    const QUuid newSection = sectionId.value();

    for (const QUuid &id : ids) {
        const SceneController::Bar *bar = m_sceneController->findBar(id);
        if (!bar) {
            continue;
        }
        validIds.append(id);
        oldMaterials.append(bar->materialId());
        const QUuid oldSec = bar->sectionId();
        oldSections.append(oldSec);
        if (oldSec != newSection) {
            changed = true;
        }
    }

    if (validIds.isEmpty() || !changed) {
        return;
    }

    m_undoService->pushSetBarPropertiesCommand(m_sceneController,
                                               validIds,
                                               oldMaterials,
                                               oldSections,
                                               std::nullopt,
                                               sectionId);
    if (m_lastSectionId) {
        *m_lastSectionId = newSection;
    }
}

} // namespace Structura::UI
