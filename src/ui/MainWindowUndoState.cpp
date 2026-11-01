#include "MainWindow.h"

#include "ShellDialog.h"
#include "app/UndoRedoService.h"

#include <QCryptographicHash>
#include <QUndoCommand>
#include <QUndoStack>

#include <utility>

namespace {

/// Comando de desfazer que troca o estado completo (materiais, secoes, apoios, cargas e extras).
class ModelStateCommand final : public QUndoCommand
{
public:
    ModelStateCommand(const QString &text, std::function<void()> undo, std::function<void()> redo)
        : QUndoCommand(text)
        , m_undo(std::move(undo))
        , m_redo(std::move(redo))
    {
    }
    void undo() override { m_undo(); }
    void redo() override
    {
        if (m_first) {   // o estado novo ja esta aplicado quando o comando entra na pilha
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

} // namespace

MainWindow::ModelState MainWindow::captureState() const
{
    return {m_materials, m_sections, m_supports, m_nodalLoads, m_memberLoads, m_extras};
}

void MainWindow::applyState(const ModelState &state)
{
    m_materials = state.materials;
    m_sections = state.sections;
    m_supports = state.supports;
    m_nodalLoads = state.nodalLoads;
    m_memberLoads = state.memberLoads;
    m_extras = state.extras;
    syncLoadVisuals();
    syncSupportVisuals();
    syncPlacementVisuals();
    rebuildCaseSelector();
    refreshGroupsPanel();
    refreshLoadsPanel();
    refreshPropertiesPanel();
    applyAppearance();
    m_selectedShellObject = -1;
    m_shellResultsShown = false;
    refreshShellVisuals(true);
    if (m_shellDialog) {
        m_shellDialog->setModel(m_extras.shell);
    }
}

// Impressao digital barata do estado: detecta se uma acao alterou algo (evita entradas vazias na pilha).
quint64 MainWindow::stateFingerprint() const
{
    QByteArray bytes;
    auto num = [&bytes](double v) { bytes += QByteArray::number(v, 'g', 12) + ','; };
    auto text = [&bytes](const QString &s) { bytes += s.toUtf8() + ';'; };
    for (const auto &m : m_materials) {
        text(m.name);
        num(m.youngModulus);
        num(m.shearModulus);
        num(m.unitWeight);
        num(m.thermalExpansion);
        num(m.fy);
        num(m.fu);
    }
    bytes += '|';
    for (const auto &s : m_sections) {
        text(s.name);
        num(s.area);
        num(s.iz);
        num(s.iy);
        num(s.j);
    }
    bytes += '|';
    for (const auto &s : m_supports) {
        num(s.nodeId);
        for (const bool r : s.restraints) {
            bytes += r ? '1' : '0';
        }
    }
    bytes += '|';
    for (const auto &l : m_nodalLoads) {
        num(l.nodeId);
        num(l.fx); num(l.fy); num(l.fz); num(l.mx); num(l.my); num(l.mz);
        text(l.loadCase);
    }
    bytes += '|';
    for (const auto &l : m_memberLoads) {
        num(l.memberId);
        num(l.qx); num(l.qy); num(l.qz);
        text(l.loadCase);
        text(l.system);
    }
    bytes += '|';
    for (const auto &m : m_extras.memberMoments) {
        num(m.memberId);
        num(m.mx); num(m.my); num(m.mz);
        text(m.loadCase);
    }
    bytes += '|';
    for (const auto &a : m_extras.memberActions) {
        num(a.memberId); num(a.kind); num(a.a); num(a.b);
        for (const double v : a.v1) { num(v); }
        for (const double v : a.v2) { num(v); }
        text(a.loadCase);
        text(a.system);
    }
    bytes += '|';
    for (const auto &d : m_extras.diaphragms) {
        text(d.name); num(d.masterNode); num(d.normalAxis);
        for (const int slave : d.slaveNodes) { num(slave); }
    }
    for (const auto &l : m_extras.rigidLinks) {
        num(l.masterNode); num(l.slaveNode); num(l.kind); num(l.dof);
    }
    for (const auto &s : m_extras.inclinedSupports) {
        num(s.nodeId); num(s.mask);
        for (const double a : s.axes) { num(a); }
    }
    for (const auto &c : m_extras.constraints) {
        bytes += c.reportReaction ? '1' : '0';
        for (int i = 0; i < c.nodes.size(); ++i) { num(c.nodes[i]); num(c.dofs.value(i)); num(c.coefs.value(i)); }
    }
    for (const auto &spec : m_extras.memberConnections) {
        num(spec.memberId);
        for (const double k : spec.springK) { num(k); }
        for (const auto &h : spec.hinges) { num(h.dof); num(h.kElastic); num(h.yieldMoment); num(h.hardeningRatio); num(h.axialYield); num(h.exponent); }
    }
    for (const auto &link : m_extras.links) {
        num(link.id); text(link.name); num(link.nodeI); num(link.nodeJ);
        for (const double a : link.axes) { num(a); }
        for (const auto &law : link.laws) {
            num(law.kind); num(law.k); num(law.p1); num(law.p2);
            for (const auto &point : law.table) { num(point.first); num(point.second); }
        }
    }
    {
        const auto &n = m_extras.nonlinear;
        bytes += n.force ? '1' : '0';
        num(n.steps); num(n.maxIterations); num(n.tolerance); num(n.criteria); num(n.maxCutbacks); num(n.controlMode); num(n.controlNode); num(n.controlDof);
        num(n.controlTarget); num(n.loadFactorMax);
    }
    bytes += '|';
    for (const auto &c : m_extras.loadCases) {
        text(c.name);
        text(c.abbreviation);
        bytes += c.selfWeight ? '1' : '0';
    }
    for (const auto &c : m_extras.combinations) {
        text(c.name);
        for (const auto &f : c.factors) {
            text(f.first);
            num(f.second);
        }
    }
    bytes += '|';
    for (const auto &p : m_extras.placements) {
        num(p.memberId); num(p.rotationDeg); num(p.reference); num(p.offsetA); num(p.offsetB);
        num(p.offsetA2); num(p.offsetB2); num(p.releaseMask); num(p.memberType);
        bytes += p.endDiffers ? '1' : '0';
    }
    bytes += '|';
    for (const auto &g : m_extras.groups) {
        num(g.id);
        text(g.name);
        for (const int member : g.members) {
            num(member);
        }
    }
    bytes += '|';
    for (const auto &a : m_extras.areaLoads) {
        num(a.id);
        text(a.loadCase);
        num(a.pressure);
        for (const int member : a.members) {
            num(member);
        }
    }
    bytes += '|' + QByteArray::fromStdString(Structura::Shell::fingerprint(m_extras.shell));
    return qHash(QCryptographicHash::hash(bytes, QCryptographicHash::Md5), 0);
}

MainWindow::StateGuard::StateGuard(MainWindow *window, const QString &text)
    : m_window(window)
    , m_text(text)
{
    if (m_window) {
        m_before = m_window->captureState();
        m_fingerprint = m_window->stateFingerprint();
    }
}

MainWindow::StateGuard::~StateGuard()
{
    if (!m_window || !m_window->m_undoService || m_window->stateFingerprint() == m_fingerprint) {
        return;
    }
    MainWindow *window = m_window;
    const ModelState before = m_before;
    const ModelState after = window->captureState();
    window->m_undoService->stack()->push(new ModelStateCommand(
        m_text, [window, before]() { window->applyState(before); }, [window, after]() { window->applyState(after); }));
}
