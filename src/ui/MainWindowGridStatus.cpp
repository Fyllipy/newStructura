#include "MainWindow.h"

#include "viz/SceneController.h"
#include "ui/GridDeleteTooltipHelper.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QStatusBar>
#include <QToolButton>
#include <QVTKOpenGLNativeWidget.h>

void MainWindow::beginGridInsert(Structura::Model::GridLine::Axis axis)
{
    Structura::UI::GridInsertController::beginInsert(m_gridInsertState, axis, m_sceneController);
    hideGridDeleteTooltip();
    updateStatus();
}

void MainWindow::cancelGridInsert()
{
    Structura::UI::GridInsertController::cancelInsert(m_gridInsertState, m_sceneController);
    updateStatus();
}

QString MainWindow::gridAxisLabel(Structura::Model::GridLine::Axis axis) const
{
    switch (axis) {
    case Structura::Model::GridLine::Axis::Y:
        return QStringLiteral("Y");
    case Structura::Model::GridLine::Axis::Z:
        return QStringLiteral("Z");
    case Structura::Model::GridLine::Axis::X:
    default:
        return QStringLiteral("X");
    }
}

Structura::Model::GridLine::Axis MainWindow::commandToAxis(Command command) const
{
    using Axis = Structura::Model::GridLine::Axis;
    switch (command) {
    case Command::AddGridLineY:
        return Axis::Y;
    case Command::AddGridLineZ:
        return Axis::Z;
    case Command::AddGridLineX:
    default:
        return Axis::X;
    }
}

void MainWindow::setupFooterBar()
{
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setStyleSheet(QStringLiteral(
        "QStatusBar { background: #e9edf4; color: #1f242c; border-top: 1px solid #cbd4e2; }"
    ));

    m_footerBar = new QWidget(this);
    auto *footerLayout = new QHBoxLayout(m_footerBar);
    footerLayout->setContentsMargins(8, 2, 8, 2);
    footerLayout->setSpacing(12);

    auto *viewControlsLabel = new QLabel(tr("Visualizacao:"), m_footerBar);
    viewControlsLabel->setStyleSheet("font-weight: 600; color: #1e232b;");
    footerLayout->addWidget(viewControlsLabel);

    m_footerResetCameraButton = new QToolButton(m_footerBar);
    m_footerResetCameraButton->setDefaultAction(m_resetCameraAction);
    m_footerResetCameraButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_footerResetCameraButton->setAutoRaise(true);
    m_footerResetCameraButton->setIconSize(QSize(20, 20));
    m_footerResetCameraButton->setToolTip(tr("Visao inicial"));
    footerLayout->addWidget(m_footerResetCameraButton);

    m_footerZoomExtentsButton = new QToolButton(m_footerBar);
    m_footerZoomExtentsButton->setDefaultAction(m_zoomExtentsAction);
    m_footerZoomExtentsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_footerZoomExtentsButton->setAutoRaise(true);
    m_footerZoomExtentsButton->setIconSize(QSize(20, 20));
    m_footerZoomExtentsButton->setToolTip(tr("Zoom extents"));
    footerLayout->addWidget(m_footerZoomExtentsButton);

    auto *separator1 = new QFrame(m_footerBar);
    separator1->setFrameShape(QFrame::VLine);
    separator1->setFrameShadow(QFrame::Sunken);
    separator1->setStyleSheet("color: #cbd4e2;");
    footerLayout->addWidget(separator1);

    auto *glyphScaleLabel = new QLabel(tr("Escala de glifos:"), m_footerBar);
    glyphScaleLabel->setStyleSheet("font-weight: 600; color: #1e232b;");
    footerLayout->addWidget(glyphScaleLabel);

    m_glyphScaleSlider = new QSlider(Qt::Horizontal, m_footerBar);
    m_glyphScaleSlider->setMinimum(50);
    m_glyphScaleSlider->setMaximum(200);
    m_glyphScaleSlider->setValue(100);
    m_glyphScaleSlider->setFixedWidth(120);
    m_glyphScaleSlider->setToolTip(tr("Ajustar tamanho dos glifos (nos, cargas, suportes)"));
    footerLayout->addWidget(m_glyphScaleSlider);

    auto *glyphScaleValueLabel = new QLabel(QStringLiteral("100%"), m_footerBar);
    glyphScaleValueLabel->setMinimumWidth(40);
    glyphScaleValueLabel->setStyleSheet("color: #1e232b;");
    footerLayout->addWidget(glyphScaleValueLabel);

    connect(m_glyphScaleSlider, &QSlider::valueChanged, this, [glyphScaleValueLabel](int value) {
        glyphScaleValueLabel->setText(QString("%1%").arg(value));
        // TODO: Apply scale to scene controller when implementing glyph scaling
    });

    auto *separator2 = new QFrame(m_footerBar);
    separator2->setFrameShape(QFrame::VLine);
    separator2->setFrameShadow(QFrame::Sunken);
    separator2->setStyleSheet("color: #cbd4e2;");
    footerLayout->addWidget(separator2);

    m_statusLabel = new QLabel(tr("Pronto"), m_footerBar);
    m_statusLabel->setStyleSheet("color: #1e232b; padding-left: 8px;");
    m_statusLabel->setMinimumWidth(300);
    footerLayout->addWidget(m_statusLabel, 1);

    statusBar()->addPermanentWidget(m_footerBar, 1);
}

