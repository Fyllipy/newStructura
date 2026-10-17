#include "MainWindow.h"

#include <QToolButton>

#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QVTKOpenGLNativeWidget.h>

#include "PlacementController.h"
#include "PlacementOverlay.h"
#include "viz/SceneController.h"

bool MainWindow::handleWindowDragEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent *>(event);
        return handleWindowDragMousePress(me->button(), me->globalPos());
    }
    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent *>(event);
        return handleWindowDragMouseMove(me->globalPos());
    }
    case QEvent::MouseButtonRelease: {
        auto *me = static_cast<QMouseEvent *>(event);
        return handleWindowDragMouseRelease(me->button());
    }
    case QEvent::MouseButtonDblClick: {
        auto *me = static_cast<QMouseEvent *>(event);
        return handleWindowDragMouseDoubleClick(me->button());
    }
    default:
        return false;
    }
}

bool MainWindow::handleDefaultViewportEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseMove: {
        auto *mm = static_cast<QMouseEvent *>(event);
        return handleDefaultViewportMouseMove(mm->pos());
    }
    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent *>(event);
        return handleDefaultViewportMousePress(me->button(), me->modifiers(), me->pos());
    }
    case QEvent::MouseButtonRelease: {
        auto *me = static_cast<QMouseEvent *>(event);
        return handleDefaultViewportMouseRelease(me->button(), me->pos(), me->globalPosition().toPoint());
    }
    case QEvent::MouseButtonDblClick: {
        // duplo clique em um elemento selecionado: abre o painel de propriedades
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton && m_selectionModel && m_propertiesToolButton
            && (!m_selectionModel->selectedNodes().isEmpty() || !m_selectionModel->selectedBars().isEmpty())) {
            if (!m_propertiesToolButton->isChecked()) {
                m_propertiesToolButton->setChecked(true);
            } else {
                refreshPropertiesPanel();
            }
        }
        return false;
    }
    case QEvent::Leave:
        return handleDefaultViewportLeave();
    default:
        return false;
    }
}

bool MainWindow::handleViewportCommandEvent(QEvent *event)
{
    if (m_placement && m_placement->active()) {
        return m_placement->handleViewportEvent(event);
    }
    return handleDefaultViewportEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_selectionModel && m_command == Command::None && !(m_placement && m_placement->active())
        && (!m_selectionModel->selectedNodes().isEmpty() || !m_selectionModel->selectedBars().isEmpty())) {
        m_selectionModel->clear();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_vtkWidget && event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape && m_selectionModel
        && m_command == Command::None && !(m_placement && m_placement->active())
        && (!m_selectionModel->selectedNodes().isEmpty() || !m_selectionModel->selectedBars().isEmpty())) {
        m_selectionModel->clear();
        return true;
    }
    if (obj == m_quickBar || obj == m_titleLabel || (obj && obj->property("dragRegion").toBool())) {
        const bool handled = handleWindowDragEvent(event);
        if (handled) {
            return true;
        }
    }

    if (obj == m_vtkWidget && m_sceneController && event->type() == QEvent::Resize) {
        positionPlaneLabel();
        positionViewCube();
        if (m_labelOverlay) {
            m_labelOverlay->resizeToParent();
            m_labelOverlay->sync(m_sceneController->compositeMatrix());
        }
    }

    if (obj == m_vtkWidget && m_sceneController) {
        const bool handled = handleViewportCommandEvent(event);
        if (handled) {
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}
