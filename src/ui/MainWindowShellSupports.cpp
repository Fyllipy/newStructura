// Apoios de cascas na vista principal (Fase 3): aplicar apoio aos nos selecionados, remover, e desenhar os apoios (glifos + legenda por tipo).
// Os apoios por clique usam o seletor `Support::Nodes` (coordenadas), entao sobrevivem a remalhar e ficam gravados no projeto.
#include "MainWindow.h"

#include "ShellDialog.h"
#include "ShellSupportDialog.h"
#include "SupportLegendOverlay.h"
#include "shell/ShellSupports.h"
#include "viz/SceneController.h"

#include <QAction>
#include <QHash>
#include <QSet>
#include <QUuid>
#include <QVTKOpenGLNativeWidget.h>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kSupportPointTol = 1.0e-5;   // m

bool samePoint(const std::array<double, 3> &a, const std::array<double, 3> &b, double tol)
{
    return std::abs(a[0] - b[0]) <= tol && std::abs(a[1] - b[1]) <= tol && std::abs(a[2] - b[2]) <= tol;
}
} // namespace

void MainWindow::refreshShellSupports()
{
    if (!m_sceneController) {
        return;
    }
    using namespace Structura::Shell;
    std::vector<Structura::Visualization::ShellRenderer::SupportGlyph> glyphs;
    std::vector<Structura::UI::SupportLegendOverlay::Entry> entries;
    float size = 0.f;
    QString warning;
    if (!m_shellMesh.quads.empty() && !m_extras.shell.supports.empty()) {
        const SupportResolution resolution = resolveSupports(m_extras.shell, m_shellMesh);
        if (!resolution.warnings.empty()) {
            warning = QString::fromStdString(resolution.warnings.front());
        }
        const double upm = shellUnitsPerMeter();
        std::array<double, 3> lo {{1e300, 1e300, 1e300}}, hi {{-1e300, -1e300, -1e300}};
        for (const auto &p : m_shellMesh.nodes) {
            for (std::size_t k = 0; k < 3; ++k) {
                lo[k] = std::min(lo[k], p[k]);
                hi[k] = std::max(hi[k], p[k]);
            }
        }
        const double diag = std::sqrt((hi[0] - lo[0]) * (hi[0] - lo[0]) + (hi[1] - lo[1]) * (hi[1] - lo[1]) + (hi[2] - lo[2]) * (hi[2] - lo[2]));
        size = static_cast<float>(std::max(0.02 * diag, 1e-3) * upm);
        int counts[6] = {0, 0, 0, 0, 0, 0};
        for (const SupportMark &mark : supportMarks(m_extras.shell, m_shellMesh, resolution)) {
            Structura::Visualization::ShellRenderer::SupportGlyph g;
            g.position = {static_cast<float>(mark.position[0] * upm), static_cast<float>(mark.position[1] * upm), static_cast<float>(mark.position[2] * upm)};
            g.kind = static_cast<int>(mark.kind);
            g.normal = {static_cast<float>(mark.normal[0]), static_cast<float>(mark.normal[1]), static_cast<float>(mark.normal[2])};
            glyphs.push_back(g);
            ++counts[std::clamp(g.kind, 0, 5)];
        }
        for (int k = 0; k < 6; ++k) {
            if (counts[k] > 0) {
                entries.push_back({k, counts[k]});
            }
        }
    }
    m_sceneController->setShellSupports(glyphs, size);
    m_sceneController->setShellSupportsVisible(m_shellSupportsShown);
    if (!m_supportLegend && m_vtkWidget && !entries.empty()) {
        m_supportLegend = new Structura::UI::SupportLegendOverlay(m_vtkWidget);
    }
    if (m_supportLegend) {
        if (m_shellSupportsShown && m_shellMeshVisible) {
            m_supportLegend->setEntries(entries);
        } else {
            m_supportLegend->clearEntries();
        }
    }
    if (!warning.isEmpty()) {
        showStatusMessage(warning, 6000);
    }
}

void MainWindow::setShellSupportsShown(bool shown)
{
    m_shellSupportsShown = shown;
    if (m_shellShowSupportsAction && m_shellShowSupportsAction->isChecked() != shown) {
        const QSignalBlocker blocker(m_shellShowSupportsAction);
        m_shellShowSupportsAction->setChecked(shown);
    }
    refreshShellSupports();
}

