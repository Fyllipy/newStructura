#include "MemberMomentDialog.h"

#include "app/LoadSystemUtils.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {
constexpr double kMomentRange = 1e6;
}

MemberMomentDialog::MemberMomentDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Momento distribuido (barras)"));
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

    m_caseCombo = new QComboBox(this);
    formLayout->addRow(tr("Caso de carga"), m_caseCombo);

    auto createSpin = [this]() -> QDoubleSpinBox * {
        auto *spin = new QDoubleSpinBox(this);
        spin->setDecimals(3);
        spin->setRange(-kMomentRange, kMomentRange);
        spin->setSingleStep(0.1);
        spin->setAccelerated(true);
        return spin;
    };

    m_mxSpin = createSpin();
    m_mySpin = createSpin();
    m_mzSpin = createSpin();

    formLayout->addRow(tr("mx (kN.m/m)"), m_mxSpin);
    formLayout->addRow(tr("my (kN.m/m)"), m_mySpin);
    formLayout->addRow(tr("mz (kN.m/m)"), m_mzSpin);

    m_systemCombo = new QComboBox(this);
    m_systemCombo->addItem(tr("Global"), QStringLiteral("GLOBAL"));
    m_systemCombo->addItem(tr("Local"), QStringLiteral("LOCAL"));
    formLayout->addRow(tr("Sistema"), m_systemCombo);

    mainLayout->addLayout(formLayout);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &MemberMomentDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &MemberMomentDialog::reject);
    mainLayout->addWidget(buttonBox);

    resize(320, sizeHint().height());
}

void MemberMomentDialog::setInitialValues(double mx, double my, double mz, const QString &system)
{
    if (m_mxSpin) m_mxSpin->setValue(mx);
    if (m_mySpin) m_mySpin->setValue(my);
    if (m_mzSpin) m_mzSpin->setValue(mz);
    if (m_systemCombo) {
        const QString normalized = Structura::App::normalizeMemberLoadSystem(system);
        const int index = m_systemCombo->findData(normalized);
        m_systemCombo->setCurrentIndex(index >= 0 ? index : 0);
    }
}

MemberMomentDialog::Values MemberMomentDialog::values() const
{
    Values result;
    if (m_mxSpin) result.mx = m_mxSpin->value();
    if (m_mySpin) result.my = m_mySpin->value();
    if (m_mzSpin) result.mz = m_mzSpin->value();
    if (m_systemCombo) {
        result.system = m_systemCombo->currentData().toString();
    }
    return result;
}

void MemberMomentDialog::setLoadCases(const QStringList &names, const QString &current)
{
    m_caseCombo->clear();
    m_caseCombo->addItems(names);
    const int index = m_caseCombo->findText(current);
    m_caseCombo->setCurrentIndex(index >= 0 ? index : 0);
}

QString MemberMomentDialog::loadCase() const
{
    return m_caseCombo->currentText();
}

void MemberMomentDialog::setSelectedCount(int count)
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

