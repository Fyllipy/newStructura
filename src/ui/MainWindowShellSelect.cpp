// Selecao de nos de cascas na vista principal (Fase 2 do plano de integracao das cascas): infraestrutura reutilizada pelas fases seguintes
// (apoios por clique, cargas). A selecao e TRANSITORIA (nao e gravada) e guardada por COORDENADAS: quando a malha e regenerada, os nos
// sao procurados de novo pela posicao, nunca pelo indice.
#include "MainWindow.h"

#include "shell/ShellSelection.h"
#include "viz/SceneController.h"

#include <QAction>
#include <QMatrix4x4>
#include <QRect>
#include <QVTKOpenGLNativeWidget.h>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kPickTolerancePx = 12.0;
}

void MainWindow::ensureShellNodeTable()
{
    if (!m_shellRender) {
        m_shellNodeTable = Structura::Shell::NodeTable {};
        m_shellNodeTableRevision = 0;
        return;
    }
    // a tabela so muda com a geometria (mesma `geometryRevision` = mesmos nos, nas mesmas posicoes)
    if (m_shellNodeTableRevision != m_shellRender->geometryRevision || m_shellNodeTable.size() == 0) {
        m_shellNodeTable = Structura::Shell::buildNodeTable(*m_shellRender);
        m_shellNodeTableRevision = m_shellRender->geometryRevision;
    }
}

Structura::Shell::ScreenProjector MainWindow::shellProjector() const
{
    Structura::Shell::ScreenProjector projector;
    const QMatrix4x4 composite = m_sceneController->compositeMatrix();
    for (int i = 0; i < 16; ++i) {
        projector.matrix[static_cast<std::size_t>(i)] = static_cast<double>(composite.constData()[i]);
    }
    projector.width = m_vtkWidget->width();
    projector.height = m_vtkWidget->height();
    return projector;
}

void MainWindow::setShellNodeSelectActive(bool active)
{
    if (active && !(m_shellRender && m_shellMeshVisible && !m_shellRender->points.empty())) {
        active = false;
        showStatusMessage(tr("Não há malha de cascas visível: gere a malha pelo Editor de cascas ou desenhe uma casca."), 4000);
    }
    if (active && m_shellProbeActive) {
        setShellProbeActive(false);   // as duas ferramentas disputam o clique
    }
    m_shellNodeSelectActive = active;
    if (m_shellNodeSelectAction && m_shellNodeSelectAction->isChecked() != active) {
        const QSignalBlocker blocker(m_shellNodeSelectAction);
        m_shellNodeSelectAction->setChecked(active);
    }
    if (m_vtkWidget) {
        m_vtkWidget->setCursor(active ? Qt::CrossCursor : Qt::ArrowCursor);
    }
    if (active) {
        ensureShellNodeTable();
        showStatusMessage(tr("Seleção de nós de cascas: clique em um nó (Ctrl alterna, Shift acrescenta) ou arraste uma caixa; Esc limpa."), 6000);
    } else {
        clearShellNodeSelection();
    }
}

void MainWindow::clearShellNodeSelection()
{
    m_shellNodeSel.clear();
    m_shellSelRefs.clear();
    updateShellSelectionVisual();
}

void MainWindow::updateShellSelectionVisual()
{
    if (!m_sceneController) {
        return;
    }
    std::vector<std::array<float, 3>> points;
    if (!m_shellNodeSel.empty()) {
        for (const int id : m_shellNodeSel.ids()) {
            const int i = m_shellNodeTable.indexOf(id);
            if (i >= 0) {
                const auto &p = m_shellNodeTable.viewPos[static_cast<std::size_t>(i)];
                points.push_back({static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2])});
            }
        }
    }
    m_sceneController->setShellSelectedNodes(points);
}

