#include "SectionDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>

SectionDialog::SectionDialog(QWidget *parent)
    : QDialog(parent)
    , m_nameEdit(new QLineEdit(this))
    , m_areaSpin(new QDoubleSpinBox(this))
    , m_izSpin(new QDoubleSpinBox(this))
    , m_iySpin(new QDoubleSpinBox(this))
    , m_jSpin(new QDoubleSpinBox(this))
{
    setWindowTitle(tr("Nova secao"));
    setModal(true);
    setMinimumWidth(320);

    m_nameEdit->setPlaceholderText(tr("Nome"));

    auto configureSpin = [](QDoubleSpinBox *spin, double step) {
        spin->setRange(1e-6, 1e6);
        spin->setDecimals(6);
        spin->setSingleStep(step);
    };

    configureSpin(m_areaSpin, 0.001);
    m_areaSpin->setSuffix(tr(" m2"));
    m_areaSpin->setValue(0.01);

    configureSpin(m_izSpin, 0.001);
    m_izSpin->setSuffix(tr(" m4"));
    m_izSpin->setValue(1e-4);

    configureSpin(m_iySpin, 0.001);
    m_iySpin->setSuffix(tr(" m4"));
    m_iySpin->setValue(1e-4);

    configureSpin(m_jSpin, 0.001);
    m_jSpin->setSuffix(tr(" m4"));
    m_jSpin->setValue(1e-4);

    auto *form = new QFormLayout();
    form->addRow(tr("Nome"), m_nameEdit);
    form->addRow(tr("Area (A)"), m_areaSpin);
    form->addRow(tr("Inercia Iz"), m_izSpin);
    form->addRow(tr("Inercia Iy"), m_iySpin);
    form->addRow(tr("Inercia polar (J)"), m_jSpin);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SectionDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SectionDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

QString SectionDialog::name() const
{
    return m_nameEdit->text().trimmed();
}

double SectionDialog::area() const
{
    return m_areaSpin->value();
}

double SectionDialog::iz() const
{
    return m_izSpin->value();
}

double SectionDialog::iy() const
{
    return m_iySpin->value();
}

double SectionDialog::j() const
{
    return m_jSpin->value();
}
