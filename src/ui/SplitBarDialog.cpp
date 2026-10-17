#include "SplitBarDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace Structura::UI {

SplitBarDialog::SplitBarDialog(int barCount, double shortest, double longest, QWidget *parent)
    : QDialog(parent)
    , m_barCount(barCount)
    , m_shortest(shortest)
    , m_longest(longest)
{
    setWindowTitle(tr("Dividir barras"));
    auto *root = new QVBoxLayout(this);
    root->addWidget(new QLabel(tr("Cria nós nas posições indicadas e divide cada barra selecionada (%1) em trechos. Perfil, cargas uniformes, "
                                  "grupos e articulações das extremidades são mantidos.").arg(barCount)));
    static_cast<QLabel *>(root->itemAt(0)->widget())->setWordWrap(true);

    m_equal = new QRadioButton(tr("Em N partes iguais"));
    m_equal->setChecked(true);
    m_parts = new QSpinBox;
    m_parts->setRange(2, 200);
    m_parts->setValue(2);
    m_every = new QRadioButton(tr("A cada distância fixa"));
    m_step = new QDoubleSpinBox;
    m_step->setRange(0.01, 1000.0);
    m_step->setDecimals(3);
    m_step->setValue(1.0);
    m_step->setSuffix(tr(" m"));
    m_list = new QRadioButton(tr("Em distâncias informadas"));
    m_distances = new QLineEdit;
    m_distances->setPlaceholderText(tr("ex.: 1,5; 3,0; 4,25  (m)"));
    m_fromEnd = new QCheckBox(tr("medir a partir do fim da barra"));

    auto *form = new QFormLayout;
    form->addRow(m_equal, m_parts);
    form->addRow(m_every, m_step);
    form->addRow(m_list, m_distances);
    form->addRow(QString(), m_fromEnd);
    root->addLayout(form);
    m_preview = new QLabel;
    m_preview->setWordWrap(true);
    m_preview->setStyleSheet(QStringLiteral("color:#5a6472;"));
    root->addWidget(m_preview);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    for (QRadioButton *radio : {m_equal, m_every, m_list}) {
        connect(radio, &QRadioButton::toggled, this, [this]() { updatePreview(); });
    }
    connect(m_parts, &QSpinBox::valueChanged, this, [this]() { updatePreview(); });
    connect(m_step, &QDoubleSpinBox::valueChanged, this, [this]() { updatePreview(); });
    connect(m_distances, &QLineEdit::textChanged, this, [this]() { updatePreview(); });
    connect(m_fromEnd, &QCheckBox::toggled, this, [this]() { updatePreview(); });
    updatePreview();
}

std::vector<double> SplitBarDialog::fractions(double length) const
{
    std::vector<double> out;
    if (length <= 1e-9) {
        return out;
    }
    if (m_equal->isChecked()) {
        const int n = m_parts->value();
        for (int k = 1; k < n; ++k) {
            out.push_back(static_cast<double>(k) / n);
        }
    } else if (m_every->isChecked()) {
        const double step = m_step->value();
        for (double d = step; d < length - 1e-6; d += step) {
            out.push_back(m_fromEnd->isChecked() ? 1.0 - d / length : d / length);
        }
    } else {
        QString text = m_distances->text();
        text.replace(QLatin1Char(','), QLatin1Char('.'));
        for (const QString &token : text.split(QRegularExpression(QStringLiteral("[\\s;]+")), Qt::SkipEmptyParts)) {
            bool ok = false;
            const double d = token.toDouble(&ok);
            if (ok && d > 0.0 && d < length) {
                out.push_back(m_fromEnd->isChecked() ? 1.0 - d / length : d / length);
            }
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end(), [](double a, double b) { return std::abs(a - b) < 1e-9; }), out.end());
    return out;
}

void SplitBarDialog::updatePreview()
{
    if (!m_preview) {
        return;
    }
    m_parts->setEnabled(m_equal->isChecked());
    m_step->setEnabled(m_every->isChecked());
    m_distances->setEnabled(m_list->isChecked());
    m_fromEnd->setEnabled(!m_equal->isChecked());
    const auto cutsShort = fractions(m_shortest);
    const auto cutsLong = fractions(m_longest);
    m_preview->setText(m_barCount == 1 ? tr("Barra de %1 m: %2 corte(s) → %3 trechos.").arg(m_longest, 0, 'f', 3).arg(cutsLong.size()).arg(cutsLong.size() + 1)
                                       : tr("Barra mais curta (%1 m): %2 corte(s); mais longa (%3 m): %4 corte(s).")
                                             .arg(m_shortest, 0, 'f', 3).arg(cutsShort.size()).arg(m_longest, 0, 'f', 3).arg(cutsLong.size()));
}

} // namespace Structura::UI
