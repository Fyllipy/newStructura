#include "NodeRenderer.h"

#include <vtkRenderer.h>
#include <vtkActor.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkUnsignedCharArray.h>
#include <vtkDistanceToCamera.h>
#include <vtkGlyph3DMapper.h>
#include <vtkSphereSource.h>
#include <vtkProperty.h>
#include <vtkPointData.h>

namespace Structura::Visualization {

NodeRenderer::NodeRenderer()
{

    m_colors->SetNumberOfComponents(3);
    m_colors->SetName("NodeColors");
    m_pointCloud->GetPointData()->SetScalars(m_colors);
    m_mask->SetNumberOfComponents(1);
    m_mask->SetName("NodeMask");
    m_pointCloud->GetPointData()->AddArray(m_mask);

    // Filter to scale glyphs based on distance to camera, keeping screen size constant
    m_distanceFilter->SetInputData(m_pointCloud);
    m_distanceFilter->SetScreenSize(5.0); // radius in pixels approximately

    // Unlit sphere acts as a perfect 2D circular silhouette from any angle
    m_diskGlyph->SetRadius(1.0);
    m_diskGlyph->SetThetaResolution(16);
    m_diskGlyph->SetPhiResolution(16);

    m_mapper->SetInputConnection(m_distanceFilter->GetOutputPort());
    m_mapper->SetSourceConnection(m_diskGlyph->GetOutputPort());
    m_mapper->SetScaleArray("DistanceToCamera");
    m_mapper->SetMasking(true);
    m_mapper->SetMaskArray("NodeMask");
    m_mapper->SetScaleModeToScaleByMagnitude();
    
    // Configura o mapper para usar as cores definidas nos scalars (m_colors)
    m_mapper->ScalarVisibilityOn();
    m_mapper->SetColorModeToDirectScalars();
    m_mapper->SetScalarModeToUsePointData();

    m_actor->SetMapper(m_mapper);
    if (auto* prop = m_actor->GetProperty()) {
        prop->LightingOff(); // Garante aspecto plano (2D flat color)
    }
}

NodeRenderer::~NodeRenderer() = default;

void NodeRenderer::addToRenderer(vtkRenderer* renderer)
{
    if (renderer) {
        m_distanceFilter->SetRenderer(renderer);
        renderer->AddActor(m_actor);
    }
}

void NodeRenderer::removeFromRenderer(vtkRenderer* renderer)
{
    if (renderer) {
        renderer->RemoveActor(m_actor);
        m_distanceFilter->SetRenderer(nullptr);
    }
}

void NodeRenderer::setPoints(vtkPoints* points)
{
    m_pointCloud->SetPoints(points);
}

void NodeRenderer::clear()
{
    m_colors->Reset();
    m_mask->Reset();
    m_appearance.clear();
    m_nodes.clear();
    m_nodeIndexById.clear();
    m_hoveredNode = QUuid();
    m_selectedNodes.clear();

    m_colors->Modified();
    m_pointCloud->Modified();
}

void NodeRenderer::addNode(const QUuid& id, double /*x*/, double /*y*/, double /*z*/)
{
    if (m_nodeIndexById.contains(id)) return;

    // A coordenada em si é inserida pelo StructureRenderer,
    // o NodeRenderer apenas acompanha o ID e o índice (que é sequencial).
    int idx = static_cast<int>(m_nodes.size());
    m_nodes.push_back(id);
    m_nodeIndexById[id] = idx;
    m_colors->InsertNextTypedTuple(COLOR_NORMAL);
    m_mask->InsertNextValue(1);

    m_colors->Modified();
    m_mask->Modified();
    m_pointCloud->Modified();
}

void NodeRenderer::updateNodePosition(const QUuid& /*id*/, double /*x*/, double /*y*/, double /*z*/)
{
    // A posição em si já foi atualizada no m_points global via StructureRenderer
    // Então apenas disparamos um modified no polydata para forçar o update da pipeline visual.
    m_pointCloud->Modified();
}

void NodeRenderer::setHoveredNode(const QUuid& id)
{
    if (m_hoveredNode == id) return;
    m_hoveredNode = id;
    rebuildColors();
}

void NodeRenderer::clearHoveredNode()
{
    if (m_hoveredNode.isNull()) return;
    m_hoveredNode = QUuid();
    rebuildColors();
}

void NodeRenderer::setSelectedNodes(const QSet<QUuid>& ids)
{
    if (m_selectedNodes == ids) return;
    m_selectedNodes = ids;
    rebuildColors();
}

void NodeRenderer::setAppearance(const AppearanceMap& appearance)
{
    m_appearance = appearance;
    rebuildColors();
}

void NodeRenderer::setVisible(bool visible)
{
    m_actor->SetVisibility(visible);
}

bool NodeRenderer::isVisible() const
{
    return m_actor->GetVisibility();
}

int NodeRenderer::nodeIndex(const QUuid& id) const
{
    return m_nodeIndexById.value(id, -1);
}

void NodeRenderer::rebuildColors()
{
    for (int i = 0; i < static_cast<int>(m_nodes.size()); ++i) {
        const QUuid& id = m_nodes[i];
        const auto appearance = m_appearance.constFind(id);
        const bool hidden = appearance != m_appearance.constEnd()
            && appearance->state == ElementAppearance::State::Hidden;
        m_mask->SetValue(i, hidden ? 0 : 1);

        std::array<unsigned char, 3> base {{COLOR_NORMAL[0], COLOR_NORMAL[1], COLOR_NORMAL[2]}};
        if (appearance != m_appearance.constEnd()) {
            if (appearance->hasColor) {
                base = appearance->color;
            }
            if (appearance->state == ElementAppearance::State::Dimmed) {
                base = dimColor(base);
            }
        }
        if (m_selectedNodes.contains(id)) {
            m_colors->SetTypedTuple(i, COLOR_SELECTED);
        } else if (id == m_hoveredNode) {
            m_colors->SetTypedTuple(i, COLOR_HOVER);
        } else {
            m_colors->SetTypedTuple(i, base.data());
        }
    }
    m_mask->Modified();
    m_colors->Modified();
    m_pointCloud->Modified();
}

} // namespace Structura::Visualization
