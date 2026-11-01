#include "EffectiveLengthDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::Services::EffectiveLengthService;

EffectiveLengthDialog::EffectiveLengthDialog(std::function<EffectiveLengthService::Output(const EffectiveLengthService::Options &)> recompute, double forceToKn, double lengthToM, QWidget *parent)
    : QDialog(parent)
    , m_recompute(std::move(recompute))
    , m_forceToKn(forceToKn)
    , m_lengthToM(lengthToM)
{
    setWindowTitle(tr("Comprimentos efetivos de flambagem"));
    resize(900, 560);
    auto *root = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Cada barra comprimida flamba no primeiro modo (menor λ) em que sua curvatura participa o suficiente; Ncr = λ·|N| e K = π·√(EI/Ncr)/L. "
                                "X = flexão em torno do eixo de maior inércia (plano da altura), Y = em torno do de menor inércia. "
                                "Valores calculados com os esforços axiais de primeira ordem da combinação; use modos suficientes para que as barras apareçam."), this);
    intro->setWordWrap(true);
    intro->setStyleSheet(QStringLiteral("color: #5a6472;"));
    root->addWidget(intro);

    auto *form = new QHBoxLayout;
    m_participation = new QDoubleSpinBox(this);
    m_participation->setRange(0.001, 1.0);
    m_participation->setDecimals(3);
    m_participation->setSingleStep(0.05);
    m_participation->setValue(0.10);
    m_participation->setToolTip(tr("Participação mínima da barra no modo: energia geométrica da barra / maior energia de barra no modo"));
    m_axialRatio = new QDoubleSpinBox(this);
    m_axialRatio->setRange(0.0, 1.0);
    m_axialRatio->setDecimals(3);
    m_axialRatio->setSingleStep(0.01);
    m_axialRatio->setValue(0.05);
    m_axialRatio->setToolTip(tr("Descarta barras com compressão menor que esta fração da maior compressão da combinação"));
    m_suspicious = new QCheckBox(tr("Aplicar também valores suspeitos (K < 0,3 ou K > 6)"), this);
    form->addWidget(new QLabel(tr("Participação mínima:"), this));
    form->addWidget(m_participation);
    form->addWidget(new QLabel(tr("|N| mínimo (fração do máximo):"), this));
    form->addWidget(m_axialRatio);
    form->addWidget(m_suspicious);
    form->addStretch(1);
    root->addLayout(form);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setTextFormat(Qt::RichText);
    root->addWidget(m_summary);

    m_table = new QTableWidget(0, 11, this);
    m_table->setHorizontalHeaderLabels({tr("Barra"), tr("L grupo (m)"), tr("N (kN)"), tr("Kx"), tr("Modo x"), tr("λ x"), tr("Partic. x"), tr("Ky"), tr("Modo y"), tr("λ y"), tr("Partic. y")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    root->addWidget(m_table, 1);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *apply = box->addButton(tr("Aplicar Kx e Ky às barras"), QDialogButtonBox::AcceptRole);
    apply->setToolTip(tr("Grava os coeficientes nos parâmetros de dimensionamento de cada barra (lx e ly informados ficam zerados)"));
    connect(box, &QDialogButtonBox::accepted, this, [this]() {
        m_apply.clear();
        for (const auto &m : m_output.members) {
            Coefficients c;
            c.memberId = m.memberId;
            c.hasX = m.x.found && (m_suspicious->isChecked() || !m.x.suspicious);
            c.hasY = m.y.found && (m_suspicious->isChecked() || !m.y.suspicious);
            c.kx = m.x.k;
            c.ky = m.y.k;
            if (m.groupSize > 1) {
                // trechos de uma coluna dividida: o comprimento efetivo do grupo vale para cada trecho
                c.lx = c.hasX ? m.x.effectiveLength * m_lengthToM : 0.0;
                c.ly = c.hasY ? m.y.effectiveLength * m_lengthToM : 0.0;
            }
            if (c.hasX || c.hasY) {
                m_apply.append(c);
            }
        }
        accept();
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(box);

    auto *recalc = new QPushButton(tr("Recalcular"), this);
    form->insertWidget(form->count() - 1, recalc);
    connect(recalc, &QPushButton::clicked, this, [this]() { refresh(); });
    refresh();
}

void EffectiveLengthDialog::refresh()
{
    EffectiveLengthService::Options options;
    options.minParticipation = m_participation->value();
    options.minAxialRatio = m_axialRatio->value();
    m_output = m_recompute(options);

    QString summary;
    if (m_output.firstLambda > 0.0) {
        summary = tr("Menor fator crítico global λ1 = <b>%1</b>").arg(m_output.firstLambda, 0, 'f', 3);
        if (m_output.amplification > 0.0) {
            static const char *classes[3] = {"pequena", "média", "grande"};
            summary += tr("; amplificação nominal 1/(1 − 1/λ1) = <b>%1</b> → deslocabilidade <b>%2</b> (NBR 8800, 4.10.4: ≤ 1,10 pequena; ≤ 1,40 média).")
                           .arg(m_output.amplification, 0, 'f', 3).arg(QString::fromUtf8(classes[std::max(0, m_output.swayClass)]));
        } else {
            summary += tr(" — <span style='color:#c0392b'>estrutura instável sob as cargas da combinação</span>.");
        }
    }
    for (const QString &note : m_output.notes) {
        summary += QStringLiteral("<br>") + note.toHtmlEscaped();
    }
    m_summary->setText(summary);

    m_table->setRowCount(0);
    auto cell = [](const QString &text, bool warn = false) {
        auto *item = new QTableWidgetItem(text);
        if (warn) {
            item->setForeground(QColor(QStringLiteral("#c0392b")));
        }
        return item;
    };
    for (const auto &m : m_output.members) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, cell(QString::number(m.memberId)));
        m_table->setItem(row, 1, cell(QString::number(m.groupLength * m_lengthToM, 'f', 3) + (m.groupSize > 1 ? QStringLiteral(" (%1 trechos)").arg(m.groupSize) : QString())));
        m_table->setItem(row, 2, cell(QString::number(m.axial * m_forceToKn, 'f', 2)));
        auto fill = [&](int first, const EffectiveLengthService::PlaneResult &p) {
            if (!p.found) {
                for (int c = 0; c < 4; ++c) {
                    m_table->setItem(row, first + c, cell(QStringLiteral("—")));
                }
                return;
            }
            m_table->setItem(row, first, cell(QString::number(p.k, 'f', 3), p.suspicious));
            m_table->setItem(row, first + 1, cell(QString::number(p.mode)));
            m_table->setItem(row, first + 2, cell(QString::number(p.lambda, 'f', 3)));
            m_table->setItem(row, first + 3, cell(QString::number(p.participation, 'f', 2)));
        };
        fill(3, m.x);
        fill(7, m.y);
    }
}

} // namespace Structura::UI
