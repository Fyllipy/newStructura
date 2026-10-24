#include "IntegrityDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::Services::IntegrityService;

IntegrityDialog::IntegrityDialog(const QStringList &combinations, Compute compute, QWidget *parent)
    : QDialog(parent)
    , m_compute(std::move(compute))
{
    setWindowTitle(tr("Integridade estrutural (NBR 8800:2024, 4.13)"));
    resize(980, 620);
    auto *root = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Travamento dos pilares em duas direções (4.13.4), força de tração das contenções horizontais e de suas ligações (4.13.7) e, quando a legislação exigir, "
                                "colapso progressivo por falha acidental de um pilar (4.13.8). As contenções são as barras horizontais ou pouco inclinadas que chegam ao nó do pilar "
                                "(vigas, escoras, tesouras); as resistências vêm do dimensionamento atual e as das ligações, das ligações definidas no modelo."));
    intro->setWordWrap(true);
    root->addWidget(intro);
    auto *form = new QFormLayout;
    m_roof = new QCheckBox(tr("Piso ou cobertura sem laje de concreto: contenções dos pilares de extremidade para tração e compressão (45 kN)"));
    m_collapse = new QCheckBox(tr("A legislação exige que a falha acidental de um pilar não cause colapso progressivo (4.13.8)"));
    m_combination = new QComboBox;
    m_combination->addItem(tr("Envoltória das combinações (conservador)"), QString());
    for (const QString &name : combinations) {
        m_combination->addItem(name, name);
    }
    m_combination->setToolTip(tr("4.13.8: combinação das ações permanentes diretas com as decorrentes do uso e ocupação da edificação."));
    form->addRow(m_roof);
    form->addRow(m_collapse);
    form->addRow(tr("Combinação para a reação vertical (4.13.8)"), m_combination);
    root->addLayout(form);
    m_summary = new QLabel;
    root->addWidget(m_summary);
    m_table = new QTableWidget(0, 7);
    m_table->setHorizontalHeaderLabels({tr("Item"), tr("Descrição"), tr("Local"), tr("Solicitante (kN)"), tr("Resistente (kN)"), tr("η"), tr("Situação")});
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->setVisible(false);
    root->addWidget(m_table, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto *recompute = buttons->addButton(tr("Verificar"), QDialogButtonBox::ActionRole);
    connect(recompute, &QPushButton::clicked, this, &IntegrityDialog::refresh);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    root->addWidget(buttons);
    refresh();
}

void IntegrityDialog::refresh()
{
    IntegrityService::Options options;
    options.roofWithoutSlab = m_roof->isChecked();
    options.progressiveCollapse = m_collapse->isChecked();
    options.combination = m_combination->currentData().toString();
    const auto rows = m_compute(options);
    m_table->setRowCount(0);
    int failed = 0, advisories = 0;
    for (const auto &row : rows) {
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        auto number = [](double value) { return value < 0.0 ? QStringLiteral("—") : QString::number(value, 'f', 1); };
        const QString status = row.ok ? tr("Atende") : (row.advisory ? tr("Recomendação não atendida") : tr("Não atende"));
        const QStringList cells = {row.clause.section(QLatin1String(", "), 1), row.description, row.location, number(row.demandKn), number(row.resistanceKn),
                                   row.eta < 0.0 ? QStringLiteral("—") : (row.eta > 100.0 ? QStringLiteral("—") : QString::number(row.eta, 'f', 3)), status};
        for (int c = 0; c < cells.size(); ++c) {
            auto *item = new QTableWidgetItem(cells[c]);
            item->setToolTip(row.note);
            if (c == 6) {
                item->setForeground(row.ok ? QColor(QStringLiteral("#1e6b3a")) : (row.advisory ? QColor(QStringLiteral("#8a5a00")) : QColor(QStringLiteral("#a32a1e"))));
            }
            m_table->setItem(r, c, item);
        }
        if (!row.ok) {
            (row.advisory ? advisories : failed)++;
        }
    }
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    if (rows.isEmpty()) {
        m_summary->setText(tr("<span style='color:#b7791f;'>Nenhum pilar com contenções foi encontrado: verifique se há barras verticais fora dos apoios e se a análise foi feita.</span>"));
    } else {
        m_summary->setText(tr("<b>%1</b> verificações: <b style='color:#a32a1e;'>%2</b> não atendem, <b style='color:#8a5a00;'>%3</b> recomendações não atendidas.").arg(rows.size()).arg(failed).arg(advisories));
    }
}

} // namespace Structura::UI
