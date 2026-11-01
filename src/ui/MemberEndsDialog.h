#pragma once

#include "app/ProjectDataTypes.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;

namespace Structura::UI {

/// Ligacoes de extremidade das barras: rigidez rotacional (semirrigidez) e rotulas plasticas bilineares com interacao N-M, em cada extremidade.
class MemberEndsDialog final : public QDialog
{
    Q_OBJECT

public:
    /// Valores sugeridos a partir da barra e da secao: momentos de plastificacao (kN.m) e rigidez elastica de referencia (kN.m/rad) em torno de y e z locais. `stiffness*` ja e a rigidez sugerida para a rotula (1000 EI/L).
    struct Defaults {
        double yieldMomentY {0.0};
        double yieldMomentZ {0.0};
        double stiffnessY {0.0};
        double stiffnessZ {0.0};
        double axialYield {0.0};   ///< kN
    };

    MemberEndsDialog(const Structura::App::ProjectData::MemberConnectionSpec &initial, const Defaults &defaults, int selectedCount, QWidget *parent = nullptr);

    /// Especificacao editada (o identificador da barra e definido por quem chama).
    [[nodiscard]] Structura::App::ProjectData::MemberConnectionSpec spec() const;

    void accept() override;

private:
    struct Row {
        QComboBox *mode {nullptr};          ///< continuo / semirrigido
        QDoubleSpinBox *stiffness {nullptr};
    };
    struct HingeRow {
        QCheckBox *enabled {nullptr};
        QDoubleSpinBox *yield {nullptr};
        QDoubleSpinBox *stiffness {nullptr};
        QDoubleSpinBox *ratio {nullptr};
        QDoubleSpinBox *axial {nullptr};
        QDoubleSpinBox *exponent {nullptr};
    };

    Defaults m_defaults;
    Row m_rows[2][3];        // [extremidade][rx, ry, rz]
    HingeRow m_hinges[2][2]; // [extremidade][ry, rz]
};

} // namespace Structura::UI
