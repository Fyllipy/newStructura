#include "RestraintDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

RestraintDialog::RestraintDialog(QWidget *parent)
    : QDialog(parent)
{
    m_restraintChecks.fill(nullptr);
    setupUI();
    setWindowTitle(tr("Aplicar Restrições Nodais"));
    resize(400, 320);
}

void RestraintDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // Info label
    m_infoLabel = new QLabel(
        tr("Selecione os graus de liberdade (DOF) que deseja fixar.\n"
           "U = Translação, R = Rotação"),
        this);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setStyleSheet("color: #555; font-size: 10pt;");
    mainLayout->addWidget(m_infoLabel);

    // Translation restraints group
    auto *translationGroup = new QGroupBox(tr("Translações"), this);
    auto *translationLayout = new QHBoxLayout(translationGroup);
    translationLayout->setSpacing(16);

    const QStringList translationLabels = {"UX", "UY", "UZ"};
    for (int i = 0; i < 3; ++i) {
        m_restraintChecks[i] = new QCheckBox(translationLabels[i], this);
        m_restraintChecks[i]->setStyleSheet("font-weight: 600; font-size: 11pt;");
        translationLayout->addWidget(m_restraintChecks[i]);
    }
    translationLayout->addStretch();
    mainLayout->addWidget(translationGroup);

    // Rotation restraints group
    auto *rotationGroup = new QGroupBox(tr("Rotações"), this);
    auto *rotationLayout = new QHBoxLayout(rotationGroup);
    rotationLayout->setSpacing(16);

    const QStringList rotationLabels = {"RX", "RY", "RZ"};
    for (int i = 0; i < 3; ++i) {
        m_restraintChecks[i + 3] = new QCheckBox(rotationLabels[i], this);
        m_restraintChecks[i + 3]->setStyleSheet("font-weight: 600; font-size: 11pt;");
        rotationLayout->addWidget(m_restraintChecks[i + 3]);
    }
    rotationLayout->addStretch();
    mainLayout->addWidget(rotationGroup);

    // Quick presets
    auto *presetsGroup = new QGroupBox(tr("Presets Rápidos"), this);
    auto *presetsLayout = new QHBoxLayout(presetsGroup);
    presetsLayout->setSpacing(8);

    auto *fixedBtn = new QPushButton(tr("Engaste"), this);
    fixedBtn->setToolTip(tr("Fixar todos os graus de liberdade"));
    connect(fixedBtn, &QPushButton::clicked, this, [this]() {
        for (auto *check : m_restraintChecks) {
            if (check) check->setChecked(true);
        }
    });
    presetsLayout->addWidget(fixedBtn);

    auto *pinnedBtn = new QPushButton(tr("Apoio Simples"), this);
    pinnedBtn->setToolTip(tr("Fixar apenas translações (UX, UY, UZ)"));
    connect(pinnedBtn, &QPushButton::clicked, this, [this]() {
        for (int i = 0; i < 3; ++i) {
            if (m_restraintChecks[i]) m_restraintChecks[i]->setChecked(true);
        }
        for (int i = 3; i < 6; ++i) {
            if (m_restraintChecks[i]) m_restraintChecks[i]->setChecked(false);
        }
    });
    presetsLayout->addWidget(pinnedBtn);

    auto *clearBtn = new QPushButton(tr("Limpar"), this);
    clearBtn->setToolTip(tr("Remover todas as restrições"));
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        for (auto *check : m_restraintChecks) {
            if (check) check->setChecked(false);
        }
    });
    presetsLayout->addWidget(clearBtn);

    presetsLayout->addStretch();
    mainLayout->addWidget(presetsGroup);

    mainLayout->addStretch();

    // Dialog buttons
    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

std::array<bool, 6> RestraintDialog::restraints() const
{
    std::array<bool, 6> result;
    for (int i = 0; i < 6; ++i) {
        result[i] = m_restraintChecks[i] ? m_restraintChecks[i]->isChecked() : false;
    }
    return result;
}

void RestraintDialog::setRestraints(const std::array<bool, 6> &restraints)
{
    for (int i = 0; i < 6; ++i) {
        if (m_restraintChecks[i]) {
            m_restraintChecks[i]->setChecked(restraints[i]);
        }
    }
    m_mixedState = false;
}

void RestraintDialog::setMixedState(bool mixed)
{
    m_mixedState = mixed;
    if (mixed && m_infoLabel) {
        m_infoLabel->setText(
            tr("Seleção múltipla com valores variados.\n"
               "Marcar um DOF aplicará a restrição a todos os nós selecionados."));
        m_infoLabel->setStyleSheet("color: #c06000; font-size: 10pt; font-weight: 600;");
    }
}
