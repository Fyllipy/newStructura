#include "viz/construction/ConstructionScene.h"

#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkUnsignedCharArray.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Structura::Construction {

namespace {

constexpr unsigned char kGuideColor[3] = {140, 153, 173};

vtkSmartPointer<vtkActor> makeRgbaLineActor(vtkPolyData *data)
{
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(data);
    mapper->ScalarVisibilityOn();
    mapper->SetColorModeToDirectScalars();
    mapper->SetScalarModeToUsePointData();
    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetLineWidth(1.0);
    actor->GetProperty()->LightingOff();
    actor->PickableOff();
    actor->SetVisibility(0);
    return actor;
}

void appendFadedPoint(vtkPoints *points, vtkUnsignedCharArray *colors, const Vec3 &p, double fade)
{
    points->InsertNextPoint(p[0], p[1], p[2]);
    colors->InsertNextTuple4(kGuideColor[0], kGuideColor[1], kGuideColor[2], std::round(std::clamp(fade, 0.0, 1.0) * 255.0));
}

} // namespace

ConstructionScene::ConstructionScene()
    : m_ghostData(vtkSmartPointer<vtkPolyData>::New())
    , m_ghostActor(vtkSmartPointer<vtkActor>::New())
    , m_previewData(vtkSmartPointer<vtkPolyData>::New())
    , m_previewActor(vtkSmartPointer<vtkActor>::New())
    , m_perpendicularData(vtkSmartPointer<vtkPolyData>::New())
    , m_orthogonalData(vtkSmartPointer<vtkPolyData>::New())
{
    auto previewMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    previewMapper->SetInputData(m_previewData);
    previewMapper->ScalarVisibilityOff();
    m_previewActor->SetMapper(previewMapper);
    if (auto *property = m_previewActor->GetProperty()) {
        property->SetColor(0x09 / 255.0, 0x69 / 255.0, 0xda / 255.0);
        property->SetLineWidth(3.0);
        property->LightingOff();
        property->RenderLinesAsTubesOn();
    }
    m_previewActor->PickableOff();
    m_previewActor->SetVisibility(0);

    {
        auto ghostMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        ghostMapper->SetInputData(m_ghostData);
        ghostMapper->ScalarVisibilityOff();
        m_ghostActor->SetMapper(ghostMapper);
        m_ghostActor->GetProperty()->SetColor(0x09 / 255.0, 0x69 / 255.0, 0xda / 255.0);
        m_ghostActor->GetProperty()->SetLineWidth(2.5);
        m_ghostActor->GetProperty()->SetOpacity(0.6);
        m_ghostActor->GetProperty()->LightingOff();
        m_ghostActor->PickableOff();
        m_ghostActor->SetVisibility(0);
    }
    m_perpendicularActor = makeRgbaLineActor(m_perpendicularData);
    m_orthogonalActor = makeRgbaLineActor(m_orthogonalData);
}

ConstructionScene::~ConstructionScene()
{
    if (m_renderer) {
        m_renderer->RemoveActor(m_grid.actor());
        m_renderer->RemoveActor(m_axes_renderer.actor());
        m_renderer->RemoveActor(m_previewActor);
        m_renderer->RemoveActor(m_ghostActor);
        for (auto &layer : m_layers) {
            m_renderer->RemoveActor(layer->grid.actor());
            m_renderer->RemoveActor(layer->axes.actor());
        }
        m_renderer->RemoveActor(m_perpendicularActor);
        m_renderer->RemoveActor(m_orthogonalActor);
    }
}

void ConstructionScene::attach(vtkRenderer *renderer, std::function<void()> requestRender)
{
    m_renderer = renderer;
    m_requestRender = std::move(requestRender);
    if (!m_renderer) {
        return;
    }
    m_renderer->AddActor(m_grid.actor());
    m_renderer->AddActor(m_axes_renderer.actor());
    m_renderer->AddActor(m_previewActor);
    m_renderer->AddActor(m_ghostActor);
    m_renderer->AddActor(m_perpendicularActor);
    m_renderer->AddActor(m_orthogonalActor);
    rebuild();
}

void ConstructionScene::render() const
{
    if (m_requestRender) {
        m_requestRender();
    }
}

