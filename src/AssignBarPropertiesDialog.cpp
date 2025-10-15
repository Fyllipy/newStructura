#include "AssignBarPropertiesDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QAbstractItemView>
#include <QVariant>

AssignBarPropertiesDialog::AssignBarPropertiesDialog(const QVector<QPair<QUuid, QString>> &materials,
                                                     const QVector<QPair<QUuid, QString>> &sections,
                                                     const std::vector<SceneController::BarInfo> &bars,
                                                     QWidget *parent)
    : QDialog(parent)
    , m_materialOptions(materials)
    , m_sectionOptions(sections)
    , m_barList(new QListWidget(this))
    , m_materialCombo(new QComboBox(this))
    , m_sectionCombo(new QComboBox(this))
{
    setWindowTitle(tr("Atribuir propriedades"));
    setModal(true);
    resize(420, 360);

    populateCombo(m_materialCombo, m_materialOptions, tr("Sem material"));
    populateCombo(m_sectionCombo, m_sectionOptions, tr("Sem secao"));

    m_barList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    int index = 0;
    for (const auto &bar : bars) {
        const QString label = tr("Barra %1 (N%2 - N%3)")
            .arg(index + 1)
            .arg(bar.startNode + 1)
            .arg(bar.endNode + 1);
        auto *item = new QListWidgetItem(label, m_barList);
        item->setData(Qt::UserRole, index);
        ++index;
    }

    auto *selectAllBtn = new QPushButton(tr("Selecionar todos"), this);
    connect(selectAllBtn, &QPushButton::clicked, this, [this]() {
        m_barList->selectAll();
    });

    auto *listLayout = new QVBoxLayout();
    listLayout->addWidget(m_barList, 1);
    listLayout->addWidget(selectAllBtn, 0, Qt::AlignRight);

    auto *propertiesLayout = new QFormLayout();
    propertiesLayout->addRow(tr("Material"), m_materialCombo);
    propertiesLayout->addRow(tr("Secao"), m_sectionCombo);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(listLayout, 1);
    mainLayout->addLayout(propertiesLayout);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &AssignBarPropertiesDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &AssignBarPropertiesDialog::reject);
    mainLayout->addWidget(buttons);
}

void AssignBarPropertiesDialog::populateCombo(QComboBox *combo, const QVector<QPair<QUuid, QString>> &options, const QString &emptyLabel)
{
    combo->clear();
    combo->addItem(emptyLabel, QVariant());
    for (const auto &option : options) {
        combo->addItem(option.second, option.first);
    }
}

int AssignBarPropertiesDialog::indexForId(const QVector<QPair<QUuid, QString>> &options, const QUuid &id) const
{
    if (id.isNull()) {
        return 0;
    }
    for (int i = 0; i < options.size(); ++i) {
        if (options[i].first == id) {
            return i + 1;
        }
    }
    return 0;
}

void AssignBarPropertiesDialog::setCurrentMaterial(const QUuid &id)
{
    m_materialCombo->setCurrentIndex(indexForId(m_materialOptions, id));
}

void AssignBarPropertiesDialog::setCurrentSection(const QUuid &id)
{
    m_sectionCombo->setCurrentIndex(indexForId(m_sectionOptions, id));
}

QUuid AssignBarPropertiesDialog::selectedMaterial() const
{
    QVariant data = m_materialCombo->currentData();
    if (!data.isValid()) {
        return {};
    }
    return data.toUuid();
}

QUuid AssignBarPropertiesDialog::selectedSection() const
{
    QVariant data = m_sectionCombo->currentData();
    if (!data.isValid()) {
        return {};
    }
    return data.toUuid();
}

QList<int> AssignBarPropertiesDialog::selectedBarIndices() const
{
    QList<int> result;
    for (QListWidgetItem *item : m_barList->selectedItems()) {
        result.append(item->data(Qt::UserRole).toInt());
    }
    return result;
}
