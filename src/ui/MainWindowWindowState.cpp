#include "MainWindow.h"

#include "viz/SceneController.h"

#include <QEvent>
#include <QStyle>
#include <QToolButton>

void MainWindow::updateMaximizeButtonIcon()
{
    if (!m_maximizeButton) {
        return;
    }
    if (isMaximized()) {
        m_maximizeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
        m_maximizeButton->setToolTip(tr("Restaurar"));
    } else {
        m_maximizeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
        m_maximizeButton->setToolTip(tr("Maximizar"));
    }
}

void MainWindow::toggleMaximized()
{
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
    updateMaximizeButtonIcon();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        updateMaximizeButtonIcon();
    }
}

void MainWindow::onShowBarLCSToggled(bool checked)
{
    if (m_sceneController) {
        m_sceneController->setShowBarLCS(checked);
    }
}
