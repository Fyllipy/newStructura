#include "MainWindow.h"
#include "DesignColors.h"

#include "SelectionModel.h"
#include "ViewOptionsBar.h"
#include "viz/SceneController.h"
#include "viz/construction/ConstructionScene.h"

#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>

#include <array>
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
    if (!m_planeLabel) {
        m_planeLabel = new QLabel(m_vtkWidget);
        m_planeLabel->setObjectName(QStringLiteral("planeLabel"));
        m_planeLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_planeLabel->setStyleSheet(QStringLiteral(
            "QLabel#planeLabel { background: rgba(255, 255, 255, 215); color: #0d7fb9; border: 1px solid #cbd4e2;"
            "  border-radius: 10px; padding: 3px 12px; font-weight: 700; font-size: 12px; }"));
    }
    const QString name = Structura::Construction::planeName(m_sceneController->constructionScene()->planeMode());
    m_planeLabel->setText(tr("Plano %1-%2").arg(name.at(0)).arg(name.at(1)));
    m_planeLabel->adjustSize();
    positionPlaneLabel();
    m_planeLabel->show();
    m_planeLabel->raise();
}

void MainWindow::positionPlaneLabel()
{
    if (m_planeLabel && m_vtkWidget) {
        // Fica a esquerda do triedro de eixos, no canto superior direito da viewport.
        m_planeLabel->move(m_vtkWidget->width() - m_planeLabel->width() - 120, 14);
    }
}
