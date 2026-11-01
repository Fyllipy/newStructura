#pragma once

#include "app/services/EffectiveLengthService.h"

#include <QDialog>

#include <functional>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QTableWidget;

namespace Structura::UI {

/// Comprimentos efetivos das barras comprimidas a partir dos modos de flambagem, com filtros de significancia e aplicacao ao dimensionamento (Kx, Ky).
class EffectiveLengthDialog final : public QDialog
{
    Q_OBJECT

public:
    /// `recompute` devolve o resultado para as opcoes dadas; `forceToKn` converte a forca do modelo em kN e `lengthToM` o comprimento em m.
    EffectiveLengthDialog(std::function<Structura::App::Services::EffectiveLengthService::Output(const Structura::App::Services::EffectiveLengthService::Options &)> recompute,
                          double forceToKn, double lengthToM, QWidget *parent = nullptr);

    struct Coefficients {
        int memberId {0};
        bool hasX {false}, hasY {false};
        double kx {1.0}, ky {1.0};
        double lx {0.0}, ly {0.0};   ///< comprimento efetivo explicito (m) quando a barra faz parte de um grupo de trechos colineares; 0 = usar K x L
    };
    /// Coeficientes a gravar nos parametros de dimensionamento (apenas os escolhidos para aplicar).
    [[nodiscard]] QVector<Coefficients> coefficientsToApply() const { return m_apply; }

private:
    void refresh();

    std::function<Structura::App::Services::EffectiveLengthService::Output(const Structura::App::Services::EffectiveLengthService::Options &)> m_recompute;
    double m_forceToKn {1.0};
    double m_lengthToM {1.0};
    QDoubleSpinBox *m_participation {nullptr};
    QDoubleSpinBox *m_axialRatio {nullptr};
    QCheckBox *m_suspicious {nullptr};
    QLabel *m_summary {nullptr};
    QTableWidget *m_table {nullptr};
    Structura::App::Services::EffectiveLengthService::Output m_output;
    QVector<Coefficients> m_apply;
};

} // namespace Structura::UI
