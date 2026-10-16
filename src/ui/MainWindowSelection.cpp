#include "MainWindow.h"

#include "SelectionFilterDialog.h"
#include "SelectionModel.h"
#include "ViewOptionsBar.h"
#include "viz/SceneController.h"
#include "viz/construction/ConstructionScene.h"

#include <QAction>
#include <QHash>
#include <QMatrix4x4>
#include <QMenu>
#include <QRubberBand>
#include <QTimer>
#include <QStatusBar>
#include <QVTKOpenGLNativeWidget.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kLevelTolerance = 1e-6;

/// Algoritmo de Cohen-Sutherland: o segmento (em pixels) intersecta o retangulo?
bool segmentIntersectsRect(QPointF a, QPointF b, const QRectF &r)
{
    auto code = [&r](const QPointF &p) {
        int c = 0;
        if (p.x() < r.left()) c |= 1;
        if (p.x() > r.right()) c |= 2;
        if (p.y() < r.top()) c |= 4;
        if (p.y() > r.bottom()) c |= 8;
        return c;
    };
    int ca = code(a);
    int cb = code(b);
    for (int i = 0; i < 16; ++i) {
        if (!(ca | cb)) {
            return true;
        }
        if (ca & cb) {
            return false;
        }
        const int out = ca ? ca : cb;
        QPointF p;
        if (out & 8) {
            p = QPointF(a.x() + (b.x() - a.x()) * (r.bottom() - a.y()) / (b.y() - a.y()), r.bottom());
        } else if (out & 4) {
            p = QPointF(a.x() + (b.x() - a.x()) * (r.top() - a.y()) / (b.y() - a.y()), r.top());
        } else if (out & 2) {
            p = QPointF(r.right(), a.y() + (b.y() - a.y()) * (r.right() - a.x()) / (b.x() - a.x()));
        } else {
            p = QPointF(r.left(), a.y() + (b.y() - a.y()) * (r.left() - a.x()) / (b.x() - a.x()));
        }
        if (out == ca) {
            a = p;
            ca = code(a);
        } else {
            b = p;
            cb = code(b);
        }
    }
    return false;
}

} // namespace

bool MainWindow::nodePassesFilter(const Structura::App::SceneNodeInfo &node) const
{
    if (m_selectionFilter.barsOnly()) {
        return false;
    }
    if (m_selectionFilter.levelEnabled && std::abs(node.z - m_selectionFilter.level) > kLevelTolerance) {
        return false;
    }
    return !isNodeHidden(node.id);
}

bool MainWindow::barPassesFilter(const Structura::App::SceneBarInfo &bar,
                                 const QHash<QUuid, Structura::App::SceneNodeInfo> &nodes) const
{
    if (!m_selectionFilter.section.isNull() && bar.sectionId != m_selectionFilter.section) {
        return false;
    }
    if (!m_selectionFilter.material.isNull() && bar.materialId != m_selectionFilter.material) {
        return false;
    }
    if (m_selectionFilter.levelEnabled) {
        const auto a = nodes.constFind(bar.startNodeId);
        const auto b = nodes.constFind(bar.endNodeId);
        if (a == nodes.constEnd() || b == nodes.constEnd()
            || std::abs(a->z - m_selectionFilter.level) > kLevelTolerance
            || std::abs(b->z - m_selectionFilter.level) > kLevelTolerance) {
            return false;
        }
    }
    return !isBarHidden(bar.id);
}

bool MainWindow::handleDefaultViewportMousePress(Qt::MouseButton button, Qt::KeyboardModifiers modifiers, const QPoint &widgetPos)
{
    if (button == Qt::LeftButton) {
        m_press.left = true;
        m_press.dragging = false;
        m_press.pos = widgetPos;
        m_press.mods = modifiers;
    } else if (button == Qt::RightButton) {
        m_press.right = true;
        m_press.rightMoved = false;
        m_press.rightPos = widgetPos;
    }
    return false;
}

