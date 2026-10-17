#pragma once

#include "app/ProjectDataTypes.h"
#include "app/services/DesignService.h"

#include <QDialog>
#include <QStringList>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QListWidget;
class QPlainTextEdit;
class QSpinBox;
class QTableWidget;
class QLabel;
class QPushButton;

namespace Structura::UI {

/// Parametros globais do dimensionamento e parametros (comprimentos efetivos, travamentos...) das barras selecionadas.
class DesignSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    DesignSettingsDialog(const Structura::App::ProjectData::DesignSettings &settings, const QStringList &combinationNames,
                         const QVector<int> &selectedMemberIds, QWidget *parent = nullptr);

    [[nodiscard]] Structura::App::ProjectData::DesignSettings settings() const;
    /// Parametros a aplicar as barras selecionadas (vazio se nao marcado).
    [[nodiscard]] bool applyToSelection() const;
    [[nodiscard]] Structura::App::ProjectData::DesignMemberParams selectionParams() const;

private:
    struct ParamEditors {
        QComboBox *steel {nullptr};
        QDoubleSpinBox *kx {nullptr};
        QDoubleSpinBox *ky {nullptr};
        QDoubleSpinBox *kz {nullptr};
        QDoubleSpinBox *lx {nullptr};
        QDoubleSpinBox *ly {nullptr};
        QDoubleSpinBox *lz {nullptr};
        QSpinBox *segments {nullptr};
        QDoubleSpinBox *lb {nullptr};
        QCheckBox *braced {nullptr};
        QCheckBox *cbOne {nullptr};
        QDoubleSpinBox *b2 {nullptr};
        QDoubleSpinBox *netArea {nullptr};
        QSpinBox *defl {nullptr};
    };
    QWidget *buildParamForm(ParamEditors &editors, bool allowInheritSteel);
    static void load(ParamEditors &editors, const Structura::App::ProjectData::DesignMemberParams &p);
    static Structura::App::ProjectData::DesignMemberParams read(const ParamEditors &editors);

    Structura::App::ProjectData::DesignSettings m_settings;
    QComboBox *m_defaultSteel {nullptr};
    QComboBox *m_tubeStandard {nullptr};
    QCheckBox *m_amplify {nullptr};
    QCheckBox *m_slenderness {nullptr};
    QCheckBox *m_secondOrder {nullptr};
    QDoubleSpinBox *m_reduction {nullptr};
    QCheckBox *m_autoB2 {nullptr};
    QDoubleSpinBox *m_rs {nullptr};
    QCheckBox *m_singleStorey {nullptr};
    QCheckBox *m_notional {nullptr};
    QDoubleSpinBox *m_notionalFactor {nullptr};
    QCheckBox *m_groupByFamily {nullptr};
    QDoubleSpinBox *m_target {nullptr};
    QListWidget *m_combinations {nullptr};
    ParamEditors m_defaults;
    ParamEditors m_selected;
    QGroupBox *m_selectionGroup {nullptr};
};

} // namespace Structura::UI
