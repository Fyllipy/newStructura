#include "BucklingDialog.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Structura::UI {

BucklingDialog::BucklingDialog(const QStringList &combinations, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Flambagem elástica"));
    setMinimumWidth(460);
    auto *root = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Autovalores de flambagem linear (K + λ·Kg)·φ = 0 com os esforços axiais de primeira ordem da combinação. "
                                "O fator crítico λ multiplica todas as cargas da combinação: λ < 1 indica instabilidade sob as cargas atuais; "
                                "λ ≥ 10 costuma indicar estrutura pouco sensível a efeitos de segunda ordem. "
                                "A análise é elástica ideal (sem imperfeições, sem plastificação) e vale para estruturas sem tirantes ativos."));
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto *row = new QHBoxLayout;
    m_combination = new QComboBox;
    m_combination->addItems(combinations);
    m_modes = new QSpinBox;
    m_modes->setRange(1, 20);
    m_modes->setValue(5);
    auto *run = new QPushButton(tr("Calcular"));
    row->addWidget(new QLabel(tr("Combinação:")));
    row->addWidget(m_combination, 1);
    row->addWidget(new QLabel(tr("Modos:")));
    row->addWidget(m_modes);
    row->addWidget(run);
    root->addLayout(row);

    m_table = new QTableWidget(0, 2);
    m_table->setHorizontalHeaderLabels({tr("Modo"), tr("Fator crítico λ")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(m_table, 1);

    auto *scaleRow = new QHBoxLayout;
    m_scale = new QSlider(Qt::Horizontal);
    m_scale->setRange(2, 30);
    m_scale->setValue(10);
    scaleRow->addWidget(new QLabel(tr("Escala da forma modal:")));
    scaleRow->addWidget(m_scale, 1);
    root->addLayout(scaleRow);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    connect(run, &QPushButton::clicked, this, [this]() {
        m_status->setStyleSheet(QString());
        m_status->setText(tr("Calculando…"));
        emit computeRequested(m_combination->currentText(), m_modes->value());
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() { emitMode(); });
    connect(m_scale, &QSlider::valueChanged, this, [this]() { emitMode(); });
}

void BucklingDialog::setResults(const QVector<double> &factors)
{
    m_table->setRowCount(factors.size());
    for (int i = 0; i < factors.size(); ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        auto *item = new QTableWidgetItem(QString::number(factors[i], 'f', 3));
        if (factors[i] < 1.0) {
            item->setForeground(QColor(QStringLiteral("#c0392b")));
        }
        m_table->setItem(i, 1, item);
    }
    if (factors.isEmpty()) {
        m_status->setText(tr("Nenhum modo de flambagem encontrado (sem esforços de compressão na combinação)."));
        return;
    }
    m_status->setText(tr("Menor fator crítico: λ = %1. Selecione um modo para ver a forma no modelo.").arg(factors.first(), 0, 'f', 3));
    m_table->selectRow(0);
}

void BucklingDialog::setError(const QString &message)
{
    m_status->setStyleSheet(QStringLiteral("color:#c0392b;"));
    m_status->setText(message);
}

void BucklingDialog::emitMode()
{
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        return;
    }
    emit modeChanged(rows.first().row(), m_scale->value() / 100.0);
}

void BucklingDialog::closeEvent(QCloseEvent *event)
{
    emit closedByUser();
    QDialog::closeEvent(event);
}

} // namespace Structura::UI
