#include "MainWindow.h"

#include "viz/postprocessor/PostProcessorRenderer.h"

#include <QAction>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
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
    m_homeTabButton->setText(tr("Inicio"));
    m_homeTabButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_homeTabButton->setCheckable(true);
    m_homeTabButton->setProperty("startButton", QStringLiteral("true"));
    m_homeTabButton->setCursor(Qt::PointingHandCursor);
    m_homeTabButton->setFixedHeight(22);
    m_homeTabButton->setChecked(true);
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
    connect(m_homeTabButton, &QToolButton::clicked, this, [this]() {
        if (m_ribbon) {
            m_ribbon->setCurrentIndex(0);
        }
    });

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
    const QList<QAction *> modelActions = { m_insertNodeCoordinatesAction, m_insertNodeScreenAction, m_insertBarAction, m_applyRestraintsAction };
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
    populateActionGrid(propGrid, { m_createMaterialAction, m_createSectionAction, m_assignPropertiesAction }, 3);
    propGrid->setRowStretch(2, 1);
    homeLayout->addWidget(propGroup, 0, Qt::AlignTop);

    homeLayout->addStretch(1);

    auto *toolsTab = new QWidget(this);
    toolsTab->setObjectName(QStringLiteral("RibbonPage"));
    toolsTab->setStyleSheet(QStringLiteral("#RibbonPage { background: #f2f5fa; }"));

    auto *toolsLayout = new QHBoxLayout(toolsTab);
    toolsLayout->setContentsMargins(4, 20, 4, 3);
    toolsLayout->setSpacing(4);

    auto *gridGroup = new QGroupBox(tr("Grid"), this);
    auto *gridLayout = new QGridLayout(gridGroup);
    gridLayout->setContentsMargins(4, 5, 4, 4);
    gridLayout->setHorizontalSpacing(30);
    gridLayout->setVerticalSpacing(15);
    gridLayout->setAlignment(Qt::AlignTop);
    const QList<QAction *> gridActions = {
        m_generateGridAction,
        m_addGridLineXAction,
        m_addGridLineYAction,
        m_addGridLineZAction,
        m_deleteGridLineAction
    };
    const int gridColumns = 2;
    populateActionGrid(gridLayout, gridActions, gridActions.size());
    const int gridRows = (gridActions.count() + gridColumns - 1) / gridColumns;
    gridLayout->setRowStretch(gridRows, 1);
    toolsLayout->addWidget(gridGroup, 0, Qt::AlignTop);
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
    const QList<QAction *> loadActions = { m_applyNodalLoadAction, m_applyDistributedLoadAction, m_runAnalysisAction };
    const int loadColumns = 2;
    populateActionGrid(loadsGrid, loadActions, loadActions.size());
    const int loadRows = (loadActions.count() + loadColumns - 1) / loadColumns;
    loadsGrid->setRowStretch(loadRows, 1);
    loadsLayout->addWidget(loadsGroup, 0, Qt::AlignTop);
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
    m_showDiagramsAction->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_showDiagramsAction->setEnabled(false); // habilitado após análise
    connect(m_showDiagramsAction, &QAction::triggered, this, &MainWindow::onShowDiagrams);

    m_hideDiagramsAction = new QAction(tr("Ocultar\nDiagramas"), this);
    m_hideDiagramsAction->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_hideDiagramsAction->setEnabled(false);
    connect(m_hideDiagramsAction, &QAction::triggered, this, &MainWindow::onHideDiagrams);

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
    resultsLayout->addStretch(1);

    m_ribbon->addTab(resultsTab, tr("Resultados"));

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
    button->setFixedSize(iconSz.width() + 6, iconSz.height() + 6);
    //button->setMinimumSize(68, 60);
    button->setMinimumSize(80, 82);
    //button->setMaximumHeight(66);
    button->setMaximumHeight(86);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setStyleSheet("QToolButton { padding-top: 10px;}");
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
    if (m_homeTabButton) {
        m_homeTabButton->setChecked(currentIndex == 0);
    }
}
