#pragma once

#include "app/ProjectDataTypes.h"

#include <QDialog>
#include <QHash>

class QTableWidget;
class QTabWidget;

namespace Structura::UI {

/**
 * @brief Gerencia casos de carga (nome, sigla, peso proprio) e combinacoes (fatores por caso).
 */
class LoadCasesDialog : public QDialog
{
    Q_OBJECT

public:
    using LoadCase = Structura::App::ProjectData::LoadCase;
    using LoadCombination = Structura::App::ProjectData::LoadCombination;

    LoadCasesDialog(const QVector<LoadCase> &cases, const QVector<LoadCombination> &combinations, QWidget *parent = nullptr);

    [[nodiscard]] QVector<LoadCase> loadCases() const;
    [[nodiscard]] QVector<LoadCombination> combinations() const;
    /// Mapa nome antigo -> nome novo para casos renomeados (as cargas devem ser atualizadas).
    [[nodiscard]] QHash<QString, QString> renamedCases() const { return m_renames; }

protected:
    void accept() override;

private:
    void addCaseRow(const QString &name, const QString &abbreviation, bool selfWeight, const QString &originalName);
    void addCombinationRow(const QString &name, const QHash<QString, double> &factors);
    void rebuildCombinationColumns();
    [[nodiscard]] QStringList currentCaseNames() const;
    [[nodiscard]] QHash<QString, QHash<QString, double>> combinationFactorsSnapshot() const;

    QTabWidget *m_tabs {nullptr};
    QTableWidget *m_caseTable {nullptr};
    QTableWidget *m_comboTable {nullptr};
    QHash<QString, QString> m_renames;
    QStringList m_comboCaseColumns;
};

} // namespace Structura::UI
