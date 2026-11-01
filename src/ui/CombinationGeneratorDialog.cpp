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
enum Column { Name, KindCol, SituationCol, GammaU, GammaF, GammaS, GammaE, Psi0, Psi1, Psi2, ColumnCount };

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
    resize(1080, 580);
    auto *root = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Indique o tipo de cada caso. As combinações últimas normais seguem a NBR 8681 (γg·Fg + γq·Fq1 + Σγq·ψ0·Fqj); "
                                "as de serviço (ELS) usam ψ1 e ψ2. Casos de vento são mutuamente exclusivos. Coeficientes iniciais: NBR 8800:2024 tabelas 1 e 2 — "
                                "confira para o seu caso (ex.: ψ0 de sobrecarga muda com o tipo de ocupação)."));
    intro->setWordWrap(true);
    root->addWidget(intro);

    m_table = new QTableWidget(cases.size(), ColumnCount);
    m_table->setHorizontalHeaderLabels({tr("Caso"), tr("Tipo de ação"), tr("Situação"), tr("γ desfav."), tr("γ fav."), tr("γ esp./constr."), tr("γ excep."), tr("ψ0"), tr("ψ1"), tr("ψ2")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    for (int row = 0; row < cases.size(); ++row) {
        const auto &c = cases[row];
        auto *nameItem = new QTableWidgetItem(c.name);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, Name, nameItem);
        auto *kind = new QComboBox;
        kind->addItem(tr("Peso próprio (aço)"), static_cast<int>(Kind::SteelSelfWeight));
        kind->addItem(tr("Permanente (moldado in loco / padrão)"), static_cast<int>(Kind::Permanent));
        kind->addItem(tr("Permanente (pré-moldado, madeira, industrializado)"), static_cast<int>(Kind::PermanentPrecast));
        kind->addItem(tr("Permanente (elementos construtivos em geral)"), static_cast<int>(Kind::PermanentGeneral));
        kind->addItem(tr("Permanente indireta (recalque, retração)"), static_cast<int>(Kind::Indirect));
        kind->addItem(tr("Sobrecarga – sem predominância de pesos fixos (ψ 0,5/0,4/0,3)"), static_cast<int>(Kind::Variable));
        kind->addItem(tr("Sobrecarga – industrial/comercial/público (ψ 0,7/0,6/0,4)"), static_cast<int>(Kind::VariableHeavy));
        kind->addItem(tr("Sobrecarga – biblioteca/depósito/garagem/cobertura (ψ 0,8/0,7/0,6)"), static_cast<int>(Kind::VariableStorage));
        kind->addItem(tr("Passarela de pedestres (ψ 0,6/0,4/0,3)"), static_cast<int>(Kind::Footbridge));
        kind->addItem(tr("Ponte rolante / carga móvel"), static_cast<int>(Kind::Crane));
        kind->addItem(tr("Vento"), static_cast<int>(Kind::Wind));
        kind->addItem(tr("Temperatura (atmosfera)"), static_cast<int>(Kind::Temperature));
        kind->addItem(tr("Temperatura gerada por equipamentos"), static_cast<int>(Kind::TemperatureEquipment));
        kind->addItem(tr("Ação excepcional (impacto, explosão, incêndio, sismo)"), static_cast<int>(Kind::Accidental));
        m_table->setCellWidget(row, KindCol, kind);
        auto *situation = new QComboBox;
        situation->addItem(tr("Normal"), 0);
        situation->addItem(tr("Especial"), 1);
        situation->addItem(tr("Especial (duração muito curta)"), 2);
        situation->addItem(tr("Construção"), 3);
        m_table->setCellWidget(row, SituationCol, situation);
        connect(situation, &QComboBox::currentIndexChanged, this, [this]() { updatePreview(); });
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
    m_special = new QCheckBox(tr("ELU – especiais e de construção (4.8.7.2.2/4.8.7.2.3): uma por ação marcada em \"Situação\""));
    m_special->setChecked(true);
    m_exceptional = new QCheckBox(tr("ELU – excepcionais (4.8.7.2.4): uma por ação excepcional; nomes EXC1, EXC2… (γa1 de combinação excepcional)"));
    m_exceptional->setChecked(true);
    m_fire = new QCheckBox(tr("Incêndio (NBR 14323, 6.3): FOGO1, FOGO2… com ψ de 0,21/0,28/0,42 nas sobrecargas e 0,20 no vento — usadas na verificação de incêndio do dimensionamento"));
    m_fire->setToolTip(tr("γg das permanentes pela Tabela 3 da NBR 14323 (a coluna de combinações excepcionais da tabela de ações), sobrecargas de uso com 0,21 (uso geral), 0,28 (comercial, escritórios, "
                          "acesso público: tipo \"Sobrecarga em edificações industriais, comerciais…\") ou 0,42 (bibliotecas, depósitos, oficinas, garagens). Uma combinação G + 0,20 W por vento (item 6.3.2). "
                          "Sem elas, a verificação usa 70 % dos esforços a frio (item 8.2.3)."));
    m_replace = new QCheckBox(tr("Substituir as combinações atuais (desmarcado: acrescentar)"));
    m_replace->setChecked(true);
    for (auto *box : {m_ultimate, m_favorable, m_special, m_exceptional, m_fire, m_quasi, m_frequent, m_rare}) {
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
    const double values[] = {d.gammaUnfavorable, d.gammaFavorable, d.gammaSpecial, d.gammaExceptional, d.psi0, d.psi1, d.psi2};
    for (int i = 0; i < 7; ++i) {
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
        const int situation = qobject_cast<QComboBox *>(m_table->cellWidget(row, SituationCol))->currentData().toInt();
        s.situation = situation == 3 ? CombinationGenerator::Situation::Construction : (situation == 0 ? CombinationGenerator::Situation::Normal : CombinationGenerator::Situation::Special);
        s.shortDuration = (situation == 2);
        s.gammaUnfavorable = value(GammaU);
        s.gammaFavorable = value(GammaF);
        s.gammaSpecial = value(GammaS);
        s.gammaExceptional = value(GammaE);
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
    o.special = m_special->isChecked();
    o.construction = m_special->isChecked();
    o.exceptional = m_exceptional->isChecked();
    o.fire = m_fire->isChecked();
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
