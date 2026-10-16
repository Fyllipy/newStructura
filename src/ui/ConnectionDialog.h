#pragma once

#include "design/Connections.h"

#include <QDialog>

#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QTabWidget;
class QTableWidget;

namespace Structura::UI {

/// Envoltoria dos esforcos de extremidade de uma barra (kN, kN.m), obtida das combinacoes ultimas.
struct EndForceEnvelope {
    bool ok {false};
    QString label;
    double n {0.0};    ///< maior tracao (kN)
    double nc {0.0};   ///< maior compressao (kN, positivo)
    double vy {0.0};
    double vz {0.0};
    double mx {0.0};
    double my {0.0};
    double t {0.0};
};

/// Verificacao de ligacoes de aco: grupo de parafusos (NBR 8800:2024, 6.3) e filetes de solda (6.2).
class ConnectionDialog final : public QDialog
{
    Q_OBJECT

public:
    using Provider = std::function<EndForceEnvelope(bool atEnd)>;
    explicit ConnectionDialog(Provider provider, QWidget *parent = nullptr);

private:
    void fetch(bool atEnd);
    void verify();
    void showChecks(const std::vector<Structura::Design::Connections::Check> &checks);

    Provider m_provider;
    QTabWidget *m_tabs {nullptr};
    QLabel *m_origin {nullptr};
    // esforcos
    QDoubleSpinBox *m_vx {nullptr};
    QDoubleSpinBox *m_vy {nullptr};
    QDoubleSpinBox *m_mz {nullptr};
    QDoubleSpinBox *m_tension {nullptr};
    // parafusos
    QComboBox *m_grade {nullptr};
    QDoubleSpinBox *m_db {nullptr};
    QCheckBox *m_thread {nullptr};
    QSpinBox *m_planes {nullptr};
    QSpinBox *m_nx {nullptr};
    QSpinBox *m_ny {nullptr};
    QDoubleSpinBox *m_sx {nullptr};
    QDoubleSpinBox *m_sy {nullptr};
    QDoubleSpinBox *m_t {nullptr};
    QDoubleSpinBox *m_fu {nullptr};
    QDoubleSpinBox *m_le {nullptr};
    QDoubleSpinBox *m_lt {nullptr};
    QCheckBox *m_deformation {nullptr};
    // solda
    QDoubleSpinBox *m_leg {nullptr};
    QDoubleSpinBox *m_length {nullptr};
    QComboBox *m_electrode {nullptr};
    QDoubleSpinBox *m_angle {nullptr};
    QDoubleSpinBox *m_wt {nullptr};
    QDoubleSpinBox *m_wfy {nullptr};
    QDoubleSpinBox *m_wfu {nullptr};
    QDoubleSpinBox *m_force {nullptr};
    QTableWidget *m_table {nullptr};
    QLabel *m_verdict {nullptr};
};

} // namespace Structura::UI
