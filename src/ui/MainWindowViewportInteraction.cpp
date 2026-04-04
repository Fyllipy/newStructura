#include "MainWindow.h"

#include "BarPropertiesDialog.h"
#include "viz/SceneController.h"

#include <QDialog>
#include <QLocale>
#include <QMessageBox>

bool MainWindow::handleInsertNodeMousePress(Qt::MouseButton button, const QPoint &widgetPos)
{
    if (button == Qt::LeftButton) {
        double wx = 0.0;
        double wy = 0.0;
        double wz = 0.0;
        if (computeWorldPointForInsert(widgetPos, wx, wy, wz, true)) {
            m_sceneController->addPoint(wx, wy, wz);
            setHoverInsertPoint(std::nullopt);
        }
        return true;
    }
    if (button == Qt::RightButton) {
        return false;
    }
    return false;
}

bool MainWindow::handleInsertNodeMouseMove(const QPoint &widgetPos)
{
    double wx = 0.0;
    double wy = 0.0;
    double wz = 0.0;
    if (computeWorldPointForInsert(widgetPos, wx, wy, wz, true)) {
        setHoverInsertPoint(QVector3D(wx, wy, wz));
    } else {
        setHoverInsertPoint(std::nullopt);
    }

    updateHighlightedNodeAtWidgetPos(widgetPos);
    return false;
}

bool MainWindow::handleInsertNodeKeyPress(int key)
{
    if (key == Qt::Key_Escape) {
        setCommand(Command::None);
        return true;
    }
    return false;
}

bool MainWindow::handleInsertBarMouseMove(const QPoint &widgetPos)
{
    updateHighlightedNodeAtWidgetPos(widgetPos);
    return false;
}

bool MainWindow::handleInsertBarMousePress(Qt::MouseButton button, const QPoint &widgetPos)
{
    if (button == Qt::LeftButton) {
        const QUuid pickedNode = pickNodeAtWidgetPos(widgetPos);
        if (!pickedNode.isNull()) {
            if (m_command == Command::InsertBarFirst) {
                m_firstBarNodeId = pickedNode;
                m_sceneController->setHighlightedNode(pickedNode);
                setCommand(Command::InsertBarSecond);
            } else {
                if (pickedNode == m_firstBarNodeId) {
                    QMessageBox::warning(this, tr("Inserir barra"), tr("Selecione dois nos distintos."));
                    return true;
                }

                BarPropertiesDialog barDialog(buildMaterialOptions(), buildSectionOptions(), this);
                barDialog.setCurrentMaterial(m_lastMaterialId);
                barDialog.setCurrentSection(m_lastSectionId);
                if (barDialog.exec() == QDialog::Accepted) {
                    const QUuid materialId = barDialog.selectedMaterial();
                    const QUuid sectionId = barDialog.selectedSection();
                    const QUuid barId = m_sceneController->addBar(m_firstBarNodeId, pickedNode, materialId, sectionId);
                    if (!barId.isNull()) {
                        const int externalBarId = nextBarExternalId();
                        m_sceneController->setBarExternalId(barId, externalBarId);
                        if (!materialId.isNull()) {
                            m_lastMaterialId = materialId;
                        }
                        if (!sectionId.isNull()) {
                            m_lastSectionId = sectionId;
                        }
                    }
                }
                m_firstBarNodeId = QUuid();
                m_sceneController->clearHighlightedNode();
                setCommand(Command::InsertBarFirst);
            }
        }
        return true;
    }

    if (button == Qt::RightButton) {
        return false;
    }

    return false;
}

bool MainWindow::handleInsertBarKeyPress(int key)
{
    if (key == Qt::Key_Escape) {
        setCommand(Command::None);
        return true;
    }
    return false;
}

bool MainWindow::handleGridInsertMouseMove(const QPoint &widgetPos)
{
    double wx = 0.0;
    double wy = 0.0;
    double wz = 0.0;
    if (computeWorldPointForInsert(widgetPos, wx, wy, wz, true)) {
        updateGridInsertFromPoint(QVector3D(wx, wy, wz));
    } else {
        m_gridInsertState.pointerValid = false;
        if (m_sceneController) {
            m_sceneController->hideGridGhostLine();
            m_sceneController->clearHighlightedGridLine();
        }
        updateStatus();
    }
    return false;
}

