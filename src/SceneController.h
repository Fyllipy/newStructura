#pragma once

#include <QObject>
#include <QUuid>
#include <QHash>
#include <QSet>
#include <QVector>
#include <QVector3D>

#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vector>
#include <memory>
#include <optional>
#include <array>

#include "ModelEntities.h"
#include "LoadVisualization.h"

class QVTKOpenGLNativeWidget;
class vtkGenericOpenGLRenderWindow;
class vtkRenderer;
class vtkAxesActor;
class vtkOrientationMarkerWidget;
class vtkRenderWindowInteractor;
class vtkPoints;
class vtkPolyData;
class vtkCellArray;
class vtkPolyDataMapper;
class vtkActor;
class vtkCellPicker;
class vtkUnsignedCharArray;
class vtkPointPicker;
class vtkGlyph3D;
class vtkArrowSource;
class vtkDoubleArray;
class vtkBillboardTextActor3D;

class SceneController : public QObject
{
    Q_OBJECT

public:
    static constexpr double kCoordTolerance = 1e-6;

    using Node = Structura::Model::Node;
    using Bar = Structura::Model::Bar;
    using GridLine = Structura::Model::GridLine;

    struct NodalLoadVisual {
        QVector3D position;
        QVector3D force;
        QVector3D moment;
    };

    struct MemberLoadVisual {
        QVector3D position;
        QVector3D force;
        QVector3D barVector;
        bool localSystem {false};
    };

    struct SupportVisual {
        QVector3D position;
        std::array<bool, 6> restraints; // UX, UY, UZ, RX, RY, RZ
    };

    struct NodeInfo {
        QUuid id;
        int externalId;
        double x;
        double y;
        double z;
    };

    struct BarInfo {
        QUuid id;
        QUuid startNodeId;
        QUuid endNodeId;
        QUuid materialId;
        QUuid sectionId;
        int externalId {0};
    };

    explicit SceneController(QObject *parent = nullptr);
    ~SceneController() override;

    void initialize(QVTKOpenGLNativeWidget *vtkWidget);

public slots:
    QUuid addPoint(double x, double y, double z);
    QUuid addPointWithId(double x, double y, double z, int externalId);
    void clearAll();
    void resetCamera();
    void zoomExtents();

public:
    // Grid API
    void createGrid(double dx, double dy, double dz, int nx, int ny, int nz);
    bool hasGrid() const;
    void snapToGrid(double &x, double &y, double &z) const;
    void gridSpacing(double &dx, double &dy, double &dz) const;
    void gridCounts(int &nx, int &ny, int &nz) const;
    // Picking API (screen to world)
    bool pickWorldPoint(int displayX, int displayY, double &x, double &y, double &z) const;
    int viewportHeight() const;
    bool worldPointOnPlaneZ0(int displayX, int displayY, double &x, double &y, double &z) const;
    bool worldPointOnViewPlane(int displayX, int displayY, double &x, double &y, double &z) const;

    // Grid lines manipulation
    const QVector<GridLine> &gridLines() const { return m_gridLines; }
    GridLine *findGridLine(const QUuid &id);
    const GridLine *findGridLine(const QUuid &id) const;
    QUuid addGridLine(GridLine::Axis axis, double coordinate1, double coordinate2);
    bool removeGridLine(const QUuid &lineId);
    QUuid pickGridLine(int displayX, int displayY) const;
    void setHighlightedGridLine(const QUuid &lineId);
    void clearHighlightedGridLine();
    void showGridGhostLine(GridLine::Axis axis, double coordinate1, double coordinate2);
    void hideGridGhostLine();
    std::optional<QUuid> nearestGridLineId(GridLine::Axis axis, double coordinate1, double coordinate2) const;

    // Loads
    void setNodalLoadVisuals(const QVector<NodalLoadVisual> &visuals);
    void setMemberLoadVisuals(const QVector<MemberLoadVisual> &visuals);

