#pragma once

#include <QDialog>

#include <array>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

namespace Structura::UI {

/// Copia em serie: N copias deslocadas por um passo (dx, dy, dz) acumulado.
class SerialCopyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SerialCopyDialog(QWidget *parent = nullptr);

    [[nodiscard]] int count() const;
    [[nodiscard]] double dx() const;
    [[nodiscard]] double dy() const;
    [[nodiscard]] double dz() const;

private:
    QSpinBox *m_count;
    QDoubleSpinBox *m_dx;
    QDoubleSpinBox *m_dy;
    QDoubleSpinBox *m_dz;
};

/// Copia em matriz (array) nas duas direcoes: cada direcao tem um eixo global (X, Y ou Z), um espacamento e um numero de copias.
/// O resultado e uma grade de (n1 + 1) x (n2 + 1) posicoes, a original inclusive; as n1 x n2 + n1 + n2 posicoes novas recebem copias.
class ArrayCopyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ArrayCopyDialog(QWidget *parent = nullptr);

    /// Vetor (m) deslocamento de uma copia: `i` passos na direcao 1 e `j` na direcao 2.
    [[nodiscard]] std::array<double, 3> offset(int i, int j) const;
    [[nodiscard]] int count1() const;
    [[nodiscard]] int count2() const;
    [[nodiscard]] int newPositions() const { return (count1() + 1) * (count2() + 1) - 1; }
    /// Texto de erro se os dados forem incoerentes (vazio = valido).
    [[nodiscard]] QString validationError() const;

private:
    void refresh();
    int axis1() const;
    int axis2() const;

    QComboBox *m_axis1;
    QComboBox *m_axis2;
    QDoubleSpinBox *m_spacing1;
    QDoubleSpinBox *m_spacing2;
    QSpinBox *m_count1;
    QSpinBox *m_count2;
    QLabel *m_summary;
};

/// Rotacao em torno do eixo normal ao plano de construcao, pelo ponto clicado.
class RotateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RotateDialog(QWidget *parent = nullptr);

    [[nodiscard]] double angleDegrees() const;
    [[nodiscard]] bool copy() const;

private:
    QDoubleSpinBox *m_angle;
    QCheckBox *m_copy;
};

/// Espelhamento: manter ou nao os elementos originais.
class MirrorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MirrorDialog(QWidget *parent = nullptr);

    [[nodiscard]] bool keepOriginals() const;

private:
    QCheckBox *m_keep;
};

} // namespace Structura::UI
