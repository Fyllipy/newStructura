#include "MainWindow.h"

#include "CoordinateDialog.h"
#include "GridDialog.h"
#include "MaterialDialog.h"
#include "SectionDialog.h"
#include "BarPropertiesDialog.h"
#include "AssignBarPropertiesDialog.h"
#include "SceneController.h"

#include <QAction>
#include <QApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QCheckBox>
#include <QStatusBar>
#include <QMessageBox>
#include <QPair>
#include <QSize>
#include <QHash>
#include <array>
#include <QLabel>
#include <QFileInfo>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QLocale>
#include <QDir>
#include <QSizePolicy>

#include <QUndoStack>
#include <QUndoCommand>
#include <QVector3D>
#include <QtMath>

#include <algorithm>

#include <QVTKOpenGLNativeWidget.h>

namespace {

class MoveNodesCommand : public QUndoCommand
{
public:
    MoveNodesCommand(SceneController *scene,
                     const QVector<QUuid> &ids,
                     const QVector<QVector3D> &oldPositions,
                     const QVector<QVector3D> &newPositions,
                     QUndoCommand *parent = nullptr)
        : QUndoCommand(parent)
        , m_scene(scene)
        , m_ids(ids)
        , m_oldPositions(oldPositions)
        , m_newPositions(newPositions)
    {
        setText(QObject::tr("Mover nÃ³(s)"));
    }

    void undo() override { apply(m_oldPositions); }
    void redo() override { apply(m_newPositions); }

private:
    void apply(const QVector<QVector3D> &positions)
    {
        if (!m_scene) {
            return;
        }
        m_scene->updateNodePositions(m_ids, positions);
    }

    SceneController *m_scene;
    QVector<QUuid> m_ids;
    QVector<QVector3D> m_oldPositions;
    QVector<QVector3D> m_newPositions;
};

class SetBarPropertiesCommand : public QUndoCommand
{
public:
    SetBarPropertiesCommand(SceneController *scene,
                            const QVector<QUuid> &ids,
                            const QVector<QUuid> &oldMaterials,
                            const QVector<QUuid> &oldSections,
                            const std::optional<QUuid> &newMaterial,
                            const std::optional<QUuid> &newSection,
                            QUndoCommand *parent = nullptr)
        : QUndoCommand(parent)
        , m_scene(scene)
        , m_ids(ids)
        , m_oldMaterials(oldMaterials)
        , m_oldSections(oldSections)
        , m_newMaterial(newMaterial)
        , m_newSection(newSection)
    {
        setText(QObject::tr("Atualizar propriedades de barra"));
    }

    void undo() override
    {
        if (!m_scene) {
            return;
        }
        for (int i = 0; i < m_ids.size(); ++i) {
            std::vector<QUuid> single { m_ids.at(i) };
            std::optional<QUuid> mat(m_oldMaterials.at(i));
            std::optional<QUuid> sec(m_oldSections.at(i));
            m_scene->assignBarProperties(single, mat, sec);
        }
    }

    void redo() override
    {
        if (!m_scene) {
            return;
        }
        if (!m_newMaterial.has_value() && !m_newSection.has_value()) {
            return;
        }
        std::vector<QUuid> barIds;
        barIds.reserve(m_ids.size());
        for (const QUuid &id : m_ids) {
            barIds.push_back(id);
        }
        m_scene->assignBarProperties(barIds, m_newMaterial, m_newSection);
    }

private:
    SceneController *m_scene;
    QVector<QUuid> m_ids;
    QVector<QUuid> m_oldMaterials;
    QVector<QUuid> m_oldSections;
    std::optional<QUuid> m_newMaterial;
    std::optional<QUuid> m_newSection;
};

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_sceneController(new SceneController(this))
    , m_selectionModel(new Structura::SelectionModel(this))
    , m_vtkWidget(new QVTKOpenGLNativeWidget(this))
    , m_ribbon(new QTabWidget(this))
    , m_homeTabButton(nullptr)
    , m_undoStack(new QUndoStack(this))
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::CustomizeWindowHint);
    setMouseTracking(true);
    setWindowTitle(tr("Structura 3D"));

    m_undoStack->setUndoLimit(128);
    m_undoAction = m_undoStack->createUndoAction(this, tr("Desfazer"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    addAction(m_undoAction);

    m_redoAction = m_undoStack->createRedoAction(this, tr("Refazer"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    addAction(m_redoAction);

    m_addPointAction = new QAction(tr("Inserir ponto"), this);
    m_addPointAction->setIcon(style()->standardIcon(QStyle::SP_DialogYesButton));
    connect(m_addPointAction, &QAction::triggered, this, &MainWindow::onAddPoint);

    m_insertBarAction = new QAction(tr("Inserir barra"), this);
    m_insertBarAction->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    connect(m_insertBarAction, &QAction::triggered, this, &MainWindow::onInsertBar);

    m_openModelAction = new QAction(tr("Abrir .dat"), this);
    m_openModelAction->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    m_openModelAction->setShortcut(QKeySequence::Open);
    connect(m_openModelAction, &QAction::triggered, this, &MainWindow::onOpenModel);

    m_saveModelAction = new QAction(tr("Salvar .dat"), this);
    m_saveModelAction->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    m_saveModelAction->setShortcut(QKeySequence::Save);
    connect(m_saveModelAction, &QAction::triggered, this, &MainWindow::onSaveModel);

    m_generateGridAction = new QAction(tr("Gerar grid"), this);
    m_generateGridAction->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
    connect(m_generateGridAction, &QAction::triggered, this, &MainWindow::onGenerateGrid);

    m_createMaterialAction = new QAction(tr("Novo material"), this);
    m_createMaterialAction->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    connect(m_createMaterialAction, &QAction::triggered, this, &MainWindow::onCreateMaterial);

    m_createSectionAction = new QAction(tr("Nova secao"), this);
    m_createSectionAction->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    connect(m_createSectionAction, &QAction::triggered, this, &MainWindow::onCreateSection);

    m_assignPropertiesAction = new QAction(tr("Atribuir a barras"), this);
    m_assignPropertiesAction->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    connect(m_assignPropertiesAction, &QAction::triggered, this, &MainWindow::onAssignProperties);

    m_resetCameraAction = new QAction(tr("Visao inicial"), this);
    m_resetCameraAction->setIcon(style()->standardIcon(QStyle::SP_CommandLink));
    connect(m_resetCameraAction, &QAction::triggered, this, &MainWindow::onResetCamera);

    m_zoomExtentsAction = new QAction(tr("Zoom extents"), this);
    m_zoomExtentsAction->setIcon(style()->standardIcon(QStyle::SP_DesktopIcon));
    connect(m_zoomExtentsAction, &QAction::triggered, this, &MainWindow::onZoomExtents);

    addAction(m_openModelAction);
    addAction(m_saveModelAction);

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
    ensurePropertiesPanel();;
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
            });
    connect(m_undoStack, &QUndoStack::indexChanged, this, [this](int) {
        refreshPropertiesPanel();
    });

    // Handle screen-click events during insertion
    m_vtkWidget->installEventFilter(this);

    // Status bar styling and initial message
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setStyleSheet(QStringLiteral(
        "QStatusBar { background: #e9edf4; color: #1f242c; border-top: 1px solid #cbd4e2; }"
    ));
    setCommand(Command::None);

    m_lastDatDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    updateMaximizeButtonIcon();
}

MainWindow::~MainWindow() = default;

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

    ensurePropertiesPanel();;
    if (m_propertiesContainer) {
        m_contentLayout->addWidget(m_propertiesContainer, 0);
    }

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
    m_propertiesToolButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_propertiesToolButton->setIconSize(QSize(24, 24));
    m_propertiesToolButton->setCheckable(true);
    m_propertiesToolButton->setAutoRaise(false);
    m_propertiesToolButton->setFixedSize(36, 36);
    m_propertiesToolButton->setCursor(Qt::PointingHandCursor);
    m_propertiesToolButton->setToolTip(tr("Propriedades"));
    layout->addWidget(m_propertiesToolButton, 0, Qt::AlignHCenter | Qt::AlignTop);
    layout->addStretch(1);

    connect(m_propertiesToolButton, &QToolButton::toggled, this, [this](bool checked) {
        ensurePropertiesPanel();;
        if (!m_propertiesContainer) {
            return;
        }
        m_propertiesContainer->setVisible(checked);
        if (checked) {
            refreshPropertiesPanel();
        }
    });
}

