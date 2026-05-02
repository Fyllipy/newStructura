#include "MainWindow.h"

#include "app/LoadSystemUtils.h"
#include "DistributedLoadDialog.h"
#include "NodalLoadDialog.h"
#include "RestraintDialog.h"
#include "viz/LoadSupportVisualMapper.h"
#include "viz/SceneController.h"

#include <QMessageBox>
#include <QStatusBar>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

bool isNearZero(double value)
{
    constexpr double eps = 1e-6;
    return std::abs(value) <= eps;
}

template<typename Container, typename Predicate>
bool eraseIf(Container &container, Predicate predicate)
{
    const int beforeCount = container.size();
    container.erase(std::remove_if(container.begin(), container.end(), predicate), container.end());
    return beforeCount != container.size();
}

} // namespace

void MainWindow::syncLoadVisuals()
{
    if (!m_sceneController) {
        return;
    }

    m_sceneController->setNodalLoadVisuals(
        Structura::Viz::LoadSupportVisualMapper::mapNodalLoadVisuals(m_sceneController, m_nodalLoads));

    m_sceneController->setMemberLoadVisuals(
        Structura::Viz::LoadSupportVisualMapper::mapMemberLoadVisuals(m_sceneController, m_memberLoads));
}

bool MainWindow::isZeroNodalLoad(double fx, double fy, double fz, double mx, double my, double mz)
{
    return isNearZero(fx) && isNearZero(fy) && isNearZero(fz)
        && isNearZero(mx) && isNearZero(my) && isNearZero(mz);
}

bool MainWindow::isZeroDistributedLoad(double qx, double qy, double qz)
{
    return isNearZero(qx) && isNearZero(qy) && isNearZero(qz);
}

void MainWindow::syncSupportVisuals()
{
    if (!m_sceneController) {
        return;
    }

    m_sceneController->setSupportVisuals(
        Structura::Viz::LoadSupportVisualMapper::mapSupportVisuals(m_sceneController, m_supports));
}

void MainWindow::updateLoadActionsEnabled()
{
    const bool hasSelection = (m_selectionModel != nullptr);
    const bool hasNodeSelection = hasSelection && !m_selectionModel->selectedNodes().isEmpty();
    const bool hasBarSelection = hasSelection && !m_selectionModel->selectedBars().isEmpty();
    if (m_applyNodalLoadAction) {
        m_applyNodalLoadAction->setEnabled(hasNodeSelection);
    }
    if (m_applyDistributedLoadAction) {
        m_applyDistributedLoadAction->setEnabled(hasBarSelection);
    }
}

void MainWindow::onApplyNodalLoad()
{
    if (!m_selectionModel || !m_sceneController) {
        return;
    }

    const QSet<QUuid> selectedNodes = m_selectionModel->selectedNodes();
    if (selectedNodes.isEmpty()) {
        QMessageBox::information(this, tr("Forca concentrada (nos)"),
                                 tr("Selecione ao menos um no para aplicar a carga."));
        return;
    }

    NodalLoadDialog dialog(this);
    dialog.setSelectedCount(selectedNodes.size());
    dialog.setInitialValues(m_lastNodalPreset.fx,
                            m_lastNodalPreset.fy,
                            m_lastNodalPreset.fz,
                            m_lastNodalPreset.mx,
                            m_lastNodalPreset.my,
                            m_lastNodalPreset.mz);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const auto values = dialog.values();
    const bool removeLoad = isZeroNodalLoad(values.fx, values.fy, values.fz,
                                            values.mx, values.my, values.mz);

    bool changed = false;
    int affected = 0;

    for (const QUuid &nodeId : selectedNodes) {
        const SceneController::Node *node = m_sceneController->findNode(nodeId);
        if (!node) {
            continue;
        }
        const int externalId = node->externalId();
        const bool hadPrevious = eraseIf(m_nodalLoads,
                                         [externalId](const NodalLoad &load) {
                                             return load.nodeId == externalId;
                                         });

        if (removeLoad) {
            if (hadPrevious) {
                changed = true;
                ++affected;
            }
            continue;
        }

        NodalLoad record;
        record.nodeId = externalId;
        record.fx = values.fx;
        record.fy = values.fy;
        record.fz = values.fz;
        record.mx = values.mx;
        record.my = values.my;
        record.mz = values.mz;
        m_nodalLoads.append(record);
        changed = true;
        ++affected;
    }

    m_lastNodalPreset.fx = values.fx;
    m_lastNodalPreset.fy = values.fy;
    m_lastNodalPreset.fz = values.fz;
    m_lastNodalPreset.mx = values.mx;
    m_lastNodalPreset.my = values.my;
    m_lastNodalPreset.mz = values.mz;

    if (changed) {
        syncLoadVisuals();
        refreshPropertiesPanel();
        if (affected > 0) {
            if (removeLoad) {
                statusBar()->showMessage(tr("Carga removida de %1 no(s).").arg(affected), 4000);
            } else {
                statusBar()->showMessage(tr("Carga aplicada a %1 no(s).").arg(affected), 4000);
            }
        }
    } else {
        statusBar()->showMessage(tr("Nenhuma carga alterada."), 3000);
    }

    updateLoadActionsEnabled();
}

