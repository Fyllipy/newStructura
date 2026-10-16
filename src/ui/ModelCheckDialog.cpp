#include "ModelCheckDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace Structura::UI {

using Structura::App::Services::ModelCheckService;

ModelCheckDialog::ModelCheckDialog(const QVector<ModelCheckService::Issue> &issues, bool blocking, QWidget *parent)
    : QDialog(parent)
    , m_issues(issues)
{
    setWindowTitle(tr("Checagem do modelo"));
    resize(720, 480);
    auto *root = new QVBoxLayout(this);
    const int errors = ModelCheckService::count(issues, ModelCheckService::Severity::Error);
    const int warnings = ModelCheckService::count(issues, ModelCheckService::Severity::Warning);
    const int infos = ModelCheckService::count(issues, ModelCheckService::Severity::Info);
    auto *summary = new QLabel;
    if (issues.isEmpty()) {
        summary->setText(tr("<b style='color:#1e8449;'>Nenhum problema encontrado.</b>"));
    } else {
        summary->setText(tr("<b style='color:#c0392b;'>%1 erro(s)</b> &nbsp; <b style='color:#b7791f;'>%2 aviso(s)</b> &nbsp; <span style='color:#2e75b6;'>%3 informação(ões)</span>"
                            "<br><span style='color:#5a6472;'>Dê dois cliques em um item para selecionar o elemento no modelo.</span>")
                             .arg(errors).arg(warnings).arg(infos));
    }
    root->addWidget(summary);
    m_list = new QListWidget;
    for (int i = 0; i < issues.size(); ++i) {
        const auto &issue = issues[i];
        QString tag;
        QColor color;
        switch (issue.severity) {
        case ModelCheckService::Severity::Error: tag = tr("ERRO"); color = QColor(QStringLiteral("#c0392b")); break;
        case ModelCheckService::Severity::Warning: tag = tr("AVISO"); color = QColor(QStringLiteral("#b7791f")); break;
        case ModelCheckService::Severity::Info: tag = tr("INFO"); color = QColor(QStringLiteral("#2e75b6")); break;
        }
        auto *item = new QListWidgetItem(QStringLiteral("[%1]  %2").arg(tag, issue.message), m_list);
        item->setForeground(color);
        item->setData(Qt::UserRole, i);
        item->setToolTip(issue.code);
    }
    root->addWidget(m_list, 1);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        const auto &issue = m_issues.at(item->data(Qt::UserRole).toInt());
        if (issue.target != ModelCheckService::TargetKind::None) {
            emit focusRequested(issue.target, issue.id);
        }
    });

    auto *buttons = new QDialogButtonBox;
    if (blocking) {
        auto *go = buttons->addButton(tr("Analisar mesmo assim"), QDialogButtonBox::AcceptRole);
        go->setStyleSheet(QStringLiteral("QPushButton { background: #c0392b; color: white; font-weight: 600; padding: 5px 14px; border-radius: 4px; }"));
        buttons->addButton(tr("Cancelar e corrigir"), QDialogButtonBox::RejectRole);
    } else {
        buttons->addButton(tr("Fechar"), QDialogButtonBox::AcceptRole);
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

} // namespace Structura::UI