bool MainWindow::commitGridInsertIfValid()
{
    if (!(m_gridInsertState.active && m_gridInsertState.pointerValid && m_sceneController)) {
        return false;
    }

    const auto axis = commandToAxis(m_command);
    const QUuid created = m_sceneController->addGridLine(axis,
                                                         m_gridInsertState.ghostCoord1,
                                                         m_gridInsertState.ghostCoord2);
    if (!created.isNull()) {
        refreshPropertiesPanel();
        refreshGridInsertVisuals();
    }
    return true;
}

bool MainWindow::handleGridInsertMousePress(Qt::MouseButton button)
{
    if (button == Qt::LeftButton) {
        commitGridInsertIfValid();
        return true;
    }
    if (button == Qt::RightButton) {
        setCommand(Command::None);
        return true;
    }
    return false;
}

bool MainWindow::handleGridInsertKeyPress(int key, Qt::KeyboardModifiers modifiers, const QString &text)
{
    if (key == Qt::Key_Escape) {
        setCommand(Command::None);
        return true;
    }
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        commitGridInsertIfValid();
        return true;
    }

    const QString previousBuffer = m_gridInsertState.inputBuffer;
    bool handled = false;
    if (key == Qt::Key_Backspace) {
        if (!m_gridInsertState.inputBuffer.isEmpty()) {
            m_gridInsertState.inputBuffer.chop(1);
            handled = true;
        }
    } else if (!(modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        if (!text.isEmpty()) {
            QChar ch = text.at(0);
            if (ch == QLatin1Char(',')) {
                ch = QLatin1Char('.');
            }
            const bool canInsert =
                ch.isDigit()
                || (ch == QLatin1Char('-') && m_gridInsertState.inputBuffer.isEmpty())
                || (ch == QLatin1Char('.') && !m_gridInsertState.inputBuffer.contains('.'));
            if (canInsert) {
                m_gridInsertState.inputBuffer.append(ch);
                handled = true;
            }
        }
    }

    if (handled) {
        const QString buffer = m_gridInsertState.inputBuffer;
        if (buffer.isEmpty()) {
            m_gridInsertState.hasTypedValue = false;
            m_gridInsertState.referenceLocked = false;
            m_gridInsertState.typedValue = 0.0;
        } else if (buffer == QStringLiteral("-") || buffer == QStringLiteral(".") || buffer == QStringLiteral("-.")) {
            m_gridInsertState.hasTypedValue = false;
            if (!m_gridInsertState.referenceLocked) {
                m_gridInsertState.referenceLocked = true;
                if (m_gridInsertState.referenceLineId.isNull()) {
                    m_gridInsertState.referenceCoord1 = m_gridInsertState.pointerCoord1;
                    m_gridInsertState.referenceCoord2 = m_gridInsertState.pointerCoord2;
                }
            }
        } else {
            bool ok = false;
            const double value = QLocale::c().toDouble(buffer, &ok);
            if (ok) {
                m_gridInsertState.typedValue = value;
                m_gridInsertState.hasTypedValue = true;
                if (!m_gridInsertState.referenceLocked) {
                    m_gridInsertState.referenceLocked = true;
                    if (m_gridInsertState.referenceLineId.isNull()) {
                        m_gridInsertState.referenceCoord1 = m_gridInsertState.pointerCoord1;
                        m_gridInsertState.referenceCoord2 = m_gridInsertState.pointerCoord2;
                    }
                }
            } else {
                m_gridInsertState.inputBuffer = previousBuffer;
                handled = false;
            }
        }
    }

    if (handled) {
        refreshGridInsertVisuals();
        return true;
    }

    return false;
}

bool MainWindow::handleDefaultViewportMouseMove(const QPoint &widgetPos)
{
    updateHighlightedNodeAtWidgetPos(widgetPos);
    return false;
}

