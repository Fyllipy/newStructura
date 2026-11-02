#include "ShellDialog.h"

#include "app/ShellSolverBridge.h"
#include "shell/ShellJob.h"
#include "shell/ShellMesh.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <set>

using namespace Structura::Shell;

namespace Structura::UI {

namespace {

QString zoneRatioText(double r)
{
    return QString::number(r, 'f', 2);
}

} // namespace

void ShellDialog::runDesign()
{
    if (!m_results.ok() || !m_hasMesh) {
        QMessageBox::information(this, tr("Cascas"), tr("Analise o modelo antes de verificar."));
        return;
    }
    std::vector<CaseResult> ultimate;
    for (int i = 0; i < m_ultimate->count(); ++i) {
        if (m_ultimate->item(i)->checkState() != Qt::Checked) {
            continue;
        }
        const std::string name = m_ultimate->item(i)->text().toStdString();
        const auto find = [&](const std::vector<CaseResult> &list) -> const CaseResult * {
            for (const CaseResult &c : list) {
                if (c.name == name) {
                    return &c;
                }
            }
            return nullptr;
        };
        const CaseResult *c = nullptr;
        for (const CaseResult &candidate : m_combined) {
            if (candidate.name == name) {
                c = &candidate;
            }
        }
        if (!c) {
            c = find(m_results.cases);
        }
        if (c) {
            ultimate.push_back(*c);
        }
    }
    const CaseResult *service = nullptr;
    const std::string serviceName = m_service->currentIndex() > 0 ? m_service->currentText().toStdString() : std::string();
    for (const CaseResult &c : m_combined) {
        if (c.name == serviceName) {
            service = &c;
        }
    }
    if (!service) {
        for (const CaseResult &c : m_results.cases) {
            if (c.name == serviceName) {
                service = &c;
            }
        }
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_design = designShells(m_mesh, m_model, ultimate, service);
    QApplication::restoreOverrideCursor();
    m_designTable->setRowCount(static_cast<int>(m_design.checks.size()));
    for (int r = 0; r < static_cast<int>(m_design.checks.size()); ++r) {
        const ZoneCheck &c = m_design.checks[static_cast<std::size_t>(r)];
        const QStringList cells = {QString::fromStdString(c.zone), QString::fromStdString(c.criterion), QString::number(c.demand, 'g', 5),
                                   c.limit > 0.0 ? QString::number(c.limit, 'g', 5) : QStringLiteral("—"), c.limit > 0.0 || c.ratio > 0.0 ? zoneRatioText(c.ratio) : QStringLiteral("—"),
                                   QString::fromStdString(c.unit), QString::fromStdString(c.reference), QString::fromStdString(c.note)};
        for (int k = 0; k < cells.size(); ++k) {
            auto *item = new QTableWidgetItem(cells[k]);
            if (c.ratio > 1.0 + 1e-9) {
                item->setBackground(QColor(255, 214, 214));
            } else if (c.ratio > 0.0 && c.limit > 0.0) {
                item->setBackground(c.ratio > 0.85 ? QColor(255, 244, 204) : QColor(222, 244, 226));
            }
            m_designTable->setItem(r, k, item);
        }
    }
    QString notes;
    for (const std::string &n : m_design.notes) {
        notes += QStringLiteral("• %1\n").arg(QString::fromStdString(n));
    }
    notes += tr("Razão máxima: %1 — %2").arg(m_design.maxRatio, 0, 'f', 2).arg(m_design.ok() ? tr("atende") : tr("NÃO atende"));
    m_designNotes->setPlainText(notes);
    refreshView();
}

QWidget *ShellDialog::buildDesignTab()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *top = new QHBoxLayout;
    auto *left = new QVBoxLayout;
    left->addWidget(new QLabel(tr("Combinações últimas (ELU) a verificar")));
    m_ultimate = new QListWidget;
    m_ultimate->setMaximumHeight(130);
    left->addWidget(m_ultimate);
    top->addLayout(left, 1);
    auto *right = new QFormLayout;
    m_service = new QComboBox;
    m_designView = new QComboBox;
    m_designView->addItems({tr("Grandeza dos resultados"), tr("Razão de utilização"), tr("Armadura face + dir. a"), tr("Armadura face + dir. b"), tr("Armadura face − dir. a"), tr("Armadura face − dir. b")});
    right->addRow(tr("Combinação de serviço (flecha)"), m_service);
    right->addRow(tr("Colorir a malha por"), m_designView);
    auto *run = new QPushButton(tr("Verificar"));
    run->setStyleSheet(QStringLiteral("QPushButton { background: #0d7fb9; color: white; font-weight: 600; padding: 5px 16px; border-radius: 4px; }"));
    right->addRow(run);
    top->addLayout(right, 1);
    layout->addLayout(top);
    m_designTable = new QTableWidget(0, 8);
    m_designTable->setHorizontalHeaderLabels({tr("Zona"), tr("Verificação"), tr("Solicitação"), tr("Limite"), tr("Razão"), tr("Un."), tr("Referência"), tr("Observação")});
    m_designTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_designTable->horizontalHeader()->setStretchLastSection(true);
    m_designTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_designTable, 1);
    m_designNotes = new QPlainTextEdit;
    m_designNotes->setReadOnly(true);
    m_designNotes->setMaximumHeight(90);
    layout->addWidget(m_designNotes);
    connect(run, &QPushButton::clicked, this, [this]() { runDesign(); });
    connect(m_designView, &QComboBox::currentIndexChanged, this, [this]() { refreshView(); });
    return page;
}

} // namespace Structura::UI
