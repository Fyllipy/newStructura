#include "MaterialDialog.h"

#include "app/services/MaterialLibrary.h"

#include <QDialogButtonBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

using Structura::App::Services::MaterialLibrary;

namespace {
constexpr int kLibraryRole = Qt::UserRole + 1;
}

MaterialDialog::MaterialDialog(bool pascalUnits, QWidget *parent)
    : QDialog(parent)
    , m_pascal(pascalUnits)
{
    setWindowTitle(tr("Novo material"));
    setModal(true);
    resize(760, 460);

    auto *root = new QHBoxLayout(this);

    // ── Biblioteca ────────────────────────────────────────────────────
    auto *left = new QVBoxLayout;
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Buscar na biblioteca (ex.: A572, ZAR, C30)"));
    m_search->setClearButtonEnabled(true);
    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({tr("Material"), tr("fy / fck"), tr("fu")});
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);

    auto *custom = new QTreeWidgetItem(m_tree, {tr("Personalizado"), QString(), QString()});
    custom->setData(0, kLibraryRole, QString());
    QFont bold = custom->font(0);
    bold.setBold(true);
    custom->setFont(0, bold);
    QTreeWidgetItem *firstSteel = nullptr;
    for (const auto category : {MaterialLibrary::Category::StructuralSteel, MaterialLibrary::Category::ColdFormedSteel, MaterialLibrary::Category::Concrete, MaterialLibrary::Category::Custom}) {
        auto *group = new QTreeWidgetItem(m_tree, {MaterialLibrary::categoryName(category), QString(), QString()});
        group->setFlags(group->flags() & ~Qt::ItemIsSelectable);
        group->setFont(0, bold);
        group->setExpanded(true);
        for (const auto &entry : MaterialLibrary::entries()) {
            if (entry.category != category) {
                continue;
            }
            if (category == MaterialLibrary::Category::Custom && !entry.user) {
                continue;
            }
            auto *item = new QTreeWidgetItem(group, {entry.name, QString::number(entry.fy, 'f', 0), entry.isSteel() ? QString::number(entry.fu, 'f', 0) : QString()});
            item->setData(0, kLibraryRole, entry.name);
            item->setToolTip(0, entry.note);
            if (!firstSteel) {
                firstSteel = item;
            }
        }
    }
    left->addWidget(m_search);
    left->addWidget(m_tree, 1);
    root->addLayout(left, 5);

    // ── Propriedades ──────────────────────────────────────────────────
    auto *right = new QVBoxLayout;
    auto *form = new QFormLayout;
    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText(tr("Nome do material"));
    auto makeSpin = [](double min, double max, double value, int decimals, const QString &suffix) {
        auto *s = new QDoubleSpinBox;
        s->setRange(min, max);
        s->setDecimals(decimals);
        s->setValue(value);
        s->setSuffix(suffix);
        return s;
    };
    m_eSpin = makeSpin(0.001, 1000.0, 200.0, 2, tr(" GPa"));
    m_gSpin = makeSpin(0.001, 1000.0, 77.0, 2, tr(" GPa"));
    m_fySpin = makeSpin(0.0, 2000.0, 345.0, 1, tr(" MPa"));
    m_fuSpin = makeSpin(0.0, 3000.0, 450.0, 1, tr(" MPa"));
    m_weightSpin = makeSpin(0.0, 1e5, 78.5, 2, tr(" kN/m³"));
    m_weightSpin->setToolTip(tr("Usado no peso próprio: γ × área da seção"));
    form->addRow(tr("Nome"), m_nameEdit);
    form->addRow(tr("Módulo de elasticidade E"), m_eSpin);
    form->addRow(tr("Módulo de cisalhamento G"), m_gSpin);
    form->addRow(tr("Resistência ao escoamento fy / fck"), m_fySpin);
    form->addRow(tr("Resistência à ruptura fu"), m_fuSpin);
    form->addRow(tr("Peso específico γ"), m_weightSpin);
    right->addLayout(form);
    m_note = new QLabel;
    m_note->setWordWrap(true);
    m_note->setStyleSheet(QStringLiteral("color: #5a6472;"));
    right->addWidget(m_note);
    m_unitsNote = new QLabel(m_pascal ? tr("O projeto usa E em Pa (força em N): os valores serão convertidos.")
                                      : tr("O projeto usa kN e m: E e G serão gravados em kN/m²."));
    m_unitsNote->setWordWrap(true);
    m_unitsNote->setStyleSheet(QStringLiteral("color: #5a6472;"));
    right->addWidget(m_unitsNote);
    m_saveToLibrary = new QCheckBox(tr("Salvar no banco de materiais do programa (disponível em todos os projetos)"));
    m_saveToLibrary->setChecked(false);
    right->addWidget(m_saveToLibrary);
    right->addStretch(1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &MaterialDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &MaterialDialog::reject);
    right->addWidget(buttons);
    root->addLayout(right, 4);

    connect(m_search, &QLineEdit::textChanged, this, &MaterialDialog::applyFilter);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
        if (item && item->parent() == nullptr && item->childCount() > 0) {
            return;
        }
        if (item) {
            selectEntry(item->data(0, kLibraryRole).toString());
        }
    });
    if (firstSteel) {
        m_tree->setCurrentItem(firstSteel);
    }
}