void MainWindow::onApplyDistributedLoad()
{
    if (!m_selectionModel || !m_sceneController) {
        return;
    }

    const QSet<QUuid> selectedBars = m_selectionModel->selectedBars();
    if (selectedBars.isEmpty()) {
        QMessageBox::information(this, tr("Carga distribuida (barras)"),
                                 tr("Selecione ao menos uma barra para aplicar a carga."));
        return;
    }

    DistributedLoadDialog dialog(this);
    dialog.setSelectedCount(selectedBars.size());
    dialog.setInitialValues(m_lastDistributedPreset.qx,
                            m_lastDistributedPreset.qy,
                            m_lastDistributedPreset.qz,
                            m_lastDistributedPreset.system);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    auto values = dialog.values();
    const QString system = Structura::App::normalizeMemberLoadSystem(values.system,
                                                                      m_lastDistributedPreset.system);

    const bool removeLoad = isZeroDistributedLoad(values.qx, values.qy, values.qz);

    bool changed = false;
    int affected = 0;

    for (const QUuid &barId : selectedBars) {
        SceneController::Bar *bar = m_sceneController->findBar(barId);
        if (!bar) {
            continue;
        }
        const int externalId = bar->externalId();
        if (externalId <= 0) {
            continue;
        }

        const bool hadPrevious = eraseIf(m_memberLoads,
                                         [externalId](const MemberLoad &load) {
                                             return load.memberId == externalId;
                                         });

        if (removeLoad) {
            if (hadPrevious) {
                changed = true;
                ++affected;
            }
            continue;
        }

        MemberLoad record;
        record.memberId = externalId;
        record.system = system;
        record.qx = values.qx;
        record.qy = values.qy;
        record.qz = values.qz;
        m_memberLoads.append(record);
        changed = true;
        ++affected;
    }

    m_lastDistributedPreset.system = system;
    m_lastDistributedPreset.qx = values.qx;
    m_lastDistributedPreset.qy = values.qy;
    m_lastDistributedPreset.qz = values.qz;

    if (changed) {
        syncLoadVisuals();
        refreshPropertiesPanel();
        if (affected > 0) {
            if (removeLoad) {
                statusBar()->showMessage(tr("Carga distribuida removida de %1 barra(s).").arg(affected), 4000);
            } else {
                statusBar()->showMessage(tr("Carga distribuida aplicada a %1 barra(s).").arg(affected), 4000);
            }
        }
    } else {
        statusBar()->showMessage(tr("Nenhuma carga distribuida alterada."), 3000);
    }

    updateLoadActionsEnabled();
}

void MainWindow::onApplyRestraints()
{
    if (!m_selectionModel || !m_sceneController) {
        return;
    }

    const QSet<QUuid> selectedNodes = m_selectionModel->selectedNodes();
    if (selectedNodes.isEmpty()) {
        QMessageBox::information(this, tr("Restricoes nodais"),
                                 tr("Selecione ao menos um no para aplicar restricoes."));
        return;
    }

    RestraintDialog dialog(this);

    bool firstNode = true;
    std::array<bool, 6> commonRestraints = {false, false, false, false, false, false};
    bool hasMixedValues = false;

    for (const QUuid &nodeId : selectedNodes) {
        const SceneController::Node *node = m_sceneController->findNode(nodeId);
        if (!node) {
            continue;
        }

        std::array<bool, 6> nodeRestraints = node->restraints();
        if (firstNode) {
            commonRestraints = nodeRestraints;
            firstNode = false;
        } else {
            for (int i = 0; i < 6; ++i) {
                if (commonRestraints[i] != nodeRestraints[i]) {
                    hasMixedValues = true;
                    break;
                }
            }
        }
        if (hasMixedValues) {
            break;
        }
    }

    if (!hasMixedValues) {
        dialog.setRestraints(commonRestraints);
    } else {
        dialog.setMixedState(true);
    }

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const std::array<bool, 6> newRestraints = dialog.restraints();

    int affected = 0;
    for (const QUuid &nodeId : selectedNodes) {
        SceneController::Node *node = m_sceneController->findNode(nodeId);
        if (!node) {
            continue;
        }

        for (int i = 0; i < 6; ++i) {
            node->setRestraint(i, newRestraints[i]);
        }

        const int externalId = node->externalId();
        eraseIf(m_supports,
                [externalId](const NodeSupport &sup) {
                    return sup.nodeId == externalId;
                });

        bool hasAnyRestraint = false;
        for (bool r : newRestraints) {
            if (r) {
                hasAnyRestraint = true;
                break;
            }
        }

        if (hasAnyRestraint) {
            NodeSupport support;
            support.nodeId = externalId;
            for (int i = 0; i < 6; ++i) {
                support.restraints[i] = newRestraints[i];
            }
            m_supports.append(support);
        }

        ++affected;
    }

    syncSupportVisuals();
    refreshPropertiesPanel();

    if (affected > 0) {
        statusBar()->showMessage(tr("Restricoes aplicadas a %1 no(s).").arg(affected), 4000);
    }
}
