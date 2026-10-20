#include "MainWindow.h"
#include "DesignColors.h"
#include "SvgIcons.h"
#include "ViewCubeWidget.h"

#include "SelectionModel.h"
#include "ViewOptionsBar.h"
#include "viz/SceneController.h"
#include "viz/construction/ConstructionScene.h"

#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>

#include <array>
#include <functional>
#include <cmath>

namespace {

using Structura::Visualization::AppearanceMap;
using Structura::Visualization::ElementAppearance;

constexpr double kLevelTolerance = 1e-6;

QColor paletteColor(int index)
{
    static const char *colors[] = {"#4e79a7", "#f28e2b", "#e15759", "#76b7b2", "#59a14f", "#edc948",
                                   "#b07aa1", "#ff9da7", "#9c755f", "#86bc86", "#d37295", "#8cd17d"};
    return QColor(QString::fromLatin1(colors[index % 12]));
}

std::array<unsigned char, 3> toArray(const QColor &color)
{
    return {{static_cast<unsigned char>(color.red()), static_cast<unsigned char>(color.green()),
             static_cast<unsigned char>(color.blue())}};
}

} // namespace

QString MainWindow::positionKey(double x, double y, double z)
{
    return QStringLiteral("%1,%2,%3").arg(std::llround(x * 1e4)).arg(std::llround(y * 1e4)).arg(std::llround(z * 1e4));
}

bool MainWindow::isNodeHidden(const QUuid &id) const
{
    return m_effHiddenNodes.contains(id);
}

bool MainWindow::isBarHidden(const QUuid &id) const
{
    return m_effHiddenBars.contains(id);
}

void MainWindow::onHideSelected()
{
    for (const QUuid &id : m_selectionModel->selectedNodes()) {
        m_hiddenNodes.insert(id);
    }
    for (const QUuid &id : m_selectionModel->selectedBars()) {
        m_hiddenBars.insert(id);
    }
    m_selectionModel->clear();
    applyAppearance();
}

void MainWindow::onIsolateSelected()
{
    QSet<QUuid> keepNodes = m_selectionModel->selectedNodes();
    const QSet<QUuid> keepBars = m_selectionModel->selectedBars();
    for (const auto &bar : m_sceneController->bars()) {
        if (keepBars.contains(bar.id)) {
            keepNodes.insert(bar.startNodeId);
            keepNodes.insert(bar.endNodeId);
        }
    }
    m_hiddenNodes.clear();
    m_hiddenBars.clear();
    for (const auto &node : m_sceneController->nodeInfos()) {
        if (!keepNodes.contains(node.id)) {
            m_hiddenNodes.insert(node.id);
        }
    }
    for (const auto &bar : m_sceneController->bars()) {
        if (!keepBars.contains(bar.id)) {
            m_hiddenBars.insert(bar.id);
        }
    }
    applyAppearance();
}

void MainWindow::onShowAllElements()
{
    m_hiddenNodes.clear();
    m_hiddenBars.clear();
    applyAppearance();
}