void ConstructionScene::setSnapGeometry(std::vector<Vec3> nodes, std::vector<Vec3> barMidpoints, std::vector<std::pair<Vec3, Vec3>> barSegments)
{
    m_nodes = std::move(nodes);
    m_barMidpoints = std::move(barMidpoints);
    m_barSegments = std::move(barSegments);
    rebuild();
}

void ConstructionScene::setAxes(const ReferenceAxes &axes)
{
    m_axes = axes;
    // O nivel acompanha o eixo mais proximo quando os eixos normais mudam.
    const auto available = levels();
    double best = available.front();
    for (const double candidate : available) {
        if (std::abs(candidate - m_level) < std::abs(best - m_level)) {
            best = candidate;
        }
    }
    m_level = best;
    rebuild();
    if (onPlaneChanged) {
        onPlaneChanged();
    }
}

std::vector<double> ConstructionScene::levels() const
{
    const auto available = m_axes.planeLevels(planeAxes(m_mode).normal);
    return std::vector<double>(available.begin(), available.end());
}

void ConstructionScene::setPlaneMode(PlaneMode mode)
{
    m_mode = mode;
    // Ao trocar de plano, volta ao nivel mais proximo de zero (como no opensa).
    const auto available = levels();
    double best = available.front();
    for (const double candidate : available) {
        if (std::abs(candidate) < std::abs(best)) {
            best = candidate;
        }
    }
    m_level = best;
    rebuild();
    if (onPlaneChanged) {
        onPlaneChanged();
    }
}

void ConstructionScene::stepLevel(int step)
{
    const auto available = levels();
    int current = 0;
    double bestDistance = std::numeric_limits<double>::max();
    for (int i = 0; i < static_cast<int>(available.size()); ++i) {
        const double distance = std::abs(available[static_cast<std::size_t>(i)] - m_level);
        if (distance < bestDistance) {
            bestDistance = distance;
            current = i;
        }
    }
    const int target = std::clamp(current + step, 0, static_cast<int>(available.size()) - 1);
    const double next = available[static_cast<std::size_t>(target)];
    if (std::abs(next - m_level) < 1e-12) {
        return;
    }
    m_level = next;
    rebuild();
    if (onPlaneChanged) {
        onPlaneChanged();
    }
}

void ConstructionScene::setLevel(double level)
{
    m_level = level;
    rebuild();
    if (onPlaneChanged) {
        onPlaneChanged();
    }
}

void ConstructionScene::setSpacing(double spacing)
{
    if (spacing <= 0.0 || std::abs(spacing - m_spacing) < 1e-12) {
        return;
    }
    m_spacing = spacing;
    rebuild();
}

void ConstructionScene::setGridVisible(bool visible)
{
    m_gridVisible = visible;
    m_grid.setVisible(visible);
    render();
}

void ConstructionScene::setReferenceAxesVisible(bool visible)
{
    m_axesVisible = visible;
    m_axes_renderer.setVisible(visible);
    if (onLabelsChanged) {
        onLabelsChanged();
    }
    render();
}

Window ConstructionScene::footprint() const
{
    const PlaneAxes planar = planeAxes(m_mode);
    bool any = false;
    Window w {0.0, 0.0, 0.0, 0.0};
    auto include = [&](double u, double v) {
        if (!any) {
            w = {u, u, v, v};
            any = true;
            return;
        }
        w.minU = std::min(w.minU, u);
        w.maxU = std::max(w.maxU, u);
        w.minV = std::min(w.minV, v);
        w.maxV = std::max(w.maxV, v);
    };
    for (const auto &node : m_nodes) {
        include(node[static_cast<std::size_t>(planar.u)], node[static_cast<std::size_t>(planar.v)]);
    }
    QVector<Structura::App::ProjectData::ReferenceAxis> uAxes;
    QVector<Structura::App::ProjectData::ReferenceAxis> vAxes;
    if (showAllLevels()) {
        uAxes = m_axes.unionAlong(0);
        vAxes = m_axes.unionAlong(1);
    } else {
        m_axes.planeAxes(planar.normal, m_level, uAxes, vAxes);
    }
    for (const auto &axis : uAxes) {
        include(axis.value, any ? w.minV : 0.0);
    }
    for (const auto &axis : vAxes) {
        include(any ? w.minU : 0.0, axis.value);
    }
    return w;
}