void MaterialDialog::selectEntry(const QString &libraryName)
{
    m_library = libraryName;
    const auto *entry = libraryName.isEmpty() ? nullptr : MaterialLibrary::find(libraryName);
    if (entry) {
        m_nameEdit->setText(entry->name);
        m_eSpin->setValue(entry->eGPa);
        m_gSpin->setValue(entry->gGPa);
        m_fySpin->setValue(entry->fy);
        m_fuSpin->setValue(entry->fu);
        m_weightSpin->setValue(entry->unitWeight);
        m_note->setText(entry->note);
    } else {
        m_note->setText(tr("Material personalizado: informe as propriedades. fy/fu = 0 indica que o material não é um aço de dimensionamento."));
    }
    updateFormEnabled();
    if (m_saveToLibrary) {
        m_saveToLibrary->setChecked(m_library.isEmpty());   // material personalizado: sugere salvar
    }
}

void MaterialDialog::updateFormEnabled()
{
    const auto *entry = m_library.isEmpty() ? nullptr : MaterialLibrary::find(m_library);
    const bool concrete = entry && !entry->isSteel();
    m_fuSpin->setEnabled(!concrete);
    m_fySpin->setToolTip(concrete ? tr("fck do concreto") : tr("fy do aço"));
}

void MaterialDialog::applyFilter(const QString &text)
{
    const QString needle = text.trimmed();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto *group = m_tree->topLevelItem(i);
        if (group->childCount() == 0) {
            continue;
        }
        bool any = false;
        for (int j = 0; j < group->childCount(); ++j) {
            auto *child = group->child(j);
            const bool match = needle.isEmpty() || child->text(0).contains(needle, Qt::CaseInsensitive);
            child->setHidden(!match);
            any = any || match;
        }
        group->setHidden(!any);
        group->setExpanded(true);
    }
}

QString MaterialDialog::name() const
{
    return m_nameEdit->text().trimmed();
}

double MaterialDialog::youngModulus() const
{
    return m_eSpin->value() * unitFactor();
}

double MaterialDialog::shearModulus() const
{
    return m_gSpin->value() * unitFactor();
}

double MaterialDialog::unitWeight() const
{
    return m_weightSpin->value();
}

double MaterialDialog::fy() const
{
    return m_fySpin->value();
}

double MaterialDialog::fu() const
{
    return m_fuSpin->isEnabled() ? m_fuSpin->value() : 0.0;
}

bool MaterialDialog::saveToLibrary() const
{
    return m_saveToLibrary && m_saveToLibrary->isChecked();
}

double MaterialDialog::youngModulusGPa() const
{
    return m_eSpin->value();
}

double MaterialDialog::shearModulusGPa() const
{
    return m_gSpin->value();
}
