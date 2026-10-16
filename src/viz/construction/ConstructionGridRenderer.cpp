#include "viz/construction/ConstructionGridRenderer.h"

#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkUnsignedCharArray.h>

#include <algorithm>
#include <cmath>

namespace Structura::Construction {

namespace {

constexpr unsigned char kGridColor[3] = {140, 153, 173};

double snapFloor(double value, double spacing)
{
    const double nearest = std::round(value / spacing) * spacing;
    if (std::abs(value - nearest) <= 1e-9) {
        return nearest;
    }
    return std::floor(value / spacing) * spacing;
}

double snapCeil(double value, double spacing)
{
    const double nearest = std::round(value / spacing) * spacing;
    if (std::abs(value - nearest) <= 1e-9) {
        return nearest;
    }
    return std::ceil(value / spacing) * spacing;
}

} // namespace

ConstructionGridRenderer::ConstructionGridRenderer()
    : m_data(vtkSmartPointer<vtkPolyData>::New())
    , m_mapper(vtkSmartPointer<vtkPolyDataMapper>::New())
    , m_actor(vtkSmartPointer<vtkActor>::New())
{
    m_mapper->SetInputData(m_data);
    m_mapper->ScalarVisibilityOn();
    m_mapper->SetColorModeToDirectScalars();
    m_mapper->SetScalarModeToUsePointData();
    m_actor->SetMapper(m_mapper);
    if (auto *property = m_actor->GetProperty()) {
        property->SetLineWidth(1.0);
        property->LightingOff();
    }
    m_actor->PickableOff();
}

ConstructionGridRenderer::~ConstructionGridRenderer() = default;

void ConstructionGridRenderer::setVisible(bool visible)
{
    m_actor->SetVisibility(visible ? 1 : 0);
}

void ConstructionGridRenderer::update(const Window &footprint, PlaneMode mode, double offset, double spacing)
{
    if (spacing <= 0.0) {
        spacing = 1.0;
    }
    if (m_built && footprint == m_footprint && mode == m_mode
        && std::abs(offset - m_offset) < 1e-9 && std::abs(spacing - m_spacing) < 1e-12) {
        return;
    }
    build(footprint, mode, offset, spacing);
}

void ConstructionGridRenderer::build(const Window &footprint, PlaneMode mode, double offset, double spacing)
{
    m_footprint = footprint;
    m_mode = mode;
    m_offset = offset;
    m_spacing = spacing;
    m_built = true;

    const double spanU = footprint.maxU - footprint.minU;
    const double spanV = footprint.maxV - footprint.minV;
    const double extent = std::max(spanU, spanV) + 2.0 * kMarginCells * spacing;
    const double cellsAcross = extent / spacing;
    double effective = spacing;
    if (cellsAcross > kMaxCellsPerSide) {
        effective = spacing * std::ceil(cellsAcross / kMaxCellsPerSide);
    }
    m_effectiveSpacing = effective;
    const double margin = kMarginCells * effective;

    m_bounds = {
        snapFloor(footprint.minU - margin, effective),
        snapCeil(footprint.maxU + margin, effective),
        snapFloor(footprint.minV - margin, effective),
        snapCeil(footprint.maxV + margin, effective),
    };

    const int cellsU = std::max(1, static_cast<int>(std::lround((m_bounds.maxU - m_bounds.minU) / effective)));
    const int cellsV = std::max(1, static_cast<int>(std::lround((m_bounds.maxV - m_bounds.minV) / effective)));
    const long long indexMinU = std::llround(m_bounds.minU / effective);
    const long long indexMinV = std::llround(m_bounds.minV / effective);

    const PlaneAxes axes = planeAxes(mode);
    const int pointsU = cellsU + 1;
    const int pointsV = cellsV + 1;

    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetDataTypeToDouble();
    points->SetNumberOfPoints(static_cast<vtkIdType>(pointsU) * pointsV);

    auto colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    colors->SetNumberOfComponents(4);
    colors->SetName("GridRGBA");
    colors->SetNumberOfTuples(static_cast<vtkIdType>(pointsU) * pointsV);

    for (int j = 0; j < pointsV; ++j) {
        const double v = static_cast<double>(indexMinV + j) * effective;
        const double distanceV = std::max({footprint.minV - v, 0.0, v - footprint.maxV});
        for (int i = 0; i < pointsU; ++i) {
            const double u = static_cast<double>(indexMinU + i) * effective;
            const double distanceU = std::max({footprint.minU - u, 0.0, u - footprint.maxU});
            const double opacity = std::clamp(1.0 - std::max(distanceU, distanceV) / margin, 0.0, 1.0);

            double xyz[3];
            xyz[axes.u] = u;
            xyz[axes.v] = v;
            xyz[axes.normal] = offset;
            const vtkIdType id = static_cast<vtkIdType>(j) * pointsU + i;
            points->SetPoint(id, xyz);
            colors->SetTuple4(id, kGridColor[0], kGridColor[1], kGridColor[2], std::round(opacity * 255.0));
        }
    }

    auto lines = vtkSmartPointer<vtkCellArray>::New();
    for (int j = 0; j < pointsV; ++j) {
        lines->InsertNextCell(pointsU);
        for (int i = 0; i < pointsU; ++i) {
            lines->InsertCellPoint(static_cast<vtkIdType>(j) * pointsU + i);
        }
    }
    for (int i = 0; i < pointsU; ++i) {
        lines->InsertNextCell(pointsV);
        for (int j = 0; j < pointsV; ++j) {
            lines->InsertCellPoint(static_cast<vtkIdType>(j) * pointsU + i);
        }
    }

    m_data->Initialize();
    m_data->SetPoints(points);
    m_data->SetLines(lines);
    m_data->GetPointData()->SetScalars(colors);
    m_data->Modified();
}

} // namespace Structura::Construction