bool MainWindow::handleDefaultViewportMouseMove(const QPoint &widgetPos)
{
    updateHighlightedNodeAtWidgetPos(widgetPos);

    if (m_press.right && (widgetPos - m_press.rightPos).manhattanLength() > 4) {
        m_press.rightMoved = true;
    }
    if (m_press.left) {
        if (!m_press.dragging && (widgetPos - m_press.pos).manhattanLength() > 4) {
            m_press.dragging = true;
            if (!m_rubberBand) {
                m_rubberBand = new QRubberBand(QRubberBand::Rectangle, m_vtkWidget);
                m_rubberBand->setAttribute(Qt::WA_TransparentForMouseEvents);
            }
            m_rubberBand->show();
        }
        if (m_press.dragging && m_rubberBand) {
            m_rubberBand->setGeometry(QRect(m_press.pos, widgetPos).normalized());
        }
    }
    return false;
}

bool MainWindow::handleDefaultViewportMouseRelease(Qt::MouseButton button, const QPoint &widgetPos, const QPoint &globalPos)
{
    if (button == Qt::LeftButton && m_press.left) {
        const bool wasDragging = m_press.dragging;
        const QPoint start = m_press.pos;
        const Qt::KeyboardModifiers mods = m_press.mods;
        m_press.left = false;
        m_press.dragging = false;
        if (m_rubberBand) {
            m_rubberBand->hide();
        }
        if (wasDragging) {
            const QRect rect = QRect(start, widgetPos).normalized();
            // Da esquerda para a direita: janela (tudo dentro); da direita para a esquerda: cruzamento.
            selectInRect(rect, widgetPos.x() < start.x(), mods);
            return true;
        }
        clickSelect(widgetPos, mods);
        return true;
    }
    if (button == Qt::RightButton && m_press.right) {
        const bool moved = m_press.rightMoved;
        m_press.right = false;
        if (!moved) {
            // Nao consome a liberacao: o interactor da camera precisa dela para encerrar a rotacao
            // iniciada no clique. O menu abre depois que o evento terminar de ser entregue.
            QTimer::singleShot(0, this, [this, widgetPos, globalPos]() { showViewportContextMenu(widgetPos, globalPos); });
        }
    }
    return false;
}

void MainWindow::clickSelect(const QPoint &widgetPos, Qt::KeyboardModifiers modifiers)
{
    QUuid nodeId = pickNodeAtWidgetPos(widgetPos);
    QUuid barId = nodeId.isNull() ? pickBarAtWidgetPos(widgetPos) : QUuid();
    if (!nodeId.isNull() && isNodeHidden(nodeId)) {
        nodeId = QUuid();
    }
    if (!barId.isNull() && isBarHidden(barId)) {
        barId = QUuid();
    }

    Structura::SelectionModel::Mode mode = Structura::SelectionModel::Mode::Replace;
    if (modifiers.testFlag(Qt::ControlModifier)) {
        mode = Structura::SelectionModel::Mode::Toggle;
    } else if (modifiers.testFlag(Qt::ShiftModifier)) {
        mode = Structura::SelectionModel::Mode::Add;
    }

    if (!nodeId.isNull()) {
        m_selectionModel->selectNode(nodeId, mode);
    } else if (!barId.isNull()) {
        m_selectionModel->selectBar(barId, mode);
    } else if (mode == Structura::SelectionModel::Mode::Replace) {
        m_selectionModel->clear();
    }
}

