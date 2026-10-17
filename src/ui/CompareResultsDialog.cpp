#include "CompareResultsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace Structura::UI {

CompareResultsDialog::CompareResultsDialog(CompareData data, QWidget *parent)
    : QDialog(parent)
    , m_data(std::move(data))
{
    setWindowTitle(tr("Comparar combinações"));
    resize(1040, 620);
    auto *root = new QHBoxLayout(this);

    auto *left = new QVBoxLayout;
    left->addWidget(new QLabel(tr("Combinações / casos a comparar:")));
    m_viewList = new QListWidget;
    for (const auto &view : m_data.views) {
        auto *item = new QListWidgetItem(view.name, m_viewList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(view.isCombination ? Qt::Checked : Qt::Unchecked);
    }
    left->addWidget(m_viewList, 1);
    m_nodes = new QRadioButton(tr("Nós (deslocamentos e reações)"));
    m_bars = new QRadioButton(tr("Barras (esforços)"));
    m_bars->setChecked(true);
    left->addWidget(m_nodes);
    left->addWidget(m_bars);
    m_quantity = new QComboBox;
    left->addWidget(new QLabel(tr("Grandeza:")));
    left->addWidget(m_quantity);
    m_endMode = new QComboBox;
    m_endMode->addItem(tr("Máximo em valor absoluto das extremidades"), 2);
    m_endMode->addItem(tr("Extremidade inicial (I)"), 0);
    m_endMode->addItem(tr("Extremidade final (J)"), 1);
    left->addWidget(m_endMode);
    m_onlySelected = new QCheckBox(tr("Somente os elementos selecionados"));
    m_onlySelected->setChecked(!m_data.selectedBars.isEmpty() || !m_data.selectedNodes.isEmpty());
    left->addWidget(m_onlySelected);
    auto *exportButton = new QPushButton(tr("Exportar CSV…"));
    left->addWidget(exportButton);
    auto *leftWidget = new QWidget;
    leftWidget->setLayout(left);
    leftWidget->setFixedWidth(300);
    root->addWidget(leftWidget);

    auto *right = new QVBoxLayout;
    m_summary = new QLabel;
    m_summary->setWordWrap(true);
    m_summary->setTextFormat(Qt::RichText);
    right->addWidget(m_summary);
    m_table = new QTableWidget;
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(22);
    right->addWidget(m_table, 1);
    root->addLayout(right, 1);

    connect(m_nodes, &QRadioButton::toggled, this, [this]() { rebuildQuantities(); });
    connect(m_quantity, &QComboBox::currentIndexChanged, this, [this]() { refresh(); });
    connect(m_endMode, &QComboBox::currentIndexChanged, this, [this]() { refresh(); });
    connect(m_onlySelected, &QCheckBox::toggled, this, [this]() { refresh(); });
    connect(m_viewList, &QListWidget::itemChanged, this, [this]() { refresh(); });
    connect(exportButton, &QPushButton::clicked, this, [this]() { exportCsv(); });
    rebuildQuantities();
}

void CompareResultsDialog::rebuildQuantities()
{
    const bool nodes = m_nodes->isChecked();
    QSignalBlocker block(m_quantity);
    m_quantity->clear();
    if (nodes) {
        const char *disp[6] = {"Ux (mm)", "Uy (mm)", "Uz (mm)", "Rx (mrad)", "Ry (mrad)", "Rz (mrad)"};
        const char *react[6] = {"Reação Fx (kN)", "Reação Fy (kN)", "Reação Fz (kN)", "Reação Mx (kN·m)", "Reação My (kN·m)", "Reação Mz (kN·m)"};
        for (int i = 0; i < 6; ++i) {
            m_quantity->addItem(QString::fromUtf8(disp[i]), i);
        }
        for (int i = 0; i < 6; ++i) {
            m_quantity->addItem(QString::fromUtf8(react[i]), 100 + i);
        }
    } else {
        const char *names[6] = {"N (kN)", "Vy (kN)", "Vz (kN)", "T (kN·m)", "My (kN·m)", "Mz (kN·m)"};
        for (int i = 0; i < 6; ++i) {
            m_quantity->addItem(QString::fromUtf8(names[i]), i);
        }
        m_quantity->setCurrentIndex(5);
    }
    m_endMode->setEnabled(!nodes);
    refresh();
}

void CompareResultsDialog::refresh()
{
    const bool nodes = m_nodes->isChecked();
    QVector<int> chosen;
    for (int i = 0; i < m_viewList->count(); ++i) {
        if (m_viewList->item(i)->checkState() == Qt::Checked) {
            chosen.append(i);
        }
    }
    const int code = m_quantity->currentData().toInt();
    const int mode = m_endMode->currentData().toInt();
    const auto &ids = nodes ? m_data.nodeIds : m_data.barIds;
    const auto &selected = nodes ? m_data.selectedNodes : m_data.selectedBars;
    QVector<int> rows;
    for (const int id : ids) {
        if (!m_onlySelected->isChecked() || selected.isEmpty() || selected.contains(id)) {
            rows.append(id);
        }
    }
    auto value = [&](const CompareData::View &view, int id, bool &ok) {
        ok = false;
        if (nodes) {
            const bool reaction = code >= 100;
            const auto &map = reaction ? view.reactions : view.displacements;
            const auto it = map.constFind(id);
            if (it == map.constEnd()) {
                return 0.0;
            }
            ok = true;
            const int c = reaction ? code - 100 : code;
            return (*it)[static_cast<std::size_t>(c)] * (!reaction ? 1000.0 : 1.0);
        }
        const auto it = view.forces.constFind(id);
        if (it == view.forces.constEnd()) {
            return 0.0;
        }
        ok = true;
        const double a = (*it)[static_cast<std::size_t>(code)], b = (*it)[static_cast<std::size_t>(code + 6)];
        return mode == 0 ? a : (mode == 1 ? b : (std::abs(a) >= std::abs(b) ? a : b));
    };

    m_table->setSortingEnabled(false);
    QStringList headers {nodes ? tr("Nó") : tr("Barra")};
    for (const int v : chosen) {
        headers << m_data.views[v].name;
    }
    headers << tr("Máx") << tr("Mín") << tr("Governa (|valor|)");
    m_table->clear();
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);
    m_table->setRowCount(rows.size());
    double globalMax = -1e300, globalMin = 1e300, globalAbs = 0.0;
    int globalAbsRow = -1;
    QString globalAbsView;
    for (int r = 0; r < rows.size(); ++r) {
        auto *idItem = new QTableWidgetItem;
        idItem->setData(Qt::DisplayRole, rows[r]);
        idItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(r, 0, idItem);
        double maxV = -1e300, minV = 1e300, bestAbs = -1.0;
        int bestCol = -1;
        QVector<double> values(chosen.size(), 0.0);
        for (int c = 0; c < chosen.size(); ++c) {
            bool ok = false;
            const double v = value(m_data.views[chosen[c]], rows[r], ok);
            values[c] = v;
            if (!ok) {
                continue;
            }
            maxV = std::max(maxV, v);
            minV = std::min(minV, v);
            if (std::abs(v) > bestAbs) {
                bestAbs = std::abs(v);
                bestCol = c;
            }
        }
        for (int c = 0; c < chosen.size(); ++c) {
            auto *item = new QTableWidgetItem;
            item->setData(Qt::DisplayRole, std::round(values[c] * 1000.0) / 1000.0);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (c == bestCol) {
                QFont f = item->font();
                f.setBold(true);
                item->setFont(f);
                item->setBackground(QColor(QStringLiteral("#fff3cd")));
            }
            m_table->setItem(r, 1 + c, item);
        }
        const int base = 1 + chosen.size();
        auto *maxItem = new QTableWidgetItem;
        auto *minItem = new QTableWidgetItem;
        auto *govItem = new QTableWidgetItem(bestCol >= 0 ? m_data.views[chosen[bestCol]].name : QString());
        if (bestCol >= 0) {
            maxItem->setData(Qt::DisplayRole, std::round(maxV * 1000.0) / 1000.0);
            minItem->setData(Qt::DisplayRole, std::round(minV * 1000.0) / 1000.0);
        }
        maxItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        minItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(r, base, maxItem);
        m_table->setItem(r, base + 1, minItem);
        m_table->setItem(r, base + 2, govItem);
        if (bestCol >= 0) {
            globalMax = std::max(globalMax, maxV);
            globalMin = std::min(globalMin, minV);
            if (bestAbs > globalAbs) {
                globalAbs = bestAbs;
                globalAbsRow = rows[r];
                globalAbsView = m_data.views[chosen[bestCol]].name;
            }
        }
    }
    m_table->setSortingEnabled(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->resizeColumnsToContents();
    if (globalAbsRow >= 0) {
        m_summary->setText(tr("<b>%1</b> — maior valor absoluto: <b>%2</b> em %3 %4 (combinação <b>%5</b>). Envoltória global: máx %6, mín %7. "
                              "A célula em destaque é a combinação que governa cada linha.")
                               .arg(m_quantity->currentText()).arg(globalAbs, 0, 'f', 3).arg(nodes ? tr("nó") : tr("barra")).arg(globalAbsRow)
                               .arg(globalAbsView).arg(globalMax, 0, 'f', 3).arg(globalMin, 0, 'f', 3));
    } else {
        m_summary->setText(tr("Marque ao menos uma combinação (e rode a análise) para comparar."));
    }
}

void CompareResultsDialog::exportCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Exportar comparação"), QStringLiteral("comparacao.csv"), tr("CSV (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Comparar"), tr("Não foi possível gravar o arquivo."));
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    QStringList header;
    for (int c = 0; c < m_table->columnCount(); ++c) {
        header << m_table->horizontalHeaderItem(c)->text();
    }
    out << header.join(QLatin1Char(';')) << '\n';
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QStringList cells;
        for (int c = 0; c < m_table->columnCount(); ++c) {
            cells << (m_table->item(r, c) ? m_table->item(r, c)->text() : QString());
        }
        out << cells.join(QLatin1Char(';')) << '\n';
    }
}

} // namespace Structura::UI
