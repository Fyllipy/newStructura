#include "MainWindow.h"

#include "BeamResultsBuilder.h"
#include "CompareResultsDialog.h"
#include "viz/postprocessor/UniformLoadForceEvaluator.h"
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
        // esforcos ao longo de toda a barra (nao so nas extremidades): cargas do caso/combinacao + esforcos de extremidade, no mesmo avaliador dos diagramas
        std::vector<std::pair<const Structura::App::SolverBridge::ModelData *, double>> terms;
        for (const auto &term : view.terms) {
            const auto runIt = m_caseRuns.constFind(term.first);
            if (runIt != m_caseRuns.constEnd()) {
                terms.emplace_back(&runIt->model, term.second);
            }
        }
        if (!terms.empty()) {
            const auto elements = Structura::UI::buildBeamResults(*terms.front().first, view.result, terms);
            for (const auto &element : elements) {
                const Structura::Visualization::UniformLoadForceEvaluator evaluator(element);
                const double length = evaluator.length();
                constexpr int kSamples = 201;   // esforco de grau <= 3 (cargas uniformes/pontuais): amostragem fina captura o extremo
                Structura::UI::CompareData::View::BarForces forces;
                auto toArray = [](const Structura::Visualization::InternalForces &f) {
                    return std::array<double, 6> {f.N, f.Vy, f.Vz, f.Mt, f.My, f.Mz};
                };
                for (int i = 0; i < kSamples; ++i) {
                    const auto values = toArray(evaluator.evaluate(length * i / (kSamples - 1)));
                    if (i == 0) {
                        forces.atI = values;
                        forces.maxAlong = values;
                        forces.minAlong = values;
                    }
                    if (i == kSamples - 1) {
                        forces.atJ = values;
                    }
                    for (std::size_t k = 0; k < 6; ++k) {
                        forces.maxAlong[k] = std::max(forces.maxAlong[k], values[k]);
                        forces.minAlong[k] = std::min(forces.minAlong[k], values[k]);
                    }
                }
                v.forces.insert(element.externalId, forces);
            }
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
