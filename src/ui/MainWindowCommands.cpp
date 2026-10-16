#include "MainWindow.h"

#include "viz/SceneController.h"

#include <QDialog>
#include <QMessageBox>
#include <QVTKOpenGLNativeWidget.h>

void MainWindow::onResetCamera()
{
    m_sceneController->resetCamera();
}

void MainWindow::onZoomExtents()
{
    m_sceneController->zoomExtents();
}

void MainWindow::onInsertBar()
{
    m_firstBarNodeId = QUuid();
    setCommand(Command::InsertBarFirst);
    if (m_vtkWidget) {
        m_vtkWidget->setFocus();
    }
}
