#pragma once

#include <QMainWindow>
#include <QVector>
#include <QUuid>
#include <QString>
#include <QPoint>

class QAction;
class QTabWidget;
class QToolButton;
class QVTKOpenGLNativeWidget;
class SceneController;
class GridDialog;
class QCheckBox;
class QGridLayout;
class QLabel;
class MaterialDialog;
class SectionDialog;
class BarPropertiesDialog;
class AssignBarPropertiesDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onAddPoint();
    void onResetCamera();
    void onZoomExtents();
    void onGenerateGrid();
    void onStartScreenInsert();
    void onSnapToggled(bool checked);
    void onInsertBar();
    void onCreateMaterial();
    void onCreateSection();
    void onAssignProperties();
    void onOpenModel();
    void onSaveModel();
    void onRibbonTabChanged(int index);

private:
    enum class Command {
        None,
        InsertNode,
        InsertBarFirst,
        InsertBarSecond
    };

    struct MaterialInfo {
        QUuid uuid;
        int externalId {0};
        QString name;
        double youngModulus {0.0};
        double shearModulus {0.0};
    };

    struct SectionInfo {
        QUuid uuid;
        int externalId {0};
        QString name;
        double area {0.0};
        double iz {0.0};
        double iy {0.0};
        double j {0.0};
    };

    void createRibbon();
    QWidget *createQuickAccessBar();
    QToolButton *createToolButton(QAction *action);
    bool eventFilter(QObject *obj, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void setCommand(Command command);
    void updateStatus();
    MaterialInfo *findMaterial(const QUuid &id);
    const MaterialInfo *findMaterial(const QUuid &id) const;
    SectionInfo *findSection(const QUuid &id);
    const SectionInfo *findSection(const QUuid &id) const;
    void populateActionGrid(QGridLayout *layout, const QList<QAction *> &actions, int columns);
    int nextMaterialExternalId() const;
    int nextSectionExternalId() const;
    int nextBarExternalId() const;
    void updateRibbonTabButtons(int currentIndex);

    SceneController *m_sceneController;
    QVTKOpenGLNativeWidget *m_vtkWidget;
    QTabWidget *m_ribbon;
    QWidget *m_quickBar;
    QLabel *m_titleLabel;
    QToolButton *m_minimizeButton;
    QToolButton *m_maximizeButton;
    QToolButton *m_closeButton;
    QToolButton *m_homeTabButton;

    QAction *m_addPointAction;
    QAction *m_generateGridAction;
    QAction *m_resetCameraAction;
    QAction *m_zoomExtentsAction;
    QAction *m_insertBarAction;
    QAction *m_createMaterialAction;
    QAction *m_createSectionAction;
    QAction *m_assignPropertiesAction;
    QAction *m_openModelAction;
    QAction *m_saveModelAction;

    Command m_command { Command::None };
    int m_firstBarNode {-1};
    QCheckBox *m_snapCheck {nullptr};

    QVector<MaterialInfo> m_materials;
    QVector<SectionInfo> m_sections;
    QUuid m_lastMaterialId;
    QUuid m_lastSectionId;

    struct NodeSupport {
        int nodeId;
        bool restraints[6];
    };
    struct NodalLoad {
        int nodeId;
        double fx;
        double fy;
        double fz;
        double mx;
        double my;
        double mz;
    };
    struct MemberLoad {
        int memberId;
        QString system;
        double qx;
        double qy;
        double qz;
    };

    QVector<NodeSupport> m_supports;
    QVector<NodalLoad> m_nodalLoads;
    QVector<MemberLoad> m_memberLoads;

    void updateMaximizeButtonIcon();
    void toggleMaximized();
    bool loadFromDat(const QString &filePath);
    bool saveToDat(const QString &filePath);
    void resetModel();

    QString m_lastDatDirectory;
    bool m_draggingWindow { false };
    QPoint m_dragOffset;
};
