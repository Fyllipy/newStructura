#include "shell/ShellLoads.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace Structura::Shell {

double edgeLength(const Mesh &mesh, int a, int b)
{
    const Vec3 &p = mesh.nodes[static_cast<std::size_t>(a)];
    const Vec3 &q = mesh.nodes[static_cast<std::size_t>(b)];
    return std::sqrt((p[0] - q[0]) * (p[0] - q[0]) + (p[1] - q[1]) * (p[1] - q[1]) + (p[2] - q[2]) * (p[2] - q[2]));
}

ResolvedPointLoad resolvePointLoad(const Model &model, const Mesh &mesh, std::size_t loadIndex)
{
    ResolvedPointLoad out;
    out.load = static_cast<int>(loadIndex);
    const Load &load = model.loads[loadIndex];
    // objetos de cada no (filtro por objeto, como nos apoios)
    std::vector<std::set<int>> nodeObjects(mesh.nodes.size());
    for (const MeshQuad &q : mesh.quads) {
        for (const int n : q.n) {
            nodeObjects[static_cast<std::size_t>(n)].insert(q.object);
        }
    }
    auto belongs = [&](int node) {
        if (load.object == 0) {
            return true;
        }
        for (const int obj : nodeObjects[static_cast<std::size_t>(node)]) {
            if (model.objects[static_cast<std::size_t>(obj)].id == load.object) {
                return true;
            }
        }
        return false;
    };
    std::set<int> found;
    for (const auto &p : load.points) {
        int best = -1;
        double bestDist = kPointLoadTolerance;
        for (std::size_t n = 0; n < mesh.nodes.size(); ++n) {
            const Vec3 &q = mesh.nodes[n];
            const double d = std::sqrt((q[0] - p[0]) * (q[0] - p[0]) + (q[1] - p[1]) * (q[1] - p[1]) + (q[2] - p[2]) * (q[2] - p[2]));
            if (d <= bestDist && belongs(static_cast<int>(n))) {
                bestDist = d;
                best = static_cast<int>(n);
            }
        }
        if (best < 0) {
            ++out.missing;
        } else {
            found.insert(best);
        }
    }
    out.nodes.assign(found.begin(), found.end());
    if (load.type == Load::LinePoints) {
        std::set<std::pair<int, int>> seen;
        for (const MeshQuad &q : mesh.quads) {
            for (int i = 0; i < 4; ++i) {
                const int a = q.n[static_cast<std::size_t>(i)], b = q.n[static_cast<std::size_t>((i + 1) % 4)];
                if (found.count(a) && found.count(b) && seen.insert({std::min(a, b), std::max(a, b)}).second) {
                    out.edges.push_back({a, b});
                }
            }
        }
    } else if (load.type == Load::RegionPoints) {
        for (std::size_t e = 0; e < mesh.quads.size(); ++e) {
            const MeshQuad &q = mesh.quads[e];
            if (found.count(q.n[0]) && found.count(q.n[1]) && found.count(q.n[2]) && found.count(q.n[3])) {
                out.elements.push_back(static_cast<int>(e));
            }
        }
    }
    return out;
}