void MainWindow::ensurePropertiesPanel()
{
    if (m_propertiesPanel) {
        return;
    }
    if (!m_contentLayout) {
        return;
    }

    QWidget *parentWidget = m_contentLayout->parentWidget();
    m_propertiesContainer = new QWidget(parentWidget);
    m_propertiesContainer->setObjectName(QStringLiteral("PropertiesContainer"));
    m_propertiesContainer->setFixedWidth(320);
    m_propertiesContainer->setStyleSheet(QStringLiteral(
        "#PropertiesContainer { background: #f5f7fb; border-left: 1px solid #d6dde8; }"));

    auto *panelLayout = new QVBoxLayout(m_propertiesContainer);
    panelLayout->setContentsMargins(12, 12, 12, 12);
    panelLayout->setSpacing(8);

    m_propertiesPanel = new PropertiesPanel(m_propertiesContainer);
    panelLayout->addWidget(m_propertiesPanel);
    panelLayout->addStretch(1);

    m_propertiesContainer->hide();
    m_contentLayout->addWidget(m_propertiesContainer, 0);

    connect(m_propertiesPanel, &PropertiesPanel::nodeCoordinateEdited,
            this, &MainWindow::onNodeCoordinateEdited);
    connect(m_propertiesPanel, &PropertiesPanel::barMaterialEdited,
            this, &MainWindow::onBarMaterialEdited);
    connect(m_propertiesPanel, &PropertiesPanel::barSectionEdited,
            this, &MainWindow::onBarSectionEdited);

    QVector<QPair<QUuid, QString>> materialOptions;
    materialOptions.reserve(m_materials.size());
    for (const auto &mat : m_materials) {
        materialOptions.append({ mat.uuid, mat.name });
    }
    m_propertiesPanel->setMaterialOptions(materialOptions);

    QVector<QPair<QUuid, QString>> sectionOptions;
    sectionOptions.reserve(m_sections.size());
    for (const auto &sec : m_sections) {
        sectionOptions.append({ sec.uuid, sec.name });
    }
    m_propertiesPanel->setSectionOptions(sectionOptions);
}

void MainWindow::refreshPropertiesPanel()
{
    ensurePropertiesPanel();;
    if (!m_propertiesPanel) {
        return;
    }

    QVector<QPair<QUuid, QString>> materialOptions;
    materialOptions.reserve(m_materials.size());
    for (const auto &mat : m_materials) {
        materialOptions.append({ mat.uuid, mat.name });
    }
    m_propertiesPanel->setMaterialOptions(materialOptions);

    QVector<QPair<QUuid, QString>> sectionOptions;
    sectionOptions.reserve(m_sections.size());
    for (const auto &sec : m_sections) {
        sectionOptions.append({ sec.uuid, sec.name });
    }
    m_propertiesPanel->setSectionOptions(sectionOptions);

    const QSet<QUuid> nodeIds = m_selectionModel ? m_selectionModel->selectedNodes() : QSet<QUuid>();
    const QSet<QUuid> barIds = m_selectionModel ? m_selectionModel->selectedBars() : QSet<QUuid>();

    m_propertiesPanel->setNodeEntries(buildNodeEntries(nodeIds));
    m_propertiesPanel->setBarEntries(buildBarEntries(barIds));
    updateGridInfoOnPanel();
}

QVector<PropertiesPanel::NodeEntry> MainWindow::buildNodeEntries(const QSet<QUuid> &nodeIds) const
{
    QVector<PropertiesPanel::NodeEntry> entries;
    if (!m_sceneController) {
        return entries;
    }
    for (const QUuid &id : nodeIds) {
        const SceneController::Node *node = m_sceneController->findNode(id);
        if (!node) {
            continue;
        }
        PropertiesPanel::NodeEntry entry;
        entry.id = id;
        entry.externalId = node->externalId();
        const auto pos = node->position();
        entry.x = pos[0];
        entry.y = pos[1];
        entry.z = pos[2];
        entry.restraints.fill(false);
        for (const auto &support : m_supports) {
            if (support.nodeId == entry.externalId) {
                for (int i = 0; i < 6; ++i) {
                    entry.restraints[static_cast<std::size_t>(i)] = support.restraints[i];
                }
                break;
            }
        }
        int loadCount = 0;
        for (const auto &load : m_nodalLoads) {
            if (load.nodeId == entry.externalId) {
                ++loadCount;
            }
        }
        entry.loadCount = loadCount;
        entries.append(entry);
    }
    return entries;
}

