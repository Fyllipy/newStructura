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
    left->addWidget(new QLabel(tr("Tabela:")));
    m_modeEnvelope = new QRadioButton(tr("Casos × esforços (envoltória dos elementos)"));
    m_modeCritical = new QRadioButton(tr("Esforços mais críticos (uma linha)"));
    m_modeDetail = new QRadioButton(tr("Detalhada (elemento × caso)"));
    m_modeEnvelope->setChecked(true);
    left->addWidget(m_modeEnvelope);
    left->addWidget(m_modeCritical);
    left->addWidget(m_modeDetail);
    m_endMode = new QComboBox;
    m_endMode->addItem(tr("Ao longo de toda a barra (máx. / mín.)"), 2);
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

    connect(m_nodes, &QRadioButton::toggled, this, [this]() { refresh(); });
    for (auto *mode : {m_modeEnvelope, m_modeCritical, m_modeDetail}) {
        connect(mode, &QRadioButton::toggled, this, [this]() { refresh(); });
    }
    connect(m_endMode, &QComboBox::currentIndexChanged, this, [this]() { refresh(); });
    connect(m_onlySelected, &QCheckBox::toggled, this, [this]() { refresh(); });
    connect(m_viewList, &QListWidget::itemChanged, this, [this]() { refresh(); });
    connect(exportButton, &QPushButton::clicked, this, [this]() { exportCsv(); });
    refresh();
}

namespace {

struct Quantity {
    QString label;
    int code {0};   ///< 0..5 deslocamentos ou esforcos; 100..105 reacoes
};

QVector<Quantity> quantitiesFor(bool nodes)
{
    QVector<Quantity> list;
    if (nodes) {
        const char *disp[6] = {"Ux (mm)", "Uy (mm)", "Uz (mm)", "Rx (mrad)", "Ry (mrad)", "Rz (mrad)"};
        const char *react[6] = {"Reação Fx (kN)", "Reação Fy (kN)", "Reação Fz (kN)", "Reação Mx (kN·m)", "Reação My (kN·m)", "Reação Mz (kN·m)"};
        for (int i = 0; i < 6; ++i) {
            list.append({QString::fromUtf8(disp[i]), i});
        }
        for (int i = 0; i < 6; ++i) {
            list.append({QString::fromUtf8(react[i]), 100 + i});
        }
    } else {
        const char *names[6] = {"N (kN)", "Vy (kN)", "Vz (kN)", "T (kN·m)", "My (kN·m)", "Mz (kN·m)"};
        for (int i = 0; i < 6; ++i) {
            list.append({QString::fromUtf8(names[i]), i});
        }
    }
    return list;
}

double rounded(double v)
{
    return std::round(v * 1000.0) / 1000.0;
}

} // namespace

