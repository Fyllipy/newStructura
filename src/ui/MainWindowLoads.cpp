#include "MainWindow.h"
#include "CombinationGeneratorDialog.h"

#include "app/LoadSystemUtils.h"
#include "DistributedLoadDialog.h"
#include "MemberMomentDialog.h"
#include "LoadCasesDialog.h"
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

void MainWindow::syncAreaPanelVisuals()
{
    if (!m_sceneController) {
        return;
    }
    QHash<int, std::array<Structura::Visualization::AreaPanelRenderer::Vec3, 2>> barEnds;
    QHash<QUuid, Structura::Visualization::AreaPanelRenderer::Vec3> position;
    for (const auto &node : m_sceneController->nodeInfos()) {
        position.insert(node.id, {node.x, node.y, node.z});
    }
    for (const auto &bar : m_sceneController->bars()) {
        const auto a = position.constFind(bar.startNodeId);
        const auto b = position.constFind(bar.endNodeId);
        if (a != position.constEnd() && b != position.constEnd()) {
            barEnds.insert(bar.externalId, {*a, *b});
        }
    }
    std::vector<Structura::Visualization::AreaPanelRenderer::Panel> panels;
    for (const auto &record : m_extras.areaLoads) {
        if (!m_extras.activeLoadCase.isEmpty() && record.loadCase != m_extras.activeLoadCase) {
            continue;
        }
        Structura::Visualization::AreaPanelRenderer::Panel panel;
        for (const auto &p : record.polygon) {
            panel.polygon.push_back(p);
        }
        panel.direction = record.direction;
        panel.pressure = record.pressure;
        panel.wind = record.kind == QLatin1String("WIND");
        for (const int member : record.members) {
            const auto it = barEnds.constFind(member);
            if (it != barEnds.constEnd()) {
                panel.bars.emplace_back((*it)[0], (*it)[1]);
            }
        }
        panels.push_back(std::move(panel));
    }
    m_sceneController->setAreaPanels(panels);
}