void MainWindow::selectInRect(const QRect &rect, bool crossing, Qt::KeyboardModifiers modifiers)
{
    if (!m_sceneController || !m_vtkWidget) {
        return;
    }
    const QMatrix4x4 matrix = m_sceneController->compositeMatrix();
    const double width = m_vtkWidget->width();
    const double height = m_vtkWidget->height();
    auto project = [&](double x, double y, double z, QPointF &out) {
        const QVector4D clip = matrix * QVector4D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 1.0f);
        if (clip.w() <= 1e-9f) {
            return false;
        }
        const double nx = clip.x() / clip.w();
        const double ny = clip.y() / clip.w();
        const double nz = clip.z() / clip.w();
        if (std::abs(nz) > 1.0) {
            return false;
        }
        out = QPointF((nx + 1.0) * width / 2.0, (1.0 - ny) * height / 2.0);
        return true;
    };

    QHash<QUuid, Structura::App::SceneNodeInfo> nodes;
    QHash<QUuid, QPointF> screen;
    QHash<QUuid, bool> visible;
    QVector<QUuid> pickedNodes;
    const QRectF area(rect);
    for (const auto &node : m_sceneController->nodeInfos()) {
        nodes.insert(node.id, node);
        QPointF p;
        const bool ok = project(node.x, node.y, node.z, p);
        visible.insert(node.id, ok);
        screen.insert(node.id, p);
        if (ok && area.contains(p) && nodePassesFilter(node)) {
            pickedNodes.append(node.id);
        }
    }

    QVector<QUuid> pickedBars;
    for (const auto &bar : m_sceneController->bars()) {
        if (!barPassesFilter(bar, nodes)) {
            continue;
        }
        if (!visible.value(bar.startNodeId) || !visible.value(bar.endNodeId)) {
            continue;
        }
        const QPointF a = screen.value(bar.startNodeId);
        const QPointF b = screen.value(bar.endNodeId);
        const bool inside = area.contains(a) && area.contains(b);
        if (inside || (crossing && segmentIntersectsRect(a, b, area))) {
            pickedBars.append(bar.id);
        }
    }

    Structura::SelectionModel::Mode mode = Structura::SelectionModel::Mode::Replace;
    if (modifiers.testFlag(Qt::ControlModifier)) {
        mode = Structura::SelectionModel::Mode::Toggle;
    } else if (modifiers.testFlag(Qt::ShiftModifier)) {
        mode = Structura::SelectionModel::Mode::Add;
    }
    if (mode == Structura::SelectionModel::Mode::Replace) {
        m_selectionModel->clear();
    }
    m_selectionModel->selectNodes(pickedNodes, mode == Structura::SelectionModel::Mode::Replace
                                                  ? Structura::SelectionModel::Mode::Add : mode);
    m_selectionModel->selectBars(pickedBars, mode == Structura::SelectionModel::Mode::Replace
                                                 ? Structura::SelectionModel::Mode::Add : mode);
    showStatusMessage(tr("%1 no(s) e %2 barra(s) selecionados (%3).")
                          .arg(pickedNodes.size())
                          .arg(pickedBars.size())
                          .arg(crossing ? tr("cruzamento") : tr("janela")),
                      4000);
}

