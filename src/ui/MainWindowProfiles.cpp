#include "MainWindow.h"
#include <QMessageBox>

#include "DistributedLoadDialog.h"
#include "LoadsPanel.h"
#include "MemberMomentDialog.h"
#include "NodalLoadDialog.h"
#include "ProfileLibrary.h"
#include "app/services/UserLibrary.h"
#include "ProfileManagerDialog.h"
#include "SelectionModel.h"
#include "app/LoadSystemUtils.h"
#include "viz/SceneController.h"

#include <QStatusBar>
#include <QVTKOpenGLNativeWidget.h>

#include <algorithm>

using Structura::UI::LoadsPanel;
using Structura::UI::ProfileLibrary;

namespace {

QString formatValue(const QString &label, double value)
{
    return QStringLiteral("%1=%2").arg(label).arg(value, 0, 'g', 5);
}

QString joinNonZero(const QVector<QPair<QString, double>> &items)
{
    QStringList parts;
    for (const auto &item : items) {
        if (std::abs(item.second) > 1e-12) {
            parts << formatValue(item.first, item.second);
        }
    }
    return parts.isEmpty() ? QStringLiteral("0") : parts.join(QStringLiteral("  "));
}

} // namespace

// ---------------------------------------------------------------- perfis

QUuid MainWindow::resolveSectionId(const QUuid &id)
{
    if (id.isNull()) {
        return id;
    }
    for (const auto &section : m_sections) {
        if (section.uuid == id) {
            return id;
        }
    }
    const auto *entry = ProfileLibrary::find(id);
    if (!entry) {
        return id;
    }
    // Um perfil equivalente ja carregado de um arquivo pode ter outro identificador.
    for (const auto &section : m_sections) {
        if (section.family == entry->family && section.name == entry->name) {
            return section.uuid;
        }
    }
    SectionInfo info = ProfileLibrary::makeSection(*entry);
    info.externalId = nextSectionExternalId();
    m_sections.append(info);
    return info.uuid;
}

void MainWindow::ensureProfileManager()
{
    if (m_profileManager) {
        return;
    }
    m_profileManager = new Structura::UI::ProfileManagerDialog(this);
    m_profileManager->setAttribute(Qt::WA_DeleteOnClose, false);
    connect(m_profileManager, &Structura::UI::ProfileManagerDialog::addToProjectRequested, this,
            [this](const QUuid &id) {
                resolveSectionId(id);
                refreshPropertiesPanel();
                syncProfileManager();
                showStatusMessage(tr("Perfil adicionado ao projeto."), 3000);
            });
    connect(m_profileManager, &Structura::UI::ProfileManagerDialog::assignRequested, this,
            [this](const QUuid &id) {
                assignSectionToSelection(resolveSectionId(id));
                syncProfileManager();
            });
    connect(m_profileManager, &Structura::UI::ProfileManagerDialog::createProfileRequested, this,
            [this](const SectionInfo &profile, bool assign, bool saveToLibrary) {
                StateGuard guard(this, tr("Criar perfil"));
                SectionInfo info = profile;
                info.uuid = QUuid::createUuid();
                info.externalId = nextSectionExternalId();
                // Nome unico no projeto.
                const QString baseName = info.name.isEmpty() ? tr("Seção %1").arg(info.externalId) : info.name;
                QString name = baseName;
                for (int suffix = 2;; ++suffix) {
                    bool taken = false;
                    for (const auto &section : m_sections) {
                        if (section.name.compare(name, Qt::CaseInsensitive) == 0) {
                            taken = true;
                            break;
                        }
                    }
                    if (!taken) {
                        break;
                    }
                    name = QStringLiteral("%1 (%2)").arg(baseName).arg(suffix);
                }
                info.name = name;
                if (saveToLibrary) {
                    if (Structura::App::Services::UserLibrary::saveProfile(info)) {
                        ProfileLibrary::setUserProfiles(Structura::App::Services::UserLibrary::loadProfiles());
                        showStatusMessage(tr("Perfil \"%1\" salvo no banco de perfis do programa.").arg(info.name), 5000);
                    } else {
                        QMessageBox::warning(this, tr("Banco de perfis"), tr("Não foi possível gravar o banco de perfis em %1.").arg(Structura::App::Services::UserLibrary::directory()));
                    }
                }
                m_sections.append(info);
                m_lastSectionId = info.uuid;
                if (assign) {
                    assignSectionToSelection(info.uuid);
                } else {
                    refreshPropertiesPanel();
                    showStatusMessage(tr("Perfil \"%1\" criado.").arg(info.name), 3000);
                }
                syncProfileManager();
                m_profileManager->profileCreated(info.uuid);
            });
}

