#include "MainWindow.h"

#include "PlacementController.h"
#include "SelectionModel.h"
#include "SplitBarDialog.h"
#include "TrussDialog.h"
#include "app/UndoRedoService.h"
#include "app/services/ShellCouplingService.h"
#include "shell/ShellMesh.h"
#include "viz/SceneController.h"

#include <QLabel>
#include <QMessageBox>
#include <QVTKOpenGLNativeWidget.h>
#include <QUndoCommand>
#include <QUndoStack>

#include <algorithm>
#include <cmath>
#include <functional>

namespace {

using Geometry = SceneController::GeometrySnapshot;

/// Desfazer/refazer de uma alteracao de topologia: a acao ja foi aplicada quando o comando entra na pilha.
class TopologyCommand final : public QUndoCommand
{
public:
    TopologyCommand(const QString &text, std::function<void()> undo, std::function<void()> redo)
        : QUndoCommand(text)
        , m_undo(std::move(undo))
        , m_redo(std::move(redo))
    {
    }
    void undo() override { m_undo(); }
    void redo() override
    {
        if (m_first) {
            m_first = false;
            return;
        }
        m_redo();
    }

private:
    std::function<void()> m_undo;
    std::function<void()> m_redo;
    bool m_first {true};
};

template <typename Container, typename Predicate>
void eraseWhere(Container &container, Predicate predicate)
{
    container.erase(std::remove_if(container.begin(), container.end(), predicate), container.end());
}

} // namespace

void MainWindow::applyTopology(const SceneController::GeometrySnapshot &geometry, const ModelState &state)
{
    m_sceneController->restoreGeometry(geometry);
    if (m_selectionModel) {
        m_selectionModel->clear();
    }
    // ids de elementos que deixaram de existir saem dos conjuntos de ocultacao
    QSet<QUuid> nodeIds, barIds;
    for (const auto &n : geometry.nodes) {
        nodeIds.insert(n.id);
    }
    for (const auto &b : geometry.bars) {
        barIds.insert(b.id);
    }
    m_hiddenNodes.intersect(nodeIds);
    m_hiddenBars.intersect(barIds);
    applyState(state);
    updateEditActionsEnabled();
    updateStatus();
}

