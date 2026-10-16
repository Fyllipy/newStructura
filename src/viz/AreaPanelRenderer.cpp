#include "viz/AreaPanelRenderer.h"

#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkUnsignedCharArray.h>

#include <algorithm>
#include <cmath>

namespace Structura::Visualization {

namespace {

using Vec3 = AreaPanelRenderer::Vec3;

Vec3 operator+(const Vec3 &a, const Vec3 &b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 operator-(const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 operator*(const Vec3 &a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
double length(const Vec3 &a) { return std::sqrt(dot(a, a)); }
Vec3 normalized(const Vec3 &a)
{
    const double n = length(a);
    return n > 1e-12 ? a * (1.0 / n) : Vec3 {0, 0, -1};
}
Vec3 cross(const Vec3 &a, const Vec3 &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }

Vec3 anyPerpendicular(const Vec3 &d)
{
    const Vec3 helper = std::abs(d[2]) < 0.9 ? Vec3 {0, 0, 1} : Vec3 {1, 0, 0};
    return normalized(cross(d, helper));
}

} // namespace

AreaPanelRenderer::AreaPanelRenderer()
    : m_surfaceData(vtkSmartPointer<vtkPolyData>::New())
    , m_surfaceMapper(vtkSmartPointer<vtkPolyDataMapper>::New())
    , m_surfaceActor(vtkSmartPointer<vtkActor>::New())
    , m_lineData(vtkSmartPointer<vtkPolyData>::New())
    , m_lineMapper(vtkSmartPointer<vtkPolyDataMapper>::New())
    , m_lineActor(vtkSmartPointer<vtkActor>::New())
{
    m_surfaceMapper->SetInputData(m_surfaceData);
    m_surfaceMapper->SetScalarModeToUseCellData();
    m_surfaceMapper->SetColorModeToDirectScalars();
    m_surfaceActor->SetMapper(m_surfaceMapper);
    m_surfaceActor->GetProperty()->SetOpacity(0.24);
    m_surfaceActor->GetProperty()->LightingOff();
    m_surfaceActor->PickableOff();
    m_surfaceActor->SetVisibility(false);

    m_lineMapper->SetInputData(m_lineData);
    m_lineMapper->SetScalarModeToUseCellData();
    m_lineMapper->SetColorModeToDirectScalars();
    m_lineActor->SetMapper(m_lineMapper);
    m_lineActor->GetProperty()->SetLineWidth(1.6f);
    m_lineActor->GetProperty()->LightingOff();
    m_lineActor->PickableOff();
    m_lineActor->SetVisibility(false);
}

AreaPanelRenderer::~AreaPanelRenderer() = default;

void AreaPanelRenderer::attach(vtkRenderer *renderer)
{
    if (!renderer) {
        return;
    }
    renderer->AddActor(m_surfaceActor);
    renderer->AddActor(m_lineActor);
}

void AreaPanelRenderer::render(const std::vector<Panel> &panels, bool visible)
{
    auto surfacePoints = vtkSmartPointer<vtkPoints>::New();
    auto surfaceCells = vtkSmartPointer<vtkCellArray>::New();
    auto surfaceColors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    surfaceColors->SetNumberOfComponents(3);
    auto linePoints = vtkSmartPointer<vtkPoints>::New();
    auto lineCells = vtkSmartPointer<vtkCellArray>::New();
    auto lineColors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    lineColors->SetNumberOfComponents(3);

    auto addLine = [&](const Vec3 &a, const Vec3 &b, const unsigned char color[3]) {
        const vtkIdType ids[2] = {linePoints->InsertNextPoint(a.data()), linePoints->InsertNextPoint(b.data())};
        lineCells->InsertNextCell(2, ids);
        lineColors->InsertNextTypedTuple(color);
    };
    auto addArrow = [&](const Vec3 &tail, const Vec3 &dir, double len, const Vec3 &side, const unsigned char color[3]) {
        const Vec3 tip = tail + dir * len;
        addLine(tail, tip, color);
        const double head = len * 0.28;
        addLine(tip, tip - dir * head + side * (head * 0.55), color);
        addLine(tip, tip - dir * head - side * (head * 0.55), color);
    };

    if (visible) {
        for (const Panel &panel : panels) {
            if (panel.polygon.size() < 3) {
                continue;
            }
            const unsigned char surface[3] = {static_cast<unsigned char>(panel.wind ? 40 : 224), static_cast<unsigned char>(panel.wind ? 150 : 161), static_cast<unsigned char>(panel.wind ? 220 : 0)};
            const unsigned char outline[3] = {static_cast<unsigned char>(panel.wind ? 20 : 160), static_cast<unsigned char>(panel.wind ? 100 : 110), static_cast<unsigned char>(panel.wind ? 170 : 0)};
            const unsigned char barArrow[3] = {192, 57, 43};

            std::vector<vtkIdType> ids;
            for (const Vec3 &p : panel.polygon) {
                ids.push_back(surfacePoints->InsertNextPoint(p.data()));
            }
            surfaceCells->InsertNextCell(static_cast<vtkIdType>(ids.size()), ids.data());
            surfaceColors->InsertNextTypedTuple(surface);
            for (std::size_t i = 0; i < panel.polygon.size(); ++i) {
                addLine(panel.polygon[i], panel.polygon[(i + 1) % panel.polygon.size()], outline);
            }

            // sentido da forca: p * d. Pressao positiva: as setas chegam ao pano; negativa (succao): saem dele.
            const bool arriving = panel.pressure >= 0.0;
            const Vec3 direction = normalized(panel.direction) * (arriving ? 1.0 : -1.0);
            const Vec3 side = anyPerpendicular(direction);
            // tamanho de referencia: diagonal do pano
            double diag = 0.0;
            for (const Vec3 &p : panel.polygon) {
                diag = std::max(diag, length(p - panel.polygon[0]));
            }
            const double arrowLength = std::max(0.25, 0.16 * diag);
            // setas distribuidas sobre o pano (interpolacao bilinear no quadrilatero)
            if (panel.polygon.size() == 4) {
                constexpr int nu = 5;
                constexpr int nv = 4;
                for (int i = 0; i < nu; ++i) {
                    for (int j = 0; j < nv; ++j) {
                        const double u = (i + 0.5) / nu;
                        const double v = (j + 0.5) / nv;
                        const Vec3 bottom = panel.polygon[0] * (1 - u) + panel.polygon[1] * u;
                        const Vec3 top = panel.polygon[3] * (1 - u) + panel.polygon[2] * u;
                        const Vec3 point = bottom * (1 - v) + top * v;
                        // a seta chega ao pano: cauda afastada contra o sentido da pressao
                        addArrow(arriving ? point - direction * arrowLength : point, direction, arrowLength, side, outline);
                    }
                }
            }
            // acao nas barras: setas mais fortes ao longo de cada barra
            for (const auto &bar : panel.bars) {
                constexpr int n = 7;
                for (int k = 0; k < n; ++k) {
                    const double t = (k + 0.5) / n;
                    const Vec3 point = bar.first * (1 - t) + bar.second * t;
                    addArrow(arriving ? point - direction * (arrowLength * 0.8) : point, direction, arrowLength * 0.8, side, barArrow);
                }
                addLine(bar.first, bar.second, barArrow);
            }
        }
    }

    m_surfaceData->SetPoints(surfacePoints);
    m_surfaceData->SetPolys(surfaceCells);
    m_surfaceData->GetCellData()->SetScalars(surfaceColors);
    m_surfaceData->Modified();
    m_lineData->SetPoints(linePoints);
    m_lineData->SetLines(lineCells);
    m_lineData->GetCellData()->SetScalars(lineColors);
    m_lineData->Modified();
    const bool any = visible && surfaceCells->GetNumberOfCells() > 0;
    m_surfaceActor->SetVisibility(any);
    m_lineActor->SetVisibility(any);
}

} // namespace Structura::Visualization
