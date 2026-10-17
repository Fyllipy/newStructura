#include "MainWindow.h"
#include "SvgIcons.h"

#include "viz/postprocessor/PostProcessorRenderer.h"

#include <QAction>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QSize>
#include <QSizePolicy>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

QWidget *MainWindow::createQuickAccessBar()
{
    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("QuickAccessBar"));
    bar->setFixedHeight(30);
    bar->setStyleSheet(R"(
        #QuickAccessBar { background: #0d7fb9; border-bottom: none; }
        #QuickAccessBar QToolButton {background: transparent; border: none; color: #eef5fb; padding: 2px 4px; margin: 0px;}
        #QuickAccessBar QToolButton:hover { background: rgba(255,255,255,0.18); border-radius: 3px; }
        #QuickAccessBar QToolButton[startButton="true"] { font-weight: 600; padding: 2px 8px; margin-left: 6px; border-radius: 3px; }
        #QuickAccessBar QToolButton[startButton="true"]::menu-indicator { image: none; width: 0px; }
        #QuickAccessBar QToolButton[startButton="true"]:hover { background: rgba(255,255,255,0.24); }
        #QuickAccessBar QToolButton[startButton="true"]:checked { background: rgba(255,255,255,0.34); }  /* use :checked em vez de [checked='true'] */
        #QuickAccessBar QToolButton[systemButton="true"] { padding: 2px; margin-left: 2px; margin-right: 0px; border-radius: 3px; }
        #QuickAccessBar QToolButton[systemButton="true"]:hover { background: rgba(255,255,255,0.25); }
        #QuickAccessBar QToolButton[systemButton="true"]:pressed { background: rgba(0,0,0,0.30); }
        #QuickAccessBar QToolButton[systemButton="true"]:last-child:hover { background: #d64545; }
        #QuickAccessBar QLabel { color: #eef5fb; font-weight: 600; }
        )");

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(8, 2, 8, 2);
    layout->setSpacing(4);

    auto *leftWidget = new QWidget(bar);
    leftWidget->setProperty("dragRegion", true);
    leftWidget->setMouseTracking(true);
    leftWidget->installEventFilter(this);
    leftWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    auto *leftLayout = new QHBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(4);

    auto createQuickButton = [bar](QAction *action) {
        auto *button = new QToolButton(bar);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setIconSize(QSize(14, 14));
        button->setAutoRaise(false);
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedSize(22, 22);
        return button;
    };

    if (m_undoAction) {
        leftLayout->addWidget(createQuickButton(m_undoAction));
    }
    if (m_redoAction) {
        leftLayout->addWidget(createQuickButton(m_redoAction));
    }

    leftLayout->addWidget(createQuickButton(m_openModelAction));
    leftLayout->addWidget(createQuickButton(m_saveModelAction));

    m_homeTabButton = new QToolButton(bar);
    m_homeTabButton->setText(tr("Arquivo"));
    m_homeTabButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_homeTabButton->setPopupMode(QToolButton::InstantPopup);
    auto *fileMenu = new QMenu(m_homeTabButton);
    fileMenu->addAction(m_newModelAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveModelAction);
    fileMenu->addAction(m_saveAsModelAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_aboutAction);
    m_homeTabButton->setMenu(fileMenu);
    m_homeTabButton->setProperty("startButton", QStringLiteral("true"));
    m_homeTabButton->setCursor(Qt::PointingHandCursor);
    m_homeTabButton->setFixedHeight(22);
    leftLayout->addWidget(m_homeTabButton);

    layout->addWidget(leftWidget, 0, Qt::AlignVCenter);

    layout->addStretch(1);
    m_titleLabel = new QLabel(tr("Structura 3D"), bar);
    connect(this, &QWidget::windowTitleChanged, m_titleLabel, &QLabel::setText);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(m_titleLabel, 0, Qt::AlignCenter);
    layout->addStretch(1);

    auto createSystemButton = [bar](const QIcon &icon, const QString &name) {
        auto *button = new QToolButton(bar);
        button->setIcon(icon);
        button->setToolTip(name);
        button->setProperty("systemButton", QStringLiteral("true"));
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setAutoRaise(false);
        button->setIconSize(QSize(12, 12));
        button->setFixedSize(26, 22);
        button->setCursor(Qt::ArrowCursor);
        return button;
    };

    m_minimizeButton = createSystemButton(style()->standardIcon(QStyle::SP_TitleBarMinButton), tr("Minimizar"));
    m_maximizeButton = createSystemButton(style()->standardIcon(QStyle::SP_TitleBarMaxButton), tr("Maximizar"));
    m_closeButton = createSystemButton(style()->standardIcon(QStyle::SP_TitleBarCloseButton), tr("Fechar"));

    layout->addWidget(m_minimizeButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(m_maximizeButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(m_closeButton, 0, Qt::AlignRight | Qt::AlignVCenter);

    connect(m_minimizeButton, &QToolButton::clicked, this, &MainWindow::showMinimized);
    connect(m_maximizeButton, &QToolButton::clicked, this, &MainWindow::toggleMaximized);
    connect(m_closeButton, &QToolButton::clicked, this, &MainWindow::close);

    return bar;
}

void MainWindow::createRibbon()
{
    m_ribbon->setObjectName("mainRibbon");
    m_ribbon->setDocumentMode(true);
    m_ribbon->setTabPosition(QTabWidget::North);
    m_ribbon->setMovable(false);
    m_ribbon->tabBar()->setExpanding(false);
    // Ribbon styling inspired by Solid Edge palette
    m_ribbon->setStyleSheet(R"(
        QTabWidget::pane { border: 0; background: #f2f5fa; top: 0px; }
        QGroupBox {background: transparent;
        border: none; margin-top: 12px; color: #1e232b;}
        QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 4px; font-weight: 600; color: #0b6da1; }
        QToolButton { background: transparent; border: none; padding: 0px; color: #1e232b;
        min-height: 34px; min-width: 60px; icon-size: 28px;}
        QToolButton:hover {background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f0f5fa, stop:1 #dbe3ef);}
        QToolButton:pressed {background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #d6dde8, stop:1 #c5ccd8);}
        QCheckBox { color: #1e232b; padding-left: 2px; }
    )");

    if (auto *tabBar = m_ribbon->tabBar()) {
    tabBar->setDrawBase(false);
    tabBar->setAutoFillBackground(true);
    tabBar->setStyleSheet(R"(
        QTabBar { background: #0d7fb9; border: none; padding: 2px 6px; margin: 0; min-height: 26px; }
        QTabBar::tab { background: transparent; color: #eaf3f9; border: none; padding: 0 12px; margin: 0 8px; font-weight: 600; }
        QTabBar::tab:selected { color: #f2992e; border-bottom: 2px solid #f2992e; margin-bottom: -2px; }
        QTabBar::tab:hover { border-bottom: 2px solid rgba(242,153,46,0.55); margin-bottom: -2px; }
    )");
    }

    auto *homeTab = new QWidget(this);
    homeTab->setObjectName(QStringLiteral("RibbonPage"));
    homeTab->setStyleSheet(QStringLiteral("#RibbonPage { background: #f2f5fa; }"));


    auto *homeLayout = new QHBoxLayout(homeTab);
    homeLayout->setContentsMargins(4, 20, 4, 3);  // 6–10 px funciona bem
    //homeLayout->setContentsMargins(4, 1, 4, 3);
    homeLayout->setSpacing(4);

    const QList<QAction *> viewActions = { m_resetCameraAction, m_zoomExtentsAction };

    auto *modelGroup = new QGroupBox(tr("Modelagem"), this);
    auto *modelGrid = new QGridLayout(modelGroup);
    modelGrid->setContentsMargins(4, 5, 4, 4);
    modelGrid->setHorizontalSpacing(30);
    modelGrid->setVerticalSpacing(15);
    modelGrid->setAlignment(Qt::AlignTop);
    const QList<QAction *> modelActions = { m_insertBarAction, m_insertNodeAction, m_splitBarsAction, m_generateTrussAction, m_measureAction, m_applyRestraintsAction };
    const int modelColumns = 2;
    populateActionGrid(modelGrid, modelActions, modelActions.size());
    const int modelRows = (modelActions.count() + modelColumns - 1) / modelColumns;
    modelGrid->setRowStretch(modelRows, 1);
    homeLayout->addWidget(modelGroup, 0, Qt::AlignTop);

    auto *propGroup = new QGroupBox(tr("Propriedades"), this);
    auto *propGrid = new QGridLayout(propGroup);
    propGrid->setContentsMargins(4, 5, 4, 4);
    propGrid->setHorizontalSpacing(30);
    propGrid->setVerticalSpacing(15);
    propGrid->setAlignment(Qt::AlignTop);
    populateActionGrid(propGrid, { m_createMaterialAction, m_createSectionAction, m_profileManagerAction, m_assignPropertiesAction, m_placementAction }, 5);
    propGrid->setRowStretch(3, 1);
    homeLayout->addWidget(propGroup, 0, Qt::AlignTop);

    auto *editGroup = new QGroupBox(tr("Edicao"), this);
    auto *editGrid = new QGridLayout(editGroup);
    editGrid->setContentsMargins(4, 5, 4, 4);
    editGrid->setHorizontalSpacing(30);
    editGrid->setVerticalSpacing(15);
    editGrid->setAlignment(Qt::AlignTop);
    m_createGroupAction = new QAction(tr("Criar\ngrupo"), this);
    m_createGroupAction->setIcon(Structura::UI::svgIcon(QStringLiteral("group.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_createGroupAction->setToolTip(tr("Cria um grupo com as barras selecionadas: perfil, posição e rotação são compartilhados"));
    connect(m_createGroupAction, &QAction::triggered, this, &MainWindow::onCreateGroup);
    m_groupsPanelAction = new QAction(tr("Grupos"), this);
    m_groupsPanelAction->setIcon(Structura::UI::svgIcon(QStringLiteral("groups.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_groupsPanelAction->setToolTip(tr("Painel de grupos: exibir/ocultar, adicionar e remover barras"));
    connect(m_groupsPanelAction, &QAction::triggered, this, &MainWindow::onToggleGroupsPanel);
    const QList<QAction *> editActions = { m_copyElementsAction, m_moveElementsAction, m_serialCopyAction, m_rotateElementsAction, m_mirrorElementsAction, m_deleteElementsAction, m_createGroupAction, m_groupsPanelAction };
    populateActionGrid(editGrid, editActions, editActions.size());
    editGrid->setRowStretch(1, 1);
    homeLayout->addWidget(editGroup, 0, Qt::AlignTop);

    auto *typeGroup = new QGroupBox(tr("Tipo de barra"), this);
    auto *typeGrid = new QGridLayout(typeGroup);
    typeGrid->setContentsMargins(4, 5, 4, 4);
    typeGrid->setHorizontalSpacing(30);
    typeGrid->setVerticalSpacing(15);
    typeGrid->setAlignment(Qt::AlignTop);
    struct TypeButton {
        const char *text;
        const char *icon;
        const char *tip;
        int op;
    };
    const TypeButton typeButtons[] = {
        {"Rotular\ninício", "hinge-start.svg", "Articula (libera as rotações de flexão) a extremidade inicial das barras selecionadas", 0},
        {"Rotular\nfim", "hinge-end.svg", "Articula a extremidade final das barras selecionadas", 1},
        {"Bi-\nrotulada", "hinge-both.svg", "Articula as duas extremidades", 2},
        {"Engastar\nextremidades", "fixed-ends.svg", "Remove as articulações (extremidades rígidas)", 3},
        {"Treliça", "truss.svg", "Barra de treliça: biarticulada, trabalha só a esforço axial (tração e compressão)", 4},
        {"Tirante", "tie.svg", "Tirante: biarticulado e só tração — a barra é desativada quando encurta (análise iterativa)", 5},
    };
    QList<QAction *> typeActions;
    for (const auto &t : typeButtons) {
        auto *action = new QAction(tr(t.text), this);
        action->setIcon(Structura::UI::svgIcon(QString::fromLatin1(t.icon), QColor(QStringLiteral("#1e232b")), 64));
        action->setToolTip(tr(t.tip));
        const int op = t.op;
        connect(action, &QAction::triggered, this, [this, op]() { applyBarTypeOperation(op); });
        typeActions.append(action);
    }
    populateActionGrid(typeGrid, typeActions, typeActions.size());
    typeGrid->setRowStretch(1, 1);
    homeLayout->addWidget(typeGroup, 0, Qt::AlignTop);

    homeLayout->addStretch(1);

    auto *toolsTab = new QWidget(this);
    toolsTab->setObjectName(QStringLiteral("RibbonPage"));
    toolsTab->setStyleSheet(QStringLiteral("#RibbonPage { background: #f2f5fa; }"));

    auto *toolsLayout = new QHBoxLayout(toolsTab);
    toolsLayout->setContentsMargins(4, 20, 4, 3);
    toolsLayout->setSpacing(4);

    auto *axesGroup = new QGroupBox(tr("Eixos"), this);
    auto *axesLayout = new QGridLayout(axesGroup);
    axesLayout->setContentsMargins(4, 5, 4, 4);
    axesLayout->setHorizontalSpacing(30);
    axesLayout->setVerticalSpacing(15);
    axesLayout->setAlignment(Qt::AlignTop);
    populateActionGrid(axesLayout, { m_axesAction }, 1);
    axesLayout->setRowStretch(1, 1);
    toolsLayout->addWidget(axesGroup, 0, Qt::AlignTop);

    auto *checkGroup = new QGroupBox(tr("Modelo"), this);
    auto *checkLayout = new QGridLayout(checkGroup);
    checkLayout->setContentsMargins(4, 5, 4, 4);
    checkLayout->setHorizontalSpacing(30);
    checkLayout->setVerticalSpacing(15);
    checkLayout->setAlignment(Qt::AlignTop);
    m_checkModelAction = new QAction(tr("Checar\nmodelo"), this);
    m_checkModelAction->setIcon(Structura::UI::svgIcon(QStringLiteral("model-check.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_checkModelAction->setToolTip(tr("Procura problemas antes de analisar: nós soltos, barras duplicadas, mecanismos, apoios insuficientes, cargas e unidades"));
    connect(m_checkModelAction, &QAction::triggered, this, &MainWindow::onCheckModel);
    m_templateAction = new QAction(tr("Modelo\npronto"), this);
    m_templateAction->setIcon(Structura::UI::svgIcon(QStringLiteral("template.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_templateAction->setToolTip(tr("Gera um galpão de duas águas ou um edifício de pórticos parametrizado (perfis, grupos, tirantes, cargas e combinações)"));
    connect(m_templateAction, &QAction::triggered, this, &MainWindow::onNewFromTemplate);
    populateActionGrid(checkLayout, { m_templateAction, m_checkModelAction }, 2);
    toolsLayout->addWidget(checkGroup, 0, Qt::AlignTop);

    auto *docsGroup = new QGroupBox(tr("Documentação"), this);
    auto *docsLayout = new QGridLayout(docsGroup);
    docsLayout->setContentsMargins(4, 5, 4, 4);
    docsLayout->setHorizontalSpacing(30);
    docsLayout->setVerticalSpacing(15);
    docsLayout->setAlignment(Qt::AlignTop);
    populateActionGrid(docsLayout, { m_quantitiesAction, m_exportDxfAction, m_sheetsAction }, 3);
    toolsLayout->addWidget(docsGroup, 0, Qt::AlignTop);
    toolsLayout->addStretch(1);

    auto *loadsTab = new QWidget(this);
    loadsTab->setObjectName(QStringLiteral("RibbonPage"));
    loadsTab->setStyleSheet(QStringLiteral("#RibbonPage { background: #f2f5fa; }"));

    auto *loadsLayout = new QHBoxLayout(loadsTab);
    loadsLayout->setContentsMargins(4, 20, 4, 3);
    loadsLayout->setSpacing(4);

    auto *loadsGroup = new QGroupBox(tr("Carregamentos"), this);
    auto *loadsGrid = new QGridLayout(loadsGroup);
    loadsGrid->setContentsMargins(4, 5, 4, 4);
    loadsGrid->setHorizontalSpacing(4);
    loadsGrid->setVerticalSpacing(4);
    loadsGrid->setAlignment(Qt::AlignTop);
    m_areaLoadAction = new QAction(tr("Carga de\nárea"), this);
    m_areaLoadAction->setIcon(Structura::UI::svgIcon(QStringLiteral("area-load.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_areaLoadAction->setToolTip(tr("Distribui uma pressão (kN/m²) nas barras selecionadas pela largura de influência"));
    connect(m_areaLoadAction, &QAction::triggered, this, &MainWindow::onApplyAreaLoad);
    m_windLoadAction = new QAction(tr("Vento\nNBR 6123"), this);
    m_windLoadAction->setIcon(Structura::UI::svgIcon(QStringLiteral("wind.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_windLoadAction->setToolTip(tr("Gera a carga de vento de uma face (q = 0,613 Vk²) nas barras selecionadas"));
    connect(m_windLoadAction, &QAction::triggered, this, &MainWindow::onApplyWindLoad);
    m_deleteLoadsAction = new QAction(tr("Excluir\ncargas"), this);
    m_deleteLoadsAction->setIcon(Structura::UI::svgIcon(QStringLiteral("delete-loads.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_deleteLoadsAction->setToolTip(tr("Exclui cargas por tipo, caso de carga e abrangência (seleção ou modelo inteiro)"));
    connect(m_deleteLoadsAction, &QAction::triggered, this, &MainWindow::onDeleteLoads);
    populateActionGrid(loadsGrid, { m_applyNodalLoadAction, m_applyDistributedLoadAction, m_applyMemberMomentAction, m_areaLoadAction, m_windLoadAction, m_deleteLoadsAction }, 6);
    loadsLayout->addWidget(loadsGroup, 0, Qt::AlignTop);

    auto *casesGroup = new QGroupBox(tr("Casos e combinações"), this);
    auto *casesGrid = new QGridLayout(casesGroup);
    casesGrid->setContentsMargins(4, 5, 4, 4);
    casesGrid->setAlignment(Qt::AlignTop);
    populateActionGrid(casesGrid, { m_loadCasesAction, m_genCombinationsAction }, 2);
    loadsLayout->addWidget(casesGroup, 0, Qt::AlignTop);

    auto *analysisGroup = new QGroupBox(tr("Análise"), this);
    auto *analysisGrid = new QGridLayout(analysisGroup);
    analysisGrid->setContentsMargins(4, 5, 4, 4);
    analysisGrid->setAlignment(Qt::AlignTop);
    populateActionGrid(analysisGrid, { m_runAnalysisAction, m_bucklingAction, m_modalAction, m_dynamicsAction }, 4);
    loadsLayout->addWidget(analysisGroup, 0, Qt::AlignTop);
    loadsLayout->addStretch(1);

    // Visualization tab
    auto *visualizationTab = new QWidget(this);
    visualizationTab->setObjectName(QStringLiteral("RibbonPage"));
    visualizationTab->setStyleSheet(QStringLiteral("#RibbonPage { background: #f2f5fa; }"));

    auto *visualizationLayout = new QHBoxLayout(visualizationTab);
    visualizationLayout->setContentsMargins(4, 20, 4, 3);
    visualizationLayout->setSpacing(4);

    auto *viewGroup = new QGroupBox(tr("Visualizacao"), this);
    auto *viewGrid = new QGridLayout(viewGroup);
    viewGrid->setContentsMargins(4, 5, 4, 4);
    viewGrid->setHorizontalSpacing(30);
    viewGrid->setVerticalSpacing(15);
    viewGrid->setAlignment(Qt::AlignTop);
    populateActionGrid(viewGrid, viewActions, viewActions.size());
    viewGrid->setRowStretch(1, 1);
    visualizationLayout->addWidget(viewGroup, 0, Qt::AlignTop);
    visualizationLayout->addStretch(1);

    m_ribbon->addTab(homeTab, tr("Inicio"));
    m_ribbon->addTab(toolsTab, tr("Ferramentas"));
    m_ribbon->addTab(loadsTab, tr("Carregamentos"));
    m_ribbon->addTab(visualizationTab, tr("Visualizacao"));

    // ── Aba Resultados ─────────────────────────────────────────────────
    m_showDiagramsAction = new QAction(tr("Mostrar\nDiagramas"), this);
    m_showDiagramsAction->setIcon(Structura::UI::svgIcon(QStringLiteral("diagrams-show.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_showDiagramsAction->setEnabled(false); // habilitado após análise
    connect(m_showDiagramsAction, &QAction::triggered, this, &MainWindow::onShowDiagrams);

    m_hideDiagramsAction = new QAction(tr("Ocultar\nDiagramas"), this);
    m_hideDiagramsAction->setIcon(Structura::UI::svgIcon(QStringLiteral("diagrams-hide.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_hideDiagramsAction->setEnabled(false);
    connect(m_hideDiagramsAction, &QAction::triggered, this, &MainWindow::onHideDiagrams);

    m_compareAction = new QAction(tr("Comparar\ncombinações"), this);
    m_compareAction->setIcon(Structura::UI::svgIcon(QStringLiteral("compare.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_compareAction->setToolTip(tr("Compara deslocamentos, reações e esforços entre combinações, com a envoltória e a combinação que governa"));
    connect(m_compareAction, &QAction::triggered, this, &MainWindow::onCompareResults);

    auto *resultsTab = new QWidget(this);
    resultsTab->setObjectName(QStringLiteral("RibbonPage"));
    resultsTab->setStyleSheet(QStringLiteral("#RibbonPage { background: #f2f5fa; }"));

    auto *resultsLayout = new QHBoxLayout(resultsTab);
    resultsLayout->setContentsMargins(4, 20, 4, 3);
    resultsLayout->setSpacing(4);

    auto *diagramsGroup = new QGroupBox(tr("Diagramas"), this);
    auto *diagramsGrid = new QGridLayout(diagramsGroup);
    diagramsGrid->setContentsMargins(4, 5, 4, 4);
    diagramsGrid->setHorizontalSpacing(30);
    diagramsGrid->setVerticalSpacing(15);
    diagramsGrid->setAlignment(Qt::AlignTop);
    populateActionGrid(diagramsGrid, { m_showDiagramsAction, m_hideDiagramsAction }, 2);
    resultsLayout->addWidget(diagramsGroup, 0, Qt::AlignTop);
    auto *compareGroup = new QGroupBox(tr("Comparação"), this);
    auto *compareGrid = new QGridLayout(compareGroup);
    compareGrid->setContentsMargins(4, 5, 4, 4);
    compareGrid->setHorizontalSpacing(30);
    compareGrid->setVerticalSpacing(15);
    compareGrid->setAlignment(Qt::AlignTop);
    populateActionGrid(compareGrid, { m_compareAction }, 1);
    resultsLayout->addWidget(compareGroup, 0, Qt::AlignTop);
    resultsLayout->addStretch(1);

    m_ribbon->addTab(resultsTab, tr("Resultados"));

    // ── Aba Dimensionamento ────────────────────────────────────────────
    m_compositeAction = new QAction(tr("Vigas e lajes\nmistas"), this);
    m_compositeAction->setIcon(Structura::UI::svgIcon(QStringLiteral("composite.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_compositeAction->setToolTip(tr("Viga mista de aço e concreto (NBR 8800, Anexo L) e laje mista com forma de aço incorporada — steel deck (Anexo N)"));
    connect(m_compositeAction, &QAction::triggered, this, &MainWindow::onCompositeFloor);

    m_designSettingsAction = new QAction(tr("Parâmetros"), this);
    m_designSettingsAction->setIcon(Structura::UI::svgIcon(QStringLiteral("design-params.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_designSettingsAction->setToolTip(tr("Aço, comprimentos efetivos, travamentos e combinações do dimensionamento"));
    connect(m_designSettingsAction, &QAction::triggered, this, &MainWindow::onDesignSettings);
    m_verifyDesignAction = new QAction(tr("Verificar\nperfis"), this);
    m_verifyDesignAction->setIcon(Structura::UI::svgIcon(QStringLiteral("check-design.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_verifyDesignAction->setToolTip(tr("Verifica os perfis adotados (NBR 8800 / NBR 14762) com os esforços da última análise"));
    connect(m_verifyDesignAction, &QAction::triggered, this, &MainWindow::onVerifyDesign);
    m_optimizeDesignAction = new QAction(tr("Dimensionar\nperfis"), this);
    m_optimizeDesignAction->setIcon(Structura::UI::svgIcon(QStringLiteral("design-optimize.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_optimizeDesignAction->setToolTip(tr("Adota o perfil mais leve da série que atende, reanalisando até convergir"));
    connect(m_optimizeDesignAction, &QAction::triggered, this, &MainWindow::onOptimizeDesign);

    auto *designTab = new QWidget(this);
    designTab->setObjectName(QStringLiteral("RibbonPage"));
    designTab->setStyleSheet(QStringLiteral("#RibbonPage { background: #f2f5fa; }"));
    auto *designLayout = new QHBoxLayout(designTab);
    designLayout->setContentsMargins(4, 20, 4, 3);
    designLayout->setSpacing(4);
    auto *designGroup = new QGroupBox(tr("Aço"), this);
    auto *designGrid = new QGridLayout(designGroup);
    designGrid->setContentsMargins(4, 5, 4, 4);
    designGrid->setHorizontalSpacing(30);
    designGrid->setVerticalSpacing(15);
    designGrid->setAlignment(Qt::AlignTop);
    populateActionGrid(designGrid, { m_designSettingsAction, m_verifyDesignAction, m_optimizeDesignAction, m_connectionsAction, m_compositeAction }, 5);
    designLayout->addWidget(designGroup, 0, Qt::AlignTop);
    m_reportAction = new QAction(tr("Memorial"), this);
    m_reportAction->setIcon(Structura::UI::svgIcon(QStringLiteral("report.svg"), QColor(QStringLiteral("#1e232b")), 64));
    m_reportAction->setToolTip(tr("Gera o memorial de cálculo e o relatório geral: escolha o conteúdo, visualize e exporte para Word (.docx)"));
    connect(m_reportAction, &QAction::triggered, this, &MainWindow::onGenerateReport);
    auto *reportGroup = new QGroupBox(tr("Documentação"), this);
    auto *reportGrid = new QGridLayout(reportGroup);
    reportGrid->setContentsMargins(4, 5, 4, 4);
    reportGrid->setHorizontalSpacing(30);
    reportGrid->setVerticalSpacing(15);
    reportGrid->setAlignment(Qt::AlignTop);
    populateActionGrid(reportGrid, { m_reportAction }, 1);
    designLayout->addWidget(reportGroup, 0, Qt::AlignTop);
    designLayout->addStretch(1);
    m_ribbon->addTab(designTab, tr("Dimensionamento"));

    // Inicializar o pós-processador
    m_postProcessor = std::make_unique<Structura::Visualization::PostProcessorRenderer>();

    // if (auto *tabBar = m_ribbon->tabBar()) {
    //     tabBar->hide();
    // }
    // }
    connect(m_ribbon, &QTabWidget::currentChanged, this, &MainWindow::onRibbonTabChanged);
    updateRibbonTabButtons(m_ribbon->currentIndex());
}

QToolButton *MainWindow::createToolButton(QAction *action)
{
    auto *button = new QToolButton(this);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setAutoRaise(false);
    const QSize iconSz(45, 45);
    button->setIconSize(iconSz);
    // Largura pela maior linha do texto (sem cortes) e altura para ate tres linhas.
    const QFontMetrics metrics(button->font());
    int textWidth = 0;
    for (const QString &line : action->text().split(QLatin1Char('\n'))) {
        textWidth = std::max(textWidth, metrics.horizontalAdvance(line));
    }
    const int lines = std::max(1, static_cast<int>(action->text().count(QLatin1Char('\n'))) + 1);
    const int width = std::max(64, textWidth + 22);
    const int height = 10 + iconSz.height() + 8 + lines * metrics.lineSpacing() + 6;
    button->setFixedSize(width, std::max(88, height));
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setStyleSheet("QToolButton { padding-top: 8px; }");
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

void MainWindow::populateActionGrid(QGridLayout *layout, const QList<QAction *> &actions, int columns)
{
    if (!layout) {
        return;
    }
    if (columns <= 0) {
        columns = 3;
    }

    layout->setHorizontalSpacing(14);
    layout->setVerticalSpacing(10);
    int row = 0;
    int column = 0;
    QWidget *container = layout->parentWidget();
    for (QAction *action : actions) {
        auto *button = createToolButton(action);
        if (container && button->parentWidget() != container) {
            button->setParent(container);
        }
        layout->addWidget(button, row, column, Qt::AlignTop);
        ++column;
        if (column >= columns) {
            column = 0;
            ++row;
        }
    }

    int stretchRow = row;
    if (column != 0) {
        ++stretchRow;
    }
    layout->setRowStretch(stretchRow, 1);
}

void MainWindow::onRibbonTabChanged(int index)
{
    updateRibbonTabButtons(index);
}

void MainWindow::updateRibbonTabButtons(int currentIndex)
{
    Q_UNUSED(currentIndex);
}
