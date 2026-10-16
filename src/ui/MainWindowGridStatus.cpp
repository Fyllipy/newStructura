#include "MainWindow.h"

#include "viz/SceneController.h"
#include "viz/postprocessor/PostProcessorRenderer.h"
#include "PlacementController.h"
#include "ViewOptionsBar.h"
#include <QVBoxLayout>

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QStatusBar>
#include <QToolButton>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkRenderWindow.h>

void MainWindow::setupFooterBar()
{
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setStyleSheet(QStringLiteral(
        "QStatusBar { background: #e9edf4; color: #1f242c; border-top: 1px solid #cbd4e2; }"
    ));

    // O rodape agora tem duas linhas: opcoes de exibicao (acima) e os controles existentes.
    m_footerBar = new QWidget(this);
    auto *footerRoot = new QVBoxLayout(m_footerBar);
    footerRoot->setContentsMargins(0, 0, 0, 0);
    footerRoot->setSpacing(0);

    auto *optionsRow = new QWidget(m_footerBar);
    auto *optionsLayout = new QHBoxLayout(optionsRow);
    optionsLayout->setContentsMargins(8, 2, 8, 0);
    m_viewOptionsBar = new Structura::UI::ViewOptionsBar(optionsRow);
    optionsLayout->addWidget(m_viewOptionsBar);
    footerRoot->addWidget(optionsRow);

    auto *footerRow = new QWidget(m_footerBar);
    auto *footerLayout = new QHBoxLayout(footerRow);
    footerLayout->setContentsMargins(8, 2, 8, 2);
    footerLayout->setSpacing(12);
    footerRoot->addWidget(footerRow);

    auto *viewControlsLabel = new QLabel(tr("Visualizacao:"), m_footerBar);
    viewControlsLabel->setStyleSheet("font-weight: 600; color: #1e232b;");
    footerLayout->addWidget(viewControlsLabel);

    m_footerResetCameraButton = new QToolButton(footerRow);
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

    // ── Controles de diagrama (pós-processamento) ────────────────────────
    auto *separatorDiagram = new QFrame(m_footerBar);
    separatorDiagram->setFrameShape(QFrame::VLine);
    separatorDiagram->setFrameShadow(QFrame::Sunken);
    separatorDiagram->setStyleSheet("color: #cbd4e2;");
    footerLayout->addWidget(separatorDiagram);

    auto *diagramLabel = new QLabel(tr("Diagrama:"), m_footerBar);
    diagramLabel->setStyleSheet("font-weight: 600; color: #1e232b;");
    footerLayout->addWidget(diagramLabel);

    m_diagramCombo = new QComboBox(m_footerBar);
    m_diagramCombo->addItem(tr("---"),  static_cast<int>(Structura::Visualization::DiagramType::None));
    m_diagramCombo->addItem(tr("N"),    static_cast<int>(Structura::Visualization::DiagramType::N));
    m_diagramCombo->addItem(tr("Vy"),   static_cast<int>(Structura::Visualization::DiagramType::Vy));
    m_diagramCombo->addItem(tr("Vz"),   static_cast<int>(Structura::Visualization::DiagramType::Vz));
    m_diagramCombo->addItem(tr("Mt"),   static_cast<int>(Structura::Visualization::DiagramType::Mt));
    m_diagramCombo->addItem(tr("My"),   static_cast<int>(Structura::Visualization::DiagramType::My));
    m_diagramCombo->addItem(tr("Mz"),   static_cast<int>(Structura::Visualization::DiagramType::Mz));
    m_diagramCombo->setMinimumWidth(70);
    m_diagramCombo->setEnabled(false);
    m_diagramCombo->setToolTip(tr("Selecionar tipo de diagrama de esforços"));
    footerLayout->addWidget(m_diagramCombo);

    auto *scaleLabel = new QLabel(tr("Escala:"), m_footerBar);
    scaleLabel->setStyleSheet("font-weight: 600; color: #1e232b;");
    footerLayout->addWidget(scaleLabel);

    m_diagramScaleSlider = new QSlider(Qt::Horizontal, m_footerBar);
    m_diagramScaleSlider->setMinimum(1);
    m_diagramScaleSlider->setMaximum(100);
    m_diagramScaleSlider->setValue(33); // ~1.0
    m_diagramScaleSlider->setFixedWidth(100);
    m_diagramScaleSlider->setEnabled(false);
    m_diagramScaleSlider->setToolTip(tr("Ajustar escala visual dos diagramas"));
    footerLayout->addWidget(m_diagramScaleSlider);

    auto *modLabel = new QLabel(tr("Modo:"), m_footerBar);
    modLabel->setStyleSheet("font-weight: 600; color: #1e232b;");
    footerLayout->addWidget(modLabel);

    m_scaleModeCombo = new QComboBox(m_footerBar);
    m_scaleModeCombo->addItem(tr("Global"),       static_cast<int>(Structura::Visualization::DiagramScaleMode::Global));
    m_scaleModeCombo->addItem(tr("Caso Atual"),   static_cast<int>(Structura::Visualization::DiagramScaleMode::CurrentLoadCase));
    m_scaleModeCombo->addItem(tr("Selecionados"), static_cast<int>(Structura::Visualization::DiagramScaleMode::SelectedElements));
    m_scaleModeCombo->setEnabled(false);
    m_scaleModeCombo->setToolTip(tr("Modo de normalização da escala"));
    footerLayout->addWidget(m_scaleModeCombo);

    // Conexões dos controles de diagrama
    connect(m_diagramCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onDiagramTypeChanged);
    connect(m_diagramScaleSlider, &QSlider::valueChanged,
            this, &MainWindow::onDiagramScaleChanged);
    connect(m_scaleModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onScaleModeChanged);

    statusBar()->addPermanentWidget(m_footerBar, 1);
}

