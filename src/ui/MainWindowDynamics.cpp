#include "MainWindow.h"

#include "DynamicsDialog.h"
#include "app/services/CombinationService.h"
#include "app/services/DynamicsService.h"
#include "viz/SceneController.h"

#include <QMessageBox>

#include <algorithm>
#include <memory>

void MainWindow::onDynamics()
{
    using Structura::App::Services::CombinationService;
    using Structura::App::Services::DynamicsService;
    if (!m_sceneController || m_sceneController->bars().empty()) {
        QMessageBox::information(this, tr("Dinâmica"), tr("Não há barras no modelo."));
        return;
    }
    QStringList names;
    for (const auto &c : CombinationService::effectiveCombinations(m_extras)) {
        names << c.name;
    }
    auto data = std::make_shared<Structura::UI::DynamicsData>();
    auto *dialog = new Structura::UI::DynamicsDialog(
        names, data,
        [this, data](const QString &source, int modes, QString *error) {
            bool includeMaterialMass = true;
            data->valid = false;
            if (!computeModal(source, modes, data->model, data->modal, &includeMaterialMass, error)) {
                return false;
            }
            if (data->modal.modalOmega.empty()) {
                if (error) {
                    *error = tr("O solver não devolveu modos de vibração (verifique apoios e massas).");
                }
                return false;
            }
            data->mass = DynamicsService::nodalMasses(data->model, includeMaterialMass);
            data->valid = true;
            return true;
        },
        this);
    connect(dialog, &Structura::UI::DynamicsDialog::windCaseRequested, this, [this](const Structura::UI::WindCaseRequest &request) {
        StateGuard guard(this, tr("Vento dinâmico"));
        bool exists = false;
        for (const auto &c : m_extras.loadCases) {
            exists = exists || c.name == request.name;
        }
        if (!exists) {
            Structura::App::ProjectData::LoadCase loadCase;
            loadCase.name = request.name;
            loadCase.abbreviation = request.name.left(3).toUpper();
            m_extras.loadCases.append(loadCase);
            rebuildCaseSelector();
        }
        m_nodalLoads.erase(std::remove_if(m_nodalLoads.begin(), m_nodalLoads.end(), [&](const NodalLoad &l) { return l.loadCase == request.name; }), m_nodalLoads.end());
        for (std::size_t i = 0; i < request.nodeIds.size(); ++i) {
            NodalLoad load;
            load.nodeId = request.nodeIds[i];
            load.loadCase = request.name;
            load.fx = request.forces[i][0];
            load.fy = request.forces[i][1];
            load.fz = request.forces[i][2];
            m_nodalLoads.append(load);
        }
        syncLoadVisuals();
        refreshPropertiesPanel();
        showStatusMessage(tr("Caso \"%1\" criado com %2 força(s) nodal(is).").arg(request.name).arg(request.nodeIds.size()), 8000);
    });
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
