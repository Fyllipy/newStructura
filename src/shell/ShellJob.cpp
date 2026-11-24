#include "shell/ShellJob.h"

#include "shell/ShellLoads.h"
#include "shell/ShellSupports.h"

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
    // a selecao dos nos e feita por `resolveSupports` (a mesma que a vista usa para desenhar os apoios)
    const SupportResolution resolution = resolveSupports(model, mesh);
    for (const std::string &w : resolution.warnings) {
        job.warnings.push_back(w);
    }
    for (const ResolvedSupport &resolved : resolution.supports) {
        const Support &s = model.supports[static_cast<std::size_t>(resolved.support)];
        const std::vector<int> &selected = resolved.nodes;
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
        for (std::size_t loadIndex = 0; loadIndex < model.loads.size(); ++loadIndex) {
            const Load &load = model.loads[loadIndex];
            if (load.caseName != name) {
                continue;
            }
            if (isPointLoad(load.type)) {
                // cargas por clique em nos: alvos pelas coordenadas, forcas nodais equivalentes (ver ShellLoads.h)
                const PointLoadEffect fx = pointLoadEffect(model, mesh, loadIndex);
                for (const std::string &w : fx.warnings) {
                    job.warnings.push_back(w);
                }
                for (const auto &[node, f] : fx.nodalForces) {
                    Job::NodalForce nf;
                    nf.node = node;
                    nf.f = {f[0], f[1], f[2], 0.0, 0.0, 0.0};
                    c.nodal.push_back(nf);
                }
                for (const auto &[shell, p] : fx.pressures) {
                    c.pressure.push_back({shell, {p, p, p, p}});
                }
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

Job combinationJob(const Job &job, const std::vector<std::pair<std::string, double>> &factors, const std::string &name)
{
    Job out;
    out.nodes = job.nodes;
    out.springs = job.springs;
    out.ties = job.ties;
    out.warnings = job.warnings;
    Job::Case combined;
    combined.name = name;
    for (const auto &[caseName, factor] : factors) {
        for (const Job::Case &c : job.cases) {
            if (c.name != caseName) {
                continue;
            }
            for (Job::NodalForce f : c.nodal) {
                for (double &v : f.f) {
                    v *= factor;
                }
                combined.nodal.push_back(f);
            }
            for (Job::Pressure p : c.pressure) {
                for (double &v : p.p) {
                    v *= factor;
                }
                combined.pressure.push_back(p);
            }
            for (Job::Body b : c.body) {
                b.b = {{b.b[0] * factor, b.b[1] * factor, b.b[2] * factor}};
                combined.body.push_back(b);
            }
            for (Job::Thermal t : c.thermal) {
                t.uniform *= factor;
                t.gradient *= factor;
                combined.thermal.push_back(t);
            }
        }
    }
    out.cases.push_back(std::move(combined));
    return out;
}

} // namespace Structura::Shell
