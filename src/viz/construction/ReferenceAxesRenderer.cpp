#include "viz/construction/ReferenceAxesRenderer.h"

#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkIdList.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkUnsignedCharArray.h>
#include <vtkVectorText.h>

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
    // cores por celula: eixos em cinza, bolhas (preenchimento branco, contorno e letras escuros)
    m_mapper->ScalarVisibilityOn();
    m_mapper->SetColorModeToDirectScalars();
    m_mapper->SetScalarModeToUseCellData();
    m_mapper->SetResolveCoincidentTopologyToPolygonOffset();
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

    // as celulas do VTK sao numeradas por tipo (linhas, depois poligonos): as cores seguem a mesma ordem
    auto lineColors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    lineColors->SetNumberOfComponents(3);
    auto polyColors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    polyColors->SetNumberOfComponents(3);
    auto polys = vtkSmartPointer<vtkCellArray>::New();
    const unsigned char axisColor[3] = {0x57, 0x60, 0x6a};
    const unsigned char inkColor[3] = {0x24, 0x29, 0x2f};
    const unsigned char whiteColor[3] = {255, 255, 255};

    // Bolha: disco branco + contorno + letras (vtkVectorText) no plano dos eixos (XY na planta), sempre planificada, nunca virada para a camera.
    auto addBubble = [&](const Vec3 &center, double radius, const PlaneAxes &planar, const QString &label) {
        auto place = [&](double x, double y, double lift) {
            Vec3 p = center;
            p[planar.u] += x;
            p[planar.v] += y;
            p[planar.normal] += lift;
            return points->InsertNextPoint(p[0], p[1], p[2]);
        };
        const double lift = 0.01 * radius;
        constexpr int kSegments = 48;
        constexpr double kPi = 3.14159265358979323846;
        // disco
        polys->InsertNextCell(kSegments);
        for (int i = 0; i < kSegments; ++i) {
            const double a = 2.0 * kPi * i / kSegments;
            polys->InsertCellPoint(place(radius * std::cos(a), radius * std::sin(a), lift));
        }
        polyColors->InsertNextTypedTuple(whiteColor);
        // contorno
        lines->InsertNextCell(kSegments + 1);
        vtkIdType first = 0;
        for (int i = 0; i <= kSegments; ++i) {
            const double a = 2.0 * kPi * (i % kSegments) / kSegments;
            const vtkIdType id = place(radius * std::cos(a), radius * std::sin(a), 2.0 * lift);
            if (i == 0) {
                first = id;
            }
            lines->InsertCellPoint(i == kSegments ? first : id);
        }
        lineColors->InsertNextTypedTuple(inkColor);
        // letras
        vtkNew<vtkVectorText> text;
        text->SetText(label.toLatin1().constData());
        text->Update();
        vtkPolyData *glyphs = text->GetOutput();
        double b[6];
        glyphs->GetBounds(b);
        const double textWidth = b[1] - b[0], textHeight = b[3] - b[2];
        if (textWidth <= 0.0 || textHeight <= 0.0) {
            return;
        }
        const double k = std::min(1.0 * radius / textHeight, 1.5 * radius / textWidth);
        const double cx = 0.5 * (b[0] + b[1]), cy = 0.5 * (b[2] + b[3]);
        vtkCellArray *glyphCells = glyphs->GetPolys();
        vtkNew<vtkIdList> ids;
        glyphCells->InitTraversal();
        std::vector<vtkIdType> mapped(static_cast<std::size_t>(glyphs->GetNumberOfPoints()), -1);
        while (glyphCells->GetNextCell(ids)) {
            polys->InsertNextCell(ids->GetNumberOfIds());
            for (vtkIdType j = 0; j < ids->GetNumberOfIds(); ++j) {
                const vtkIdType src = ids->GetId(j);
                if (mapped[static_cast<std::size_t>(src)] < 0) {
                    double q[3];
                    glyphs->GetPoint(src, q);
                    mapped[static_cast<std::size_t>(src)] = place((q[0] - cx) * k, (q[1] - cy) * k, 3.0 * lift);
                }
                polys->InsertCellPoint(mapped[static_cast<std::size_t>(src)]);
            }
            polyColors->InsertNextTypedTuple(inkColor);
        }
    };

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
                    lineColors->InsertNextTypedTuple(axisColor);
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

        if (!spec.label.isEmpty() && length > 0.0) {
            // bolha de cada ponta: circulo plano, tangente a ponta do eixo e alinhado com ele (como nas plantas de arquitetura)
            const Vec3 unit = direction * (1.0 / length);
            const PlaneAxes planar = planeAxes(mode);
            const double radius = std::max(0.3, 0.025 * std::max(gridBounds.maxU - gridBounds.minU, gridBounds.maxV - gridBounds.minV));
            for (const bool atEnd : {false, true}) {
                const Vec3 tip = atEnd ? end : start;
                const Vec3 outward = atEnd ? unit : unit * -1.0;
                const Vec3 center = tip + outward * radius;
                addBubble(center, radius, planar, spec.label);
            }
        }
    }

    m_data->Initialize();
    m_data->SetPoints(points);
    m_data->SetLines(lines);
    m_data->SetPolys(polys);
    auto colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    colors->SetNumberOfComponents(3);
    colors->SetName("AxisColors");
    for (vtkIdType i = 0; i < lineColors->GetNumberOfTuples(); ++i) {
        colors->InsertNextTypedTuple(lineColors->GetPointer(3 * i));
    }
    for (vtkIdType i = 0; i < polyColors->GetNumberOfTuples(); ++i) {
        colors->InsertNextTypedTuple(polyColors->GetPointer(3 * i));
    }
    m_data->GetCellData()->SetScalars(colors);
    m_data->Modified();
}

} // namespace Structura::Construction
