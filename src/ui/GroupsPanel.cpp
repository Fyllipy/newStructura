#include "GroupsPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace Structura::UI {

namespace {
constexpr int kIdRole = Qt::UserRole + 1;
}

GroupsPanel::GroupsPanel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(260);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto *title = new QLabel(tr("Grupos de barras"));
    title->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 700; color: #1e232b;"));
    root->addWidget(title);
    auto *hint = new QLabel(tr("Barras de um grupo compartilham perfil, posição e rotação. Marque para exibir; desmarque para ocultar."));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #5a6472;"));
    root->addWidget(hint);

    auto *create = new QPushButton(tr("Criar grupo com a seleção"));
    create->setStyleSheet(QStringLiteral("QPushButton { background: #0d7fb9; color: white; font-weight: 600; padding: 5px 12px; border-radius: 4px; }"
                                         "QPushButton:hover { background: #0b6fa3; }"));
    root->addWidget(create);

    m_list = new QListWidget;
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    root->addWidget(m_list, 1);
    m_empty = new QLabel(tr("Nenhum grupo criado."));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setStyleSheet(QStringLiteral("color: #8a8f98;"));
    root->addWidget(m_empty);

    auto *row1 = new QHBoxLayout;
    auto *add = new QPushButton(tr("Adicionar seleção"));
    auto *remove = new QPushButton(tr("Remover seleção"));
    row1->addWidget(add);
    row1->addWidget(remove);
    root->addLayout(row1);
    auto *row2 = new QHBoxLayout;
    auto *select = new QPushButton(tr("Selecionar barras"));
    auto *rename = new QPushButton(tr("Renomear"));
    auto *del = new QPushButton(tr("Excluir"));
    row2->addWidget(select);
    row2->addWidget(rename);
    row2->addWidget(del);
    root->addLayout(row2);

    connect(create, &QPushButton::clicked, this, &GroupsPanel::createRequested);
    connect(add, &QPushButton::clicked, this, [this] { if (currentGroupId() > 0) emit addSelectionRequested(currentGroupId()); });
    connect(remove, &QPushButton::clicked, this, [this] { if (currentGroupId() > 0) emit removeSelectionRequested(currentGroupId()); });
    connect(select, &QPushButton::clicked, this, [this] { if (currentGroupId() > 0) emit selectRequested(currentGroupId()); });
    connect(rename, &QPushButton::clicked, this, [this] { if (currentGroupId() > 0) emit renameRequested(currentGroupId()); });
    connect(del, &QPushButton::clicked, this, [this] { if (currentGroupId() > 0) emit deleteRequested(currentGroupId()); });
    connect(m_list, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        if (!m_updating) {
            emit visibilityChanged(item->data(kIdRole).toInt(), item->checkState() != Qt::Checked);
        }
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) { emit selectRequested(item->data(kIdRole).toInt()); });
}

int GroupsPanel::currentGroupId() const
{
    const auto *item = m_list->currentItem();
    return item ? item->data(kIdRole).toInt() : 0;
}

void GroupsPanel::setGroups(const QVector<Structura::App::ProjectData::BarGroup> &groups, const QHash<int, bool> &hidden)
{
    const int previous = currentGroupId();
    m_updating = true;
    m_list->clear();
    for (const auto &group : groups) {
        const QString name = group.name.isEmpty() ? QString::number(group.id) : group.name;
        auto *item = new QListWidgetItem(tr("%1  (%2 barras)").arg(name).arg(group.members.size()), m_list);
        item->setData(kIdRole, group.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(hidden.value(group.id, false) ? Qt::Unchecked : Qt::Checked);
        if (group.id == previous) {
            m_list->setCurrentItem(item);
        }
    }
    m_empty->setVisible(groups.isEmpty());
    m_updating = false;
}

} // namespace Structura::UI
