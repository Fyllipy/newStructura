#include "MainWindow.h"

#include "GroupsPanel.h"
#include "SelectionModel.h"
#include "ui/MainWindowPresenter.h"
#include "viz/SceneController.h"

#include <QDockWidget>
#include <QInputDialog>
#include <QMessageBox>
#include <QStatusBar>

#include <algorithm>

using Structura::App::ProjectData::BarGroup;

namespace {

int groupIndexOfMember(const QVector<BarGroup> &groups, int member)
{
    for (int i = 0; i < groups.size(); ++i) {
        if (groups[i].members.contains(member)) {
            return i;
        }
    }
    return -1;
}

int nextGroupId(const QVector<BarGroup> &groups)
{
    int id = 1;
    for (const auto &g : groups) {
        id = std::max(id, g.id + 1);
    }
    return id;
}

void dropEmptyGroups(QVector<BarGroup> &groups)
{
    groups.erase(std::remove_if(groups.begin(), groups.end(), [](const BarGroup &g) { return g.members.isEmpty(); }), groups.end());
}

} // namespace

QVector<QUuid> MainWindow::expandWithGroups(const QVector<QUuid> &ids) const
{
    if (m_extras.groups.isEmpty() || !m_sceneController) {
        return ids;
    }
    QVector<QUuid> result = ids;
    QSet<QUuid> present(ids.begin(), ids.end());
    QSet<int> wantedGroups;
    for (const QUuid &id : ids) {
        if (const auto *bar = m_sceneController->findBar(id)) {
            const int index = groupIndexOfMember(m_extras.groups, bar->externalId());
            if (index >= 0) {
                wantedGroups.insert(m_extras.groups[index].id);
            }
        }
    }
    if (wantedGroups.isEmpty()) {
        return ids;
    }
    for (const auto &bar : m_sceneController->bars()) {
        const int index = groupIndexOfMember(m_extras.groups, bar.externalId);
        if (index >= 0 && wantedGroups.contains(m_extras.groups[index].id) && !present.contains(bar.id)) {
            result.append(bar.id);
            present.insert(bar.id);
        }
    }
    return result;
}

void MainWindow::refreshGroupsPanel()
{
    if (!m_groupsPanel) {
        return;
    }
    QHash<int, bool> hidden;
    const auto bars = m_sceneController->bars();
    for (const auto &group : m_extras.groups) {
        bool all = !group.members.isEmpty();
        for (const auto &bar : bars) {
            if (group.members.contains(bar.externalId) && !m_hiddenBars.contains(bar.id)) {
                all = false;
                break;
            }
        }
        hidden.insert(group.id, all);
    }
    m_groupsPanel->setGroups(m_extras.groups, hidden);
}

void MainWindow::setGroupHidden(int groupId, bool hidden)
{
    for (const auto &group : m_extras.groups) {
        if (group.id != groupId) {
            continue;
        }
        for (const auto &bar : m_sceneController->bars()) {
            if (group.members.contains(bar.externalId)) {
                if (hidden) {
                    m_hiddenBars.insert(bar.id);
                } else {
                    m_hiddenBars.remove(bar.id);
                }
            }
        }
    }
    applyAppearance();
    syncBarTypeMarkers();
}

