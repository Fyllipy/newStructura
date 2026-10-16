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
    setWindowTitle(tr("Restrições nodais"));
    setMinimumWidth(460);
}

void RestraintDialog::setupUI()
{
    setStyleSheet(QStringLiteral(
        "RestraintDialog { background: #f6f8fb; }"
        "QLabel#RestraintTitle { font-size: 14pt; font-weight: 700; color: #1e232b; }"
        "QLabel#RestraintHint { color: #5a6472; }"
        "QLabel#RestraintKind { font-weight: 700; color: #0d7fb9; padding: 4px 10px; background: #e3f1fa; border-radius: 10px; }"
        "QGroupBox { background: white; border: 1px solid #d7dde6; border-radius: 6px; margin-top: 14px; padding: 12px 10px 10px 10px; font-weight: 600; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #3b4452; }"
        "QCheckBox { font-weight: 700; font-size: 11pt; padding: 8px 0px; min-width: 56px; border: 1px solid #c3ccd8; border-radius: 6px; background: #f2f5fa; color: #3b4452; }"
        "QCheckBox::indicator { width: 0; height: 0; }"
        "QCheckBox:checked { background: #0d7fb9; border-color: #0a6a9b; color: white; }"
        "QCheckBox:hover { border-color: #0d7fb9; }"
        "QPushButton { padding: 6px 14px; border: 1px solid #c3ccd8; border-radius: 5px; background: white; }"
        "QPushButton:hover { border-color: #0d7fb9; background: #eef6fc; }"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 14);
    mainLayout->setSpacing(10);

    auto *title = new QLabel(tr("Restrições nodais"), this);
    title->setObjectName(QStringLiteral("RestraintTitle"));
    mainLayout->addWidget(title);
    m_infoLabel = new QLabel(tr("Marque os graus de liberdade que ficam impedidos nos nós selecionados (U = translação, R = rotação)."), this);
    m_infoLabel->setObjectName(QStringLiteral("RestraintHint"));
    m_infoLabel->setWordWrap(true);
    mainLayout->addWidget(m_infoLabel);

    auto makeGroup = [&](const QString &name, const QStringList &labels, int offset) {
        auto *group = new QGroupBox(name, this);
        auto *layout = new QHBoxLayout(group);
        layout->setSpacing(10);
        for (int i = 0; i < 3; ++i) {
            auto *check = new QCheckBox(labels[i], this);
            check->setCursor(Qt::PointingHandCursor);
            m_restraintChecks[static_cast<std::size_t>(offset + i)] = check;
            layout->addWidget(check, 1);
            connect(check, &QCheckBox::toggled, this, [this]() { updateKindLabel(); });
        }
        mainLayout->addWidget(group);
    };
    makeGroup(tr("Translações"), {QStringLiteral("UX"), QStringLiteral("UY"), QStringLiteral("UZ")}, 0);
    makeGroup(tr("Rotações"), {QStringLiteral("RX"), QStringLiteral("RY"), QStringLiteral("RZ")}, 3);

    auto *presets = new QGroupBox(tr("Tipos usuais de apoio"), this);
    auto *presetsLayout = new QHBoxLayout(presets);
    presetsLayout->setSpacing(8);
    auto addPreset = [&](const QString &text, const QString &tip, std::array<bool, 6> values) {
        auto *button = new QPushButton(text, this);
        button->setToolTip(tip);
        connect(button, &QPushButton::clicked, this, [this, values]() { setRestraints(values); updateKindLabel(); });
        presetsLayout->addWidget(button);
    };
    addPreset(tr("Engaste"), tr("Fixa os seis graus de liberdade"), {true, true, true, true, true, true});
    addPreset(tr("Articulado"), tr("Fixa as três translações; as rotações ficam livres"), {true, true, true, false, false, false});
    addPreset(tr("Rolete (UZ)"), tr("Impede apenas o deslocamento vertical"), {false, false, true, false, false, false});
    addPreset(tr("Livre"), tr("Remove todas as restrições"), {false, false, false, false, false, false});
    mainLayout->addWidget(presets);

    m_kindLabel = new QLabel(this);
    m_kindLabel->setObjectName(QStringLiteral("RestraintKind"));
    mainLayout->addWidget(m_kindLabel, 0, Qt::AlignLeft);
    mainLayout->addStretch();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Aplicar"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
    updateKindLabel();
}

void RestraintDialog::updateKindLabel()
{
    if (!m_kindLabel) {
        return;
    }
    const auto r = restraints();
    const int translations = int(r[0]) + int(r[1]) + int(r[2]);
    const int rotations = int(r[3]) + int(r[4]) + int(r[5]);
    QString kind;
    if (translations == 3 && rotations == 3) {
        kind = tr("Engaste");
    } else if (translations == 3 && rotations == 0) {
        kind = tr("Apoio articulado");
    } else if (translations + rotations == 0) {
        kind = tr("Nó livre");
    } else if (translations == 1 && rotations == 0 && r[2]) {
        kind = tr("Rolete vertical");
    } else {
        kind = tr("Personalizado (%1 translação(ões), %2 rotação(ões))").arg(translations).arg(rotations);
    }
    m_kindLabel->setText(tr("Resultado: %1").arg(kind));
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
