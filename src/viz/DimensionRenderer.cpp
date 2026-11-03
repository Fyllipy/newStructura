#include "viz/DimensionRenderer.h"

#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

namespace Structura::Visualization {

DimensionRenderer::DimensionRenderer()
    : m_data(vtkSmartPointer<vtkPolyData>::New())
    , m_mapper(vtkSmartPointer<vtkPolyDataMapper>::New())
    , m_actor(vtkSmartPointer<vtkActor>::New())
{
    m_mapper->SetInputData(m_data);
    m_actor->SetMapper(m_mapper);
    m_actor->GetProperty()->SetColor(0.12, 0.37, 0.68);
    m_actor->GetProperty()->SetLineWidth(1.6f);
    m_actor->GetProperty()->LightingOff();
    m_actor->PickableOff();
    m_actor->SetVisibility(false);
}

DimensionRenderer::~DimensionRenderer() = default;

void DimensionRenderer::attach(vtkRenderer *renderer)
{
    if (renderer) {
        renderer->AddActor(m_actor);
    }
}

void DimensionRenderer::render(const std::vector<std::vector<Vec3>> &lines)
{
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto cells = vtkSmartPointer<vtkCellArray>::New();
    for (const auto &line : lines) {
        if (line.size() < 2) {
            continue;
        }
        cells->InsertNextCell(static_cast<vtkIdType>(line.size()));
        for (const auto &p : line) {
            cells->InsertCellPoint(points->InsertNextPoint(p[0], p[1], p[2]));
        }
    }
    m_data->SetPoints(points);
    m_data->SetLines(cells);
    m_data->Modified();
    m_actor->SetVisibility(!lines.empty());
}

} // namespace Structura::Visualization
