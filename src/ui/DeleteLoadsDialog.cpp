#include "DeleteLoadsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace Structura::UI {

DeleteLoadsDialog::DeleteLoadsDialog(const QStringList &caseNames, int selectedNodes, int selectedBars, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Excluir cargas"));
    setMinimumWidth(460);
    auto *root = new QVBoxLayout(this);

    auto *types = new QGroupBox(tr("Tipos de carga"));
    auto *typesLayout = new QVBoxLayout(types);
    m_nodal = new QCheckBox(tr("Forças e momentos nodais"));
    m_distributed = new QCheckBox(tr("Cargas distribuídas nas barras"));
    m_moments = new QCheckBox(tr("Momentos distribuídos nas barras"));
    m_panels = new QCheckBox(tr("Cargas de área e vento (panos e as cargas distribuídas geradas por eles)"));
    for (auto *box : {m_nodal, m_distributed, m_moments, m_panels}) {
        box->setChecked(true);
        typesLayout->addWidget(box);
    }
    root->addWidget(types);

    auto *scope = new QGroupBox(tr("Onde excluir"));
    auto *scopeLayout = new QVBoxLayout(scope);
    m_selection = new QRadioButton(tr("Somente nos elementos selecionados (%1 nó(s), %2 barra(s))").arg(selectedNodes).arg(selectedBars));
    m_all = new QRadioButton(tr("Em todo o modelo"));
    (selectedNodes + selectedBars > 0 ? m_selection : m_all)->setChecked(true);
    scopeLayout->addWidget(m_selection);
    scopeLayout->addWidget(m_all);
    auto *form = new QFormLayout;
    m_case = new QComboBox;
    m_case->addItem(tr("Todos os casos"), QString());
    for (const QString &name : caseNames) {
        m_case->addItem(name, name);
    }
    form->addRow(tr("Caso de carga"), m_case);
    scopeLayout->addLayout(form);
    root->addWidget(scope);

    auto *note = new QLabel(tr("A exclusão pode ser desfeita com Ctrl+Z."));
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #7a4b00;"));
    root->addWidget(note);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    auto *ok = buttons->addButton(tr("Excluir"), QDialogButtonBox::AcceptRole);
    ok->setStyleSheet(QStringLiteral("QPushButton { background: #c0392b; color: white; font-weight: 600; padding: 5px 14px; border-radius: 4px; }"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

bool DeleteLoadsDialog::nodalForces() const { return m_nodal->isChecked(); }
bool DeleteLoadsDialog::distributedForces() const { return m_distributed->isChecked(); }
bool DeleteLoadsDialog::distributedMoments() const { return m_moments->isChecked(); }
bool DeleteLoadsDialog::areaPanels() const { return m_panels->isChecked(); }
QString DeleteLoadsDialog::loadCase() const { return m_case->currentData().toString(); }
bool DeleteLoadsDialog::wholeModel() const { return m_all->isChecked(); }

} // namespace Structura::UI
