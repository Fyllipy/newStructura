#include "NodalLoadDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {
constexpr double kLoadRange = 1e6;
}

NodalLoadDialog::NodalLoadDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Forca concentrada (nos)"));
    setModal(true);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    m_selectionLabel = new QLabel(tr("Nenhum no selecionado"), this);
    m_selectionLabel->setStyleSheet(QStringLiteral("font-weight: 500; color: #1f2530;"));
    mainLayout->addWidget(m_selectionLabel);

    auto *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignLeft);
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    formLayout->setSpacing(6);

    auto createSpin = [this]() -> QDoubleSpinBox * {
        auto *spin = new QDoubleSpinBox(this);
        spin->setDecimals(3);
        spin->setRange(-kLoadRange, kLoadRange);
        spin->setSingleStep(0.1);
        spin->setAccelerated(true);
        return spin;
    };

    m_fxSpin = createSpin();
    m_fySpin = createSpin();
    m_fzSpin = createSpin();
    m_mxSpin = createSpin();
    m_mySpin = createSpin();
    m_mzSpin = createSpin();

    formLayout->addRow(tr("Fx (kN)"), m_fxSpin);
    formLayout->addRow(tr("Fy (kN)"), m_fySpin);
    formLayout->addRow(tr("Fz (kN)"), m_fzSpin);
    formLayout->addRow(tr("Mx (kN.m)"), m_mxSpin);
    formLayout->addRow(tr("My (kN.m)"), m_mySpin);
    formLayout->addRow(tr("Mz (kN.m)"), m_mzSpin);

    mainLayout->addLayout(formLayout);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &NodalLoadDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &NodalLoadDialog::reject);
    mainLayout->addWidget(buttonBox);

    resize(320, sizeHint().height());
}

void NodalLoadDialog::setInitialValues(double fx, double fy, double fz,
                                       double mx, double my, double mz)
{
    if (m_fxSpin) m_fxSpin->setValue(fx);
    if (m_fySpin) m_fySpin->setValue(fy);
    if (m_fzSpin) m_fzSpin->setValue(fz);
    if (m_mxSpin) m_mxSpin->setValue(mx);
    if (m_mySpin) m_mySpin->setValue(my);
    if (m_mzSpin) m_mzSpin->setValue(mz);
}

NodalLoadDialog::Values NodalLoadDialog::values() const
{
    Values result;
    if (m_fxSpin) result.fx = m_fxSpin->value();
    if (m_fySpin) result.fy = m_fySpin->value();
    if (m_fzSpin) result.fz = m_fzSpin->value();
    if (m_mxSpin) result.mx = m_mxSpin->value();
    if (m_mySpin) result.my = m_mySpin->value();
    if (m_mzSpin) result.mz = m_mzSpin->value();
    return result;
}

void NodalLoadDialog::setSelectedCount(int count)
{
    if (!m_selectionLabel) {
        return;
    }
    if (count <= 0) {
        m_selectionLabel->setText(tr("Nenhum no selecionado"));
    } else if (count == 1) {
        m_selectionLabel->setText(tr("1 no selecionado"));
    } else {
        m_selectionLabel->setText(tr("%1 nos selecionados").arg(count));
    }
}

