#include "MainWindow.h"

#include "AxesPanel.h"
#include "SectionPlacementPanel.h"
#include "PlacementController.h"
#include "PlacementOverlay.h"
#include "ViewOptionsBar.h"
#include "viz/postprocessor/PostProcessorRenderer.h"
#include "viz/SceneController.h"
#include "viz/construction/ConstructionScene.h"

#include "app/services/CombinationService.h"
#include "app/services/ConnectionPlacement.h"
#include "app/services/ConnectionSolids.h"
#include "app/services/DesignService.h"
#include "app/services/SectionPlacementService.h"
#include "viz/LocalCoordinateSystem.h"
#include <QHash>
#include <QRegularExpression>
#include <QVTKOpenGLNativeWidget.h>
#include <QToolButton>

#include <array>

namespace {

using Bar = Structura::UI::ViewOptionsBar;

const QStringList &toggleKeys()
{
    static const QStringList keys = {
        Bar::kGrid, Bar::kReferenceAxes, Bar::kNodeLabels, Bar::kBarLabels, Bar::kLocalAxes,
        Bar::kNodes, Bar::kSolids, Bar::kConnections, Bar::kConstraints, Bar::kAllLevels, Bar::kSupports, Bar::kNodeForces, Bar::kNodeMoments,
        Bar::kBarForces, Bar::kBarMoments, Bar::kHideLoadLabels, Bar::kAreaPanels, Bar::kSnap, Bar::kPlaneOnly,
    };
    return keys;
}

const QStringList &snapTypeKeys()
{
    static const QStringList keys = {
        Bar::kSnapGrid, Bar::kSnapEndpoint, Bar::kSnapCenter, Bar::kSnapPerpendicular, Bar::kSnapOrthogonal,
    };
    return keys;
}

const QString kGridSpacingKey = QStringLiteral("grid_spacing");

} // namespace

void MainWindow::onToggleAxesPanel()
{
    if (!m_axesPanel) {
        return;
    }
    if (m_axesPanel->isVisible()) {
        m_axesPanel->hide();
        return;
    }
    if (m_placementPanel) {
        m_placementPanel->hide();
    }
    m_axesPanel->setAxes(m_extras.axes,
                         m_sceneController->constructionScene() ? m_sceneController->constructionScene()->level() : 0.0);
    if (m_viewOptionsBar) {
        m_axesPanel->setSuggestedStep(m_viewOptionsBar->gridSpacing());
    }
    m_axesPanel->reposition();
    m_axesPanel->show();
    m_axesPanel->raise();
}

void MainWindow::rebuildCaseSelector()
{
    if (!m_viewOptionsBar) {
        return;
    }
    QStringList names;
    for (const auto &loadCase : Structura::App::Services::CombinationService::effectiveCases(m_extras)) {
        names << loadCase.name;
    }
    m_viewOptionsBar->setCases(names, m_extras.activeLoadCase);
}

void MainWindow::rebuildResultViewSelector()
{
    if (!m_viewOptionsBar) {
        return;
    }
    QStringList names;
    for (const auto &view : m_resultViews) {
        names << (view.isCombination ? view.name : tr("Caso: %1").arg(view.name));
    }
    m_viewOptionsBar->setResultViews(names, 0);
}

