#pragma once

#include "app/ProjectDataTypes.h"

#include <QHash>
#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QLabel;

namespace Structura::UI {

/// Painel de grupos de barras: visibilidade, selecao e edicao dos membros.
class GroupsPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit GroupsPanel(QWidget *parent = nullptr);

    /// `hidden`: grupos cujas barras estao todas ocultas.
    void setGroups(const QVector<Structura::App::ProjectData::BarGroup> &groups, const QHash<int, bool> &hidden);
    [[nodiscard]] int currentGroupId() const;

signals:
    void createRequested();
    void addSelectionRequested(int groupId);
    void removeSelectionRequested(int groupId);
    void renameRequested(int groupId);
    void deleteRequested(int groupId);
    void selectRequested(int groupId);
    void visibilityChanged(int groupId, bool hidden);

private:
    QListWidget *m_list {nullptr};
    QLabel *m_empty {nullptr};
    bool m_updating {false};
};

} // namespace Structura::UI
