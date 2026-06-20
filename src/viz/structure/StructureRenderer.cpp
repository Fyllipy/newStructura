#include "StructureRenderer.h"
#include "NodeRenderer.h"
#include "MemberRenderer.h"

#include <vtkRenderer.h>
#include <vtkPoints.h>
#include <vtkActor.h>

namespace Structura::Visualization {

StructureRenderer::StructureRenderer()
    : m_nodeRenderer(std::make_unique<NodeRenderer>())
    , m_memberRenderer(std::make_unique<MemberRenderer>())
{
    // O StructureRenderer é dono dos pontos, as sub-classes apenas visualizam.
    m_nodeRenderer->setPoints(m_points);
    m_memberRenderer->setPoints(m_points);
}

StructureRenderer::~StructureRenderer() = default;

void StructureRenderer::addToRenderer(vtkRenderer* renderer)
{
    m_nodeRenderer->addToRenderer(renderer);
    m_memberRenderer->addToRenderer(renderer);
}

void StructureRenderer::removeFromRenderer(vtkRenderer* renderer)
{
    m_nodeRenderer->removeFromRenderer(renderer);
    m_memberRenderer->removeFromRenderer(renderer);
}

void StructureRenderer::clear()
{
    m_points->Reset();
    m_nodeIndexById.clear();
    
    m_nodeRenderer->clear();
    m_memberRenderer->clear();

    m_points->Modified();
}

void StructureRenderer::beginUpdate()
{
    m_updating = true;
}

void StructureRenderer::endUpdate()
{
    m_updating = false;
    m_points->Modified();
}

void StructureRenderer::addNode(const QUuid& id, double x, double y, double z)
{
    if (m_nodeIndexById.contains(id)) return;

    int idx = m_points->InsertNextPoint(x, y, z);
    m_nodeIndexById[id] = idx;
    
    m_nodeRenderer->addNode(id, x, y, z);
    
    if (!m_updating) {
        m_points->Modified();
    }
}

void StructureRenderer::updateNodePosition(const QUuid& id, double x, double y, double z)
{
    int idx = m_nodeIndexById.value(id, -1);
    if (idx >= 0) {
        m_points->SetPoint(idx, x, y, z);
        m_nodeRenderer->updateNodePosition(id, x, y, z);
        
        if (!m_updating) {
            m_points->Modified();
        }
    }
}

void StructureRenderer::addBar(const QUuid& id, const QUuid& startNodeId, const QUuid& endNodeId)
{
    int i_idx = m_nodeIndexById.value(startNodeId, -1);
    int j_idx = m_nodeIndexById.value(endNodeId, -1);
    
    if (i_idx >= 0 && j_idx >= 0) {
        m_memberRenderer->addBar(id, i_idx, j_idx);
    }
}

void StructureRenderer::setHoveredNode(const QUuid& id)
{
    m_nodeRenderer->setHoveredNode(id);
}

void StructureRenderer::clearHoveredNode()
{
    m_nodeRenderer->clearHoveredNode();
}

void StructureRenderer::setSelectedNodes(const QSet<QUuid>& ids)
{
    m_nodeRenderer->setSelectedNodes(ids);
}

void StructureRenderer::setHoveredBar(const QUuid& id)
{
    m_memberRenderer->setHoveredBar(id);
}

void StructureRenderer::clearHoveredBar()
{
    m_memberRenderer->clearHoveredBar();
}

void StructureRenderer::setSelectedBars(const QSet<QUuid>& ids)
{
    m_memberRenderer->setSelectedBars(ids);
}

vtkActor* StructureRenderer::getNodeActor() const
{
    return m_nodeRenderer->getActor();
}

vtkActor* StructureRenderer::getBarActor() const
{
    return m_memberRenderer->getActor();
}

} // namespace Structura::Visualization