void MainWindow::syncConnectionVisuals()
{
    using Structura::App::Services::ConnectionSolids;
    if (!m_sceneController || !m_viewOptionsBar || !m_viewOptionsBar->isChecked(Bar::kConnections)) {
        return;
    }
    QHash<QUuid, std::array<double, 3>> positionById;
    for (const auto &node : m_sceneController->nodeInfos()) {
        positionById.insert(node.id, {node.x, node.y, node.z});
    }
    const double lengthFactor = Structura::App::Services::DesignService::unitFactors(m_materials).second;
    Structura::Geometry::DefaultLocalAxisProvider axisProvider;
    ConnectionSolids::Mesh mesh;
    const auto allBars = m_sceneController->bars();
    QHash<QUuid, const SectionInfo *> sectionsById;
    for (const auto &section : m_sections) {
        sectionsById.insert(section.uuid, &section);
    }

    // Eixos locais e pontos do eixo da barra (com giro da secao e excentricidade, como no solver e nos solidos)
    struct BarFrame {
        std::array<double, 3> start {}, end {}, x {}, y {}, z {};
        const SectionInfo *section {nullptr};
    };
    auto frameOf = [&](const SceneController::BarInfo &bar) -> std::optional<BarFrame> {
        const auto start = positionById.constFind(bar.startNodeId);
        const auto end = positionById.constFind(bar.endNodeId);
        if (start == positionById.constEnd() || end == positionById.constEnd()) {
            return std::nullopt;
        }
        std::optional<std::array<double, 3>> kPoint;
        if (const auto *model = m_sceneController->findBar(bar.id)) {
            if (model->hasKPoint()) {
                const auto &k = *model->kPoint();
                kPoint = std::array<double, 3> {k.x(), k.y(), k.z()};
            }
        }
        Structura::App::ProjectData::MemberPlacement placement;
        for (const auto &candidate : m_extras.placements) {
            if (candidate.memberId == bar.externalId) {
                placement = candidate;
                break;
            }
        }
        const auto lcs = Structura::Geometry::rotatedAboutX(axisProvider.computeLCS(*start, *end, kPoint), placement.rotationDeg);
        BarFrame frame;
        const auto sectionIt = sectionsById.constFind(bar.sectionId);
        frame.section = sectionIt == sectionsById.constEnd() ? nullptr : *sectionIt;
        const auto offset = Structura::App::Services::SectionPlacementService::axisOffset(frame.section, placement);
        for (std::size_t i = 0; i < 3; ++i) {
            const double shift = lcs.yPrime[i] * offset.dy + lcs.zPrime[i] * offset.dz;
            frame.start[i] = (*start)[i] + shift;
            frame.end[i] = (*end)[i] + shift;
        }
        frame.x = lcs.xPrime;
        frame.y = lcs.yPrime;
        frame.z = lcs.zPrime;
        return frame;
    };

    for (const auto &connection : m_extras.connections) {
        const SceneController::BarInfo *bar = nullptr;
        for (const auto &candidate : allBars) {
            if (candidate.externalId == connection.memberId) {
                bar = &candidate;
                break;
            }
        }
        if (!bar) {
            continue;
        }
        const auto beam = frameOf(*bar);
        if (!beam) {
            continue;
        }
        ConnectionSolids::Frame frame;
        const double sign = connection.atEnd ? -1.0 : 1.0;   // s aponta do no para dentro da barra
        frame.origin = connection.atEnd ? beam->end : beam->start;   // eixo da barra no no (ja com a excentricidade)
        for (std::size_t i = 0; i < 3; ++i) {
            frame.s[i] = sign * beam->x[i];
            frame.t[i] = beam->y[i];
            frame.u[i] = beam->z[i];
        }
        // s = 0 fica na face do apoio: plano da mesa/alma da barra de apoio escolhida, na posicao real dela
        double supportOffset = connection.values.value(QStringLiteral("supOffset"), 0.0) / lengthFactor;
        const int supportId = static_cast<int>(connection.values.value(QStringLiteral("supMember"), 0.0) + 0.5);
        if (supportId > 0) {
            for (const auto &candidate : allBars) {
                if (candidate.externalId != supportId) {
                    continue;
                }
                const auto host = frameOf(candidate);
                if (!host || !host->section) {
                    break;
                }
                const auto &g = host->section->geometry;
                Structura::App::Services::SupportSection support;
                support.axisPoint = host->start;
                support.y = host->y;
                support.z = host->z;
                support.depth = g.value(QStringLiteral("d"), g.value(QStringLiteral("D"), 0.0));
                support.width = g.value(QStringLiteral("bf"), g.value(QStringLiteral("b"), support.depth));
                support.webThickness = g.value(QStringLiteral("tw"), 0.0);
                support.isIShape = g.contains(QStringLiteral("tf")) && g.contains(QStringLiteral("tw"));
                double computed = 0.0;
                if (Structura::App::Services::supportFaceOffset(frame.origin, frame.s, support, 1.0 / lengthFactor, computed)) {
                    supportOffset = computed;   // viga quase paralela ao apoio: mantem o afastamento gravado
                }
                break;
            }
        }
        for (std::size_t i = 0; i < 3; ++i) {
            frame.origin[i] += frame.s[i] * supportOffset;
        }
        const auto primitives = ConnectionSolids::build(connection);
        frame.scale = 1.0 / lengthFactor;
        ConnectionSolids::append(mesh, primitives, frame, connection.verified ? std::max(connection.eta, 1e-6) : 0.0);
    }
    Structura::Visualization::ConnectionRenderer::Mesh out;
    out.points = std::move(mesh.points);
    out.normals = std::move(mesh.normals);
    out.colors = std::move(mesh.colors);
    out.triangles = std::move(mesh.triangles);
    m_sceneController->setConnectionMesh(out);
}

