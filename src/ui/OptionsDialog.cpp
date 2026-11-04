#include "OptionsDialog.h"

#include "app/AppSettings.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTabWidget>
#include <QVBoxLayout>

namespace Structura::UI {

OptionsDialog::OptionsDialog(double nodeScreenSize, bool startDark, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Opções"));
    auto *root = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    root->addWidget(tabs);

    auto *visual = new QWidget(tabs);
    auto *form = new QFormLayout(visual);
    m_theme = new QComboBox(visual);
    m_theme->addItem(tr("Modo claro"), false);
    m_theme->addItem(tr("Modo escuro"), true);
    m_theme->setCurrentIndex(startDark ? 1 : 0);
    form->addRow(tr("Tema:"), m_theme);
    connect(m_theme, &QComboBox::currentIndexChanged, this, [this]() { emit darkModeChanged(this->darkMode()); });
    m_nodeSize = new QSlider(Qt::Horizontal, visual);
    m_nodeSize->setRange(static_cast<int>(Structura::App::Options::kNodeScreenSizeMin), static_cast<int>(Structura::App::Options::kNodeScreenSizeMax));
    m_nodeSize->setValue(static_cast<int>(nodeScreenSize + 0.5));
    m_nodeSize->setMinimumWidth(240);
    m_nodeSizeValue = new QLabel(visual);
    m_nodeSizeValue->setMinimumWidth(48);
    auto *row = new QHBoxLayout;
    row->addWidget(m_nodeSize, 1);
    row->addWidget(m_nodeSizeValue);
    form->addRow(tr("Tamanho do nó na tela:"), row);
    auto *hint = new QLabel(tr("Raio do círculo que representa cada nó, em pixels (o tamanho não muda com o zoom)."), visual);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#59636f; font-size:8.5pt;"));
    form->addRow(hint);
    auto *reset = new QPushButton(tr("Restaurar padrão"), visual);
    connect(reset, &QPushButton::clicked, this, [this]() { m_nodeSize->setValue(static_cast<int>(Structura::App::Options::kNodeScreenSizeDefault)); });
    form->addRow(reset);
    tabs->addTab(visual, tr("Opções visuais"));

    connect(m_nodeSize, &QSlider::valueChanged, this, [this](int value) {
        m_nodeSizeValue->setText(tr("%1 px").arg(value));
        emit nodeScreenSizeChanged(value);
    });
    m_nodeSizeValue->setText(tr("%1 px").arg(m_nodeSize->value()));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("OK"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

double OptionsDialog::nodeScreenSize() const
{
    return m_nodeSize->value();
}

bool OptionsDialog::darkMode() const
{
    return m_theme->currentData().toBool();
}

} // namespace Structura::UI
