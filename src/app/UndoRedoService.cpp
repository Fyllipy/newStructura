#include "UndoRedoService.h"

#include "../SceneController.h"

#include <QAction>
#include <vector>

namespace Structura::App {

namespace {

class MoveNodesCommandImpl : public QUndoCommand
{
public:
    MoveNodesCommandImpl(SceneController *scene,
                         QVector<QUuid> ids,
                         QVector<QVector3D> oldPositions,
                         QVector<QVector3D> newPositions,
                         QUndoCommand *parent = nullptr)
        : QUndoCommand(parent)
        , m_scene(scene)
        , m_ids(std::move(ids))
        , m_oldPositions(std::move(oldPositions))
        , m_newPositions(std::move(newPositions))
    {
        setText(QObject::tr("Mover nó(s)"));
    }

    void undo() override { apply(m_oldPositions); }
    void redo() override { apply(m_newPositions); }

private:
    void apply(const QVector<QVector3D> &positions)
    {
        if (!m_scene) {
            return;
        }
        m_scene->updateNodePositions(m_ids, positions);
    }

    SceneController *m_scene;
    QVector<QUuid> m_ids;
    QVector<QVector3D> m_oldPositions;
    QVector<QVector3D> m_newPositions;
};

class SetBarPropertiesCommandImpl : public QUndoCommand
{
public:
    SetBarPropertiesCommandImpl(SceneController *scene,
                                QVector<QUuid> ids,
                                QVector<QUuid> oldMaterials,
                                QVector<QUuid> oldSections,
                                std::optional<QUuid> newMaterial,
                                std::optional<QUuid> newSection,
                                QUndoCommand *parent = nullptr)
        : QUndoCommand(parent)
        , m_scene(scene)
        , m_ids(std::move(ids))
        , m_oldMaterials(std::move(oldMaterials))
        , m_oldSections(std::move(oldSections))
        , m_newMaterial(std::move(newMaterial))
        , m_newSection(std::move(newSection))
    {
        setText(QObject::tr("Atualizar propriedades de barra"));
    }

    void undo() override
    {
        if (!m_scene) {
            return;
        }
        for (int i = 0; i < m_ids.size(); ++i) {
            std::vector<QUuid> single { m_ids.at(i) };
            std::optional<QUuid> mat(m_oldMaterials.at(i));
            std::optional<QUuid> sec(m_oldSections.at(i));
            m_scene->assignBarProperties(single, mat, sec);
        }
    }

    void redo() override
    {
        if (!m_scene) {
            return;
        }
        if (!m_newMaterial.has_value() && !m_newSection.has_value()) {
            return;
        }
        std::vector<QUuid> barIds;
        barIds.reserve(m_ids.size());
        for (const QUuid &id : m_ids) {
            barIds.push_back(id);
        }
        m_scene->assignBarProperties(barIds, m_newMaterial, m_newSection);
    }

private:
    SceneController *m_scene;
    QVector<QUuid> m_ids;
    QVector<QUuid> m_oldMaterials;
    QVector<QUuid> m_oldSections;
    std::optional<QUuid> m_newMaterial;
    std::optional<QUuid> m_newSection;
};

} // namespace

UndoRedoService::UndoRedoService(QObject *parent)
    : QObject(parent)
    , m_stack(new QUndoStack(this))
{
    m_stack->setUndoLimit(128);
}

QAction *UndoRedoService::createUndoAction(QObject *parent, const QString &text) const
{
    return m_stack->createUndoAction(parent, text);
}

QAction *UndoRedoService::createRedoAction(QObject *parent, const QString &text) const
{
    return m_stack->createRedoAction(parent, text);
}

void UndoRedoService::pushMoveNodesCommand(SceneController *scene,
                                           const QVector<QUuid> &ids,
                                           const QVector<QVector3D> &oldPositions,
                                           const QVector<QVector3D> &newPositions)
{
    if (!scene) {
        return;
    }

    if (ids.isEmpty() || ids.size() != oldPositions.size() || ids.size() != newPositions.size()) {
        return;
    }

    m_stack->push(new MoveNodesCommandImpl(scene, ids, oldPositions, newPositions));
}

void UndoRedoService::pushSetBarPropertiesCommand(SceneController *scene,
                                                  const QVector<QUuid> &ids,
                                                  const QVector<QUuid> &oldMaterials,
                                                  const QVector<QUuid> &oldSections,
                                                  const std::optional<QUuid> &newMaterial,
                                                  const std::optional<QUuid> &newSection)
{
    if (!scene) {
        return;
    }
    if (ids.isEmpty() || ids.size() != oldMaterials.size() || ids.size() != oldSections.size()) {
        return;
    }

    m_stack->push(new SetBarPropertiesCommandImpl(scene,
                                                  ids,
                                                  oldMaterials,
                                                  oldSections,
                                                  newMaterial,
                                                  newSection));
}

} // namespace Structura::App