// ── Slots do pós-processador ───────────────────────────────────────────────────────

void MainWindow::onShowDiagrams()
{
    if (!m_postProcessor) return;
    m_postProcessor->activate();
    // Selecionar diagrama atual (se houver)
    if (m_diagramCombo) {
        onDiagramTypeChanged(m_diagramCombo->currentIndex());
    }
}

void MainWindow::onHideDiagrams()
{
    if (!m_postProcessor) return;
    m_postProcessor->deactivate();
    if (m_vtkWidget && m_vtkWidget->renderWindow()) {
        m_vtkWidget->renderWindow()->Render();
    }
}

void MainWindow::onDiagramTypeChanged(int /*index*/)
{
    if (!m_postProcessor || !m_diagramCombo) return;
    const auto type = static_cast<Structura::Visualization::DiagramType>(
        m_diagramCombo->currentData().toInt());
    m_postProcessor->showDiagram(type);
    if (m_vtkWidget && m_vtkWidget->renderWindow()) {
        m_vtkWidget->renderWindow()->Render();
    }
}

void MainWindow::onDiagramScaleChanged(int value)
{
    if (!m_postProcessor) return;
    // Mapear 1..100 -> 0.1..5.0 (curva raiz quadrada para resposta mais natural)
    const double scale = 0.1 + (static_cast<double>(value) / 100.0) * 4.9;
    m_postProcessor->setUserScale(scale);
    if (m_vtkWidget && m_vtkWidget->renderWindow()) {
        m_vtkWidget->renderWindow()->Render();
    }
}

void MainWindow::onScaleModeChanged(int /*index*/)
{
    if (!m_postProcessor || !m_scaleModeCombo) return;
    const auto mode = static_cast<Structura::Visualization::DiagramScaleMode>(
        m_scaleModeCombo->currentData().toInt());
    m_postProcessor->setScaleMode(mode);
    if (m_vtkWidget && m_vtkWidget->renderWindow()) {
        m_vtkWidget->renderWindow()->Render();
    }
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
    case Command::InsertBarFirst:
    case Command::InsertBarSecond:
    {
        QString message = (m_placement && m_placement->hasStartPoint())
            ? tr("Criar barra: clique no ponto final")
            : tr("Criar barra: clique no ponto inicial");
        message += tr(" | Tab edita | Esc sai");
        showStatusMessage(message);
        break;
    }
    case Command::CopyElements:
    case Command::MoveElements:
    {
        const QString verb = m_command == Command::CopyElements ? tr("Copiar") : tr("Mover");
        QString message = (m_placement && m_placement->hasStartPoint())
            ? tr("%1: clique no ponto de destino").arg(verb)
            : tr("%1: clique no ponto base").arg(verb);
        message += tr(" | Tab edita | Esc sai");
        showStatusMessage(message);
        break;
    }
    case Command::RotateElements:
    {
        showStatusMessage(tr("Rotacionar: clique no ponto central (pivo) | Esc sai"));
        break;
    }
    case Command::MirrorElements:
    {
        QString message = (m_placement && m_placement->hasStartPoint())
            ? tr("Espelhar: clique no segundo ponto do eixo")
            : tr("Espelhar: clique no primeiro ponto do eixo");
        message += tr(" | Tab edita | Esc sai");
        showStatusMessage(message);
        break;
    }
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
