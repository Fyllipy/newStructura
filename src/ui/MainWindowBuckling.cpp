#include "MainWindow.h"

#include "BucklingDialog.h"
#include "BusyProgress.h"
#include "app/services/AnalysisService.h"
#include "app/services/BucklingService.h"
#include "app/services/CombinationService.h"
#include "viz/SceneController.h"

#include <QMessageBox>
#include <QStatusBar>

#include <algorithm>
#include <memory>

void MainWindow::onBucklingAnalysis()
{
    using Structura::App::Services::AnalysisService;
    using Structura::App::Services::BucklingService;
    using Structura::App::Services::CombinationService;
    if (!m_sceneController || m_sceneController->bars().empty()) {
        QMessageBox::information(this, tr("Flambagem elástica"), tr("Não há barras no modelo."));
        return;
    }
    const auto combinations = CombinationService::effectiveCombinations(m_extras);
    QStringList names;
    for (const auto &c : combinations) {
        names << c.name;
    }

    auto *dialog = new Structura::UI::BucklingDialog(names, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    struct Shared {
        Structura::App::SolverBridge::ModelData model;
        Structura::App::SolverBridge::Result result;
        double extent {1.0};
    };
    auto shared = std::make_shared<Shared>();

    connect(dialog, &Structura::UI::BucklingDialog::computeRequested, this, [this, dialog, shared, combinations](const QString &name, int modes) {
        AnalysisService analysis;
        const auto cases = CombinationService::effectiveCases(m_extras);
        QMap<QString, Structura::App::SolverBridge::ModelData> caseModels;
        for (const auto &loadCase : cases) {
            auto build = analysis.buildModel(m_sceneController, m_materials, m_sections, m_supports, m_nodalLoads, m_memberLoads, m_extras, loadCase.name);
            if (!build.success) {
                dialog->setError(build.errors.join(QLatin1Char('\n')));
                return;
            }
            caseModels.insert(loadCase.name, build.model);
        }
        std::vector<std::pair<const Structura::App::SolverBridge::ModelData *, double>> terms;
        for (const auto &combination : combinations) {
            if (combination.name != name) {
                continue;
            }
            for (const auto &term : combination.factors) {
                const auto it = caseModels.constFind(term.first);
                if (it != caseModels.constEnd()) {
                    terms.emplace_back(&it.value(), term.second);
                }
            }
        }
        if (terms.empty()) {
            dialog->setError(tr("A combinação não tem casos com carga."));
            return;
        }
        try {
            shared->model = CombinationService::mergeModels(terms);
            shared->result = Structura::UI::runBusy(this, tr("Flambagem elástica"), tr("Extraindo %1 modos de flambagem…").arg(modes),
                                                    [&]() { return BucklingService::run(shared->model, modes); });
        } catch (const std::exception &error) {
            dialog->setError(QString::fromStdString(error.what()));
            return;
        }
        double minX = 1e300, maxX = -1e300, minY = 1e300, maxY = -1e300, minZ = 1e300, maxZ = -1e300;
        for (const auto &n : shared->model.nodes) {
            minX = std::min(minX, n.x); maxX = std::max(maxX, n.x);
            minY = std::min(minY, n.y); maxY = std::max(maxY, n.y);
            minZ = std::min(minZ, n.z); maxZ = std::max(maxZ, n.z);
        }
        shared->extent = std::max({maxX - minX, maxY - minY, maxZ - minZ, 1e-6});
        QVector<double> factors;
        for (const double f : shared->result.bucklingFactors) {
            factors.append(f);
        }
        m_extras.results.bucklingCombination = name;
        m_extras.results.bucklingFactors.clear();
        for (const double f : factors) {
            m_extras.results.bucklingFactors.append(f);
        }
        dialog->setResults(factors);
    });
    connect(dialog, &Structura::UI::BucklingDialog::modeChanged, this, [this, shared](int mode, double fraction) {
        if (mode < 0 || mode >= static_cast<int>(shared->result.bucklingModes.size())) {
            return;
        }
        m_sceneController->setModeShape(BucklingService::shapeLines(shared->model, shared->result.bucklingModes[static_cast<std::size_t>(mode)],
                                                                    fraction * shared->extent));
    });
    connect(dialog, &Structura::UI::BucklingDialog::closedByUser, this, [this]() {
        if (m_sceneController) {
            m_sceneController->setModeShape({});
        }
    });
    dialog->show();
}
