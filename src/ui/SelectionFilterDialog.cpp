#include "SelectionFilterDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <cmath>

namespace Structura::UI {

SelectionFilterDialog::SelectionFilterDialog(const SelectionFilter &current,
                                             const QVector<Option> &levels,
                                             const QVector<Option> &sections,
                                             const QVector<Option> &materials,
                                             QWidget *parent)
    : QDialog(parent)
    , m_level(new QComboBox(this))
    , m_section(new QComboBox(this))
    , m_material(new QComboBox(this))
{
    setWindowTitle(tr("Filtros de selecao"));
    setModal(true);

    m_level->addItem(tr("(todos os niveis)"), QVariant());
    for (const Option &option : levels) {
        m_level->addItem(option.text, option.value);
        if (current.levelEnabled && std::abs(option.value - current.level) < 1e-6) {
            m_level->setCurrentIndex(m_level->count() - 1);
        }
    }
    m_section->addItem(tr("(todas as secoes)"), QUuid());
    for (const Option &option : sections) {
        m_section->addItem(option.text, option.id);
        if (option.id == current.section) {
            m_section->setCurrentIndex(m_section->count() - 1);
        }
    }
    m_material->addItem(tr("(todos os materiais)"), QUuid());
    for (const Option &option : materials) {
        m_material->addItem(option.text, option.id);
        if (option.id == current.material) {
            m_material->setCurrentIndex(m_material->count() - 1);
        }
    }

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Os filtros valem para a selecao por janela e para\n"
                                    "\"Selecionar tudo que atende ao filtro\".\n"
                                    "Filtros de secao/material consideram apenas barras."), this));
    auto *form = new QFormLayout();
    form->addRow(tr("Nivel (altura Z)"), m_level);
    form->addRow(tr("Secao"), m_section);
    form->addRow(tr("Material"), m_material);
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

SelectionFilter SelectionFilterDialog::filter() const
{
    SelectionFilter result;
    const QVariant level = m_level->currentData();
    result.levelEnabled = level.isValid() && !level.isNull() && m_level->currentIndex() > 0;
    result.level = result.levelEnabled ? level.toDouble() : 0.0;
    result.section = m_section->currentData().toUuid();
    result.material = m_material->currentData().toUuid();
    return result;
}

} // namespace Structura::UI
