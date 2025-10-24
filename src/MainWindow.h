#pragma once

#include <QMainWindow>
#include <QVector>
#include <QUuid>
#include <QString>
#include <QPoint>
#include <QVector3D>
#include <optional>

#include "SelectionModel.h"
#include "PropertiesPanel.h"
#include "ModelEntities.h"
#include "ui/MainWindowPresenter.h"

class QAction;
class QTabWidget;
class QToolButton;
class QVTKOpenGLNativeWidget;
class SceneController;
class GridDialog;
class QCheckBox;
class QSlider;
class QGridLayout;
class QLabel;
class MaterialDialog;
class SectionDialog;
class BarPropertiesDialog;
class AssignBarPropertiesDialog;
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
}
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onInsertNodeByCoordinates();
    void onResetCamera();
    void onZoomExtents();
    void onGenerateGrid();
    void onStartScreenInsert();
    void onAddGridLineX();
    void onAddGridLineY();
    void onAddGridLineZ();
    void onDeleteGridLine();
    void onApplyNodalLoad();
    void onApplyDistributedLoad();
    void onApplyRestraints();
    void onSnapToggled(bool checked);
    void onShowBarLCSToggled(bool checked);
    void onInsertBar();
    void onCreateMaterial();
    void onCreateSection();
    void onAssignProperties();
    void onOpenModel();
    void onSaveModel();
    void onRibbonTabChanged(int index);
    void onNodeCoordinateEdited(const QVector<QUuid> &ids, char axis, double value);
    void onBarMaterialEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &materialId);
    void onBarSectionEdited(const QVector<QUuid> &ids, const std::optional<QUuid> &sectionId);

private:
    using Presenter = Structura::UI::MainWindowPresenter;
    using MaterialInfo = Presenter::MaterialInfo;
    using SectionInfo = Presenter::SectionInfo;
    using NodeSupport = Presenter::NodeSupport;
    using NodalLoad = Presenter::NodalLoad;
    using MemberLoad = Presenter::MemberLoad;
    using NodalLoadPreset = Presenter::NodalLoadPreset;
    using DistributedLoadPreset = Presenter::DistributedLoadPreset;

    enum class Command {
        None,
        InsertNode,
        InsertBarFirst,
        InsertBarSecond,
        AddGridLineX,
        AddGridLineY,
        AddGridLineZ,
        DeleteGridLine
    };

    struct GridInsertState {
        Structura::Model::GridLine::Axis axis {Structura::Model::GridLine::Axis::X};
        bool active {false};
        bool pointerValid {false};
        double pointerCoord1 {0.0};
        double pointerCoord2 {0.0};
        double pointerAxisCoord {0.0};
        QUuid highlightedLineId;
        QUuid referenceLineId;
        double referenceCoord1 {0.0};
        double referenceCoord2 {0.0};
        bool referenceLocked {false};
        QString inputBuffer;
        double typedValue {0.0};
        bool hasTypedValue {false};
        double ghostCoord1 {0.0};
        double ghostCoord2 {0.0};
        bool ghostVisible {false};
    };

    void createRibbon();
    QWidget *createQuickAccessBar();
    QToolButton *createToolButton(QAction *action);
    bool eventFilter(QObject *obj, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void setCommand(Command command);
    void updateStatus();
    void showStatusMessage(const QString &message, int timeout = 0);
    MaterialInfo *findMaterial(const QUuid &id);
    const MaterialInfo *findMaterial(const QUuid &id) const;
    SectionInfo *findSection(const QUuid &id);
    const SectionInfo *findSection(const QUuid &id) const;
    void populateActionGrid(QGridLayout *layout, const QList<QAction *> &actions, int columns);
    int nextMaterialExternalId() const;
    int nextSectionExternalId() const;
    int nextBarExternalId() const;
    void updateRibbonTabButtons(int currentIndex);
    void setupCentralLayouts();
    void setupRightToolColumn();
    void ensurePropertiesPanel();
    void refreshPropertiesPanel();
    QVector<PropertiesPanel::NodeEntry> buildNodeEntries(const QSet<QUuid> &nodeIds) const;
    QVector<PropertiesPanel::BarEntry> buildBarEntries(const QSet<QUuid> &barIds) const;
    void updateGridInfoOnPanel();
    bool computeWorldPointForInsert(const QPoint &widgetPos, double &x, double &y, double &z, bool applySnap) const;
    void setHoverInsertPoint(const std::optional<QVector3D> &point);
    void beginGridInsert(Structura::Model::GridLine::Axis axis);
    void updateGridInsertFromPoint(const QVector3D &worldPoint);
    void refreshGridInsertVisuals();
    void resetGridInsertState();
    void cancelGridInsert();
    bool isGridInsertCommand(Command command) const;
    QString gridAxisLabel(Structura::Model::GridLine::Axis axis) const;
    void updateGridDeleteTooltip(const QPoint &widgetPos, const QUuid &lineId);
    void hideGridDeleteTooltip();
    void updateGridActionsEnabled();
    Structura::Model::GridLine::Axis commandToAxis(Command command) const;
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

    QAction *m_insertNodeCoordinatesAction;
    QAction *m_insertNodeScreenAction;
    QAction *m_generateGridAction;
    QAction *m_addGridLineXAction;
    QAction *m_addGridLineYAction;
    QAction *m_addGridLineZAction;
    QAction *m_deleteGridLineAction;
    QAction *m_applyNodalLoadAction;
    QAction *m_applyDistributedLoadAction;
    QAction *m_applyRestraintsAction;
    QAction *m_resetCameraAction;
    QAction *m_zoomExtentsAction;
    QAction *m_insertBarAction;
    QAction *m_createMaterialAction;
    QAction *m_createSectionAction;
    QAction *m_assignPropertiesAction;
    QAction *m_openModelAction;
    QAction *m_saveModelAction;
    QAction *m_undoAction;
    QAction *m_redoAction;
    Structura::App::UndoRedoService *m_undoService;

    Command m_command { Command::None };
    QUuid m_firstBarNodeId;
    QCheckBox *m_snapCheck {nullptr};
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
    QLabel *m_gridDeleteTooltip {nullptr};
    QUuid m_pendingDeleteLineId;
    GridInsertState m_gridInsertState;

    Presenter *m_presenter;
    QVector<MaterialInfo> m_materials;
    QVector<SectionInfo> m_sections;
    QUuid m_lastMaterialId;
    QUuid m_lastSectionId;
    QVector<NodeSupport> m_supports;
    QVector<NodalLoad> m_nodalLoads;
    QVector<MemberLoad> m_memberLoads;
    NodalLoadPreset m_lastNodalPreset;
    DistributedLoadPreset m_lastDistributedPreset;

    void updateMaximizeButtonIcon();
    void toggleMaximized();
    bool loadFromDat(const QString &filePath);
    bool saveToDat(const QString &filePath);
    void resetModel();

    QString m_lastDatDirectory;
    bool m_draggingWindow { false };
    QPoint m_dragOffset;
    std::optional<QVector3D> m_hoverInsertPoint;
};
