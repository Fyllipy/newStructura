#include "MainWindow.h"

#include "ViewCubeWidget.h"
#include "viz/SceneController.h"

#include <QAction>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QToolButton>
#include <QVTKOpenGLNativeWidget.h>

#include <array>
#include <cmath>

void MainWindow::setupViewCube()
{
    if (!m_vtkWidget || !m_sceneController || m_viewCube) {
        return;
    }
    m_viewCube = new Structura::UI::ViewCubeWidget(m_vtkWidget);
    m_viewCube->setCameraProvider([this](double *position, double *target, double *up) {
        double angle = 0.0;
        bool parallel = false;
        return m_sceneController && m_sceneController->cameraState(position, target, up, angle, parallel);
    });
    connect(m_viewCube, &Structura::UI::ViewCubeWidget::viewRequested, this,
            [this](double dx, double dy, double dz, double ux, double uy, double uz) { m_sceneController->lookFromDirection(dx, dy, dz, ux, uy, uz); });
    connect(m_sceneController, &SceneController::cameraChanged, m_viewCube, qOverload<>(&QWidget::update));
    connect(m_sceneController, &SceneController::cameraChanged, this, [this]() {
        if (m_viewCube) {
            m_viewCube->setPerspective(m_sceneController->isPerspective());
        }
    });
    connect(m_viewCube, &Structura::UI::ViewCubeWidget::perspectiveToggled, this, [this](bool perspective) {
        m_sceneController->setPerspective(perspective);
        markModified();
    });
    auto *menu = new QMenu(m_viewCube->viewsButton());
    m_viewCube->viewsButton()->setMenu(menu);
    connect(menu, &QMenu::aboutToShow, this, [this, menu]() {
        menu->clear();
        for (int i = 0; i < m_extras.views3d.size(); ++i) {
            const auto &view = m_extras.views3d.at(i);
            if (view.explicitCamera) {
                menu->addAction(view.name, this, [this, i]() { applySavedView(i); });
            }
        }
        if (!menu->isEmpty()) {
            menu->addSeparator();
        }
        menu->addAction(tr("Salvar vista atual…"), this, [this]() { saveCurrentView(); });
        QMenu *remove = menu->addMenu(tr("Excluir vista salva"));
        for (int i = 0; i < m_extras.views3d.size(); ++i) {
            if (m_extras.views3d.at(i).explicitCamera) {
                remove->addAction(m_extras.views3d.at(i).name, this, [this, i]() { deleteSavedView(i); });
            }
        }
        remove->setEnabled(!remove->isEmpty());
    });
    positionViewCube();
    m_viewCube->show();
    m_viewCube->raise();
}

void MainWindow::positionViewCube()
{
    // plano, cubo e barra de ferramentas ficam juntos no canto superior direito (ver positionPlaneLabel)
    positionPlaneLabel();
    if (m_viewCube) {
        m_viewCube->raise();
    }
}

void MainWindow::saveCurrentView()
{
    double position[3], target[3], up[3], angle = 30.0;
    bool parallel = false;
    if (!m_sceneController->cameraState(position, target, up, angle, parallel)) {
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Salvar vista"), tr("Nome da vista:"), QLineEdit::Normal, tr("Vista %1").arg(m_extras.views3d.size() + 1), &ok).trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    Structura::App::ProjectData::SavedView3D view;
    view.name = name;
    view.perspective = !parallel;
    view.fovDeg = angle;
    view.explicitCamera = true;
    for (int i = 0; i < 3; ++i) {
        view.position[static_cast<std::size_t>(i)] = position[i];
        view.target[static_cast<std::size_t>(i)] = target[i];
        view.up[static_cast<std::size_t>(i)] = up[i];
    }
    bool replaced = false;
    for (auto &existing : m_extras.views3d) {
        if (existing.explicitCamera && existing.name.compare(name, Qt::CaseInsensitive) == 0) {
            existing = view;
            replaced = true;
        }
    }
    if (!replaced) {
        m_extras.views3d.append(view);
    }
    markModified();
    showStatusMessage(tr("Vista \"%1\" salva (também disponível nas pranchas).").arg(name), 5000);
}

void MainWindow::applySavedView(int index)
{
    if (index < 0 || index >= m_extras.views3d.size()) {
        return;
    }
    const auto &view = m_extras.views3d.at(index);
    m_sceneController->setCameraState(view.position.data(), view.target.data(), view.up.data(), view.fovDeg, !view.perspective);
}

void MainWindow::deleteSavedView(int index)
{
    if (index < 0 || index >= m_extras.views3d.size()) {
        return;
    }
    const QString name = m_extras.views3d.at(index).name;
    m_extras.views3d.removeAt(index);
    markModified();
    showStatusMessage(tr("Vista \"%1\" excluída.").arg(name), 4000);
}