void MainWindow::syncSolidVisuals()
{
    if (!m_sceneController || !m_viewOptionsBar || !m_viewOptionsBar->isChecked(Bar::kSolids)) {
        return;
    }
    QHash<QUuid, const SectionInfo *> sectionsById;
    for (const auto &section : m_sections) {
        sectionsById.insert(section.uuid, &section);
    }
    QHash<QUuid, std::array<double, 3>> positionById;
    for (const auto &node : m_sceneController->nodeInfos()) {
        positionById.insert(node.id, {node.x, node.y, node.z});
    }

    Structura::Geometry::DefaultLocalAxisProvider axisProvider;
    QVector<SceneController::SolidMemberVisual> visuals;
    for (const auto &bar : m_sceneController->bars()) {
        const auto sectionIt = sectionsById.constFind(bar.sectionId);
        if (sectionIt == sectionsById.constEnd() || (*sectionIt)->family.isEmpty()) {
            continue;
        }
        const auto appearance = m_barAppearance.constFind(bar.id);
        if (appearance != m_barAppearance.constEnd() && appearance->state == Structura::Visualization::ElementAppearance::State::Hidden) {
            continue;
        }
        const auto start = positionById.constFind(bar.startNodeId);
        const auto end = positionById.constFind(bar.endNodeId);
        if (start == positionById.constEnd() || end == positionById.constEnd()) {
            continue;
        }
        std::optional<std::array<double, 3>> kPoint;
        if (const auto *model = m_sceneController->findBar(bar.id)) {
            if (model->hasKPoint()) {
                const auto &k = *model->kPoint();
                kPoint = std::array<double, 3> {k.x(), k.y(), k.z()};
            }
        }
        // Giro da secao + excentricidade (centroide deslocado do no), como no solver.
        Structura::App::ProjectData::MemberPlacement placement;
        for (const auto &candidate : m_extras.placements) {
            if (candidate.memberId == bar.externalId) {
                placement = candidate;
                break;
            }
        }
        const auto lcs = Structura::Geometry::rotatedAboutX(axisProvider.computeLCS(*start, *end, kPoint),
                                                            placement.rotationDeg);
        const auto offset = Structura::App::Services::SectionPlacementService::axisOffset(sectionIt.value(), placement);
        auto toVector = [](const std::array<double, 3> &v) {
            return QVector3D(static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2]));
        };
        const QVector3D axisShift = toVector(lcs.yPrime) * static_cast<float>(offset.dy)
            + toVector(lcs.zPrime) * static_cast<float>(offset.dz);
        SceneController::SolidMemberVisual visual;
        visual.barId = bar.id;
        visual.start = toVector(*start) + axisShift;
        visual.end = toVector(*end) + axisShift;
        visual.xAxis = toVector(lcs.xPrime);
        visual.yAxis = toVector(lcs.yPrime);
        visual.zAxis = toVector(lcs.zPrime);
        visual.family = (*sectionIt)->family;
        visual.parameters = (*sectionIt)->geometry;
        visual.mirrored = (*sectionIt)->mirrored;
        if (appearance != m_barAppearance.constEnd()) {
            visual.dimmed = appearance->state == Structura::Visualization::ElementAppearance::State::Dimmed;
            if (appearance->hasColor) {
                visual.hasColor = true;
                visual.color[0] = appearance->color[0];
                visual.color[1] = appearance->color[1];
                visual.color[2] = appearance->color[2];
            }
        }
        visuals.append(visual);
    }
    m_sceneController->setSolidMembers(visuals);
}

