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

} // namespace Structura::UI
