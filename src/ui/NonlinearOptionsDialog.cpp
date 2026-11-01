#include "NonlinearOptionsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Structura::UI {

namespace PD = Structura::App::ProjectData;

NonlinearOptionsDialog::NonlinearOptionsDialog(const PD::NonlinearSettings &initial, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Análise não linear incremental"));
    auto *root = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("A análise incremental (Newton–Raphson) é usada automaticamente quando há rótulas plásticas ou ligações com lei não linear. "
                                "Marque a opção abaixo para executá-la também sem esses elementos (por exemplo, para obter a curva de capacidade da estrutura elástica com 2ª ordem)."), this);
    intro->setWordWrap(true);
    intro->setStyleSheet(QStringLiteral("color: #5a6472;"));
    root->addWidget(intro);
    m_force = new QCheckBox(tr("Executar sempre a análise incremental"), this);
    m_force->setChecked(initial.force);
    root->addWidget(m_force);

    auto *solution = new QGroupBox(tr("Solução"), this);
    auto *form = new QFormLayout(solution);
    m_steps = new QSpinBox(solution);
    m_steps->setRange(1, 10000);
    m_steps->setValue(initial.steps);
    m_iterations = new QSpinBox(solution);
    m_iterations->setRange(2, 500);
    m_iterations->setValue(initial.maxIterations);
    m_cutbacks = new QSpinBox(solution);
    m_cutbacks->setRange(0, 30);
    m_cutbacks->setValue(initial.maxCutbacks);
    m_cutbacks->setToolTip(tr("Quantas vezes o passo pode ser dividido por 2 quando a iteração não converge"));
    m_tolerance = new QDoubleSpinBox(solution);
    m_tolerance->setDecimals(10);
    m_tolerance->setRange(1e-12, 1e-2);
    m_tolerance->setSingleStep(1e-7);
    m_tolerance->setValue(initial.tolerance);
    form->addRow(tr("Passos de carga/deslocamento"), m_steps);
    form->addRow(tr("Iterações máximas por passo"), m_iterations);
    form->addRow(tr("Reduções do passo (bisseção)"), m_cutbacks);
    form->addRow(tr("Tolerância relativa"), m_tolerance);
    m_criterionForce = new QCheckBox(tr("Resíduo de forças desequilibradas"), solution);
    m_criterionDisplacement = new QCheckBox(tr("Incremento de deslocamentos"), solution);
    m_criterionEnergy = new QCheckBox(tr("Incremento de energia"), solution);
    m_criterionForce->setChecked((initial.criteria & 1) != 0);
    m_criterionDisplacement->setChecked((initial.criteria & 2) != 0);
    m_criterionEnergy->setChecked((initial.criteria & 4) != 0);
    form->addRow(tr("Critérios (todos os marcados)"), m_criterionForce);
    form->addRow(QString(), m_criterionDisplacement);
    form->addRow(QString(), m_criterionEnergy);
    root->addWidget(solution);

    auto *control = new QGroupBox(tr("Controle"), this);
    auto *controlForm = new QFormLayout(control);
    m_control = new QComboBox(control);
    m_control->addItem(tr("Controle de carga (fator de carga crescente)"), 0);
    m_control->addItem(tr("Controle de deslocamento (curva de capacidade / pushover)"), 1);
    m_control->setCurrentIndex(initial.controlMode == 1 ? 1 : 0);
    m_loadFactor = new QDoubleSpinBox(control);
    m_loadFactor->setRange(0.001, 1000.0);
    m_loadFactor->setDecimals(4);
    m_loadFactor->setValue(initial.loadFactorMax);
    m_loadFactor->setToolTip(tr("1 = as cargas da combinação; maior que 1 procura a carga limite"));
    m_node = new QSpinBox(control);
    m_node->setRange(0, 1000000);
    m_node->setValue(initial.controlNode);
    m_dof = new QComboBox(control);
    for (const char *name : {"ux", "uy", "uz", "rx", "ry", "rz"}) {
        m_dof->addItem(QLatin1String(name));
    }
    m_dof->setCurrentIndex(std::clamp(initial.controlDof, 0, 5));
    m_target = new QDoubleSpinBox(control);
    m_target->setRange(-1e6, 1e6);
    m_target->setDecimals(6);
    m_target->setValue(initial.controlTarget);
    m_target->setToolTip(tr("Deslocamento (m) ou rotação (rad) final do grau de controle; o fator de carga é a incógnita"));
    controlForm->addRow(tr("Tipo"), m_control);
    controlForm->addRow(tr("Fator de carga final"), m_loadFactor);
    controlForm->addRow(tr("Nó de controle (número)"), m_node);
    controlForm->addRow(tr("Grau de liberdade"), m_dof);
    controlForm->addRow(tr("Deslocamento alvo"), m_target);
    root->addWidget(control);
    connect(m_control, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { updateControlFields(); });
    updateControlFields();

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
}

void NonlinearOptionsDialog::updateControlFields()
{
    const bool displacement = m_control->currentData().toInt() == 1;
    m_loadFactor->setEnabled(!displacement);
    m_target->setEnabled(displacement);
    m_node->setToolTip(displacement ? tr("Obrigatório no controle de deslocamento; no controle de carga acompanha a curva de capacidade") : tr("Opcional: nó cujo deslocamento aparece na curva de capacidade"));
}

PD::NonlinearSettings NonlinearOptionsDialog::settings() const
{
    PD::NonlinearSettings s;
    s.force = m_force->isChecked();
    s.steps = m_steps->value();
    s.maxIterations = m_iterations->value();
    s.maxCutbacks = m_cutbacks->value();
    s.tolerance = m_tolerance->value();
    s.criteria = (m_criterionForce->isChecked() ? 1 : 0) | (m_criterionDisplacement->isChecked() ? 2 : 0) | (m_criterionEnergy->isChecked() ? 4 : 0);
    if (s.criteria == 0) {
        s.criteria = 5;
    }
    s.controlMode = m_control->currentData().toInt();
    s.loadFactorMax = m_loadFactor->value();
    s.controlNode = m_node->value();
    s.controlDof = m_dof->currentIndex();
    s.controlTarget = m_target->value();
    return s;
}

} // namespace Structura::UI
