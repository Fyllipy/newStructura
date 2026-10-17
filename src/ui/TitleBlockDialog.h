#pragma once

#include "app/ProjectDataTypes.h"
#include "drawing/DrawingModel.h"

#include <QDialog>

class QLineEdit;
class QPlainTextEdit;
class QTableWidget;

namespace Structura::UI {

class SheetPreviewWidget;

/// Edicao do carimbo: campos, tabela de revisoes, notas gerais e titulos personalizados das pranchas.
class TitleBlockDialog final : public QDialog
{
    Q_OBJECT

public:
    struct ViewTitle {
        QString id;
        QString defaultTitle;
    };

    TitleBlockDialog(const Structura::App::ProjectData::TitleBlockSettings &settings, const QVector<ViewTitle> &views, QWidget *parent = nullptr);

    [[nodiscard]] Structura::App::ProjectData::TitleBlockSettings settings() const;

private:
    void refreshPreview();

    QVector<ViewTitle> m_views;
    QLineEdit *m_company {nullptr};
    QLineEdit *m_client {nullptr};
    QLineEdit *m_site {nullptr};
    QLineEdit *m_project {nullptr};
    QLineEdit *m_author {nullptr};
    QLineEdit *m_registration {nullptr};
    QLineEdit *m_drafter {nullptr};
    QLineEdit *m_checker {nullptr};
    QLineEdit *m_date {nullptr};
    QLineEdit *m_revision {nullptr};
    QLineEdit *m_prefix {nullptr};
    QPlainTextEdit *m_notes {nullptr};
    QTableWidget *m_revisions {nullptr};
    QTableWidget *m_titles {nullptr};
    SheetPreviewWidget *m_preview {nullptr};
    std::vector<Structura::Drawing::Sheet> m_previewSheet;
};

} // namespace Structura::UI
