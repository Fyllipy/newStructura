#include "MainWindow.h"

#include "SelectionModel.h"
#include "app/UndoRedoService.h"
#include "viz/SceneController.h"

#include <QMessageBox>
#include <QStatusBar>
#include <QUndoStack>

namespace {
namespace PD = Structura::App::ProjectData;
}

// Rotulas (articulacoes de extremidade) e tipos funcionais de barra (trelica / tirante) nas barras selecionadas.
// op: 0 rotular inicio, 1 rotular fim, 2 bi-rotulada, 3 engastar extremidades, 4 trelica, 5 tirante
void MainWindow::applyBarTypeOperation(int op)
{
    if (!m_sceneController || !m_selectionModel || !m_undoService) {
        return;
    }
    const auto selected = m_selectionModel->selectedBars();
    QVector<QUuid> ids;
    for (const auto &bar : m_sceneController->bars()) {
        if (selected.contains(bar.id) && bar.externalId > 0) {
            ids.append(bar.id);
        }
    }
    if (ids.isEmpty()) {
        QMessageBox::information(this, tr("Tipo de barra"), tr("Selecione uma ou mais barras."));
        return;
    }
    const int startBits = PD::MemberRelease::kStartRy | PD::MemberRelease::kStartRz;
    const int endBits = PD::MemberRelease::kEndRy | PD::MemberRelease::kEndRz;
    const int rotationBits = PD::MemberRelease::kStartRx | PD::MemberRelease::kStartRy | PD::MemberRelease::kStartRz
                             | PD::MemberRelease::kEndRx | PD::MemberRelease::kEndRy | PD::MemberRelease::kEndRz;

    m_undoService->stack()->beginMacro(tr("Tipo de barra"));
    for (const QUuid &id : ids) {
        const auto *bar = m_sceneController->findBar(id);
        if (!bar) {
            continue;
        }
        PD::MemberPlacement current;
        for (const auto &p : m_extras.placements) {
            if (p.memberId == bar->externalId()) {
                current = p;
            }
        }
        int mask = current.releaseMask;
        int type = current.memberType;
        switch (op) {
        case 0: mask |= startBits; break;
        case 1: mask |= endBits; break;
        case 2: mask |= startBits | endBits; type = PD::MemberType::kNormal; break;
        case 3: mask &= ~rotationBits; type = PD::MemberType::kNormal; break;
        case 4: mask |= startBits | endBits; type = PD::MemberType::kTruss; break;
        case 5: mask |= startBits | endBits; type = PD::MemberType::kTie; break;
        default: break;
        }
        PD::MemberPlacementPatch patch;
        patch.releaseMask = mask;
        patch.memberType = type;
        onBarPlacementEdited({id}, patch);
    }
    m_undoService->stack()->endMacro();
    refreshPropertiesPanel();
    static const char *names[] = {"Rotulada no início", "Rotulada no fim", "Bi-rotulada", "Extremidades engastadas", "Treliça (biarticulada)", "Tirante (só tração)"};
    showStatusMessage(tr("%1: %2 barra(s).").arg(QString::fromUtf8(names[std::clamp(op, 0, 5)])).arg(ids.size()), 5000);
}