    // Supports (restraints)
    void setSupportVisuals(const QVector<SupportVisual> &visuals);
    
    // Bar Local Coordinate Systems (LCS)
    void setShowBarLCS(bool show);
    bool isShowingBarLCS() const { return m_showBarLCS; }
    void updateBarLCSVisuals();

    // Nodes
    int nodeCount() const;
    std::vector<NodeInfo> nodeInfos() const;
    const Node *findNode(const QUuid &id) const;
    Node *findNode(const QUuid &id);
    QUuid pickNode(int displayX, int displayY) const;
    QUuid pickBar(int displayX, int displayY) const;
    void setHighlightedNode(const QUuid &nodeId);
    void clearHighlightedNode();
    void setSelectedNodes(const QSet<QUuid> &nodeIds);
    bool updateNodePosition(const QUuid &nodeId, double x, double y, double z);
    bool updateNodePositions(const QVector<QUuid> &nodeIds, const QVector<QVector3D> &positions);

    // Bars
    QUuid addBar(const QUuid &startNodeId,
                 const QUuid &endNodeId,
                 const QUuid &materialId,
                 const QUuid &sectionId);
    void assignBarProperties(const std::vector<QUuid> &barIds,
                             const std::optional<QUuid> &materialId,
                             const std::optional<QUuid> &sectionId);
    void setBarExternalId(const QUuid &barId, int externalId);
    std::vector<BarInfo> bars() const;
    const Bar *findBar(const QUuid &id) const;
    Bar *findBar(const QUuid &id);
    void setSelectedBars(const QSet<QUuid> &barIds);

private:
    void updateBounds();
    int nodeIndex(const QUuid &id) const;
    int barIndex(const QUuid &id) const;
    int gridLineIndex(const QUuid &id) const;
    void applyNodeColor(const QUuid &id, const unsigned char color[3]);
    void applyBarColor(int barIndex, const unsigned char color[3]);
    void updateGridColors();
    void initializePointRendering();
    void initializeBarRendering();
    void initializeGridRendering();
    void initializeGridGhostRendering();
    void initializeBarLcsRendering();
    void initializePickers();
    void clearNodes();
    void clearBars();
    void clearGrid();
    void clearLoads();
    void clearSupports();
    void requestRender();
    static void populateAxisCoordinates(QVector<double> &coords, double step, int count);
    static bool nearlyEqual(double lhs, double rhs, double epsilon = kCoordTolerance);

    struct LineEndpoints {
        std::array<double, 3> start;
        std::array<double, 3> end;
    };
    LineEndpoints makeLineEndpoints(GridLine::Axis axis, double coordinate1, double coordinate2) const;
    void rebuildGridFromCoordinates();
    bool insertCoordinate(QVector<double> &coords, double value) const;
    bool removeCoordinate(QVector<double> &coords, double value);
    static double nearestCoordinate(const QVector<double> &coords, double value);
    static double computeMinSpacing(const QVector<double> &coords);
    std::pair<double, double> minMaxAlongAxis(GridLine::Axis axis) const;
    QString gridLineKey(GridLine::Axis axis, double coord1, double coord2) const;
    void updateLoadVisuals();
    void updateSupportVisuals();
    void rebuildBarLCSVisuals();

    vtkNew<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkNew<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkOrientationMarkerWidget> m_orientationMarker;

    vtkSmartPointer<vtkPoints> m_points;
    vtkSmartPointer<vtkPolyData> m_pointCloud;
    vtkSmartPointer<vtkCellArray> m_vertices;
    vtkSmartPointer<vtkPolyDataMapper> m_pointMapper;
    vtkSmartPointer<vtkActor> m_pointActor;
    vtkSmartPointer<vtkUnsignedCharArray> m_pointColors;

