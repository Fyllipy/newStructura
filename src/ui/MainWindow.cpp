#include "MainWindow.h"

#include "viz/SceneController.h"
#include "app/UndoRedoService.h"
#include "app/services/IdGeneratorService.h"
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
#include <QComboBox>
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
#include <vtkRenderWindow.h>
#include <vtkRendererCollection.h>
#include <vtkRenderer.h>

#include "viz/postprocessor/PostProcessorRenderer.h"
#include "PlacementController.h"
#include "SvgIcons.h"
#include "ProfileLibrary.h"
#include "app/services/MaterialLibrary.h"
#include "app/services/UserLibrary.h"
#include "AxesPanel.h"
#include "SectionPlacementPanel.h"
#include "PlacementOverlay.h"


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
    setWindowTitle(tr("Structura"));

    // Banco de dados do programa: perfis e materiais salvos pelo usuario
    Structura::UI::ProfileLibrary::setUserProfiles(Structura::App::Services::UserLibrary::loadProfiles());
    Structura::App::Services::MaterialLibrary::setUserEntries(Structura::App::Services::UserLibrary::loadMaterials());

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
    presenterDeps.expandIds = [this](const QVector<QUuid> &ids) { return expandWithGroups(ids); };
    m_presenter = new Structura::UI::MainWindowPresenter(presenterDeps, this);

    m_insertBarAction = new QAction(tr("Inserir\nbarra"), this);
    m_insertBarAction->setIcon(Structura::UI::svgIcon(QStringLiteral("bar-insert.svg"), QColor(QStringLiteral("#1e232b")), 64));
    connect(m_insertBarAction, &QAction::triggered, this, &MainWindow::onInsertBar);

    m_insertNodeAction = new QAction(tr("Inserir\nnó"), this);
    m_insertNodeAction->setIcon(Structura::UI::svgIcon(QStringLiteral("node-insert.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_insertNodeAction->setToolTip(tr("Insere um nó com snap ao longo das barras: ao colocar o nó sobre uma barra, ela é dividida nesse ponto"));
    connect(m_insertNodeAction, &QAction::triggered, this, &MainWindow::onInsertNode);

    m_measureAction = new QAction(tr("Medir"), this);
    m_measureAction->setIcon(Structura::UI::svgIcon(QStringLiteral("measure.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_measureAction->setToolTip(tr("Mede a distância entre dois pontos (com snap em nós, centros e ao longo das barras): distância, deltas e inclinação"));
    connect(m_measureAction, &QAction::triggered, this, &MainWindow::onMeasure);

    m_splitBarsAction = new QAction(tr("Dividir\nbarras"), this);
    m_splitBarsAction->setIcon(Structura::UI::svgIcon(QStringLiteral("split-bar.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_splitBarsAction->setToolTip(tr("Divide as barras selecionadas em N partes iguais, a cada X metros ou em distâncias informadas"));
    m_splitBarsAction->setEnabled(false);
    connect(m_splitBarsAction, &QAction::triggered, this, &MainWindow::onSplitBars);

    m_generateTrussAction = new QAction(tr("Gerar\ntreliça"), this);
    m_generateTrussAction->setIcon(Structura::UI::svgIcon(QStringLiteral("truss.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_generateTrussAction->setToolTip(tr("Gera uma treliça plana (Pratt, Howe, Warren, Fink, K; banzos paralelos, duas águas, uma água, arco ou tesoura) a partir de um ponto inicial"));
    connect(m_generateTrussAction, &QAction::triggered, this, &MainWindow::onGenerateTruss);

    m_deleteElementsAction = new QAction(tr("Excluir\nelementos"), this);
    m_deleteElementsAction->setIcon(Structura::UI::svgIcon(QStringLiteral("trash.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_deleteElementsAction->setToolTip(tr("Exclui os nós e barras selecionados (Delete). Barras ligadas a um nó excluído também são removidas"));
    m_deleteElementsAction->setShortcut(Qt::Key_Delete);
    m_deleteElementsAction->setEnabled(false);
    connect(m_deleteElementsAction, &QAction::triggered, this, &MainWindow::onDeleteSelected);

    m_openModelAction = new QAction(tr("Abrir .dat"), this);
    m_openModelAction->setIcon(QIcon(QStringLiteral(":/icons/open.png")));
    m_openModelAction->setShortcut(QKeySequence::Open);
    connect(m_openModelAction, &QAction::triggered, this, &MainWindow::onOpenModel);

    m_newModelAction = new QAction(tr("Novo"), this);
    m_newModelAction->setShortcut(QKeySequence::New);
    connect(m_newModelAction, &QAction::triggered, this, &MainWindow::onNewModel);

    m_saveAsModelAction = new QAction(tr("Salvar como…"), this);
    m_saveAsModelAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsModelAction, &QAction::triggered, this, &MainWindow::onSaveModelAs);

    m_aboutAction = new QAction(tr("Sobre"), this);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAbout);

    m_saveModelAction = new QAction(tr("Salvar .dat"), this);
    m_saveModelAction->setIcon(QIcon(QStringLiteral(":/icons/save.png")));
    m_saveModelAction->setShortcut(QKeySequence::Save);
    connect(m_saveModelAction, &QAction::triggered, this, &MainWindow::onSaveModel);

    m_runAnalysisAction = new QAction(tr("Rodar\nanalise"), this);
    m_runAnalysisAction->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_runAnalysisAction->setShortcut(Qt::Key_F5);
    connect(m_runAnalysisAction, &QAction::triggered, this, &MainWindow::onRunStructuralAnalysis);

    m_applyNodalLoadAction = new QAction(tr("Forca\nconcentrada (nos)"), this);
    m_applyNodalLoadAction->setIcon(Structura::UI::svgIcon(QStringLiteral("force-point.svg"), QColor(QStringLiteral("#1e232b")), 64));
    connect(m_applyNodalLoadAction, &QAction::triggered, this, &MainWindow::onApplyNodalLoad);

    m_applyDistributedLoadAction = new QAction(tr("Distribuida\n(barras)"), this);
    m_applyDistributedLoadAction->setIcon(Structura::UI::svgIcon(QStringLiteral("force-distributed.svg"), QColor(QStringLiteral("#1e232b")), 64));
    connect(m_applyDistributedLoadAction, &QAction::triggered, this, &MainWindow::onApplyDistributedLoad);

    m_copyElementsAction = new QAction(tr("Copiar\nelementos"), this);
    m_copyElementsAction->setIcon(Structura::UI::svgIcon(QStringLiteral("copy.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_copyElementsAction->setEnabled(false);
    connect(m_copyElementsAction, &QAction::triggered, this, &MainWindow::onCopyElements);

    m_moveElementsAction = new QAction(tr("Mover\nelementos"), this);
    m_moveElementsAction->setIcon(Structura::UI::svgIcon(QStringLiteral("move.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_moveElementsAction->setEnabled(false);
    connect(m_moveElementsAction, &QAction::triggered, this, &MainWindow::onMoveElements);

    auto makeEditAction = [this](const QString &text, const QString &icon, void (MainWindow::*slot)()) {
        auto *action = new QAction(text, this);
        action->setIcon(Structura::UI::svgIcon(icon, QColor(QStringLiteral("#1e232b")), 64));
        action->setEnabled(false);
        connect(action, &QAction::triggered, this, slot);
        return action;
    };
    m_serialCopyAction = makeEditAction(tr("Copia em\nserie"), QStringLiteral("serial.svg"), &MainWindow::onSerialCopy);
    m_rotateElementsAction = makeEditAction(tr("Rotacionar\nelementos"), QStringLiteral("rotate.svg"), &MainWindow::onRotateElements);
    m_mirrorElementsAction = makeEditAction(tr("Espelhar\nelementos"), QStringLiteral("mirror.svg"), &MainWindow::onMirrorElements);

    m_applyMemberMomentAction = new QAction(tr("Momento\ndistribuido"), this);
    m_applyMemberMomentAction->setIcon(Structura::UI::svgIcon(QStringLiteral("bar-moment.svg"),
                                                              QColor(QStringLiteral("#1e232b")), 64));
    connect(m_applyMemberMomentAction, &QAction::triggered, this, &MainWindow::onApplyMemberMoment);

    m_loadCasesAction = new QAction(tr("Casos e\ncombinacoes"), this);
    m_loadCasesAction->setIcon(Structura::UI::svgIcon(QStringLiteral("cases.svg"),
                                                      QColor(QStringLiteral("#1e232b")), 64));
    connect(m_loadCasesAction, &QAction::triggered, this, &MainWindow::onEditLoadCases);
    m_genCombinationsAction = new QAction(tr("Gerar\ncombinacoes"), this);
    m_genCombinationsAction->setIcon(Structura::UI::svgIcon(QStringLiteral("combinations.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_genCombinationsAction->setToolTip(tr("Gera automaticamente as combinações ELU e ELS (NBR 8681 / NBR 8800) a partir dos casos de carga"));
    connect(m_genCombinationsAction, &QAction::triggered, this, &MainWindow::onGenerateCombinations);
    m_bucklingAction = new QAction(tr("Flambagem\nelastica"), this);
    m_bucklingAction->setIcon(Structura::UI::svgIcon(QStringLiteral("buckling.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_bucklingAction->setToolTip(tr("Fatores críticos de carga e modos de flambagem elástica linear de uma combinação"));
    connect(m_bucklingAction, &QAction::triggered, this, &MainWindow::onBucklingAnalysis);
    m_dynamicsAction = new QAction(tr("Dinamica"), this);
    m_dynamicsAction->setIcon(Structura::UI::svgIcon(QStringLiteral("dynamics.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_dynamicsAction->setToolTip(tr("Espectro de resposta (SRSS/CQC), vibração de pisos (NBR 8800, Anexo I) e resposta no tempo"));
    connect(m_dynamicsAction, &QAction::triggered, this, &MainWindow::onDynamics);
    m_modalAction = new QAction(tr("Analise\nmodal"), this);
    m_modalAction->setIcon(Structura::UI::svgIcon(QStringLiteral("modal.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_modalAction->setToolTip(tr("Frequências naturais, períodos, participação de massa e formas modais"));
    connect(m_modalAction, &QAction::triggered, this, &MainWindow::onModalAnalysis);
    m_connectionsAction = new QAction(tr("Ligacoes"), this);
    m_connectionsAction->setIcon(Structura::UI::svgIcon(QStringLiteral("connection.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_connectionsAction->setToolTip(tr("Verifica grupos de parafusos e filetes de solda (NBR 8800:2024, 6.2 e 6.3), com esforços da barra selecionada"));
    connect(m_connectionsAction, &QAction::triggered, this, &MainWindow::onCheckConnections);
    m_quantitiesAction = new QAction(tr("Quantitativos"), this);
    m_quantitiesAction->setIcon(Structura::UI::svgIcon(QStringLiteral("quantities.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_quantitiesAction->setToolTip(tr("Lista de materiais por perfil: comprimento, massa e área de pintura"));
    connect(m_quantitiesAction, &QAction::triggered, this, &MainWindow::onShowQuantities);
    m_exportDxfAction = new QAction(tr("Exportar\nDXF"), this);
    m_exportDxfAction->setIcon(Structura::UI::svgIcon(QStringLiteral("dxf.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_exportDxfAction->setToolTip(tr("Exporta os eixos das barras em DXF (3D, planta ou elevações), uma camada por perfil"));
    connect(m_exportDxfAction, &QAction::triggered, this, &MainWindow::onExportDxf);
    m_sheetsAction = new QAction(tr("Pranchas"), this);
    m_sheetsAction->setIcon(Structura::UI::svgIcon(QStringLiteral("sheets.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_sheetsAction->setToolTip(tr("Gera plantas, elevações com cotas, lista de barras e detalhes de ligações; exporta PDF e DXF"));
    connect(m_sheetsAction, &QAction::triggered, this, &MainWindow::onGenerateDrawings);

    m_applyRestraintsAction = new QAction(tr("Restricoes\nnodais"), this);
    m_applyRestraintsAction->setIcon(Structura::UI::svgIcon(QStringLiteral("restraint.svg"), QColor(QStringLiteral("#1e232b")), 64));
    connect(m_applyRestraintsAction, &QAction::triggered, this, &MainWindow::onApplyRestraints);

    m_createMaterialAction = new QAction(tr("Novo\nmaterial"), this);
    m_createMaterialAction->setIcon(Structura::UI::svgIcon(QStringLiteral("material-new.svg"), QColor(QStringLiteral("#1e232b")), 64));
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

    m_profileManagerAction = new QAction(tr("Gerenciador\nde perfis"), this);
    m_profileManagerAction->setIcon(Structura::UI::svgIcon(QStringLiteral("section.svg"),
                                                           QColor(QStringLiteral("#0d7fb9")), 64));
    connect(m_profileManagerAction, &QAction::triggered, this, &MainWindow::onOpenProfileManager);

    m_axesAction = new QAction(tr("Eixos de\nreferencia"), this);
    m_axesAction->setIcon(Structura::UI::svgIcon(QStringLiteral("axes-config.svg"),
                                                  QColor(QStringLiteral("#0d7fb9")), 64));
    connect(m_axesAction, &QAction::triggered, this, &MainWindow::onToggleAxesPanel);

    m_placementAction = new QAction(tr("Posicionar\nseção"), this);
    m_placementAction->setIcon(Structura::UI::svgIcon(QStringLiteral("section-place.svg"),
                                                      QColor(QStringLiteral("#0d7fb9")), 64));
    m_placementAction->setToolTip(tr("Posicionar a seção em relação ao nó: giro, ponto de referência, excentricidade e articulações"));
    connect(m_placementAction, &QAction::triggered, this, &MainWindow::onTogglePlacementPanel);

    addAction(m_openModelAction);
    addAction(m_saveModelAction);
    addAction(m_saveAsModelAction);
    addAction(m_deleteElementsAction);
    addAction(m_newModelAction);
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

    if (m_postProcessor && m_vtkWidget->renderWindow() && m_vtkWidget->renderWindow()->GetRenderers()->GetFirstRenderer()) {
        m_postProcessor->initialize(m_vtkWidget->renderWindow()->GetRenderers()->GetFirstRenderer());
    }

    connect(m_selectionModel, &Structura::SelectionModel::selectionChanged, this,
            [this](const QSet<QUuid> &nodes, const QSet<QUuid> &bars) {
                if (!m_sceneController) {
                    return;
                }
                m_sceneController->setSelectedNodes(nodes);
                m_sceneController->setSelectedBars(bars);
                
                if (m_postProcessor) {
                    std::vector<int> activeIds;
                    activeIds.reserve(bars.size());
                    for (const auto& barId : bars) {
                        if (const auto* bar = m_sceneController->findBar(barId)) {
                            activeIds.push_back(bar->externalId());
                        }
                    }
                    m_postProcessor->setActiveElementIds(activeIds);
                    
                    // Se o modo for SelectedElements, redesenha
                    if (m_scaleModeCombo && 
                        m_scaleModeCombo->currentData().toInt() == static_cast<int>(Structura::Visualization::DiagramScaleMode::SelectedElements)) {
                        if (m_diagramCombo && m_postProcessor->isActive()) {
                            onDiagramTypeChanged(m_diagramCombo->currentIndex());
                        }
                    }
                }

                refreshPropertiesPanel();
                updateStatus();
                updateLoadActionsEnabled();
                updateEditActionsEnabled();
            });
    setupSessionServices();
    connect(m_undoService->stack(), &QUndoStack::indexChanged, this, [this](int) {
        refreshPropertiesPanel();
        syncLoadVisuals();
        syncSupportVisuals();
        syncPlacementVisuals();
    });

    // Handle screen-click events during insertion
    m_vtkWidget->installEventFilter(this);

    setupFooterBar();
    setupPlacement();

    setCommand(Command::None);

    m_lastDatDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    updateMaximizeButtonIcon();
    updateLoadActionsEnabled();
    syncLoadVisuals();
    ensureDefaultMaterial();
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
    // os botoes da antiga barra lateral (propriedades, LCS, grupos, eixos) ficam no painel flutuante da viewport (ver updatePlaneLabel)
    m_toolColumn->hide();

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
    m_propertiesToolButton->setObjectName(QStringLiteral("propertiesToolButton"));
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
        positionPlaneLabel();
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
    auto entries = Structura::UI::PropertiesPanelEntryBuilder::buildBarEntries(m_sceneController,
                                                                        barIds,
                                                                        m_materials,
                                                                        m_sections,
                                                                        m_memberLoads,
                                                                        tr("Sem material"),
                                                                        tr("Sem secao"),
                                                                        m_extras.placements);
    if (entries.size() == 1) {
        auto &entry = entries.first();
        const int id = entry.externalId;
        auto fmt = [](double v) { return QString::number(v, 'g', 5); };
        QStringList lines;
        auto caseName = [this](const QString &name) { return name.isEmpty() ? Structura::App::ProjectData::defaultLoadCaseName() : name; };
        for (const auto &c : m_extras.loadCases) {
            if (c.selfWeight) {
                lines << tr("<b>%1</b>: peso próprio").arg(c.name.toHtmlEscaped());
            }
        }
        for (const auto &l : m_memberLoads) {
            if (l.memberId == id) {
                lines << tr("<b>%1</b>: q (%2) = [%3; %4; %5] kN/m").arg(caseName(l.loadCase).toHtmlEscaped(), l.system.isEmpty() ? tr("global") : l.system.toLower(), fmt(l.qx), fmt(l.qy), fmt(l.qz));
            }
        }
        for (const auto &m : m_extras.memberMoments) {
            if (m.memberId == id) {
                lines << tr("<b>%1</b>: momento (%2) = [%3; %4; %5] kN·m/m").arg(caseName(m.loadCase).toHtmlEscaped(), m.system.isEmpty() ? tr("global") : m.system.toLower(), fmt(m.mx), fmt(m.my), fmt(m.mz));
            }
        }
        for (const auto &p : m_extras.areaLoads) {
            if (p.members.contains(id)) {
                lines << tr("<b>%1</b>: %2 de %3 kN/m² (%4)").arg(caseName(p.loadCase).toHtmlEscaped(), p.kind == QLatin1String("WIND") ? tr("vento") : tr("carga de área"), fmt(p.pressure), p.description.toHtmlEscaped());
            }
        }
        const bool anyNodal = false;
        Q_UNUSED(anyNodal)
        if (!lines.isEmpty()) {
            entry.loadDetails = tr("<b>Carregamentos atuantes:</b><br>") + lines.join(QStringLiteral("<br>"));
        } else {
            entry.loadDetails = tr("<i>Nenhum carregamento aplicado diretamente nesta barra.</i>");
        }
    }
    return entries;
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
    const bool willBeShellMode = command == Command::ShellPolygon || command == Command::ShellRectangle || command == Command::ShellDisk
        || command == Command::ShellExtrude || command == Command::ShellCylinder || command == Command::ShellRevolve || command == Command::ShellMove
        || command == Command::ShellCopy;
    const bool willBeEditMode = (command == Command::CopyElements || command == Command::MoveElements
                                 || command == Command::RotateElements || command == Command::MirrorElements
                                 || command == Command::InsertNode || command == Command::Measure || willBeShellMode);
    m_shellDraft = ShellDraft {};

    if (wasBarMode && !willBeBarMode) {
        m_sceneController->clearHighlightedNode();
        m_firstBarNodeId = QUuid();
    }

    m_command = command;
    if (m_measureCard && command != Command::Measure) {
        m_measureCard->hide();
    }

    if (m_placement) {
        if (willBeBarMode) {
            m_placement->start(Structura::UI::PlacementController::Mode::Bar);
        } else if (command == Command::CopyElements) {
            m_placement->start(Structura::UI::PlacementController::Mode::Copy);
        } else if (command == Command::MoveElements) {
            m_placement->start(Structura::UI::PlacementController::Mode::Move);
        } else if (command == Command::RotateElements) {
            m_placement->start(Structura::UI::PlacementController::Mode::Rotate);
        } else if (command == Command::MirrorElements) {
            m_placement->start(Structura::UI::PlacementController::Mode::Mirror);
        } else if (command == Command::InsertNode) {
            m_placement->start(Structura::UI::PlacementController::Mode::Node);
        } else if (command == Command::Measure) {
            m_placement->start(Structura::UI::PlacementController::Mode::Measure);
        } else if (command == Command::ShellPolygon || command == Command::ShellExtrude || command == Command::ShellRevolve) {
            m_placement->start(Structura::UI::PlacementController::Mode::Polyline);
        } else if (command == Command::ShellRectangle) {
            m_placement->start(Structura::UI::PlacementController::Mode::Rectangle);
        } else if (command == Command::ShellDisk || command == Command::ShellCylinder) {
            m_placement->start(Structura::UI::PlacementController::Mode::Circle);
        } else if (command == Command::ShellMove) {
            m_placement->start(Structura::UI::PlacementController::Mode::Move);
        } else if (command == Command::ShellCopy) {
            m_placement->start(Structura::UI::PlacementController::Mode::Copy);
        } else {
            m_placement->stop();
        }
    }

    if (!willBeBarMode) {
        m_sceneController->clearHighlightedNode();
        m_firstBarNodeId = QUuid();
    }

    if (!willBeBarMode && !willBeEditMode) {
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

    std::optional<QUuid> resolved = sectionId;
    if (sectionId) {
        resolved = resolveSectionId(*sectionId);
    }
    m_presenter->handleBarSectionEdited(ids, resolved);
    refreshPropertiesPanel();
}






