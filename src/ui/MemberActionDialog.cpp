#include "MemberActionDialog.h"

#include "app/LoadSystemUtils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <algorithm>

namespace {
namespace Kind = Structura::App::ProjectData::MemberActionKind;
constexpr double kRange = 1e9;

/// Rotulo da linha do formulario; addRow com texto vazio nao cria QLabel, entao o ponteiro pode ser nulo.
void setRowLabel(QFormLayout *form, QWidget *field, const QString &text)
{
    if (auto *label = qobject_cast<QLabel *>(form->labelForField(field))) {
        label->setText(text);
    }
}
} // namespace

MemberActionDialog::MemberActionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Carga de barra com posição"));
    setModal(true);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    m_selectionLabel = new QLabel(tr("Nenhuma barra selecionada"), this);
    m_selectionLabel->setStyleSheet(QStringLiteral("font-weight: 500; color: #1f2530;"));
    root->addWidget(m_selectionLabel);

    m_form = new QFormLayout();
    m_form->setLabelAlignment(Qt::AlignLeft);
    m_form->setSpacing(6);

    m_kindCombo = new QComboBox(this);
    m_kindCombo->addItem(tr("Trapezoidal / parcial (força por comprimento)"), Kind::kTrapezoid);
    m_kindCombo->addItem(tr("Força concentrada"), Kind::kPointForce);
    m_kindCombo->addItem(tr("Momento concentrado"), Kind::kPointMoment);
    m_kindCombo->addItem(tr("Temperatura (uniforme e gradiente)"), Kind::kTemperature);
    m_kindCombo->addItem(tr("Retração (deformação uniforme)"), Kind::kShrinkage);
    m_form->addRow(tr("Tipo"), m_kindCombo);

    m_caseCombo = new QComboBox(this);
    m_form->addRow(tr("Caso de carga"), m_caseCombo);

    m_systemCombo = new QComboBox(this);
    m_systemCombo->addItem(tr("Global"), QStringLiteral("GLOBAL"));
    m_systemCombo->addItem(tr("Local"), QStringLiteral("LOCAL"));
    m_form->addRow(tr("Sistema"), m_systemCombo);

    m_percent = new QCheckBox(tr("Posições em % do comprimento da barra"), this);
    m_form->addRow(QString(), m_percent);

    auto spin = [this](int decimals) {
        auto *s = new QDoubleSpinBox(this);
        s->setDecimals(decimals);
        s->setRange(-kRange, kRange);
        s->setSingleStep(0.1);
        s->setAccelerated(true);
        return s;
    };
    m_aSpin = spin(3);
    m_bSpin = spin(3);
    m_aSpin->setRange(0.0, kRange);
    m_bSpin->setRange(0.0, kRange);
    m_form->addRow(tr("Posição inicial a (m, do nó I)"), m_aSpin);
    m_form->addRow(tr("Posição final b (m, do nó I)"), m_bSpin);
    for (int i = 0; i < 3; ++i) {
        m_v1[static_cast<std::size_t>(i)] = spin(4);
        m_form->addRow(QStringLiteral(" "), m_v1[static_cast<std::size_t>(i)]);
    }
    for (int i = 0; i < 3; ++i) {
        m_v2[static_cast<std::size_t>(i)] = spin(4);
        m_form->addRow(QStringLiteral(" "), m_v2[static_cast<std::size_t>(i)]);
    }
    root->addLayout(m_form);

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    m_hint->setStyleSheet(QStringLiteral("color: #5a6472;"));
    root->addWidget(m_hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &MemberActionDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &MemberActionDialog::reject);
    root->addWidget(buttons);

    connect(m_kindCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { updateForKind(); });
    connect(m_percent, &QCheckBox::toggled, this, [this](bool percent) {
        const double reference = percent ? 100.0 : (m_length > 0.0 ? m_length : 1e9);
        const double factor = m_length > 0.0 ? (percent ? 100.0 / m_length : m_length / 100.0) : 1.0;
        for (QDoubleSpinBox *s : {m_aSpin, m_bSpin}) {
            s->setRange(0.0, reference);
            s->setValue(std::min(s->value() * factor, reference));
        }
        updateForKind();
    });
    updateForKind();
    resize(460, sizeHint().height());
}

int MemberActionDialog::kind() const
{
    return m_kindCombo->currentData().toInt();
}

