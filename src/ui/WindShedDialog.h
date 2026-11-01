#pragma once

#include "app/services/WindShedService.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QTableWidget;

namespace Structura::UI {

/// Vento em galpoes (NBR 6123:2023, Tabelas 6 a 8 e pressao interna 6.3.2): parametros, abertura e pre-visualizacao dos casos gerados a partir das barras do modelo.
class WindShedDialog final : public QDialog
{
    Q_OBJECT

public:
    WindShedDialog(const QVector<Structura::App::Services::WindShedService::Bar> &bars, QWidget *parent = nullptr);

    [[nodiscard]] Structura::App::Services::WindShedService::Parameters parameters() const;
    [[nodiscard]] Structura::App::Services::WindShedService::Result result() const;

private:
    void updatePreview();
    void pickTopography();

    QVector<Structura::App::Services::WindShedService::Bar> m_bars;
    QDoubleSpinBox *m_v0 {nullptr};
    QDoubleSpinBox *m_s1 {nullptr};
    QDoubleSpinBox *m_s3 {nullptr};
    QComboBox *m_category {nullptr};
    QComboBox *m_ridge {nullptr};
    QComboBox *m_reference {nullptr};
    QComboBox *m_cpiMode {nullptr};
    QDoubleSpinBox *m_ratio {nullptr};
    QComboBox *m_permeable {nullptr};
    QDoubleSpinBox *m_cpiFirst {nullptr};
    QDoubleSpinBox *m_cpiSecond {nullptr};
    QDoubleSpinBox *m_overhang {nullptr};
    QCheckBox *m_transverse {nullptr};
    QCheckBox *m_longitudinal {nullptr};
    QLabel *m_summary {nullptr};
    QTableWidget *m_table {nullptr};
    QPlainTextEdit *m_notes {nullptr};
};

} // namespace Structura::UI
