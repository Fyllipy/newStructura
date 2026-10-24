#include "MainWindow.h"

#include "DesignRunData.h"
#include "IntegrityDialog.h"
#include "SelectionModel.h"
#include "app/services/DesignService.h"
#include "app/services/IntegrityService.h"
#include "viz/SceneController.h"

#include <QMessageBox>
#include <QSet>

using Structura::App::Services::DesignService;
using Structura::App::Services::IntegrityService;

void MainWindow::onStructuralIntegrity()
{
    if (!m_sceneController) {
        return;
    }
    auto run = std::make_shared<DesignRunData>();
    if (!buildDesignRun(*run)) {
        QMessageBox::information(this, tr("Integridade estrutural"), run->error.isEmpty() ? tr("Execute a análise antes de verificar a integridade estrutural.") : run->error);
        return;
    }
    const auto verdicts = DesignService::verifyAll(run->effects, run->refs, run->settings, run->lengthFactor);

    IntegrityService::Input base;
    QSet<int> supported;
    for (const auto &support : m_supports) {
        for (const bool r : support.restraints) {
            if (r) {
                supported.insert(support.nodeId);
                break;
            }
        }
    }
    QHash<QUuid, int> nodeExternal;
    for (const auto &node : m_sceneController->nodeInfos()) {
        base.nodes.append({node.externalId, node.x, node.y, node.z, supported.contains(node.externalId)});
        nodeExternal.insert(node.id, node.externalId);
    }
    QHash<int, double> ntRd, ncRd;
    for (const auto &v : verdicts) {
        for (const auto &check : v.output.checks) {
            if (check.id == "tension") {
                ntRd.insert(v.memberId, check.resistance);
            } else if (check.id == "compression") {
                ncRd.insert(v.memberId, check.resistance);
            }
        }
    }
    for (const auto &bar : m_sceneController->bars()) {
        IntegrityService::Bar b;
        b.id = bar.externalId;
        b.nodeI = nodeExternal.value(bar.startNodeId, -1);
        b.nodeJ = nodeExternal.value(bar.endNodeId, -1);
        const auto section = connectionSectionInfo(bar.sectionId);
        b.name = section.name;
        b.ntRdKn = ntRd.value(bar.externalId, -1.0);
        b.ncRdKn = ncRd.value(bar.externalId, -1.0);
        if (b.ntRdKn < 0.0 && section.area > 0.0) {
            // sem esforco de tracao nas combinacoes: escoamento da secao bruta com o aco da barra
            const auto params = m_extras.design.paramsFor(bar.externalId);
            const auto steel = DesignService::findSteel(params.steel.isEmpty() ? m_extras.design.defaultSteel : params.steel);
            if (steel) {
                b.ntRdKn = section.area * 1e6 * steel->fy / 1.10 / 1000.0;
            }
        }
        base.bars.append(b);
    }
    QStringList combinations;
    for (const auto &effects : run->effects) {
        double minimumN = 0.0;
        for (const auto &combo : effects.combos) {
            const QString name = QString::fromStdString(combo.name);
            if (!combinations.contains(name)) {
                combinations.append(name);
            }
            for (const auto &station : combo.stations) {
                minimumN = std::min(minimumN, station.e.N);
            }
        }
        base.columnLoads.append({effects.memberId, -minimumN});
        for (const auto &end : effects.ends) {
            const QString combo = QString::fromStdString(end.combo);
            auto vertical = [&](int first) { return end.f[static_cast<std::size_t>(first)] * end.x[2] + end.f[static_cast<std::size_t>(first) + 1] * end.y[2] + end.f[static_cast<std::size_t>(first) + 2] * end.z[2]; };
            base.endForces.append({effects.memberId, false, combo, vertical(0), end.f[0]});
            base.endForces.append({effects.memberId, true, combo, vertical(6), end.f[6]});
        }
    }
    base.connections = m_extras.connections;

    Structura::UI::IntegrityDialog dialog(combinations, [base](const IntegrityService::Options &options) {
        IntegrityService::Input input = base;
        input.options = options;
        return IntegrityService::run(input);
    }, this);
    dialog.exec();
}