void ConstructionScene::rebuild()
{
    m_grid.update(footprint(), m_mode, m_level, m_spacing);
    m_axes_renderer.update(m_axes, m_grid.bounds(), m_mode, m_level);
    m_grid.setVisible(m_gridVisible);
    m_axes_renderer.setVisible(m_axesVisible);
    m_labels = m_axes_renderer.labels();

    // Demais niveis (malha e eixos de referencia): somente com "todos os niveis" ativo no plano XY.
    std::vector<double> others;
    if (showAllLevels()) {
        for (const double elevation : m_axes.planeLevels(2)) {
            if (std::abs(elevation - m_level) > 1e-9) {
                others.push_back(elevation);
            }
        }
    }
    while (m_layers.size() > others.size()) {
        if (m_renderer) {
            m_renderer->RemoveActor(m_layers.back()->grid.actor());
            m_renderer->RemoveActor(m_layers.back()->axes.actor());
        }
        m_layers.pop_back();
    }
    while (m_layers.size() < others.size()) {
        auto layer = std::make_unique<LevelLayer>();
        layer->grid.actor()->GetProperty()->SetOpacity(0.55);
        if (m_renderer) {
            m_renderer->AddActor(layer->grid.actor());
            m_renderer->AddActor(layer->axes.actor());
        }
        m_layers.push_back(std::move(layer));
    }
    const Window footprintWindow = footprint();
    for (std::size_t i = 0; i < others.size(); ++i) {
        LevelLayer &layer = *m_layers[i];
        layer.grid.update(footprintWindow, PlaneMode::XY, others[i], m_spacing);
        layer.axes.update(m_axes, layer.grid.bounds(), PlaneMode::XY, others[i]);
        layer.grid.setVisible(m_gridVisible);
        layer.axes.setVisible(m_axesVisible);
        const AxisLabels &extra = layer.axes.labels();
        m_labels.positions.insert(m_labels.positions.end(), extra.positions.begin(), extra.positions.end());
        m_labels.texts.insert(m_labels.texts.end(), extra.texts.begin(), extra.texts.end());
    }
    if (onLabelsChanged) {
        onLabelsChanged();
    }
    render();
}

void ConstructionScene::setShowAllLevels(bool show)
{
    if (m_showAllLevels == show) {
        return;
    }
    m_showAllLevels = show;
    rebuild();
}

void ConstructionScene::setGhostSegments(const std::vector<std::pair<Vec3, Vec3>> &segments)
{
    if (segments.empty()) {
        if (m_ghostActor->GetVisibility()) {
            m_ghostActor->SetVisibility(0);
        }
        return;
    }
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetDataTypeToDouble();
    auto lines = vtkSmartPointer<vtkCellArray>::New();
    for (const auto &segment : segments) {
        const vtkIdType a = points->InsertNextPoint(segment.first[0], segment.first[1], segment.first[2]);
        const vtkIdType b = points->InsertNextPoint(segment.second[0], segment.second[1], segment.second[2]);
        lines->InsertNextCell(2);
        lines->InsertCellPoint(a);
        lines->InsertCellPoint(b);
    }
    m_ghostData->Initialize();
    m_ghostData->SetPoints(points);
    m_ghostData->SetLines(lines);
    m_ghostData->Modified();
    m_ghostActor->SetVisibility(1);
}

void ConstructionScene::latticeValues(std::vector<double> &u, std::vector<double> &v) const
{
    latticeValues(u, v, m_level);
}

void ConstructionScene::latticeValues(std::vector<double> &u, std::vector<double> &v, double level) const
{
    u.clear();
    v.clear();
    const Window &w = m_grid.bounds();
    const double s = m_grid.effectiveSpacing();
    const PlaneAxes planar = planeAxes(m_mode);

    auto fill = [s](std::vector<double> &out, double lo, double hi,
                    const QVector<Structura::App::ProjectData::ReferenceAxis> &axes) {
        const long long first = std::llround(lo / s);
        const long long last = std::llround(hi / s);
        for (long long k = first; k <= last; ++k) {
            out.push_back(static_cast<double>(k) * s);
        }
        for (const auto &axis : axes) {
            out.push_back(axis.value);
        }
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end(),
                              [](double a, double b) { return std::abs(a - b) < 1e-9; }),
                  out.end());
    };
    QVector<Structura::App::ProjectData::ReferenceAxis> uAxes;
    QVector<Structura::App::ProjectData::ReferenceAxis> vAxes;
    m_axes.planeAxes(planar.normal, level, uAxes, vAxes);
    fill(u, w.minU, w.maxU, uAxes);
    fill(v, w.minV, w.maxV, vAxes);
}

