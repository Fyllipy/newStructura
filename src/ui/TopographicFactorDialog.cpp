#include "TopographicFactorDialog.h"

#include "app/services/WindStaticService.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::Services::WindStaticService;

TopographicFactorDialog::TopographicFactorDialog(double current, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Fator topográfico S1 (NBR 6123:2023, 5.2)"));
    auto *root = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    m_kind = new QComboBox(this);
    m_kind->addItem(tr("Terreno plano ou fracamente acidentado: S1 = 1,0"), 0);
    m_kind->addItem(tr("Talude ou morro, no ponto B (topo): S1(z)"), 1);
    m_kind->addItem(tr("Vale profundo, protegido de ventos de qualquer direção: S1 = 0,9"), 2);
    auto spin = [this](double min, double max, double value, const QString &suffix) {
        auto *s = new QDoubleSpinBox(this);
        s->setRange(min, max);
        s->setDecimals(2);
        s->setValue(value);
        s->setSuffix(suffix);
        return s;
    };
    m_theta = spin(0.0, 90.0, 10.0, tr(" °"));
    m_theta->setToolTip(tr("Inclinação média do talude ou da encosta do morro"));
    m_dt = spin(0.1, 10000.0, 20.0, tr(" m"));
    m_dt->setToolTip(tr("Diferença de nível entre a base e o topo do talude ou morro"));
    m_z = spin(0.0, 1000.0, 10.0, tr(" m"));
    m_z->setToolTip(tr("Altura acima do terreno no ponto B em que S2 é calculado (use a cota de referência da edificação)"));
    form->addRow(tr("Situação"), m_kind);
    form->addRow(tr("Inclinação θt"), m_theta);
    form->addRow(tr("Desnível dt"), m_dt);
    form->addRow(tr("Altura z"), m_z);
    root->addLayout(form);
    m_result = new QLabel(this);
    m_result->setStyleSheet(QStringLiteral("font-weight:600;"));
    root->addWidget(m_result);
    auto *note = new QLabel(tr("Procedimento da norma para taludes e morros alongados com escoamento bidimensional; entre os pontos A e B e entre B e C interpola-se linearmente (S1 = 1 em A e C). "
                               "Primeira aproximação: use com precaução e, em relevos complexos, ensaie em túnel de vento."), this);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color:#5a6472;"));
    root->addWidget(note);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);
    connect(m_kind, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { update(); });
    for (auto *s : {m_theta, m_dt, m_z}) {
        connect(s, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this]() { update(); });
    }
    if (current < 0.95 && current > 0.85) {
        m_kind->setCurrentIndex(2);
    }
    update();
}

double TopographicFactorDialog::s1() const
{
    switch (m_kind->currentData().toInt()) {
    case 1: return WindStaticService::s1Topographic(m_theta->value(), m_z->value(), m_dt->value());
    case 2: return 0.9;
    default: return 1.0;
    }
}

void TopographicFactorDialog::update()
{
    const bool slope = m_kind->currentData().toInt() == 1;
    for (auto *s : {m_theta, m_dt, m_z}) {
        s->setEnabled(slope);
    }
    m_result->setText(tr("S1 = %1").arg(s1(), 0, 'f', 3));
}

} // namespace Structura::UI