bool MainWindow::handleDefaultViewportMousePress(Qt::MouseButton button, Qt::KeyboardModifiers modifiers, const QPoint &widgetPos)
{
    if (button != Qt::LeftButton) {
        return false;
    }

    const QUuid nodeId = pickNodeAtWidgetPos(widgetPos);
    const QUuid barId = nodeId.isNull() ? pickBarAtWidgetPos(widgetPos) : QUuid();

    Structura::SelectionModel::Mode mode = Structura::SelectionModel::Mode::Replace;
    if (modifiers.testFlag(Qt::ControlModifier)) {
        mode = Structura::SelectionModel::Mode::Toggle;
    } else if (modifiers.testFlag(Qt::ShiftModifier)) {
        mode = Structura::SelectionModel::Mode::Add;
    }

    if (!nodeId.isNull()) {
        m_selectionModel->selectNode(nodeId, mode);
        return true;
    }
    if (!barId.isNull()) {
        m_selectionModel->selectBar(barId, mode);
        return true;
    }

    if (mode == Structura::SelectionModel::Mode::Replace) {
        m_selectionModel->clear();
        return true;
    }

    return false;
}

bool MainWindow::handleDefaultViewportLeave()
{
    m_sceneController->clearHighlightedNode();
    return false;
}

bool MainWindow::handleWindowDragMousePress(Qt::MouseButton button, const QPoint &globalPos)
{
    if (button == Qt::LeftButton) {
        if (!isMaximized()) {
            m_draggingWindow = true;
            m_dragOffset = globalPos - frameGeometry().topLeft();
        }
        return true;
    }
    return false;
}

bool MainWindow::handleWindowDragMouseMove(const QPoint &globalPos)
{
    if (m_draggingWindow) {
        const QPoint newPos = globalPos - m_dragOffset;
        move(newPos);
        return true;
    }
    return false;
}

bool MainWindow::handleWindowDragMouseRelease(Qt::MouseButton button)
{
    if (button == Qt::LeftButton) {
        m_draggingWindow = false;
        return true;
    }
    return false;
}

bool MainWindow::handleWindowDragMouseDoubleClick(Qt::MouseButton button)
{
    if (button == Qt::LeftButton) {
        m_draggingWindow = false;
        toggleMaximized();
        return true;
    }
    return false;
}

bool MainWindow::handleGridDeleteMouseMove(const QPoint &widgetPos)
{
    const QPoint disp = toDisplayPoint(widgetPos);
    const QUuid lineId = m_sceneController->pickGridLine(disp.x(), disp.y());
    if (!lineId.isNull()) {
        m_sceneController->setHighlightedGridLine(lineId);
        updateGridDeleteTooltip(widgetPos, lineId);
    } else {
        m_sceneController->clearHighlightedGridLine();
        hideGridDeleteTooltip();
        m_pendingDeleteLineId = QUuid();
    }
    return false;
}

bool MainWindow::handleGridDeleteMousePress(Qt::MouseButton button, const QPoint &widgetPos)
{
    if (button == Qt::LeftButton) {
        const QPoint disp = toDisplayPoint(widgetPos);
        const QUuid lineId = m_sceneController->pickGridLine(disp.x(), disp.y());
        if (!lineId.isNull()) {
            if (m_pendingDeleteLineId == lineId) {
                if (m_sceneController->removeGridLine(lineId)) {
                    m_pendingDeleteLineId = QUuid();
                    hideGridDeleteTooltip();
                    refreshPropertiesPanel();
                }
            } else {
                m_pendingDeleteLineId = lineId;
                updateGridDeleteTooltip(widgetPos, lineId);
            }
        }
        return true;
    }

    if (button == Qt::RightButton) {
        setCommand(Command::None);
        return true;
    }

    return false;
}

bool MainWindow::handleGridDeleteKeyPress(int key)
{
    if (key == Qt::Key_Escape) {
        setCommand(Command::None);
        return true;
    }
    if ((key == Qt::Key_Return || key == Qt::Key_Enter) && !m_pendingDeleteLineId.isNull()) {
        if (m_sceneController->removeGridLine(m_pendingDeleteLineId)) {
            m_pendingDeleteLineId = QUuid();
            hideGridDeleteTooltip();
            refreshPropertiesPanel();
        }
        return true;
    }
    return false;
}