QVector<PropertiesPanel::BarEntry> MainWindow::buildBarEntries(const QSet<QUuid> &barIds) const
{
    QVector<PropertiesPanel::BarEntry> entries;
    if (!m_sceneController) {
        return entries;
    }

    const auto nodeInfos = m_sceneController->nodeInfos();
    QHash<QUuid, SceneController::NodeInfo> nodeMap;
    for (const auto &info : nodeInfos) {
        nodeMap.insert(info.id, info);
    }

    for (const QUuid &id : barIds) {
        const SceneController::Bar *bar = m_sceneController->findBar(id);
        if (!bar) {
            continue;
        }
        PropertiesPanel::BarEntry entry;
        entry.id = id;
        entry.externalId = bar->externalId();
        entry.materialId = bar->materialId();
        entry.sectionId = bar->sectionId();

        const auto materialInfo = findMaterial(entry.materialId);
        entry.materialName = materialInfo ? materialInfo->name : tr("Sem material");
        const auto sectionInfo = findSection(entry.sectionId);
        entry.sectionName = sectionInfo ? sectionInfo->name : tr("Sem secao");

        QVector3D startPos;
        QVector3D endPos;
        bool hasStart = false;
        bool hasEnd = false;
        auto startIt = nodeMap.constFind(bar->startNodeId());
        if (startIt != nodeMap.constEnd()) {
            entry.nodeI = startIt.value().externalId;
            startPos = QVector3D(startIt.value().x, startIt.value().y, startIt.value().z);
            hasStart = true;
        }
        auto endIt = nodeMap.constFind(bar->endNodeId());
        if (endIt != nodeMap.constEnd()) {
            entry.nodeJ = endIt.value().externalId;
            endPos = QVector3D(endIt.value().x, endIt.value().y, endIt.value().z);
            hasEnd = true;
        }
        entry.length = (hasStart && hasEnd) ? (startPos - endPos).length() : 0.0;
        int distributedLoads = 0;
        for (const auto &load : m_memberLoads) {
            if (load.memberId == entry.externalId) {
                ++distributedLoads;
            }
        }
        entry.distributedLoadCount = distributedLoads;

        entries.append(entry);
    }
    return entries;
}