void MainWindow::refreshLevelSelector()
{
    if (!m_viewOptionsBar || !m_sceneController || !m_sceneController->constructionScene()) {
        return;
    }
    auto *construction = m_sceneController->constructionScene();
    const auto normal = Structura::Construction::planeAxes(construction->planeMode()).normal;
    const auto values = construction->axes().planeLevels(normal);
    QStringList texts;
    for (const double value : values) {
        const QString number = QString::number(value, 'f', 3).remove(QRegularExpression(QStringLiteral("\\.?0+$")));
        if (normal == 2) {
            const auto *level = construction->axes().levelAt(value);
            texts << (level ? QStringLiteral("%1  (%2 m)").arg(level->name, number) : QStringLiteral("Z = %1 m").arg(number));
        } else {
            texts << QStringLiteral("%1 = %2 m").arg(normal == 1 ? QStringLiteral("Y") : QStringLiteral("X"), number);
        }
    }
    m_viewOptionsBar->setLevels(texts, values, construction->level());
}

void MainWindow::refreshElementLabels()
{
    if (!m_labelOverlay || !m_sceneController || !m_viewOptionsBar) {
        return;
    }
    const auto nodes = m_sceneController->nodeInfos();
    std::vector<std::array<double, 3>> nodePositions;
    QVector<QString> nodeTexts;
    nodePositions.reserve(nodes.size());
    QHash<QUuid, std::array<double, 3>> positionById;
    for (const auto &node : nodes) {
        nodePositions.push_back({node.x, node.y, node.z});
        nodeTexts.append(QString::number(node.externalId));
        positionById.insert(node.id, {node.x, node.y, node.z});
    }
    m_labelOverlay->setGroup(QStringLiteral("node"), nodePositions, nodeTexts,
                             m_viewOptionsBar->isChecked(Bar::kNodeLabels));

    std::vector<std::array<double, 3>> barPositions;
    QVector<QString> barTexts;
    for (const auto &bar : m_sceneController->bars()) {
        const auto start = positionById.constFind(bar.startNodeId);
        const auto end = positionById.constFind(bar.endNodeId);
        if (start == positionById.constEnd() || end == positionById.constEnd()) {
            continue;
        }
        barPositions.push_back({(start->at(0) + end->at(0)) / 2.0,
                                (start->at(1) + end->at(1)) / 2.0,
                                (start->at(2) + end->at(2)) / 2.0});
        barTexts.append(QString::number(bar.externalId));
    }
    m_labelOverlay->setGroup(QStringLiteral("bar"), barPositions, barTexts,
                             m_viewOptionsBar->isChecked(Bar::kBarLabels));
    m_labelOverlay->sync(m_sceneController->compositeMatrix());
}

