// Cargas de cascas dadas por clique na vista principal (Fase 4): aplicar aos nos selecionados (forca nodal, carga linear, carga de superficie),
// remover e desenhar como setas. As cargas ficam gravadas por coordenadas (Load::points) e entram no calculo pelo buildJob.
#include "MainWindow.h"

#include "ShellDialog.h"
#include "ShellLoadDialog.h"
#include "shell/ShellLoads.h"
#include "viz/SceneController.h"

#include <QAction>

#include <algorithm>
#include <cmath>
#include <map>

namespace {
constexpr double kLoadPointTol = Structura::Shell::kPointLoadTolerance;

bool samePoint(const std::array<double, 3> &a, const std::array<double, 3> &b, double tol)
{
    return std::abs(a[0] - b[0]) <= tol && std::abs(a[1] - b[1]) <= tol && std::abs(a[2] - b[2]) <= tol;
}
} // namespace

void MainWindow::refreshShellLoads()
{
    if (!m_sceneController) {
        return;
    }
    using namespace Structura::Shell;
    std::vector<Structura::Visualization::ShellRenderer::LoadArrowGlyph> glyphs;
    const bool any = std::any_of(m_extras.shell.loads.begin(), m_extras.shell.loads.end(), [](const Load &l) { return isPointLoad(l.type); });
    if (any && !m_shellMesh.quads.empty()) {
        const std::vector<LoadArrow> arrows = loadArrows(m_extras.shell, m_shellMesh);
        std::array<double, 3> lo {{1e300, 1e300, 1e300}}, hi {{-1e300, -1e300, -1e300}};
        for (const auto &p : m_shellMesh.nodes) {
            for (std::size_t k = 0; k < 3; ++k) {
                lo[k] = std::min(lo[k], p[k]);
                hi[k] = std::max(hi[k], p[k]);
            }
        }
        const double diag = std::sqrt((hi[0] - lo[0]) * (hi[0] - lo[0]) + (hi[1] - lo[1]) * (hi[1] - lo[1]) + (hi[2] - lo[2]) * (hi[2] - lo[2]));
        const double upm = shellUnitsPerMeter();
        const double base = std::max(0.08 * diag, 1e-3) * upm;
        // comprimento relativo ao maior valor de cada tipo (forca, linear e superficie tem unidades diferentes)
        double maxMag[3] = {0.0, 0.0, 0.0};
        for (const LoadArrow &a : arrows) {
            maxMag[std::clamp(a.kind, 0, 2)] = std::max(maxMag[std::clamp(a.kind, 0, 2)], a.magnitude);
        }
        for (const LoadArrow &a : arrows) {
            const int k = std::clamp(a.kind, 0, 2);
            Structura::Visualization::ShellRenderer::LoadArrowGlyph g;
            g.tip = {static_cast<float>(a.position[0] * upm), static_cast<float>(a.position[1] * upm), static_cast<float>(a.position[2] * upm)};
            g.direction = {static_cast<float>(a.direction[0]), static_cast<float>(a.direction[1]), static_cast<float>(a.direction[2])};
            g.length = static_cast<float>(base * (0.35 + 0.65 * (maxMag[k] > 0.0 ? a.magnitude / maxMag[k] : 1.0)));
            g.kind = k;
            glyphs.push_back(g);
        }
    }
    m_sceneController->setShellLoadArrows(glyphs);
    m_sceneController->setShellLoadArrowsVisible(m_shellLoadsShown && m_shellMeshVisible);
}

void MainWindow::setShellLoadsShown(bool shown)
{
    m_shellLoadsShown = shown;
    if (m_shellShowLoadsAction && m_shellShowLoadsAction->isChecked() != shown) {
        const QSignalBlocker blocker(m_shellShowLoadsAction);
        m_shellShowLoadsAction->setChecked(shown);
    }
    refreshShellLoads();
}

namespace {

/// Coordenadas exatas (double) dos nos selecionados (a referencia da selecao vem de vertices float).
std::vector<std::array<double, 3>> exactSelectionPoints(const Structura::Shell::NodeTable &table, const Structura::Shell::Mesh &mesh, const std::vector<int> &nodeIds)
{
    std::vector<std::array<double, 3>> points;
    for (const int id : nodeIds) {
        const int ti = table.indexOf(id);
        if (ti < 0) {
            continue;
        }
        std::array<double, 3> p = table.restPos[static_cast<std::size_t>(ti)];
        if (id >= 0 && static_cast<std::size_t>(id) < mesh.nodes.size() && samePoint(mesh.nodes[static_cast<std::size_t>(id)], p, 1e-3)) {
            p = mesh.nodes[static_cast<std::size_t>(id)];
        }
        points.push_back(p);
    }
    return points;
}

} // namespace

