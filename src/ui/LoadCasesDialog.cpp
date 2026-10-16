#include "LoadCasesDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Structura::UI {

namespace {

constexpr int kOriginalNameRole = Qt::UserRole + 1;

bool validName(const QString &name)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_\\-]+$"));
    return pattern.match(name).hasMatch();
}

QString formatFactor(double value)
{
    return QString::number(value, 'g', 6);
}

} // namespace

LoadCasesDialog::LoadCasesDialog(const QVector<LoadCase> &cases,
                                 const QVector<LoadCombination> &combinations,
                                 QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Casos de carga e combinacoes"));
    setModal(true);
    resize(560, 420);

    auto *layout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    layout->addWidget(m_tabs, 1);

    // ── Aba de casos ──────────────────────────────────────────────────────
    auto *casesPage = new QWidget(this);
    auto *casesLayout = new QVBoxLayout(casesPage);
    auto *casesHint = new QLabel(tr("Cada caso e resolvido separadamente. \"Peso proprio\" inclui gama x area de todas as barras."), casesPage);
    casesHint->setWordWrap(true);
    casesLayout->addWidget(casesHint);

    m_caseTable = new QTableWidget(0, 3, casesPage);
    m_caseTable->setHorizontalHeaderLabels({tr("Nome"), tr("Sigla"), tr("Peso proprio")});
    m_caseTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_caseTable->verticalHeader()->setVisible(false);
    m_caseTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    casesLayout->addWidget(m_caseTable, 1);

    auto *caseButtons = new QHBoxLayout();
    auto *addCase = new QPushButton(tr("Adicionar caso"), casesPage);
    auto *removeCase = new QPushButton(tr("Remover caso"), casesPage);
    caseButtons->addWidget(addCase);
    caseButtons->addWidget(removeCase);
    caseButtons->addStretch(1);
    casesLayout->addLayout(caseButtons);
    m_tabs->addTab(casesPage, tr("Casos"));

    for (const LoadCase &loadCase : cases) {
        addCaseRow(loadCase.name, loadCase.abbreviation, loadCase.selfWeight, loadCase.name);
    }
    connect(addCase, &QPushButton::clicked, this, [this]() {
        QSet<QString> used;
        for (const QString &name : currentCaseNames()) {
            used.insert(name);
        }
        int index = m_caseTable->rowCount() + 1;
        QString name;
        do {
            name = QStringLiteral("CASO%1").arg(index++);
        } while (used.contains(name));
        addCaseRow(name, name, false, QString());
        m_caseTable->setCurrentCell(m_caseTable->rowCount() - 1, 0);
    });
    connect(removeCase, &QPushButton::clicked, this, [this]() {
        const int row = m_caseTable->currentRow();
        if (row < 0 || m_caseTable->rowCount() <= 1) {
            return;
        }
        m_caseTable->removeRow(row);
    });

    // ── Aba de combinacoes ────────────────────────────────────────────────
    auto *comboPage = new QWidget(this);
    auto *comboLayout = new QVBoxLayout(comboPage);
    auto *comboHint = new QLabel(tr("Fator aplicado ao resultado de cada caso (vazio ou 0 = nao participa)."), comboPage);
    comboHint->setWordWrap(true);
    comboLayout->addWidget(comboHint);
    m_comboTable = new QTableWidget(0, 1, comboPage);
    m_comboTable->verticalHeader()->setVisible(false);
    comboLayout->addWidget(m_comboTable, 1);
    auto *comboButtons = new QHBoxLayout();
    auto *addCombo = new QPushButton(tr("Adicionar combinacao"), comboPage);
    auto *removeCombo = new QPushButton(tr("Remover combinacao"), comboPage);
    comboButtons->addWidget(addCombo);
    comboButtons->addWidget(removeCombo);
    comboButtons->addStretch(1);
    comboLayout->addLayout(comboButtons);
    m_tabs->addTab(comboPage, tr("Combinacoes"));

    m_comboCaseColumns = currentCaseNames();
    rebuildCombinationColumns();
    for (const LoadCombination &combination : combinations) {
        QHash<QString, double> factors;
        for (const auto &term : combination.factors) {
            factors.insert(term.first, term.second);
        }
        addCombinationRow(combination.name, factors);
    }
    connect(addCombo, &QPushButton::clicked, this, [this]() {
        QHash<QString, double> factors;
        for (const QString &name : m_comboCaseColumns) {
            factors.insert(name, 1.0);
        }
        addCombinationRow(QStringLiteral("Combo %1").arg(m_comboTable->rowCount() + 1).replace(QLatin1Char(' '), QLatin1Char('_')),
                          factors);
    });
    connect(removeCombo, &QPushButton::clicked, this, [this]() {
        const int row = m_comboTable->currentRow();
        if (row >= 0) {
            m_comboTable->removeRow(row);
        }
    });
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 1 && currentCaseNames() != m_comboCaseColumns) {
            const auto snapshot = combinationFactorsSnapshot();
            QStringList names;
            for (int row = 0; row < m_comboTable->rowCount(); ++row) {
                names << m_comboTable->item(row, 0)->text();
            }
            m_comboCaseColumns = currentCaseNames();
            rebuildCombinationColumns();
            for (const QString &name : names) {
                QHash<QString, double> factors = snapshot.value(name);
                // Casos novos entram com fator 1 para facilitar o uso.
                for (const QString &caseName : m_comboCaseColumns) {
                    if (!factors.contains(caseName) && !snapshot.value(name).isEmpty()) {
                        factors.insert(caseName, 0.0);
                    }
                }
                addCombinationRow(name, factors);
            }
        }
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &LoadCasesDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &LoadCasesDialog::reject);
    layout->addWidget(buttons);
}

