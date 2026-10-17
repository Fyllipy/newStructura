#include "MainWindow.h"

#include "CompareResultsDialog.h"
#include "viz/SceneController.h"

#include <QMessageBox>

void MainWindow::onCompareResults()
{
    if (m_resultViews.isEmpty() || !m_sceneController) {
        QMessageBox::information(this, tr("Comparar combinações"), tr("Rode a análise (F5) antes de comparar resultados."));
        return;
    }
    Structura::UI::CompareData data;
    for (const auto &view : m_resultViews) {
        Structura::UI::CompareData::View v;
        v.name = view.name;
        v.isCombination = view.isCombination;
        for (const auto &nr : view.result.nodeResults) {
            v.displacements.insert(nr.nodeId, nr.displacements);
            v.reactions.insert(nr.nodeId, nr.reactions);
        }
        for (const auto &mr : view.result.memberResults) {
            v.forces.insert(mr.memberId, mr.localForces);
        }
        data.views.append(std::move(v));
    }
    QHash<QUuid, int> nodeExternal;
    for (const auto &node : m_sceneController->nodeInfos()) {
        data.nodeIds.append(node.externalId);
        nodeExternal.insert(node.id, node.externalId);
    }
    QHash<QUuid, int> barExternal;
    for (const auto &bar : m_sceneController->bars()) {
        data.barIds.append(bar.externalId);
        barExternal.insert(bar.id, bar.externalId);
    }
    std::sort(data.nodeIds.begin(), data.nodeIds.end());
    std::sort(data.barIds.begin(), data.barIds.end());
    if (m_selectionModel) {
        for (const QUuid &id : m_selectionModel->selectedNodes()) {
            data.selectedNodes.insert(nodeExternal.value(id));
        }
        for (const QUuid &id : m_selectionModel->selectedBars()) {
            data.selectedBars.insert(barExternal.value(id));
        }
    }
    auto *dialog = new Structura::UI::CompareResultsDialog(std::move(data), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
