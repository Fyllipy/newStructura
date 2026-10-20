#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QVector>
#include <QUuid>
#include <QString>
#include <QPoint>
#include <QVector3D>
#include <array>
#include <QColor>
#include <QPair>
#include <QPair>
#include "app/SceneDataTypes.h"
#include <QColor>
#include <QPair>
#include "SelectionFilterDialog.h"
#include "viz/structure/ElementAppearance.h"
#include <functional>
#include <QMap>
#include <vector>
#include "app/SolverBridge.h"
#include "app/GeometrySnapshot.h"
#include "app/services/ReportService.h"
#include "app/services/ModelCheckService.h"
#include "shell/ShellMesh.h"
#include "shell/ShellResults.h"
#include <optional>
#include <memory>

#include "SelectionModel.h"
#include "PropertiesPanel.h"
#include "core/model/Node.h"
#include "core/model/Bar.h"
#include "core/model/Material.h"
#include "core/model/Section.h"
#include "ui/AssignBarPropertiesDialog.h"
#include "ui/MainWindowPresenter.h"
#include "viz/postprocessor/PostProcessorDataTypes.h"

class QAction;
class QTimer;
class QTabWidget;
class QToolButton;
class QVTKOpenGLNativeWidget;
class SceneController;
namespace Structura::UI { class ProfileManagerDialog; class LoadsPanel; class DesignPanel; class GroupsPanel; class ViewCubeWidget; class ShellDialog; }
struct DesignRunData;
class QDockWidget;
class QCheckBox;
class QSlider;
class QGridLayout;
class QLabel;
class QComboBox;
class MaterialDialog;
class BarPropertiesDialog;
class NodalLoadDialog;
class DistributedLoadDialog;
class RestraintDialog;
class QHBoxLayout;


namespace Structura {
namespace App {
class UndoRedoService;
}
namespace UI {
class MainWindowPresenter;
class PlacementController;
class ViewOptionsBar;
class AxesPanel;
class SectionPlacementPanel;
class ViewportLabelOverlay;
}
namespace Visualization {
class PostProcessorRenderer;
}
}


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// Abre um modelo .dat (usado tambem ao iniciar com um arquivo como argumento).
    bool openModelFile(const QString &filePath) { return loadFromDat(filePath); }
    /// Verificacao automatizada (variavel STRUCTURA_AUTOSHOT): analisa o modelo aberto, exibe os diagramas e grava capturas da tela em `directory`.
    void automatedCapture(const QString &directory);
    void offerRecovery();