void MainWindow::applyShellLoadToSelection()
{
    using namespace Structura::Shell;
    if (m_shellNodeSel.empty()) {
        showStatusMessage(tr("Selecione nós de cascas primeiro (botão \"Selecionar nós\")."), 5000);
        return;
    }
    const auto points = exactSelectionPoints(m_shellNodeTable, m_shellMesh, m_shellNodeSel.ids());
    if (points.empty()) {
        showStatusMessage(tr("Os nós selecionados não estão na malha atual."), 5000);
        return;
    }
    // alvos de cada tipo na malha atual (arestas e elementos com todos os nos na selecao)
    Model probe;
    probe.objects = m_extras.shell.objects;
    for (const Load::Type t : {Load::LinePoints, Load::RegionPoints}) {
        Load l;
        l.type = t;
        l.points = points;
        probe.loads.push_back(l);
    }
    const int edges = static_cast<int>(resolvePointLoad(probe, m_shellMesh, 0).edges.size());
    const int elements = static_cast<int>(resolvePointLoad(probe, m_shellMesh, 1).elements.size());
    QStringList cases;
    for (const auto &c : m_extras.loadCases) {
        cases << c.name;
    }
    if (cases.isEmpty()) {
        cases << Structura::App::ProjectData::defaultLoadCaseName();
    }
    Structura::UI::ShellLoadDialog dialog(cases, static_cast<int>(points.size()), edges, elements, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    Load load = dialog.load();
    if (load.caseName.empty()) {
        showStatusMessage(tr("Informe o caso de carga."), 4000);
        return;
    }
    if (load.v[0] == 0.0 && load.v[1] == 0.0 && load.v[2] == 0.0) {
        showStatusMessage(tr("Valor da carga nulo: nada a aplicar."), 4000);
        return;
    }
    {
        StateGuard guard(this, tr("Carga de casca"));
        auto &loads = m_extras.shell.loads;
        // mesma carga (caso, tipo e valores) em outros nos: junta os pontos
        Load *target = nullptr;
        for (auto &l : loads) {
            if (l.type == load.type && l.caseName == load.caseName && l.object == 0 && l.v == load.v && isPointLoad(l.type)) {
                target = &l;
                break;
            }
        }
        if (!target) {
            load.object = 0;
            loads.push_back(load);
            target = &loads.back();
        }
        for (const auto &p : points) {
            const bool present = std::any_of(target->points.begin(), target->points.end(), [&](const std::array<double, 3> &q) { return samePoint(p, q, kLoadPointTol); });
            if (!present) {
                target->points.push_back(p);
            }
        }
        m_shellResultsShown = false;
        clearShellNodeSelection();
        refreshShellVisuals(true);
        if (m_shellDialog) {
            m_shellDialog->setModel(m_extras.shell);
        }
        markModified();
    }
    showStatusMessage(tr("Carga aplicada no caso \"%1\". Reanalise para atualizar os resultados.").arg(QString::fromStdString(dialog.load().caseName)), 6000);
}

void MainWindow::removeShellLoadFromSelection()
{
    using namespace Structura::Shell;
    if (m_shellNodeSel.empty()) {
        showStatusMessage(tr("Selecione nós de cascas primeiro (botão \"Selecionar nós\")."), 5000);
        return;
    }
    const auto points = exactSelectionPoints(m_shellNodeTable, m_shellMesh, m_shellNodeSel.ids());
    int removed = 0;
    {
        StateGuard guard(this, tr("Remover cargas de casca"));
        auto &loads = m_extras.shell.loads;
        for (auto &l : loads) {
            if (!isPointLoad(l.type)) {
                continue;
            }
            const std::size_t before = l.points.size();
            l.points.erase(std::remove_if(l.points.begin(), l.points.end(),
                                          [&](const std::array<double, 3> &q) { return std::any_of(points.begin(), points.end(), [&](const std::array<double, 3> &p) { return samePoint(p, q, kLoadPointTol); }); }),
                           l.points.end());
            removed += static_cast<int>(before - l.points.size());
        }
        loads.erase(std::remove_if(loads.begin(), loads.end(), [](const Load &l) { return isPointLoad(l.type) && l.points.empty(); }), loads.end());
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
        showStatusMessage(tr("%1 ponto(s) de carga removido(s). Cargas de objeto (peso próprio, pressão, líquido...) são editadas no Editor de cascas.").arg(removed), 7000);
    } else {
        showStatusMessage(tr("Os nós selecionados não têm carga por clique."), 6000);
    }
}