void MainWindow::syncProfileManager()
{
    if (!m_profileManager) {
        return;
    }
    m_profileManager->setProjectSections(m_sections);
    m_profileManager->setSelectedBarCount(m_selectionModel->selectedBars().size());
}

void MainWindow::assignSectionToSelection(const QUuid &sectionId)
{
    QVector<QUuid> bars;
    for (const QUuid &barId : m_selectionModel->selectedBars()) {
        bars.append(barId);
    }
    if (bars.isEmpty() || sectionId.isNull()) {
        return;
    }
    m_presenter->handleBarSectionEdited(bars, sectionId);
    m_lastSectionId = sectionId;
    refreshPropertiesPanel();
    applyAppearance();
    showStatusMessage(tr("Perfil atribuído a %1 barra(s).").arg(bars.size()), 3000);
}

void MainWindow::onOpenProfileManager()
{
    ensureProfileManager();
    syncProfileManager();
    m_profileManager->showBrowsePage();
    m_profileManager->show();
    m_profileManager->raise();
    m_profileManager->activateWindow();
}

// ---------------------------------------------------------------- painel de cargas

void MainWindow::onShowLoadsPanel()
{
    if (!m_loadsPanel) {
        m_loadsPanel = new LoadsPanel(m_vtkWidget);
        connect(m_loadsPanel, &LoadsPanel::editRequested, this, &MainWindow::onLoadsPanelEdit);
        connect(m_loadsPanel, &LoadsPanel::deleteRequested, this, &MainWindow::onLoadsPanelDelete);
    }
    refreshLoadsPanel();
    m_loadsPanel->reposition();
    m_loadsPanel->show();
    m_loadsPanel->raise();
}

void MainWindow::refreshLoadsPanel()
{
    if (m_profileManager && m_profileManager->isVisible()) {
        m_profileManager->setSelectedBarCount(m_selectionModel->selectedBars().size());
    }
    if (!m_loadsPanel || !m_loadsPanel->isVisible()) {
        return;
    }
    QSet<int> nodeIds;
    for (const QUuid &id : m_selectionModel->selectedNodes()) {
        if (const auto *node = m_sceneController->findNode(id)) {
            nodeIds.insert(node->externalId());
        }
    }
    QSet<int> barIds;
    for (const QUuid &id : m_selectionModel->selectedBars()) {
        if (const auto *bar = m_sceneController->findBar(id)) {
            barIds.insert(bar->externalId());
        }
    }

    QVector<LoadsPanel::Row> rows;
    for (int i = 0; i < m_nodalLoads.size(); ++i) {
        const auto &load = m_nodalLoads[i];
        if (!nodeIds.contains(load.nodeId)) {
            continue;
        }
        rows.append({LoadsPanel::Kind::Nodal, i, tr("No %1").arg(load.nodeId), load.loadCase,
                     joinNonZero({{"Fx", load.fx}, {"Fy", load.fy}, {"Fz", load.fz},
                                  {"Mx", load.mx}, {"My", load.my}, {"Mz", load.mz}})});
    }
    for (int i = 0; i < m_memberLoads.size(); ++i) {
        const auto &load = m_memberLoads[i];
        if (!barIds.contains(load.memberId)) {
            continue;
        }
        rows.append({LoadsPanel::Kind::Distributed, i, tr("Barra %1").arg(load.memberId), load.loadCase,
                     joinNonZero({{"qx", load.qx}, {"qy", load.qy}, {"qz", load.qz}})
                         + QStringLiteral(" (%1)").arg(load.system)});
    }
    for (int i = 0; i < m_extras.memberMoments.size(); ++i) {
        const auto &moment = m_extras.memberMoments[i];
        if (!barIds.contains(moment.memberId)) {
            continue;
        }
        rows.append({LoadsPanel::Kind::Moment, i, tr("Barra %1").arg(moment.memberId), moment.loadCase,
                     joinNonZero({{"mx", moment.mx}, {"my", moment.my}, {"mz", moment.mz}})
                         + QStringLiteral(" (%1)").arg(moment.system)});
    }
    m_loadsPanel->setRows(rows);
    m_loadsPanel->reposition();
}