void CompareResultsDialog::refresh()
{
    const bool nodes = m_nodes->isChecked();
    QVector<int> chosen;
    for (int i = 0; i < m_viewList->count(); ++i) {
        if (m_viewList->item(i)->checkState() == Qt::Checked) {
            chosen.append(i);
        }
    }
    const int mode = m_endMode->currentData().toInt();
    m_endMode->setEnabled(!nodes);
    const auto &ids = nodes ? m_data.nodeIds : m_data.barIds;
    const auto &selected = nodes ? m_data.selectedNodes : m_data.selectedBars;
    QVector<int> rows;
    for (const int id : ids) {
        if (!m_onlySelected->isChecked() || selected.isEmpty() || selected.contains(id)) {
            rows.append(id);
        }
    }
    const QVector<Quantity> quantities = quantitiesFor(nodes);
    auto value = [&](const CompareData::View &view, int id, int code, bool &ok) {
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
        const auto c = static_cast<std::size_t>(code);
        if (mode == 0) {
            return it->atI[c];
        }
        if (mode == 1) {
            return it->atJ[c];
        }
        // ao longo da barra: o extremo de maior modulo (o maximo ou o minimo, conforme o caso)
        return std::abs(it->maxAlong[c]) >= std::abs(it->minAlong[c]) ? it->maxAlong[c] : it->minAlong[c];
    };
    const QString elementName = nodes ? tr("nó") : tr("barra");

    // maior valor absoluto de cada grandeza sobre os elementos considerados: valor, elemento
    struct Extreme {
        double value {0.0};
        int id {-1};
        bool found {false};
    };
    auto extremeOf = [&](int view, int code) {
        Extreme e;
        for (const int id : rows) {
            bool ok = false;
            const double v = value(m_data.views[view], id, code, ok);
            if (ok && (!e.found || std::abs(v) > std::abs(e.value))) {
                e = {v, id, true};
            }
        }
        return e;
    };

    m_table->setSortingEnabled(false);
    m_table->clear();
    QStringList headers;
    const bool detail = m_modeDetail->isChecked();
    if (detail) {
        headers << (nodes ? tr("Nó") : tr("Barra")) << tr("Caso / combinação");
    } else if (m_modeCritical->isChecked()) {
        headers << QString();
    } else {
        headers << tr("Caso / combinação");
    }
    for (const auto &q : quantities) {
        headers << q.label;
    }
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);
    const int first = detail ? 2 : 1;
    auto numberItem = [](double v) {
        auto *item = new QTableWidgetItem;
        item->setData(Qt::DisplayRole, rounded(v));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return item;
    };
    auto highlight = [](QTableWidgetItem *item) {
        QFont f = item->font();
        f.setBold(true);
        item->setFont(f);
        item->setBackground(QColor(QStringLiteral("#fff3cd")));
    };

    if (chosen.isEmpty() || rows.isEmpty()) {
        m_table->setRowCount(0);
        m_summary->setText(tr("Marque ao menos uma combinação (e rode a análise) para comparar."));
        return;
    }

    if (detail) {
        m_table->setRowCount(static_cast<int>(rows.size() * chosen.size()));
        // o destaque marca, em cada coluna, a linha de maior valor absoluto
        QVector<double> bestAbs(quantities.size(), -1.0);
        QVector<int> bestRow(quantities.size(), -1);
        int r = 0;
        for (const int id : rows) {
            for (const int v : chosen) {
                auto *idItem = new QTableWidgetItem;
                idItem->setData(Qt::DisplayRole, id);
                idItem->setTextAlignment(Qt::AlignCenter);
                m_table->setItem(r, 0, idItem);
                m_table->setItem(r, 1, new QTableWidgetItem(m_data.views[v].name));
                for (int q = 0; q < quantities.size(); ++q) {
                    bool ok = false;
                    const double x = value(m_data.views[v], id, quantities[q].code, ok);
                    m_table->setItem(r, first + q, numberItem(ok ? x : 0.0));
                    if (ok && std::abs(x) > bestAbs[q]) {
                        bestAbs[q] = std::abs(x);
                        bestRow[q] = r;
                    }
                }
                ++r;
            }
        }
        for (int q = 0; q < quantities.size(); ++q) {
            if (bestRow[q] >= 0 && bestAbs[q] > 0.0) {
                highlight(m_table->item(bestRow[q], first + q));
            }
        }
        m_summary->setText(tr("<b>%1 linhas</b>: %2 × %3 combinações. A célula em destaque é o maior valor absoluto de cada coluna.")
                               .arg(r).arg(nodes ? tr("%1 nós").arg(rows.size()) : tr("%1 barras").arg(rows.size())).arg(chosen.size()));
        m_table->setSortingEnabled(true);
    } else if (m_modeEnvelope->isChecked()) {
        m_table->setRowCount(static_cast<int>(chosen.size()));
        QVector<double> bestAbs(quantities.size(), -1.0);
        QVector<int> bestRow(quantities.size(), -1);
        for (int r = 0; r < chosen.size(); ++r) {
            m_table->setItem(r, 0, new QTableWidgetItem(m_data.views[chosen[r]].name));
            for (int q = 0; q < quantities.size(); ++q) {
                const Extreme e = extremeOf(chosen[r], quantities[q].code);
                auto *item = numberItem(e.value);
                if (e.found) {
                    item->setToolTip(tr("Maior valor absoluto em %1 %2").arg(elementName).arg(e.id));
                    if (std::abs(e.value) > bestAbs[q]) {
                        bestAbs[q] = std::abs(e.value);
                        bestRow[q] = r;
                    }
                }
                m_table->setItem(r, first + q, item);
            }
        }
        for (int q = 0; q < quantities.size(); ++q) {
            if (bestRow[q] >= 0 && bestAbs[q] > 0.0) {
                highlight(m_table->item(bestRow[q], first + q));
            }
        }
        m_summary->setText(tr("Cada célula é o maior valor absoluto da grandeza entre os %1 considerados (passe o mouse para ver qual). "
                              "Em destaque, a combinação que governa cada coluna.").arg(nodes ? tr("nós") : tr("barras")));
    } else {
        // uma linha: o valor mais critico de cada grandeza, com a combinacao e o elemento que o produzem
        m_table->setRowCount(1);
        auto *label = new QTableWidgetItem(tr("Mais crítico"));
        QFont f = label->font();
        f.setBold(true);
        label->setFont(f);
        m_table->setItem(0, 0, label);
        for (int q = 0; q < quantities.size(); ++q) {
            Extreme best;
            int bestView = -1;
            for (const int v : chosen) {
                const Extreme e = extremeOf(v, quantities[q].code);
                if (e.found && (!best.found || std::abs(e.value) > std::abs(best.value))) {
                    best = e;
                    bestView = v;
                }
            }
            QString text;
            if (best.found) {
                text = QStringLiteral("%1\n%2 · %3 %4").arg(QString::number(rounded(best.value), 'f', 3), m_data.views[bestView].name, elementName).arg(best.id);
            }
            auto *item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            m_table->setItem(0, 1 + q, item);
        }
        m_table->resizeRowsToContents();
        m_summary->setText(tr("Maior valor absoluto de cada grandeza entre as combinações marcadas, com a combinação e o %1 que o produzem.").arg(elementName));
    }
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->resizeColumnsToContents();
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