private slots:
    void onResetCamera();
    void onZoomExtents();
    void onApplyNodalLoad();
    void onApplyDistributedLoad();
    void onApplyMemberMoment();
    void onEditLoadCases();
    void onGenerateCombinations();
    void onBucklingAnalysis();
    void onModalAnalysis();
    void onCheckConnections();
    void onOpenConnectionCalculator();
    void onShowQuantities();
    void rebuildResultViews();
    bool restoreStoredResults();
    void collectResultsForSave();
    void onExportDxf();
    void onGenerateDrawings();
    void onApplyRestraints();
    void onCopyElements();
    void onMoveElements();
    void onSerialCopy();
    void onRotateElements();
    void onMirrorElements();
    void onToggleAxesPanel();
    void onShowBarLCSToggled(bool checked);
    void onInsertBar();
    void onCreateMaterial();
    void onCreateSection();
    void onAssignProperties();
    void onOpenModel();
    void onSaveModel();
    void onSaveModelAs();
    void onDynamics();
    void onCompositeFloor();
    void onGenerateTruss();
    void onShellEditor();
    bool newFromTemplateDialog();
    bool addSteelMaterial(const QString &name);
    void ensureDefaultMaterial();
    void syncConnectionVisuals();
    QString friendlySolverMessage(const QString &raw) const;
    void reportActionableError(const QString &title, const QString &raw);
    bool isModelDirty() const;
    void markModified();
    void updateWindowTitle();
    quint64 quickFingerprint() const;
    void markResultsCurrent();
    void updateStaleIndicator();
    void setupSessionServices();
    bool maybeSave();
    void removeRecoveryFiles();
    void autosaveNow();
    void updateDiagramProbe(const QPoint &widgetPos);
    void scheduleDiagramProbe(const QPoint &widgetPos);
    void updateDiagramInfo();
    void updateModelHover(const QPoint &widgetPos);
    void onDiagramLabelModeChanged(int index);
    void onDiagramHatchToggled(bool on);
    bool computeModal(const QString &source, int modes, Structura::App::SolverBridge::ModelData &model,
                      Structura::App::SolverBridge::Result &result, bool *includeMaterialMass, QString *error);
    void onDeleteSelected();
    void onInsertNode();
    void onNewModel();
    void onAbout();
    void onRunStructuralAnalysis();
    bool executeAnalysis(QString *report = nullptr);
    void onDesignSettings();
    void onVerifyDesign();
    void onOptimizeDesign();
    void onChangeProfileForMember(int memberId);
    void focusMember(int memberId);
    void showDesignResults(bool optimized);
    bool buildDesignRun(DesignRunData &run);
    QString designCacheKey() const;
    QVector<QUuid> expandWithGroups(const QVector<QUuid> &ids) const;
    void onCreateGroup();
    void onToggleGroupsPanel();
    void onCheckModel();
    void onNewFromTemplate();
    QVector<Structura::App::Services::ModelCheckService::Issue> checkModel();
    void focusCheckTarget(int kind, int id);
    void applyBarTypeOperation(int op);
    void syncAreaPanelVisuals();
    void onDeleteLoads();
    void onGenerateReport();
    void onBarReport();
    bool buildReportInput(Structura::App::Services::ReportService::Input &input, bool withDesign);
    void onApplyAreaLoad();
    void onApplyWindLoad();
    void applyAreaLoad(bool wind);
    void refreshGroupsPanel();
    void setGroupHidden(int groupId, bool hidden);
    void adoptGroupProperties(int referenceMember, const QVector<int> &newMembers);
    void ensureDesignPanel();
    QUuid sectionForCandidate(const Structura::App::ProjectData::SectionInfo &candidate);
    void onRibbonTabChanged(int index);
    void onNodeCoordinateEdited(const QVector<QUuid> &ids, char axis, double value);
    void onBarMaterialEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &materialId);
    void onBarSectionEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &sectionId);
    void onBarPlacementEdited(const QVector<QUuid> &ids,
                              const Structura::App::ProjectData::MemberPlacementPatch &patch);
    void onTogglePlacementPanel();
    /// Atualiza (se visivel) a janela de posicionamento com as barras selecionadas.
    void refreshPlacementPanel();
    // Pós-processador
    void onShowDiagrams();
    void onHideDiagrams();
    void onDiagramTypeChanged(int index);
    void onDiagramScaleChanged(int value);
    void onScaleModeChanged(int index);


