#include "LoadsPanel.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPair>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace Structura::UI {

namespace {
constexpr int kKindRole = Qt::UserRole;
constexpr int kIndexRole = Qt::UserRole + 1;
} // namespace

LoadsPanel::LoadsPanel(QWidget *parent)
    : QFrame(parent)
    , m_tree(new QTreeWidget(this))
    , m_summary(new QLabel(this))
    , m_edit(new QPushButton(tr("Editar"), this))
    , m_delete(new QPushButton(tr("Excluir"), this))
{
    setObjectName(QStringLiteral("loadsPanel"));
    setStyleSheet(QStringLiteral(
        "QFrame#loadsPanel { background: rgba(255, 255, 255, 245); border: 1px solid #cbd4e2; border-radius: 8px; }"
        "QLabel { color: #1e232b; background: transparent; }"));
    setFixedWidth(430);

    auto *title = new QLabel(tr("Cargas da selecao"), this);
    title->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 13px; color: #0d7fb9;"));
    auto *close = new QPushButton(QStringLiteral("x"), this);
    close->setFixedSize(22, 22);
    close->setFlat(true);
    close->setToolTip(tr("Fechar"));

    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({tr("Elemento"), tr("Tipo"), tr("Caso"), tr("Valores")});
    m_tree->setRootIsDecorated(false);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setUniformRowHeights(true);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setFixedHeight(220);
    m_tree->setColumnWidth(0, 80);
    m_tree->setColumnWidth(1, 90);
    m_tree->setColumnWidth(2, 70);

    m_summary->setStyleSheet(QStringLiteral("color: #6e7781; font-size: 11px;"));

    auto *header = new QHBoxLayout();
    header->addWidget(title, 1);
    header->addWidget(close);
    auto *buttons = new QHBoxLayout();
    buttons->addWidget(m_summary, 1);
    buttons->addWidget(m_edit);
    buttons->addWidget(m_delete);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 10);
    layout->setSpacing(6);
    layout->addLayout(header);
    layout->addWidget(m_tree);
    layout->addLayout(buttons);

    connect(close, &QPushButton::clicked, this, &QWidget::hide);
    connect(m_edit, &QPushButton::clicked, this, [this]() {
        const auto items = m_tree->selectedItems();
        if (!items.isEmpty()) {
            emit editRequested(items.first()->data(0, kKindRole).toInt(), items.first()->data(0, kIndexRole).toInt());
        }
    });
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
        emit editRequested(item->data(0, kKindRole).toInt(), item->data(0, kIndexRole).toInt());
    });
    connect(m_delete, &QPushButton::clicked, this, [this]() {
        QVector<QPair<int, int>> list;
        for (const auto *item : m_tree->selectedItems()) {
            list.append({item->data(0, kKindRole).toInt(), item->data(0, kIndexRole).toInt()});
        }
        if (!list.isEmpty()) {
            emit deleteRequested(list);
        }
    });
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        const int count = m_tree->selectedItems().size();
        m_edit->setEnabled(count == 1);
        m_delete->setEnabled(count > 0);
    });
    m_edit->setEnabled(false);
    m_delete->setEnabled(false);
}

void LoadsPanel::setRows(const QVector<Row> &rows)
{
    m_tree->clear();
    for (const Row &row : rows) {
        QString type;
        switch (row.kind) {
        case Kind::Nodal: type = tr("Forca nodal"); break;
        case Kind::Distributed: type = tr("Distribuida"); break;
        case Kind::Moment: type = tr("Momento"); break;
        }
        auto *item = new QTreeWidgetItem(m_tree, {row.element, type, row.loadCase, row.values});
        item->setData(0, kKindRole, static_cast<int>(row.kind));
        item->setData(0, kIndexRole, row.index);
    }
    m_summary->setText(rows.isEmpty() ? tr("Nenhuma carga nos elementos selecionados.")
                                      : tr("%1 carga(s). Duplo clique edita.").arg(rows.size()));
}

void LoadsPanel::reposition()
{
    if (parentWidget()) {
        adjustSize();
        move(12, 12);
    }
}

} // namespace Structura::UI