void MainWindow::applyAppearance()
{
    if (!m_sceneController || !m_viewOptionsBar) {
        return;
    }
    auto *construction = m_sceneController->constructionScene();
    const bool planeOnly = m_viewOptionsBar->isChecked(Structura::UI::ViewOptionsBar::kPlaneOnly);
    const int colorMode = m_viewOptionsBar->colorMode();
    const int normal = construction ? Structura::Construction::planeAxes(construction->planeMode()).normal : 2;
    const double level = construction ? construction->level() : 0.0;

    const auto nodeInfos = m_sceneController->nodeInfos();
    QHash<QUuid, Structura::App::SceneNodeInfo> nodeById;
    for (const auto &node : nodeInfos) {
        nodeById.insert(node.id, node);
    }
    auto atLevel = [&](const Structura::App::SceneNodeInfo &node) {
        const double coordinate = normal == 0 ? node.x : (normal == 1 ? node.y : node.z);
        return std::abs(coordinate - level) <= kLevelTolerance;
    };

    AppearanceMap nodes;
    m_effHiddenNodes.clear();
    m_effHiddenBars.clear();
    m_offNodeKeys.clear();
    m_offBarKeys.clear();
    m_hiddenNodeExt.clear();
    m_hiddenBarExt.clear();
    for (const auto &node : nodeInfos) {
        ElementAppearance appearance;
        if (m_hiddenNodes.contains(node.id)) {
            appearance.state = ElementAppearance::State::Hidden;
        } else if (planeOnly && !atLevel(node)) {
            appearance.state = ElementAppearance::State::Hidden;
        }
        if (appearance.state != ElementAppearance::State::Normal) {
            m_offNodeKeys.insert(positionKey(node.x, node.y, node.z));
        }
        if (appearance.state == ElementAppearance::State::Hidden) {
            m_effHiddenNodes.insert(node.id);
            m_hiddenNodeExt.insert(node.externalId);
        }
        nodes.insert(node.id, appearance);
    }

    // Cores por secao/material/aproveitamento e legenda.
    QHash<int, QColor> etaColorOf;
    QHash<QUuid, QColor> colorOf;
    QVector<QPair<QString, QColor>> legend;
    if (colorMode == 1) {
        int index = 0;
        for (const auto &section : m_sections) {
            colorOf.insert(section.uuid, paletteColor(index));
            legend.append({section.name, paletteColor(index)});
            ++index;
        }
    } else if (colorMode == 2) {
        int index = 0;
        for (const auto &material : m_materials) {
            colorOf.insert(material.uuid, paletteColor(index));
            legend.append({material.name, paletteColor(index)});
            ++index;
        }
    }

    if (colorMode == 3) {
        legend.append({tr("η ≤ 0,70"), Structura::UI::designEtaColor(0.5)});
        legend.append({tr("0,70 < η ≤ 0,90"), Structura::UI::designEtaColor(0.8)});
        legend.append({tr("0,90 < η ≤ 1,00"), Structura::UI::designEtaColor(0.95)});
        legend.append({tr("η > 1,00 (não atende)"), Structura::UI::designEtaColor(1.2)});
        legend.append({tr("sem resultado"), QColor(QStringLiteral("#9aa3b0"))});
    }

    AppearanceMap bars;
    for (const auto &bar : m_sceneController->bars()) {
        ElementAppearance appearance;
        const auto a = nodeById.constFind(bar.startNodeId);
        const auto b = nodeById.constFind(bar.endNodeId);
        const bool endHidden = (a != nodeById.constEnd() && m_effHiddenNodes.contains(a->id))
                               || (b != nodeById.constEnd() && m_effHiddenNodes.contains(b->id));
        if (m_hiddenBars.contains(bar.id) || endHidden) {
            appearance.state = ElementAppearance::State::Hidden;
        } else if (planeOnly && a != nodeById.constEnd() && b != nodeById.constEnd() && !(atLevel(*a) && atLevel(*b))) {
            appearance.state = ElementAppearance::State::Hidden;
        }
        if (appearance.state != ElementAppearance::State::Normal && a != nodeById.constEnd() && b != nodeById.constEnd()) {
            m_offBarKeys.insert(positionKey(a->x, a->y, a->z) + QLatin1Char('|') + positionKey(b->x, b->y, b->z));
            m_offBarKeys.insert(positionKey(b->x, b->y, b->z) + QLatin1Char('|') + positionKey(a->x, a->y, a->z));
        }
        if (colorMode == 3) {
            const auto etaIt = m_designEta.constFind(bar.externalId);
            appearance.hasColor = true;
            appearance.color = toArray(etaIt == m_designEta.constEnd() ? QColor(QStringLiteral("#9aa3b0")) : Structura::UI::designEtaColor(*etaIt));
        } else if (colorMode != 0) {
            const QUuid key = colorMode == 1 ? bar.sectionId : bar.materialId;
            const auto color = colorOf.constFind(key);
            if (color != colorOf.constEnd()) {
                appearance.hasColor = true;
                appearance.color = toArray(*color);
            }
        }
        if (appearance.state == ElementAppearance::State::Hidden) {
            m_effHiddenBars.insert(bar.id);
            m_hiddenBarExt.insert(bar.externalId);
        }
        bars.insert(bar.id, appearance);
    }
    // no cujas barras estao todas ocultas tambem fica oculto (nos soltos seguem o proprio estado)
    {
        QHash<QUuid, int> total, hiddenCount;
        for (const auto &bar : m_sceneController->bars()) {
            const bool hidden = bars.value(bar.id).state == ElementAppearance::State::Hidden;
            for (const QUuid &end : {bar.startNodeId, bar.endNodeId}) {
                total[end] += 1;
                hiddenCount[end] += hidden ? 1 : 0;
            }
        }
        for (const auto &node : nodeInfos) {
            if (total.value(node.id) > 0 && total.value(node.id) == hiddenCount.value(node.id)
                && nodes.value(node.id).state != ElementAppearance::State::Hidden) {
                ElementAppearance appearance = nodes.value(node.id);
                appearance.state = ElementAppearance::State::Hidden;
                nodes.insert(node.id, appearance);
                m_effHiddenNodes.insert(node.id);
                m_hiddenNodeExt.insert(node.externalId);
                m_offNodeKeys.insert(positionKey(node.x, node.y, node.z));
            }
        }
    }
    m_barAppearance = bars;

    m_sceneController->setElementAppearance(nodes, bars);
    syncLoadVisuals();
    syncSupportVisuals();
    syncSolidVisuals();
    refreshElementLabels();
    updateLegend(colorMode == 1 ? tr("Secoes") : (colorMode == 3 ? tr("Aproveitamento") : tr("Materiais")), legend);
    refreshGroupsPanel();
}

