#include "ModalDialog.h"

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

#include <cmath>

namespace Structura::UI {

ModalDialog::ModalDialog(const QStringList &massSources, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Análise modal"));
    setMinimumWidth(640);
    auto *root = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Vibração livre não amortecida: (K − ω²·M)·φ = 0. A massa vem das cargas gravitacionais (vertical ÷ g) da combinação escolhida — "
                                "normalmente a quase permanente (peso próprio + permanentes + ψ2·sobrecarga) — e, se a combinação não incluir o peso próprio, "
                                "da massa específica dos perfis. Os apoios definem as restrições; articulações de extremidade não alteram a matriz de massa."));
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto *row = new QHBoxLayout;
    m_source = new QComboBox;
    m_source->addItem(tr("Somente massa dos perfis"), QString());
    for (const QString &name : massSources) {
        m_source->addItem(name, name);
    }
    if (m_source->count() > 1) {
        m_source->setCurrentIndex(1);
        for (int i = 1; i < m_source->count(); ++i) {   // prefere a combinacao quase permanente, depois qualquer ELS
            if (m_source->itemText(i).startsWith(QLatin1String("ELS-QP"))) {
                m_source->setCurrentIndex(i);
                break;
            }
            if (m_source->itemText(i).startsWith(QLatin1String("ELS")) && !m_source->itemText(m_source->currentIndex()).startsWith(QLatin1String("ELS"))) {
                m_source->setCurrentIndex(i);
            }
        }
    }
    m_modes = new QSpinBox;
    m_modes->setRange(1, 30);
    m_modes->setValue(6);
    auto *run = new QPushButton(tr("Calcular"));
    row->addWidget(new QLabel(tr("Fonte de massa:")));
    row->addWidget(m_source, 1);
    row->addWidget(new QLabel(tr("Modos:")));
    row->addWidget(m_modes);
    row->addWidget(run);
    root->addLayout(row);

    m_table = new QTableWidget(0, 7);
    m_table->setHorizontalHeaderLabels({tr("Modo"), tr("f (Hz)"), tr("T (s)"), tr("ω (rad/s)"), tr("Massa X (%)"), tr("Massa Y (%)"), tr("Massa Z (%)")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setMinimumHeight(200);
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
        emit computeRequested(m_source->currentData().toString(), m_modes->value());
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() { emitMode(); });
    connect(m_scale, &QSlider::valueChanged, this, [this]() { emitMode(); });
}

void ModalDialog::setResults(const QVector<double> &omega, const QVector<std::array<double, 3>> &ratios)
{
    m_table->setRowCount(omega.size());
    std::array<double, 3> sum {0.0, 0.0, 0.0};
    const double twoPi = 2.0 * 3.14159265358979323846;
    for (int i = 0; i < omega.size(); ++i) {
        const QStringList cells {QString::number(i + 1), QString::number(omega[i] / twoPi, 'f', 3), QString::number(twoPi / omega[i], 'f', 4), QString::number(omega[i], 'f', 3),
                                 QString::number(ratios[i][0] * 100.0, 'f', 1), QString::number(ratios[i][1] * 100.0, 'f', 1), QString::number(ratios[i][2] * 100.0, 'f', 1)};
        for (int c = 0; c < cells.size(); ++c) {
            m_table->setItem(i, c, new QTableWidgetItem(cells[c]));
        }
        for (int d = 0; d < 3; ++d) {
            sum[static_cast<std::size_t>(d)] += ratios[i][static_cast<std::size_t>(d)];
        }
    }
    if (omega.isEmpty()) {
        m_status->setText(tr("Nenhum modo encontrado."));
        return;
    }
    m_status->setText(tr("Massa efetiva acumulada: X %1% · Y %2% · Z %3%. A norma costuma exigir ≥ 90% por direção quando a resposta modal é usada.")
                          .arg(sum[0] * 100.0, 0, 'f', 1).arg(sum[1] * 100.0, 0, 'f', 1).arg(sum[2] * 100.0, 0, 'f', 1));
    m_table->selectRow(0);
}

void ModalDialog::setError(const QString &message)
{
    m_status->setStyleSheet(QStringLiteral("color:#c0392b;"));
    m_status->setText(message);
}

void ModalDialog::emitMode()
{
    const auto rows = m_table->selectionModel()->selectedRows();
    if (!rows.isEmpty()) {
        emit modeChanged(rows.first().row(), m_scale->value() / 100.0);
    }
}

void ModalDialog::closeEvent(QCloseEvent *event)
{
    emit closedByUser();
    QDialog::closeEvent(event);
}

} // namespace Structura::UI
