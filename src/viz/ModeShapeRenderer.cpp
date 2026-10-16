#include "viz/ModeShapeRenderer.h"

#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

namespace Structura::Visualization {

ModeShapeRenderer::ModeShapeRenderer()
    : m_data(vtkSmartPointer<vtkPolyData>::New())
    , m_mapper(vtkSmartPointer<vtkPolyDataMapper>::New())
    , m_actor(vtkSmartPointer<vtkActor>::New())
{
    m_mapper->SetInputData(m_data);
    m_actor->SetMapper(m_mapper);
    m_actor->GetProperty()->SetColor(0.85, 0.2, 0.1);
    m_actor->GetProperty()->SetLineWidth(3.0f);
    m_actor->GetProperty()->LightingOff();
    m_actor->PickableOff();
    m_actor->SetVisibility(false);
}

ModeShapeRenderer::~ModeShapeRenderer() = default;

void ModeShapeRenderer::attach(vtkRenderer *renderer)
{
    if (renderer) {
        renderer->AddActor(m_actor);
    }
}

void ModeShapeRenderer::render(const std::vector<std::vector<Vec3>> &lines, bool visible)
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
    m_actor->SetVisibility(visible && !lines.empty());
}

} // namespace Structura::Visualization
