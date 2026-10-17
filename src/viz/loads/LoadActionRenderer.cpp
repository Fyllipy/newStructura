#include "viz/loads/LoadActionRenderer.h"

#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace Structura::Visualization {

namespace {

using Vec3 = LoadActionRenderer::Vec3;

constexpr double kPi = 3.14159265358979323846;
constexpr double kMaxHeight = 1.0;
constexpr double kMinHeight = 0.1;
constexpr double kLabelClearance = 0.12;
constexpr double kAxialTail = 1.0;
constexpr double kMomentRadius = 0.30;
constexpr double kMomentSpacing = 1.0;

// Cores das acoes (hex -> rgb)
constexpr Vec3 kGlobalColor {0xcf / 255.0, 0x22 / 255.0, 0x2e / 255.0};
constexpr Vec3 kLocalColor {0x09 / 255.0, 0x69 / 255.0, 0xda / 255.0};
constexpr Vec3 kSelfWeightColor {0x2d / 255.0, 0xa4 / 255.0, 0x4e / 255.0};

Vec3 add(const Vec3 &a, const Vec3 &b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 mul(const Vec3 &a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dotp(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
double len(const Vec3 &a) { return std::sqrt(dotp(a, a)); }
Vec3 unit(const Vec3 &a)
{
    const double l = len(a);
    return l > 0.0 ? mul(a, 1.0 / l) : a;
}
Vec3 crossp(const Vec3 &a, const Vec3 &b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double sign(double v) { return v > 0.0 ? 1.0 : (v < 0.0 ? -1.0 : 0.0); }

Vec3 perpendicularTo(const Vec3 &vector)
{
    Vec3 reference {0.0, 0.0, 1.0};
    if (std::abs(dotp(vector, reference)) > 0.9) {
        reference = {0.0, 1.0, 0.0};
    }
    return unit(crossp(vector, reference));
}

QString formatNumber(double value)
{
    double rounded = std::round(value * 1000.0) / 1000.0;
    if (std::abs(rounded) < 1e-9) {
        rounded = 0.0;
    }
    QString text = QString::number(rounded, 'f', 3);
    while (text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }
    return text.replace(QLatin1Char('.'), QLatin1Char(','));
}

double forceHeight(double value, double maximum)
{
    value = std::abs(value);
    if (value < 1e-12 || maximum <= 0.0) {
        return 0.0;
    }
    return std::max(kMinHeight, kMaxHeight * value / maximum);
}

struct Batches {
    // Faces por cor
    std::map<std::tuple<double, double, double>, std::pair<std::vector<Vec3>, std::vector<std::vector<int>>>> faces;
    // Linhas (segmentos / polilinhas) por (cor, espessura)
    struct LineBatch {
        std::vector<Vec3> points;
        std::vector<std::vector<int>> lines;
    };
    std::map<std::tuple<double, double, double, double>, LineBatch> lines;

    void addFace(const Vec3 &color, const std::vector<Vec3> &vertices)
    {
        auto &batch = faces[{color[0], color[1], color[2]}];
        std::vector<int> face;
        for (const Vec3 &v : vertices) {
            face.push_back(static_cast<int>(batch.first.size()));
            batch.first.push_back(v);
        }
        batch.second.push_back(std::move(face));
    }

    LineBatch &lineBatch(const Vec3 &color, double width)
    {
        return lines[{color[0], color[1], color[2], width}];
    }

    void addSegments(const Vec3 &color, double width, const std::vector<Vec3> &pts, const std::vector<std::pair<int, int>> &segments)
    {
        auto &batch = lineBatch(color, width);
        const int offset = static_cast<int>(batch.points.size());
        batch.points.insert(batch.points.end(), pts.begin(), pts.end());
        for (const auto &segment : segments) {
            batch.lines.push_back({offset + segment.first, offset + segment.second});
        }
    }

    void addPolyline(const Vec3 &color, double width, const std::vector<Vec3> &pts, const std::vector<int> &indices)
    {
        auto &batch = lineBatch(color, width);
        const int offset = static_cast<int>(batch.points.size());
        batch.points.insert(batch.points.end(), pts.begin(), pts.end());
        std::vector<int> line;
        for (const int index : indices) {
            line.push_back(offset + index);
        }
        batch.lines.push_back(std::move(line));
    }
};

/// Retorna (eixo, u, v) do plano cuja normal e o eixo local `direction`.
std::array<Vec3, 3> momentPlane(const std::array<Vec3, 3> &basis, int direction)
{
    const Vec3 &lx = basis[0];
    const Vec3 &ly = basis[1];
    const Vec3 &lz = basis[2];
    if (direction == 0) {
        return {lx, ly, lz};
    }
    if (direction == 1) {
        return {ly, lx, mul(lz, -1.0)};
    }
    return {lz, lx, ly};
}

void addMomentSymbol(Batches &batches, const Vec3 &color, const Vec3 &center, const Vec3 &axis,
                     const Vec3 &u, const Vec3 &v, double value, double radius)
{
    const double orientation = value > 0 ? -1.0 : 1.0;
    constexpr int kSteps = 25;
    std::vector<Vec3> points;
    std::vector<int> arc;
    double lastAngle = 0.0;
    for (int i = 0; i < kSteps; ++i) {
        const double angle = orientation * kPi * 1.55 * i / (kSteps - 1);
        lastAngle = angle;
        points.push_back(add(center, add(mul(u, radius * std::cos(angle)), mul(v, radius * std::sin(angle)))));
        arc.push_back(i);
    }
    Vec3 tangent = add(mul(u, -std::sin(lastAngle)), mul(v, std::cos(lastAngle)));
    tangent = mul(tangent, orientation);
    const Vec3 tip = points.back();
    const Vec3 back = sub(tip, mul(tangent, radius * 0.28));
    const Vec3 side = mul(unit(crossp(axis, tangent)), radius * 0.14);
    points.push_back(add(back, side));
    points.push_back(sub(back, side));
    points.push_back(tip);
    batches.addPolyline(color, 3.0, points, arc);
    batches.addSegments(color, 3.0, points, {{kSteps, kSteps + 2}, {kSteps + 1, kSteps + 2}});
}

} // namespace

LoadActionRenderer::~LoadActionRenderer()
{
    clear();
}

void LoadActionRenderer::clear()
{
    if (m_renderer) {
        for (const auto &actor : m_actors) {
            m_renderer->RemoveActor(actor);
        }
    }
    m_actors.clear();
    m_labels = {};
}

void LoadActionRenderer::render(const std::vector<NodeAction> &nodes,
                                const std::vector<MemberAction> &members,
                                const Visibility &visibility)
{
    clear();
    if (!m_renderer) {
        return;
    }

    Batches batches;
    const Vec3 momentColor = kLocalColor;
    const Vec3 nodeColor = kGlobalColor;

    // Intensidade maxima entre forcas distribuidas e nodais visiveis.
    double maximum = 0.0;
    if (visibility.memberForces) {
        for (const auto &action : members) {
            if (action.kind == MemberKind::Force) {
                for (const double component : action.values) {
                    maximum = std::max(maximum, std::abs(component));
                }
            }
        }
    }
    if (visibility.nodeForces) {
        for (const auto &node : nodes) {
            for (const double component : node.force) {
                maximum = std::max(maximum, std::abs(component));
            }
        }
    }

    // ── Forcas distribuidas ─────────────────────────────────────────────────
    if (visibility.memberForces && maximum > 0.0) {
        for (const auto &action : members) {
            if (action.kind != MemberKind::Force) {
                continue;
            }
            const Vec3 memberVector = sub(action.end, action.start);
            const double length = len(memberVector);
            if (length == 0.0) {
                continue;
            }
            const Vec3 tangent = mul(memberVector, 1.0 / length);
            for (int axis = 0; axis < 3; ++axis) {
                const double value = action.values[static_cast<std::size_t>(axis)];
                if (value == 0.0) {
                    continue;
                }
                Vec3 forceDirection {0.0, 0.0, 0.0};
                Vec3 color = kGlobalColor;
                if (action.selfWeight) {
                    forceDirection = {0.0, 0.0, 1.0};
                    color = kSelfWeightColor;
                } else if (action.local) {
                    forceDirection = action.basis[static_cast<std::size_t>(axis)];
                    color = kLocalColor;
                } else {
                    forceDirection[static_cast<std::size_t>(axis)] = 1.0;
                }

                const double valueSign = sign(value);
                if (std::abs(std::abs(dotp(tangent, forceDirection)) - 1.0) < 1e-9) {
                    // Carga axial: setas ao longo da barra, com uma cauda de 1 unidade.
                    const Vec3 loadDirection = mul(forceDirection, valueSign);
                    Vec3 lineStart = action.start;
                    Vec3 lineEnd = action.end;
                    Vec3 tailEnd;
                    if (dotp(memberVector, loadDirection) >= 0) {
                        lineStart = sub(lineStart, mul(loadDirection, kAxialTail));
                        tailEnd = lineStart;
                    } else {
                        lineEnd = sub(lineEnd, mul(loadDirection, kAxialTail));
                        tailEnd = lineEnd;
                    }
                    const Vec3 lineVector = sub(lineEnd, lineStart);
                    const double lineLength = len(lineVector);
                    const double chevron = std::min(0.14, length * 0.035);
                    const int count = std::max(2, std::min(12, static_cast<int>(lineLength / 0.60) + 1));
                    const Vec3 chevronNormal = perpendicularTo(tangent);
                    std::vector<Vec3> pts {lineStart, lineEnd};
                    std::vector<std::pair<int, int>> segments {{0, 1}};
                    for (int i = 0; i < count; ++i) {
                        const double fraction = 0.10 + 0.80 * i / (count - 1);
                        const Vec3 p = add(lineStart, mul(lineVector, fraction));
                        const Vec3 cd = mul(forceDirection, valueSign);
                        const Vec3 armA = add(sub(p, mul(cd, chevron)), mul(chevronNormal, chevron * 0.55));
                        const Vec3 armB = sub(sub(p, mul(cd, chevron)), mul(chevronNormal, chevron * 0.55));
                        const Vec3 tip = add(p, mul(cd, chevron * 0.75));
                        const int first = static_cast<int>(pts.size());
                        pts.push_back(armA);
                        pts.push_back(armB);
                        pts.push_back(tip);
                        segments.push_back({first, first + 2});
                        segments.push_back({first + 1, first + 2});
                    }
                    batches.addSegments(color, 2.0, pts, segments);
                    m_labels.positions.push_back(sub(tailEnd, mul(loadDirection, kLabelClearance)));
                    m_labels.texts.push_back(formatNumber(value) + QStringLiteral(" kN/m"));
                    m_labels.colors.push_back(color);
                    m_labels.weights.push_back(std::abs(value));
                    continue;
                }

                // Faixa no lado oposto ao sentido da acao (convencao grafica).
                const Vec3 offset = mul(forceDirection, -valueSign * forceHeight(value, maximum));
                const Vec3 topStart = add(action.start, offset);
                const Vec3 topEnd = add(action.end, offset);
                batches.addFace(color, {action.start, action.end, topEnd, topStart});
                batches.addSegments(color, 2.0, {action.start, action.end, topEnd, topStart},
                                    {{1, 2}, {2, 3}, {3, 0}});
                const double offsetLength = len(offset);
                const Vec3 outward = offsetLength > 0 ? mul(offset, 1.0 / offsetLength)
                                                      : mul(forceDirection, -valueSign);
                m_labels.positions.push_back(add(mul(add(topStart, topEnd), 0.5), mul(outward, kLabelClearance)));
                m_labels.texts.push_back(formatNumber(value) + QStringLiteral(" kN/m"));
                m_labels.colors.push_back(color);
                m_labels.weights.push_back(std::abs(value));
            }
        }
    }

    // ── Forcas nodais ───────────────────────────────────────────────────────
    if (visibility.nodeForces) {
        for (const auto &node : nodes) {
            for (int axis = 0; axis < 3; ++axis) {
                const double force = node.force[static_cast<std::size_t>(axis)];
                if (std::abs(force) < 1e-12) {
                    continue;
                }
                Vec3 direction {0.0, 0.0, 0.0};
                direction[static_cast<std::size_t>(axis)] = sign(force);
                const double arrowLength = forceHeight(force, maximum);
                const Vec3 lineStart = sub(node.position, mul(direction, arrowLength));
                const double chevron = arrowLength * 0.18;
                const Vec3 normal = perpendicularTo(direction);
                const Vec3 armA = add(sub(node.position, mul(direction, chevron)), mul(normal, chevron * 0.55));
                const Vec3 armB = sub(sub(node.position, mul(direction, chevron)), mul(normal, chevron * 0.55));
                batches.addSegments(nodeColor, 3.0, {lineStart, node.position, armA, armB}, {{0, 1}, {2, 1}, {3, 1}});
                m_labels.positions.push_back(sub(lineStart, mul(direction, kLabelClearance)));
                m_labels.texts.push_back(formatNumber(force) + QStringLiteral(" kN"));
                m_labels.colors.push_back(nodeColor);
                m_labels.weights.push_back(std::abs(force));
            }
        }
    }

    // ── Momentos distribuidos nas barras ────────────────────────────────────
    if (visibility.memberMoments) {
        const std::array<Vec3, 3> identity {Vec3 {1, 0, 0}, Vec3 {0, 1, 0}, Vec3 {0, 0, 1}};
        for (const auto &action : members) {
            if (action.kind != MemberKind::Moment) {
                continue;
            }
            const double memberLength = len(sub(action.end, action.start));
            for (int axis = 0; axis < 3; ++axis) {
                const double value = action.values[static_cast<std::size_t>(axis)];
                if (value == 0.0) {
                    continue;
                }
                const auto plane = momentPlane(action.local ? action.basis : identity, axis);
                const int count = std::max(2, std::min(8, static_cast<int>(memberLength / kMomentSpacing) + 1));
                std::vector<Vec3> centers;
                for (int i = 0; i < count; ++i) {
                    const double fraction = 0.15 + 0.70 * i / (count - 1);
                    centers.push_back(add(action.start, mul(sub(action.end, action.start), fraction)));
                }
                for (const Vec3 &center : centers) {
                    addMomentSymbol(batches, momentColor, center, plane[0], plane[1], plane[2], value, kMomentRadius);
                }
                m_labels.positions.push_back(add(centers[centers.size() / 2], mul(plane[1], kMomentRadius * 1.35)));
                m_labels.texts.push_back(formatNumber(value) + QStringLiteral(" kNm/m"));
                m_labels.colors.push_back(momentColor);
                m_labels.weights.push_back(std::abs(value));
            }
        }
    }

    // ── Momentos nodais ─────────────────────────────────────────────────────
    if (visibility.nodeMoments) {
        const std::array<Vec3, 3> identity {Vec3 {1, 0, 0}, Vec3 {0, 1, 0}, Vec3 {0, 0, 1}};
        for (const auto &node : nodes) {
            for (int axis = 0; axis < 3; ++axis) {
                const double value = node.moment[static_cast<std::size_t>(axis)];
                if (std::abs(value) < 1e-12) {
                    continue;
                }
                const auto plane = momentPlane(identity, axis);
                addMomentSymbol(batches, nodeColor, node.position, plane[0], plane[1], plane[2], value, kMomentRadius);
                m_labels.positions.push_back(add(node.position, mul(plane[1], kMomentRadius * 1.35)));
                m_labels.texts.push_back(formatNumber(value) + QStringLiteral(" kNm"));
                m_labels.colors.push_back(nodeColor);
                m_labels.weights.push_back(std::abs(value));
            }
        }
    }

    // ── Atores ──────────────────────────────────────────────────────────────
    for (const auto &[colorKey, batch] : batches.faces) {
        if (batch.first.empty()) {
            continue;
        }
        auto points = vtkSmartPointer<vtkPoints>::New();
        points->SetDataTypeToDouble();
        for (const Vec3 &p : batch.first) {
            points->InsertNextPoint(p[0], p[1], p[2]);
        }
        auto polys = vtkSmartPointer<vtkCellArray>::New();
        for (const auto &face : batch.second) {
            polys->InsertNextCell(static_cast<vtkIdType>(face.size()));
            for (const int index : face) {
                polys->InsertCellPoint(index);
            }
        }
        auto data = vtkSmartPointer<vtkPolyData>::New();
        data->SetPoints(points);
        data->SetPolys(polys);
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputData(data);
        auto actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(std::get<0>(colorKey), std::get<1>(colorKey), std::get<2>(colorKey));
        actor->GetProperty()->SetOpacity(0.5);
        actor->GetProperty()->LightingOff();
        actor->PickableOff();
        m_renderer->AddActor(actor);
        m_actors.push_back(actor);
    }
    for (const auto &[key, batch] : batches.lines) {
        if (batch.points.empty()) {
            continue;
        }
        auto points = vtkSmartPointer<vtkPoints>::New();
        points->SetDataTypeToDouble();
        for (const Vec3 &p : batch.points) {
            points->InsertNextPoint(p[0], p[1], p[2]);
        }
        auto lines = vtkSmartPointer<vtkCellArray>::New();
        for (const auto &line : batch.lines) {
            lines->InsertNextCell(static_cast<vtkIdType>(line.size()));
            for (const int index : line) {
                lines->InsertCellPoint(index);
            }
        }
        auto data = vtkSmartPointer<vtkPolyData>::New();
        data->SetPoints(points);
        data->SetLines(lines);
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputData(data);
        auto actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(std::get<0>(key), std::get<1>(key), std::get<2>(key));
        actor->GetProperty()->SetLineWidth(std::get<3>(key));
        actor->GetProperty()->LightingOff();
        actor->GetProperty()->RenderLinesAsTubesOn();
        actor->PickableOff();
        m_renderer->AddActor(actor);
        m_actors.push_back(actor);
    }
}

} // namespace Structura::Visualization
