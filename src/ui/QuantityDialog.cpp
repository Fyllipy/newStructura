#include "QuantityDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::Services::QuantityService;

QuantityDialog::QuantityDialog(const QuantityService::Summary &summary, QWidget *parent)
    : QDialog(parent)
    , m_summary(summary)
{
    setWindowTitle(tr("Quantitativos e lista de materiais"));
    resize(860, 480);
    auto *root = new QVBoxLayout(this);
    auto *head = new QLabel(tr("<b>%1 barras</b> · comprimento total <b>%2 m</b> · massa total <b>%3 kg</b> (%4 t) · área de pintura <b>%5 m²</b>")
                                .arg(summary.barCount)
                                .arg(summary.totalLength, 0, 'f', 2)
                                .arg(summary.totalMass, 0, 'f', 1)
                                .arg(summary.totalMass / 1000.0, 0, 'f', 3)
                                .arg(summary.totalPaintArea, 0, 'f', 1));
    root->addWidget(head);

    auto *table = new QTableWidget(summary.rows.size(), 7);
    table->setHorizontalHeaderLabels({tr("Perfil"), tr("Material"), tr("Qtd"), tr("Comprimento (m)"), tr("kg/m"), tr("Massa (kg)"), tr("Pintura (m²)")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    for (int i = 0; i < summary.rows.size(); ++i) {
        const auto &r = summary.rows[i];
        const QStringList cells {r.section, r.material, QString::number(r.count), QString::number(r.totalLength, 'f', 2),
                                 QString::number(r.massPerMeter, 'f', 2), QString::number(r.totalMass, 'f', 1), QString::number(r.paintArea, 'f', 1)};
        for (int c = 0; c < cells.size(); ++c) {
            auto *item = new QTableWidgetItem(cells[c]);
            if (c >= 2) {
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            }
            table->setItem(i, c, item);
        }
    }
    root->addWidget(table, 1);
    if (!summary.warnings.isEmpty()) {
        auto *warn = new QLabel(summary.warnings.join(QLatin1Char('\n')));
        warn->setStyleSheet(QStringLiteral("color:#b7791f;"));
        warn->setWordWrap(true);
        root->addWidget(warn);
    }
    if (!summary.connections.isEmpty()) {
        auto *title = new QLabel(tr("<b>Ligações</b> — massa de peças: <b>%1 kg</b>").arg(summary.connectionMass, 0, 'f', 1));
        root->addWidget(title);
        auto *conn = new QTableWidget(summary.connections.size(), 4);
        conn->setHorizontalHeaderLabels({tr("Peça"), tr("Qtd"), tr("Massa unitária (kg)"), tr("Massa total (kg)")});
        conn->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        conn->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        conn->verticalHeader()->hide();
        conn->setEditTriggers(QAbstractItemView::NoEditTriggers);
        conn->setMaximumHeight(170);
        for (int i = 0; i < summary.connections.size(); ++i) {
            const auto &c = summary.connections[i];
            const QStringList cells {c.description, QString::number(c.quantity), QString::number(c.massEach, 'f', 2), QString::number(c.totalMass, 'f', 1)};
            for (int col = 0; col < cells.size(); ++col) {
                conn->setItem(i, col, new QTableWidgetItem(cells[col]));
            }
        }
        root->addWidget(conn);
    }
    auto *note = new QLabel(tr("Massa = A·L·γ do material, sem perdas, chapas de ligação, parafusos ou soldas. Pintura = perímetro da seção × comprimento."));
    note->setStyleSheet(QStringLiteral("color:#5a6472;"));
    note->setWordWrap(true);
    root->addWidget(note);

    auto *buttons = new QDialogButtonBox;
    auto *csv = buttons->addButton(tr("Exportar CSV…"), QDialogButtonBox::ActionRole);
    auto *copy = buttons->addButton(tr("Copiar"), QDialogButtonBox::ActionRole);
    buttons->addButton(tr("Fechar"), QDialogButtonBox::AcceptRole);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(copy, &QPushButton::clicked, this, [this]() { QApplication::clipboard()->setText(QuantityService::toCsv(m_summary)); });
    connect(csv, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(this, tr("Exportar quantitativos"), QStringLiteral("quantitativos.csv"), tr("CSV (*.csv)"));
        if (path.isEmpty()) {
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, tr("Quantitativos"), tr("Não foi possível gravar o arquivo."));
            return;
        }
        file.write("\xEF\xBB\xBF");   // BOM para abrir acentos no Excel
        file.write(QuantityService::toCsv(m_summary).toUtf8());
    });
    root->addWidget(buttons);
}

} // namespace Structura::UI
