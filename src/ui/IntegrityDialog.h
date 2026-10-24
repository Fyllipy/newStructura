#pragma once

#include "app/services/IntegrityService.h"

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLabel;
class QTableWidget;

namespace Structura::UI {

/// Integridade estrutural (NBR 8800:2024, 4.13): opcoes e quadro de verificacoes. `compute` recalcula as linhas com as opcoes escolhidas.
class IntegrityDialog final : public QDialog
{
    Q_OBJECT

public:
    using Compute = std::function<QVector<Structura::App::Services::IntegrityService::Row>(const Structura::App::Services::IntegrityService::Options &)>;
    IntegrityDialog(const QStringList &combinations, Compute compute, QWidget *parent = nullptr);

private:
    void refresh();

    Compute m_compute;
    QCheckBox *m_roof {nullptr};
    QCheckBox *m_collapse {nullptr};
    QComboBox *m_combination {nullptr};
    QLabel *m_summary {nullptr};
    QTableWidget *m_table {nullptr};
};

} // namespace Structura::UI
