#include "DeformedShapeRenderer.h"

#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

#include <algorithm>
#include <cmath>

namespace Structura::Visualization {

DeformedShapeRenderer::DeformedShapeRenderer()
    : m_data(vtkSmartPointer<vtkPolyData>::New())
    , m_mapper(vtkSmartPointer<vtkPolyDataMapper>::New())
    , m_actor(vtkSmartPointer<vtkActor>::New())
{
    m_mapper->SetInputData(m_data);
    m_actor->SetMapper(m_mapper);
    m_actor->GetProperty()->SetColor(0.91, 0.35, 0.05);
    m_actor->GetProperty()->SetLineWidth(3.0f);
    m_actor->GetProperty()->LightingOff();
    m_actor->PickableOff();
    m_actor->SetVisibility(false);
}

DeformedShapeRenderer::~DeformedShapeRenderer() = default;

void DeformedShapeRenderer::initialize(vtkRenderer *renderer)
{
    m_renderer = renderer;
    if (m_renderer) {
        m_renderer->AddActor(m_actor);
    }
}

void DeformedShapeRenderer::clearAll()
{
    m_members.clear();
    m_maxDisplacement = 0.0;
    m_maxNode = 0;
    m_data->Initialize();
    m_actor->SetVisibility(false);
}

void DeformedShapeRenderer::setData(std::vector<Member> members, double modelExtent, int maxNode)
{
    m_members = std::move(members);
    m_extent = std::max(modelExtent, 1e-9);
    m_maxNode = maxNode;
    m_maxDisplacement = 0.0;
    for (const auto &member : m_members) {
        for (const auto &d : member.disp) {
            m_maxDisplacement = std::max(m_maxDisplacement, std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));
        }
    }
    // escala automatica: o maior deslocamento vira ~6% do tamanho do modelo
    m_autoScale = m_maxDisplacement > 1e-15 ? 0.06 * m_extent / m_maxDisplacement : 1.0;
    rebuild();
}

void DeformedShapeRenderer::setUserScale(double scale)
{
    m_userScale = std::max(scale, 1e-6);
    rebuild();
}

void DeformedShapeRenderer::setVisible(bool visible)
{
    m_visible = visible;
    m_actor->SetVisibility(visible && !m_members.empty() ? 1 : 0);
}

void DeformedShapeRenderer::rebuild()
{
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto cells = vtkSmartPointer<vtkCellArray>::New();
    const double k = m_autoScale * m_userScale;
    for (const auto &member : m_members) {
        const std::size_t n = std::min(member.base.size(), member.disp.size());
        if (n < 2) {
            continue;
        }
        cells->InsertNextCell(static_cast<vtkIdType>(n));
        for (std::size_t i = 0; i < n; ++i) {
            cells->InsertCellPoint(points->InsertNextPoint(member.base[i][0] + k * member.disp[i][0], member.base[i][1] + k * member.disp[i][1],
                                                           member.base[i][2] + k * member.disp[i][2]));
        }
    }
    m_data->SetPoints(points);
    m_data->SetLines(cells);
    m_data->Modified();
    m_actor->SetVisibility(m_visible && !m_members.empty() ? 1 : 0);
}

} // namespace Structura::Visualization
