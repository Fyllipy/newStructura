#pragma once

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QRadioButton;

namespace Structura::UI {

/// Escolha das cargas a excluir: tipos, caso de carga e abrangencia (selecao ou modelo inteiro).
class DeleteLoadsDialog final : public QDialog
{
    Q_OBJECT

public:
    DeleteLoadsDialog(const QStringList &caseNames, int selectedNodes, int selectedBars, QWidget *parent = nullptr);

    [[nodiscard]] bool nodalForces() const;
    [[nodiscard]] bool distributedForces() const;
    [[nodiscard]] bool distributedMoments() const;
    [[nodiscard]] bool areaPanels() const;
    /// Vazio = todos os casos.
    [[nodiscard]] QString loadCase() const;
    [[nodiscard]] bool wholeModel() const;

private:
    QCheckBox *m_nodal {nullptr};
    QCheckBox *m_distributed {nullptr};
    QCheckBox *m_moments {nullptr};
    QCheckBox *m_panels {nullptr};
    QComboBox *m_case {nullptr};
    QRadioButton *m_selection {nullptr};
    QRadioButton *m_all {nullptr};
};

} // namespace Structura::UI