void MemberActionDialog::updateForKind()
{
    const int k = kind();
    const bool trapezoid = k == Kind::kTrapezoid;
    const bool point = k == Kind::kPointForce || k == Kind::kPointMoment;
    const bool thermal = k == Kind::kTemperature;
    const bool shrink = k == Kind::kShrinkage;
    const bool positioned = trapezoid || point;
    const QString unit = m_percent->isChecked() ? QStringLiteral("%") : QStringLiteral("m");

    m_form->setRowVisible(m_systemCombo, !(thermal || shrink));
    m_form->setRowVisible(m_percent, positioned);
    m_form->setRowVisible(m_aSpin, positioned);
    m_form->setRowVisible(m_bSpin, trapezoid);
    setRowLabel(m_form, m_aSpin, point ? tr("Posição a (%1, do nó I)").arg(unit) : tr("Posição inicial a (%1, do nó I)").arg(unit));
    setRowLabel(m_form, m_bSpin, tr("Posição final b (%1, do nó I)").arg(unit));

    QStringList first, second;
    if (trapezoid) {
        first = {tr("qx em a (kN/m)"), tr("qy em a (kN/m)"), tr("qz em a (kN/m)")};
        second = {tr("qx em b (kN/m)"), tr("qy em b (kN/m)"), tr("qz em b (kN/m)")};
        m_hint->setText(tr("Carga linearmente variável entre a e b (a = 0 e b = comprimento: carga em toda a barra; valores iguais: parcial uniforme). "
                           "Os valores são forças por comprimento no sistema escolhido."));
    } else if (k == Kind::kPointForce) {
        first = {tr("Fx (kN)"), tr("Fy (kN)"), tr("Fz (kN)")};
        m_hint->setText(tr("Força concentrada na posição a. Em a = 0 ou a = comprimento a carga vai direto ao nó."));
    } else if (k == Kind::kPointMoment) {
        first = {tr("Mx (kN·m)"), tr("My (kN·m)"), tr("Mz (kN·m)")};
        m_hint->setText(tr("Momento concentrado na posição a (Mx = torque). Torque pontual não é combinado com torção com empenamento."));
    } else if (thermal) {
        first = {tr("ΔT uniforme (°C)"), tr("Gradiente em y local (°C/m)"), tr("Gradiente em z local (°C/m)")};
        m_hint->setText(tr("Variação de temperatura em relação à de montagem, em toda a barra. O gradiente é a variação de temperatura por metro "
                           "de altura medida segundo o eixo local indicado (fibra +y/+z mais quente: gradiente positivo). Usa o coeficiente de dilatação α do material."));
    } else {
        first = {tr("ε de retração (‰)"), QString(), QString()};
        m_hint->setText(tr("Deformação livre uniforme (negativa para encurtamento), em ‰. Barra com os apoios impedindo o encurtamento fica tracionada."));
    }
    for (int i = 0; i < 3; ++i) {
        const std::size_t u = static_cast<std::size_t>(i);
        const bool showFirst = !first.value(i).isEmpty();
        const bool showSecond = trapezoid;
        m_form->setRowVisible(m_v1[u], showFirst);
        m_form->setRowVisible(m_v2[u], showSecond);
        if (showFirst) {
            setRowLabel(m_form, m_v1[u], first.value(i));
        }
        if (showSecond) {
            setRowLabel(m_form, m_v2[u], second.value(i));
        }
    }
    if (m_length > 0.0 && positioned) {
        m_hint->setText(m_hint->text() + tr("  Comprimento da barra: %1 m.").arg(m_length, 0, 'f', 3));
    }
}

void MemberActionDialog::setSelectedCount(int count)
{
    if (count <= 0) {
        m_selectionLabel->setText(tr("Nenhuma barra selecionada"));
    } else if (count == 1) {
        m_selectionLabel->setText(tr("1 barra selecionada"));
    } else {
        m_selectionLabel->setText(tr("%1 barras selecionadas").arg(count));
    }
}

void MemberActionDialog::setLoadCases(const QStringList &names, const QString &current)
{
    m_caseCombo->clear();
    m_caseCombo->addItems(names);
    const int index = m_caseCombo->findText(current);
    m_caseCombo->setCurrentIndex(index >= 0 ? index : 0);
}

QString MemberActionDialog::loadCase() const
{
    return m_caseCombo->currentText();
}

void MemberActionDialog::setMemberLength(double length)
{
    m_length = length;
    if (!m_percent->isChecked()) {
        const double limit = length > 0.0 ? length : 1e9;
        m_aSpin->setRange(0.0, limit);
        m_bSpin->setRange(0.0, limit);
        if (length > 0.0 && m_bSpin->value() == 0.0) {
            m_bSpin->setValue(length);
        }
    }
    updateForKind();
}

void MemberActionDialog::setInitial(const Structura::App::ProjectData::MemberAction &action)
{
    const int index = m_kindCombo->findData(action.kind);
    m_kindCombo->setCurrentIndex(index >= 0 ? index : 0);
    m_kindCombo->setEnabled(false);
    const int systemIndex = m_systemCombo->findData(Structura::App::normalizeMemberLoadSystem(action.system, QStringLiteral("LOCAL")));
    m_systemCombo->setCurrentIndex(systemIndex >= 0 ? systemIndex : 0);
    m_aSpin->setValue(action.a);
    m_bSpin->setValue(action.b);
    for (int i = 0; i < 3; ++i) {
        const std::size_t u = static_cast<std::size_t>(i);
        double v1 = action.v1[u];
        if (action.kind == Kind::kShrinkage && i == 0) {
            v1 *= 1000.0;
        }
        m_v1[u]->setValue(v1);
        m_v2[u]->setValue(action.v2[u]);
    }
}

MemberActionDialog::Values MemberActionDialog::values() const
{
    Values result;
    result.kind = kind();
    result.system = (result.kind == Kind::kTemperature || result.kind == Kind::kShrinkage) ? QStringLiteral("LOCAL")
                                                                                              : m_systemCombo->currentData().toString();
    result.percent = m_percent->isChecked() && (result.kind <= Kind::kPointMoment);
    result.a = m_aSpin->value();
    result.b = result.kind == Kind::kTrapezoid ? m_bSpin->value() : result.a;
    for (int i = 0; i < 3; ++i) {
        const std::size_t u = static_cast<std::size_t>(i);
        result.v1[u] = m_v1[u]->value();
        result.v2[u] = result.kind == Kind::kTrapezoid ? m_v2[u]->value() : 0.0;
    }
    if (result.kind == Kind::kShrinkage) {
        result.v1 = {{m_v1[0]->value() * 1e-3, 0.0, 0.0}};
    }
    if (result.kind == Kind::kTemperature || result.kind == Kind::kShrinkage) {
        result.a = 0.0;
        result.b = 0.0;
    }
    return result;
}
