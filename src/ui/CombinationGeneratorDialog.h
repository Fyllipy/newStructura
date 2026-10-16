#pragma once

#include "app/services/CombinationGenerator.h"

#include <QDialog>

class QCheckBox;
class QLabel;
class QTableWidget;

namespace Structura::UI {

/// Gera combinacoes NBR 8681 a partir dos casos de carga, com tipo de acao e coeficientes editaveis.
class CombinationGeneratorDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CombinationGeneratorDialog(const QVector<Structura::App::ProjectData::LoadCase> &cases, QWidget *parent = nullptr);

    [[nodiscard]] QVector<Structura::App::ProjectData::LoadCombination> combinations() const;
    [[nodiscard]] bool replaceExisting() const;

private:
    void applyKindDefaults(int row);
    void updatePreview();
    [[nodiscard]] QVector<Structura::App::Services::CombinationGenerator::CaseSetup> setups() const;
    [[nodiscard]] Structura::App::Services::CombinationGenerator::Options options() const;

    QTableWidget *m_table {nullptr};
    QCheckBox *m_ultimate {nullptr};
    QCheckBox *m_quasi {nullptr};
    QCheckBox *m_frequent {nullptr};
    QCheckBox *m_rare {nullptr};
    QCheckBox *m_favorable {nullptr};
    QCheckBox *m_replace {nullptr};
    QLabel *m_preview {nullptr};
};

} // namespace Structura::UI