void MainWindow::commitShellNodeSelection(const std::vector<int> &ids, Structura::Shell::NodeSelection::Mode mode)
{
    m_shellNodeSel.apply(ids, mode);
    // referencias por coordenadas: o que sobrevive a uma nova malha
    m_shellSelRefs = Structura::Shell::makeNodeRefs(m_shellNodeTable, m_shellNodeSel.ids());
    updateShellSelectionVisual();
    const std::size_t n = m_shellNodeSel.size();
    if (n == 0) {
        showStatusMessage(tr("Nenhum nó de casca selecionado."), 3000);
    } else if (n == 1 && !m_shellSelRefs.empty()) {
        const auto &p = m_shellSelRefs.front().position;
        showStatusMessage(tr("1 nó de casca selecionado: x = %1 m, y = %2 m, z = %3 m").arg(p[0], 0, 'f', 3).arg(p[1], 0, 'f', 3).arg(p[2], 0, 'f', 3), 5000);
    } else {
        showStatusMessage(tr("%1 nós de cascas selecionados.").arg(n), 4000);
    }
}

bool MainWindow::shellNodeClick(const QPoint &widgetPos, Qt::KeyboardModifiers modifiers)
{
    if (!m_shellNodeSelectActive || !m_sceneController || !m_vtkWidget || !m_shellRender) {
        return false;
    }
    ensureShellNodeTable();
    using Mode = Structura::Shell::NodeSelection::Mode;
    const Mode mode = modifiers.testFlag(Qt::ControlModifier) ? Mode::Toggle : (modifiers.testFlag(Qt::ShiftModifier) ? Mode::Add : Mode::Replace);
    const auto projector = shellProjector();
    const auto occlusion = Structura::Shell::buildDepthMap(*m_shellRender, projector);   // nos escondidos atras das cascas nao contam
    const int index = Structura::Shell::pickNearestNode(m_shellNodeTable, projector, widgetPos.x(), widgetPos.y(), kPickTolerancePx, nullptr, &occlusion);
    std::vector<int> ids;
    if (index >= 0) {
        ids.push_back(m_shellNodeTable.ids[static_cast<std::size_t>(index)]);
    } else if (mode != Mode::Replace) {
        return true;   // clique no vazio com Ctrl/Shift: nada muda
    }
    commitShellNodeSelection(ids, mode);   // clique no vazio sem modificador limpa a selecao
    return true;
}

bool MainWindow::shellNodeRect(const QRect &rect, Qt::KeyboardModifiers modifiers)
{
    if (!m_shellNodeSelectActive || !m_sceneController || !m_vtkWidget || !m_shellRender) {
        return false;
    }
    ensureShellNodeTable();
    using Mode = Structura::Shell::NodeSelection::Mode;
    const Mode mode = modifiers.testFlag(Qt::ControlModifier) ? Mode::Toggle : (modifiers.testFlag(Qt::ShiftModifier) ? Mode::Add : Mode::Replace);
    const auto projector = shellProjector();
    const auto occlusion = Structura::Shell::buildDepthMap(*m_shellRender, projector);
    const auto inside = Structura::Shell::nodesInRect(m_shellNodeTable, projector, rect.left(), rect.top(), rect.right(), rect.bottom(), &occlusion);
    std::vector<int> ids;
    ids.reserve(inside.size());
    for (const int i : inside) {
        ids.push_back(m_shellNodeTable.ids[static_cast<std::size_t>(i)]);
    }
    commitShellNodeSelection(ids, mode);
    return true;
}

/// Depois de uma nova malha (remalhada, deformada, editada): reencontra os nos selecionados pelas coordenadas.
void MainWindow::reresolveShellNodeSelection()
{
    ensureShellNodeTable();
    if (m_shellSelRefs.empty()) {
        m_shellNodeSel.clear();
        updateShellSelectionVisual();
        return;
    }
    // os vertices do RenderMesh sao float: a tolerancia acompanha a extensao do modelo (~1e-7 relativo), com piso de 1e-5 m
    double extent = 0.0;
    for (const auto &p : m_shellNodeTable.restPos) {
        extent = std::max({extent, std::abs(p[0]), std::abs(p[1]), std::abs(p[2])});
    }
    const auto resolved = Structura::Shell::resolveNodeRefs(m_shellNodeTable, m_shellSelRefs, std::max(1e-5, 4e-7 * extent));
    m_shellNodeSel.set(resolved.nodeIds);
    if (resolved.unresolved > 0) {
        showStatusMessage(tr("%1 nó(s) selecionado(s) não existem mais na malha atual e saíram da seleção.").arg(resolved.unresolved), 6000);
        m_shellSelRefs = Structura::Shell::makeNodeRefs(m_shellNodeTable, m_shellNodeSel.ids());
    }
    updateShellSelectionVisual();
}
