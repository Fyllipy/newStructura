#pragma once

#include "app/services/ModelCheckService.h"

#include <QDialog>

class QLabel;
class QListWidget;

namespace Structura::UI {

/// Resultado da checagem do modelo. Em modo `blocking` (antes da analise) oferece "Analisar mesmo assim".
class ModelCheckDialog final : public QDialog
{
    Q_OBJECT

public:
    ModelCheckDialog(const QVector<Structura::App::Services::ModelCheckService::Issue> &issues, bool blocking, QWidget *parent = nullptr);

signals:
    /// Duplo clique em um problema com elemento associado.
    void focusRequested(Structura::App::Services::ModelCheckService::TargetKind kind, int id);

private:
    QVector<Structura::App::Services::ModelCheckService::Issue> m_issues;
    QListWidget *m_list {nullptr};
};

} // namespace Structura::UI
