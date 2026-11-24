#pragma once

#include "shell/ShellModel.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;

namespace Structura::UI {

/// Escolha do tipo de apoio a aplicar aos nos de casca selecionados na vista 3D.
/// Devolve um `Support` modelo (restricoes ou mola); a ligacao aos nos (selector `Nodes`, pontos e tolerancia) e feita por quem chama.
class ShellSupportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShellSupportDialog(int nodeCount, QWidget *parent = nullptr);

    /// Apoio escolhido (fixed, springPerArea, compressionOnly, springNormal).
    [[nodiscard]] Structura::Shell::Support support() const;

private:
    void applyPreset(int index);
    void updateEnabled();

    QComboBox *m_preset {nullptr};
    std::array<QCheckBox *, 6> m_dof {};
    QDoubleSpinBox *m_stiffness {nullptr};
    QCheckBox *m_compressionOnly {nullptr};
    QComboBox *m_normal {nullptr};
    QWidget *m_springBox {nullptr};
};

} // namespace Structura::UI