void ConstructionScene::setPreviewLine(const std::optional<std::pair<Vec3, Vec3>> &line)
{
    if (!line || norm(line->second - line->first) <= 1e-12) {
        if (m_previewActor->GetVisibility()) {
            m_previewActor->SetVisibility(0);
        }
        return;
    }
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetDataTypeToDouble();
    points->InsertNextPoint(line->first[0], line->first[1], line->first[2]);
    points->InsertNextPoint(line->second[0], line->second[1], line->second[2]);
    auto cells = vtkSmartPointer<vtkCellArray>::New();
    cells->InsertNextCell(2);
    cells->InsertCellPoint(0);
    cells->InsertCellPoint(1);
    m_previewData->Initialize();
    m_previewData->SetPoints(points);
    m_previewData->SetLines(cells);
    m_previewData->Modified();
    m_previewActor->SetVisibility(1);
}

void ConstructionScene::updateGuideActor(vtkPolyData *data, const Vec3 &start, bool perpendicular)
{
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetDataTypeToDouble();
    auto colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    colors->SetNumberOfComponents(4);
    colors->SetName("GuideRGBA");
    auto cells = vtkSmartPointer<vtkCellArray>::New();

    const double length = kGuideHalfLength;
    const PlaneAxes planar = planeAxes(m_mode);

    if (perpendicular) {
        Vec3 direction {0.0, 0.0, 0.0};
        direction[static_cast<std::size_t>(planar.normal)] = 1.0;
        const double dash = std::max(length / 18.0, 0.15);
        const double gap = dash * 0.65;
        double cursor = -length;
        auto appendDash = [&](double a, double b) {
            const vtkIdType first = points->GetNumberOfPoints();
            appendFadedPoint(points, colors, start + direction * a, 1.0 - std::abs(a) / length);
            appendFadedPoint(points, colors, start + direction * b, 1.0 - std::abs(b) / length);
            cells->InsertNextCell(2);
            cells->InsertCellPoint(first);
            cells->InsertCellPoint(first + 1);
        };
        while (cursor < length - 1e-9) {
            appendDash(cursor, std::min(cursor + dash, length));
            cursor += dash + gap;
        }
        // Garante que a guia alcance exatamente o extremo positivo.
        appendDash(std::max(-length, length - dash), length);
    } else {
        for (const int axis : {planar.u, planar.v}) {
            Vec3 direction {0.0, 0.0, 0.0};
            direction[static_cast<std::size_t>(axis)] = 1.0;
            const vtkIdType first = points->GetNumberOfPoints();
            for (const double coordinate : {-length, 0.0, length}) {
                appendFadedPoint(points, colors, start + direction * coordinate, 1.0 - std::abs(coordinate) / length);
            }
            cells->InsertNextCell(3);
            cells->InsertCellPoint(first);
            cells->InsertCellPoint(first + 1);
            cells->InsertCellPoint(first + 2);
        }
    }

    data->Initialize();
    data->SetPoints(points);
    data->SetLines(cells);
    data->GetPointData()->SetScalars(colors);
    data->Modified();
}

void ConstructionScene::setGuides(const std::optional<Vec3> &start)
{
    if (!start || !m_snap.enabled) {
        m_perpendicularActor->SetVisibility(0);
        m_orthogonalActor->SetVisibility(0);
        return;
    }
    if (m_snap.perpendicular) {
        updateGuideActor(m_perpendicularData, *start, true);
        m_perpendicularActor->SetVisibility(1);
    } else {
        m_perpendicularActor->SetVisibility(0);
    }
    if (m_snap.orthogonal) {
        updateGuideActor(m_orthogonalData, *start, false);
        m_orthogonalActor->SetVisibility(1);
    } else {
        m_orthogonalActor->SetVisibility(0);
    }
}

void ConstructionScene::hidePlacementVisuals()
{
    m_ghostActor->SetVisibility(0);
    m_previewActor->SetVisibility(0);
    m_perpendicularActor->SetVisibility(0);
    m_orthogonalActor->SetVisibility(0);
}

} // namespace Structura::Construction
