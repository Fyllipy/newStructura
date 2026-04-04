#include "MaterialDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>

MaterialDialog::MaterialDialog(QWidget *parent)
    : QDialog(parent)
    , m_nameEdit(new QLineEdit(this))
    , m_modulusSpin(new QDoubleSpinBox(this))
    , m_shearSpin(new QDoubleSpinBox(this))
{
    setWindowTitle(tr("Novo material"));
    setModal(true);
    setMinimumWidth(320);

    m_nameEdit->setPlaceholderText(tr("Nome"));

    m_modulusSpin->setRange(1e3, 1e12);
    m_modulusSpin->setDecimals(3);
    m_modulusSpin->setSingleStep(1e6);
    m_modulusSpin->setSuffix(tr(" Pa"));
    m_modulusSpin->setValue(2.1e11);

    m_shearSpin->setRange(1e2, 1e12);
    m_shearSpin->setDecimals(3);
    m_shearSpin->setSingleStep(1e6);
    m_shearSpin->setSuffix(tr(" Pa"));
    m_shearSpin->setValue(8.1e10);

    auto *form = new QFormLayout();
    form->addRow(tr("Nome"), m_nameEdit);
    form->addRow(tr("Modulo de elasticidade (E)"), m_modulusSpin);
    form->addRow(tr("Modulo de cisalhamento (G)"), m_shearSpin);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &MaterialDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &MaterialDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

QString MaterialDialog::name() const
{
    return m_nameEdit->text().trimmed();
}

double MaterialDialog::youngModulus() const
{
    return m_modulusSpin->value();
}

double MaterialDialog::shearModulus() const
{
    return m_shearSpin->value();
}
