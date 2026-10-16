#include "CombinationGeneratorDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::Services::CombinationGenerator;
using Kind = CombinationGenerator::Kind;

namespace {
enum Column { Name, KindCol, GammaU, GammaF, Psi0, Psi1, Psi2, ColumnCount };

QDoubleSpinBox *coefficient(double value)
{
    auto *s = new QDoubleSpinBox;
    s->setRange(0.0, 3.0);
    s->setDecimals(2);
    s->setSingleStep(0.05);
    s->setValue(value);
    return s;
}
} // namespace

CombinationGeneratorDialog::CombinationGeneratorDialog(const QVector<Structura::App::ProjectData::LoadCase> &cases, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Gerar combinações (NBR 8681)"));
    resize(860, 520);
    auto *root = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Indique o tipo de cada caso. As combinações últimas normais seguem a NBR 8681 (γg·Fg + γq·Fq1 + Σγq·ψ0·Fqj); "
                                "as de serviço (ELS) usam ψ1 e ψ2. Casos de vento são mutuamente exclusivos. Coeficientes iniciais: NBR 8800:2024 tabelas 1 e 2 — "
                                "confira para o seu caso (ex.: ψ0 de sobrecarga muda com o tipo de ocupação)."));
    intro->setWordWrap(true);
    root->addWidget(intro);

    m_table = new QTableWidget(cases.size(), ColumnCount);
    m_table->setHorizontalHeaderLabels({tr("Caso"), tr("Tipo de ação"), tr("γ desfav."), tr("γ fav."), tr("ψ0"), tr("ψ1"), tr("ψ2")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    for (int row = 0; row < cases.size(); ++row) {
        const auto &c = cases[row];
        auto *nameItem = new QTableWidgetItem(c.name);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, Name, nameItem);
        auto *kind = new QComboBox;
        kind->addItem(tr("Peso próprio (aço)"), static_cast<int>(Kind::SteelSelfWeight));
        kind->addItem(tr("Permanente"), static_cast<int>(Kind::Permanent));
        kind->addItem(tr("Sobrecarga / variável"), static_cast<int>(Kind::Variable));
        kind->addItem(tr("Vento"), static_cast<int>(Kind::Wind));
        kind->addItem(tr("Temperatura"), static_cast<int>(Kind::Temperature));
        m_table->setCellWidget(row, KindCol, kind);
        for (int col = GammaU; col <= Psi2; ++col) {
            m_table->setCellWidget(row, col, coefficient(0.0));
        }
        const Kind guessed = CombinationGenerator::guessKind(c.name, c.selfWeight);
        kind->setCurrentIndex(kind->findData(static_cast<int>(guessed)));
        applyKindDefaults(row);
        connect(kind, &QComboBox::currentIndexChanged, this, [this, row]() { applyKindDefaults(row); updatePreview(); });
        for (int col = GammaU; col <= Psi2; ++col) {
            connect(qobject_cast<QDoubleSpinBox *>(m_table->cellWidget(row, col)), &QDoubleSpinBox::valueChanged, this, [this]() { updatePreview(); });
        }
    }
    root->addWidget(m_table, 1);

    m_ultimate = new QCheckBox(tr("ELU – combinações últimas normais"));
    m_ultimate->setChecked(true);
    m_favorable = new QCheckBox(tr("ELU – permanentes favoráveis com vento dominante (arrancamento/sucção)"));
    m_favorable->setChecked(true);
    m_quasi = new QCheckBox(tr("ELS – quase permanente"));
    m_quasi->setChecked(true);
    m_frequent = new QCheckBox(tr("ELS – frequente (flechas)"));
    m_frequent->setChecked(true);
    m_rare = new QCheckBox(tr("ELS – rara"));
    m_replace = new QCheckBox(tr("Substituir as combinações atuais (desmarcado: acrescentar)"));
    m_replace->setChecked(true);
    for (auto *box : {m_ultimate, m_favorable, m_quasi, m_frequent, m_rare}) {
        root->addWidget(box);
        connect(box, &QCheckBox::toggled, this, [this]() { updatePreview(); });
    }
    root->addWidget(m_replace);
    m_preview = new QLabel;
    m_preview->setWordWrap(true);
    m_preview->setStyleSheet(QStringLiteral("color:#5a6472;"));
    root->addWidget(m_preview);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    buttons->addButton(tr("Gerar"), QDialogButtonBox::AcceptRole);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
    updatePreview();
}

void CombinationGeneratorDialog::applyKindDefaults(int row)
{
    auto *kind = qobject_cast<QComboBox *>(m_table->cellWidget(row, KindCol));
    const auto d = CombinationGenerator::defaults(m_table->item(row, Name)->text(), static_cast<Kind>(kind->currentData().toInt()));
    const double values[] = {d.gammaUnfavorable, d.gammaFavorable, d.psi0, d.psi1, d.psi2};
    for (int i = 0; i < 5; ++i) {
        auto *spin = qobject_cast<QDoubleSpinBox *>(m_table->cellWidget(row, GammaU + i));
        const QSignalBlocker blocker(spin);
        spin->setValue(values[i]);
    }
}

QVector<CombinationGenerator::CaseSetup> CombinationGeneratorDialog::setups() const
{
    QVector<CombinationGenerator::CaseSetup> out;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        CombinationGenerator::CaseSetup s;
        s.name = m_table->item(row, Name)->text();
        s.kind = static_cast<Kind>(qobject_cast<QComboBox *>(m_table->cellWidget(row, KindCol))->currentData().toInt());
        auto value = [&](int col) { return qobject_cast<QDoubleSpinBox *>(m_table->cellWidget(row, col))->value(); };
        s.gammaUnfavorable = value(GammaU);
        s.gammaFavorable = value(GammaF);
        s.psi0 = value(Psi0);
        s.psi1 = value(Psi1);
        s.psi2 = value(Psi2);
        out.append(s);
    }
    return out;
}

CombinationGenerator::Options CombinationGeneratorDialog::options() const
{
    CombinationGenerator::Options o;
    o.ultimate = m_ultimate->isChecked();
    o.favorablePermanents = m_favorable->isChecked();
    o.serviceQuasiPermanent = m_quasi->isChecked();
    o.serviceFrequent = m_frequent->isChecked();
    o.serviceRare = m_rare->isChecked();
    return o;
}

QVector<Structura::App::ProjectData::LoadCombination> CombinationGeneratorDialog::combinations() const
{
    return CombinationGenerator::generate(setups(), options());
}

bool CombinationGeneratorDialog::replaceExisting() const
{
    return m_replace->isChecked();
}

void CombinationGeneratorDialog::updatePreview()
{
    const auto combos = combinations();
    QStringList lines;
    for (const auto &c : combos) {
        QStringList terms;
        for (const auto &f : c.factors) {
            terms << QStringLiteral("%1·%2").arg(f.second, 0, 'g', 4).arg(f.first);
        }
        lines << QStringLiteral("%1 = %2").arg(c.name, terms.join(QStringLiteral(" + ")));
    }
    m_preview->setText(tr("%1 combinações: ").arg(combos.size()) + (lines.size() > 6 ? lines.mid(0, 6).join(QStringLiteral("; ")) + QStringLiteral("…") : lines.join(QStringLiteral("; "))));
}

} // namespace Structura::UI
