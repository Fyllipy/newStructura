#include "DistributedLoadDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {
constexpr double kDistributedRange = 1e6;
}

DistributedLoadDialog::DistributedLoadDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Carga distribuida (barras)"));
    setModal(true);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    m_selectionLabel = new QLabel(tr("Nenhuma barra selecionada"), this);
    m_selectionLabel->setStyleSheet(QStringLiteral("font-weight: 500; color: #1f2530;"));
    mainLayout->addWidget(m_selectionLabel);

    auto *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignLeft);
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    formLayout->setSpacing(6);

    auto createSpin = [this]() -> QDoubleSpinBox * {
        auto *spin = new QDoubleSpinBox(this);
        spin->setDecimals(3);
        spin->setRange(-kDistributedRange, kDistributedRange);
        spin->setSingleStep(0.1);
        spin->setAccelerated(true);
        return spin;
    };

    m_qxSpin = createSpin();
    m_qySpin = createSpin();
    m_qzSpin = createSpin();

    formLayout->addRow(tr("qx (kN/m)"), m_qxSpin);
    formLayout->addRow(tr("qy (kN/m)"), m_qySpin);
    formLayout->addRow(tr("qz (kN/m)"), m_qzSpin);

    m_systemCombo = new QComboBox(this);
    m_systemCombo->addItem(tr("Global"), QStringLiteral("GLOBAL"));
    m_systemCombo->addItem(tr("Local"), QStringLiteral("LOCAL"));
    formLayout->addRow(tr("Sistema"), m_systemCombo);

    mainLayout->addLayout(formLayout);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &DistributedLoadDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &DistributedLoadDialog::reject);
    mainLayout->addWidget(buttonBox);

    resize(320, sizeHint().height());
}

void DistributedLoadDialog::setInitialValues(double qx, double qy, double qz, const QString &system)
{
    if (m_qxSpin) m_qxSpin->setValue(qx);
    if (m_qySpin) m_qySpin->setValue(qy);
    if (m_qzSpin) m_qzSpin->setValue(qz);
    if (m_systemCombo) {
        const QString normalized = system.trimmed().toUpper();
        const int index = m_systemCombo->findData(normalized);
        m_systemCombo->setCurrentIndex(index >= 0 ? index : 0);
    }
}

DistributedLoadDialog::Values DistributedLoadDialog::values() const
{
    Values result;
    if (m_qxSpin) result.qx = m_qxSpin->value();
    if (m_qySpin) result.qy = m_qySpin->value();
    if (m_qzSpin) result.qz = m_qzSpin->value();
    if (m_systemCombo) {
        result.system = m_systemCombo->currentData().toString();
    }
    return result;
}

void DistributedLoadDialog::setSelectedCount(int count)
{
    if (!m_selectionLabel) {
        return;
    }
    if (count <= 0) {
        m_selectionLabel->setText(tr("Nenhuma barra selecionada"));
    } else if (count == 1) {
        m_selectionLabel->setText(tr("1 barra selecionada"));
    } else {
        m_selectionLabel->setText(tr("%1 barras selecionadas").arg(count));
    }
}

