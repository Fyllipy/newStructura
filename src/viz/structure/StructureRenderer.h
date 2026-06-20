#pragma once

#include <QUuid>
#include <QSet>
#include <QHash>
#include <memory>
#include <vector>

#include <vtkSmartPointer.h>
#include <vtkNew.h>

class vtkRenderer;
class vtkPoints;
class vtkActor;

namespace Structura::Visualization {

class NodeRenderer;
class MemberRenderer;

class StructureRenderer
{
public:
    StructureRenderer();
    ~StructureRenderer();

    void addToRenderer(vtkRenderer* renderer);
    void removeFromRenderer(vtkRenderer* renderer);

    void clear();

    // Geometria
    void beginUpdate();
    void endUpdate();
    
    void addNode(const QUuid& id, double x, double y, double z);
    void updateNodePosition(const QUuid& id, double x, double y, double z);

    void addBar(const QUuid& id, const QUuid& startNodeId, const QUuid& endNodeId);

    // Estados Visuais (Nós)
    void setHoveredNode(const QUuid& id);
    void clearHoveredNode();
    void setSelectedNodes(const QSet<QUuid>& ids);
    
    // Estados Visuais (Barras)
    void setHoveredBar(const QUuid& id);
    void clearHoveredBar();
    void setSelectedBars(const QSet<QUuid>& ids);

    // Getters para Pickers
    vtkActor* getNodeActor() const;
    vtkActor* getBarActor() const;

private:
    std::unique_ptr<NodeRenderer> m_nodeRenderer;
    std::unique_ptr<MemberRenderer> m_memberRenderer;

    vtkNew<vtkPoints> m_points;
    QHash<QUuid, int> m_nodeIndexById;
    
    bool m_updating {false};
};

} // namespace Structura::Visualization
