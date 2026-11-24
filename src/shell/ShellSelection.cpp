#include "shell/ShellSelection.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <tuple>

namespace Structura::Shell {

int NodeTable::indexOf(int id) const
{
    // ids crescem na ordem em que a tabela e montada (por no), mas nao ha garantia de contiguidade: busca binaria em copia ordenada seria
    // desnecessaria para o uso (poucas chamadas); varredura linear mantem a tabela simples.
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

NodeTable buildNodeTable(const RenderMesh &mesh)
{
    NodeTable t;
    if (mesh.vertexNode.size() != mesh.points.size() || mesh.points.empty()) {
        return t;
    }
    const double upm = mesh.unitsPerMeter > 0.0 ? mesh.unitsPerMeter : 1.0;
    const bool hasRest = mesh.restPoints.size() == mesh.points.size();
    std::map<int, std::size_t> slot;
    for (std::size_t v = 0; v < mesh.points.size(); ++v) {
        const int id = mesh.vertexNode[v];
        if (id < 0 || slot.count(id)) {
            continue;
        }
        slot[id] = t.ids.size();
        t.ids.push_back(id);
        const auto &view = mesh.points[v];
        const auto &rest = hasRest ? mesh.restPoints[v] : mesh.points[v];
        t.viewPos.push_back({static_cast<double>(view[0]), static_cast<double>(view[1]), static_cast<double>(view[2])});
        t.restPos.push_back({static_cast<double>(rest[0]) / upm, static_cast<double>(rest[1]) / upm, static_cast<double>(rest[2]) / upm});
        const std::size_t quad = v / 4;
        int object = -1;
        if (quad < mesh.quadObject.size()) {
            const int o = mesh.quadObject[quad];
            if (o >= 0 && static_cast<std::size_t>(o) < mesh.objectIds.size()) {
                object = mesh.objectIds[static_cast<std::size_t>(o)];
            }
        }
        t.objectId.push_back(object);
    }
    return t;
}

bool ScreenProjector::project(const std::array<double, 3> &p, double &sx, double &sy, double &depth) const
{
    const auto &m = matrix;
    const double x = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
    const double y = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
    const double z = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
    const double w = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];
    if (!(w > 1e-9)) {
        return false;   // atras da camera
    }
    const double nz = z / w;
    if (!(std::abs(nz) <= 1.0)) {
        return false;   // fora do volume de recorte
    }
    sx = (x / w + 1.0) * width / 2.0;
    sy = (1.0 - y / w) * height / 2.0;
    depth = nz;
    return true;
}

namespace {

/// Projecao sem recorte em profundidade (so exige o ponto a frente da camera): usada na rasterizacao.
bool projectRaw(const ScreenProjector &pr, const std::array<double, 3> &p, double &sx, double &sy, double &nz)
{
    const auto &m = pr.matrix;
    const double x = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
    const double y = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
    const double z = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
    const double w = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];
    if (!(w > 1e-9)) {
        return false;
    }
    sx = (x / w + 1.0) * pr.width / 2.0;
    sy = (1.0 - y / w) * pr.height / 2.0;
    nz = z / w;
    return true;
}

} // namespace

