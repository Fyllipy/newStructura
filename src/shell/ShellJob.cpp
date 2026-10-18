#include "shell/ShellJob.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace Structura::Shell {

namespace {

constexpr double kPi = 3.14159265358979323846;

bool objectSelected(const Model &model, int objectId, int objectIndex)
{
    return objectId == 0 || model.objects[static_cast<std::size_t>(objectIndex)].id == objectId;
}

} // namespace

std::vector<double> tributaryAreas(const Mesh &mesh, int object)
{
    std::vector<double> area(mesh.nodes.size(), 0.0);
    for (const MeshQuad &q : mesh.quads) {
        if (object >= 0 && q.object != object) {
            continue;
        }
        const double a = quadArea(mesh, q) / 4.0;
        for (const int n : q.n) {
            area[static_cast<std::size_t>(n)] += a;
        }
    }
    return area;
}

Job buildJob(const Model &model, const Mesh &mesh, const std::vector<std::string> &caseNames)
{
    Job job;
    job.nodes.resize(mesh.nodes.size());
    for (std::size_t i = 0; i < mesh.nodes.size(); ++i) {
        job.nodes[i].x = mesh.nodes[i];
    }

    job.ties = mesh.ties;

    // ---- apoios -------------------------------------------------------------------------------------------------------------
    // arestas livres: aresta usada por um unico elemento
    std::map<std::pair<int, int>, int> edgeCount;
    std::map<std::pair<int, int>, int> edgeObject;
    for (const MeshQuad &q : mesh.quads) {
        for (int i = 0; i < 4; ++i) {
            const int a = q.n[static_cast<std::size_t>(i)], b = q.n[static_cast<std::size_t>((i + 1) % 4)];
            const auto key = std::make_pair(std::min(a, b), std::max(a, b));
            ++edgeCount[key];
            edgeObject[key] = q.object;
        }
    }
    std::vector<std::set<int>> nodeObjects(mesh.nodes.size());
    for (const MeshQuad &q : mesh.quads) {
        for (const int n : q.n) {
            nodeObjects[static_cast<std::size_t>(n)].insert(q.object);
        }
    }
    for (const Support &s : model.supports) {
        std::vector<int> selected;
        auto belongs = [&](int node) {
            if (s.object == 0) {
                return true;
            }
            for (const int obj : nodeObjects[static_cast<std::size_t>(node)]) {
                if (model.objects[static_cast<std::size_t>(obj)].id == s.object) {
                    return true;
                }
            }
            return false;
        };
        auto limited = [&](int node) {
            if (s.limitAxis < 0 || s.limitAxis > 2) {
                return true;
            }
            const double c = mesh.nodes[static_cast<std::size_t>(node)][static_cast<std::size_t>(s.limitAxis)];
            return s.limitBelow ? c <= s.limitValue + s.tol : c >= s.limitValue - s.tol;
        };
        if (s.selector == Support::Plane) {
            const std::size_t axis = static_cast<std::size_t>(std::clamp(s.axis, 0, 2));
            for (std::size_t n = 0; n < mesh.nodes.size(); ++n) {
                if (std::abs(mesh.nodes[n][axis] - s.value) <= s.tol && belongs(static_cast<int>(n)) && limited(static_cast<int>(n))) {
                    selected.push_back(static_cast<int>(n));
                }
            }
            if (s.legs > 0 && !selected.empty()) {
                // centro do anel e angulos dos apoios pontuais
                Vec3 center {0.0, 0.0, 0.0};
                for (const int n : selected) {
                    for (int c = 0; c < 3; ++c) {
                        center[static_cast<std::size_t>(c)] += mesh.nodes[static_cast<std::size_t>(n)][static_cast<std::size_t>(c)] / static_cast<double>(selected.size());
                    }
                }
                const std::size_t a1 = axis == 0 ? 1 : 0, a2 = axis == 2 ? 1 : 2;
                std::vector<int> chosen;
                for (int leg = 0; leg < s.legs; ++leg) {
                    const double angle = (s.legStart + 360.0 * leg / s.legs) * kPi / 180.0;
                    int best = -1;
                    double bestDiff = 1e9;
                    for (const int n : selected) {
                        const Vec3 &p = mesh.nodes[static_cast<std::size_t>(n)];
                        const double a = std::atan2(p[a2] - center[a2], p[a1] - center[a1]);
                        double diff = std::abs(std::remainder(a - angle, 2.0 * kPi));
                        // prefere os nos de maior raio (borda do anel)
                        diff += 1e-6 * (1.0 - std::hypot(p[a1] - center[a1], p[a2] - center[a2]));
                        if (diff < bestDiff) {
                            bestDiff = diff;
                            best = n;
                        }
                    }
                    if (best >= 0) {
                        chosen.push_back(best);
                    }
                }
                selected = chosen;
            }
        } else if (s.selector == Support::FreeEdge) {
            std::set<int> nodes;
            for (const auto &[edge, count] : edgeCount) {
                if (count != 1) {
                    continue;
                }
                if (s.object != 0 && model.objects[static_cast<std::size_t>(edgeObject[edge])].id != s.object) {
                    continue;
                }
                nodes.insert(edge.first);
                nodes.insert(edge.second);
            }
            for (const int n : nodes) {
                if (limited(n)) {
                    selected.push_back(n);
                }
            }
        } else {
            int best = -1;
            double bestDist = 1e300;
            for (std::size_t n = 0; n < mesh.nodes.size(); ++n) {
                const Vec3 &p = mesh.nodes[n];
                const double d = std::hypot(std::hypot(p[0] - s.value, p[1] - s.value2), p[2] - s.value3);
                if (d < bestDist && belongs(static_cast<int>(n))) {
                    bestDist = d;
                    best = static_cast<int>(n);
                }
            }
            if (best >= 0) {
                selected.push_back(best);
            }
        }
        if (selected.empty()) {
            job.warnings.push_back("Um apoio nao encontrou nos (confira a posicao ou a tolerancia).");
            continue;
        }
        if (s.springPerArea > 0.0) {
            const std::vector<double> area = tributaryAreas(mesh, -1);
            for (const int n : selected) {
                Job::SpringDef sp;
                sp.node = n;
                const double k = s.springPerArea * area[static_cast<std::size_t>(n)];
                if (s.compressionOnly) {
                    sp.k = {k, 0.0, 0.0, 0.0, 0.0, 0.0};
                    sp.compressionOnly = true;
                    sp.normal = s.springNormal;
                } else {
                    sp.k = {k, k, k, 0.0, 0.0, 0.0};
                }
                job.springs.push_back(sp);
            }
        } else {
            for (const int n : selected) {
                for (int c = 0; c < 6; ++c) {
                    if (s.fixed[static_cast<std::size_t>(c)]) {
                        job.nodes[static_cast<std::size_t>(n)].fixed[static_cast<std::size_t>(c)] = 1;
                    }
                }
            }
        }
    }

    // ---- cargas --------------------------------------------------------------------------------------------------------------
    for (const std::string &name : caseNames) {
        Job::Case c;
        c.name = name;
        for (const Load &load : model.loads) {
            if (load.caseName != name) {
                continue;
            }
            for (std::size_t e = 0; e < mesh.quads.size(); ++e) {
                const MeshQuad &q = mesh.quads[e];
                if (!objectSelected(model, load.object, q.object)) {
                    continue;
                }
                const int shell = static_cast<int>(e);
                switch (load.type) {
                case Load::SelfWeight: {
                    const double gamma = model.materials[static_cast<std::size_t>(q.material)].unitWeight * load.v[0];
                    c.body.push_back({shell, {0.0, 0.0, -gamma}});
                    break;
                }
                case Load::Pressure: {
                    c.pressure.push_back({shell, {load.v[0], load.v[0], load.v[0], load.v[0]}});
                    break;
                }
                case Load::Hydrostatic: {
                    Job::Pressure p;
                    p.shell = shell;
                    for (int i = 0; i < 4; ++i) {
                        const double z = mesh.nodes[static_cast<std::size_t>(q.n[static_cast<std::size_t>(i)])][2];
                        p.p[static_cast<std::size_t>(i)] = load.v[2] + load.v[0] * std::max(0.0, load.v[1] - z);
                    }
                    c.pressure.push_back(p);
                    break;
                }
                case Load::Thermal:
                    c.thermal.push_back({shell, load.v[0], load.v[1]});
                    break;
                case Load::Area: {
                    const Vec3 n = quadNormal(mesh, q);
                    double area = quadArea(mesh, q);
                    const double qx = load.v[0] / 1.0, qy = load.v[1], qz = load.v[2];
                    if (load.v[3] != 0.0) {
                        // por area projetada em planta: area * |n . z|
                        area *= std::abs(n[2]);
                    }
                    for (const int node : q.n) {
                        Job::NodalForce f;
                        f.node = node;
                        f.f = {qx * area / 4.0, qy * area / 4.0, qz * area / 4.0, 0.0, 0.0, 0.0};
                        c.nodal.push_back(f);
                    }
                    break;
                }
                }
            }
        }
        job.cases.push_back(std::move(c));
    }
    return job;
}

} // namespace Structura::Shell
