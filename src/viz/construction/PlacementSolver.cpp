#include "viz/construction/PlacementSolver.h"

#include "viz/construction/ConstructionScene.h"

#include <vtkCamera.h>
#include <vtkMatrix4x4.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Structura::Construction {

struct PlacementSolver::Projection {
    std::array<double, 16> matrix {};
    double width {1.0};
    double height {1.0};

    [[nodiscard]] std::optional<QPointF> project(const Vec3 &p) const
    {
        const double x = matrix[0] * p[0] + matrix[1] * p[1] + matrix[2] * p[2] + matrix[3];
        const double y = matrix[4] * p[0] + matrix[5] * p[1] + matrix[6] * p[2] + matrix[7];
        const double z = matrix[8] * p[0] + matrix[9] * p[1] + matrix[10] * p[2] + matrix[11];
        const double w = matrix[12] * p[0] + matrix[13] * p[1] + matrix[14] * p[2] + matrix[15];
        if (w <= 1e-12) {
            return std::nullopt;
        }
        const double nx = x / w;
        const double ny = y / w;
        const double nz = z / w;
        if (std::abs(nz) > 1.0 || std::abs(nx) > 1.0 || std::abs(ny) > 1.0) {
            return std::nullopt;
        }
        return QPointF((nx + 1.0) * width / 2.0, (1.0 - ny) * height / 2.0);
    }
};

PlacementSolver::PlacementSolver(vtkRenderer *renderer, vtkRenderWindow *window, const ConstructionScene *scene)
    : m_renderer(renderer)
    , m_window(window)
    , m_scene(scene)
{
}

PlacementSolver::Projection PlacementSolver::makeProjection() const
{
    Projection projection;
    projection.width = std::max(1.0, m_widgetSize.width());
    projection.height = std::max(1.0, m_widgetSize.height());
    if (!m_renderer) {
        return projection;
    }
    vtkCamera *camera = m_renderer->GetActiveCamera();
    vtkMatrix4x4 *matrix = camera->GetCompositeProjectionTransformMatrix(m_renderer->GetTiledAspectRatio(), -1.0, 1.0);
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            projection.matrix[static_cast<std::size_t>(row * 4 + column)] = matrix->GetElement(row, column);
        }
    }
    return projection;
}

bool PlacementSolver::worldToWidget(const Vec3 &world, QPointF &screen) const
{
    const auto projected = makeProjection().project(world);
    if (!projected) {
        return false;
    }
    screen = *projected;
    return true;
}

bool PlacementSolver::cursorOnPlane(const QPointF &pointer, Vec3 &out) const
{
    if (!m_renderer || !m_window) {
        return false;
    }
    const int *renderSize = m_renderer->GetSize();
    if (!renderSize || renderSize[0] <= 0 || renderSize[1] <= 0) {
        return false;
    }
    const double widgetW = std::max(1.0, m_widgetSize.width());
    const double widgetH = std::max(1.0, m_widgetSize.height());
    const double scaleX = static_cast<double>(renderSize[0]) / widgetW;
    const double scaleY = static_cast<double>(renderSize[1]) / widgetH;
    const double displayX = pointer.x() * scaleX;
    const double displayY = (widgetH - pointer.y()) * scaleY;

    auto unproject = [&](double depth, Vec3 &world) {
        m_renderer->SetDisplayPoint(displayX, displayY, depth);
        m_renderer->DisplayToWorld();
        double p[4];
        m_renderer->GetWorldPoint(p);
        if (std::abs(p[3]) <= 1e-12) {
            return false;
        }
        world = {p[0] / p[3], p[1] / p[3], p[2] / p[3]};
        return true;
    };

    Vec3 nearPoint {};
    Vec3 farPoint {};
    if (!unproject(0.0, nearPoint) || !unproject(1.0, farPoint)) {
        return false;
    }
    const Vec3 direction = farPoint - nearPoint;
    const PlaneAxes axes = planeAxes(m_scene->planeMode());
    const double denominator = direction[static_cast<std::size_t>(axes.normal)];
    if (std::abs(denominator) <= 1e-12) {
        return false;
    }
    auto intersect = [&](double level, Vec3 &hit, double &t) {
        t = (level - nearPoint[static_cast<std::size_t>(axes.normal)]) / denominator;
        if (!std::isfinite(t) || t < 0.0) {
            return false;
        }
        hit = nearPoint + direction * t;
        hit[static_cast<std::size_t>(axes.normal)] = level;
        return true;
    };

    // Com todos os niveis visiveis, vale o plano mais proximo da camera cujo ponto cai na malha.
    if (m_scene->showAllLevels()) {
        const Window &window = m_scene->gridWindow();
        bool found = false;
        double bestT = 0.0;
        for (const double level : m_scene->planeLevelsList()) {
            Vec3 hit {};
            double t = 0.0;
            if (!intersect(level, hit, t)) {
                continue;
            }
            const double hu = hit[static_cast<std::size_t>(axes.u)];
            const double hv = hit[static_cast<std::size_t>(axes.v)];
            if (hu < window.minU || hu > window.maxU || hv < window.minV || hv > window.maxV) {
                continue;
            }
            if (!found || t < bestT) {
                found = true;
                bestT = t;
                out = hit;
                m_hitLevel = level;
            }
        }
        if (found) {
            return true;
        }
    }

    double t = 0.0;
    if (!intersect(m_scene->level(), out, t)) {
        return false;
    }
    m_hitLevel = m_scene->level();
    return true;
}