void MainWindow::applyViewOption(const QString &key, bool on)
{
    auto *construction = m_sceneController ? m_sceneController->constructionScene() : nullptr;
    if (!construction) {
        return;
    }
    if (key == Bar::kGrid) {
        construction->setGridVisible(on);
        if (m_placement) {
            m_placement->scheduleRefresh();
        }
    } else if (key == Bar::kReferenceAxes) {
        construction->setReferenceAxesVisible(on);
    } else if (key == Bar::kNodeLabels) {
        m_labelOverlay->setGroupVisible(QStringLiteral("node"), on);
    } else if (key == Bar::kBarLabels) {
        m_labelOverlay->setGroupVisible(QStringLiteral("bar"), on);
    } else if (key == Bar::kLocalAxes) {
        if (m_showBarLCSToolButton && m_showBarLCSToolButton->isChecked() != on) {
            m_showBarLCSToolButton->setChecked(on);
        }
    } else if (key == Bar::kNodes) {
        m_sceneController->setNodesVisible(on);
    } else if (key == Bar::kConstraints) {
        m_sceneController->setConstraintsVisible(on);
    } else if (key == Bar::kSupports) {
        m_sceneController->setSupportsVisible(on);
    } else if (key == Bar::kAllLevels) {
        construction->setShowAllLevels(on);
        if (m_placement) {
            m_placement->scheduleRefresh();
        }
    } else if (key == Bar::kHideLoadLabels) {
        applyLoadVisibility();
    } else if (key == Bar::kPlaneOnly) {
        applyAppearance();
    } else if (key == Bar::kConnections) {
        m_sceneController->setConnectionsVisible(on);
        syncConnectionVisuals();
    } else if (key == Bar::kSolids) {
        m_sceneController->setSolidsVisible(on);
        syncSolidVisuals();
        syncConnectionVisuals();
    } else if (key == Bar::kNodeForces || key == Bar::kNodeMoments
               || key == Bar::kBarForces || key == Bar::kBarMoments) {
        applyLoadVisibility();
    } else if (key == Bar::kAreaPanels) {
        applyLoadVisibility();
    } else if (key == Bar::kSnap) {
        construction->snap().enabled = on;
        if (m_placement) {
            m_placement->scheduleRefresh();
        }
    }
    // Opcoes de carga (forcas/momentos) sao aplicadas pela visualizacao de cargas.
}

void MainWindow::storeViewOptionsInExtras()
{
    if (!m_viewOptionsBar) {
        return;
    }
    for (const QString &key : toggleKeys()) {
        m_extras.viewOptions.insert(key, m_viewOptionsBar->isChecked(key) ? 1.0 : 0.0);
    }
    for (const QString &key : snapTypeKeys()) {
        m_extras.viewOptions.insert(key, m_viewOptionsBar->isSnapTypeChecked(key) ? 1.0 : 0.0);
    }
    m_extras.viewOptions.insert(kGridSpacingKey, m_viewOptionsBar->gridSpacing());
    if (m_sceneController) {
        m_extras.viewOptions.insert(QStringLiteral("camera_perspective"), m_sceneController->isPerspective() ? 1.0 : 0.0);
    }
}

void MainWindow::applyViewOptionsFromExtras()
{
    if (!m_viewOptionsBar || !m_sceneController) {
        return;
    }
    auto *construction = m_sceneController->constructionScene();
    const auto &options = m_extras.viewOptions;
    for (const QString &key : toggleKeys()) {
        if (options.contains(key)) {
            const bool on = options.value(key) != 0.0;
            m_viewOptionsBar->setChecked(key, on);
            applyViewOption(key, on);
        }
    }
    for (const QString &key : snapTypeKeys()) {
        if (options.contains(key)) {
            const bool on = options.value(key) != 0.0;
            m_viewOptionsBar->setSnapTypeChecked(key, on);
            emit m_viewOptionsBar->snapTypeToggled(key, on);
        }
    }
    if (m_sceneController) {
        // padrao: isometrica (paralela); arquivos antigos sem a chave tambem abrem em isometrica
        m_sceneController->setPerspective(options.value(QStringLiteral("camera_perspective"), 0.0) != 0.0);
    }
    if (options.contains(kGridSpacingKey) && construction) {
        const double spacing = options.value(kGridSpacingKey);
        if (spacing > 0.0) {
            m_viewOptionsBar->setGridSpacing(spacing);
            construction->setSpacing(spacing);
        }
    }
}

