#pragma once

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLabel;

namespace Structura::UI {

/// Calculo guiado do fator topografico S1 (NBR 6123:2023, 5.2): terreno plano, talude ou morro (ponto B) e vale profundo.
class TopographicFactorDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit TopographicFactorDialog(double current, QWidget *parent = nullptr);

    [[nodiscard]] double s1() const;

private:
    void update();

    QComboBox *m_kind {nullptr};
    QDoubleSpinBox *m_theta {nullptr};
    QDoubleSpinBox *m_dt {nullptr};
    QDoubleSpinBox *m_z {nullptr};
    QLabel *m_result {nullptr};
};

} // namespace Structura::UI
