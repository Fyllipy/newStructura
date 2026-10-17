#pragma once

#include <QDialog>

#include <vector>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QSpinBox;

namespace Structura::UI {

/// Dividir barras: em N partes iguais, a cada X m, ou em distancias informadas (do inicio ou do fim).
class SplitBarDialog final : public QDialog
{
    Q_OBJECT

public:
    SplitBarDialog(int barCount, double shortest, double longest, QWidget *parent = nullptr);

    /// Fracoes (0..1) do comprimento `length` em que cada barra deve ser cortada, em ordem crescente.
    [[nodiscard]] std::vector<double> fractions(double length) const;

private:
    void updatePreview();

    QRadioButton *m_equal {nullptr};
    QRadioButton *m_every {nullptr};
    QRadioButton *m_list {nullptr};
    QSpinBox *m_parts {nullptr};
    QDoubleSpinBox *m_step {nullptr};
    QLineEdit *m_distances {nullptr};
    QCheckBox *m_fromEnd {nullptr};
    QLabel *m_preview {nullptr};
    int m_barCount {0};
    double m_shortest {0.0}, m_longest {0.0};
};

} // namespace Structura::UI
