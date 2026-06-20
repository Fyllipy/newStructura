#pragma once

#include <QUuid>
#include <QHash>
#include <QSet>
#include <QVector3D>

#include <vtkSmartPointer.h>
#include <vtkNew.h>

class vtkRenderer;
class vtkActor;
class vtkPolyData;
class vtkPoints;
class vtkCellArray;
class vtkUnsignedCharArray;
class vtkDistanceToCamera;
class vtkGlyph3DMapper;
class vtkSphereSource;

namespace Structura::Visualization {

class NodeRenderer
{
public:
    NodeRenderer();
    ~NodeRenderer();

    // Setup and teardown
    void addToRenderer(vtkRenderer* renderer);
    void removeFromRenderer(vtkRenderer* renderer);

    void setPoints(vtkPoints* points);

    // Data management
    void clear();
    void addNode(const QUuid& id, double x, double y, double z);
    void updateNodePosition(const QUuid& id, double x, double y, double z);

    // Visual states
    void setHoveredNode(const QUuid& id);
    void clearHoveredNode();
    void setSelectedNodes(const QSet<QUuid>& ids);

    // Visibility
    void setVisible(bool visible);
    bool isVisible() const;

    // Pickable
    vtkActor* getActor() const { return m_actor.GetPointer(); }

private:
    void rebuildColors();
    int nodeIndex(const QUuid& id) const;

    vtkNew<vtkPolyData> m_pointCloud;
    vtkNew<vtkUnsignedCharArray> m_colors;
    vtkNew<vtkDistanceToCamera> m_distanceFilter;
    vtkNew<vtkSphereSource> m_diskGlyph;
    vtkNew<vtkGlyph3DMapper> m_mapper;
    vtkNew<vtkActor> m_actor;

    std::vector<QUuid> m_nodes;
    QHash<QUuid, int> m_nodeIndexById;

    QUuid m_hoveredNode;
    QSet<QUuid> m_selectedNodes;

    // Design tokens (Cores modernas e vetoriais)
    static constexpr unsigned char COLOR_NORMAL[3] = {100, 116, 139};   // Slate 500
    static constexpr unsigned char COLOR_HOVER[3] = {56, 189, 248};     // Sky 400
    static constexpr unsigned char COLOR_SELECTED[3] = {245, 158, 11};  // Amber 500
};

} // namespace Structura::Visualization
