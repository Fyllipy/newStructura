#pragma once

#include <QDialog>
#include <QSet>
#include <QStringList>

class QListWidget;

namespace Structura::UI {

/// Janela para escolher, por caixas de marcacao, os casos de carregamento exibidos na viewport.
class LoadCaseVisibilityDialog : public QDialog
{
    Q_OBJECT

public:
    LoadCaseVisibilityDialog(const QStringList &cases, const QSet<QString> &visible, QWidget *parent = nullptr);

    /// Casos marcados.
    [[nodiscard]] QSet<QString> visibleCases() const;
    /// Todos os casos marcados (equivale a nao filtrar).
    [[nodiscard]] bool allChecked() const;

private:
    void setAll(bool checked);

    QListWidget *m_list;
};

} // namespace Structura::UI
