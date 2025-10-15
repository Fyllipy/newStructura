#include "CoordinateDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

CoordinateDialog::CoordinateDialog(QWidget *parent)
    : QDialog(parent)
    , m_xInput(new QDoubleSpinBox(this))
    , m_yInput(new QDoubleSpinBox(this))
    , m_zInput(new QDoubleSpinBox(this))
    , m_screenButton(new QPushButton(tr("Inserir na tela"), this))
{
    setWindowTitle(tr("Inserir ponto"));
    setModal(true);
    setMinimumWidth(280);

    const auto configureSpinBox = [](QDoubleSpinBox *spinBox) {
        spinBox->setRange(-1e6, 1e6);
        spinBox->setDecimals(3);
        spinBox->setSingleStep(1.0);
    };

    configureSpinBox(m_xInput);
    configureSpinBox(m_yInput);
    configureSpinBox(m_zInput);

    auto *formLayout = new QFormLayout();
    formLayout->addRow(tr("Coordenada X"), m_xInput);
    formLayout->addRow(tr("Coordenada Y"), m_yInput);
    formLayout->addRow(tr("Coordenada Z"), m_zInput);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &CoordinateDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &CoordinateDialog::reject);

    // Custom row: screen-insert button
    auto *row = new QHBoxLayout();
    row->addStretch(1);
    row->addWidget(m_screenButton, 0);
    connect(m_screenButton, &QPushButton::clicked, this, [this]() {
        emit requestScreenInsertion();
    });

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addLayout(row);
    mainLayout->addWidget(buttonBox);
}

double CoordinateDialog::x() const
{
    return m_xInput->value();
}

double CoordinateDialog::y() const
{
    return m_yInput->value();
}

double CoordinateDialog::z() const
{
    return m_zInput->value();
}
