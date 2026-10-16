#pragma once

#include "app/services/AreaLoadService.h"

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QStackedWidget;

namespace Structura::UI {

/// Resultado do dialogo: carga de area generica ou vento (NBR 6123) ja convertido em pressao.
struct AreaLoadRequest {
    QString caseName;
    double pressure {0.0};      ///< kN/m2 (com sinal) no sentido indicado
    int directionKind {0};      ///< 0 = vetor fixo, 1 = normal para fora da estrutura, 2 = normal para dentro
    Structura::App::Services::AreaLoadService::Vec3 direction {0.0, 0.0, -1.0};
    bool projected {false};
    double manualWidth {0.0};
    double overhang {0.0};
    bool wind {false};
    QString description;
};

/// Carga de area (pressao sobre superficie) e vento NBR 6123, distribuidos nas barras selecionadas.
class AreaLoadDialog final : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { Area, Wind };

    AreaLoadDialog(Mode mode, const QStringList &caseNames, const QString &defaultCase, double structureHeight, int selectedBars,
                   QWidget *parent = nullptr);

    [[nodiscard]] AreaLoadRequest request() const;

private:
    void updateWindLabels();
    void applyFacePreset(int index);

    Mode m_mode;
    QComboBox *m_case {nullptr};
    QDoubleSpinBox *m_manualWidth {nullptr};
    QDoubleSpinBox *m_overhang {nullptr};
    // area
    QDoubleSpinBox *m_pressure {nullptr};
    QComboBox *m_direction {nullptr};
    QCheckBox *m_projected {nullptr};
    // vento
    QDoubleSpinBox *m_v0 {nullptr};
    QDoubleSpinBox *m_s1 {nullptr};
    QDoubleSpinBox *m_s3 {nullptr};
    QComboBox *m_category {nullptr};
    QComboBox *m_class {nullptr};
    QDoubleSpinBox *m_height {nullptr};
    QComboBox *m_windDirection {nullptr};
    QComboBox *m_facePreset {nullptr};
    QDoubleSpinBox *m_cpe {nullptr};
    QDoubleSpinBox *m_ci {nullptr};
    QLabel *m_windResult {nullptr};
};

} // namespace Structura::UI