void MainWindow::updateGridInsertFromPoint(const QVector3D &worldPoint)
{
    Structura::UI::GridInsertController::updateFromPoint(m_gridInsertState, worldPoint, m_sceneController);
    refreshGridInsertVisuals();
}

void MainWindow::refreshGridInsertVisuals()
{
    Structura::UI::GridInsertController::refreshVisuals(m_gridInsertState, m_sceneController);
    updateStatus();
}

void MainWindow::updateGridDeleteTooltip(const QPoint &widgetPos, const QUuid &lineId)
{
    if (!m_vtkWidget) {
        return;
    }

    if (lineId.isNull()) {
        hideGridDeleteTooltip();
        return;
    }

    QString message;
    if (!m_pendingDeleteLineId.isNull() && m_pendingDeleteLineId == lineId) {
        message = tr("Clique novamente para confirmar exclusao");
    } else {
        QString details;
        if (m_sceneController) {
            if (const auto *line = m_sceneController->findGridLine(lineId)) {
                details = tr("%1 @ %2 / %3")
                    .arg(gridAxisLabel(line->axis()),
                         QString::number(line->coordinate1(), 'f', 3),
                         QString::number(line->coordinate2(), 'f', 3));
            }
        }
        if (details.isEmpty()) {
            details = tr("linha");
        }
        message = tr("Clique para selecionar %1").arg(details);
    }

    Structura::UI::GridDeleteTooltipHelper::show(m_gridDeleteTooltip,
                                                  m_vtkWidget,
                                                  widgetPos,
                                                  message);
}

void MainWindow::hideGridDeleteTooltip()
{
    Structura::UI::GridDeleteTooltipHelper::hide(m_gridDeleteTooltip);
}

void MainWindow::updateGridActionsEnabled()
{
    const bool hasGrid = m_sceneController && m_sceneController->hasGrid();
    if (m_addGridLineXAction) m_addGridLineXAction->setEnabled(hasGrid);
    if (m_addGridLineYAction) m_addGridLineYAction->setEnabled(hasGrid);
    if (m_addGridLineZAction) m_addGridLineZAction->setEnabled(hasGrid);
    if (m_deleteGridLineAction) m_deleteGridLineAction->setEnabled(hasGrid);
}

void MainWindow::showStatusMessage(const QString &message, int timeout)
{
    if (m_statusLabel) {
        m_statusLabel->setText(message);
    }
    statusBar()->showMessage(message, timeout);
}

void MainWindow::updateStatus()
{
    switch (m_command) {
    case Command::InsertNode:
    {
        QString message = tr("Insercao de nos: clique esquerdo para inserir | Esc para sair");
        if (m_hoverInsertPoint) {
            const QVector3D &p = *m_hoverInsertPoint;
            message += tr(" | X=%1 Y=%2 Z=%3")
                .arg(QString::number(p.x(), 'f', 3),
                     QString::number(p.y(), 'f', 3),
                     QString::number(p.z(), 'f', 3));
        }
        showStatusMessage(message);
        break;
    }
    case Command::InsertBarFirst:
        showStatusMessage(tr("Criar barra: selecione o primeiro no (Esc para cancelar)"));
        break;
    case Command::InsertBarSecond:
    {
        QString nodeLabel = QStringLiteral("?");
        if (!m_firstBarNodeId.isNull()) {
            if (const auto *node = m_sceneController->findNode(m_firstBarNodeId)) {
                nodeLabel = QString::number(node->externalId());
            }
        }
        showStatusMessage(tr("Criar barra: selecione o segundo no (primeiro = N%1) | Esc para cancelar")
            .arg(nodeLabel));
        break;
    }
    case Command::AddGridLineX:
    case Command::AddGridLineY:
    case Command::AddGridLineZ:
    {
        const auto axis = commandToAxis(m_command);
        QString message = tr("Adicionar linha %1: clique para posicionar")
            .arg(gridAxisLabel(axis));
        if (m_gridInsertState.hasTypedValue) {
            message += tr(" | deslocamento=%1 m").arg(QString::number(m_gridInsertState.typedValue, 'f', 3));
        } else if (m_gridInsertState.pointerValid) {
            message += tr(" | alvo=%1 / %2 m")
                .arg(QString::number(m_gridInsertState.ghostCoord1, 'f', 3),
                     QString::number(m_gridInsertState.ghostCoord2, 'f', 3));
        }
        message += tr(" | Enter confirma | Esc cancelar");
        showStatusMessage(message);
        break;
    }
    case Command::DeleteGridLine:
        if (!m_pendingDeleteLineId.isNull()) {
            showStatusMessage(tr("Excluir linha de grid: clique novamente para confirmar | Esc para cancelar"));
        } else {
            showStatusMessage(tr("Excluir linha de grid: selecione uma linha e clique para marcar | Esc para cancelar"));
        }
        break;
    default:
    {
        const int nodeCount = m_selectionModel ? m_selectionModel->selectedNodes().size() : 0;
        const int barCount = m_selectionModel ? m_selectionModel->selectedBars().size() : 0;
        if (nodeCount > 0 || barCount > 0) {
            showStatusMessage(tr("Selecionados: %1 no(s), %2 barra(s)").arg(nodeCount).arg(barCount));
        } else {
            showStatusMessage(tr("Pronto"));
        }
        break;
    }
    }
}
