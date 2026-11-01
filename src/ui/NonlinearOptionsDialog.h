#pragma once

#include "app/ProjectDataTypes.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;

namespace Structura::UI {

/// Controle da analise nao linear incremental: passos, iteracoes, criterios de convergencia, controle de carga ou de deslocamento.
class NonlinearOptionsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit NonlinearOptionsDialog(const Structura::App::ProjectData::NonlinearSettings &initial, QWidget *parent = nullptr);

    [[nodiscard]] Structura::App::ProjectData::NonlinearSettings settings() const;

private:
    void updateControlFields();

    QCheckBox *m_force {nullptr};
    QSpinBox *m_steps {nullptr};
    QSpinBox *m_iterations {nullptr};
    QSpinBox *m_cutbacks {nullptr};
    QDoubleSpinBox *m_tolerance {nullptr};
    QCheckBox *m_criterionForce {nullptr};
    QCheckBox *m_criterionDisplacement {nullptr};
    QCheckBox *m_criterionEnergy {nullptr};
    QComboBox *m_control {nullptr};
    QDoubleSpinBox *m_loadFactor {nullptr};
    QSpinBox *m_node {nullptr};
    QComboBox *m_dof {nullptr};
    QDoubleSpinBox *m_target {nullptr};
};

} // namespace Structura::UI
