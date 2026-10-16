#include "viz/BarTypeRenderer.h"

#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkGlyph3D.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSphereSource.h>
#include <vtkUnsignedCharArray.h>

#include <algorithm>
#include <cmath>

namespace Structura::Visualization {

namespace {

double length(const BarTypeRenderer::Vec3 &a, const BarTypeRenderer::Vec3 &b)
{
    return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
}

} // namespace

BarTypeRenderer::BarTypeRenderer()
    : m_lineActor(vtkSmartPointer<vtkActor>::New())
{
}

BarTypeRenderer::~BarTypeRenderer()
{
    if (m_renderer) {
        for (auto &actor : m_actors) {
            m_renderer->RemoveActor(actor);
        }
        m_renderer->RemoveActor(m_lineActor);
    }
}

void BarTypeRenderer::attach(vtkRenderer *renderer)
{
    m_renderer = renderer;
    if (m_renderer) {
        m_renderer->AddActor(m_lineActor);
    }
}

vtkSmartPointer<vtkActor> BarTypeRenderer::makeGlyphActor(vtkPolyData *points, double radius, bool outlineOnly)
{
    auto sphere = vtkSmartPointer<vtkSphereSource>::New();
    sphere->SetRadius(radius);
    sphere->SetThetaResolution(20);
    sphere->SetPhiResolution(14);
    auto glyph = vtkSmartPointer<vtkGlyph3D>::New();
    glyph->SetInputData(points);
    glyph->SetSourceConnection(sphere->GetOutputPort());
    glyph->SetScaleModeToDataScalingOff();
    glyph->SetColorModeToColorByScalar();
    glyph->Update();
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(glyph->GetOutputPort());
    mapper->SetScalarModeToUsePointData();
    mapper->SetColorModeToDirectScalars();
    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->PickableOff();
    actor->GetProperty()->LightingOff();
    if (outlineOnly) {   // so as faces de tras: forma o contorno escuro da esfera
        actor->GetProperty()->FrontfaceCullingOn();
    }
    return actor;
}

void BarTypeRenderer::render(const std::vector<Marker> &markers, bool visible)
{
    if (!m_renderer) {
        return;
    }
    for (auto &actor : m_actors) {
        m_renderer->RemoveActor(actor);
    }
    m_actors.clear();

    // raio de referencia: fracao do comprimento medio das barras
    double total = 0.0;
    int count = 0;
    for (const auto &m : markers) {
        total += length(m.start, m.end);
        ++count;
    }
    const double mean = count > 0 ? total / count : 1.0;
    const double hingeRadius = std::clamp(0.05 * mean, 0.02, 0.35);

    auto hingePoints = vtkSmartPointer<vtkPolyData>::New();
    auto hingeVertices = vtkSmartPointer<vtkPoints>::New();
    auto hingeWhite = vtkSmartPointer<vtkUnsignedCharArray>::New();
    hingeWhite->SetNumberOfComponents(3);
    hingeWhite->SetName("color");
    auto hingeDark = vtkSmartPointer<vtkPolyData>::New();
    auto darkVertices = vtkSmartPointer<vtkPoints>::New();
    auto darkColors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    darkColors->SetNumberOfComponents(3);
    darkColors->SetName("color");
    auto tieMarks = vtkSmartPointer<vtkPolyData>::New();
    auto markVertices = vtkSmartPointer<vtkPoints>::New();
    auto markColors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    markColors->SetNumberOfComponents(3);
    markColors->SetName("color");

    auto lines = vtkSmartPointer<vtkPolyData>::New();
    auto linePoints = vtkSmartPointer<vtkPoints>::New();
    auto lineCells = vtkSmartPointer<vtkCellArray>::New();
    auto lineColors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    lineColors->SetNumberOfComponents(3);

    auto inset = [](const Vec3 &from, const Vec3 &to, double distance) {
        const double len = std::max(length(from, to), 1e-9);
        const double t = std::min(distance / len, 0.45);
        return Vec3 {from[0] + (to[0] - from[0]) * t, from[1] + (to[1] - from[1]) * t, from[2] + (to[2] - from[2]) * t};
    };
    for (const auto &m : markers) {
        const double len = length(m.start, m.end);
        if (len < 1e-9) {
            continue;
        }
        const double offset = hingeRadius * 2.2;   // a esfera fica ao lado do no, sobre a barra
        if (m.hingeStart) {
            const Vec3 p = inset(m.start, m.end, offset);
            hingeVertices->InsertNextPoint(p[0], p[1], p[2]);
            hingeWhite->InsertNextTuple3(255, 255, 255);
            darkVertices->InsertNextPoint(p[0], p[1], p[2]);
            darkColors->InsertNextTuple3(30, 35, 43);
        }
        if (m.hingeEnd) {
            const Vec3 p = inset(m.end, m.start, offset);
            hingeVertices->InsertNextPoint(p[0], p[1], p[2]);
            hingeWhite->InsertNextTuple3(255, 255, 255);
            darkVertices->InsertNextPoint(p[0], p[1], p[2]);
            darkColors->InsertNextTuple3(30, 35, 43);
        }
        if (m.kind == 1 || m.kind == 2) {
            const bool tie = m.kind == 2;
            const unsigned char r = tie ? 230 : 142, g = tie ? 126 : 68, b = tie ? 34 : 173;
            const vtkIdType a = linePoints->InsertNextPoint(m.start[0], m.start[1], m.start[2]);
            const vtkIdType c = linePoints->InsertNextPoint(m.end[0], m.end[1], m.end[2]);
            lineCells->InsertNextCell(2);
            lineCells->InsertCellPoint(a);
            lineCells->InsertCellPoint(c);
            lineColors->InsertNextTuple3(r, g, b);
            // marcas ao longo da barra: tirante = tres pontos; trelica = ponto medio
            const int marks = tie ? 3 : 1;
            for (int i = 1; i <= marks; ++i) {
                const double t = static_cast<double>(i) / (marks + 1);
                markVertices->InsertNextPoint(m.start[0] + (m.end[0] - m.start[0]) * t, m.start[1] + (m.end[1] - m.start[1]) * t,
                                              m.start[2] + (m.end[2] - m.start[2]) * t);
                markColors->InsertNextTuple3(r, g, b);
            }
        }
    }

    auto build = [](vtkPolyData *data, vtkPoints *points, vtkUnsignedCharArray *colors) {
        data->SetPoints(points);
        data->GetPointData()->SetScalars(colors);
    };
    build(hingePoints, hingeVertices, hingeWhite);
    build(hingeDark, darkVertices, darkColors);
    build(tieMarks, markVertices, markColors);

    if (hingeVertices->GetNumberOfPoints() > 0) {
        m_actors.push_back(makeGlyphActor(hingeDark, hingeRadius * 1.3, true));
        m_actors.push_back(makeGlyphActor(hingePoints, hingeRadius, false));
    }
    if (markVertices->GetNumberOfPoints() > 0) {
        m_actors.push_back(makeGlyphActor(tieMarks, hingeRadius * 0.55, false));
    }
    for (auto &actor : m_actors) {
        actor->SetVisibility(visible);
        m_renderer->AddActor(actor);
    }

    lines->SetPoints(linePoints);
    lines->SetLines(lineCells);
    lines->GetCellData()->SetScalars(lineColors);
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(lines);
    mapper->SetScalarModeToUseCellData();
    mapper->SetColorModeToDirectScalars();
    m_lineActor->SetMapper(mapper);
    m_lineActor->GetProperty()->SetLineWidth(4.0f);
    m_lineActor->GetProperty()->LightingOff();
    m_lineActor->PickableOff();
    m_lineActor->SetVisibility(visible && lineCells->GetNumberOfCells() > 0);
}

} // namespace Structura::Visualization
