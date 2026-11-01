#pragma once

#include "app/services/AreaLoadService.h"

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;

namespace Structura::UI {

/// Resultado do dialogo de carga de area.
struct AreaLoadRequest {
    QString caseName;
    double pressure {0.0};      ///< kN/m2 (com sinal) no sentido indicado
    int directionKind {0};      ///< 0 = vetor fixo, 1 = normal para fora da estrutura, 2 = normal para dentro
    Structura::App::Services::AreaLoadService::Vec3 direction {0.0, 0.0, -1.0};
    bool projected {false};
    double manualWidth {0.0};
    double overhang {0.0};
    QString description;
};

/// Carga de area (pressao sobre superficie) distribuida nas barras selecionadas.
class AreaLoadDialog final : public QDialog
{
    Q_OBJECT

public:
    AreaLoadDialog(const QStringList &caseNames, const QString &defaultCase, int selectedBars, QWidget *parent = nullptr);

    [[nodiscard]] AreaLoadRequest request() const;

private:
    QComboBox *m_case {nullptr};
    QDoubleSpinBox *m_manualWidth {nullptr};
    QDoubleSpinBox *m_overhang {nullptr};
    // area
    QDoubleSpinBox *m_pressure {nullptr};
    QComboBox *m_direction {nullptr};
    QCheckBox *m_projected {nullptr};
};

} // namespace Structura::UI