DepthMap buildDepthMap(const RenderMesh &mesh, const ScreenProjector &projector)
{
    DepthMap map;
    if (!(projector.width >= 1.0) || !(projector.height >= 1.0)) {
        return map;
    }
    const double pixels = projector.width * projector.height;
    map.scale = std::max(1, static_cast<int>(std::ceil(std::sqrt(pixels / 1.5e6))));
    map.width = std::max(1, static_cast<int>(std::ceil(projector.width / map.scale)));
    map.height = std::max(1, static_cast<int>(std::ceil(projector.height / map.scale)));
    const float inf = std::numeric_limits<float>::infinity();
    map.depth.assign(static_cast<std::size_t>(map.width) * static_cast<std::size_t>(map.height), inf);
    map.slope.assign(map.depth.size(), 0.0f);
    const double inv = 1.0 / map.scale;
    for (const auto &tri : mesh.triangles) {
        double x[3], y[3], z[3];
        bool ok = true;
        for (int k = 0; k < 3 && ok; ++k) {
            const auto &pt = mesh.points[static_cast<std::size_t>(tri[static_cast<std::size_t>(k)])];
            double sx, sy, nz;
            ok = projectRaw(projector, {static_cast<double>(pt[0]), static_cast<double>(pt[1]), static_cast<double>(pt[2])}, sx, sy, nz);
            x[k] = sx * inv;
            y[k] = sy * inv;
            z[k] = nz;
        }
        if (!ok) {
            continue;
        }
        const double det = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
        if (std::abs(det) < 1e-12) {
            continue;   // de perfil: nao cobre pixels
        }
        // plano de profundidade z = a x + b y + c (linear no espaco da tela, tambem em perspectiva)
        const double a = ((z[1] - z[0]) * (y[2] - y[0]) - (z[2] - z[0]) * (y[1] - y[0])) / det;
        const double b = ((x[1] - x[0]) * (z[2] - z[0]) - (x[2] - x[0]) * (z[1] - z[0])) / det;
        const double c = z[0] - a * x[0] - b * y[0];
        const int x0 = std::max(0, static_cast<int>(std::floor(std::min({x[0], x[1], x[2]}))));
        const int x1 = std::min(map.width - 1, static_cast<int>(std::ceil(std::max({x[0], x[1], x[2]}))));
        const int y0 = std::max(0, static_cast<int>(std::floor(std::min({y[0], y[1], y[2]}))));
        const int y1 = std::min(map.height - 1, static_cast<int>(std::ceil(std::max({y[0], y[1], y[2]}))));
        const float slope = static_cast<float>(std::abs(a) + std::abs(b));
        for (int py = y0; py <= y1; ++py) {
            for (int px = x0; px <= x1; ++px) {
                const double cx = px + 0.5, cy = py + 0.5;   // centro do pixel
                const double w0 = ((x[1] - cx) * (y[2] - cy) - (x[2] - cx) * (y[1] - cy)) / det;
                const double w1 = ((x[2] - cx) * (y[0] - cy) - (x[0] - cx) * (y[2] - cy)) / det;
                const double w2 = 1.0 - w0 - w1;
                constexpr double kEdge = -1e-7;
                if (w0 < kEdge || w1 < kEdge || w2 < kEdge) {
                    continue;
                }
                const float zz = static_cast<float>(a * cx + b * cy + c);
                const std::size_t idx = static_cast<std::size_t>(py) * static_cast<std::size_t>(map.width) + static_cast<std::size_t>(px);
                if (zz < map.depth[idx]) {
                    map.depth[idx] = zz;
                    map.slope[idx] = slope;
                }
            }
        }
    }
    return map;
}

bool isNodeVisible(const DepthMap &map, const ScreenProjector &projector, const std::array<double, 3> &p)
{
    if (map.empty()) {
        return true;   // sem mapa: sem oclusao
    }
    double sx, sy, nz;
    if (!projectRaw(projector, p, sx, sy, nz)) {
        return false;
    }
    const int cx = static_cast<int>(std::floor(sx / map.scale)), cy = static_cast<int>(std::floor(sy / map.scale));
    float nearest = std::numeric_limits<float>::infinity();
    float bias = 0.0f;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int px = cx + dx, py = cy + dy;
            if (px < 0 || py < 0 || px >= map.width || py >= map.height) {
                continue;
            }
            const std::size_t idx = static_cast<std::size_t>(py) * static_cast<std::size_t>(map.width) + static_cast<std::size_t>(px);
            if (map.depth[idx] < nearest) {
                nearest = map.depth[idx];
            }
            // a tolerancia vem da inclinacao da superficie na vizinhanca: o no pode estar ate ~1,5 pixel do centro do pixel amostrado
            bias = std::max(bias, map.slope[idx]);
        }
    }
    if (!std::isfinite(nearest)) {
        return true;   // nada cobre o pixel
    }
    return nz <= static_cast<double>(nearest) + 1.5 * static_cast<double>(bias) + 2e-6;
}

int pickNearestNode(const NodeTable &table, const ScreenProjector &projector, double sx, double sy, double tolerancePx, double *distancePx, const DepthMap *occlusion)
{
    int best = -1;
    double bestDist = tolerancePx, bestDepth = 2.0;
    for (std::size_t i = 0; i < table.size(); ++i) {
        double px, py, depth;
        if (!projector.project(table.viewPos[i], px, py, depth)) {
            continue;
        }
        const double d = std::hypot(px - sx, py - sy);
        if (d > tolerancePx) {
            continue;
        }
        if (occlusion && !isNodeVisible(*occlusion, projector, table.viewPos[i])) {
            continue;
        }
        const bool better = best < 0 || d < bestDist - 0.5 || (std::abs(d - bestDist) <= 0.5 && depth < bestDepth);
        if (better) {
            best = static_cast<int>(i);
            bestDist = d;
            bestDepth = depth;
        }
    }
    if (best >= 0 && distancePx) {
        *distancePx = bestDist;
    }
    return best;
}

