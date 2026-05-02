#include "BarPropertiesDialog.h"

#include "DialogComboUtils.h"

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

    Structura::UI::populateUuidCombo(m_materialCombo, m_materialOptions, tr("Sem material"));
    Structura::UI::populateUuidCombo(m_sectionCombo, m_sectionOptions, tr("Sem secao"));

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

void BarPropertiesDialog::setCurrentMaterial(const QUuid &id)
{
    m_materialCombo->setCurrentIndex(Structura::UI::indexForUuid(m_materialOptions, id));
}

void BarPropertiesDialog::setCurrentSection(const QUuid &id)
{
    m_sectionCombo->setCurrentIndex(Structura::UI::indexForUuid(m_sectionOptions, id));
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