void MainWindow::setupViewOptions()
{
    if (!m_viewOptionsBar || !m_sceneController || !m_vtkWidget) {
        return;
    }
    auto *construction = m_sceneController->constructionScene();
    if (!construction) {
        return;
    }

    m_placementPanel = new Structura::UI::SectionPlacementPanel(m_vtkWidget);
    connect(m_placementPanel, &Structura::UI::SectionPlacementPanel::placementEdited, this,
            &MainWindow::onBarPlacementEdited);

    m_axesPanel = new Structura::UI::AxesPanel(m_vtkWidget);
    connect(m_axesPanel, &Structura::UI::AxesPanel::axesCommitted, this,
            [this, construction](const Structura::App::ProjectData::ReferenceAxes &axes) {
                m_extras.axes = axes;
                construction->setAxes(axes);
            });

    connect(m_axesPanel, &Structura::UI::AxesPanel::levelActivated, this, [construction](double elevation) {
        if (construction->planeMode() == Structura::Construction::PlaneMode::XY) {
            construction->setLevel(elevation);
        }
    });

    connect(m_viewOptionsBar, &Structura::UI::ViewOptionsBar::toggled, this,
            [this](const QString &key, bool on) { applyViewOption(key, on); });
    connect(m_viewOptionsBar, &Structura::UI::ViewOptionsBar::snapTypeToggled, this,
            [this, construction](const QString &key, bool on) {
                auto &snap = construction->snap();
                if (key == Bar::kSnapGrid) {
                    snap.grid = on;
                } else if (key == Bar::kSnapEndpoint) {
                    snap.endpoint = on;
                } else if (key == Bar::kSnapCenter) {
                    snap.center = on;
                } else if (key == Bar::kSnapPerpendicular) {
                    snap.perpendicular = on;
                } else if (key == Bar::kSnapOrthogonal) {
                    snap.orthogonal = on;
                }
                if (m_placement) {
                    m_placement->scheduleRefresh();
                }
            });
    connect(m_viewOptionsBar, &Structura::UI::ViewOptionsBar::activeCaseChanged, this,
            [this](const QString &name) {
                m_extras.activeLoadCase = name;
                syncLoadVisuals();
            });
    connect(m_viewOptionsBar, &Structura::UI::ViewOptionsBar::resultViewChanged, this,
            [this](int index) { showResultView(index); });
    connect(m_viewOptionsBar, &Structura::UI::ViewOptionsBar::gridSpacingChanged, this,
            [construction](double spacing) { construction->setSpacing(spacing); });
    connect(m_viewOptionsBar, &Structura::UI::ViewOptionsBar::previousPlaneRequested, this,
            [construction]() { construction->stepLevel(-1); });
    connect(m_viewOptionsBar, &Structura::UI::ViewOptionsBar::nextPlaneRequested, this,
            [construction]() { construction->stepLevel(1); });
    connect(m_viewOptionsBar, &Structura::UI::ViewOptionsBar::cyclePlaneRequested, this, [construction]() {
        using Structura::Construction::PlaneMode;
        switch (construction->planeMode()) {
        case PlaneMode::XY:
            construction->setPlaneMode(PlaneMode::XZ);
            break;
        case PlaneMode::XZ:
            construction->setPlaneMode(PlaneMode::YZ);
            break;
        case PlaneMode::YZ:
        default:
            construction->setPlaneMode(PlaneMode::XY);
            break;
        }
    });

    auto previousPlaneChanged = construction->onPlaneChanged;
    construction->onPlaneChanged = [this, construction, previousPlaneChanged]() {
        if (previousPlaneChanged) {
            previousPlaneChanged();
        }
        m_viewOptionsBar->setPlaneName(Structura::Construction::planeName(construction->planeMode()));
        refreshLevelSelector();
        updatePlaneLabel();
        // O isolamento depende do nivel/plano ativo.
        applyAppearance();
    };
    connect(m_viewOptionsBar, &Structura::UI::ViewOptionsBar::levelSelected, this,
            [this, construction](double value) {
                construction->setLevel(value);
                applyAppearance();
            });
    using Opts = Structura::UI::ViewOptionsBar;
    connect(m_viewOptionsBar, &Opts::planViewRequested, this, [this, construction]() {
        m_sceneController->lookAtPlane(Structura::Construction::planeAxes(construction->planeMode()).normal,
                                       construction->level());
    });
    updatePlaneLabel();
    connect(m_viewOptionsBar, &Opts::selectionFilterRequested, this, &MainWindow::onSelectionFilter);
    connect(m_viewOptionsBar, &Opts::selectMatchingRequested, this, &MainWindow::onSelectMatching);
    connect(m_viewOptionsBar, &Opts::invertSelectionRequested, this, &MainWindow::onInvertSelection);
    connect(m_viewOptionsBar, &Opts::clearSelectionRequested, this, [this]() { m_selectionModel->clear(); });
    connect(m_viewOptionsBar, &Opts::loadsPanelRequested, this, &MainWindow::onShowLoadsPanel);
    connect(m_viewOptionsBar, &Opts::colorModeChanged, this, [this](int) { applyAppearance(); });
    connect(m_sceneController, &SceneController::geometryRefreshed, this, [this]() {
        static bool busy = false;
        if (busy) {
            return;
        }
        busy = true;
        applyAppearance();
        syncBarTypeMarkers();
        busy = false;
    });

    // Sincroniza o botao lateral de eixos locais (LCS) com a barra de exibicao.
    if (m_showBarLCSToolButton) {
        connect(m_showBarLCSToolButton, &QToolButton::toggled, this, [this](bool on) {
            m_viewOptionsBar->setChecked(Bar::kLocalAxes, on);
        });
    }

    connect(m_sceneController, &SceneController::geometryRefreshed, this, &MainWindow::refreshElementLabels);
    connect(m_sceneController, &SceneController::geometryRefreshed, this, &MainWindow::syncSolidVisuals);
    connect(m_sceneController, &SceneController::geometryRefreshed, this, &MainWindow::syncConnectionVisuals);
    connect(m_sceneController, &SceneController::loadLabelsChanged, this, [this]() {
        const auto labels = m_sceneController->loadLabels();
        QVector<QString> texts;
        texts.reserve(static_cast<int>(labels.texts.size()));
        for (const QString &text : labels.texts) {
            texts.append(text);
        }
        std::vector<QColor> colors;
        colors.reserve(labels.colors.size());
        for (const auto &c : labels.colors) {
            colors.emplace_back(QColor::fromRgbF(c[0], c[1], c[2]));
        }
        m_labelOverlay->setGroup(QStringLiteral("action"), labels.positions, texts, !diagramsShowing() && !m_viewOptionsBar->isChecked(Bar::kHideLoadLabels), colors, labels.weights, true);
        m_labelOverlay->sync(m_sceneController->compositeMatrix());
    });

    // Estado inicial dos elementos visuais conforme os botoes.
    for (const QString &key : toggleKeys()) {
        applyViewOption(key, m_viewOptionsBar->isChecked(key));
    }
    m_viewOptionsBar->setPlaneName(Structura::Construction::planeName(construction->planeMode()));
    refreshLevelSelector();
    if (m_extras.loadCases.isEmpty()) {
        m_extras.loadCases.append({Structura::App::ProjectData::defaultLoadCaseName(),
                                   Structura::App::ProjectData::defaultLoadCaseName(), false});
    }
    rebuildCaseSelector();
    refreshElementLabels();
}