void MainWindow::positionSupportLegend()
{
    if (m_supportLegend && !m_supportLegend->isHidden()) {
        m_supportLegend->placeInParent();
    }
}

namespace {

/// Coordenadas exatas (double) dos nos selecionados: da malha, se o no existe e coincide com a referencia (a referencia vem de vertices float).
std::vector<std::array<double, 3>> exactPoints(const std::vector<Structura::Shell::NodeRef> &refs, const Structura::Shell::NodeTable &table, const Structura::Shell::Mesh &mesh,
                                               const std::vector<int> &nodeIds)
{
    std::vector<std::array<double, 3>> points;
    for (const int id : nodeIds) {
        const int ti = table.indexOf(id);
        if (ti < 0) {
            continue;
        }
        std::array<double, 3> p = table.restPos[static_cast<std::size_t>(ti)];
        if (id >= 0 && static_cast<std::size_t>(id) < mesh.nodes.size()) {
            const auto &m = mesh.nodes[static_cast<std::size_t>(id)];
            if (samePoint(m, p, 1e-3)) {
                p = m;
            }
        }
        points.push_back(p);
    }
    (void)refs;
    return points;
}

} // namespace

void MainWindow::applyShellSupportToSelection()
{
    // alvos: nos de casca selecionados + nos da malha que ficam sobre as barras selecionadas
    auto points = exactPoints(m_shellSelRefs, m_shellNodeTable, m_shellMesh, m_shellNodeSel.ids());
    int onBars = 0;
    if (m_selectionModel && !m_selectionModel->selectedBars().isEmpty() && m_sceneController && !m_shellMesh.nodes.empty()) {
        const double upm = std::max(shellUnitsPerMeter(), 1e-12);
        QHash<QUuid, std::array<double, 3>> position;
        for (const auto &node : m_sceneController->nodeInfos()) {
            position.insert(node.id, {node.x / upm, node.y / upm, node.z / upm});
        }
        const QSet<QUuid> selected = m_selectionModel->selectedBars();
        constexpr double kOnBarTol = 1.0e-3;   // m
        for (const auto &bar : m_sceneController->bars()) {
            if (!selected.contains(bar.id)) {
                continue;
            }
            const auto a = position.constFind(bar.startNodeId), b = position.constFind(bar.endNodeId);
            if (a == position.constEnd() || b == position.constEnd()) {
                continue;
            }
            const std::array<double, 3> d {{(*b)[0] - (*a)[0], (*b)[1] - (*a)[1], (*b)[2] - (*a)[2]}};
            const double len2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
            if (len2 < 1e-18) {
                continue;
            }
            for (const auto &p : m_shellMesh.nodes) {
                double t = ((p[0] - (*a)[0]) * d[0] + (p[1] - (*a)[1]) * d[1] + (p[2] - (*a)[2]) * d[2]) / len2;
                t = std::clamp(t, 0.0, 1.0);
                const double dx = p[0] - ((*a)[0] + t * d[0]), dy = p[1] - ((*a)[1] + t * d[1]), dz = p[2] - ((*a)[2] + t * d[2]);
                if (std::sqrt(dx * dx + dy * dy + dz * dz) > kOnBarTol) {
                    continue;
                }
                const std::array<double, 3> q {{p[0], p[1], p[2]}};
                if (std::none_of(points.begin(), points.end(), [&](const std::array<double, 3> &e) { return samePoint(e, q, kSupportPointTol); })) {
                    points.push_back(q);
                    ++onBars;
                }
            }
        }
    }
    if (points.empty()) {
        showStatusMessage(m_shellNodeSel.empty() ? tr("Selecione nós de cascas (botão \"Selecionar nós\") ou barras que tenham casca sobre elas.")
                                                 : tr("Os nós selecionados não estão na malha atual."),
                          6000);
        return;
    }
    Structura::UI::ShellSupportDialog dialog(static_cast<int>(points.size()), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    Structura::Shell::Support support = dialog.support();
    bool restrains = support.springPerArea > 0.0;
    for (const int f : support.fixed) {
        restrains = restrains || f != 0;
    }
    if (!restrains) {
        showStatusMessage(tr("Nenhum grau de liberdade restringido: nada a aplicar."), 5000);
        return;
    }
    {
        StateGuard guard(this, tr("Apoios de casca"));
        auto &supports = m_extras.shell.supports;
        // o apoio novo SUBSTITUI o que esses nos tinham por clique: tira os pontos dos outros apoios por nos
        for (auto &s : supports) {
            if (s.selector != Structura::Shell::Support::Nodes) {
                continue;
            }
            s.points.erase(std::remove_if(s.points.begin(), s.points.end(),
                                          [&](const std::array<double, 3> &q) {
                                              return std::any_of(points.begin(), points.end(), [&](const std::array<double, 3> &p) { return samePoint(p, q, kSupportPointTol); });
                                          }),
                           s.points.end());
        }
        // junta ao apoio por nos de mesma definicao, se existir (mantem a lista curta)
        Structura::Shell::Support *target = nullptr;
        for (auto &s : supports) {
            if (s.selector == Structura::Shell::Support::Nodes && s.object == 0 && s.fixed == support.fixed && s.springPerArea == support.springPerArea
                && s.compressionOnly == support.compressionOnly && s.springNormal == support.springNormal) {
                target = &s;
                break;
            }
        }
        if (!target) {
            support.selector = Structura::Shell::Support::Nodes;
            support.object = 0;
            support.tol = kSupportPointTol;
            supports.push_back(support);
            target = &supports.back();
        }
        target->points.insert(target->points.end(), points.begin(), points.end());
        supports.erase(std::remove_if(supports.begin(), supports.end(), [](const Structura::Shell::Support &s) { return s.selector == Structura::Shell::Support::Nodes && s.points.empty(); }),
                       supports.end());
        m_shellResultsShown = false;
        clearShellNodeSelection();
        refreshShellVisuals(true);
        if (m_shellDialog) {
            m_shellDialog->setModel(m_extras.shell);
        }
        markModified();
    }
    showStatusMessage(onBars > 0 ? tr("Apoio aplicado a %1 nó(s) (%2 sobre as barras selecionadas). Reanalise para atualizar os resultados.").arg(points.size()).arg(onBars)
                                 : tr("Apoio aplicado a %1 nó(s). Reanalise para atualizar os resultados.").arg(points.size()),
                      6000);
}

void MainWindow::removeShellSupportFromSelection()
{
    if (m_shellNodeSel.empty()) {
        showStatusMessage(tr("Selecione nós de cascas primeiro (botão \"Selecionar nós\")."), 5000);
        return;
    }
    const auto points = exactPoints(m_shellSelRefs, m_shellNodeTable, m_shellMesh, m_shellNodeSel.ids());
    int removed = 0;
    {
        StateGuard guard(this, tr("Remover apoios de casca"));
        auto &supports = m_extras.shell.supports;
        for (auto &s : supports) {
            if (s.selector != Structura::Shell::Support::Nodes) {
                continue;
            }
            const std::size_t before = s.points.size();
            s.points.erase(std::remove_if(s.points.begin(), s.points.end(),
                                          [&](const std::array<double, 3> &q) {
                                              return std::any_of(points.begin(), points.end(), [&](const std::array<double, 3> &p) { return samePoint(p, q, kSupportPointTol); });
                                          }),
                           s.points.end());
            removed += static_cast<int>(before - s.points.size());
        }
        supports.erase(std::remove_if(supports.begin(), supports.end(), [](const Structura::Shell::Support &s) { return s.selector == Structura::Shell::Support::Nodes && s.points.empty(); }),
                       supports.end());
        if (removed > 0) {
            m_shellResultsShown = false;
            clearShellNodeSelection();
            refreshShellVisuals(true);
            if (m_shellDialog) {
                m_shellDialog->setModel(m_extras.shell);
            }
            markModified();
        }
    }
    if (removed > 0) {
        showStatusMessage(tr("%1 apoio(s) por clique removido(s). Apoios definidos por plano, borda ou ponto continuam no Editor de cascas.").arg(removed), 7000);
    } else {
        showStatusMessage(tr("Os nós selecionados não têm apoio por clique. Apoios definidos por plano, borda ou ponto são editados no Editor de cascas."), 7000);
    }
}
