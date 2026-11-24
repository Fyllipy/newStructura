#include "ui/ShellSupportDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include <algorithm>

namespace Structura::UI {

namespace {
enum Preset { Fixed, Pinned, RollerZ, RollerX, RollerY, Spring, Custom };
}

ShellSupportDialog::ShellSupportDialog(int nodeCount, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Apoio nos nós de casca selecionados"));
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("O apoio será aplicado a %1 nó(s) selecionado(s) e gravado por coordenadas (acompanha a malha quando ela é refeita).").arg(nodeCount)));

    auto *form = new QFormLayout;
    m_preset = new QComboBox;
    m_preset->addItems({tr("Engaste (3 translações e 3 rotações)"), tr("Articulado (3 translações)"), tr("Apoio móvel: só Z restrito"),
                        tr("Apoio móvel: só X restrito"), tr("Apoio móvel: só Y restrito"), tr("Apoio elástico (mola)"), tr("Personalizado")});
    form->addRow(tr("Tipo"), m_preset);
    layout->addLayout(form);

    auto *dofBox = new QGroupBox(tr("Graus de liberdade restringidos"));
    auto *grid = new QGridLayout(dofBox);
    const QStringList names = {QStringLiteral("UX"), QStringLiteral("UY"), QStringLiteral("UZ"), QStringLiteral("RX"), QStringLiteral("RY"), QStringLiteral("RZ")};
    for (int i = 0; i < 6; ++i) {
        m_dof[static_cast<std::size_t>(i)] = new QCheckBox(names[i]);
        grid->addWidget(m_dof[static_cast<std::size_t>(i)], i / 3, i % 3);
        connect(m_dof[static_cast<std::size_t>(i)], &QCheckBox::toggled, this, [this]() {
            if (m_preset->currentIndex() != Custom && m_preset->currentIndex() != Spring) {
                const QSignalBlocker blocker(m_preset);
                m_preset->setCurrentIndex(Custom);
            }
        });
    }
    layout->addWidget(dofBox);

    m_springBox = new QGroupBox(tr("Mola"));
    auto *springForm = new QFormLayout(m_springBox);
    m_stiffness = new QDoubleSpinBox;
    m_stiffness->setRange(1e-3, 1e6);
    m_stiffness->setDecimals(3);
    m_stiffness->setValue(50.0);
    m_stiffness->setSuffix(tr(" MN/m³"));
    m_stiffness->setToolTip(tr("Rigidez por área: cada nó recebe a rigidez multiplicada pela sua área tributária da malha"));
    springForm->addRow(tr("Rigidez por área"), m_stiffness);
    m_compressionOnly = new QCheckBox(tr("Só comprime (fundação)"));
    springForm->addRow(m_compressionOnly);
    m_normal = new QComboBox;
    m_normal->addItems({tr("+Z (para cima)"), tr("−Z"), tr("+X"), tr("−X"), tr("+Y"), tr("−Y")});
    m_normal->setToolTip(tr("Sentido do apoio para a estrutura (a mola só reage a compressão nesse sentido)"));
    springForm->addRow(tr("Normal"), m_normal);
    layout->addWidget(m_springBox);

    auto *note = new QLabel(tr("Resultados já calculados deixam de valer: reanalise depois de aplicar."));
    note->setWordWrap(true);
    layout->addWidget(note);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_preset, &QComboBox::currentIndexChanged, this, [this](int i) { applyPreset(i); });
    connect(m_compressionOnly, &QCheckBox::toggled, this, [this]() { updateEnabled(); });
    applyPreset(Pinned);
    m_preset->setCurrentIndex(Pinned);
}

void ShellSupportDialog::applyPreset(int index)
{
    std::array<int, 6> dof {{0, 0, 0, 0, 0, 0}};
    switch (index) {
    case Fixed: dof = {{1, 1, 1, 1, 1, 1}}; break;
    case Pinned: dof = {{1, 1, 1, 0, 0, 0}}; break;
    case RollerZ: dof = {{0, 0, 1, 0, 0, 0}}; break;
    case RollerX: dof = {{1, 0, 0, 0, 0, 0}}; break;
    case RollerY: dof = {{0, 1, 0, 0, 0, 0}}; break;
    default: break;
    }
    if (index != Custom && index != Spring) {
        for (int i = 0; i < 6; ++i) {
            const QSignalBlocker blocker(m_dof[static_cast<std::size_t>(i)]);
            m_dof[static_cast<std::size_t>(i)]->setChecked(dof[static_cast<std::size_t>(i)] != 0);
        }
    }
    updateEnabled();
}

void ShellSupportDialog::updateEnabled()
{
    const bool spring = m_preset->currentIndex() == Spring;
    m_springBox->setEnabled(spring);
    m_normal->setEnabled(spring && m_compressionOnly->isChecked());
    for (QCheckBox *c : m_dof) {
        c->setEnabled(!spring);
    }
}

Structura::Shell::Support ShellSupportDialog::support() const
{
    Structura::Shell::Support s;
    s.fixed = {{0, 0, 0, 0, 0, 0}};
    if (m_preset->currentIndex() == Spring) {
        s.springPerArea = m_stiffness->value() * 1.0e6;
        s.compressionOnly = m_compressionOnly->isChecked();
        static const std::array<std::array<double, 3>, 6> normals {{{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}}};
        s.springNormal = normals[static_cast<std::size_t>(std::clamp(m_normal->currentIndex(), 0, 5))];
    } else {
        for (int i = 0; i < 6; ++i) {
            s.fixed[static_cast<std::size_t>(i)] = m_dof[static_cast<std::size_t>(i)]->isChecked() ? 1 : 0;
        }
    }
    return s;
}

} // namespace Structura::UI
