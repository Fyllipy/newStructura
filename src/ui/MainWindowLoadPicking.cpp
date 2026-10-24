#include "MainWindow.h"

#include "LoadCaseVisibilityDialog.h"
#include "SelectionModel.h"
#include "ViewOptionsBar.h"
#include "viz/SceneController.h"

#include <QAction>
#include <QKeyEvent>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
#include <QVTKOpenGLNativeWidget.h>

// ---------------------------------------------------------------------------------------------------------------------
// Casos visiveis na viewport
// ---------------------------------------------------------------------------------------------------------------------

bool MainWindow::loadCaseShown(const QString &loadCase) const
{
    if (!m_filterVisibleLoadCases) {
        return m_extras.activeLoadCase.isEmpty() || (loadCase.isEmpty() ? loadCaseNames().first() : loadCase) == m_extras.activeLoadCase;
    }
    return m_visibleLoadCases.contains(loadCase.isEmpty() ? loadCaseNames().first() : loadCase);
}

void MainWindow::onLoadCaseVisibility()
{
    const QStringList names = loadCaseNames();
    QSet<QString> current = m_visibleLoadCases;
    if (!m_filterVisibleLoadCases) {
        // sem filtro: todos os casos (ou so o ativo da barra de vista, quando houver) estao marcados
        current.clear();
        for (const QString &name : names) {
            if (m_extras.activeLoadCase.isEmpty() || name == m_extras.activeLoadCase) {
                current.insert(name);
            }
        }
    }
    Structura::UI::LoadCaseVisibilityDialog dialog(names, current, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_visibleLoadCases = dialog.visibleCases();
    m_filterVisibleLoadCases = !dialog.allChecked();
    if (!m_filterVisibleLoadCases) {
        m_visibleLoadCases.clear();
    } else if (m_viewOptionsBar) {
        // o seletor "Caso" da barra de vista passa a refletir "(todos)": a lista marcada tem precedencia
        m_extras.activeLoadCase.clear();
        m_viewOptionsBar->setCases(names, QString());
    }
    clearSelectedLoad();
    syncLoadVisuals();
    showStatusMessage(m_filterVisibleLoadCases ? tr("Exibindo %1 de %2 caso(s) de carregamento.").arg(m_visibleLoadCases.size()).arg(names.size())
                                               : tr("Exibindo os carregamentos de todos os casos."),
                      4000);
}

// ---------------------------------------------------------------------------------------------------------------------
// Aplicar carga sem selecao previa: escolhe os alvos na viewport e conclui com Enter ou botao direito
// ---------------------------------------------------------------------------------------------------------------------

void MainWindow::beginLoadPick(LoadPick kind)
{
    cancelLoadPick();
    clearSelectedLoad();
    m_loadPick = kind;
    m_selectionModel->clear();
    QString what;
    switch (kind) {
    case LoadPick::Nodal: what = tr("Força concentrada: selecione um ou mais nós"); break;
    case LoadPick::Distributed: what = tr("Carga distribuída: selecione uma ou mais barras"); break;
    case LoadPick::Moment: what = tr("Momento distribuído: selecione uma ou mais barras"); break;
    case LoadPick::None: return;
    }
    showStatusMessage(tr("%1 (clique, Ctrl/Shift para somar ou janela) e pressione Enter ou o botão direito do mouse para concluir; Esc cancela.").arg(what), 0);
    if (m_vtkWidget) {
        m_vtkWidget->setFocus();
    }
}

void MainWindow::cancelLoadPick()
{
    if (m_loadPick == LoadPick::None) {
        return;
    }
    m_loadPick = LoadPick::None;
    showStatusMessage(tr("Comando de carga cancelado."), 3000);
}

void MainWindow::finishLoadPick()
{
    if (m_loadPick == LoadPick::None || !m_selectionModel) {
        return;
    }
    const LoadPick kind = m_loadPick;
    const bool hasTargets = kind == LoadPick::Nodal ? !m_selectionModel->selectedNodes().isEmpty() : !m_selectionModel->selectedBars().isEmpty();
    m_loadPick = LoadPick::None;
    if (!hasTargets) {
        showStatusMessage(kind == LoadPick::Nodal ? tr("Nenhum nó selecionado: comando de carga encerrado.") : tr("Nenhuma barra selecionada: comando de carga encerrado."), 4000);
        return;
    }
    showStatusMessage(QString(), 1);
    switch (kind) {
    case LoadPick::Nodal: onApplyNodalLoad(); break;
    case LoadPick::Distributed: onApplyDistributedLoad(); break;
    case LoadPick::Moment: onApplyMemberMoment(); break;
    case LoadPick::None: break;
    }
}

bool MainWindow::handleLoadPickKey(QKeyEvent *event)
{
    if (m_loadPick == LoadPick::None) {
        return false;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        finishLoadPick();
        return true;
    }
    if (event->key() == Qt::Key_Escape) {
        cancelLoadPick();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
// Selecao individual de cargas na viewport
// ---------------------------------------------------------------------------------------------------------------------

bool MainWindow::trySelectLoadAt(const QPoint &widgetPos)
{
    if (!m_sceneController || !m_vtkWidget || m_loadPick != LoadPick::None) {
        return false;
    }
    const auto ref = m_sceneController->pickLoad(QPointF(widgetPos), m_vtkWidget->width(), m_vtkWidget->height());
    if (!ref.valid()) {
        return false;
    }
    m_selectionModel->clear();
    selectShellObject(-1);
    m_sceneController->setHighlightedLoad(ref);
    updateEditActionsEnabled();
    QString text;
    switch (ref.kind) {
    case 0:
        if (ref.index >= 0 && ref.index < m_nodalLoads.size()) {
            const auto &l = m_nodalLoads[ref.index];
            text = tr("Carga nodal no nó %1 (caso %2): F = [%3; %4; %5] kN, M = [%6; %7; %8] kN·m").arg(l.nodeId).arg(l.loadCase).arg(l.fx).arg(l.fy).arg(l.fz).arg(l.mx).arg(l.my).arg(l.mz);
        }
        break;
    case 1:
        if (ref.index >= 0 && ref.index < m_memberLoads.size()) {
            const auto &l = m_memberLoads[ref.index];
            text = tr("Carga distribuída na barra %1 (caso %2): q = [%3; %4; %5] kN/m").arg(l.memberId).arg(l.loadCase).arg(l.qx).arg(l.qy).arg(l.qz);
        }
        break;
    case 2:
        if (ref.index >= 0 && ref.index < m_extras.memberMoments.size()) {
            const auto &l = m_extras.memberMoments[ref.index];
            text = tr("Momento distribuído na barra %1 (caso %2): m = [%3; %4; %5] kN·m/m").arg(l.memberId).arg(l.loadCase).arg(l.mx).arg(l.my).arg(l.mz);
        }
        break;
    default:
        text = tr("Peso próprio (definido no caso de carga: não pode ser excluído individualmente).");
        break;
    }
    showStatusMessage(text + (ref.kind <= 2 ? tr("  —  Delete exclui esta carga.") : QString()), 0);
    return true;
}

void MainWindow::clearSelectedLoad()
{
    if (m_sceneController && m_sceneController->highlightedLoad().valid()) {
        m_sceneController->setHighlightedLoad({});
        updateEditActionsEnabled();
    }
}

bool MainWindow::deleteSelectedLoad()
{
    if (!m_sceneController) {
        return false;
    }
    const auto ref = m_sceneController->highlightedLoad();
    if (!ref.valid()) {
        return false;
    }
    if (ref.kind == 3) {
        showStatusMessage(tr("O peso próprio vem do caso de carga: desmarque-o em Casos e combinações."), 5000);
        return true;
    }
    {
        StateGuard guard(this, tr("Excluir carga"));
        bool removed = false;
        if (ref.kind == 0 && ref.index >= 0 && ref.index < m_nodalLoads.size()) {
            m_nodalLoads.removeAt(ref.index);
            removed = true;
        } else if (ref.kind == 1 && ref.index >= 0 && ref.index < m_memberLoads.size()) {
            m_memberLoads.removeAt(ref.index);
            removed = true;
        } else if (ref.kind == 2 && ref.index >= 0 && ref.index < m_extras.memberMoments.size()) {
            m_extras.memberMoments.removeAt(ref.index);
            removed = true;
        }
        m_sceneController->setHighlightedLoad({});
        if (removed) {
            syncLoadVisuals();
            refreshPropertiesPanel();
            showStatusMessage(tr("Carga excluída."), 3000);
        }
    }
    updateEditActionsEnabled();
    return true;
}