void LoadCasesDialog::addCaseRow(const QString &name, const QString &abbreviation, bool selfWeight, const QString &originalName)
{
    const int row = m_caseTable->rowCount();
    m_caseTable->insertRow(row);
    auto *nameItem = new QTableWidgetItem(name);
    nameItem->setData(kOriginalNameRole, originalName);
    if (!originalName.isEmpty()) {
        // Casos existentes nao sao renomeados (as cargas e combinacoes os referenciam pelo nome).
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setToolTip(tr("Casos existentes nao podem ser renomeados"));
    }
    m_caseTable->setItem(row, 0, nameItem);
    m_caseTable->setItem(row, 1, new QTableWidgetItem(abbreviation));
    auto *weightItem = new QTableWidgetItem();
    weightItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
    weightItem->setCheckState(selfWeight ? Qt::Checked : Qt::Unchecked);
    m_caseTable->setItem(row, 2, weightItem);
}

QStringList LoadCasesDialog::currentCaseNames() const
{
    QStringList names;
    for (int row = 0; row < m_caseTable->rowCount(); ++row) {
        names << m_caseTable->item(row, 0)->text().trimmed();
    }
    return names;
}

QHash<QString, QHash<QString, double>> LoadCasesDialog::combinationFactorsSnapshot() const
{
    QHash<QString, QHash<QString, double>> result;
    for (int row = 0; row < m_comboTable->rowCount(); ++row) {
        QHash<QString, double> factors;
        for (int column = 0; column < m_comboCaseColumns.size(); ++column) {
            const auto *item = m_comboTable->item(row, column + 1);
            if (item) {
                factors.insert(m_comboCaseColumns.at(column), item->text().replace(QLatin1Char(','), QLatin1Char('.')).toDouble());
            }
        }
        result.insert(m_comboTable->item(row, 0)->text(), factors);
    }
    return result;
}

