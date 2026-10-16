#pragma once

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
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