private:
    /// Posicao 3D de cada ligacao (origem na face do apoio, eixos s/t/u) e quanto a barra deve ser recuada (mm) do no ate essa face.
    struct ConnectionPlacementInfo {
        int index {0};   ///< indice em m_extras.connections
        std::array<double, 3> origin {}, s {}, t {}, u {};
        double trimMm {0.0};
    };
    QVector<ConnectionPlacementInfo> connectionPlacements() const;
    using Presenter = Structura::UI::MainWindowPresenter;
    using MaterialInfo = Presenter::MaterialInfo;
    using SectionInfo = Presenter::SectionInfo;
    using NodeSupport = Presenter::NodeSupport;
    using NodalLoad = Presenter::NodalLoad;
    using MemberLoad = Presenter::MemberLoad;
    using NodalLoadPreset = Presenter::NodalLoadPreset;
    using DistributedLoadPreset = Presenter::DistributedLoadPreset;

    /// Estado completo do modelo (alem da cena) para o desfazer de cargas, grupos, materiais e opcoes.
    struct ModelState {
        QVector<MaterialInfo> materials;
        QVector<SectionInfo> sections;
        QVector<NodeSupport> supports;
        QVector<NodalLoad> nodalLoads;
        QVector<MemberLoad> memberLoads;
        Structura::App::ProjectData::ProjectExtras extras;
    };
    ModelState captureState() const;
    void applyState(const ModelState &state);
    quint64 stateFingerprint() const;

    /// Empilha um comando de desfazer ao fim do escopo se a acao alterou o estado.
    class StateGuard
    {
    public:
        StateGuard(MainWindow *window, const QString &text);
        ~StateGuard();
        StateGuard(const StateGuard &) = delete;
        StateGuard &operator=(const StateGuard &) = delete;

    private:
        MainWindow *m_window;
        QString m_text;
        ModelState m_before;
        quint64 m_fingerprint {0};
    };

    enum class Command {
        None,
        InsertBarFirst,
        InsertBarSecond,
        CopyElements,
        MoveElements,
        RotateElements,
        MirrorElements,
        InsertNode,
        Measure,
        ShellPolygon,     ///< chapa por contorno (polilinha fechada)
        ShellRectangle,   ///< chapa retangular (dois cantos)
        ShellDisk,        ///< disco / anel (centro e raio)
        ShellExtrude,     ///< extrusao de uma linha (parede, casca reglada)
        ShellCylinder,    ///< cilindro / cone (circulo e altura)
        ShellRevolve,     ///< revolucao de um perfil em torno de um eixo
        ShellMove,
        ShellCopy
    };

    /// Etapas dos comandos de casca que exigem mais de um desenho (linha base e depois vetor de extrusao ou eixo).
    struct ShellDraft {
        int stage {0};
        std::vector<std::array<double, 3>> points;   ///< m
        bool closed {false};
        double radius {0.0};
    };
    ShellDraft m_shellDraft;
    Structura::Shell::Mesh m_shellMesh;
    int m_selectedShellObject {-1};
    bool m_shellResultsShown {false};
    double m_lastShellThickness {200.0};
    int m_lastShellMaterial {0};
    QAction *m_shellPolygonAction {nullptr};
    QAction *m_shellRectangleAction {nullptr};
    QAction *m_shellDiskAction {nullptr};
    QAction *m_shellExtrudeAction {nullptr};
    QAction *m_shellCylinderAction {nullptr};
    QAction *m_shellRevolveAction {nullptr};
    QAction *m_shellDeleteAction {nullptr};
    double shellUnitsPerMeter() const;
    void refreshShellVisuals(bool force = false);
    int pickShellObjectAtWidgetPos(const QPoint &widgetPos) const;
    void selectShellObject(int id);
    QString shellKindLabel(int kind) const;
    bool commitShell(Structura::Shell::Object object, const QString &undoText);
    void restartShellStage();
    bool onShellShape(int mode, const std::vector<std::array<double, 3>> &points, bool closed);
    bool applyShellDisplacement(int mode, const std::array<double, 3> &from, const std::array<double, 3> &to);
    void deleteSelectedShell();
    void flipSelectedShell();
    void showShellContextMenu(const QPoint &globalPos);

    void createRibbon();
    QWidget *createQuickAccessBar();
    QToolButton *createToolButton(QAction *action);
    bool eventFilter(QObject *obj, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool handleWindowDragEvent(QEvent *event);
    bool handleViewportCommandEvent(QEvent *event);
    bool handleDefaultViewportEvent(QEvent *event);
    bool handleWindowDragMousePress(Qt::MouseButton button, const QPoint &globalPos);
    bool handleWindowDragMouseMove(const QPoint &globalPos);
    bool handleWindowDragMouseRelease(Qt::MouseButton button);
    bool handleWindowDragMouseDoubleClick(Qt::MouseButton button);
    void setCommand(Command command);
    void updateStatus();
    void showStatusMessage(const QString &message, int timeout = 0);
    void populateActionGrid(QGridLayout *layout, const QList<QAction *> &actions, int columns);
    int nextMaterialExternalId() const;
    int nextSectionExternalId() const;
    int nextBarExternalId() const;
    void updateRibbonTabButtons(int currentIndex);
    void setupCentralLayouts();
    void setupRightToolColumn();
    void ensurePropertiesPanel();
    void refreshPropertiesPanel();
    /// Reaplica giro/excentricidade de secao aos desenhos (solidos, eixos locais, cargas).
    void syncPlacementVisuals();
    void syncBarTypeMarkers();
    QVector<QPair<QUuid, QString>> buildMaterialOptions() const;
    std::vector<AssignBarPropertiesDialog::BarInfo> buildAssignDialogBarInfos() const;
    std::vector<AssignBarPropertiesDialog::NodeInfo> buildAssignDialogNodeInfos() const;
    std::vector<QUuid> collectAssignDialogSelectionIds(const QList<int> &selection,
                                                       const std::vector<AssignBarPropertiesDialog::BarInfo> &bars) const;
    QPoint toDisplayPoint(const QPoint &widgetPos) const;
    QUuid pickNodeAtWidgetPos(const QPoint &widgetPos) const;
    QUuid pickBarAtWidgetPos(const QPoint &widgetPos) const;
    void updateHighlightedNodeAtWidgetPos(const QPoint &widgetPos);
    QVector<PropertiesPanel::NodeEntry> buildNodeEntries(const QSet<QUuid> &nodeIds) const;
    QVector<PropertiesPanel::BarEntry> buildBarEntries(const QSet<QUuid> &barIds) const;
    void setHoverInsertPoint(const std::optional<QVector3D> &point);
    QUuid findNodeAtPosition(const std::array<double, 3> &position) const;
    void setupPlacement();
    void setupViewOptions();
    void applyViewOption(const QString &key, bool on);
    bool diagramsShowing() const;
    void applyLoadVisibility();
    void applyViewOptionsFromExtras();
    void storeViewOptionsInExtras();
    void refreshElementLabels();
    void syncSolidVisuals();
    void refreshLevelSelector();
    QVector<QUuid> editNodeIds() const;
    void updateEditActionsEnabled();
    void beginDisplacementCommand(Command command);
    bool applyDisplacement(bool copy, const std::array<double, 3> &from, const std::array<double, 3> &to);
    bool applyTransform(bool copy, const std::function<std::array<double, 3>(const std::array<double, 3> &)> &map);
    bool applyRotation(const std::array<double, 3> &pivot);
    bool applyInsertNode(const std::array<double, 3> &point);
    void onSplitBars();
    /// Divide as barras nos nos das cascas que caem sobre o seu eixo (pre-requisito da continuidade barra-casca). Devolve o numero de cortes.
    int splitBarsAtShellNodes(bool apply = true);
    void onShellCoupleToggled(bool on);
    bool coupledShellResults(const QStringList &cases, bool runAnalysis, Structura::Shell::Results &out, std::string &error);
    QAction *m_shellCoupleAction {nullptr};
    QAction *m_shellSplitAction {nullptr};
    QAction *m_shellMonolithicAction {nullptr};
    void onCompareResults();
    void setupViewCube();
    void positionViewCube();
    void saveCurrentView();
    void applySavedView(int index);
    void deleteSavedView(int index);
    void onMeasure();
    bool applyMeasure(const std::array<double, 3> &from, const std::array<double, 3> &to);
    struct SplitPlan {
        std::size_t barIndex {0};
        std::vector<std::pair<double, QUuid>> cuts;   ///< (fracao do comprimento, no)
    };
    void splitCore(Structura::App::GeometrySnapshot &after, ModelState &state, const std::vector<SplitPlan> &plan);
    void applyTopology(const Structura::App::GeometrySnapshot &geometry, const ModelState &state);
    bool applyMirror(const std::array<double, 3> &a, const std::array<double, 3> &b);
    void showResultView(int index);
    QStringList loadCaseNames() const;
    QString dialogLoadCase() const;
    void rebuildResultViewSelector();
    void rebuildCaseSelector();
    void updateAxisLabels();
    bool handleDefaultViewportMouseMove(const QPoint &widgetPos);
    bool handleDefaultViewportMousePress(Qt::MouseButton button, Qt::KeyboardModifiers modifiers, const QPoint &widgetPos);
    bool handleDefaultViewportLeave();
    bool handleDefaultViewportMouseRelease(Qt::MouseButton button, const QPoint &widgetPos, const QPoint &globalPos);
    void clickSelect(const QPoint &widgetPos, Qt::KeyboardModifiers modifiers);
    void selectInRect(const QRect &rect, bool crossing, Qt::KeyboardModifiers modifiers);
    void showViewportContextMenu(const QPoint &widgetPos, const QPoint &globalPos);
    bool nodePassesFilter(const Structura::App::SceneNodeInfo &node) const;
    bool barPassesFilter(const Structura::App::SceneBarInfo &bar, const QHash<QUuid, Structura::App::SceneNodeInfo> &nodes) const;
    bool isNodeHidden(const QUuid &id) const;
    bool isBarHidden(const QUuid &id) const;
    void applyAppearance();
    void updateLegend(const QString &title, const QVector<QPair<QString, QColor>> &entries);
    void onSelectionFilter();
    void onOpenProfileManager();
    void ensureProfileManager();
    void assignSectionToSelection(const QUuid &sectionId);
    void syncProfileManager();
    void onShowLoadsPanel();
    void refreshLoadsPanel();
    void onLoadsPanelEdit(int kind, int index);
    void onLoadsPanelDelete(const QVector<QPair<int, int>> &items);
    QUuid resolveSectionId(const QUuid &id);
    void onSelectMatching();
    void onInvertSelection();
    void onHideSelected();
    void onIsolateSelected();
    void onShowAllElements();
    void updateLoadActionsEnabled();
    void syncLoadVisuals();
    void syncSupportVisuals();
    void setupFooterBar();
    static bool isZeroNodalLoad(double fx, double fy, double fz, double mx, double my, double mz);
    static bool isZeroDistributedLoad(double qx, double qy, double qz);

    SceneController *m_sceneController;
    Structura::SelectionModel *m_selectionModel;
    QVTKOpenGLNativeWidget *m_vtkWidget;
    QTabWidget *m_ribbon;
    QWidget *m_quickBar;
    QLabel *m_titleLabel;
    QToolButton *m_minimizeButton;
    QToolButton *m_maximizeButton;
    QToolButton *m_closeButton;
    QToolButton *m_homeTabButton;

    QAction *m_applyNodalLoadAction;
    QAction *m_applyDistributedLoadAction;
    QAction *m_applyMemberMomentAction {nullptr};
    QAction *m_copyElementsAction {nullptr};
    QAction *m_moveElementsAction {nullptr};
    QAction *m_serialCopyAction {nullptr};
    QAction *m_rotateElementsAction {nullptr};
    QAction *m_mirrorElementsAction {nullptr};
    QAction *m_loadCasesAction {nullptr};
    QAction *m_genCombinationsAction {nullptr};
    QAction *m_bucklingAction {nullptr};
    QAction *m_dynamicsAction {nullptr};
    QAction *m_modalAction {nullptr};
    QAction *m_connectionsAction {nullptr};
    QAction *m_compositeAction {nullptr};
    QAction *m_shellAction {nullptr};
    QPointer<Structura::UI::ShellDialog> m_shellDialog;
    QAction *m_quantitiesAction {nullptr};
    QAction *m_exportDxfAction {nullptr};
    QAction *m_sheetsAction {nullptr};
    QAction *m_applyRestraintsAction;
    QAction *m_resetCameraAction;
    QAction *m_zoomExtentsAction;
    QAction *m_insertBarAction;
    QAction *m_insertNodeAction {nullptr};
    QAction *m_splitBarsAction {nullptr};
    QAction *m_generateTrussAction {nullptr};
    QAction *m_compareAction {nullptr};
    QAction *m_measureAction {nullptr};
    class QLabel *m_measureCard {nullptr};
    Structura::UI::ViewCubeWidget *m_viewCube {nullptr};
    QAction *m_deleteElementsAction {nullptr};
    QAction *m_createMaterialAction;
    QAction *m_createSectionAction;
    QAction *m_assignPropertiesAction;
    QAction *m_openModelAction;
    QAction *m_saveModelAction;
    QAction *m_saveAsModelAction;
    QAction *m_newModelAction;
    QAction *m_aboutAction;
    QAction *m_runAnalysisAction;
    QAction *m_undoAction;
    QAction *m_redoAction;
    Structura::App::UndoRedoService *m_undoService;

    Command m_command { Command::None };
    QUuid m_firstBarNodeId;
    QWidget *m_footerBar {nullptr};
    QLabel *m_statusLabel {nullptr};
    QToolButton *m_footerResetCameraButton {nullptr};
    QToolButton *m_footerZoomExtentsButton {nullptr};
    QSlider *m_glyphScaleSlider {nullptr};
    QWidget *m_toolColumn {nullptr};
    QToolButton *m_propertiesToolButton {nullptr};
    QToolButton *m_showBarLCSToolButton {nullptr};
    QWidget *m_propertiesContainer {nullptr};
    PropertiesPanel *m_propertiesPanel {nullptr};
    QHBoxLayout *m_contentLayout {nullptr};

    Structura::UI::PlacementController *m_placement {nullptr};
    Structura::UI::ViewportLabelOverlay *m_labelOverlay {nullptr};
    Structura::UI::ViewOptionsBar *m_viewOptionsBar {nullptr};
    Structura::UI::AxesPanel *m_axesPanel {nullptr};
    Structura::UI::SectionPlacementPanel *m_placementPanel {nullptr};
    QAction *m_placementAction {nullptr};
    QAction *m_designSettingsAction {nullptr};
    QAction *m_verifyDesignAction {nullptr};
    QAction *m_createGroupAction {nullptr};
    QAction *m_checkModelAction {nullptr};
    QAction *m_templateAction {nullptr};
    QAction *m_reportAction {nullptr};
    QString m_projectName;
    QAction *m_areaLoadAction {nullptr};
    QAction *m_deleteLoadsAction {nullptr};
    QAction *m_windLoadAction {nullptr};
    QAction *m_groupsPanelAction {nullptr};
    class QDockWidget *m_groupsDock {nullptr};
    Structura::UI::GroupsPanel *m_groupsPanel {nullptr};
    QAction *m_optimizeDesignAction {nullptr};
    Structura::UI::DesignPanel *m_designPanel {nullptr};
    QDockWidget *m_designDock {nullptr};
    QHash<int, double> m_designEta;   ///< barra (id externo) -> aproveitamento, para colorir
    QAction *m_axesAction {nullptr};
    QTimer *m_cameraSyncTimer {nullptr};

    struct CaseRun {
        Structura::App::SolverBridge::ModelData model;
        Structura::App::SolverBridge::Result result;
    };
    struct ResultView {
        QString name;
        bool isCombination {false};
        Structura::App::SolverBridge::Result result;
        std::vector<std::pair<QString, double>> terms;
    };
    QMap<QString, CaseRun> m_caseRuns;
    QString m_analysisFingerprint;   ///< modelo analisado em m_caseRuns (gravado junto dos resultados)
    QVector<ResultView> m_resultViews;

    Presenter *m_presenter;
    QVector<MaterialInfo> m_materials;
    Structura::UI::SelectionFilter m_selectionFilter;
    Structura::UI::ProfileManagerDialog *m_profileManager {nullptr};
    Structura::UI::LoadsPanel *m_loadsPanel {nullptr};
    QAction *m_profileManagerAction {nullptr};
    struct PressState {
        bool left {false};
        bool dragging {false};
        bool right {false};
        bool rightMoved {false};
        QPoint pos;
        QPoint rightPos;
        Qt::KeyboardModifiers mods;
    } m_press;
    QWidget *m_rubberBand {nullptr};   ///< retangulo de selecao (janela azul / cruzamento verde tracejado)
    class QFrame *m_legend {nullptr};
    QSet<QUuid> m_hiddenNodes;
    QSet<QUuid> m_hiddenBars;
    void updatePlaneLabel();
    void positionPlaneLabel();
    QLabel *m_planeLabel {nullptr};
    static QString positionKey(double x, double y, double z);
    QSet<QUuid> m_effHiddenNodes;
    QSet<QString> m_offNodeKeys; ///< Posicoes de nos esmaecidos/ocultos (apoios e cargas nao sao desenhados)
    QSet<QString> m_offBarKeys;  ///< Extremos de barras esmaecidas/ocultas
    QSet<QUuid> m_effHiddenBars;
    QSet<int> m_hiddenNodeExt;
    QSet<int> m_hiddenBarExt;
    Structura::Visualization::AppearanceMap m_barAppearance;
    QVector<SectionInfo> m_sections;
    QUuid m_lastMaterialId;
    QUuid m_lastSectionId;
    QVector<NodeSupport> m_supports;
    QVector<NodalLoad> m_nodalLoads;
    QVector<MemberLoad> m_memberLoads;
    Structura::App::ProjectData::ProjectExtras m_extras;
    NodalLoadPreset m_lastNodalPreset;
    DistributedLoadPreset m_lastDistributedPreset;

    void updateMaximizeButtonIcon();
    void toggleMaximized();
    QString datDialogInitialDirectory() const;
    void showDatStatusMessage(const QString &prefix, const QString &filePath);
    bool loadFromDat(const QString &filePath);
    bool saveToDat(const QString &filePath, bool remember = true);
    void resetModel();

    QString m_lastDatDirectory;
    bool m_extraDirty {false};              ///< alteracoes fora da pilha de desfazer (carimbo, vistas...)
    std::shared_ptr<DesignRunData> m_designCache;   ///< esforcos de projeto da ultima analise (reutilizados)
    QString m_designCacheKey;
    quint64 m_resultsFingerprint {0};       ///< modelo no momento da ultima analise
    class QLabel *m_staleLabel {nullptr};
    class QTimer *m_autosaveTimer {nullptr};
    QString m_currentFilePath;   ///< .dat aberto/salvo (vazio = modelo ainda sem arquivo)
    bool m_draggingWindow { false };
    QPoint m_dragOffset;
    std::optional<QVector3D> m_hoverInsertPoint;

    // ── Pós-processador ─────────────────────────────────────────────────────
    std::unique_ptr<Structura::Visualization::PostProcessorRenderer> m_postProcessor;
    int m_analysisRevision{0};

    // Ações da ribbon (aba Resultados)
    QAction *m_showDiagramsAction{nullptr};
    QAction *m_hideDiagramsAction{nullptr};

    // Controles da footer bar
    QComboBox *m_diagramCombo{nullptr};       ///< N / Vy / Vz / Mt / My / Mz
    QSlider   *m_diagramScaleSlider{nullptr}; ///< 1..100 → 0.1..5.0
    QComboBox *m_diagramLabelCombo{nullptr};
    class QTimer *m_diagramScaleTimer{nullptr};
    class QTimer *m_probeTimer{nullptr};
    QPoint m_probePos;
    double m_pendingDiagramScale{1.0};
    class QCheckBox *m_diagramHatchCheck{nullptr};
    class QLabel *m_probeCard{nullptr};
    class QLabel *m_probeDot{nullptr};
    class QLabel *m_diagramInfo{nullptr};
    QComboBox *m_scaleModeCombo{nullptr};     ///< Global / Caso Atual / Selecionados
};
