#pragma once

#include <QObject>
#include <QUndoStack>
#include <QVector>
#include <QVector3D>
#include <QUuid>
#include <QString>
#include <optional>

#include "app/ISceneEditGateway.h"

class QAction;

namespace Structura::App {

/**
 * @brief Service wrapper around QUndoStack providing domain specific commands.
 */
class UndoRedoService : public QObject
{
    Q_OBJECT

public:
    explicit UndoRedoService(QObject *parent = nullptr);

    [[nodiscard]] QUndoStack *stack() const noexcept { return m_stack; }
    [[nodiscard]] QAction *createUndoAction(QObject *parent, const QString &text) const;
    [[nodiscard]] QAction *createRedoAction(QObject *parent, const QString &text) const;

    void pushMoveNodesCommand(ISceneEditGateway *scene,
                              const QVector<QUuid> &ids,
                              const QVector<QVector3D> &oldPositions,
                              const QVector<QVector3D> &newPositions);

    void pushSetBarPropertiesCommand(ISceneEditGateway *scene,
                                     const QVector<QUuid> &ids,
                                     const QVector<QUuid> &oldMaterials,
                                     const QVector<QUuid> &oldSections,
                                     const std::optional<QUuid> &newMaterial,
                                     const std::optional<QUuid> &newSection);

private:
    class MoveNodesCommand;
    class SetBarPropertiesCommand;

    QUndoStack *m_stack;
};

} // namespace Structura::App
