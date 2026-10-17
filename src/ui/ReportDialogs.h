#pragma once

#include "app/services/ReportService.h"

#include <QDialog>
#include <QHash>
#include <QImage>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTextBrowser;

namespace Structura::UI {

/// Utilidades compartilhadas pelos dialogos: mostra o relatorio no QTextBrowser e exporta para .docx.
void showReport(QTextBrowser *browser, const Structura::Report::ReportDocument &document, bool includeCover);
bool exportDocx(QWidget *parent, const Structura::Report::ReportDocument &document, const QString &suggestedName);

/// Memorial / relatorio geral do projeto: escolha do conteudo, previa e exportacao para .docx.
class ReportDialog final : public QDialog
{
    Q_OBJECT

public:
    ReportDialog(const Structura::App::Services::ReportService::Input &input, const QVector<int> &selectedMembers, QWidget *parent = nullptr);

private:
    Structura::App::Services::ReportService::Options options() const;
    void refreshPreview();
    void exportFile();

    Structura::App::Services::ReportService::Input m_input;
    QVector<int> m_selectedMembers;
    QTextBrowser *m_preview {nullptr};
    QLabel *m_status {nullptr};
    QCheckBox *m_cover {nullptr};
    QCheckBox *m_toc {nullptr};
    QCheckBox *m_criteria {nullptr};
    QCheckBox *m_materials {nullptr};
    QCheckBox *m_geometry {nullptr};
    QCheckBox *m_loads {nullptr};
    QCheckBox *m_connections {nullptr};
    QCheckBox *m_results {nullptr};
    QCheckBox *m_designSummary {nullptr};
    QCheckBox *m_designDetails {nullptr};
    QCheckBox *m_service {nullptr};
    QCheckBox *m_drawings {nullptr};
    QCheckBox *m_formulas {nullptr};
    QComboBox *m_scope {nullptr};
    QListWidget *m_items {nullptr};
};

/// Memorial de uma barra: lista de itens de calculo (como o CYPE 3D) e memorial completo.
class BarReportDialog final : public QDialog
{
    Q_OBJECT

public:
    BarReportDialog(const Structura::App::Services::ReportService::Input &input, int memberId, QWidget *parent = nullptr);

private:
    void refresh();

    Structura::App::Services::ReportService::Input m_input;
    int m_memberId {0};
    QListWidget *m_list {nullptr};
    QTextBrowser *m_view {nullptr};
    QCheckBox *m_formulas {nullptr};
};

} // namespace Structura::UI
