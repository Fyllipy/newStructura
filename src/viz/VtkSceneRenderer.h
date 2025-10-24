#pragma once

#include "ISceneRenderer.h"
#include <QObject>
#include <QHash>
#include <vtkSmartPointer.h>
#include <vtkNew.h>

// Forward declarations
class QVTKOpenGLNativeWidget;
class vtkGenericOpenGLRenderWindow;
class vtkRenderer;
class vtkOrientationMarkerWidget;
class vtkPoints;
class vtkPolyData;
class vtkCellArray;
class vtkPolyDataMapper;
class vtkActor;
class vtkUnsignedCharArray;
class vtkCellPicker;
class vtkPointPicker;

namespace Structura::Visualization {
    class LoadVisualization;
}

namespace Structura::Viz {

/**
 * @brief VTK-based implementation of ISceneRenderer.
 * 
 * This class encapsulates all VTK rendering logic, maintaining VTK objects
 * and performing all visualization operations.
 * 
 * It does not contain domain logic - only rendering.
 */
class VtkSceneRenderer : public QObject, public ISceneRenderer
{
    Q_OBJECT

public:
    explicit VtkSceneRenderer(QObject *parent = nullptr);
    ~VtkSceneRenderer() override;

    // ISceneRenderer implementation
    void initialize(QVTKOpenGLNativeWidget *widget) override;
    void renderSnapshot(const ModelSnapshot &snapshot) override;
    void updateNodes(const std::vector<ModelSnapshot::NodeData> &nodes) override;
    void updateBars(const std::vector<ModelSnapshot::BarData> &bars) override;
    void updateGridLines(const std::vector<ModelSnapshot::GridLineData> &gridLines) override;
    void highlightNode(const QUuid &nodeId) override;
    void setSelectedNodes(const QSet<QUuid> &nodeIds) override;
    void setSelectedBars(const QSet<QUuid> &barIds) override;
    void highlightGridLine(const QUuid &lineId) override;
    void showGridGhostLine(int axis,
                          const std::array<double, 3> &startPoint,
                          const std::array<double, 3> &endPoint) override;
    void hideGridGhostLine() override;
    void updateLoads(const std::vector<ModelSnapshot::NodalLoadData> &nodalLoads,
                    const std::vector<ModelSnapshot::MemberLoadData> &memberLoads) override;
    void updateSupports(const std::vector<ModelSnapshot::SupportData> &supports) override;
    void updateBarLCS(const std::vector<ModelSnapshot::BarLCSData> &barLCS, bool visible) override;
    void clearAll() override;
    void resetCamera() override;
    void zoomExtents() override;
    void refresh() override;
    
    QUuid pickNode(int displayX, int displayY) const override;
    QUuid pickBar(int displayX, int displayY) const override;
    QUuid pickGridLine(int displayX, int displayY) const override;
    bool pickWorldPoint(int displayX, int displayY,
                       double &x, double &y, double &z) const override;
    int viewportHeight() const override;

private:
    // Helper methods
    void initializeVtkPipeline();
    void setupNodeRendering();
    void setupBarRendering();
    void setupGridRendering();
    void setupLoadRendering();
    void setupSupportRendering();
    void setupBarLCSRendering();
    
    void rebuildNodeGeometry(const std::vector<ModelSnapshot::NodeData> &nodes);
    void rebuildBarGeometry(const std::vector<ModelSnapshot::BarData> &bars,
                           const std::vector<ModelSnapshot::NodeData> &nodes);
    void rebuildGridGeometry(const std::vector<ModelSnapshot::GridLineData> &gridLines);
    void rebuildSupportGeometry(const std::vector<ModelSnapshot::SupportData> &supports);
    void rebuildBarLCSGeometry(const std::vector<ModelSnapshot::BarLCSData> &barLCS);
    
    void applyNodeColor(int nodeIndex, const unsigned char color[3]);
    void applyBarColor(int barIndex, const unsigned char color[3]);
    void applyGridLineColor(int lineIndex, const unsigned char color[3]);
    
    int findNodeIndex(const QUuid &id) const;
    int findBarIndex(const QUuid &id) const;
    int findGridLineIndex(const QUuid &id) const;
    
