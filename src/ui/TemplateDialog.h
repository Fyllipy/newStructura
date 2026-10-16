#pragma once

#include "app/services/ModelTemplateService.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QStackedWidget;

namespace Structura::UI {

/// Assistente de modelos prontos: galpao de duas aguas ou edificio de porticos.
class TemplateDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit TemplateDialog(QWidget *parent = nullptr);

    /// Texto do .dat gerado com os parametros escolhidos (vazio em caso de erro; `error` descreve).
    [[nodiscard]] QString generate(QString *error) const;
    [[nodiscard]] QString suggestedName() const;

private:
    QComboBox *m_kind {nullptr};
    QStackedWidget *m_stack {nullptr};
    // galpao
    QDoubleSpinBox *m_span {nullptr};
    QDoubleSpinBox *m_eave {nullptr};
    QDoubleSpinBox *m_slope {nullptr};
    QSpinBox *m_frames {nullptr};
    QDoubleSpinBox *m_frameSpacing {nullptr};
    QSpinBox *m_panels {nullptr};
    QComboBox *m_shedColumn {nullptr};
    QComboBox *m_shedRafter {nullptr};
    QCheckBox *m_pinned {nullptr};
    QCheckBox *m_roofBracing {nullptr};
    QDoubleSpinBox *m_roofLive {nullptr};
    QDoubleSpinBox *m_roofDead {nullptr};
    // edificio
    QSpinBox *m_baysX {nullptr};
    QSpinBox *m_baysY {nullptr};
    QSpinBox *m_stories {nullptr};
    QDoubleSpinBox *m_bayX {nullptr};
    QDoubleSpinBox *m_bayY {nullptr};
    QDoubleSpinBox *m_storyHeight {nullptr};
    QComboBox *m_column {nullptr};
    QComboBox *m_beamX {nullptr};
    QComboBox *m_beamY {nullptr};
    QCheckBox *m_bracing {nullptr};
    QDoubleSpinBox *m_floorDead {nullptr};
    QDoubleSpinBox *m_floorLive {nullptr};
};

} // namespace Structura::UI
