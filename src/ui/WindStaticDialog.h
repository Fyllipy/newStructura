#pragma once

#include "app/services/WindStaticService.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QTableWidget;

namespace Structura::UI {

/// Vento estatico automatico em edificios (NBR 6123:2023): parametros, planta e pavimentos, direcoes e excentricidade, com pre-visualizacao das forcas por pavimento.
class WindStaticDialog final : public QDialog
{
    Q_OBJECT

public:
    /// `planBounds` = xmin, xmax, ymin, ymax (m); `levels` = cotas detectadas (m).
    WindStaticDialog(const std::array<double, 4> &planBounds, const QVector<double> &levels, QWidget *parent = nullptr);

    [[nodiscard]] Structura::App::Services::WindStaticService::Parameters parameters() const;
    [[nodiscard]] Structura::App::Services::WindStaticService::Building building() const;
    [[nodiscard]] int directions() const;
    [[nodiscard]] int eccentricityMode() const;
    /// Calculo atual (o mesmo da pre-visualizacao).
    [[nodiscard]] Structura::App::Services::WindStaticService::Result result() const;

private:
    void updatePreview();
    [[nodiscard]] QVector<double> parseLevels(bool *ok) const;

    QDoubleSpinBox *m_v0 {nullptr};
    QDoubleSpinBox *m_s1 {nullptr};
    QDoubleSpinBox *m_s3 {nullptr};
    QDoubleSpinBox *m_parapet {nullptr};
    QComboBox *m_category {nullptr};
    QCheckBox *m_neighbourhood {nullptr};
    QCheckBox *m_turbulence {nullptr};
    QDoubleSpinBox *m_plan[4] {nullptr, nullptr, nullptr, nullptr};
    QLineEdit *m_levels {nullptr};
    QCheckBox *m_directions[4] {nullptr, nullptr, nullptr, nullptr};
    QComboBox *m_eccentricity {nullptr};
    QLabel *m_summary {nullptr};
    QTableWidget *m_table {nullptr};
};

} // namespace Structura::UI
