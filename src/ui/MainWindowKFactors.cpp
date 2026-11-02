#include "MainWindow.h"

#include "KFactorDialog.h"
#include "SelectionModel.h"
#include "viz/SceneController.h"

#include <QMessageBox>
#include <QSet>

#include <algorithm>

/// Define manualmente os comprimentos de flambagem das barras selecionadas: pelo tipo de vinculacao (K teorico ou recomendado), por um K arbitrario
/// ou pelo comprimento efetivo. Os valores ficam nos parametros de dimensionamento de cada barra e valem para a verificacao dos perfis.
void MainWindow::onBucklingLengths()
{
    if (!m_sceneController || !m_selectionModel) {
        return;
    }
    const QSet<QUuid> selected = m_selectionModel->selectedBars();
    QVector<int> ids;
    for (const auto &bar : m_sceneController->bars()) {
        if (selected.contains(bar.id)) {
            ids.append(bar.externalId);
        }
    }
    std::sort(ids.begin(), ids.end());
    if (ids.isEmpty()) {
        QMessageBox::information(this, tr("Comprimentos de flambagem"), tr("Selecione ao menos uma barra para definir os comprimentos de flambagem."));
        return;
    }

    Structura::UI::KFactorDialog dialog(ids, m_extras.design.paramsFor(ids.first()), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    using Mode = Structura::UI::KAxisSetting::Mode;
    const auto settings = dialog.settings();
    if (settings.x.mode == Mode::Keep && settings.y.mode == Mode::Keep && settings.z.mode == Mode::Keep) {
        return;
    }

    auto apply = [](const Structura::UI::KAxisSetting &setting, double &k, double &length) {
        switch (setting.mode) {
        case Mode::Keep:
            break;
        case Mode::Type:
        case Mode::Coefficient:
            k = setting.value;
            length = 0.0;
            break;
        case Mode::Length:
            k = 1.0;
            length = setting.value;
            break;
        }
    };

    StateGuard guard(this, tr("Comprimentos de flambagem"));
    for (const int id : ids) {
        auto params = m_extras.design.paramsFor(id);
        apply(settings.x, params.kx, params.lx);
        apply(settings.y, params.ky, params.ly);
        apply(settings.z, params.kz, params.lz);
        bool found = false;
        for (auto &existing : m_extras.design.members) {
            if (existing.memberId == id) {
                existing = params;
                found = true;
            }
        }
        if (!found) {
            m_extras.design.members.append(params);
        }
    }
    refreshPropertiesPanel();
    showStatusMessage(tr("Comprimentos de flambagem definidos em %1 barra(s); verifique os perfis para atualizar o dimensionamento.").arg(ids.size()), 8000);
}
