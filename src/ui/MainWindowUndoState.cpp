#include "MainWindow.h"

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
