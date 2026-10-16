#include "viz/construction/ReferenceAxesRenderer.h"

#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>

#include <algorithm>
#include <cmath>

namespace Structura::Construction {

namespace {

Vec3 worldPoint(double u, double v, PlaneMode mode, double offset)
{
    const PlaneAxes axes = planeAxes(mode);
    Vec3 p {};
    p[axes.u] = u;
    p[axes.v] = v;
    p[axes.normal] = offset;
    return p;
}

} // namespace

ReferenceAxesRenderer::ReferenceAxesRenderer()
    : m_data(vtkSmartPointer<vtkPolyData>::New())
    , m_mapper(vtkSmartPointer<vtkPolyDataMapper>::New())
    , m_actor(vtkSmartPointer<vtkActor>::New())
{
    m_mapper->SetInputData(m_data);
    m_mapper->ScalarVisibilityOff();
    m_actor->SetMapper(m_mapper);
    if (auto *property = m_actor->GetProperty()) {
        property->SetColor(0x57 / 255.0, 0x60 / 255.0, 0x6a / 255.0);
        property->SetLineWidth(1.5);
        property->LightingOff();
        property->RenderLinesAsTubesOn();
    }
    m_actor->PickableOff();
}

ReferenceAxesRenderer::~ReferenceAxesRenderer() = default;

void ReferenceAxesRenderer::setVisible(bool visible)
{
    m_actor->SetVisibility(visible ? 1 : 0);
}

std::vector<ReferenceAxesRenderer::LineSpec> ReferenceAxesRenderer::lineSpecs(const Axes &axes,
                                                                              const Window &bounds,
                                                                              PlaneMode mode,
                                                                              double offset) const
{
    const PlaneAxes planar = planeAxes(mode);
    QVector<Structura::App::ProjectData::ReferenceAxis> firstAxes;
    QVector<Structura::App::ProjectData::ReferenceAxis> secondAxes;
    axes.planeAxes(planar.normal, offset, firstAxes, secondAxes);

    auto extents = [](const QVector<Structura::App::ProjectData::ReferenceAxis> &list,
                      double minimum, double maximum) {
        if (list.isEmpty()) {
            return std::pair<double, double>(minimum, maximum);
        }
        double lo = list.first().value;
        double hi = list.first().value;
        for (const auto &axis : list) {
            lo = std::min(lo, axis.value);
            hi = std::max(hi, axis.value);
        }
        return std::pair<double, double>(lo - kExtension, hi + kExtension);
    };
    const auto [uStart, uEnd] = extents(firstAxes, bounds.minU, bounds.maxU);
    const auto [vStart, vEnd] = extents(secondAxes, bounds.minV, bounds.maxV);

    const double labelOffset = std::max(0.25, std::max(bounds.maxU - bounds.minU, bounds.maxV - bounds.minV) * 0.015);

    std::vector<LineSpec> specs;
    for (const auto &axis : firstAxes) {
        specs.push_back({axis.value, vStart, axis.value, vEnd, axis.label, labelOffset, 0.0});
    }
    for (const auto &axis : secondAxes) {
        specs.push_back({uStart, axis.value, uEnd, axis.value, axis.label, 0.0, labelOffset});
    }
    return specs;
}

void ReferenceAxesRenderer::update(const Axes &axes, const Window &gridBounds, PlaneMode mode, double offset)
{
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetDataTypeToDouble();
    auto lines = vtkSmartPointer<vtkCellArray>::New();
    m_labels = {};

    const auto specs = lineSpecs(axes, gridBounds, mode, offset);
    for (const auto &spec : specs) {
        const Vec3 start = worldPoint(spec.u0, spec.v0, mode, offset);
        const Vec3 end = worldPoint(spec.u1, spec.v1, mode, offset);
        const Vec3 direction = end - start;
        const double length = norm(direction);
        if (length > 0.0) {
            const Vec3 unit = direction * (1.0 / length);
            // Evita explosao de segmentos em linhas muito longas.
            const double pattern = kDashLength + kDotLength + 2.0 * kGapLength;
            const double scale = std::max(1.0, length / (pattern * 1500.0));
            const double dash = kDashLength * scale;
            const double dotLen = kDotLength * scale;
            const double gap = kGapLength * scale;

            auto appendSegment = [&](double from, double segment) {
                const double to = std::min(from + segment, length);
                if (to > from) {
                    const Vec3 a = start + unit * from;
                    const Vec3 b = start + unit * to;
                    const vtkIdType ia = points->InsertNextPoint(a[0], a[1], a[2]);
                    const vtkIdType ib = points->InsertNextPoint(b[0], b[1], b[2]);
                    lines->InsertNextCell(2);
                    lines->InsertCellPoint(ia);
                    lines->InsertCellPoint(ib);
                }
                return to;
            };

            double position = 0.0;
            while (position < length) {
                position = appendSegment(position, dash);
                position += gap;
                position = appendSegment(position, dotLen);
                position += gap;
            }
        }

        if (!spec.label.isEmpty()) {
            m_labels.positions.push_back(worldPoint(spec.u0 + spec.labelOffsetU, spec.v0 + spec.labelOffsetV, mode, offset));
            m_labels.positions.push_back(worldPoint(spec.u1 + spec.labelOffsetU, spec.v1 + spec.labelOffsetV, mode, offset));
            m_labels.texts.push_back(spec.label);
            m_labels.texts.push_back(spec.label);
        }
    }

    m_data->Initialize();
    m_data->SetPoints(points);
    m_data->SetLines(lines);
    m_data->Modified();
}

} // namespace Structura::Construction
