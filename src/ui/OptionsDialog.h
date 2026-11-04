#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QSlider;

namespace Structura::UI {

/// Opcoes do programa (menu Arquivo > Opcoes). Aba "Opcoes visuais": tamanho do no na tela, com previa imediata.
class OptionsDialog final : public QDialog
{
    Q_OBJECT

public:
    OptionsDialog(double nodeScreenSize, bool darkMode, QWidget *parent = nullptr);

    [[nodiscard]] double nodeScreenSize() const;
    [[nodiscard]] bool darkMode() const;

signals:
    void nodeScreenSizeChanged(double pixels);   ///< previa enquanto o controle se move
    void darkModeChanged(bool dark);             ///< previa do tema ao trocar a opcao

private:
    QSlider *m_nodeSize {nullptr};
    QComboBox *m_theme {nullptr};
    QLabel *m_nodeSizeValue {nullptr};
};

} // namespace Structura::UI
