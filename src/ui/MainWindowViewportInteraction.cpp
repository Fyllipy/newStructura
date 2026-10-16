#include "MainWindow.h"

#include "BarPropertiesDialog.h"
#include "viz/SceneController.h"

#include <QDialog>
#include <QLocale>
#include <QMessageBox>

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

