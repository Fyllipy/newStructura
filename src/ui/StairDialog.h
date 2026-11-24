#pragma once

#include "app/services/StairGenerator.h"

#include <QDialog>
#include <QStringList>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace Structura::UI {

class StairPreview;

/// Nível de referência (nome e cota em m) oferecido como atalho para as cotas inicial e final.
struct StairLevel {
    QString name;
    double elevation {0.0};
};

struct StairRequest {
    QString name;
    Structura::App::Services::StairGenerator::Params params;
    int stringerSection {0}, treadSection {0}, landingSection {0}, material {0};   ///< índices nas listas informadas
};

/// Gerador de escadas: tipo, níveis, perfis, apoios e carregamentos associados a casos de carga.
class StairDialog final : public QDialog
{
    Q_OBJECT

public:
    /// `initial` (opcional) preenche o diálogo para editar uma escada já gerada.
    StairDialog(const QStringList &sections, const QStringList &materials, const QStringList &loadCases, const QVector<StairLevel> &levels,
                const StairRequest *initial, QWidget *parent = nullptr);
    [[nodiscard]] StairRequest request() const;

private:
    void updateState();
    [[nodiscard]] Structura::App::Services::StairGenerator::Params currentParams() const;
    void addLoadRow(Structura::App::Services::StairGenerator::LoadKind kind, double value, const QString &loadCase);

    QStringList m_loadCases;
    QVector<StairLevel> m_levels;

    QLineEdit *m_name {nullptr};
    QComboBox *m_type {nullptr}, *m_layout {nullptr}, *m_baseLevel {nullptr}, *m_topLevel {nullptr}, *m_turn {nullptr};
    QDoubleSpinBox *m_baseZ {nullptr}, *m_topZ {nullptr}, *m_riser {nullptr}, *m_tread {nullptr}, *m_width {nullptr};
    QDoubleSpinBox *m_x0 {nullptr}, *m_y0 {nullptr}, *m_azimuth {nullptr}, *m_x1 {nullptr}, *m_y1 {nullptr};
    QCheckBox *m_endByPoint {nullptr};
    QSpinBox *m_firstSteps {nullptr};
    QDoubleSpinBox *m_landing {nullptr}, *m_gap {nullptr}, *m_outerR {nullptr}, *m_innerR {nullptr}, *m_angle {nullptr};
    QComboBox *m_stringer {nullptr}, *m_tread2 {nullptr}, *m_landingSection {nullptr}, *m_material {nullptr}, *m_baseSupport {nullptr}, *m_topSupport {nullptr};
    QTableWidget *m_loads {nullptr};
    QLabel *m_info {nullptr};
    StairPreview *m_preview {nullptr};
};

} // namespace Structura::UI