void MainWindow::onLoadsPanelEdit(int kind, int index)
{
    bool changed = false;
    if (kind == static_cast<int>(LoadsPanel::Kind::Nodal) && index >= 0 && index < m_nodalLoads.size()) {
        NodalLoad &load = m_nodalLoads[index];
        NodalLoadDialog dialog(this);
        dialog.setLoadCases(loadCaseNames(), load.loadCase);
        dialog.setSelectedCount(1);
        dialog.setInitialValues(load.fx, load.fy, load.fz, load.mx, load.my, load.mz);
        if (dialog.exec() == QDialog::Accepted) {
            const auto v = dialog.values();
            load.fx = v.fx; load.fy = v.fy; load.fz = v.fz;
            load.mx = v.mx; load.my = v.my; load.mz = v.mz;
            load.loadCase = dialog.loadCase();
            const int nodeId = load.nodeId;
            const QString loadCase = load.loadCase;
            for (int i = m_nodalLoads.size() - 1; i >= 0; --i) {
                if (i != index && m_nodalLoads[i].nodeId == nodeId && m_nodalLoads[i].loadCase == loadCase) {
                    m_nodalLoads.removeAt(i);
                    if (i < index) { --index; }
                }
            }
            if (isZeroNodalLoad(v.fx, v.fy, v.fz, v.mx, v.my, v.mz)) {
                m_nodalLoads.removeAt(index);
            }
            changed = true;
        }
    } else if (kind == static_cast<int>(LoadsPanel::Kind::Distributed) && index >= 0 && index < m_memberLoads.size()) {
        MemberLoad &load = m_memberLoads[index];
        DistributedLoadDialog dialog(this);
        dialog.setLoadCases(loadCaseNames(), load.loadCase);
        dialog.setSelectedCount(1);
        dialog.setInitialValues(load.qx, load.qy, load.qz, load.system);
        if (dialog.exec() == QDialog::Accepted) {
            const auto v = dialog.values();
            load.qx = v.qx; load.qy = v.qy; load.qz = v.qz;
            load.system = Structura::App::normalizeMemberLoadSystem(v.system, load.system);
            load.loadCase = dialog.loadCase();
            const int memberId = load.memberId;
            const QString loadCase = load.loadCase;
            for (int i = m_memberLoads.size() - 1; i >= 0; --i) {
                if (i != index && m_memberLoads[i].memberId == memberId && m_memberLoads[i].loadCase == loadCase) {
                    m_memberLoads.removeAt(i);
                    if (i < index) { --index; }
                }
            }
            if (isZeroDistributedLoad(v.qx, v.qy, v.qz)) {
                m_memberLoads.removeAt(index);
            }
            changed = true;
        }
    } else if (kind == static_cast<int>(LoadsPanel::Kind::Moment) && index >= 0 && index < m_extras.memberMoments.size()) {
        auto &moment = m_extras.memberMoments[index];
        MemberMomentDialog dialog(this);
        dialog.setLoadCases(loadCaseNames(), moment.loadCase);
        dialog.setSelectedCount(1);
        dialog.setInitialValues(moment.mx, moment.my, moment.mz, moment.system);
        if (dialog.exec() == QDialog::Accepted) {
            const auto v = dialog.values();
            moment.mx = v.mx; moment.my = v.my; moment.mz = v.mz;
            moment.system = Structura::App::normalizeMemberLoadSystem(v.system, moment.system);
            moment.loadCase = dialog.loadCase();
            const int memberId = moment.memberId;
            const QString loadCase = moment.loadCase;
            for (int i = m_extras.memberMoments.size() - 1; i >= 0; --i) {
                if (i != index && m_extras.memberMoments[i].memberId == memberId
                    && m_extras.memberMoments[i].loadCase == loadCase) {
                    m_extras.memberMoments.removeAt(i);
                    if (i < index) { --index; }
                }
            }
            if (isZeroDistributedLoad(v.mx, v.my, v.mz)) {
                m_extras.memberMoments.removeAt(index);
            }
            changed = true;
        }
    }
    if (changed) {
        syncLoadVisuals();
        refreshPropertiesPanel();
        showStatusMessage(tr("Carga atualizada."), 3000);
    }
}

void MainWindow::onLoadsPanelDelete(const QVector<QPair<int, int>> &items)
{
    QVector<int> nodal;
    QVector<int> member;
    QVector<int> moment;
    for (const auto &item : items) {
        if (item.first == static_cast<int>(LoadsPanel::Kind::Nodal)) {
            nodal.append(item.second);
        } else if (item.first == static_cast<int>(LoadsPanel::Kind::Distributed)) {
            member.append(item.second);
        } else {
            moment.append(item.second);
        }
    }
    auto removeIndices = [](auto &vector, QVector<int> indices) {
        std::sort(indices.begin(), indices.end(), std::greater<int>());
        for (int i : indices) {
            if (i >= 0 && i < vector.size()) {
                vector.removeAt(i);
            }
        }
    };
    removeIndices(m_nodalLoads, nodal);
    removeIndices(m_memberLoads, member);
    removeIndices(m_extras.memberMoments, moment);
    syncLoadVisuals();
    refreshPropertiesPanel();
    showStatusMessage(tr("%1 carga(s) excluida(s).").arg(items.size()), 3000);
}
