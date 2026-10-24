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
#include "viz/structure/SectionShapes.h"
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

    createActions();

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
    for (auto &entry : entries) {
        const QString family = entry.section.family;
        entry.compositeSection = entry.hasSection
            && (family == QLatin1String("Dupla Cantoneira T") || family == QLatin1String("Duplo U Costas") || family == QLatin1String("Duplo U Face a Face"));
        if (entry.compositeSection) {
            const auto params = m_extras.design.paramsFor(entry.externalId);
            entry.compositeGapMm = entry.section.geometry.value(QStringLiteral("e"));
            entry.compositeSpacingMm = params.compositeSpacing;
            entry.compositeWelded = params.compositeWelded;
        }
    }
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

void MainWindow::onBarCompositeGapEdited(const QVector<QUuid> &ids, double gapMm)
{
    using Structura::Visualization::SectionShapes;
    StateGuard guard(this, tr("Afastamento da barra composta"));
    QHash<QUuid, QVector<QUuid>> barsBySection;
    for (const QUuid &id : ids) {
        if (const auto *bar = m_sceneController->findBar(id)) {
            barsBySection[bar->sectionId()].append(id);
        }
    }
    for (auto it = barsBySection.constBegin(); it != barsBySection.constEnd(); ++it) {
        const auto source = std::find_if(m_sections.begin(), m_sections.end(), [&](const SectionInfo &s) { return s.uuid == it.key(); });
        if (source == m_sections.end() || !SectionShapes::isComposite(source->family)) {
            continue;
        }
        SectionInfo variant = *source;
        if (std::abs(variant.geometry.value(QStringLiteral("e")) - gapMm) < 1e-9) {
            continue;
        }
        variant.geometry[QStringLiteral("e")] = gapMm;
        // a secao e exclusiva das barras editadas? (nenhuma outra barra a usa)
        int users = 0;
        for (const auto &bar : m_sceneController->bars()) {
            users += bar.sectionId == it.key() ? 1 : 0;
        }
        const bool exclusive = users <= it.value().size();
        auto recalculate = [&](SectionInfo &section) {
            QString error;
            const auto properties = SectionShapes::properties(section.family, section.geometry, &error);
            if (!properties) {
                QMessageBox::warning(this, tr("Afastamento"), tr("Não foi possível recalcular a seção: %1").arg(error));
                return false;
            }
            section.area = properties->area;
            section.iz = properties->iz;
            section.iy = properties->iy;
            section.j = properties->j;
            return true;
        };
        QString baseName = source->name;
        const int marker = baseName.indexOf(QStringLiteral(" e="));
        if (marker > 0) {
            baseName = baseName.left(marker);
        }
        auto uniqueName = [&](const QUuid &self) {
            QString name = QStringLiteral("%1 e=%2").arg(baseName, QString::number(gapMm, 'g', 5));
            const QString root = name;
            for (int suffix = 2; std::any_of(m_sections.begin(), m_sections.end(), [&](const SectionInfo &s) { return s.uuid != self && s.name.compare(name, Qt::CaseInsensitive) == 0; }); ++suffix) {
                name = QStringLiteral("%1 (%2)").arg(root).arg(suffix);
            }
            return name;
        };
        // reutiliza outra secao ja existente com a mesma geometria
        QUuid target;
        for (const auto &section : m_sections) {
            if (section.uuid != it.key() && section.family == variant.family && section.mirrored == variant.mirrored && section.geometry == variant.geometry) {
                target = section.uuid;
                break;
            }
        }
        if (target.isNull() && exclusive) {
            // so estas barras usam a secao: ela propria e atualizada (nenhuma secao nova)
            const QUuid self = source->uuid;
            if (!recalculate(variant)) {
                continue;
            }
            variant.name = uniqueName(self);
            *source = variant;
            target = self;
        } else if (target.isNull()) {
            if (!recalculate(variant)) {
                continue;
            }
            variant.uuid = QUuid::createUuid();
            variant.externalId = nextSectionExternalId();
            variant.name = uniqueName(variant.uuid);
            m_sections.append(variant);
            target = variant.uuid;
        }
        m_presenter->handleBarSectionEdited(it.value(), target);
        if (target != it.key() && exclusive) {
            m_sections.erase(std::remove_if(m_sections.begin(), m_sections.end(), [&](const SectionInfo &section) { return section.uuid == it.key(); }), m_sections.end());
        }
    }
    syncProfileManager();
    refreshPropertiesPanel();
    applyAppearance();
}

void MainWindow::onBarCompositeEdited(const QVector<QUuid> &ids, double spacingMm, bool welded)
{
    StateGuard guard(this, tr("Espaçamento da barra composta"));
    for (const QUuid &id : ids) {
        const auto *bar = m_sceneController->findBar(id);
        if (!bar) {
            continue;
        }
        auto params = m_extras.design.paramsFor(bar->externalId());
        params.compositeSpacing = spacingMm;
        params.compositeWelded = welded;
        auto it = std::find_if(m_extras.design.members.begin(), m_extras.design.members.end(), [&](const auto &m) { return m.memberId == bar->externalId(); });
        if (it != m_extras.design.members.end()) {
            *it = params;
        } else {
            m_extras.design.members.append(params);
        }
    }
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






