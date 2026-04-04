#include "TemporaryAnalysisResultsDialog.h"

#include <QDialogButtonBox>
#include <QTextEdit>
#include <QVBoxLayout>

namespace Structura::UI {

// PROVISORIO: esta janela e chamada em MainWindow::onRunStructuralAnalysis() em src/ui/MainWindow.cpp.
TemporaryAnalysisResultsDialog::TemporaryAnalysisResultsDialog(const QString &resultsText, QWidget *parent)
    : QDialog(parent)
    , m_textEdit(new QTextEdit(this))
{
    setWindowTitle(tr("Resultados da analise (provisorio)"));
    resize(880, 560);

    m_textEdit->setReadOnly(true);
    m_textEdit->setLineWrapMode(QTextEdit::NoWrap);
    m_textEdit->setPlainText(resultsText);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_textEdit);
    layout->addWidget(buttons);

    setLayout(layout);
}

} // namespace Structura::UI
