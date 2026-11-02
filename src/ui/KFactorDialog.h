#pragma once

#include "app/ProjectDataTypes.h"

#include <QDialog>
#include <QVector>
#include <QWidget>

class QButtonGroup;
class QComboBox;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QRadioButton;

namespace Structura::UI {

/// Como o comprimento de flambagem de um eixo e definido.
struct KAxisSetting {
    enum class Mode {
        Keep,          ///< nao altera
        Type,          ///< K do tipo de vinculacao escolhido no desenho (valor teorico ou recomendado)
        Coefficient,   ///< K arbitrario
        Length,        ///< comprimento efetivo informado (m)
    };
    Mode mode {Mode::Keep};
    double value {1.0};   ///< K (Type e Coefficient) ou comprimento efetivo em m (Length)
};

struct KFactorSettings {
    KAxisSetting x, y, z;
};

/// Um tipo de vinculacao de extremidades com seus valores de K (teorico e recomendado).
struct KType {
    const char *name;
    double theoretical;
    double recommended;
};
const QVector<KType> &kTypes();

/// Cartao clicavel com o desenho da deformada de flambagem de um tipo de vinculacao e seus valores de K.
class KTypeCard final : public QWidget
{
    Q_OBJECT

public:
    KTypeCard(int index, QWidget *parent = nullptr);
    void setSelected(bool selected);
    void setRecommendedHighlight(bool recommended);
    QSize sizeHint() const override { return {128, 190}; }

signals:
    void clicked(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    int m_index;
    bool m_selected {false};
    bool m_recommended {true};
};

/// Define o comprimento de flambagem (K) das barras selecionadas: por tipo de vinculacao (com desenho), K arbitrario ou comprimento efetivo.
class KFactorDialog final : public QDialog
{
    Q_OBJECT

public:
    KFactorDialog(const QVector<int> &memberIds, const Structura::App::ProjectData::DesignMemberParams &current, QWidget *parent = nullptr);

    [[nodiscard]] KFactorSettings settings() const;

private:
    struct AxisRow {
        QComboBox *mode {nullptr};
        QDoubleSpinBox *value {nullptr};
        QLabel *unit {nullptr};
    };
    AxisRow makeRow(const QString &title, double currentK, double currentL, QGridLayout *grid, int row);
    void selectType(int index);
    void refreshTypeValue();
    void updateRow(AxisRow &row);
    [[nodiscard]] double typeValue() const;
    [[nodiscard]] KAxisSetting settingOf(const AxisRow &row) const;

    QVector<KTypeCard *> m_cards;
    int m_type {2};   // articulada-articulada
    QRadioButton *m_theoretical {nullptr};
    QRadioButton *m_recommended {nullptr};
    AxisRow m_x, m_y, m_z;
};

} // namespace Structura::UI