PointLoadEffect pointLoadEffect(const Model &model, const Mesh &mesh, std::size_t loadIndex)
{
    PointLoadEffect fx;
    const Load &load = model.loads[loadIndex];
    const ResolvedPointLoad r = resolvePointLoad(model, mesh, loadIndex);
    if (r.missing > 0) {
        fx.warnings.push_back("Carga por pontos: " + std::to_string(r.missing) + " ponto(s) nao coincidem com nos da malha atual (a malha foi refeita?).");
    }
    const std::array<double, 3> v {{load.v[0], load.v[1], load.v[2]}};
    switch (load.type) {
    case Load::NodalPoints:
        if (r.nodes.empty()) {
            fx.warnings.push_back("Uma forca em nos nao encontrou nos.");
        }
        for (const int n : r.nodes) {
            fx.nodalForces.push_back({n, v});
        }
        break;
    case Load::LinePoints:
        if (r.edges.empty()) {
            fx.warnings.push_back("Uma carga linear nao encontrou arestas (os dois nos de cada aresta precisam estar selecionados).");
        }
        for (const auto &e : r.edges) {
            const double half = 0.5 * edgeLength(mesh, e[0], e[1]);
            for (const int n : e) {
                fx.nodalForces.push_back({n, {v[0] * half, v[1] * half, v[2] * half}});
            }
        }
        break;
    case Load::RegionPoints:
        if (r.elements.empty()) {
            fx.warnings.push_back("Uma carga de superficie nao encontrou elementos (os quatro nos de cada elemento precisam estar selecionados).");
        }
        for (const int e : r.elements) {
            const MeshQuad &q = mesh.quads[static_cast<std::size_t>(e)];
            const int mode = static_cast<int>(std::lround(load.v[3]));
            if (mode == 2) {
                fx.pressures.push_back({e, load.v[0]});
                continue;
            }
            double area = quadArea(mesh, q);
            if (mode == 1) {
                area *= std::abs(quadNormal(mesh, q)[2]);   // por area projetada em planta
            }
            for (const int n : q.n) {
                fx.nodalForces.push_back({n, {v[0] * area / 4.0, v[1] * area / 4.0, v[2] * area / 4.0}});
            }
        }
        break;
    default:
        break;
    }
    return fx;
}

std::vector<LoadArrow> loadArrows(const Model &model, const Mesh &mesh)
{
    std::vector<LoadArrow> arrows;
    auto unit = [](std::array<double, 3> d, double &mag) {
        mag = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (mag > 0.0) {
            d = {d[0] / mag, d[1] / mag, d[2] / mag};
        }
        return d;
    };
    for (std::size_t i = 0; i < model.loads.size(); ++i) {
        const Load &load = model.loads[i];
        if (!isPointLoad(load.type)) {
            continue;
        }
        const ResolvedPointLoad r = resolvePointLoad(model, mesh, i);
        const std::array<double, 3> v {{load.v[0], load.v[1], load.v[2]}};
        double mag = 0.0;
        const std::array<double, 3> dir = unit(v, mag);
        auto add = [&](int kind, const std::array<double, 3> &pos, const std::array<double, 3> &d, double m) {
            if (!(m > 0.0)) {
                return;
            }
            LoadArrow a;
            a.load = static_cast<int>(i);
            a.kind = kind;
            a.position = pos;
            a.direction = d;
            a.magnitude = m;
            a.caseName = load.caseName;
            arrows.push_back(a);
        };
        if (load.type == Load::NodalPoints) {
            for (const int n : r.nodes) {
                add(0, mesh.nodes[static_cast<std::size_t>(n)], dir, mag);
            }
        } else if (load.type == Load::LinePoints) {
            for (const auto &e : r.edges) {
                const Vec3 &p = mesh.nodes[static_cast<std::size_t>(e[0])], &q = mesh.nodes[static_cast<std::size_t>(e[1])];
                add(1, {0.5 * (p[0] + q[0]), 0.5 * (p[1] + q[1]), 0.5 * (p[2] + q[2])}, dir, mag);
            }
        } else {
            const int mode = static_cast<int>(std::lround(load.v[3]));
            for (const int e : r.elements) {
                const MeshQuad &q = mesh.quads[static_cast<std::size_t>(e)];
                std::array<double, 3> c {{0.0, 0.0, 0.0}};
                for (const int n : q.n) {
                    for (std::size_t k = 0; k < 3; ++k) {
                        c[k] += mesh.nodes[static_cast<std::size_t>(n)][k] / 4.0;
                    }
                }
                if (mode == 2) {
                    const Vec3 n = quadNormal(mesh, q);
                    const double sign = load.v[0] >= 0.0 ? 1.0 : -1.0;
                    add(2, c, {sign * n[0], sign * n[1], sign * n[2]}, std::abs(load.v[0]));
                } else {
                    add(2, c, dir, mag);
                }
            }
        }
    }
    return arrows;
}

} // namespace Structura::Shell
