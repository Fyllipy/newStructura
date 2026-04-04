#include "MainWindow.h"

#include "CoordinateDialog.h"
#include "GridDialog.h"
#include "viz/SceneController.h"
#include "app/UndoRedoService.h"
#include "app/services/IdGeneratorService.h"
#include "ui/GridInsertController.h"
#include "ui/PropertiesPanelEntryBuilder.h"
#include "ui/MainWindowPresenter.h"

#include <QAction>
#include <QApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QStyle>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QMessageBox>
#include <QPair>
#include <QSize>
#include <array>
#include <QLabel>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QUndoStack>
#include <QVector3D>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <vector>

#include <QVTKOpenGLNativeWidget.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_sceneController(new SceneController(this))
    , m_selectionModel(new Structura::SelectionModel(this))
    , m_vtkWidget(new QVTKOpenGLNativeWidget(this))
    , m_ribbon(new QTabWidget(this))
    , m_homeTabButton(nullptr)
    , m_undoService(new Structura::App::UndoRedoService(this))
    , m_presenter(nullptr)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::CustomizeWindowHint);
    setMouseTracking(true);
    setWindowTitle(tr("Structura 3D"));

    m_undoAction = m_undoService->createUndoAction(this, tr("Desfazer"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setIcon(QIcon(QStringLiteral(":/icons/undo.png")));
    addAction(m_undoAction);

    m_redoAction = m_undoService->createRedoAction(this, tr("Refazer"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setIcon(QIcon(QStringLiteral(":/icons/redo.png")));
    addAction(m_redoAction);

    Structura::UI::MainWindowPresenter::Dependencies presenterDeps;
    presenterDeps.sceneController = m_sceneController;
    presenterDeps.selectionModel = m_selectionModel;
    presenterDeps.undoService = m_undoService;
    presenterDeps.materials = &m_materials;
    presenterDeps.sections = &m_sections;
    presenterDeps.lastMaterialId = &m_lastMaterialId;
    presenterDeps.lastSectionId = &m_lastSectionId;
    presenterDeps.supports = &m_supports;
    presenterDeps.nodalLoads = &m_nodalLoads;
    presenterDeps.memberLoads = &m_memberLoads;
    presenterDeps.lastNodalPreset = &m_lastNodalPreset;
    presenterDeps.lastDistributedPreset = &m_lastDistributedPreset;
    m_presenter = new Structura::UI::MainWindowPresenter(presenterDeps, this);

    m_insertNodeCoordinatesAction = new QAction(tr("Inserir\nno (Coordenadas)"), this);
    m_insertNodeCoordinatesAction->setIcon(style()->standardIcon(QStyle::SP_DialogYesButton));
    connect(m_insertNodeCoordinatesAction, &QAction::triggered, this, &MainWindow::onInsertNodeByCoordinates);

    m_insertNodeScreenAction = new QAction(tr("Inserir\nno (Tela)"), this);
    m_insertNodeScreenAction->setIcon(QIcon(QStringLiteral(":/icons/addNode.png")));
    connect(m_insertNodeScreenAction, &QAction::triggered, this, &MainWindow::onStartScreenInsert);

    m_insertBarAction = new QAction(tr("Inserir\nbarra"), this);
    m_insertBarAction->setIcon(QIcon(QStringLiteral(":/icons/addBar.png")));
    connect(m_insertBarAction, &QAction::triggered, this, &MainWindow::onInsertBar);

    m_openModelAction = new QAction(tr("Abrir .dat"), this);
    m_openModelAction->setIcon(QIcon(QStringLiteral(":/icons/open.png")));
    m_openModelAction->setShortcut(QKeySequence::Open);
    connect(m_openModelAction, &QAction::triggered, this, &MainWindow::onOpenModel);

    m_saveModelAction = new QAction(tr("Salvar .dat"), this);
    m_saveModelAction->setIcon(QIcon(QStringLiteral(":/icons/save.png")));
    m_saveModelAction->setShortcut(QKeySequence::Save);
    connect(m_saveModelAction, &QAction::triggered, this, &MainWindow::onSaveModel);

    m_runAnalysisAction = new QAction(tr("Rodar\nanalise"), this);
    m_runAnalysisAction->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_runAnalysisAction->setShortcut(Qt::Key_F5);
    connect(m_runAnalysisAction, &QAction::triggered, this, &MainWindow::onRunStructuralAnalysis);

    m_generateGridAction = new QAction(tr("Gerar grid"), this);
    m_generateGridAction->setIcon(QIcon(QStringLiteral(":/icons/genGrid.png")));
    connect(m_generateGridAction, &QAction::triggered, this, &MainWindow::onGenerateGrid);

    m_addGridLineXAction = new QAction(tr("Adicionar\nlinha X"), this);
    m_addGridLineXAction->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    connect(m_addGridLineXAction, &QAction::triggered, this, &MainWindow::onAddGridLineX);

    m_addGridLineYAction = new QAction(tr("Adicionar\nlinha Y"), this);
    m_addGridLineYAction->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    connect(m_addGridLineYAction, &QAction::triggered, this, &MainWindow::onAddGridLineY);

    m_addGridLineZAction = new QAction(tr("Adicionar\nlinha Z"), this);
    m_addGridLineZAction->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    connect(m_addGridLineZAction, &QAction::triggered, this, &MainWindow::onAddGridLineZ);

    m_deleteGridLineAction = new QAction(tr("Deletar\nlinha de grid"), this);
    m_deleteGridLineAction->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    connect(m_deleteGridLineAction, &QAction::triggered, this, &MainWindow::onDeleteGridLine);

    m_applyNodalLoadAction = new QAction(tr("Forca\nconcentrada (nos)"), this);
    m_applyNodalLoadAction->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    connect(m_applyNodalLoadAction, &QAction::triggered, this, &MainWindow::onApplyNodalLoad);

    m_applyDistributedLoadAction = new QAction(tr("Distribuida\n(barras)"), this);
    m_applyDistributedLoadAction->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    connect(m_applyDistributedLoadAction, &QAction::triggered, this, &MainWindow::onApplyDistributedLoad);

    m_applyRestraintsAction = new QAction(tr("Restricoes\nnodais"), this);
    m_applyRestraintsAction->setIcon(style()->standardIcon(QStyle::SP_DialogNoButton));
    connect(m_applyRestraintsAction, &QAction::triggered, this, &MainWindow::onApplyRestraints);

    m_createMaterialAction = new QAction(tr("Novo\nmaterial"), this);
    m_createMaterialAction->setIcon(QIcon(QStringLiteral(":/icons/addMaterial.png")));
    connect(m_createMaterialAction, &QAction::triggered, this, &MainWindow::onCreateMaterial);

    m_createSectionAction = new QAction(tr("Nova\nsecao"), this);
    m_createSectionAction->setIcon(QIcon(QStringLiteral(":/icons/addSection.png")));
    connect(m_createSectionAction, &QAction::triggered, this, &MainWindow::onCreateSection);

    m_assignPropertiesAction = new QAction(tr("Atribuir\na barras"), this);
    m_assignPropertiesAction->setIcon(QIcon(QStringLiteral(":/icons/setBars.png")));
    connect(m_assignPropertiesAction, &QAction::triggered, this, &MainWindow::onAssignProperties);

    m_resetCameraAction = new QAction(tr("Visao\ninicial"), this);
    m_resetCameraAction->setIcon(QIcon(QStringLiteral(":/icons/initial.png")));
    connect(m_resetCameraAction, &QAction::triggered, this, &MainWindow::onResetCamera);

    m_zoomExtentsAction = new QAction(tr("Zoom\nextents"), this);
    m_zoomExtentsAction->setIcon(QIcon(QStringLiteral(":/icons/zoomExtents.png")));
    connect(m_zoomExtentsAction, &QAction::triggered, this, &MainWindow::onZoomExtents);

    addAction(m_openModelAction);
    addAction(m_saveModelAction);
    addAction(m_runAnalysisAction);

    m_quickBar = createQuickAccessBar();
    m_quickBar->setMouseTracking(true);
    m_quickBar->installEventFilter(this);
    if (m_titleLabel) {
        m_titleLabel->setMouseTracking(true);
        m_titleLabel->installEventFilter(this);
    }
    if (m_homeTabButton) {
        m_homeTabButton->setMouseTracking(true);
    }
    createRibbon();

    setupCentralLayouts();
    refreshPropertiesPanel();

    m_sceneController->initialize(m_vtkWidget);
    connect(m_selectionModel, &Structura::SelectionModel::selectionChanged, this,
            [this](const QSet<QUuid> &nodes, const QSet<QUuid> &bars) {
                if (!m_sceneController) {
                    return;
                }
                m_sceneController->setSelectedNodes(nodes);
                m_sceneController->setSelectedBars(bars);
                refreshPropertiesPanel();
                updateStatus();
                updateLoadActionsEnabled();
            });
    connect(m_undoService->stack(), &QUndoStack::indexChanged, this, [this](int) {
        refreshPropertiesPanel();
    });

    // Handle screen-click events during insertion
    m_vtkWidget->installEventFilter(this);

    setupFooterBar();

    setCommand(Command::None);

    m_lastDatDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    updateMaximizeButtonIcon();
    updateLoadActionsEnabled();
    syncLoadVisuals();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupCentralLayouts()
{
    auto *centralWidget = new QWidget(this);
    auto *outerLayout = new QVBoxLayout(centralWidget);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    outerLayout->addWidget(m_quickBar);
    outerLayout->addWidget(m_ribbon);

    auto *contentWidget = new QWidget(centralWidget);
    m_contentLayout = new QHBoxLayout(contentWidget);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);

    m_vtkWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_contentLayout->addWidget(m_vtkWidget, 1);

    ensurePropertiesPanel();

    setupRightToolColumn();
    m_contentLayout->addWidget(m_toolColumn, 0);

    outerLayout->addWidget(contentWidget, 1);
    setCentralWidget(centralWidget);
}

void MainWindow::setupRightToolColumn()
{
    if (m_toolColumn) {
        return;
    }

    m_toolColumn = new QWidget(this);
    m_toolColumn->setObjectName(QStringLiteral("ToolColumn"));
    m_toolColumn->setFixedWidth(48);
    m_toolColumn->setStyleSheet(QStringLiteral(
        "#ToolColumn { background: #f2f5fa; border-left: 1px solid #d6dde8; }"
        "#ToolColumn QToolButton { border: none; background: transparent; }"
        "#ToolColumn QToolButton:checked { background: rgba(19, 147, 214, 0.18); border-radius: 6px; }"
        "#ToolColumn QToolButton:hover { background: rgba(19, 147, 214, 0.24); border-radius: 6px; }"
    ));

    auto *layout = new QVBoxLayout(m_toolColumn);
    layout->setContentsMargins(12, 20, 12, 20);
    layout->setSpacing(16);

    m_propertiesToolButton = new QToolButton(m_toolColumn);
    m_propertiesToolButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_propertiesToolButton->setIcon(QIcon(QStringLiteral(":/icons/properties.png")));
    m_propertiesToolButton->setIconSize(QSize(30, 30));
    m_propertiesToolButton->setCheckable(true);
    m_propertiesToolButton->setAutoRaise(false);
    m_propertiesToolButton->setFixedSize(36, 36);
    m_propertiesToolButton->setCursor(Qt::PointingHandCursor);
    m_propertiesToolButton->setToolTip(tr("Propriedades"));
    layout->addWidget(m_propertiesToolButton, 0, Qt::AlignHCenter | Qt::AlignTop);
    
    // Add LCS visualization button
    m_showBarLCSToolButton = new QToolButton(m_toolColumn);
    m_showBarLCSToolButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_showBarLCSToolButton->setText(QStringLiteral("LCS"));
    m_showBarLCSToolButton->setIconSize(QSize(30, 30));
    m_showBarLCSToolButton->setCheckable(true);
    m_showBarLCSToolButton->setAutoRaise(false);
    m_showBarLCSToolButton->setFixedSize(36, 36);
    m_showBarLCSToolButton->setCursor(Qt::PointingHandCursor);
    m_showBarLCSToolButton->setToolTip(tr("Mostrar Eixos Locais (LCS)"));
    layout->addWidget(m_showBarLCSToolButton, 0, Qt::AlignHCenter);
    
    layout->addStretch(1);

    connect(m_propertiesToolButton, &QToolButton::toggled, this, [this](bool checked) {
        ensurePropertiesPanel();
        if (!m_propertiesContainer) {
            return;
        }
        m_propertiesContainer->setVisible(checked);
        if (checked) {
            refreshPropertiesPanel();
        }
    });
    
    connect(m_showBarLCSToolButton, &QToolButton::toggled, this, &MainWindow::onShowBarLCSToggled);
}

QPoint MainWindow::toDisplayPoint(const QPoint &widgetPos) const
{
    return QPoint(widgetPos.x(), m_sceneController->viewportHeight() - 1 - widgetPos.y());
}

QUuid MainWindow::pickNodeAtWidgetPos(const QPoint &widgetPos) const
{
    const QPoint disp = toDisplayPoint(widgetPos);
    return m_sceneController->pickNode(disp.x(), disp.y());
}

QUuid MainWindow::pickBarAtWidgetPos(const QPoint &widgetPos) const
{
    const QPoint disp = toDisplayPoint(widgetPos);
    return m_sceneController->pickBar(disp.x(), disp.y());
}

void MainWindow::updateHighlightedNodeAtWidgetPos(const QPoint &widgetPos)
{
    const QUuid nodeId = pickNodeAtWidgetPos(widgetPos);
    if (!nodeId.isNull()) {
        m_sceneController->setHighlightedNode(nodeId);
    } else {
        m_sceneController->clearHighlightedNode();
    }
}

QVector<PropertiesPanel::NodeEntry> MainWindow::buildNodeEntries(const QSet<QUuid> &nodeIds) const
{
    return Structura::UI::PropertiesPanelEntryBuilder::buildNodeEntries(m_sceneController,
                                                                         nodeIds,
                                                                         m_supports,
                                                                         m_nodalLoads);
}

QVector<PropertiesPanel::BarEntry> MainWindow::buildBarEntries(const QSet<QUuid> &barIds) const
{
    return Structura::UI::PropertiesPanelEntryBuilder::buildBarEntries(m_sceneController,
                                                                        barIds,
                                                                        m_materials,
                                                                        m_sections,
                                                                        m_memberLoads,
                                                                        tr("Sem material"),
                                                                        tr("Sem secao"));
}

bool MainWindow::computeWorldPointForInsert(const QPoint &widgetPos,
                                            double &x,
                                            double &y,
                                            double &z,
                                            bool applySnap) const
{
    if (!m_sceneController) {
        return false;
    }
    const int viewportHeight = m_sceneController->viewportHeight();
    if (viewportHeight <= 0) {
        return false;
    }
    const int displayX = widgetPos.x();
    const int displayY = viewportHeight - 1 - widgetPos.y();
    bool ok = m_sceneController->worldPointOnViewPlane(displayX, displayY, x, y, z);
    if (!ok) {
        ok = m_sceneController->pickWorldPoint(displayX, displayY, x, y, z);
    }
    if (!ok) {
        ok = m_sceneController->worldPointOnPlaneZ0(displayX, displayY, x, y, z);
    }
    if (!ok) {
        return false;
    }
    if (applySnap && m_snapCheck && m_snapCheck->isChecked() && m_sceneController->hasGrid()) {
        m_sceneController->snapToGrid(x, y, z);
    }
    return true;
}

void MainWindow::setHoverInsertPoint(const std::optional<QVector3D> &point)
{
    auto fuzzyEqual = [](const QVector3D &a, const QVector3D &b) {
        const float eps = 1e-3f;
        return qAbs(a.x() - b.x()) < eps
            && qAbs(a.y() - b.y()) < eps
            && qAbs(a.z() - b.z()) < eps;
    };

    bool changed = false;
    if (m_hoverInsertPoint.has_value() != point.has_value()) {
        changed = true;
    } else if (m_hoverInsertPoint && point && !fuzzyEqual(*m_hoverInsertPoint, *point)) {
        changed = true;
    }

    if (!changed) {
        return;
    }

    m_hoverInsertPoint = point;
    updateStatus();
}

bool MainWindow::isGridInsertCommand(Command command) const
{
    return command == Command::AddGridLineX
        || command == Command::AddGridLineY
        || command == Command::AddGridLineZ;
}

int MainWindow::nextMaterialExternalId() const
{
    return Structura::App::Services::IdGeneratorService::nextMaterialExternalId(m_materials);
}

int MainWindow::nextSectionExternalId() const
{
    return Structura::App::Services::IdGeneratorService::nextSectionExternalId(m_sections);
}

int MainWindow::nextBarExternalId() const
{
    std::vector<int> externalIds;
    if (m_sceneController) {
        const auto bars = m_sceneController->bars();
        externalIds.reserve(bars.size());
        for (const auto &bar : bars) {
            externalIds.push_back(bar.externalId);
        }
    }

    return Structura::App::Services::IdGeneratorService::nextBarExternalId(externalIds);
}

void MainWindow::setCommand(Command command)
{
    if (m_command == command) {
        updateStatus();
        return;
    }

    const bool wasBarMode = (m_command == Command::InsertBarFirst || m_command == Command::InsertBarSecond);
    const bool willBeBarMode = (command == Command::InsertBarFirst || command == Command::InsertBarSecond);
    const bool wasGridInsert = isGridInsertCommand(m_command);
    const bool willBeGridInsert = isGridInsertCommand(command);
    const bool wasGridDelete = (m_command == Command::DeleteGridLine);
    const bool willBeGridDelete = (command == Command::DeleteGridLine);

    if (wasBarMode && !willBeBarMode) {
        m_sceneController->clearHighlightedNode();
        m_firstBarNodeId = QUuid();
    }

    if (wasGridInsert && !willBeGridInsert) {
        cancelGridInsert();
    }

    if (wasGridDelete && !willBeGridDelete) {
        hideGridDeleteTooltip();
        if (m_sceneController) {
            m_sceneController->clearHighlightedGridLine();
        }
        m_pendingDeleteLineId = QUuid();
    }

    m_command = command;

    if (!willBeBarMode) {
        m_sceneController->clearHighlightedNode();
        m_firstBarNodeId = QUuid();
    }

    if (willBeGridInsert) {
        beginGridInsert(commandToAxis(command));
    } else if (!willBeGridInsert) {
        m_gridInsertState.active = false;
    }

    if (willBeGridDelete) {
        hideGridDeleteTooltip();
        if (m_sceneController) {
            m_sceneController->hideGridGhostLine();
            m_sceneController->clearHighlightedGridLine();
        }
        m_pendingDeleteLineId = QUuid();
    }

    if (command != Command::InsertNode) {
        m_hoverInsertPoint.reset();
    }

    updateStatus();
}

void MainWindow::onNodeCoordinateEdited(const QVector<QUuid> &ids, char axis, double value)
{
    if (!m_presenter) {
        return;
    }

    m_presenter->handleNodeCoordinateEdited(ids, axis, value);
    refreshPropertiesPanel();
}

void MainWindow::onBarMaterialEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &materialId)
{
    if (!m_presenter) {
        return;
    }

    m_presenter->handleBarMaterialEdited(ids, materialId);
    refreshPropertiesPanel();
}

void MainWindow::onBarSectionEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &sectionId)
{
    if (!m_presenter) {
        return;
    }

    m_presenter->handleBarSectionEdited(ids, sectionId);
    refreshPropertiesPanel();
}