bool MainWindow::diagramsShowing() const
{
    return m_postProcessor && m_postProcessor->isActive() && m_postProcessor->currentDiagram() != Structura::Visualization::DiagramType::None;
}

/// Carregamentos (setas, paineis e valores) ficam ocultos enquanto um diagrama de esforcos esta na tela, para nao poluir a leitura.
void MainWindow::applyLoadVisibility()
{
    using Bar = Structura::UI::ViewOptionsBar;
    if (!m_sceneController || !m_viewOptionsBar) {
        return;
    }
    const bool hide = diagramsShowing();
    const bool nodeForces = !hide && m_viewOptionsBar->isChecked(Bar::kNodeForces);
    const bool nodeMoments = !hide && m_viewOptionsBar->isChecked(Bar::kNodeMoments);
    const bool barForces = !hide && m_viewOptionsBar->isChecked(Bar::kBarForces);
    const bool barMoments = !hide && m_viewOptionsBar->isChecked(Bar::kBarMoments);
    m_sceneController->setLoadVisibility(nodeForces, nodeMoments, barForces, barMoments);
    m_sceneController->setAreaPanelsVisible(!hide && m_viewOptionsBar->isChecked(Bar::kAreaPanels));
    if (m_labelOverlay) {
        m_labelOverlay->setGroupVisible(QStringLiteral("action"), !hide && !m_viewOptionsBar->isChecked(Bar::kHideLoadLabels) && (nodeForces || nodeMoments || barForces || barMoments));
    }
}