std::optional<PlacementTarget> PlacementSolver::snapPosition(const QPointF &pointer,
                                                             const Vec3 &cursor,
                                                             const Projection &projection) const
{
    const SnapSettings &settings = m_scene->snap();
    if (!settings.enabled) {
        return std::nullopt;
    }

    struct Candidate {
        double distance;
        int priority;
        SnapKind kind;
        Vec3 point;
    };
    std::vector<Candidate> candidates;

    auto screenDistance = [&](const QPointF &screen) {
        const double dx = screen.x() - pointer.x();
        const double dy = screen.y() - pointer.y();
        return std::sqrt(dx * dx + dy * dy);
    };

    // A malha so exerce atracao enquanto esta visivel, como no opensa.
    if (settings.grid && m_scene->gridVisible()) {
        std::vector<double> uValues;
        std::vector<double> vValues;
        m_scene->latticeValues(uValues, vValues, m_hitLevel);
        if (!uValues.empty() && !vValues.empty()) {
            const PlaneAxes axes = planeAxes(m_scene->planeMode());
            auto nearestTwo = [](const std::vector<double> &values, double target) {
                std::array<double, 2> best {values.front(), values.front()};
                std::array<double, 2> bestDistance {std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
                for (const double value : values) {
                    const double d = std::abs(value - target);
                    if (d < bestDistance[0]) {
                        bestDistance[1] = bestDistance[0];
                        best[1] = best[0];
                        bestDistance[0] = d;
                        best[0] = value;
                    } else if (d < bestDistance[1]) {
                        bestDistance[1] = d;
                        best[1] = value;
                    }
                }
                return best;
            };
            const auto uCandidates = nearestTwo(uValues, cursor[static_cast<std::size_t>(axes.u)]);
            const auto vCandidates = nearestTwo(vValues, cursor[static_cast<std::size_t>(axes.v)]);
            std::optional<Candidate> best;
            for (const double uValue : uCandidates) {
                for (const double vValue : vCandidates) {
                    Vec3 point = cursor;
                    point[static_cast<std::size_t>(axes.u)] = uValue;
                    point[static_cast<std::size_t>(axes.v)] = vValue;
                    const auto screen = projection.project(point);
                    if (!screen) {
                        continue;
                    }
                    const double distance = screenDistance(*screen);
                    if (!best || distance < best->distance) {
                        best = Candidate {distance, 2, SnapKind::Grid, point};
                    }
                }
            }
            if (best) {
                candidates.push_back(*best);
            }
        }
    }

    if (settings.endpoint) {
        for (const Vec3 &node : m_scene->nodePositions()) {
            if (const auto screen = projection.project(node)) {
                candidates.push_back({screenDistance(*screen), 0, SnapKind::Endpoint, node});
            }
        }
    }
    if (settings.center) {
        for (const Vec3 &mid : m_scene->barMidpoints()) {
            if (const auto screen = projection.project(mid)) {
                candidates.push_back({screenDistance(*screen), 1, SnapKind::Center, mid});
            }
        }
    }

    if (m_barSnap) {
        // Ao longo da barra: o ponto mais proximo do cursor na tela. Leva uma pequena penalidade para que
        // nos, centros e pontos da malha proximos continuem prevalecendo.
        constexpr double kBarBias = 6.0;
        for (const auto &segment : m_scene->barSegments()) {
            const auto sa = projection.project(segment.first);
            const auto sb = projection.project(segment.second);
            if (!sa || !sb) {
                continue;
            }
            const double ex = sb->x() - sa->x(), ey = sb->y() - sa->y();
            const double len2 = ex * ex + ey * ey;
            if (len2 < 1e-9) {
                continue;
            }
            double t = ((pointer.x() - sa->x()) * ex + (pointer.y() - sa->y()) * ey) / len2;
            t = std::clamp(t, 0.0, 1.0);
            const double cx = sa->x() + t * ex - pointer.x(), cy = sa->y() + t * ey - pointer.y();
            if (std::sqrt(cx * cx + cy * cy) > kSnapTolerancePixels * 2.0) {
                continue;
            }
            // refina em 3D (a perspectiva distorce o parametro da projecao)
            auto at = [&](double u) {
                return Vec3 {segment.first[0] + u * (segment.second[0] - segment.first[0]),
                             segment.first[1] + u * (segment.second[1] - segment.first[1]),
                             segment.first[2] + u * (segment.second[2] - segment.first[2])};
            };
            auto cost = [&](double u) {
                const auto screen = projection.project(at(u));
                return screen ? screenDistance(*screen) : 1e9;
            };
            double lo = std::max(0.0, t - 0.25), hi = std::min(1.0, t + 0.25);
            for (int i = 0; i < 24; ++i) {
                const double m1 = lo + (hi - lo) / 3.0, m2 = hi - (hi - lo) / 3.0;
                (cost(m1) < cost(m2)) ? hi = m2 : lo = m1;
            }
            const double u = 0.5 * (lo + hi);
            candidates.push_back({cost(u) + kBarBias, 2, SnapKind::OnBar, at(u)});
        }
    }

    const Candidate *chosen = nullptr;
    for (const Candidate &candidate : candidates) {
        if (candidate.distance > kSnapTolerancePixels) {
            continue;
        }
        if (!chosen || candidate.distance < chosen->distance
            || (candidate.distance == chosen->distance && candidate.priority < chosen->priority)) {
            chosen = &candidate;
        }
    }
    if (!chosen) {
        return std::nullopt;
    }
    return PlacementTarget {chosen->point, chosen->kind};
}

std::optional<std::pair<Vec3, double>> PlacementSolver::lineSnapCandidate(const Vec3 &direction,
                                                                           const Vec3 &start,
                                                                           const QPointF &pointer,
                                                                           const Projection &projection) const
{
    if (!m_scene->snap().enabled) {
        return std::nullopt;
    }
    const auto screenStart = projection.project(start);
    if (!screenStart) {
        return std::nullopt;
    }
    const double guideLength = ConstructionScene::kGuideHalfLength;
    const double probeLength = std::max(guideLength * 0.25, 0.5);
    for (const double sign : {1.0, -1.0}) {
        const Vec3 probeEnd = start + direction * (probeLength * sign);
        const auto screenEnd = projection.project(probeEnd);
        if (!screenEnd) {
            continue;
        }
        const double sx = screenEnd->x() - screenStart->x();
        const double sy = screenEnd->y() - screenStart->y();
        const double lengthSquared = sx * sx + sy * sy;
        if (lengthSquared <= 1e-12) {
            continue;
        }
        const double fraction = ((pointer.x() - screenStart->x()) * sx + (pointer.y() - screenStart->y()) * sy) / lengthSquared;
        const double distance = probeLength * sign * fraction;
        if (std::abs(distance) > kGuideSnapHalfLength) {
            continue;
        }
        const double closestX = screenStart->x() + fraction * sx;
        const double closestY = screenStart->y() + fraction * sy;
        const double screenDist = std::hypot(pointer.x() - closestX, pointer.y() - closestY);
        if (screenDist <= kSnapTolerancePixels) {
            return std::make_pair(start + direction * distance, screenDist);
        }
    }
    return std::nullopt;
}

std::optional<PlacementTarget> PlacementSolver::guideTarget(const Vec3 &start,
                                                            const QPointF &pointer,
                                                            const Projection &projection) const
{
    const SnapSettings &settings = m_scene->snap();
    const PlaneAxes axes = planeAxes(m_scene->planeMode());

    struct Candidate {
        double distance;
        Vec3 point;
        SnapKind kind;
    };
    std::optional<Candidate> best;
    auto consider = [&](const std::optional<std::pair<Vec3, double>> &candidate, SnapKind kind) {
        if (candidate && (!best || candidate->second < best->distance)) {
            best = Candidate {candidate->second, candidate->first, kind};
        }
    };

    if (settings.perpendicular) {
        Vec3 normal {0.0, 0.0, 0.0};
        normal[static_cast<std::size_t>(axes.normal)] = 1.0;
        consider(lineSnapCandidate(normal, start, pointer, projection), SnapKind::Perpendicular);
    }
    if (settings.orthogonal) {
        for (const int axis : {axes.u, axes.v}) {
            Vec3 direction {0.0, 0.0, 0.0};
            direction[static_cast<std::size_t>(axis)] = 1.0;
            consider(lineSnapCandidate(direction, start, pointer, projection), SnapKind::Orthogonal);
        }
    }
    if (!best) {
        return std::nullopt;
    }
    return PlacementTarget {best->point, best->kind};
}

PlacementTarget PlacementSolver::resolve(const QPointF &pointer,
                                         const Vec3 &cursor,
                                         const std::optional<Vec3> &start) const
{
    const Projection projection = makeProjection();
    const auto snap = snapPosition(pointer, cursor, projection);
    std::optional<PlacementTarget> guide;
    if (start) {
        guide = guideTarget(*start, pointer, projection);
    }

    if (snap) {
        // Um no ou centro de barra e um snap concreto do modelo e prevalece; o
        // candidato de malha so e um fallback quando o cursor esta alinhado a
        // uma guia (caso contrario a coordenada normal nunca poderia variar).
        if (snap->kind != SnapKind::Grid || !guide) {
            return *snap;
        }
    }
    if (!start) {
        return {cursor, SnapKind::None};
    }
    if (guide) {
        PlacementTarget result = *guide;
        // Alinhado a uma guia ortogonal: o ponto escorrega pela guia, entao a coordenada ao longo dela
        // tambem deve ser atraida pelos valores da malha (senao o snap de malha deixa de funcionar).
        const SnapSettings &settings = m_scene->snap();
        if (result.kind == SnapKind::Orthogonal && settings.enabled && settings.grid && m_scene->gridVisible()) {
            const PlaneAxes axes = planeAxes(m_scene->planeMode());
            const auto du = std::abs(result.point[static_cast<std::size_t>(axes.u)] - (*start)[static_cast<std::size_t>(axes.u)]);
            const auto dv = std::abs(result.point[static_cast<std::size_t>(axes.v)] - (*start)[static_cast<std::size_t>(axes.v)]);
            const int axis = du >= dv ? axes.u : axes.v;
            std::vector<double> uValues;
            std::vector<double> vValues;
            m_scene->latticeValues(uValues, vValues, m_hitLevel);
            const std::vector<double> &values = axis == axes.u ? uValues : vValues;
            if (!values.empty()) {
                double nearest = values.front();
                const double current = result.point[static_cast<std::size_t>(axis)];
                for (const double value : values) {
                    if (std::abs(value - current) < std::abs(nearest - current)) {
                        nearest = value;
                    }
                }
                result.point[static_cast<std::size_t>(axis)] = nearest;
            }
        }
        return result;
    }

    PlacementTarget constrained {cursor, SnapKind::None};
    const PlaneAxes axes = planeAxes(m_scene->planeMode());
    if (!m_scene->showAllLevels()) {
        constrained.point[static_cast<std::size_t>(axes.normal)] = (*start)[static_cast<std::size_t>(axes.normal)];
    }
    return constrained;
}

} // namespace Structura::Construction