void MainWindow::syncLoadVisuals()
{
    if (!m_sceneController) {
        return;
    }
    syncAreaPanelVisuals();

    Structura::Viz::LoadSupportVisualMapper::LoadContext context;
    context.nodalLoads = &m_nodalLoads;
    context.memberLoads = &m_memberLoads;
    context.memberMoments = &m_extras.memberMoments;
    context.materials = &m_materials;
    context.sections = &m_sections;
    context.placements = &m_extras.placements;
    context.loadCases = &m_extras.loadCases;
    context.activeCase = m_extras.activeLoadCase;

    // Elementos esmaecidos/ocultos (isolar nivel, ocultar) nao mostram cargas nem apoios.
    auto nodalVisuals = Structura::Viz::LoadSupportVisualMapper::mapNodalLoadVisuals(m_sceneController, context);
    eraseIf(nodalVisuals, [this](const auto &visual) {
        return m_offNodeKeys.contains(positionKey(visual.position.x(), visual.position.y(), visual.position.z()));
    });
    m_sceneController->setNodalLoadVisuals(nodalVisuals);

    auto memberVisuals = Structura::Viz::LoadSupportVisualMapper::mapMemberActionVisuals(m_sceneController, context);
    eraseIf(memberVisuals, [this](const auto &visual) {
        return m_offBarKeys.contains(positionKey(visual.start.x(), visual.start.y(), visual.start.z()) + QLatin1Char('|')
                                     + positionKey(visual.end.x(), visual.end.y(), visual.end.z()));
    });
    m_sceneController->setMemberActionVisuals(memberVisuals);
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

    auto supportVisuals = Structura::Viz::LoadSupportVisualMapper::mapSupportVisuals(m_sceneController, m_supports);
    eraseIf(supportVisuals, [this](const auto &visual) {
        return m_offNodeKeys.contains(positionKey(visual.position.x(), visual.position.y(), visual.position.z()));
    });
    m_sceneController->setSupportVisuals(supportVisuals);
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
    StateGuard guard(this, tr("Carga nodal"));
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
    dialog.setLoadCases(loadCaseNames(), dialogLoadCase());
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
    const QString chosenCase = dialog.loadCase();
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
                                         [externalId, &chosenCase](const NodalLoad &load) {
                                             return load.nodeId == externalId && load.loadCase == chosenCase;
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
        record.loadCase = chosenCase;
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
    StateGuard guard(this, tr("Carga distribuída"));
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
    dialog.setLoadCases(loadCaseNames(), dialogLoadCase());
    dialog.setSelectedCount(selectedBars.size());
    dialog.setInitialValues(m_lastDistributedPreset.qx,
                            m_lastDistributedPreset.qy,
                            m_lastDistributedPreset.qz,
                            m_lastDistributedPreset.system);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    auto values = dialog.values();
    const QString chosenCase = dialog.loadCase();
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
                                         [externalId, &chosenCase](const MemberLoad &load) {
                                             return load.memberId == externalId && load.loadCase == chosenCase;
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
        record.loadCase = chosenCase;
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

QStringList MainWindow::loadCaseNames() const
{
    QStringList names;
    for (const auto &loadCase : m_extras.loadCases) {
        names << loadCase.name;
    }
    if (names.isEmpty()) {
        names << Structura::App::ProjectData::defaultLoadCaseName();
    }
    return names;
}

QString MainWindow::dialogLoadCase() const
{
    return m_extras.activeLoadCase.isEmpty() ? loadCaseNames().first() : m_extras.activeLoadCase;
}

void MainWindow::onApplyMemberMoment()
{
    StateGuard guard(this, tr("Momento distribuído"));
    if (!m_selectionModel || !m_sceneController) {
        return;
    }

    const QSet<QUuid> selectedBars = m_selectionModel->selectedBars();
    if (selectedBars.isEmpty()) {
        QMessageBox::information(this, tr("Momento distribuido (barras)"),
                                 tr("Selecione ao menos uma barra para aplicar o momento."));
        return;
    }

    MemberMomentDialog dialog(this);
    dialog.setLoadCases(loadCaseNames(), dialogLoadCase());
    dialog.setSelectedCount(selectedBars.size());
    dialog.setInitialValues(0.0, 0.0, 0.0, QStringLiteral("LOCAL"));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const auto values = dialog.values();
    const QString chosenCase = dialog.loadCase();
    const QString system = Structura::App::normalizeMemberLoadSystem(values.system, QStringLiteral("LOCAL"));
    const bool removeMoment = isZeroDistributedLoad(values.mx, values.my, values.mz);

    int affected = 0;
    for (const QUuid &barId : selectedBars) {
        const SceneController::Bar *bar = m_sceneController->findBar(barId);
        if (!bar || bar->externalId() <= 0) {
            continue;
        }
        const int externalId = bar->externalId();
        const bool hadPrevious = eraseIf(m_extras.memberMoments,
                                         [externalId, &chosenCase](const Structura::App::ProjectData::MemberMoment &moment) {
                                             return moment.memberId == externalId && moment.loadCase == chosenCase;
                                         });
        if (removeMoment) {
            affected += hadPrevious ? 1 : 0;
            continue;
        }
        Structura::App::ProjectData::MemberMoment record;
        record.memberId = externalId;
        record.loadCase = chosenCase;
        record.system = system;
        record.mx = values.mx;
        record.my = values.my;
        record.mz = values.mz;
        m_extras.memberMoments.append(record);
        ++affected;
    }

    syncLoadVisuals();
    refreshPropertiesPanel();
    statusBar()->showMessage(removeMoment ? tr("Momento removido de %1 barra(s).").arg(affected)
                                          : tr("Momento aplicado a %1 barra(s).").arg(affected),
                             4000);
}

void MainWindow::onEditLoadCases()
{
    StateGuard guard(this, tr("Casos e combinações"));
    Structura::UI::LoadCasesDialog dialog(m_extras.loadCases, m_extras.combinations, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const auto newCases = dialog.loadCases();
    QSet<QString> names;
    for (const auto &loadCase : newCases) {
        names.insert(loadCase.name);
    }
    const QString fallback = newCases.isEmpty() ? Structura::App::ProjectData::defaultLoadCaseName() : newCases.first().name;

    // Cargas de casos removidos passam para o primeiro caso.
    for (auto &load : m_nodalLoads) {
        if (!names.contains(load.loadCase)) {
            load.loadCase = fallback;
        }
    }
    for (auto &load : m_memberLoads) {
        if (!names.contains(load.loadCase)) {
            load.loadCase = fallback;
        }
    }
    for (auto &moment : m_extras.memberMoments) {
        if (!names.contains(moment.loadCase)) {
            moment.loadCase = fallback;
        }
    }

    m_extras.loadCases = newCases;
    m_extras.combinations = dialog.combinations();
    if (!m_extras.activeLoadCase.isEmpty() && !names.contains(m_extras.activeLoadCase)) {
        m_extras.activeLoadCase.clear();
    }
    rebuildCaseSelector();
    syncLoadVisuals();
    statusBar()->showMessage(tr("Casos e combinacoes atualizados."), 3000);
}

void MainWindow::onApplyRestraints()
{
    StateGuard guard(this, tr("Restrições nodais"));
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

void MainWindow::onGenerateCombinations()
{
    QVector<Structura::App::ProjectData::LoadCase> cases = m_extras.loadCases;
    if (cases.isEmpty()) {
        statusBar()->showMessage(tr("Defina os casos de carga antes de gerar combinações."), 4000);
        return;
    }
    StateGuard guard(this, tr("Gerar combinações"));
    Structura::UI::CombinationGeneratorDialog dialog(cases, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto generated = dialog.combinations();
    if (generated.isEmpty()) {
        return;
    }
    if (dialog.replaceExisting()) {
        m_extras.combinations = generated;
    } else {
        m_extras.combinations += generated;
    }
    statusBar()->showMessage(tr("%1 combinações geradas.").arg(generated.size()), 5000);
}
