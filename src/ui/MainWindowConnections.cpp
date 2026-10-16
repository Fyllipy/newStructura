#include "MainWindow.h"

#include "ConnectionDialog.h"
#include "DesignRunData.h"
#include "SelectionModel.h"
#include "viz/SceneController.h"

#include <algorithm>
#include <cmath>

void MainWindow::onCheckConnections()
{
    if (!m_sceneController) {
        return;
    }
    Structura::UI::ConnectionDialog dialog([this](bool atEnd) {
        Structura::UI::EndForceEnvelope env;
        if (!m_selectionModel) {
            return env;
        }
        int memberId = 0;
        const auto selected = m_selectionModel->selectedBars();
        for (const auto &bar : m_sceneController->bars()) {
            if (selected.contains(bar.id)) {
                memberId = bar.externalId;
                break;
            }
        }
        if (memberId == 0) {
            return env;
        }
        DesignRunData run;
        if (!buildDesignRun(run)) {
            return env;
        }
        for (const auto &effects : run.effects) {
            if (effects.memberId != memberId) {
                continue;
            }
            for (const auto &combo : effects.combos) {
                if (combo.stations.empty()) {
                    continue;
                }
                const auto &station = atEnd ? combo.stations.back() : combo.stations.front();
                const auto &e = station.e;
                const double kN = 1.0 / 1000.0, kNm = 1.0 / 1e6;
                env.n = std::max(env.n, e.N * kN);
                env.nc = std::max(env.nc, -e.N * kN);
                env.vy = std::max(env.vy, std::abs(e.Vy) * kN);
                env.vz = std::max(env.vz, std::abs(e.Vz) * kN);
                env.mx = std::max(env.mx, std::abs(e.Mx) * kNm);
                env.my = std::max(env.my, std::abs(e.My) * kNm);
                env.t = std::max(env.t, std::abs(e.T) * kNm);
                env.ok = true;
            }
        }
        env.label = QObject::tr("Barra %1, %2").arg(memberId).arg(atEnd ? QObject::tr("extremidade final") : QObject::tr("extremidade inicial"));
        return env;
    }, this);
    dialog.exec();
}
