#include "LoadCaseVisibilityDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace Structura::UI {

LoadCaseVisibilityDialog::LoadCaseVisibilityDialog(const QStringList &cases, const QSet<QString> &visible, QWidget *parent)
    : QDialog(parent)
    , m_list(new QListWidget(this))
{
    setWindowTitle(tr("Casos de carregamento visíveis"));
    resize(360, 420);

    auto *hint = new QLabel(tr("Marque os casos cujos carregamentos devem aparecer na tela. Os demais ficam ocultos (só a exibição muda; "
                               "a análise considera todos os casos)."),
                            this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#5a6574;"));

    for (const QString &name : cases) {
        auto *item = new QListWidgetItem(name, m_list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(visible.contains(name) ? Qt::Checked : Qt::Unchecked);
    }

    auto *all = new QPushButton(tr("Marcar todos"), this);
    auto *none = new QPushButton(tr("Desmarcar todos"), this);
    connect(all, &QPushButton::clicked, this, [this]() { setAll(true); });
    connect(none, &QPushButton::clicked, this, [this]() { setAll(false); });
    auto *quick = new QHBoxLayout();
    quick->addWidget(all);
    quick->addWidget(none);
    quick->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(hint);
    layout->addLayout(quick);
    layout->addWidget(m_list, 1);
    layout->addWidget(buttons);
}

QSet<QString> LoadCaseVisibilityDialog::visibleCases() const
{
    QSet<QString> result;
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->checkState() == Qt::Checked) {
            result.insert(m_list->item(i)->text());
        }
    }
    return result;
}

bool LoadCaseVisibilityDialog::allChecked() const
{
    return visibleCases().size() == m_list->count();
}

void LoadCaseVisibilityDialog::setAll(bool checked)
{
    for (int i = 0; i < m_list->count(); ++i) {
        m_list->item(i)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
}

} // namespace Structura::UI