    // VTK objects
    vtkNew<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkNew<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkOrientationMarkerWidget> m_orientationMarker;
    
    // Node rendering
    vtkSmartPointer<vtkPoints> m_points;
    vtkSmartPointer<vtkPolyData> m_pointCloud;
    vtkSmartPointer<vtkCellArray> m_vertices;
    vtkSmartPointer<vtkPolyDataMapper> m_pointMapper;
    vtkSmartPointer<vtkActor> m_pointActor;
    vtkSmartPointer<vtkUnsignedCharArray> m_pointColors;
    
    // Bar rendering
    vtkSmartPointer<vtkPolyData> m_barData;
    vtkSmartPointer<vtkCellArray> m_barLines;
    vtkSmartPointer<vtkPolyDataMapper> m_barMapper;
    vtkSmartPointer<vtkActor> m_barActor;
    vtkSmartPointer<vtkUnsignedCharArray> m_barColors;
    
    // Grid rendering
    vtkSmartPointer<vtkPolyData> m_gridData;
    vtkSmartPointer<vtkPolyDataMapper> m_gridMapper;
    vtkSmartPointer<vtkActor> m_gridActor;
    vtkSmartPointer<vtkPoints> m_gridPoints;
    vtkSmartPointer<vtkCellArray> m_gridCells;
    vtkSmartPointer<vtkUnsignedCharArray> m_gridColors;
    
    // Grid ghost line
    vtkSmartPointer<vtkPolyData> m_gridGhostData;
    vtkSmartPointer<vtkPoints> m_gridGhostPoints;
    vtkSmartPointer<vtkCellArray> m_gridGhostCells;
    vtkSmartPointer<vtkPolyDataMapper> m_gridGhostMapper;
    vtkSmartPointer<vtkActor> m_gridGhostActor;
    
    // Support rendering
    vtkSmartPointer<vtkPolyData> m_supportData;
    vtkSmartPointer<vtkPolyDataMapper> m_supportMapper;
    vtkSmartPointer<vtkActor> m_supportActor;
    
    // Bar LCS rendering
    vtkSmartPointer<vtkPolyData> m_lcsData;
    vtkSmartPointer<vtkPoints> m_lcsPoints;
    vtkSmartPointer<vtkCellArray> m_lcsCells;
    vtkSmartPointer<vtkUnsignedCharArray> m_lcsColors;
    vtkSmartPointer<vtkPolyDataMapper> m_lcsMapper;
    vtkSmartPointer<vtkActor> m_lcsActor;
    
    // Pickers
    vtkSmartPointer<vtkCellPicker> m_picker;
    vtkSmartPointer<vtkPointPicker> m_nodePicker;
    vtkSmartPointer<vtkCellPicker> m_barPicker;
    
    // Load visualization
    std::unique_ptr<Structura::Visualization::LoadVisualization> m_loadVisualization;
    
    // State tracking (for efficient updates)
    std::vector<ModelSnapshot::NodeData> m_currentNodes;
    std::vector<ModelSnapshot::BarData> m_currentBars;
    std::vector<ModelSnapshot::GridLineData> m_currentGridLines;
    
    QHash<QUuid, int> m_nodeIndexById;
    QHash<QUuid, int> m_barIndexById;
    QHash<QUuid, int> m_gridLineIndexById;
    QHash<vtkIdType, int> m_gridCellToLineIndex;
    
    QUuid m_highlightedNodeId;
    QUuid m_highlightedGridLineId;
    QSet<QUuid> m_selectedNodeIds;
    QSet<QUuid> m_selectedBarIds;
    
    // Colors
    unsigned char m_defaultNodeColor[3] {228, 74, 25};
    unsigned char m_selectedNodeColor[3] {30, 126, 255};
    unsigned char m_hoverNodeColor[3] {255, 198, 30};
    unsigned char m_defaultBarColor[3] {71, 82, 102};
    unsigned char m_selectedBarColor[3] {255, 198, 30};
    unsigned char m_defaultGridColor[3] {140, 153, 173};
    unsigned char m_highlightGridColor[3] {255, 198, 30};
    
    bool m_initialized {false};
};

} // namespace Structura::Viz
