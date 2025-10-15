#pragma once

#include <QObject>
#include <QUuid>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vector>

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

class SceneController : public QObject
{
    Q_OBJECT

public:
    struct BarInfo {
        int startNode;
        int endNode;
        QUuid materialId;
        QUuid sectionId;
        int externalId {0};
    };

    explicit SceneController(QObject *parent = nullptr);
    ~SceneController() override;

    void initialize(QVTKOpenGLNativeWidget *vtkWidget);

public slots:
    void addPoint(double x, double y, double z);
    int addPointWithId(double x, double y, double z, int externalId);
    void clearAll();
    void resetCamera();
    void zoomExtents();

public:
    // Grid API
    void createGrid(double dx, double dy, double dz, int nx, int ny, int nz);
    bool hasGrid() const;
    void snapToGrid(double &x, double &y, double &z) const;
    // Picking API (screen to world)
    bool pickWorldPoint(int displayX, int displayY, double &x, double &y, double &z) const;
    int viewportHeight() const;
    bool worldPointOnPlaneZ0(int displayX, int displayY, double &x, double &y, double &z) const;
    bool worldPointOnViewPlane(int displayX, int displayY, double &x, double &y, double &z) const;

    // Nodes
    int nodeCount() const;
    void nodePosition(int index, double &x, double &y, double &z) const;
    int nodeIndexByExternalId(int externalId) const;
    struct NodeInfo {
        int externalId;
        double x;
        double y;
        double z;
    };
    std::vector<NodeInfo> nodeInfos() const;
    int pickNode(int displayX, int displayY) const;
    void setHighlightedNode(int nodeIndex);
    void clearHighlightedNode();

    // Bars
    int addBar(int startNodeIndex, int endNodeIndex, const QUuid &materialId, const QUuid &sectionId);
    void assignBarProperties(const std::vector<int> &barIndices, const QUuid &materialId, const QUuid &sectionId);
    void setBarExternalId(int barIndex, int externalId);
    const std::vector<BarInfo> &bars() const;

private:
    void updateBounds();

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

    // Grid state
    vtkSmartPointer<vtkPolyData> m_gridData;
    vtkSmartPointer<vtkPolyDataMapper> m_gridMapper;
    vtkSmartPointer<vtkActor> m_gridActor;
    double m_dx {0.0}, m_dy {0.0}, m_dz {0.0};
    int m_nx {0}, m_ny {0}, m_nz {0};

    // Picker
    vtkSmartPointer<vtkCellPicker> m_picker;
    vtkSmartPointer<vtkPointPicker> m_nodePicker;

    struct NodeRecord {
        vtkIdType pointId;
        double position[3];
        int externalId;
    };
    std::vector<NodeRecord> m_nodes;
    std::vector<BarInfo> m_bars;
    int m_highlightNode {-1};
    unsigned char m_defaultNodeColor[3] {228, 74, 25};
    unsigned char m_highlightNodeColor[3] {30, 126, 255};

    int m_nextNodeExternalId {1};
};
