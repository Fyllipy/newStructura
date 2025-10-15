#include "GridDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>

GridDialog::GridDialog(QWidget *parent)
    : QDialog(parent)
    , m_dx(new QDoubleSpinBox(this))
    , m_dy(new QDoubleSpinBox(this))
    , m_dz(new QDoubleSpinBox(this))
    , m_nx(new QSpinBox(this))
    , m_ny(new QSpinBox(this))
    , m_nz(new QSpinBox(this))
{
    setWindowTitle(tr("Gerar grid"));
    setModal(true);
    setMinimumWidth(300);

    const auto cfgStep = [](QDoubleSpinBox *s) {
        s->setRange(1e-6, 1e6);
        s->setDecimals(3);
        s->setSingleStep(1.0);
        s->setValue(1.0);
    };
    cfgStep(m_dx);
    cfgStep(m_dy);
    cfgStep(m_dz);

    const auto cfgCount = [](QSpinBox *s) {
        s->setRange(1, 1000);
        s->setSingleStep(1);
        s->setValue(11);
    };
    cfgCount(m_nx);
    cfgCount(m_ny);
    cfgCount(m_nz);

    auto *form = new QFormLayout();
    form->addRow(tr("Dx"), m_dx);
    form->addRow(tr("Dy"), m_dy);
    form->addRow(tr("Dz"), m_dz);
    form->addRow(tr("Linhas X"), m_nx);
    form->addRow(tr("Linhas Y"), m_ny);
    form->addRow(tr("Linhas Z"), m_nz);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &GridDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &GridDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

double GridDialog::dx() const { return m_dx->value(); }
double GridDialog::dy() const { return m_dy->value(); }
double GridDialog::dz() const { return m_dz->value(); }
int GridDialog::nx() const { return m_nx->value(); }
int GridDialog::ny() const { return m_ny->value(); }
int GridDialog::nz() const { return m_nz->value(); }