void MainWindow::onToggleGroupsPanel()
{
    if (!m_groupsPanel) {
        m_groupsPanel = new Structura::UI::GroupsPanel(this);
        m_groupsDock = new QDockWidget(tr("Grupos"), this);
        m_groupsDock->setObjectName(QStringLiteral("groupsDock"));
        m_groupsDock->setWidget(m_groupsPanel);
        m_groupsDock->setAllowedAreas(Qt::NoDockWidgetArea);   // janela flutuante
        m_groupsDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable);
        m_groupsDock->setFloating(true);
        m_groupsDock->resize(340, 460);
        m_groupsDock->move(mapToGlobal(QPoint(40, 220)));

        using Panel = Structura::UI::GroupsPanel;
        connect(m_groupsPanel, &Panel::createRequested, this, &MainWindow::onCreateGroup);
        connect(m_groupsPanel, &Panel::visibilityChanged, this, [this](int id, bool hidden) { setGroupHidden(id, hidden); });
        connect(m_groupsPanel, &Panel::selectRequested, this, [this](int id) {
            QVector<QUuid> ids;
            for (const auto &group : m_extras.groups) {
                if (group.id != id) {
                    continue;
                }
                for (const auto &bar : m_sceneController->bars()) {
                    if (group.members.contains(bar.externalId)) {
                        ids.append(bar.id);
                    }
                }
            }
            m_selectionModel->selectBars(ids);
        });
        connect(m_groupsPanel, &Panel::deleteRequested, this, [this](int id) {
            StateGuard guard(this, tr("Excluir grupo"));
            setGroupHidden(id, false);
            for (int i = 0; i < m_extras.groups.size(); ++i) {
                if (m_extras.groups[i].id == id) {
                    m_extras.groups.removeAt(i);
                    break;
                }
            }
            refreshGroupsPanel();
        });
        connect(m_groupsPanel, &Panel::renameRequested, this, [this](int id) {
            StateGuard guard(this, tr("Renomear grupo"));
            for (auto &group : m_extras.groups) {
                if (group.id != id) {
                    continue;
                }
                bool ok = false;
                const QString name = QInputDialog::getText(this, tr("Renomear grupo"), tr("Nome do grupo:"), QLineEdit::Normal,
                                                           group.name, &ok);
                if (ok) {
                    group.name = name.trimmed();
                }
            }
            refreshGroupsPanel();
        });
        auto selectedExternalIds = [this]() {
            QVector<int> ids;
            const auto selected = m_selectionModel->selectedBars();
            for (const auto &bar : m_sceneController->bars()) {
                if (selected.contains(bar.id)) {
                    ids.append(bar.externalId);
                }
            }
            return ids;
        };
        connect(m_groupsPanel, &Panel::removeSelectionRequested, this, [this, selectedExternalIds](int id) {
            StateGuard guard(this, tr("Remover barras do grupo"));
            const QVector<int> remove = selectedExternalIds();
            for (auto &group : m_extras.groups) {
                if (group.id == id) {
                    for (const int member : remove) {
                        group.members.removeAll(member);
                    }
                }
            }
            dropEmptyGroups(m_extras.groups);
            refreshGroupsPanel();
        });
        connect(m_groupsPanel, &Panel::addSelectionRequested, this, [this, selectedExternalIds](int id) {
            StateGuard guard(this, tr("Adicionar barras ao grupo"));
            const QVector<int> add = selectedExternalIds();
            if (add.isEmpty()) {
                showStatusMessage(tr("Selecione as barras que serão adicionadas ao grupo."), 4000);
                return;
            }
            int target = -1;
            for (int i = 0; i < m_extras.groups.size(); ++i) {
                if (m_extras.groups[i].id == id) {
                    target = i;
                }
            }
            if (target < 0) {
                return;
            }
            int reference = 0;   // primeiro membro que nao esta sendo adicionado
            for (const int member : m_extras.groups[target].members) {
                if (!add.contains(member)) {
                    reference = member;
                    break;
                }
            }
            for (auto &group : m_extras.groups) {
                for (const int member : add) {
                    if (group.id != id) {
                        group.members.removeAll(member);
                    }
                }
            }
            for (auto &group : m_extras.groups) {
                if (group.id == id) {
                    for (const int member : add) {
                        if (!group.members.contains(member)) {
                            group.members.append(member);
                        }
                    }
                }
            }
            dropEmptyGroups(m_extras.groups);
            adoptGroupProperties(reference, add);
            refreshGroupsPanel();
        });
    }
    if (m_groupsDock->isVisible()) {
        m_groupsDock->hide();
    } else {
        refreshGroupsPanel();
        m_groupsDock->show();
        m_groupsDock->raise();
    }
}

// Novos membros assumem perfil, posicionamento e rotacao do membro de referencia do grupo.
void MainWindow::adoptGroupProperties(int referenceMember, const QVector<int> &newMembers)
{
    if (referenceMember <= 0) {
        return;
    }
    QUuid sectionId;
    QVector<QUuid> targets;
    for (const auto &bar : m_sceneController->bars()) {
        if (bar.externalId == referenceMember) {
            sectionId = bar.sectionId;
        }
        if (newMembers.contains(bar.externalId)) {
            targets.append(bar.id);
        }
    }
    if (!sectionId.isNull() && !targets.isEmpty()) {
        m_presenter->handleBarSectionEdited(targets, sectionId);
    }
    using Placement = Structura::App::ProjectData::MemberPlacement;
    Placement reference;
    reference.memberId = referenceMember;
    for (const auto &p : m_extras.placements) {
        if (p.memberId == referenceMember) {
            reference = p;
        }
    }
    for (const int member : newMembers) {
        for (int i = 0; i < m_extras.placements.size(); ++i) {
            if (m_extras.placements[i].memberId == member) {
                m_extras.placements.removeAt(i);
                break;
            }
        }
        Placement copy = reference;
        copy.memberId = member;
        if (!copy.isDefault()) {
            m_extras.placements.append(copy);
        }
    }
    syncPlacementVisuals();
    refreshPropertiesPanel();
}

void MainWindow::onCreateGroup()
{
    StateGuard guard(this, tr("Criar grupo"));
    if (!m_sceneController || !m_selectionModel) {
        return;
    }
    QVector<int> members;
    const auto selected = m_selectionModel->selectedBars();
    for (const auto &bar : m_sceneController->bars()) {
        if (selected.contains(bar.id) && bar.externalId > 0) {
            members.append(bar.externalId);
        }
    }
    if (members.isEmpty()) {
        QMessageBox::information(this, tr("Criar grupo"), tr("Selecione as barras que farão parte do grupo."));
        return;
    }
    const int suggested = nextGroupId(m_extras.groups);
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Criar grupo"),
                                               tr("Nome do grupo (deixe em branco para usar o número %1):").arg(suggested),
                                               QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        return;
    }
    // cada barra pertence a no maximo um grupo
    for (auto &group : m_extras.groups) {
        for (const int member : members) {
            group.members.removeAll(member);
        }
    }
    dropEmptyGroups(m_extras.groups);
    BarGroup group;
    group.id = nextGroupId(m_extras.groups);
    group.name = name.trimmed();
    group.members = members;
    m_extras.groups.append(group);
    showStatusMessage(tr("Grupo \"%1\" criado com %2 barra(s). Perfil, posição e rotação passam a ser compartilhados.")
                          .arg(group.name.isEmpty() ? QString::number(group.id) : group.name)
                          .arg(members.size()),
                      6000);
    if (!m_groupsPanel || !m_groupsDock || !m_groupsDock->isVisible()) {
        onToggleGroupsPanel();
    } else {
        refreshGroupsPanel();
    }
}
