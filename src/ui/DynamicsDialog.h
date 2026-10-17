#pragma once

#include "app/services/DynamicsService.h"
#include "app/services/WindDynamicsService.h"

#include <QDialog>

#include <functional>
#include <memory>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QRadioButton;
class QSpinBox;
class QTableWidget;
class QTabWidget;

namespace Structura::UI {

class LineChartWidget;

/// Modelo e modos usados pela dinamica (preenchidos pelo MainWindow; vivem enquanto o dialogo existir).
struct DynamicsData {
    Structura::App::SolverBridge::ModelData model;
    Structura::App::SolverBridge::Result modal;
    std::vector<double> mass;
    bool valid {false};

    [[nodiscard]] Structura::App::Services::DynamicsService::Basis basis() const
    {
        Structura::App::Services::DynamicsService::Basis b;
        b.model = &model;
        b.modal = &modal;
        b.mass = mass;
        return b;
    }
};

/// Dinamica: modos, espectro de resposta (SRSS/CQC), vibracao de pisos (NBR 8800:2024, Anexo I) e resposta no tempo.
/// Forcas nodais de um caso de carga de vento dinamico a criar no modelo.
struct WindCaseRequest {
    QString name;
    std::vector<int> nodeIds;
    std::vector<std::array<double, 3>> forces;   ///< kN (X, Y, Z)
};

class DynamicsDialog final : public QDialog
{
    Q_OBJECT

public:
    using ComputeModes = std::function<bool(const QString &source, int modes, QString *error)>;

    DynamicsDialog(const QStringList &massSources, std::shared_ptr<DynamicsData> data, ComputeModes compute, QWidget *parent = nullptr);

signals:
    void windCaseRequested(const Structura::UI::WindCaseRequest &request);

private:
    void computeModes();
    void runWind();
    void createWindCases();
    void refreshModes();
    void runSpectrum();
    void exportSpectrum();
    void updateSpectrumChart();
    void runFloor();
    void runTime();
    void updateTimeChart();
    void updateDamping();
    [[nodiscard]] bool requireModes();

    std::shared_ptr<DynamicsData> m_data;
    ComputeModes m_compute;
    QComboBox *m_source {nullptr};
    QSpinBox *m_modeCount {nullptr};
    QTableWidget *m_modeTable {nullptr};
    QLabel *m_modeStatus {nullptr};
    QTabWidget *m_tabs {nullptr};

    // espectro
    QRadioButton *m_parametric {nullptr};
    QDoubleSpinBox *m_plateau {nullptr}, *m_sa1 {nullptr}, *m_tl {nullptr};
    QPlainTextEdit *m_table {nullptr};
    QDoubleSpinBox *m_reduction {nullptr}, *m_importance {nullptr}, *m_damping {nullptr};
    QComboBox *m_combine {nullptr}, *m_directional {nullptr};
    QCheckBox *m_dir[3] {nullptr, nullptr, nullptr};
    QSpinBox *m_usedModes {nullptr};
    LineChartWidget *m_spectrumChart {nullptr};
    QLabel *m_spectrumSummary {nullptr};
    QTableWidget *m_contribTable {nullptr}, *m_dispTable {nullptr}, *m_forceTable {nullptr};
    Structura::App::Services::DynamicsService::SpectrumResult m_spectrumResult;

    // piso
    QComboBox *m_floorMode {nullptr}, *m_occupancy {nullptr};
    QCheckBox *m_ceiling {nullptr};
    QDoubleSpinBox *m_partitions {nullptr}, *m_floorDamping {nullptr}, *m_person {nullptr};
    QRadioButton *m_peakCriterion {nullptr};
    QLabel *m_floorResult {nullptr};

    // vento dinamico
    QComboBox *m_windMode {nullptr}, *m_windDir {nullptr}, *m_windCategory {nullptr};
    QDoubleSpinBox *m_windV0 {nullptr}, *m_windS1 {nullptr}, *m_windS3 {nullptr}, *m_windWidth {nullptr}, *m_windCa {nullptr}, *m_windXi {nullptr};
    QCheckBox *m_windFluctOnly {nullptr}, *m_windCross {nullptr};
    QLabel *m_windSummary {nullptr};
    QTableWidget *m_windTable {nullptr};
    Structura::App::Services::WindDynamicsService::Result m_windResult;
    Structura::App::Services::WindDynamicsService::Input m_windInput;

    // tempo
    QComboBox *m_kind {nullptr}, *m_loadNode {nullptr}, *m_loadDir {nullptr}, *m_respNode {nullptr}, *m_respDir {nullptr}, *m_quantity {nullptr};
    QDoubleSpinBox *m_amp {nullptr}, *m_freq {nullptr}, *m_pulse {nullptr}, *m_duration {nullptr}, *m_timeDamping {nullptr};
    QPlainTextEdit *m_forceTableText {nullptr};
    QSpinBox *m_timeModes {nullptr};
    LineChartWidget *m_timeChart {nullptr};
    QLabel *m_timeSummary {nullptr};
    Structura::App::Services::DynamicsService::TimeResult m_timeResult;
};

} // namespace Structura::UI
