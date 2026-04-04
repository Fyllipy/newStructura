#include "MainWindow.h"

#include "CoordinateDialog.h"
#include "GridDialog.h"
#include "viz/SceneController.h"

#include <QDialog>
#include <QMessageBox>
#include <QVTKOpenGLNativeWidget.h>

void MainWindow::onInsertNodeByCoordinates()
{
    CoordinateDialog dialog(this);
    // Allow starting on-screen insertion from this dialog
    connect(&dialog, &CoordinateDialog::requestScreenInsertion, this, [this, &dialog]() {
        dialog.reject(); // close dialog before starting insert mode
        onStartScreenInsert();
    });
    if (dialog.exec() == QDialog::Accepted) {
        m_sceneController->addPoint(dialog.x(), dialog.y(), dialog.z());
    }
}

void MainWindow::onResetCamera()
{
    m_sceneController->resetCamera();
}

void MainWindow::onZoomExtents()
{
    m_sceneController->zoomExtents();
}

void MainWindow::onGenerateGrid()
{
    GridDialog gd(this);
    if (gd.exec() == QDialog::Accepted) {
        m_sceneController->createGrid(gd.dx(), gd.dy(), gd.dz(), gd.nx(), gd.ny(), gd.nz());
        refreshPropertiesPanel();
    }
}

void MainWindow::onAddGridLineX()
{
    setCommand(Command::AddGridLineX);
    if (m_vtkWidget) {
        m_vtkWidget->setFocus();
    }
}

void MainWindow::onAddGridLineY()
{
    setCommand(Command::AddGridLineY);
    if (m_vtkWidget) {
        m_vtkWidget->setFocus();
    }
}

void MainWindow::onAddGridLineZ()
{
    setCommand(Command::AddGridLineZ);
    if (m_vtkWidget) {
        m_vtkWidget->setFocus();
    }
}

void MainWindow::onDeleteGridLine()
{
    setCommand(Command::DeleteGridLine);
    if (m_vtkWidget) {
        m_vtkWidget->setFocus();
    }
}

void MainWindow::onStartScreenInsert()
{
    setCommand(Command::InsertNode);
    if (m_vtkWidget) {
        m_vtkWidget->setFocus();
    }
}

void MainWindow::onInsertBar()
{
    if (m_sceneController->nodeCount() < 2) {
        QMessageBox::information(this, tr("Inserir barra"), tr("Insira ao menos dois nos antes de criar uma barra."));
        return;
    }

    m_firstBarNodeId = QUuid();
    setCommand(Command::InsertBarFirst);
    if (m_vtkWidget) {
        m_vtkWidget->setFocus();
    }
}
