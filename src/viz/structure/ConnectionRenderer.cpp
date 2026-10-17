#include "viz/structure/ConnectionRenderer.h"

#include <vtkCellArray.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkUnsignedCharArray.h>

namespace Structura::Visualization {

ConnectionRenderer::ConnectionRenderer()
    : m_actor(vtkSmartPointer<vtkActor>::New())
{
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(vtkSmartPointer<vtkPolyData>::New());
    mapper->SetColorModeToDirectScalars();
    m_actor->SetMapper(mapper);
    m_actor->PickableOff();
    m_actor->SetVisibility(0);
    m_actor->GetProperty()->SetAmbient(0.35);
    m_actor->GetProperty()->SetDiffuse(0.7);
    m_actor->GetProperty()->SetSpecular(0.25);
}

void ConnectionRenderer::attach(vtkRenderer *renderer)
{
    m_renderer = renderer;
    if (m_renderer) {
        m_renderer->AddActor(m_actor);
    }
}

void ConnectionRenderer::setVisible(bool visible)
{
    m_visible = visible;
    m_actor->SetVisibility(visible ? 1 : 0);
}

void ConnectionRenderer::setMesh(const Mesh &mesh)
{
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetNumberOfPoints(static_cast<vtkIdType>(mesh.points.size()));
    auto normals = vtkSmartPointer<vtkFloatArray>::New();
    normals->SetNumberOfComponents(3);
    normals->SetNumberOfTuples(static_cast<vtkIdType>(mesh.points.size()));
    auto colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    colors->SetNumberOfComponents(3);
    colors->SetNumberOfTuples(static_cast<vtkIdType>(mesh.points.size()));
    for (std::size_t i = 0; i < mesh.points.size(); ++i) {
        const auto id = static_cast<vtkIdType>(i);
        points->SetPoint(id, mesh.points[i][0], mesh.points[i][1], mesh.points[i][2]);
        normals->SetTuple3(id, mesh.normals[i][0], mesh.normals[i][1], mesh.normals[i][2]);
        colors->SetTuple3(id, mesh.colors[i][0], mesh.colors[i][1], mesh.colors[i][2]);
    }
    auto cells = vtkSmartPointer<vtkCellArray>::New();
    for (const auto &t : mesh.triangles) {
        const vtkIdType ids[3] = {t[0], t[1], t[2]};
        cells->InsertNextCell(3, ids);
    }
    auto data = vtkSmartPointer<vtkPolyData>::New();
    data->SetPoints(points);
    data->SetPolys(cells);
    data->GetPointData()->SetNormals(normals);
    data->GetPointData()->SetScalars(colors);
    if (auto *mapper = vtkPolyDataMapper::SafeDownCast(m_actor->GetMapper())) {
        mapper->SetInputData(data);
        mapper->Modified();
    }
}

} // namespace Structura::Visualization