void MainWindow::onDeleteSelected()
{
    if (!m_sceneController || !m_selectionModel || m_command != Command::None) {
        return;
    }
    const QSet<QUuid> selectedNodes = m_selectionModel->selectedNodes();
    const QSet<QUuid> selectedBars = m_selectionModel->selectedBars();
    if (selectedNodes.isEmpty() && selectedBars.isEmpty() && m_selectedShellObject >= 0) {
        deleteSelectedShell();
        return;
    }
    if (selectedNodes.isEmpty() && selectedBars.isEmpty()) {
        showStatusMessage(tr("Selecione nós, barras ou cascas para excluir."), 3000);
        return;
    }

    const Geometry before = m_sceneController->captureGeometry();
    const ModelState stateBefore = captureState();

    // barras ligadas a um no excluido saem junto
    QSet<QUuid> removeBars = selectedBars;
    int attached = 0;
    for (const auto &bar : before.bars) {
        if (!removeBars.contains(bar.id) && (selectedNodes.contains(bar.start) || selectedNodes.contains(bar.end))) {
            removeBars.insert(bar.id);
            ++attached;
        }
    }
    if (attached > 0) {
        const auto answer = QMessageBox::question(this, tr("Excluir"),
                                                  tr("%1 barra(s) ligada(s) aos nós selecionados também serão excluídas. Continuar?").arg(attached));
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    QSet<int> nodeExt, barExt;
    Geometry after;
    for (const auto &n : before.nodes) {
        if (selectedNodes.contains(n.id)) {
            nodeExt.insert(n.externalId);
        } else {
            after.nodes.push_back(n);
        }
    }
    for (const auto &b : before.bars) {
        if (removeBars.contains(b.id)) {
            barExt.insert(b.externalId);
        } else {
            after.bars.push_back(b);
        }
    }

    // dados do projeto que dependem dos elementos excluidos
    ModelState state = stateBefore;
    eraseWhere(state.supports, [&](const auto &s) { return nodeExt.contains(s.nodeId); });
    eraseWhere(state.nodalLoads, [&](const auto &l) { return nodeExt.contains(l.nodeId); });
    eraseWhere(state.memberLoads, [&](const auto &l) { return barExt.contains(l.memberId); });
    auto &extras = state.extras;
    eraseWhere(extras.memberMoments, [&](const auto &m) { return barExt.contains(m.memberId); });
    eraseWhere(extras.placements, [&](const auto &p) { return barExt.contains(p.memberId); });
    eraseWhere(extras.design.members, [&](const auto &p) { return barExt.contains(p.memberId); });
    eraseWhere(extras.connections, [&](const auto &c) { return nodeExt.contains(c.nodeId) || barExt.contains(c.memberId); });
    for (auto &group : extras.groups) {
        eraseWhere(group.members, [&](int id) { return barExt.contains(id); });
    }
    eraseWhere(extras.groups, [](const auto &g) { return g.members.isEmpty(); });
    for (auto &panel : extras.areaLoads) {
        eraseWhere(panel.members, [&](int id) { return barExt.contains(id); });
    }
    eraseWhere(extras.areaLoads, [](const auto &a) { return a.members.isEmpty(); });

    applyTopology(after, state);
    if (m_undoService) {
        m_undoService->stack()->push(new TopologyCommand(
            tr("Excluir elementos"),
            [this, before, stateBefore]() { applyTopology(before, stateBefore); },
            [this, after, state]() { applyTopology(after, state); }));
    }
    refreshPropertiesPanel();
    showStatusMessage(tr("%1 nó(s) e %2 barra(s) excluído(s). Ctrl+Z desfaz.").arg(selectedNodes.size()).arg(removeBars.size()), 5000);
}

void MainWindow::onInsertNode()
{
    setCommand(Command::InsertNode);
    if (m_vtkWidget) {
        m_vtkWidget->setFocus();
    }
}

/// Divide barras do plano: cada barra recebe nos nas fracoes indicadas; o primeiro trecho conserva o numero da barra
/// e os demais ganham numeros novos. Cargas uniformes, grupos e parametros acompanham todos os trechos.
void MainWindow::splitCore(SceneController::GeometrySnapshot &after, ModelState &state, const std::vector<SplitPlan> &plan)
{
    int maxBar = 0;
    for (const auto &b : after.bars) {
        maxBar = std::max(maxBar, b.externalId);
    }
    auto &extras = state.extras;
    for (const SplitPlan &item : plan) {
        const Geometry::B original = after.bars[item.barIndex];
        const int oldExt = original.externalId;
        const std::size_t segments = item.cuts.size() + 1;
        std::vector<double> edge(segments + 1, 0.0);
        std::vector<QUuid> nodeAt(segments + 1);
        nodeAt[0] = original.start;
        nodeAt[segments] = original.end;
        edge[segments] = 1.0;
        for (std::size_t k = 0; k < item.cuts.size(); ++k) {
            edge[k + 1] = item.cuts[k].first;
            nodeAt[k + 1] = item.cuts[k].second;
        }
        std::vector<int> ext(segments, oldExt);
        for (std::size_t s = 1; s < segments; ++s) {
            ext[s] = ++maxBar;
        }
        // geometria
        after.bars[item.barIndex].end = nodeAt[1];
        for (std::size_t s = 1; s < segments; ++s) {
            Geometry::B part = original;
            part.id = QUuid::createUuid();
            part.start = nodeAt[s];
            part.end = nodeAt[s + 1];
            part.externalId = ext[s];
            after.bars.push_back(part);
        }
        // posicionamento: deslocamentos interpolados; articulacoes ficam nas extremidades originais
        for (int i = 0; i < extras.placements.size(); ++i) {
            if (extras.placements[i].memberId != oldExt) {
                continue;
            }
            const auto base = extras.placements[i];
            auto lerpA = [&](double t) { return base.offsetA + t * (base.endOffsetA() - base.offsetA); };
            auto lerpB = [&](double t) { return base.offsetB + t * (base.endOffsetB() - base.offsetB); };
            for (std::size_t s = 0; s < segments; ++s) {
                auto p = base;
                p.memberId = ext[s];
                p.offsetA = lerpA(edge[s]);
                p.offsetB = lerpB(edge[s]);
                p.offsetA2 = lerpA(edge[s + 1]);
                p.offsetB2 = lerpB(edge[s + 1]);
                p.endDiffers = std::abs(p.offsetA2 - p.offsetA) > 1e-12 || std::abs(p.offsetB2 - p.offsetB) > 1e-12;
                p.releaseMask = (s == 0 ? (base.releaseMask & 0x3F) : 0) | (s + 1 == segments ? (base.releaseMask & ~0x3F) : 0);
                if (s == 0) {
                    extras.placements[i] = p;
                } else {
                    extras.placements.append(p);
                }
            }
            break;
        }
        for (std::size_t s = 1; s < segments; ++s) {
            const auto loads = state.memberLoads;
            for (auto load : loads) {
                if (load.memberId == oldExt) {
                    load.memberId = ext[s];
                    state.memberLoads.append(load);
                }
            }
            const auto moments = extras.memberMoments;
            for (auto moment : moments) {
                if (moment.memberId == oldExt) {
                    moment.memberId = ext[s];
                    extras.memberMoments.append(moment);
                }
            }
            for (auto &group : extras.groups) {
                if (group.members.contains(oldExt)) {
                    group.members.append(ext[s]);
                }
            }
            for (auto &panel : extras.areaLoads) {
                if (panel.members.contains(oldExt)) {
                    panel.members.append(ext[s]);
                }
            }
            const auto memberParams = extras.design.members;
            for (auto params : memberParams) {
                if (params.memberId == oldExt) {
                    params.memberId = ext[s];
                    extras.design.members.append(params);
                }
            }
        }
        // ligacao na extremidade final da viga original passa a pertencer ao ultimo trecho
        for (auto &connection : extras.connections) {
            if (connection.memberId == oldExt && connection.atEnd) {
                connection.memberId = ext[segments - 1];
            }
        }
    }
}

namespace {

constexpr double kNodeTolerance = 1e-4;   // m

QString posKey(double x, double y, double z)
{
    return QStringLiteral("%1,%2,%3").arg(std::llround(x * 1e4)).arg(std::llround(y * 1e4)).arg(std::llround(z * 1e4));
}

} // namespace

bool MainWindow::applyInsertNode(const std::array<double, 3> &point)
{
    constexpr double kOnBarTolerance = 1e-3;   // m

    const Geometry before = m_sceneController->captureGeometry();
    for (const auto &n : before.nodes) {
        if (std::hypot(n.x - point[0], n.y - point[1], n.z - point[2]) < kNodeTolerance) {
            showStatusMessage(tr("Já existe um nó nesse ponto."), 3000);
            return false;
        }
    }
    QHash<QUuid, std::array<double, 3>> position;
    int maxNode = 0;
    for (const auto &n : before.nodes) {
        position.insert(n.id, {n.x, n.y, n.z});
        maxNode = std::max(maxNode, n.externalId);
    }
    Geometry::N node;
    node.id = QUuid::createUuid();
    node.externalId = std::max(maxNode + 1, 1);
    node.x = point[0];
    node.y = point[1];
    node.z = point[2];

    std::vector<SplitPlan> plan;
    for (std::size_t i = 0; i < before.bars.size(); ++i) {
        const auto &bar = before.bars[i];
        if (!position.contains(bar.start) || !position.contains(bar.end)) {
            continue;
        }
        const auto a = position.value(bar.start), b = position.value(bar.end);
        const double ex = b[0] - a[0], ey = b[1] - a[1], ez = b[2] - a[2];
        const double length2 = ex * ex + ey * ey + ez * ez;
        if (length2 < 1e-12) {
            continue;
        }
        const double length = std::sqrt(length2);
        const double t = ((point[0] - a[0]) * ex + (point[1] - a[1]) * ey + (point[2] - a[2]) * ez) / length2;
        if (t * length <= kOnBarTolerance || (1.0 - t) * length <= kOnBarTolerance) {
            continue;
        }
        const double dx = a[0] + t * ex - point[0], dy = a[1] + t * ey - point[1], dz = a[2] + t * ez - point[2];
        if (std::sqrt(dx * dx + dy * dy + dz * dz) <= kOnBarTolerance) {
            plan.push_back({i, {{t, node.id}}});
        }
    }

    const ModelState stateBefore = captureState();
    ModelState state = stateBefore;
    Geometry after = before;
    after.nodes.push_back(node);
    splitCore(after, state, plan);
    applyTopology(after, state);
    if (m_undoService) {
        m_undoService->stack()->push(new TopologyCommand(
            plan.empty() ? tr("Inserir nó") : tr("Inserir nó e dividir barra"),
            [this, before, stateBefore]() { applyTopology(before, stateBefore); },
            [this, after, state]() { applyTopology(after, state); }));
    }
    refreshPropertiesPanel();
    showStatusMessage(plan.empty() ? tr("Nó %1 criado.").arg(node.externalId)
                                   : tr("Nó %1 criado; %2 barra(s) dividida(s) nesse ponto.").arg(node.externalId).arg(plan.size()),
                      4000);
    return true;
}

void MainWindow::onSplitBars()
{
    if (!m_sceneController || !m_selectionModel) {
        return;
    }
    const QSet<QUuid> selected = m_selectionModel->selectedBars();
    if (selected.isEmpty()) {
        QMessageBox::information(this, tr("Dividir barras"), tr("Selecione uma ou mais barras para dividir."));
        return;
    }
    const Geometry before = m_sceneController->captureGeometry();
    QHash<QUuid, std::array<double, 3>> position;
    QHash<QString, QUuid> nodeAtPosition;
    int maxNode = 0;
    for (const auto &n : before.nodes) {
        position.insert(n.id, {n.x, n.y, n.z});
        nodeAtPosition.insert(posKey(n.x, n.y, n.z), n.id);
        maxNode = std::max(maxNode, n.externalId);
    }
    // comprimento da barra mais curta e da mais longa entre as selecionadas (para validar e pre-visualizar)
    double shortest = 1e300, longest = 0.0;
    for (const auto &bar : before.bars) {
        if (!selected.contains(bar.id) || !position.contains(bar.start) || !position.contains(bar.end)) {
            continue;
        }
        const auto a = position.value(bar.start), b = position.value(bar.end);
        const double length = std::hypot(b[0] - a[0], b[1] - a[1], b[2] - a[2]);
        shortest = std::min(shortest, length);
        longest = std::max(longest, length);
    }
    Structura::UI::SplitBarDialog dialog(selected.size(), shortest, longest, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    std::vector<SplitPlan> plan;
    std::vector<Geometry::N> created;
    int cutCount = 0;
    for (std::size_t i = 0; i < before.bars.size(); ++i) {
        const auto &bar = before.bars[i];
        if (!selected.contains(bar.id) || !position.contains(bar.start) || !position.contains(bar.end)) {
            continue;
        }
        const auto a = position.value(bar.start), b = position.value(bar.end);
        const double length = std::hypot(b[0] - a[0], b[1] - a[1], b[2] - a[2]);
        SplitPlan item {i, {}};
        for (const double t : dialog.fractions(length)) {
            if (t * length <= kNodeTolerance * 10.0 || (1.0 - t) * length <= kNodeTolerance * 10.0) {
                continue;
            }
            const double x = a[0] + t * (b[0] - a[0]), y = a[1] + t * (b[1] - a[1]), z = a[2] + t * (b[2] - a[2]);
            const QString key = posKey(x, y, z);
            QUuid id = nodeAtPosition.value(key);
            if (id.isNull()) {
                Geometry::N node;
                node.id = QUuid::createUuid();
                node.externalId = ++maxNode;
                node.x = x;
                node.y = y;
                node.z = z;
                created.push_back(node);
                nodeAtPosition.insert(key, node.id);
                id = node.id;
            }
            item.cuts.push_back({t, id});
            ++cutCount;
        }
        std::sort(item.cuts.begin(), item.cuts.end());
        if (!item.cuts.empty()) {
            plan.push_back(std::move(item));
        }
    }
    if (plan.empty()) {
        showStatusMessage(tr("Nenhuma divisão válida para as barras selecionadas."), 4000);
        return;
    }
    const ModelState stateBefore = captureState();
    ModelState state = stateBefore;
    Geometry after = before;
    for (const auto &node : created) {
        after.nodes.push_back(node);
    }
    splitCore(after, state, plan);
    applyTopology(after, state);
    if (m_undoService) {
        m_undoService->stack()->push(new TopologyCommand(
            tr("Dividir barras"), [this, before, stateBefore]() { applyTopology(before, stateBefore); },
            [this, after, state]() { applyTopology(after, state); }));
    }
    refreshPropertiesPanel();
    showStatusMessage(tr("%1 barra(s) dividida(s): %2 corte(s), %3 nó(s) novo(s). Ctrl+Z desfaz.").arg(plan.size()).arg(cutCount).arg(created.size()), 6000);
}

int MainWindow::splitBarsAtShellNodes(bool apply)
{
    if (!m_sceneController || m_extras.shell.objects.empty()) {
        return 0;
    }
    const Geometry before = m_sceneController->captureGeometry();
    // modelo minimo (nos e barras com os ids externos) para localizar os nos das cascas sobre o eixo das barras
    Structura::App::SolverBridge::ModelData lite;
    QHash<QUuid, int> externalOf;
    QHash<QString, QUuid> nodeAtPosition;
    int maxNode = 0;
    for (const auto &n : before.nodes) {
        Structura::App::SolverBridge::NodeData node;
        node.id = n.externalId;
        node.x = n.x;
        node.y = n.y;
        node.z = n.z;
        lite.nodes.push_back(node);
        externalOf.insert(n.id, n.externalId);
        nodeAtPosition.insert(posKey(n.x, n.y, n.z), n.id);
        maxNode = std::max(maxNode, n.externalId);
    }
    std::unordered_map<int, std::size_t> barOfExternal;
    for (std::size_t i = 0; i < before.bars.size(); ++i) {
        const auto &b = before.bars[i];
        Structura::App::SolverBridge::MemberData member;
        member.id = b.externalId;
        member.nodeI = externalOf.value(b.start, 0);
        member.nodeJ = externalOf.value(b.end, 0);
        lite.members.push_back(member);
        barOfExternal[b.externalId] = i;
    }
    const Structura::Shell::Mesh mesh = Structura::Shell::generateMesh(m_extras.shell);
    const auto cuts = Structura::App::Services::ShellCouplingService::barCuts(mesh, lite);
    if (cuts.empty()) {
        return 0;
    }
    std::vector<SplitPlan> plan;
    std::vector<Geometry::N> created;
    int cutCount = 0;
    for (const auto &bar : cuts) {
        const auto found = barOfExternal.find(bar.memberId);
        if (found == barOfExternal.end()) {
            continue;
        }
        SplitPlan item {found->second, {}};
        for (std::size_t k = 0; k < bar.parameters.size(); ++k) {
            const auto &p = bar.points[k];
            const QString key = posKey(p[0], p[1], p[2]);
            QUuid id = nodeAtPosition.value(key);
            if (id.isNull()) {
                Geometry::N node;
                node.id = QUuid::createUuid();
                node.externalId = ++maxNode;
                node.x = p[0];
                node.y = p[1];
                node.z = p[2];
                created.push_back(node);
                nodeAtPosition.insert(key, node.id);
                id = node.id;
            }
            item.cuts.push_back({bar.parameters[k], id});
            ++cutCount;
        }
        plan.push_back(std::move(item));
    }
    if (plan.empty()) {
        return 0;
    }
    if (!apply) {
        return cutCount;
    }
    const ModelState stateBefore = captureState();
    ModelState state = stateBefore;
    Geometry after = before;
    for (const auto &node : created) {
        after.nodes.push_back(node);
    }
    splitCore(after, state, plan);
    applyTopology(after, state);
    if (m_undoService) {
        m_undoService->stack()->push(new TopologyCommand(
            tr("Dividir barras nos nós das cascas"), [this, before, stateBefore]() { applyTopology(before, stateBefore); },
            [this, after, state]() { applyTopology(after, state); }));
    }
    refreshPropertiesPanel();
    return cutCount;
}

void MainWindow::onMeasure()
{
    setCommand(Command::Measure);
    if (m_vtkWidget) {
        m_vtkWidget->setFocus();
    }
}

bool MainWindow::applyMeasure(const std::array<double, 3> &from, const std::array<double, 3> &to)
{
    const double dx = to[0] - from[0], dy = to[1] - from[1], dz = to[2] - from[2];
    const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (distance < 1e-9) {
        showStatusMessage(tr("Os dois pontos coincidem."), 3000);
        return false;
    }
    const double horizontal = std::hypot(dx, dy);
    const double slope = std::atan2(dz, horizontal) * 180.0 / 3.14159265358979323846;
    const double azimuth = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846;
    const QString html = tr("<b>Medição</b><br>Distância: <b>%1 m</b><br>ΔX = %2 m · ΔY = %3 m · ΔZ = %4 m<br>"
                            "Distância em planta: %5 m · inclinação: %6° · azimute: %7°")
                             .arg(distance, 0, 'f', 4).arg(dx, 0, 'f', 4).arg(dy, 0, 'f', 4).arg(dz, 0, 'f', 4)
                             .arg(horizontal, 0, 'f', 4).arg(slope, 0, 'f', 2).arg(azimuth, 0, 'f', 2);
    if (!m_measureCard && m_vtkWidget) {
        m_measureCard = new QLabel(m_vtkWidget);
        m_measureCard->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_measureCard->setTextFormat(Qt::RichText);
        m_measureCard->setStyleSheet(QStringLiteral("QLabel { background: rgba(255,255,255,245); border: 1px solid #0d7fb9; border-radius: 6px; padding: 8px 12px; color: #1e232b; font-size: 12px; }"));
    }
    if (m_measureCard) {
        m_measureCard->setText(html);
        m_measureCard->adjustSize();
        m_measureCard->move((m_vtkWidget->width() - m_measureCard->width()) / 2, 12);
        m_measureCard->show();
        m_measureCard->raise();
    }
    showStatusMessage(tr("Distância %1 m (ΔX %2, ΔY %3, ΔZ %4).").arg(distance, 0, 'f', 3).arg(dx, 0, 'f', 3).arg(dy, 0, 'f', 3).arg(dz, 0, 'f', 3), 8000);
    return true;
}

void MainWindow::onGenerateTruss()
{
    using Structura::App::Services::TrussGenerator;
    if (!m_sceneController || m_command != Command::None) {
        return;
    }
    if (m_sections.isEmpty() || m_materials.isEmpty()) {
        QMessageBox::information(this, tr("Gerar treliça"), tr("Crie ao menos um material e um perfil antes de gerar a treliça."));
        return;
    }
    QStringList sectionNames, materialNames;
    for (const auto &s : m_sections) {
        sectionNames << s.name;
    }
    for (const auto &m : m_materials) {
        materialNames << m.name;
    }
    Structura::UI::TrussDialog dialog(sectionNames, materialNames, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto request = dialog.request();
    const auto shape = TrussGenerator::generate(request.shape);
    if (shape.nodes.empty() || shape.bars.empty()) {
        return;
    }
    auto sectionAt = [this](int i) { return m_sections.at(std::clamp(i, 0, static_cast<int>(m_sections.size()) - 1)).uuid; };
    const QUuid material = m_materials.at(std::clamp(request.material, 0, static_cast<int>(m_materials.size()) - 1)).uuid;

    const Geometry before = m_sceneController->captureGeometry();
    const ModelState stateBefore = captureState();
    Geometry after = before;
    ModelState state = stateBefore;
    int nextNode = 0, nextBar = 0;
    for (const auto &n : before.nodes) {
        nextNode = std::max(nextNode, n.externalId);
    }
    for (const auto &b : before.bars) {
        nextBar = std::max(nextBar, b.externalId);
    }
    std::vector<QUuid> nodeIds;
    std::vector<int> nodeExt;
    for (const auto &n : shape.nodes) {
        Geometry::N node;
        node.id = QUuid::createUuid();
        node.externalId = ++nextNode;
        node.x = request.x0 + (request.planeYZ ? 0.0 : n[0]);
        node.y = request.y0 + (request.planeYZ ? n[0] : 0.0);
        node.z = request.z0 + n[1];
        after.nodes.push_back(node);
        nodeIds.push_back(node.id);
        nodeExt.push_back(node.externalId);
    }
    for (const auto &b : shape.bars) {
        Geometry::B bar;
        bar.id = QUuid::createUuid();
        bar.externalId = ++nextBar;
        bar.start = nodeIds[static_cast<std::size_t>(b.a)];
        bar.end = nodeIds[static_cast<std::size_t>(b.b)];
        bar.material = material;
        const int sectionIndex = b.role == TrussGenerator::Role::Vertical ? request.verticalSection
                               : b.role == TrussGenerator::Role::Diagonal ? request.diagonalSection
                                                                          : request.chordSection;
        bar.section = sectionAt(sectionIndex);
        after.bars.push_back(bar);
        if (request.trussBars) {
            Structura::App::ProjectData::MemberPlacement placement;
            placement.memberId = bar.externalId;
            placement.releaseMask = Structura::App::ProjectData::MemberType::kReleaseRotations;
            placement.memberType = Structura::App::ProjectData::MemberType::kTruss;
            state.extras.placements.append(placement);
        }
    }
    // apoios: fixo no primeiro e movel no ultimo no do banzo inferior; travamento fora do plano em todos
    const int outOfPlane = request.planeYZ ? 0 : 1;
    int left = 0, right = 0;
    for (std::size_t i = 0; i < shape.nodes.size(); ++i) {
        if (shape.nodes[i][0] < shape.nodes[static_cast<std::size_t>(left)][0] - 1e-9 ||
            (std::abs(shape.nodes[i][0] - shape.nodes[static_cast<std::size_t>(left)][0]) < 1e-9 && shape.nodes[i][1] < shape.nodes[static_cast<std::size_t>(left)][1])) {
            left = static_cast<int>(i);
        }
        if (shape.nodes[i][0] > shape.nodes[static_cast<std::size_t>(right)][0] + 1e-9 ||
            (std::abs(shape.nodes[i][0] - shape.nodes[static_cast<std::size_t>(right)][0]) < 1e-9 && shape.nodes[i][1] < shape.nodes[static_cast<std::size_t>(right)][1])) {
            right = static_cast<int>(i);
        }
    }
    for (std::size_t i = 0; i < shape.nodes.size(); ++i) {
        Structura::App::ProjectData::NodeSupport support;
        support.nodeId = nodeExt[i];
        bool any = false;
        if (request.lockOutOfPlane) {
            support.restraints[outOfPlane] = true;
            any = true;
        }
        if (request.supports && static_cast<int>(i) == left) {
            support.restraints[0] = support.restraints[1] = support.restraints[2] = true;
            any = true;
        } else if (request.supports && static_cast<int>(i) == right) {
            support.restraints[2] = true;
            support.restraints[outOfPlane] = true;
            any = true;
        }
        if (any) {
            state.supports.append(support);
        }
    }
    applyTopology(after, state);
    if (m_undoService) {
        m_undoService->stack()->push(new TopologyCommand(
            tr("Gerar treliça"),
            [this, before, stateBefore]() { applyTopology(before, stateBefore); },
            [this, after, state]() { applyTopology(after, state); }));
    }
    m_sceneController->zoomExtents();
    showStatusMessage(tr("Treliça gerada: %1 nós e %2 barras (%3 painéis). Ctrl+Z desfaz.").arg(shape.nodes.size()).arg(shape.bars.size()).arg(shape.panels), 6000);
}
