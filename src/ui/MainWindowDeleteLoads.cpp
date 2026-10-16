#include "MainWindow.h"

#include "DeleteLoadsDialog.h"
#include "SelectionModel.h"
#include "viz/SceneController.h"

#include <QMessageBox>
#include <QSet>
#include <QStatusBar>

#include <algorithm>

void MainWindow::onDeleteLoads()
{
    StateGuard guard(this, tr("Excluir cargas"));
    if (!m_sceneController || !m_selectionModel) {
        return;
    }
    // ids externos da selecao
    QSet<int> nodeIds;
    QSet<int> barIds;
    const auto selectedNodes = m_selectionModel->selectedNodes();
    for (const auto &node : m_sceneController->nodeInfos()) {
        if (selectedNodes.contains(node.id)) {
            nodeIds.insert(node.externalId);
        }
    }
    const auto selectedBars = m_selectionModel->selectedBars();
    for (const auto &bar : m_sceneController->bars()) {
        if (selectedBars.contains(bar.id)) {
            barIds.insert(bar.externalId);
        }
    }

    Structura::UI::DeleteLoadsDialog dialog(loadCaseNames(), nodeIds.size(), barIds.size(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const bool all = dialog.wholeModel();
    const QString caseName = dialog.loadCase();
    if (all && QMessageBox::question(this, tr("Excluir cargas"),
                                     tr("Excluir as cargas escolhidas em TODO o modelo%1?").arg(caseName.isEmpty() ? tr(" (todos os casos)") : tr(" (caso \"%1\")").arg(caseName)))
                   != QMessageBox::Yes) {
        return;
    }
    auto caseMatches = [&](const QString &c) { return caseName.isEmpty() || c == caseName; };

    int nodal = 0, distributed = 0, moments = 0, panels = 0;
    if (dialog.nodalForces()) {
        for (int i = m_nodalLoads.size() - 1; i >= 0; --i) {
            if (caseMatches(m_nodalLoads[i].loadCase) && (all || nodeIds.contains(m_nodalLoads[i].nodeId))) {
                m_nodalLoads.removeAt(i);
                ++nodal;
            }
        }
    }
    if (dialog.distributedForces()) {
        for (int i = m_memberLoads.size() - 1; i >= 0; --i) {
            if (caseMatches(m_memberLoads[i].loadCase) && (all || barIds.contains(m_memberLoads[i].memberId))) {
                m_memberLoads.removeAt(i);
                ++distributed;
            }
        }
    }
    if (dialog.distributedMoments()) {
        for (int i = m_extras.memberMoments.size() - 1; i >= 0; --i) {
            if (caseMatches(m_extras.memberMoments[i].loadCase) && (all || barIds.contains(m_extras.memberMoments[i].memberId))) {
                m_extras.memberMoments.removeAt(i);
                ++moments;
            }
        }
    }
    if (dialog.areaPanels()) {
        for (int i = m_extras.areaLoads.size() - 1; i >= 0; --i) {
            const auto &panel = m_extras.areaLoads[i];
            if (!caseMatches(panel.loadCase)) {
                continue;
            }
            bool touches = all;
            for (const int member : panel.members) {
                touches = touches || barIds.contains(member);
            }
            if (!touches) {
                continue;
            }
            // as cargas distribuidas que o pano gerou nas suas barras tambem saem
            for (int k = m_memberLoads.size() - 1; k >= 0; --k) {
                if (m_memberLoads[k].loadCase == panel.loadCase && panel.members.contains(m_memberLoads[k].memberId)) {
                    m_memberLoads.removeAt(k);
                    ++distributed;
                }
            }
            m_extras.areaLoads.removeAt(i);
            ++panels;
        }
    }
    syncLoadVisuals();
    refreshPropertiesPanel();
    showStatusMessage(tr("Cargas excluídas: %1 nodal(is), %2 distribuída(s), %3 momento(s), %4 pano(s).").arg(nodal).arg(distributed).arg(moments).arg(panels), 7000);
}
