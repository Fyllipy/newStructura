#pragma once

#include <QDialog>
#include <QString>

class QTextEdit;

namespace Structura::UI {

class TemporaryAnalysisResultsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TemporaryAnalysisResultsDialog(const QString &resultsText, QWidget *parent = nullptr);

private:
    QTextEdit *m_textEdit {nullptr};
};

} // namespace Structura::UI
