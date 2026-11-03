#pragma once

#include <QDialog>

class QLabel;
class QSlider;

namespace Structura::UI {

/// Opcoes do programa (menu Arquivo > Opcoes). Aba "Opcoes visuais": tamanho do no na tela, com previa imediata.
class OptionsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit OptionsDialog(double nodeScreenSize, QWidget *parent = nullptr);

    [[nodiscard]] double nodeScreenSize() const;

signals:
    void nodeScreenSizeChanged(double pixels);   ///< previa enquanto o controle se move

private:
    QSlider *m_nodeSize {nullptr};
    QLabel *m_nodeSizeValue {nullptr};
};

} // namespace Structura::UI