void MainWindow::updateLegend(const QString &title, const QVector<QPair<QString, QColor>> &entries)
{
    if (!m_vtkWidget) {
        return;
    }
    if (entries.isEmpty()) {
        if (m_legend) {
            m_legend->hide();
        }
        return;
    }
    if (!m_legend) {
        m_legend = new QFrame(m_vtkWidget);
        m_legend->setObjectName(QStringLiteral("colorLegend"));
        m_legend->setStyleSheet(QStringLiteral(
            "QFrame#colorLegend { background: rgba(255, 255, 255, 235); border: 1px solid #cbd4e2; border-radius: 6px; }"
            "QLabel { color: #1e232b; background: transparent; border: 0; font-size: 11px; }"));
        new QVBoxLayout(m_legend);
    }
    auto *layout = static_cast<QVBoxLayout *>(m_legend->layout());
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(3);
    while (QLayoutItem *item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    auto *heading = new QLabel(title, m_legend);
    heading->setStyleSheet(QStringLiteral("font-weight: 700; color: #0d7fb9;"));
    layout->addWidget(heading);
    for (const auto &entry : entries) {
        auto *row = new QWidget(m_legend);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);
        auto *swatch = new QFrame(row);
        swatch->setFixedSize(14, 14);
        swatch->setStyleSheet(QStringLiteral("background: %1; border: 1px solid #6e7781; border-radius: 3px;").arg(entry.second.name()));
        rowLayout->addWidget(swatch);
        rowLayout->addWidget(new QLabel(entry.first, row));
        layout->addWidget(row);
    }
    m_legend->adjustSize();
    m_legend->move(12, m_vtkWidget->height() - m_legend->height() - 12);
    m_legend->show();
    m_legend->raise();
}