std::vector<int> nodesInRect(const NodeTable &table, const ScreenProjector &projector, double x0, double y0, double x1, double y1, const DepthMap *occlusion)
{
    if (x0 > x1) {
        std::swap(x0, x1);
    }
    if (y0 > y1) {
        std::swap(y0, y1);
    }
    std::vector<int> out;
    for (std::size_t i = 0; i < table.size(); ++i) {
        double px, py, depth;
        if (projector.project(table.viewPos[i], px, py, depth) && px >= x0 && px <= x1 && py >= y0 && py <= y1) {
            if (occlusion && !isNodeVisible(*occlusion, projector, table.viewPos[i])) {
                continue;
            }
            out.push_back(static_cast<int>(i));
        }
    }
    return out;
}

void NodeSelection::apply(const std::vector<int> &ids, Mode mode)
{
    if (mode == Mode::Replace) {
        m_nodes.clear();
    }
    for (const int id : ids) {
        if (mode == Mode::Toggle) {
            if (!m_nodes.erase(id)) {
                m_nodes.insert(id);
            }
        } else {
            m_nodes.insert(id);
        }
    }
}

std::vector<NodeRef> makeNodeRefs(const NodeTable &table, const std::vector<int> &nodeIds)
{
    std::vector<NodeRef> refs;
    for (const int id : nodeIds) {
        const int i = table.indexOf(id);
        if (i < 0) {
            continue;
        }
        NodeRef r;
        r.objectId = table.objectId[static_cast<std::size_t>(i)];
        r.position = table.restPos[static_cast<std::size_t>(i)];
        refs.push_back(r);
    }
    return refs;
}

RefResolution resolveNodeRefs(const NodeTable &table, const std::vector<NodeRef> &refs, double tolerance)
{
    RefResolution out;
    out.tableIndex.assign(refs.size(), -1);
    // grade espacial: celulas de lado `tolerance` (busca nas 27 vizinhas); a malha tem milhares de nos e as referencias dezenas
    const double cell = std::max(tolerance, 1e-9);
    using Key = std::tuple<long long, long long, long long>;
    auto keyOf = [&](const std::array<double, 3> &p) {
        return Key {static_cast<long long>(std::floor(p[0] / cell)), static_cast<long long>(std::floor(p[1] / cell)), static_cast<long long>(std::floor(p[2] / cell))};
    };
    std::map<Key, std::vector<int>> grid;
    for (std::size_t i = 0; i < table.size(); ++i) {
        grid[keyOf(table.restPos[i])].push_back(static_cast<int>(i));
    }
    std::set<int> used;
    for (std::size_t r = 0; r < refs.size(); ++r) {
        const auto &p = refs[r].position;
        const auto [kx, ky, kz] = keyOf(p);
        int best = -1;
        double bestDist = tolerance;
        for (long long dx = -1; dx <= 1; ++dx) {
            for (long long dy = -1; dy <= 1; ++dy) {
                for (long long dz = -1; dz <= 1; ++dz) {
                    const auto it = grid.find(Key {kx + dx, ky + dy, kz + dz});
                    if (it == grid.end()) {
                        continue;
                    }
                    for (const int i : it->second) {
                        const auto &q = table.restPos[static_cast<std::size_t>(i)];
                        const double d = std::sqrt((q[0] - p[0]) * (q[0] - p[0]) + (q[1] - p[1]) * (q[1] - p[1]) + (q[2] - p[2]) * (q[2] - p[2]));
                        if (d <= bestDist) {
                            bestDist = d;
                            best = i;
                        }
                    }
                }
            }
        }
        if (best < 0) {
            ++out.unresolved;
            continue;
        }
        out.tableIndex[r] = best;
        if (bestDist > 1e-9) {
            ++out.snapped;
        }
        const int id = table.ids[static_cast<std::size_t>(best)];
        if (!used.insert(id).second) {
            ++out.merged;
        } else {
            out.nodeIds.push_back(id);
        }
    }
    return out;
}

} // namespace Structura::Shell
