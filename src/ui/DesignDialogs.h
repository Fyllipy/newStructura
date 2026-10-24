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
        QComboBox *oneFlange {nullptr};
        QCheckBox *forceAway {nullptr};
        QDoubleSpinBox *b2 {nullptr};
        QDoubleSpinBox *netArea {nullptr};
        QSpinBox *defl {nullptr};
        QDoubleSpinBox *stiffener {nullptr};
        QComboBox *panel {nullptr};
        QCheckBox *coldWork {nullptr};
        QDoubleSpinBox *bearing {nullptr};
        QComboBox *fatigueCategory {nullptr};
        QDoubleSpinBox *fatigueCycles {nullptr};
        QDoubleSpinBox *openingD {nullptr};
        QDoubleSpinBox *openingPos {nullptr};
        QDoubleSpinBox *openingOffset {nullptr};
        QDoubleSpinBox *openingNeighbourD {nullptr};
        QDoubleSpinBox *openingSpacing {nullptr};
        QComboBox *ctMode {nullptr};
        QDoubleSpinBox *ctEc {nullptr};
        QDoubleSpinBox *ctLc {nullptr};
        QDoubleSpinBox *ctAc {nullptr};
        QDoubleSpinBox *holeDb {nullptr};
        QSpinBox *holeCount {nullptr};
        QDoubleSpinBox *holeT {nullptr};
        QDoubleSpinBox *staggerS {nullptr};
        QDoubleSpinBox *staggerG {nullptr};
        QSpinBox *staggerLinks {nullptr};
        QDoubleSpinBox *compositeSpacing {nullptr};
        QCheckBox *compositeWelded {nullptr};
        QCheckBox *threadedEnds {nullptr};
        QDoubleSpinBox *webWeldLeg {nullptr};
        QDoubleSpinBox *webWeldFw {nullptr};
        QDoubleSpinBox *flangeLineLoad {nullptr};
        QDoubleSpinBox *coverB {nullptr};
        QDoubleSpinBox *coverT {nullptr};
        QDoubleSpinBox *coverStart {nullptr};
        QDoubleSpinBox *coverEnd {nullptr};
        QComboBox *coverFlanges {nullptr};
        QDoubleSpinBox *coverWeldLeg {nullptr};
        QCheckBox *coverEndWeld {nullptr};
        QCheckBox *plasticAnalysis {nullptr};
        QComboBox *mixType {nullptr};
        QComboBox *mixModel {nullptr};
        QDoubleSpinBox *mixHc {nullptr};
        QDoubleSpinBox *mixBc {nullptr};
        QDoubleSpinBox *mixFck {nullptr};
        QDoubleSpinBox *mixFsk {nullptr};
        QDoubleSpinBox *mixBarArea {nullptr};
        QSpinBox *mixBarsH {nullptr};
        QSpinBox *mixBarsV {nullptr};
        QDoubleSpinBox *mixCover {nullptr};
        QDoubleSpinBox *mixAlphaE {nullptr};
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
    QCheckBox *m_stability {nullptr};
    QCheckBox *m_shear {nullptr};
    QCheckBox *m_warping {nullptr};
    QCheckBox *m_geometric {nullptr};
    class QSpinBox *m_imperfectionDenominator {nullptr};
    class QDoubleSpinBox *m_rigidZone {nullptr};
    class QSpinBox *m_loadSteps {nullptr};
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