void MainWindow::onSelectionFilter()
{
    using Structura::UI::SelectionFilterDialog;
    QVector<SelectionFilterDialog::Option> levels;
    QVector<double> seen;
    auto addLevel = [&](double elevation, const QString &name) {
        for (const double value : seen) {
            if (std::abs(value - elevation) < kLevelTolerance) {
                return;
            }
        }
        seen.append(elevation);
        SelectionFilterDialog::Option option;
        option.value = elevation;
        option.text = name.isEmpty() ? tr("Z = %1 m").arg(elevation, 0, 'f', 3)
                                     : tr("%1  (%2 m)").arg(name).arg(elevation, 0, 'f', 3);
        levels.append(option);
    };
    if (auto *construction = m_sceneController->constructionScene()) {
        for (const auto &level : construction->axes().levels) {
            addLevel(level.elevation, level.name);
        }
    }
    for (const auto &node : m_sceneController->nodeInfos()) {
        addLevel(std::round(node.z * 1000.0) / 1000.0, QString());
    }
    std::sort(levels.begin(), levels.end(), [](const auto &a, const auto &b) { return a.value < b.value; });

    QVector<SelectionFilterDialog::Option> sections;
    for (const auto &section : m_sections) {
        sections.append({section.name, section.uuid, 0.0});
    }
    QVector<SelectionFilterDialog::Option> materials;
    for (const auto &material : m_materials) {
        materials.append({material.name, material.uuid, 0.0});
    }

    SelectionFilterDialog dialog(m_selectionFilter, levels, sections, materials, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_selectionFilter = dialog.filter();
    if (m_viewOptionsBar) {
        m_viewOptionsBar->setSelectionFilterActive(m_selectionFilter.active());
    }
    showStatusMessage(m_selectionFilter.active() ? tr("Filtro de selecao ativo.") : tr("Filtro de selecao removido."), 3000);
}

void MainWindow::onSelectMatching()
{
    QHash<QUuid, Structura::App::SceneNodeInfo> nodes;
    QVector<QUuid> pickedNodes;
    for (const auto &node : m_sceneController->nodeInfos()) {
        nodes.insert(node.id, node);
        if (nodePassesFilter(node)) {
            pickedNodes.append(node.id);
        }
    }
    QVector<QUuid> pickedBars;
    for (const auto &bar : m_sceneController->bars()) {
        if (barPassesFilter(bar, nodes)) {
            pickedBars.append(bar.id);
        }
    }
    m_selectionModel->clear();
    m_selectionModel->selectNodes(pickedNodes, Structura::SelectionModel::Mode::Add);
    m_selectionModel->selectBars(pickedBars, Structura::SelectionModel::Mode::Add);
    showStatusMessage(tr("%1 no(s) e %2 barra(s) atendem ao filtro.").arg(pickedNodes.size()).arg(pickedBars.size()), 4000);
}

void MainWindow::onInvertSelection()
{
    QHash<QUuid, Structura::App::SceneNodeInfo> nodes;
    QVector<QUuid> flipNodes;
    for (const auto &node : m_sceneController->nodeInfos()) {
        nodes.insert(node.id, node);
        if (nodePassesFilter(node)) {
            flipNodes.append(node.id);
        }
    }
    QVector<QUuid> flipBars;
    for (const auto &bar : m_sceneController->bars()) {
        if (barPassesFilter(bar, nodes)) {
            flipBars.append(bar.id);
        }
    }
    m_selectionModel->selectNodes(flipNodes, Structura::SelectionModel::Mode::Toggle);
    m_selectionModel->selectBars(flipBars, Structura::SelectionModel::Mode::Toggle);
}

void MainWindow::showViewportContextMenu(const QPoint &widgetPos, const QPoint &globalPos)
{
    // O elemento sob o cursor entra na selecao se ainda nao estava.
    const QUuid nodeId = pickNodeAtWidgetPos(widgetPos);
    const QUuid barId = nodeId.isNull() ? pickBarAtWidgetPos(widgetPos) : QUuid();
    if (!nodeId.isNull() && !isNodeHidden(nodeId) && !m_selectionModel->isNodeSelected(nodeId)) {
        m_selectionModel->selectNode(nodeId, Structura::SelectionModel::Mode::Replace);
    } else if (!barId.isNull() && !isBarHidden(barId) && !m_selectionModel->isBarSelected(barId)) {
        m_selectionModel->selectBar(barId, Structura::SelectionModel::Mode::Replace);
    }

    const bool hasSelection = !m_selectionModel->selectedNodes().isEmpty() || !m_selectionModel->selectedBars().isEmpty();
    const bool hasBars = !m_selectionModel->selectedBars().isEmpty();
    const bool hasNodes = !m_selectionModel->selectedNodes().isEmpty();

    QMenu menu(this);
    auto add = [&](const QString &text, bool enabled, auto slot) {
        QAction *action = menu.addAction(text);
        action->setEnabled(enabled);
        connect(action, &QAction::triggered, this, slot);
        return action;
    };
    add(tr("Atribuir material/secao..."), hasBars, &MainWindow::onAssignProperties);
    add(tr("Forca nodal..."), hasNodes, &MainWindow::onApplyNodalLoad);
    add(tr("Carga distribuida..."), hasBars, &MainWindow::onApplyDistributedLoad);
    add(tr("Momento distribuido..."), hasBars, &MainWindow::onApplyMemberMoment);
    add(tr("Restricoes nodais..."), hasNodes, &MainWindow::onApplyRestraints);
    add(tr("Cargas da selecao..."), hasSelection, &MainWindow::onShowLoadsPanel);
    add(tr("Gerenciador de perfis..."), true, &MainWindow::onOpenProfileManager);
    menu.addSeparator();
    add(tr("Copiar"), hasSelection, &MainWindow::onCopyElements);
    add(tr("Mover"), hasSelection, &MainWindow::onMoveElements);
    add(tr("Copia em serie..."), hasSelection, &MainWindow::onSerialCopy);
    add(tr("Rotacionar"), hasSelection, &MainWindow::onRotateElements);
    add(tr("Espelhar"), hasSelection, &MainWindow::onMirrorElements);
    menu.addSeparator();
    add(tr("Ocultar selecionados"), hasSelection, &MainWindow::onHideSelected);
    add(tr("Isolar selecionados"), hasSelection, &MainWindow::onIsolateSelected);
    add(tr("Mostrar todos"), !m_hiddenNodes.isEmpty() || !m_hiddenBars.isEmpty(), &MainWindow::onShowAllElements);
    menu.addSeparator();
    add(tr("Inverter selecao"), true, &MainWindow::onInvertSelection);
    add(tr("Limpar selecao"), hasSelection, [this]() { m_selectionModel->clear(); });
    menu.exec(globalPos);
}
