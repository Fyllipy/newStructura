#include "MainWindow.h"

#include "BusyProgress.h"
#include "ModalDialog.h"
#include "app/services/AnalysisService.h"
#include "app/services/BucklingService.h"
#include "app/services/CombinationService.h"
#include "app/services/ModalService.h"
#include "viz/SceneController.h"

#include <QMap>
#include <QMessageBox>

#include <algorithm>
#include <memory>

bool MainWindow::computeModal(const QString &source, int modes, Structura::App::SolverBridge::ModelData &model,
                              Structura::App::SolverBridge::Result &result, bool *includeMaterialMassOut, QString *error)
{
    using Structura::App::Services::AnalysisService;
    using Structura::App::Services::CombinationService;
    using Structura::App::Services::ModalService;
    const auto combinations = CombinationService::effectiveCombinations(m_extras);
    AnalysisService analysis;
    const auto cases = CombinationService::effectiveCases(m_extras);
    QMap<QString, Structura::App::SolverBridge::ModelData> caseModels;
    for (const auto &loadCase : cases) {
        auto build = analysis.buildModel(m_sceneController, m_materials, m_sections, m_supports, m_nodalLoads, m_memberLoads, m_extras, loadCase.name);
        if (!build.success) {
            if (error) {
                *error = build.errors.join(QLatin1Char('\n'));
            }
            return false;
        }
        caseModels.insert(loadCase.name, build.model);
    }
    bool includeMaterialMass = true;
    std::vector<std::pair<const Structura::App::SolverBridge::ModelData *, double>> terms;
    if (!source.isEmpty()) {
        for (const auto &combination : combinations) {
            if (combination.name != source) {
                continue;
            }
            for (const auto &term : combination.factors) {
                const auto it = caseModels.constFind(term.first);
                if (it == caseModels.constEnd()) {
                    continue;
                }
                terms.emplace_back(&it.value(), term.second);
                for (const auto &c : cases) {
                    if (c.name == term.first && c.selfWeight && term.second != 0.0) {
                        includeMaterialMass = false;   // o peso proprio ja entra como carga
                    }
                }
            }
        }
    }
    try {
        if (terms.empty()) {
            model = caseModels.isEmpty() ? Structura::App::SolverBridge::ModelData {} : caseModels.begin().value();
            model.nodalLoads.clear();
            model.memberLoads.clear();
            model.memberMoments.clear();
        } else {
            model = CombinationService::mergeModels(terms);
        }
        result = Structura::UI::runBusy(this, tr("Análise modal"), tr("Extraindo %1 modos de vibração…").arg(modes),
                                        [&]() { return ModalService::run(model, modes, includeMaterialMass); });
    } catch (const std::exception &e) {
        if (error) {
            *error = friendlySolverMessage(QString::fromUtf8(e.what()));
        }
        return false;
    }
    if (includeMaterialMassOut) {
        *includeMaterialMassOut = includeMaterialMass;
    }
    return true;
}

void MainWindow::onModalAnalysis()
{
    using Structura::App::Services::AnalysisService;
    using Structura::App::Services::BucklingService;
    using Structura::App::Services::CombinationService;
    using Structura::App::Services::ModalService;
    if (!m_sceneController || m_sceneController->bars().empty()) {
        QMessageBox::information(this, tr("Análise modal"), tr("Não há barras no modelo."));
        return;
    }
    const auto combinations = CombinationService::effectiveCombinations(m_extras);
    QStringList names;
    for (const auto &c : combinations) {
        names << c.name;
    }

    auto *dialog = new Structura::UI::ModalDialog(names, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    struct Shared {
        Structura::App::SolverBridge::ModelData model;
        Structura::App::SolverBridge::Result result;
        double extent {1.0};
    };
    auto shared = std::make_shared<Shared>();

    connect(dialog, &Structura::UI::ModalDialog::computeRequested, this, [this, dialog, shared](const QString &source, int modes) {
        QString error;
        bool includeMaterialMass = true;
        if (!computeModal(source, modes, shared->model, shared->result, &includeMaterialMass, &error)) {
            dialog->setError(error);
            return;
        }
        double minX = 1e300, maxX = -1e300, minY = 1e300, maxY = -1e300, minZ = 1e300, maxZ = -1e300;
        for (const auto &n : shared->model.nodes) {
            minX = std::min(minX, n.x); maxX = std::max(maxX, n.x);
            minY = std::min(minY, n.y); maxY = std::max(maxY, n.y);
            minZ = std::min(minZ, n.z); maxZ = std::max(maxZ, n.z);
        }
        shared->extent = std::max({maxX - minX, maxY - minY, maxZ - minZ, 1e-6});
        QVector<double> omega;
        QVector<std::array<double, 3>> ratios;
        for (std::size_t i = 0; i < shared->result.modalOmega.size(); ++i) {
            omega.append(shared->result.modalOmega[i]);
            ratios.append(shared->result.modalMassRatio[i]);
        }
        m_extras.results.modalSource = source;
        m_extras.results.modalOmega = omega;
        m_extras.results.modalMassRatio = ratios;
        dialog->setResults(omega, ratios);
    });
    connect(dialog, &Structura::UI::ModalDialog::modeChanged, this, [this, shared](int mode, double fraction) {
        if (mode < 0 || mode >= static_cast<int>(shared->result.modalShapes.size())) {
            return;
        }
        m_sceneController->setModeShape(BucklingService::shapeLines(shared->model, shared->result.modalShapes[static_cast<std::size_t>(mode)],
                                                                    fraction * shared->extent));
    });
    connect(dialog, &Structura::UI::ModalDialog::closedByUser, this, [this]() {
        if (m_sceneController) {
            m_sceneController->setModeShape({});
        }
    });
    dialog->show();
}