void MainWindow::updateGridInfoOnPanel()
{
    if (!m_propertiesPanel || !m_sceneController) {
        return;
    }
    const bool hasGrid = m_sceneController->hasGrid();
    double dx = 0.0, dy = 0.0, dz = 0.0;
    int nx = 0, ny = 0, nz = 0;
    if (hasGrid) {
        m_sceneController->gridSpacing(dx, dy, dz);
        m_sceneController->gridCounts(nx, ny, nz);
    }
    m_propertiesPanel->setGridInfo(hasGrid, dx, dy, dz, nx, ny, nz);
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
        QGroupBox {background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f8fafc, stop:1 #f2f5fa);
        border: 1px solid #bec7d4; border-radius: 3px; margin-top: 4px; color: #1e232b; padding-top: 6px;}
        QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 4px; font-weight: 600; color: #0b6da1; }
        QToolButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #fbfdff, stop:1 #e7ecf3);color: #1e232b; border: 1px solid #c0c7d2; border-radius: 3px; padding: 2px 4px;
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
    modelGrid->setHorizontalSpacing(4);
    modelGrid->setVerticalSpacing(4);
    modelGrid->setAlignment(Qt::AlignTop);
    const QList<QAction *> modelActions = { m_addPointAction, m_insertBarAction, m_generateGridAction };
    populateActionGrid(modelGrid, modelActions, 3);
    m_snapCheck = new QCheckBox(tr("Snap ao grid"), modelGroup);
    m_snapCheck->setChecked(true);
    m_snapCheck->setStyleSheet(QStringLiteral("QCheckBox { margin-top: 2px; }"));
    const int modelRows = (modelActions.count() + 3 - 1) / 3;
    modelGrid->addWidget(m_snapCheck, modelRows, 0, 1, 3, Qt::AlignLeft | Qt::AlignVCenter);
    modelGrid->setRowStretch(modelRows + 1, 1);
    connect(m_snapCheck, &QCheckBox::toggled, this, &MainWindow::onSnapToggled);
    homeLayout->addWidget(modelGroup, 0, Qt::AlignTop);

    auto *propGroup = new QGroupBox(tr("Propriedades"), this);
    auto *propGrid = new QGridLayout(propGroup);
    propGrid->setContentsMargins(4, 5, 4, 4);
    propGrid->setHorizontalSpacing(4);
    propGrid->setVerticalSpacing(4);
    propGrid->setAlignment(Qt::AlignTop);
    populateActionGrid(propGrid, { m_createMaterialAction, m_createSectionAction, m_assignPropertiesAction }, 3);
    propGrid->setRowStretch(2, 1);
    homeLayout->addWidget(propGroup, 0, Qt::AlignTop);

    auto *viewGroup = new QGroupBox(tr("Visualizacao"), this);
    auto *viewGrid = new QGridLayout(viewGroup);
    viewGrid->setContentsMargins(4, 5, 4, 4);
    viewGrid->setHorizontalSpacing(4);
    viewGrid->setVerticalSpacing(4);
    viewGrid->setAlignment(Qt::AlignTop);
    populateActionGrid(viewGrid, viewActions, 2);
    viewGrid->setRowStretch(1, 1);
    homeLayout->addWidget(viewGroup, 0, Qt::AlignTop);

    homeLayout->addStretch(1);

    m_ribbon->addTab(homeTab, tr("Inicio"));
    // if (auto *tabBar = m_ribbon->tabBar()) {
    //     tabBar->hide();
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
    button->setIconSize(QSize(28, 28));
    button->setMinimumSize(68, 60);
    button->setMaximumHeight(66);
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

int MainWindow::nextMaterialExternalId() const
{
    int maxId = 0;
    for (const auto &mat : m_materials) {
        if (mat.externalId > maxId) {
            maxId = mat.externalId;
        }
    }
    return maxId + 1;
}

int MainWindow::nextSectionExternalId() const
{
    int maxId = 0;
    for (const auto &sec : m_sections) {
        if (sec.externalId > maxId) {
            maxId = sec.externalId;
        }
    }
    return maxId + 1;
}

int MainWindow::nextBarExternalId() const
{
    int maxId = 0;
    if (m_sceneController) {
        const auto bars = m_sceneController->bars();
        for (const auto &bar : bars) {
            if (bar.externalId > maxId) {
                maxId = bar.externalId;
            }
        }
    }
    return maxId + 1;
}

void MainWindow::setCommand(Command command)
{
    if (m_command == command) {
        updateStatus();
        return;
    }

    const bool wasBarMode = (m_command == Command::InsertBarFirst || m_command == Command::InsertBarSecond);
    const bool willBeBarMode = (command == Command::InsertBarFirst || command == Command::InsertBarSecond);

    if (wasBarMode && !willBeBarMode) {
        m_sceneController->clearHighlightedNode();
        m_firstBarNodeId = QUuid();
    }

    m_command = command;

    if (!willBeBarMode) {
        m_sceneController->clearHighlightedNode();
        m_firstBarNodeId = QUuid();
    }

    updateStatus();
}

MainWindow::MaterialInfo *MainWindow::findMaterial(const QUuid &id)
{
    if (id.isNull()) {
        return nullptr;
    }
    for (auto &mat : m_materials) {
        if (mat.uuid == id) {
            return &mat;
        }
    }
    return nullptr;
}

const MainWindow::MaterialInfo *MainWindow::findMaterial(const QUuid &id) const
{
    return const_cast<MainWindow *>(this)->findMaterial(id);
}

MainWindow::SectionInfo *MainWindow::findSection(const QUuid &id)
{
    if (id.isNull()) {
        return nullptr;
    }
    for (auto &sec : m_sections) {
        if (sec.uuid == id) {
            return &sec;
        }
    }
    return nullptr;
}

const MainWindow::SectionInfo *MainWindow::findSection(const QUuid &id) const
{
    return const_cast<MainWindow *>(this)->findSection(id);
}

void MainWindow::onAddPoint()
{
    CoordinateDialog dialog(this);
    // Allow starting on-screen insertion from this dialog
    connect(&dialog, &CoordinateDialog::requestScreenInsertion, this, [this, &dialog](){
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

void MainWindow::onStartScreenInsert()
{
    setCommand(Command::InsertNode);
    if (m_vtkWidget) {
        m_vtkWidget->setFocus();
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_quickBar || obj == m_titleLabel || (obj && obj->property("dragRegion").toBool())) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                if (!isMaximized()) {
                    m_draggingWindow = true;
                    m_dragOffset = me->globalPos() - frameGeometry().topLeft();
                }
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            if (m_draggingWindow) {
                auto *me = static_cast<QMouseEvent *>(event);
                const QPoint newPos = me->globalPos() - m_dragOffset;
                move(newPos);
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                m_draggingWindow = false;
                return true;
            }
            break;
        }
        case QEvent::MouseButtonDblClick: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                m_draggingWindow = false;
                toggleMaximized();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    if (obj == m_vtkWidget && m_sceneController) {
        const auto toDisplay = [&](const QPoint &p) -> QPoint {
            return QPoint(p.x(), m_sceneController->viewportHeight() - 1 - p.y());
        };
        const auto pickNodeAt = [&](const QPoint &p) -> QUuid {
            const QPoint disp = toDisplay(p);
            return m_sceneController->pickNode(disp.x(), disp.y());
        };
        const auto pickBarAt = [&](const QPoint &p) -> QUuid {
            const QPoint disp = toDisplay(p);
            return m_sceneController->pickBar(disp.x(), disp.y());
        };

        if (m_command == Command::InsertNode) {
            switch (event->type()) {
            case QEvent::MouseButtonPress: {
                auto *me = static_cast<QMouseEvent *>(event);
                if (me->button() == Qt::LeftButton) {
                    const QPoint p = me->pos();
                    const QPoint disp = toDisplay(p);
                    double wx = 0, wy = 0, wz = 0;
                    if (m_sceneController->worldPointOnViewPlane(disp.x(), disp.y(), wx, wy, wz)) {
                        if (m_snapCheck && m_snapCheck->isChecked() && m_sceneController->hasGrid()) {
                            m_sceneController->snapToGrid(wx, wy, wz);
                        }
                        m_sceneController->addPoint(wx, wy, wz);
                    }
                    return true;
                } else if (me->button() == Qt::RightButton) {
                    return false;
                }
                break;
            }
            case QEvent::MouseMove: {
                auto *mm = static_cast<QMouseEvent *>(event);
                const QUuid nodeId = pickNodeAt(mm->pos());
                if (!nodeId.isNull()) {
                    m_sceneController->setHighlightedNode(nodeId);
                } else {
                    m_sceneController->clearHighlightedNode();
                }
                return false;
            }
            case QEvent::KeyPress: {
                auto *ke = static_cast<QKeyEvent *>(event);
                if (ke->key() == Qt::Key_Escape) {
                    setCommand(Command::None);
                    return true;
                }
                break;
            }
            default:
                break;
            }
        } else if (m_command == Command::InsertBarFirst || m_command == Command::InsertBarSecond) {
            switch (event->type()) {
            case QEvent::MouseMove: {
                auto *mm = static_cast<QMouseEvent *>(event);
                const QUuid nodeId = pickNodeAt(mm->pos());
                if (!nodeId.isNull()) {
                    m_sceneController->setHighlightedNode(nodeId);
                } else {
                    m_sceneController->clearHighlightedNode();
                }
                return false;
            }
            case QEvent::MouseButtonPress: {
                auto *me = static_cast<QMouseEvent *>(event);
                if (me->button() == Qt::LeftButton) {
                    const QUuid pickedNode = pickNodeAt(me->pos());
                    if (!pickedNode.isNull()) {
                        if (m_command == Command::InsertBarFirst) {
                            m_firstBarNodeId = pickedNode;
                            m_sceneController->setHighlightedNode(pickedNode);
                            setCommand(Command::InsertBarSecond);
                        } else {
                            if (pickedNode == m_firstBarNodeId) {
                                QMessageBox::warning(this, tr("Inserir barra"), tr("Selecione dois nos distintos."));
                                return true;
                            }

                            QVector<QPair<QUuid, QString>> materialOptions;
                            materialOptions.reserve(m_materials.size());
                            for (const auto &mat : m_materials) {
                                materialOptions.append({ mat.uuid, mat.name });
                            }
                            QVector<QPair<QUuid, QString>> sectionOptions;
                            sectionOptions.reserve(m_sections.size());
                            for (const auto &sec : m_sections) {
                                sectionOptions.append({ sec.uuid, sec.name });
                            }

                            BarPropertiesDialog barDialog(materialOptions, sectionOptions, this);
                            barDialog.setCurrentMaterial(m_lastMaterialId);
                            barDialog.setCurrentSection(m_lastSectionId);
                            if (barDialog.exec() == QDialog::Accepted) {
                                const QUuid materialId = barDialog.selectedMaterial();
                                const QUuid sectionId = barDialog.selectedSection();
                                const QUuid barId = m_sceneController->addBar(m_firstBarNodeId, pickedNode, materialId, sectionId);
                                if (!barId.isNull()) {
                                    const int externalBarId = nextBarExternalId();
                                    m_sceneController->setBarExternalId(barId, externalBarId);
                                    if (!materialId.isNull()) {
                                        m_lastMaterialId = materialId;
                                    }
                                    if (!sectionId.isNull()) {
                                        m_lastSectionId = sectionId;
                                    }
                                }
                            }
                            m_firstBarNodeId = QUuid();
                            m_sceneController->clearHighlightedNode();
                            setCommand(Command::InsertBarFirst);
                        }
                    }
                    return true;
                } else if (me->button() == Qt::RightButton) {
                    return false;
                }
                break;
            }
            case QEvent::KeyPress: {
                auto *ke = static_cast<QKeyEvent *>(event);
                if (ke->key() == Qt::Key_Escape) {
                    setCommand(Command::None);
                    return true;
                }
                break;
            }
            default:
                break;
            }
        } else {
            switch (event->type()) {
            case QEvent::MouseMove: {
                auto *mm = static_cast<QMouseEvent *>(event);
                const QUuid nodeId = pickNodeAt(mm->pos());
                if (!nodeId.isNull()) {
                    m_sceneController->setHighlightedNode(nodeId);
                } else {
                    m_sceneController->clearHighlightedNode();
                }
                return false;
            }
            case QEvent::MouseButtonPress: {
                auto *me = static_cast<QMouseEvent *>(event);
                if (me->button() != Qt::LeftButton) {
                    return false;
                }

                const QUuid nodeId = pickNodeAt(me->pos());
                const QUuid barId = nodeId.isNull() ? pickBarAt(me->pos()) : QUuid();

                Structura::SelectionModel::Mode mode = Structura::SelectionModel::Mode::Replace;
                if (me->modifiers().testFlag(Qt::ControlModifier)) {
                    mode = Structura::SelectionModel::Mode::Toggle;
                } else if (me->modifiers().testFlag(Qt::ShiftModifier)) {
                    mode = Structura::SelectionModel::Mode::Add;
                }

                if (!nodeId.isNull()) {
                    m_selectionModel->selectNode(nodeId, mode);
                    return true;
                }
                if (!barId.isNull()) {
                    m_selectionModel->selectBar(barId, mode);
                    return true;
                }

                if (mode == Structura::SelectionModel::Mode::Replace) {
                    m_selectionModel->clear();
                    return true;
                }
                break;
            }
            case QEvent::Leave: {
                m_sceneController->clearHighlightedNode();
                break;
            }
            default:
                break;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
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

void MainWindow::onCreateMaterial()
{
    MaterialDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    MaterialInfo info;
    info.uuid = QUuid::createUuid();
    info.externalId = nextMaterialExternalId();
    info.name = dialog.name();
    info.youngModulus = dialog.youngModulus();
    info.shearModulus = dialog.shearModulus();
    if (info.name.trimmed().isEmpty()) {
        info.name = tr("Material %1").arg(info.externalId);
    }
    m_materials.append(info);
    m_lastMaterialId = info.uuid;
    refreshPropertiesPanel();
}

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

void MainWindow::onCreateSection()
{
    SectionDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    SectionInfo info;
    info.uuid = QUuid::createUuid();
    info.externalId = nextSectionExternalId();
    info.name = dialog.name();
    info.area = dialog.area();
    info.iz = dialog.iz();
    info.iy = dialog.iy();
    info.j = dialog.j();
    if (info.name.trimmed().isEmpty()) {
        info.name = tr("Secao %1").arg(info.externalId);
    }
    m_sections.append(info);
    m_lastSectionId = info.uuid;
    refreshPropertiesPanel();
}

void MainWindow::onAssignProperties()
{
    auto bars = m_sceneController->bars();
    if (bars.empty()) {
        QMessageBox::information(this, tr("Atribuir a barras"), tr("Nao existem barras cadastradas."));
        return;
    }

    const auto nodes = m_sceneController->nodeInfos();

    QVector<QPair<QUuid, QString>> materialOptions;
    materialOptions.reserve(m_materials.size());
    for (const auto &mat : m_materials) {
        materialOptions.append({ mat.uuid, mat.name });
    }
    QVector<QPair<QUuid, QString>> sectionOptions;
    sectionOptions.reserve(m_sections.size());
    for (const auto &sec : m_sections) {
        sectionOptions.append({ sec.uuid, sec.name });
    }

    AssignBarPropertiesDialog dialog(materialOptions, sectionOptions, bars, nodes, this);
    dialog.setCurrentMaterial(m_lastMaterialId);
    dialog.setCurrentSection(m_lastSectionId);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QList<int> selection = dialog.selectedBarIndices();
    if (selection.isEmpty()) {
        return;
    }

    const QUuid materialId = dialog.selectedMaterial();
    const QUuid sectionId = dialog.selectedSection();

    std::vector<QUuid> barIds;
    barIds.reserve(selection.size());
    for (int idx : selection) {
        if (idx >= 0 && idx < static_cast<int>(bars.size())) {
            barIds.push_back(bars[static_cast<std::size_t>(idx)].id);
        }
    }

    if (!barIds.empty()) {
        m_sceneController->assignBarProperties(barIds,
                                               std::optional<QUuid>(materialId),
                                               std::optional<QUuid>(sectionId));
    }

    if (!materialId.isNull()) {
        m_lastMaterialId = materialId;
    }
    if (!sectionId.isNull()) {
        m_lastSectionId = sectionId;
    }

    refreshPropertiesPanel();
}

void MainWindow::onOpenModel()
{
    const QString initialDir = m_lastDatDirectory.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        : m_lastDatDirectory;
    const QString filePath = QFileDialog::getOpenFileName(this, tr("Abrir modelo"), initialDir, tr("Arquivos DAT (*.dat);;Todos os arquivos (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }
    if (loadFromDat(filePath)) {
        m_lastDatDirectory = QFileInfo(filePath).absolutePath();
        statusBar()->showMessage(tr("Modelo carregado: %1").arg(QFileInfo(filePath).fileName()), 5000);
    }
}

void MainWindow::onSaveModel()
{
    const QString initialDir = m_lastDatDirectory.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        : m_lastDatDirectory;
    const QString filePath = QFileDialog::getSaveFileName(this, tr("Salvar modelo"), initialDir + QLatin1String("/modelo.dat"), tr("Arquivos DAT (*.dat);;Todos os arquivos (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }
    QString finalPath = filePath;
    if (!finalPath.endsWith(QLatin1String(".dat"), Qt::CaseInsensitive)) {
        finalPath.append(QLatin1String(".dat"));
    }
    if (saveToDat(finalPath)) {
        m_lastDatDirectory = QFileInfo(finalPath).absolutePath();
        statusBar()->showMessage(tr("Modelo salvo: %1").arg(QFileInfo(finalPath).fileName()), 5000);
    }
}

void MainWindow::resetModel()
{
    setCommand(Command::None);
    m_sceneController->clearAll();
    m_firstBarNodeId = QUuid();
    if (m_selectionModel) {
        m_selectionModel->clear();
    }
    m_materials.clear();
    m_sections.clear();
    m_supports.clear();
    m_nodalLoads.clear();
    m_memberLoads.clear();
    m_lastMaterialId = QUuid();
    m_lastSectionId = QUuid();
    refreshPropertiesPanel();
}

bool MainWindow::loadFromDat(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Erro"), tr("Nao foi possivel abrir %1").arg(QDir::toNativeSeparators(filePath)));
        return false;
    }

    QTextStream stream(&file);
    stream.setLocale(QLocale::c());

    enum class Section { None, Materials, Sections, Nodes, Members, NodalLoads, MemberLoads };
    Section current = Section::None;

    QVector<MaterialInfo> materialsTmp;
    QVector<SectionInfo> sectionsTmp;
    struct LoadedNode {
        int id;
        double x;
        double y;
        double z;
        std::array<int, 6> restraints;
    };
    QVector<LoadedNode> nodesTmp;
    struct LoadedMember {
        int id;
        int nodeI;
        int nodeJ;
        int materialId;
        int sectionId;
    };
    QVector<LoadedMember> membersTmp;
    QVector<NodeSupport> supportsTmp;
    QVector<NodalLoad> nodalLoadsTmp;
    QVector<MemberLoad> memberLoadsTmp;

    int lineNumber = 0;
    QRegularExpression whitespace("\\s+");

    auto toInt = [&](const QString &token, bool *ok = nullptr) -> int {
        bool localOk = false;
        int value = token.toInt(&localOk);
        if (ok) *ok = localOk;
        return value;
    };

    auto toDouble = [&](const QString &token, bool *ok = nullptr) -> double {
        bool localOk = false;
        double value = token.toDouble(&localOk);
        if (ok) *ok = localOk;
        return value;
    };

    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        ++lineNumber;
        const QString trimmed = line.trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }

        if (trimmed.startsWith(QLatin1Char('['))) {
            if (trimmed.compare("[MATERIALS]", Qt::CaseInsensitive) == 0) {
                current = Section::Materials;
            } else if (trimmed.compare("[SECTIONS]", Qt::CaseInsensitive) == 0) {
                current = Section::Sections;
            } else if (trimmed.compare("[NODES]", Qt::CaseInsensitive) == 0) {
                current = Section::Nodes;
            } else if (trimmed.compare("[MEMBERS]", Qt::CaseInsensitive) == 0) {
                current = Section::Members;
            } else if (trimmed.compare("[NODAL_LOADS]", Qt::CaseInsensitive) == 0) {
                current = Section::NodalLoads;
            } else if (trimmed.compare("[MEMBER_LOADS]", Qt::CaseInsensitive) == 0) {
                current = Section::MemberLoads;
            } else {
                current = Section::None;
            }
            continue;
        }

        QStringList parts = trimmed.split(whitespace, Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            continue;
        }

        bool ok = false;
        switch (current) {
        case Section::Materials: {
            if (parts.size() < 3) {
                QMessageBox::warning(this, tr("Erro"), tr("Linha de material invalida (%1)").arg(lineNumber));
                return false;
            }
            MaterialInfo mat;
            mat.uuid = QUuid::createUuid();
            mat.externalId = toInt(parts[0], &ok);
            if (!ok) {
                QMessageBox::warning(this, tr("Erro"), tr("ID de material invalido na linha %1").arg(lineNumber));
                return false;
            }
            mat.name = tr("Material %1").arg(mat.externalId);
            mat.youngModulus = toDouble(parts[1], &ok);
            if (!ok) {
                QMessageBox::warning(this, tr("Erro"), tr("Valor de E invalido na linha %1").arg(lineNumber));
                return false;
            }
            mat.shearModulus = toDouble(parts[2], &ok);
            if (!ok) {
                QMessageBox::warning(this, tr("Erro"), tr("Valor de G invalido na linha %1").arg(lineNumber));
                return false;
            }
            materialsTmp.append(mat);
            break;
        }
        case Section::Sections: {
            if (parts.size() < 5) {
                QMessageBox::warning(this, tr("Erro"), tr("Linha de secao invalida (%1)").arg(lineNumber));
                return false;
            }
            SectionInfo sec;
            sec.uuid = QUuid::createUuid();
            sec.externalId = toInt(parts[0], &ok);
            if (!ok) {
                QMessageBox::warning(this, tr("Erro"), tr("ID de secao invalido na linha %1").arg(lineNumber));
                return false;
            }
            sec.name = tr("Secao %1").arg(sec.externalId);
            sec.area = toDouble(parts[1], &ok);
            sec.iz = toDouble(parts[2], &ok);
            sec.iy = toDouble(parts[3], &ok);
            sec.j = toDouble(parts[4], &ok);
            if (!ok) {
                QMessageBox::warning(this, tr("Erro"), tr("Valores da secao invalidos na linha %1").arg(lineNumber));
                return false;
            }
            sectionsTmp.append(sec);
            break;
        }
        case Section::Nodes: {
            if (parts.size() < 10) {
                QMessageBox::warning(this, tr("Erro"), tr("Linha de no invalida (%1)").arg(lineNumber));
                return false;
            }
            LoadedNode node;
            node.id = toInt(parts[0], &ok);
            if (!ok) {
                QMessageBox::warning(this, tr("Erro"), tr("ID de no invalido na linha %1").arg(lineNumber));
                return false;
            }
            node.x = toDouble(parts[1], &ok);
            node.y = toDouble(parts[2], &ok);
            node.z = toDouble(parts[3], &ok);
            if (!ok) {
                QMessageBox::warning(this, tr("Erro"), tr("Coordenadas invalidas na linha %1").arg(lineNumber));
                return false;
            }
            for (int i = 0; i < 6; ++i) {
                node.restraints[i] = (i + 4 < parts.size()) ? toInt(parts[4 + i], &ok) : 0;
                if (!ok) {
                    node.restraints[i] = 0;
                }
            }
            nodesTmp.append(node);
            break;
        }
        case Section::Members: {
            if (parts.size() < 5) {
                QMessageBox::warning(this, tr("Erro"), tr("Linha de barra invalida (%1)").arg(lineNumber));
                return false;
            }
            LoadedMember member;
            member.id = toInt(parts[0], &ok);
            member.nodeI = toInt(parts[1], &ok);
            member.nodeJ = toInt(parts[2], &ok);
            member.materialId = toInt(parts[3], &ok);
            member.sectionId = toInt(parts[4], &ok);
            if (!ok) {
                QMessageBox::warning(this, tr("Erro"), tr("Linha de barra invalida (%1)").arg(lineNumber));
                return false;
            }
            membersTmp.append(member);
            break;
        }
        case Section::NodalLoads: {
            if (parts.size() < 7) {
                // allow blank data lines
                break;
            }
            NodalLoad load;
            load.nodeId = toInt(parts[0], &ok);
            if (!ok) break;
            load.fx = toDouble(parts[1], &ok);
            load.fy = toDouble(parts[2], &ok);
            load.fz = toDouble(parts[3], &ok);
            load.mx = toDouble(parts[4], &ok);
            load.my = toDouble(parts[5], &ok);
            load.mz = toDouble(parts[6], &ok);
            nodalLoadsTmp.append(load);
            break;
        }
        case Section::MemberLoads: {
            if (parts.size() < 5) {
                break;
            }
            MemberLoad load;
            load.memberId = toInt(parts[0], &ok);
            if (!ok) break;
            load.system = parts[1].toUpper();
            load.qx = toDouble(parts[2], &ok);
            load.qy = toDouble(parts[3], &ok);
            load.qz = toDouble(parts[4], &ok);
            memberLoadsTmp.append(load);
            break;
        }
        case Section::None:
        default:
            break;
        }
    }

    file.close();

    // Build supports from node data
    supportsTmp.clear();
    for (const auto &node : nodesTmp) {
        NodeSupport support;
        support.nodeId = node.id;
        for (int i = 0; i < 6; ++i) {
            support.restraints[i] = node.restraints[i] != 0;
        }
        supportsTmp.append(support);
    }

    resetModel();

    m_materials = materialsTmp;
    m_sections = sectionsTmp;
    m_supports = supportsTmp;
    m_nodalLoads = nodalLoadsTmp;
    m_memberLoads = memberLoadsTmp;

    if (!m_materials.isEmpty()) {
        m_lastMaterialId = m_materials.first().uuid;
    }
    if (!m_sections.isEmpty()) {
        m_lastSectionId = m_sections.first().uuid;
    }

    // maps for lookup
    QHash<int, QUuid> materialMap;
    for (MaterialInfo &mat : m_materials) {
        materialMap.insert(mat.externalId, mat.uuid);
        if (mat.name.trimmed().isEmpty()) {
            mat.name = tr("Material %1").arg(mat.externalId);
        }
    }
    QHash<int, QUuid> sectionMap;
    for (SectionInfo &sec : m_sections) {
        sectionMap.insert(sec.externalId, sec.uuid);
        if (sec.name.trimmed().isEmpty()) {
            sec.name = tr("Secao %1").arg(sec.externalId);
        }
    }

    QHash<int, QUuid> nodeUuidMap;
    for (const auto &node : nodesTmp) {
        const QUuid uuid = m_sceneController->addPointWithId(node.x, node.y, node.z, node.id);
        nodeUuidMap.insert(node.id, uuid);
    }

    for (const auto &member : membersTmp) {
        const QUuid startId = nodeUuidMap.value(member.nodeI);
        const QUuid endId = nodeUuidMap.value(member.nodeJ);
        if (startId.isNull() || endId.isNull()) {
            QMessageBox::warning(this, tr("Erro"), tr("Barra %1 referencia nos inexistentes").arg(member.id));
            continue;
        }
        const QUuid materialUuid = materialMap.value(member.materialId);
        const QUuid sectionUuid = sectionMap.value(member.sectionId);
        const QUuid barId = m_sceneController->addBar(startId, endId, materialUuid, sectionUuid);
        if (!barId.isNull()) {
            m_sceneController->setBarExternalId(barId, member.id);
        }
    }

    updateStatus();
    return true;
}

bool MainWindow::saveToDat(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Erro"), tr("Nao foi possivel salvar em %1").arg(QDir::toNativeSeparators(filePath)));
        return false;
    }

    QTextStream stream(&file);
    stream.setLocale(QLocale::c());

    // Ensure bar external ids exist
    auto bars = m_sceneController->bars();
    int nextBarId = nextBarExternalId();
    for (const auto &bar : bars) {
        if (bar.externalId <= 0) {
            m_sceneController->setBarExternalId(bar.id, nextBarId++);
        }
    }
    bars = m_sceneController->bars();

    // Prepare sorted lists
    auto materials = m_materials;
    std::sort(materials.begin(), materials.end(), [](const MaterialInfo &a, const MaterialInfo &b) {
        return a.externalId < b.externalId;
    });
    int maxMatId = 0;
    for (const auto &mat : materials) {
        if (mat.externalId > maxMatId) {
            maxMatId = mat.externalId;
        }
    }
    int nextMatId = maxMatId + 1;
    for (auto &mat : materials) {
        if (mat.externalId <= 0) {
            mat.externalId = nextMatId++;
        }
    }

    auto sections = m_sections;
    std::sort(sections.begin(), sections.end(), [](const SectionInfo &a, const SectionInfo &b) {
        return a.externalId < b.externalId;
    });
    int maxSecId = 0;
    for (const auto &sec : sections) {
        if (sec.externalId > maxSecId) {
            maxSecId = sec.externalId;
        }
    }
    int nextSecId = maxSecId + 1;
    for (auto &sec : sections) {
        if (sec.externalId <= 0) {
            sec.externalId = nextSecId++;
        }
    }

    // Update original containers with newly assigned IDs
    for (const auto &mat : materials) {
        for (MaterialInfo &orig : m_materials) {
            if (orig.uuid == mat.uuid) {
                orig.externalId = mat.externalId;
                break;
            }
        }
    }
    for (const auto &sec : sections) {
        for (SectionInfo &orig : m_sections) {
            if (orig.uuid == sec.uuid) {
                orig.externalId = sec.externalId;
                break;
            }
        }
    }

    const auto nodes = m_sceneController->nodeInfos();
    QHash<QUuid, SceneController::NodeInfo> nodeInfoMap;
    for (const auto &node : nodes) {
        nodeInfoMap.insert(node.id, node);
    }
    auto sortedNodes = nodes;
    std::sort(sortedNodes.begin(), sortedNodes.end(), [](const SceneController::NodeInfo &a, const SceneController::NodeInfo &b) {
        return a.externalId < b.externalId;
    });

    QVector<NodeSupport> supports = m_supports;
    // ensure support entries for all nodes
    for (const auto &node : sortedNodes) {
        bool found = false;
        for (const auto &sup : supports) {
            if (sup.nodeId == node.externalId) {
                found = true;
                break;
            }
        }
        if (!found) {
            NodeSupport sup{};
            sup.nodeId = node.externalId;
            sup.restraints[0] = sup.restraints[1] = sup.restraints[2] = sup.restraints[3] = sup.restraints[4] = sup.restraints[5] = false;
            supports.append(sup);
        }
    }

    auto members = bars;
    std::sort(members.begin(), members.end(), [](const SceneController::BarInfo &a, const SceneController::BarInfo &b) {
        return a.externalId < b.externalId;
    });

    stream << "[MATERIALS]\n";
    stream << "# ID    E (Pa)          G (Pa)\n";
    for (const auto &mat : materials) {
        stream << QString::asprintf("%-8d %14.6e %14.6e\n", mat.externalId, mat.youngModulus, mat.shearModulus);
    }
    stream << "\n";

    stream << "[SECTIONS]\n";
    stream << "# ID    A (m^2)     Iz (m^4)      Iy (m^4)      J (m^4)\n";
    for (const auto &sec : sections) {
        stream << QString::asprintf("%-8d %14.6e %14.6e %14.6e %14.6e\n", sec.externalId, sec.area, sec.iz, sec.iy, sec.j);
    }
    stream << "\n";

    stream << "[NODES]\n";
    stream << "# ID    X (m)    Y (m)    Z (m)    UX  UY  UZ  RX  RY  RZ\n";
    for (const auto &node : sortedNodes) {
        int restraints[6] = {0,0,0,0,0,0};
        for (const auto &sup : supports) {
            if (sup.nodeId == node.externalId) {
                for (int i = 0; i < 6; ++i) {
                    restraints[i] = sup.restraints[i] ? 1 : 0;
                }
                break;
            }
        }
        stream << QString::asprintf("%-8d %10.6f %10.6f %10.6f    %d   %d   %d   %d   %d   %d\n",
                                     node.externalId,
                                     node.x,
                                     node.y,
                                     node.z,
                                     restraints[0], restraints[1], restraints[2], restraints[3], restraints[4], restraints[5]);
    }
    stream << "\n";

    stream << "[MEMBERS]\n";
    stream << "# ID    Node_i   Node_j   Material_ID   Section_ID\n";
    for (const auto &bar : members) {
        const auto startIt = nodeInfoMap.constFind(bar.startNodeId);
        const auto endIt = nodeInfoMap.constFind(bar.endNodeId);
        const int startExternalId = (startIt != nodeInfoMap.constEnd()) ? startIt.value().externalId : 0;
        const int endExternalId = (endIt != nodeInfoMap.constEnd()) ? endIt.value().externalId : 0;
        const MaterialInfo *material = findMaterial(bar.materialId);
        const SectionInfo *section = findSection(bar.sectionId);
        const int materialId = material ? material->externalId : 0;
        const int sectionId = section ? section->externalId : 0;
        stream << QString::asprintf("%-8d %8d %8d %10d %12d\n",
                                     bar.externalId,
                                     startExternalId,
                                     endExternalId,
                                     materialId,
                                     sectionId);
    }
    stream << "\n";

    stream << "[NODAL_LOADS]\n";
    stream << "# Node_ID   Fx (N)    Fy (N)   Fz (N)   Mx (Nm)   My (Nm)   Mz (Nm)\n";
    for (const auto &load : m_nodalLoads) {
        stream << QString::asprintf("%-8d %10.6f %10.6f %10.6f %10.6f %10.6f %10.6f\n",
                                     load.nodeId, load.fx, load.fy, load.fz, load.mx, load.my, load.mz);
    }
    stream << "\n";

    stream << "[MEMBER_LOADS]\n";
    stream << "# Formato: Member_ID  Sistema(Local/Global)  qx (N/m)  qy (N/m)  qz (N/m)\n";
    stream << "# Sistemas aceitos: LOCAL, GLOBAL (ou L/G)\n";
    for (const auto &load : m_memberLoads) {
        QByteArray systemBytes = load.system.isEmpty() ? QByteArray("LOCAL") : load.system.toUpper().toLatin1();
        stream << QString::asprintf("%-12d %-10s %12.6f %12.6f %12.6f\n",
                                     load.memberId,
                                     systemBytes.constData(),
                                     load.qx, load.qy, load.qz);
    }

    file.close();
    return true;
}

void MainWindow::onSnapToggled(bool)
{
    if (m_command == Command::InsertNode) {
        updateStatus();
    }
}

void MainWindow::updateStatus()
{
    switch (m_command) {
    case Command::InsertNode:
        statusBar()->showMessage(tr("Insercao de nos: clique esquerdo para inserir (%1) | Esc para sair")
            .arg(m_snapCheck && m_snapCheck->isChecked() ? tr("snap ligado") : tr("snap desligado")));
        break;
    case Command::InsertBarFirst:
        statusBar()->showMessage(tr("Criar barra: selecione o primeiro no (Esc para cancelar)"));
        break;
    case Command::InsertBarSecond:
    {
        QString nodeLabel = QStringLiteral("?");
        if (!m_firstBarNodeId.isNull()) {
            if (const auto *node = m_sceneController->findNode(m_firstBarNodeId)) {
                nodeLabel = QString::number(node->externalId());
            }
        }
        statusBar()->showMessage(tr("Criar barra: selecione o segundo no (primeiro = N%1) | Esc para cancelar")
            .arg(nodeLabel));
        break;
    }
    default:
    {
        const int nodeCount = m_selectionModel ? m_selectionModel->selectedNodes().size() : 0;
        const int barCount = m_selectionModel ? m_selectionModel->selectedBars().size() : 0;
        if (nodeCount > 0 || barCount > 0) {
            statusBar()->showMessage(tr("Selecionados: %1 no(s), %2 barra(s)").arg(nodeCount).arg(barCount));
        } else {
            statusBar()->showMessage(tr("Pronto"));
        }
        break;
    }
    }
}
void MainWindow::onNodeCoordinateEdited(const QVector<QUuid> &ids, char axis, double value)
{
    if (!m_sceneController || !m_undoStack || ids.isEmpty()) {
        return;
    }

    QVector<QUuid> validIds;
    QVector<QVector3D> oldPositions;
    QVector<QVector3D> newPositions;

    for (const QUuid &id : ids) {
        const SceneController::Node *node = m_sceneController->findNode(id);
        if (!node) {
            continue;
        }
        const auto pos = node->position();
        QVector3D oldPos(pos[0], pos[1], pos[2]);
        QVector3D newPos = oldPos;
        switch (axis) {
        case 'x':
            newPos.setX(value);
            break;
        case 'y':
            newPos.setY(value);
            break;
        case 'z':
            newPos.setZ(value);
            break;
        default:
            return;
        }
        if (qFuzzyCompare(oldPos.x() + 1.0, newPos.x() + 1.0) &&
            qFuzzyCompare(oldPos.y() + 1.0, newPos.y() + 1.0) &&
            qFuzzyCompare(oldPos.z() + 1.0, newPos.z() + 1.0)) {
            continue;
        }
        validIds.append(id);
        oldPositions.append(oldPos);
        newPositions.append(newPos);
    }

    if (validIds.isEmpty()) {
        return;
    }

    auto *command = new MoveNodesCommand(m_sceneController, validIds, oldPositions, newPositions);
    m_undoStack->push(command);
    refreshPropertiesPanel();
}

void MainWindow::onBarMaterialEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &materialId)
{
    if (!m_sceneController || !m_undoStack || ids.isEmpty() || !materialId.has_value()) {
        return;
    }

    QVector<QUuid> validIds;
    QVector<QUuid> oldMaterials;
    QVector<QUuid> oldSections;
    bool changed = false;
    const QUuid newMaterial = materialId.value();

    for (const QUuid &id : ids) {
        const SceneController::Bar *bar = m_sceneController->findBar(id);
        if (!bar) {
            continue;
        }
        validIds.append(id);
        const QUuid oldMat = bar->materialId();
        oldMaterials.append(oldMat);
        oldSections.append(bar->sectionId());
        if (oldMat != newMaterial) {
            changed = true;
        }
    }

    if (validIds.isEmpty() || !changed) {
        return;
    }

    auto *command = new SetBarPropertiesCommand(m_sceneController,
                                                validIds,
                                                oldMaterials,
                                                oldSections,
                                                materialId,
                                                std::nullopt);
    m_undoStack->push(command);
    m_lastMaterialId = newMaterial;
    refreshPropertiesPanel();
}

void MainWindow::onBarSectionEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &sectionId)
{
    if (!m_sceneController || !m_undoStack || ids.isEmpty() || !sectionId.has_value()) {
        return;
    }

    QVector<QUuid> validIds;
    QVector<QUuid> oldMaterials;
    QVector<QUuid> oldSections;
    bool changed = false;
    const QUuid newSection = sectionId.value();

    for (const QUuid &id : ids) {
        const SceneController::Bar *bar = m_sceneController->findBar(id);
        if (!bar) {
            continue;
        }
        validIds.append(id);
        oldMaterials.append(bar->materialId());
        const QUuid oldSec = bar->sectionId();
        oldSections.append(oldSec);
        if (oldSec != newSection) {
            changed = true;
        }
    }

    if (validIds.isEmpty() || !changed) {
        return;
    }

    auto *command = new SetBarPropertiesCommand(m_sceneController,
                                                validIds,
                                                oldMaterials,
                                                oldSections,
                                                std::nullopt,
                                                sectionId);
    m_undoStack->push(command);
    m_lastSectionId = newSection;
    refreshPropertiesPanel();
}






