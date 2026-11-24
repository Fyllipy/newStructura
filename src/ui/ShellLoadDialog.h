#pragma once

#include "shell/ShellModel.h"

#include <QDialog>
#include <QStringList>

class QComboBox;
class QDoubleSpinBox;
class QLabel;

namespace Structura::UI {

/// Escolha da carga a aplicar aos nos de casca selecionados na vista 3D: forca nos nos, carga linear sobre as arestas ou carga de superficie
/// sobre os elementos cujos nos estao todos selecionados. Devolve uma `Load` modelo (caso, tipo e valores); os pontos sao ligados por quem chama.
class ShellLoadDialog : public QDialog
{
    Q_OBJECT

public:
    /// `nodes`, `edges`, `elements`: alvos da selecao atual (um tipo so pode ser escolhido se tem alvos).
    ShellLoadDialog(const QStringList &caseNames, int nodes, int edges, int elements, QWidget *parent = nullptr);

    [[nodiscard]] Structura::Shell::Load load() const;

private:
    void updateFields();

    QComboBox *m_case {nullptr};
    QComboBox *m_type {nullptr};
    QComboBox *m_mode {nullptr};
    std::array<QDoubleSpinBox *, 3> m_value {};
    QLabel *m_unit {nullptr};
    QLabel *m_hint {nullptr};
    int m_nodes {0}, m_edges {0}, m_elements {0};
};

} // namespace Structura::UI
