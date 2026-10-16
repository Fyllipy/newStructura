#include "MemberRenderer.h"

#include <vtkRenderer.h>
#include <vtkActor.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkCellArray.h>
#include <vtkUnsignedCharArray.h>
#include <vtkProperty.h>
#include <vtkCellData.h>

namespace Structura::Visualization {

MemberRenderer::MemberRenderer()
{
    m_barData->SetLines(m_barLines);

    m_colors->SetNumberOfComponents(4);
    m_colors->SetName("BarColors");
    m_barData->GetCellData()->SetScalars(m_colors);

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(m_barData);
    mapper->ScalarVisibilityOn();
    mapper->SetColorModeToDirectScalars();
    mapper->SetScalarModeToUseCellData();

    m_actor->SetMapper(mapper);
    if (auto* prop = m_actor->GetProperty()) {
        prop->SetLineWidth(2.5);  // A bit thicker for better MSAA appearance
        prop->LightingOff();      // Vector style, no volumetric shading
    }
}

MemberRenderer::~MemberRenderer() = default;

void MemberRenderer::addToRenderer(vtkRenderer* renderer)
{
    if (renderer) {
        renderer->AddActor(m_actor);
    }
}

void MemberRenderer::removeFromRenderer(vtkRenderer* renderer)
{
    if (renderer) {
        renderer->RemoveActor(m_actor);
    }
}

void MemberRenderer::setPoints(vtkPoints* points)
{
    m_barData->SetPoints(points);
}

void MemberRenderer::clear()
{
    m_barLines->Reset();
    m_colors->Reset();
    m_bars.clear();
    m_hoveredBar = QUuid();
    m_selectedBars.clear();

    m_barLines->Modified();
    m_colors->Modified();
    m_barData->Modified();
}

void MemberRenderer::addBar(const QUuid& id, vtkIdType nodeI_idx, vtkIdType nodeJ_idx)
{
    vtkIdType pts[2] = {nodeI_idx, nodeJ_idx};
    m_barLines->InsertNextCell(2, pts);
    m_bars.push_back(id);
    {
        const unsigned char rgba[4] = {COLOR_NORMAL[0], COLOR_NORMAL[1], COLOR_NORMAL[2], 255};
        m_colors->InsertNextTypedTuple(rgba);
    }

    m_barLines->Modified();
    m_colors->Modified();
    m_barData->Modified();
}

void MemberRenderer::setHoveredBar(const QUuid& id)
{
    if (m_hoveredBar == id) return;
    m_hoveredBar = id;
    rebuildColors();
}

void MemberRenderer::clearHoveredBar()
{
    if (m_hoveredBar.isNull()) return;
    m_hoveredBar = QUuid();
    rebuildColors();
}

void MemberRenderer::setSelectedBars(const QSet<QUuid>& ids)
{
    if (m_selectedBars == ids) return;
    m_selectedBars = ids;
    rebuildColors();
}

void MemberRenderer::setAppearance(const AppearanceMap& appearance)
{
    m_appearance = appearance;
    rebuildColors();
}

void MemberRenderer::setVisible(bool visible)
{
    m_actor->SetVisibility(visible);
}

bool MemberRenderer::isVisible() const
{
    return m_actor->GetVisibility();
}

void MemberRenderer::rebuildColors()
{
    for (int i = 0; i < static_cast<int>(m_bars.size()); ++i) {
        const QUuid& id = m_bars[i];
        const auto appearance = m_appearance.constFind(id);
        unsigned char rgba[4] = {COLOR_NORMAL[0], COLOR_NORMAL[1], COLOR_NORMAL[2], 255};
        if (appearance != m_appearance.constEnd()) {
            if (appearance->hasColor) {
                rgba[0] = appearance->color[0];
                rgba[1] = appearance->color[1];
                rgba[2] = appearance->color[2];
            }
            if (appearance->state == ElementAppearance::State::Dimmed) {
                const auto dimmed = dimColor({{rgba[0], rgba[1], rgba[2]}});
                rgba[0] = dimmed[0];
                rgba[1] = dimmed[1];
                rgba[2] = dimmed[2];
            } else if (appearance->state == ElementAppearance::State::Hidden) {
                rgba[3] = 0;
            }
        }
        if (rgba[3] != 0) {
            if (m_selectedBars.contains(id)) {
                rgba[0] = COLOR_SELECTED[0]; rgba[1] = COLOR_SELECTED[1]; rgba[2] = COLOR_SELECTED[2];
            } else if (id == m_hoveredBar) {
                rgba[0] = COLOR_HOVER[0]; rgba[1] = COLOR_HOVER[1]; rgba[2] = COLOR_HOVER[2];
            }
        }
        m_colors->SetTypedTuple(i, rgba);
    }
    m_colors->Modified();
    m_barData->Modified();
}

} // namespace Structura::Visualization
