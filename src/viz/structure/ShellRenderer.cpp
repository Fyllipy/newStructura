#include "viz/structure/ShellRenderer.h"

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

ShellRenderer::ShellRenderer()
    : m_faces(vtkSmartPointer<vtkActor>::New())
    , m_edges(vtkSmartPointer<vtkActor>::New())
{
    auto faceMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    faceMapper->SetInputData(vtkSmartPointer<vtkPolyData>::New());
    faceMapper->SetColorModeToDirectScalars();
    // as arestas ficam ligeiramente a frente para nao brigar com as faces
    faceMapper->SetResolveCoincidentTopologyToPolygonOffset();
    m_faces->SetMapper(faceMapper);
    m_faces->PickableOn();
    m_faces->SetVisibility(0);
    m_picker = vtkSmartPointer<vtkCellPicker>::New();
    m_picker->SetTolerance(0.0005);
    m_picker->SetPickFromList(true);
    m_picker->AddPickList(m_faces);
    m_faces->GetProperty()->SetAmbient(0.45);
    m_faces->GetProperty()->SetDiffuse(0.65);
    m_faces->GetProperty()->SetSpecular(0.05);
    m_faces->GetProperty()->BackfaceCullingOff();

    auto edgeMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    edgeMapper->SetInputData(vtkSmartPointer<vtkPolyData>::New());
    edgeMapper->SetResolveCoincidentTopologyToPolygonOffset();
    m_edges->SetMapper(edgeMapper);
    m_edges->PickableOff();
    m_edges->SetVisibility(0);
    m_edges->GetProperty()->SetColor(0.15, 0.17, 0.2);
    m_edges->GetProperty()->SetOpacity(0.55);
    m_edges->GetProperty()->SetLineWidth(1.0);
    m_edges->GetProperty()->LightingOff();
}

void ShellRenderer::attach(vtkRenderer *renderer)
{
    m_renderer = renderer;
    if (m_renderer) {
        m_renderer->AddActor(m_faces);
        m_renderer->AddActor(m_edges);
    }
}

int ShellRenderer::pickQuad(int displayX, int displayY) const
{
    if (!m_renderer || !m_visible || !m_picker) {
        return -1;
    }
    if (m_picker->Pick(displayX, displayY, 0.0, m_renderer)) {
        const vtkIdType cell = m_picker->GetCellId();
        if (cell >= 0) {
            return static_cast<int>(cell / 2);   // dois triangulos por quadrilatero
        }
    }
    return -1;
}

void ShellRenderer::setVisible(bool visible)
{
    m_visible = visible;
    m_faces->SetVisibility(visible ? 1 : 0);
    m_edges->SetVisibility(visible ? 1 : 0);
}

void ShellRenderer::setMesh(const Structura::Shell::RenderMesh &mesh)
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
    if (auto *mapper = vtkPolyDataMapper::SafeDownCast(m_faces->GetMapper())) {
        mapper->SetInputData(data);
        mapper->Modified();
    }

    auto edgePoints = vtkSmartPointer<vtkPoints>::New();
    edgePoints->SetNumberOfPoints(static_cast<vtkIdType>(mesh.edgePoints.size()));
    auto lines = vtkSmartPointer<vtkCellArray>::New();
    for (std::size_t i = 0; i < mesh.edgePoints.size(); ++i) {
        edgePoints->SetPoint(static_cast<vtkIdType>(i), mesh.edgePoints[i][0], mesh.edgePoints[i][1], mesh.edgePoints[i][2]);
        if (i % 2 == 1) {
            const vtkIdType ids[2] = {static_cast<vtkIdType>(i - 1), static_cast<vtkIdType>(i)};
            lines->InsertNextCell(2, ids);
        }
    }
    auto edgeData = vtkSmartPointer<vtkPolyData>::New();
    edgeData->SetPoints(edgePoints);
    edgeData->SetLines(lines);
    if (auto *mapper = vtkPolyDataMapper::SafeDownCast(m_edges->GetMapper())) {
        mapper->SetInputData(edgeData);
        mapper->Modified();
    }
}

} // namespace Structura::Visualization