void LoadCasesDialog::rebuildCombinationColumns()
{
    m_comboTable->setRowCount(0);
    m_comboTable->setColumnCount(1 + static_cast<int>(m_comboCaseColumns.size()));
    QStringList headers {tr("Combinacao")};
    headers.append(m_comboCaseColumns);
    m_comboTable->setHorizontalHeaderLabels(headers);
    m_comboTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void LoadCasesDialog::addCombinationRow(const QString &name, const QHash<QString, double> &factors)
{
    const int row = m_comboTable->rowCount();
    m_comboTable->insertRow(row);
    m_comboTable->setItem(row, 0, new QTableWidgetItem(name));
    for (int column = 0; column < m_comboCaseColumns.size(); ++column) {
        const double value = factors.value(m_comboCaseColumns.at(column), 0.0);
        m_comboTable->setItem(row, column + 1, new QTableWidgetItem(value == 0.0 ? QString() : formatFactor(value)));
    }
}

QVector<LoadCasesDialog::LoadCase> LoadCasesDialog::loadCases() const
{
    QVector<LoadCase> result;
    for (int row = 0; row < m_caseTable->rowCount(); ++row) {
        LoadCase loadCase;
        loadCase.name = m_caseTable->item(row, 0)->text().trimmed();
        loadCase.abbreviation = m_caseTable->item(row, 1)->text().trimmed();
        if (loadCase.abbreviation.isEmpty()) {
            loadCase.abbreviation = loadCase.name;
        }
        loadCase.selfWeight = m_caseTable->item(row, 2)->checkState() == Qt::Checked;
        result.append(loadCase);
    }
    return result;
}

QVector<LoadCasesDialog::LoadCombination> LoadCasesDialog::combinations() const
{
    // As colunas da tabela refletem os casos no momento da ultima reconstrucao.
    QVector<LoadCombination> result;
    for (int row = 0; row < m_comboTable->rowCount(); ++row) {
        LoadCombination combination;
        combination.name = m_comboTable->item(row, 0)->text().trimmed();
        for (int column = 0; column < m_comboCaseColumns.size(); ++column) {
            const auto *item = m_comboTable->item(row, column + 1);
            if (!item) {
                continue;
            }
            bool ok = false;
            const double factor = item->text().replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
            if (ok && factor != 0.0) {
                combination.factors.append({m_comboCaseColumns.at(column), factor});
            }
        }
        result.append(combination);
    }
    return result;
}

void LoadCasesDialog::accept()
{
    QSet<QString> names;
    for (const LoadCase &loadCase : loadCases()) {
        if (!validName(loadCase.name)) {
            QMessageBox::warning(this, windowTitle(),
                                 tr("Nome de caso invalido: \"%1\". Use apenas letras, numeros, \"_\" e \"-\".").arg(loadCase.name));
            return;
        }
        if (names.contains(loadCase.name)) {
            QMessageBox::warning(this, windowTitle(), tr("Nome de caso repetido: \"%1\".").arg(loadCase.name));
            return;
        }
        names.insert(loadCase.name);
    }

    // Atualiza as colunas das combinacoes se os casos mudaram sem abrir a aba.
    if (currentCaseNames() != m_comboCaseColumns) {
        m_tabs->setCurrentIndex(1);
        m_tabs->setCurrentIndex(0);
    }

    QSet<QString> comboNames;
    for (const LoadCombination &combination : combinations()) {
        if (!validName(combination.name)) {
            QMessageBox::warning(this, windowTitle(),
                                 tr("Nome de combinacao invalido: \"%1\". Use apenas letras, numeros, \"_\" e \"-\".").arg(combination.name));
            return;
        }
        if (comboNames.contains(combination.name)) {
            QMessageBox::warning(this, windowTitle(), tr("Nome de combinacao repetido: \"%1\".").arg(combination.name));
            return;
        }
        comboNames.insert(combination.name);
    }

    m_renames.clear();
    for (int row = 0; row < m_caseTable->rowCount(); ++row) {
        const QString original = m_caseTable->item(row, 0)->data(kOriginalNameRole).toString();
        const QString current = m_caseTable->item(row, 0)->text().trimmed();
        if (!original.isEmpty() && original != current) {
            m_renames.insert(original, current);
        }
    }
    QDialog::accept();
}

} // namespace Structura::UI