void MainWindow::updatePlaneLabel()
{
    if (!m_vtkWidget || !m_sceneController || !m_sceneController->constructionScene()) {
        return;
    }
    auto *construction = m_sceneController->constructionScene();
    using Structura::Construction::PlaneMode;
    if (!m_planeLabel) {
        m_planeLabel = new QToolButton(m_vtkWidget);
        m_planeLabel->setObjectName(QStringLiteral("planeLabel"));
        m_planeLabel->setToolButtonStyle(Qt::ToolButtonTextOnly);
        m_planeLabel->setPopupMode(QToolButton::InstantPopup);
        m_planeLabel->setCursor(Qt::PointingHandCursor);
        m_planeLabel->setFocusPolicy(Qt::NoFocus);
        m_planeLabel->setToolTip(tr("Plano de construção: define o plano em que barras, nós e cascas são desenhados"));
        m_planeLabel->setStyleSheet(QStringLiteral(
            "QToolButton#planeLabel { background: rgba(255, 255, 255, 235); color: #0b6da1; border: 1px solid #cbd4e2;"
            "  border-radius: 6px; padding: 4px 24px 4px 12px; font-weight: 600; font-size: 12px; }"
            "QToolButton#planeLabel:hover { background: #e6f2fa; border-color: #9fc4de; }"
            "QToolButton#planeLabel::menu-indicator { subcontrol-origin: padding; subcontrol-position: center right; right: 8px; width: 9px; height: 9px; }"));
        auto *menu = new QMenu(m_planeLabel);
        const struct {
            PlaneMode mode;
            const char *text;
        } planes[] = {{PlaneMode::XY, "Plano X-Y"}, {PlaneMode::XZ, "Plano X-Z"}, {PlaneMode::YZ, "Plano Y-Z"}};
        for (const auto &plane : planes) {
            const PlaneMode mode = plane.mode;
            menu->addAction(tr(plane.text), this, [this, mode]() {
                if (m_sceneController && m_sceneController->constructionScene()) {
                    m_sceneController->constructionScene()->setPlaneMode(mode);
                }
            });
        }
        m_planeLabel->setMenu(menu);

        // botoes de vista flutuantes (ajustar a tela, visao inicial, vista em planta) sob o seletor de plano
        m_viewPanel = new QFrame(m_vtkWidget);
        m_viewPanel->setObjectName(QStringLiteral("viewPanel"));
        m_viewPanel->setStyleSheet(QStringLiteral(
            "QFrame#viewPanel { background: rgba(255, 255, 255, 235); border: 1px solid #cbd4e2; border-radius: 8px; }"
            "QFrame#viewPanel QToolButton { background: transparent; border: none; border-radius: 6px; }"
            "QFrame#viewPanel QToolButton:hover { background: #dfe8f3; }"
            "QFrame#viewPanel QToolButton:pressed { background: #cdd8e7; }"
            "QFrame#viewPanel QToolButton:checked { background: rgba(19, 147, 214, 0.20); }"));
        auto *panelLayout = new QVBoxLayout(m_viewPanel);
        panelLayout->setContentsMargins(4, 4, 4, 4);
        panelLayout->setSpacing(2);
        // propriedades, eixos locais, painel de grupos e eixos de referencia (funcoes da antiga barra lateral)
        auto prepare = [](QToolButton *button, int icon) {
            button->setIconSize(QSize(icon, icon));
            button->setFixedSize(36, 36);
            button->setCursor(Qt::PointingHandCursor);
            button->setFocusPolicy(Qt::NoFocus);
        };
        if (m_propertiesToolButton) {
            prepare(m_propertiesToolButton, 24);
            panelLayout->addWidget(m_propertiesToolButton);
        }
        if (m_showBarLCSToolButton) {
            prepare(m_showBarLCSToolButton, 24);
            m_showBarLCSToolButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
            panelLayout->addWidget(m_showBarLCSToolButton);
        }
        auto addShortcut = [this, panelLayout, &prepare](const QString &icon, const QString &tip, std::function<void()> handler) {
            auto *button = new QToolButton(m_viewPanel);
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
            button->setIcon(Structura::UI::svgIcon(icon, QColor(QStringLiteral("#1e232b")), 64));
            button->setToolTip(tip);
            prepare(button, 22);
            connect(button, &QToolButton::clicked, this, [handler]() { handler(); });
            panelLayout->addWidget(button);
        };
        addShortcut(QStringLiteral("groups.svg"), tr("Painel de grupos"), [this]() { onToggleGroupsPanel(); });
        addShortcut(QStringLiteral("axes-config.svg"), tr("Eixos de referência"), [this]() {
            if (m_axesAction) {
                m_axesAction->trigger();
            }
        });
        if (m_toolColumn) {
            m_toolColumn->deleteLater();
            m_toolColumn = nullptr;
        }
        m_viewPanel->adjustSize();
    }
    const QString name = Structura::Construction::planeName(construction->planeMode());
    m_planeLabel->setText(tr("Plano %1-%2").arg(name.at(0)).arg(name.at(1)));
    m_planeLabel->adjustSize();
    positionPlaneLabel();
    m_planeLabel->show();
    m_planeLabel->raise();
    if (m_viewPanel) {
        m_viewPanel->show();
        m_viewPanel->raise();
    }
}

void MainWindow::positionPlaneLabel()
{
    if (!m_vtkWidget) {
        return;
    }
    // Canto superior direito, da direita para a esquerda: barra de ferramentas (a direita do cubo), cubo de vistas e, acima dos dois, o seletor de plano.
    const int margin = 12;
    int right = m_vtkWidget->width() - margin;
    if (m_propertiesContainer) {
        // painel de propriedades flutuante: encosta no lado direito, em toda a altura, e os controles passam para a esquerda dele
        m_propertiesContainer->setGeometry(m_vtkWidget->width() - m_propertiesContainer->width(), 0, m_propertiesContainer->width(), m_vtkWidget->height());
        m_propertiesContainer->raise();
        if (m_propertiesContainer->isVisible()) {
            right -= m_propertiesContainer->width();
        }
    }
    const int rightEdge = right;
    int left = right;
    if (m_viewPanel) {
        m_viewPanel->move(right - m_viewPanel->width(), 44);
        right = m_viewPanel->x() - 6;
        left = m_viewPanel->x();
    }
    if (m_viewCube) {
        m_viewCube->move(right - m_viewCube->width(), 44);
        left = m_viewCube->x();
    }
    if (m_planeLabel) {
        m_planeLabel->move(rightEdge - m_planeLabel->width(), 10);
    }
    Q_UNUSED(left)
}
