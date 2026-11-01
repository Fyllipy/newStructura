#pragma once

#include "app/ProjectDataTypes.h"

#include <QDialog>
#include <QString>
#include <QStringList>

#include <array>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;

/// Carga geral de barra com posicao: trapezoidal/parcial, forca pontual, momento pontual, temperatura e retracao.
class MemberActionDialog : public QDialog
{
    Q_OBJECT

public:
    struct Values {
        int kind {Structura::App::ProjectData::MemberActionKind::kTrapezoid};
        QString system;
        bool percent {false};   ///< posicoes a/b em % do comprimento de cada barra
        double a {0.0};
        double b {0.0};
        std::array<double, 3> v1 {{0.0, 0.0, 0.0}};
        std::array<double, 3> v2 {{0.0, 0.0, 0.0}};
    };

    explicit MemberActionDialog(QWidget *parent = nullptr);

    void setSelectedCount(int count);
    /// Casos de carga disponiveis e o caso inicialmente selecionado.
    void setLoadCases(const QStringList &names, const QString &current);
    [[nodiscard]] QString loadCase() const;
    /// Comprimento da barra editada (m): limita as posicoes e mostra a faixa valida; 0 = varias barras.
    void setMemberLength(double length);
    /// Edicao de uma carga existente (o tipo fica travado).
    void setInitial(const Structura::App::ProjectData::MemberAction &action);
    [[nodiscard]] Values values() const;

private:
    void updateForKind();
    [[nodiscard]] int kind() const;

    QComboBox *m_kindCombo {nullptr};
    QComboBox *m_caseCombo {nullptr};
    QComboBox *m_systemCombo {nullptr};
    QCheckBox *m_percent {nullptr};
    QDoubleSpinBox *m_aSpin {nullptr};
    QDoubleSpinBox *m_bSpin {nullptr};
    std::array<QDoubleSpinBox *, 3> m_v1 {{nullptr, nullptr, nullptr}};
    std::array<QDoubleSpinBox *, 3> m_v2 {{nullptr, nullptr, nullptr}};
    QFormLayout *m_form {nullptr};
    QLabel *m_selectionLabel {nullptr};
    QLabel *m_hint {nullptr};
    double m_length {0.0};
};
