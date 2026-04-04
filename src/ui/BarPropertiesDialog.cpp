#include "BarPropertiesDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QVariant>

BarPropertiesDialog::BarPropertiesDialog(const QVector<QPair<QUuid, QString>> &materials,
                                         const QVector<QPair<QUuid, QString>> &sections,
                                         QWidget *parent)
    : QDialog(parent)
    , m_materialOptions(materials)
    , m_sectionOptions(sections)
    , m_materialCombo(new QComboBox(this))
    , m_sectionCombo(new QComboBox(this))
{
    setWindowTitle(tr("Propriedades da barra"));
    setModal(true);
    setMinimumWidth(300);

    populateCombo(m_materialCombo, m_materialOptions, tr("Sem material"));
    populateCombo(m_sectionCombo, m_sectionOptions, tr("Sem secao"));

    auto *form = new QFormLayout();
    form->addRow(tr("Material"), m_materialCombo);
    form->addRow(tr("Secao"), m_sectionCombo);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &BarPropertiesDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &BarPropertiesDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

void BarPropertiesDialog::populateCombo(QComboBox *combo, const QVector<QPair<QUuid, QString>> &options, const QString &emptyLabel)
{
    combo->clear();
    combo->addItem(emptyLabel, QVariant());
    for (const auto &option : options) {
        combo->addItem(option.second, option.first);
    }
}

int BarPropertiesDialog::indexForId(const QVector<QPair<QUuid, QString>> &options, const QUuid &id) const
{
    if (id.isNull()) {
        return 0;
    }
    for (int i = 0; i < options.size(); ++i) {
        if (options[i].first == id) {
            return i + 1; // +1 because of "Sem" entry
        }
    }
    return 0;
}

void BarPropertiesDialog::setCurrentMaterial(const QUuid &id)
{
    m_materialCombo->setCurrentIndex(indexForId(m_materialOptions, id));
}

void BarPropertiesDialog::setCurrentSection(const QUuid &id)
{
    m_sectionCombo->setCurrentIndex(indexForId(m_sectionOptions, id));
}

QUuid BarPropertiesDialog::selectedMaterial() const
{
    QVariant data = m_materialCombo->currentData();
    if (!data.isValid()) {
        return {};
    }
    return data.toUuid();
}

QUuid BarPropertiesDialog::selectedSection() const
{
    QVariant data = m_sectionCombo->currentData();
    if (!data.isValid()) {
        return {};
    }
    return data.toUuid();
}