    vtkSmartPointer<vtkPolyData> m_barData;
    vtkSmartPointer<vtkCellArray> m_barLines;
    vtkSmartPointer<vtkPolyDataMapper> m_barMapper;
    vtkSmartPointer<vtkActor> m_barActor;
    vtkSmartPointer<vtkUnsignedCharArray> m_barColors;

    // Grid state
    vtkSmartPointer<vtkPolyData> m_gridData;
    vtkSmartPointer<vtkPolyDataMapper> m_gridMapper;
    vtkSmartPointer<vtkActor> m_gridActor;
    vtkSmartPointer<vtkPoints> m_gridPoints;
    vtkSmartPointer<vtkCellArray> m_gridCells;
    vtkSmartPointer<vtkUnsignedCharArray> m_gridColors;
    QVector<GridLine> m_gridLines;
    QHash<QUuid, int> m_gridLineIndexById;
    QHash<vtkIdType, int> m_gridCellToLineIndex;
    QUuid m_highlightGridLineId;

    vtkSmartPointer<vtkPolyData> m_gridGhostData;
    vtkSmartPointer<vtkPoints> m_gridGhostPoints;
    vtkSmartPointer<vtkCellArray> m_gridGhostCells;
    vtkSmartPointer<vtkPolyDataMapper> m_gridGhostMapper;
    vtkSmartPointer<vtkActor> m_gridGhostActor;
    GridLine::Axis m_ghostAxis {GridLine::Axis::X};

    double m_dx {0.0}, m_dy {0.0}, m_dz {0.0};
    int m_nx {0}, m_ny {0}, m_nz {0};
    QVector<double> m_xCoords;
    QVector<double> m_yCoords;
    QVector<double> m_zCoords;

    // Load visualization (new clean implementation)
    std::unique_ptr<Structura::Visualization::LoadVisualization> m_loadVisualization;
    QVector<NodalLoadVisual> m_nodalLoadVisuals;
    QVector<MemberLoadVisual> m_memberLoadVisuals;

    // Support visualization
    QVector<SupportVisual> m_supportVisuals;
    vtkSmartPointer<vtkPolyData> m_supportData;
    vtkSmartPointer<vtkPolyDataMapper> m_supportMapper;
    vtkSmartPointer<vtkActor> m_supportActor;
    
    // Bar LCS visualization
    bool m_showBarLCS {false};
    vtkSmartPointer<vtkPolyData> m_lcsData;
    vtkSmartPointer<vtkPoints> m_lcsPoints;
    vtkSmartPointer<vtkCellArray> m_lcsCells;
    vtkSmartPointer<vtkUnsignedCharArray> m_lcsColors;
    vtkSmartPointer<vtkPolyDataMapper> m_lcsMapper;
    vtkSmartPointer<vtkActor> m_lcsActor;

    // Picker
    vtkSmartPointer<vtkCellPicker> m_picker;
    vtkSmartPointer<vtkPointPicker> m_nodePicker;
    vtkSmartPointer<vtkCellPicker> m_barPicker;

    std::vector<Node> m_nodes;
    std::vector<vtkIdType> m_nodePointIds;
    std::vector<QUuid> m_pointIdToNodeId;
    QHash<QUuid, int> m_nodeIndexById;

    std::vector<Bar> m_bars;
    QHash<QUuid, int> m_barIndexById;

    QUuid m_highlightNodeId;
    QSet<QUuid> m_selectedNodeIds;
    QSet<QUuid> m_selectedBarIds;
    unsigned char m_defaultNodeColor[3] {228, 74, 25};
    unsigned char m_selectedNodeColor[3] {30, 126, 255};
    unsigned char m_hoverNodeColor[3] {255, 198, 30};
    unsigned char m_defaultBarColor[3] {71, 82, 102};
    unsigned char m_selectedBarColor[3] {255, 198, 30};
    unsigned char m_defaultGridColor[3] {140, 153, 173};
    unsigned char m_highlightGridColor[3] {255, 198, 30};

    int m_nextNodeExternalId {1};
};
