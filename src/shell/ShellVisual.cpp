#include "shell/ShellVisual.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace Structura::Shell {

std::array<unsigned char, 3> rainbow(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    // azul -> ciano -> verde -> amarelo -> vermelho
    double r, g, b;
    if (t < 0.25) {
        r = 0.0;
        g = t / 0.25;
        b = 1.0;
    } else if (t < 0.5) {
        r = 0.0;
        g = 1.0;
        b = 1.0 - (t - 0.25) / 0.25;
    } else if (t < 0.75) {
        r = (t - 0.5) / 0.25;
        g = 1.0;
        b = 0.0;
    } else {
        r = 1.0;
        g = 1.0 - (t - 0.75) / 0.25;
        b = 0.0;
    }
    return {static_cast<unsigned char>(std::lround(255.0 * r)), static_cast<unsigned char>(std::lround(255.0 * g)), static_cast<unsigned char>(std::lround(255.0 * b))};
}

RenderMesh buildRenderMesh(const Mesh &mesh, const Model &model, const CaseResult *result, const RenderOptions &opt)
{
    RenderMesh out;
    const std::size_t quads = mesh.quads.size();
    const bool scalars = opt.elementScalars != nullptr && opt.elementScalars->size() == quads;
    const bool values = scalars || (result != nullptr && result->elements.size() == quads && result->displacement.size() == mesh.nodes.size());
    const QuantityInfo &info = quantityInfo(opt.quantity);

    // valores por canto de elemento
    std::vector<std::array<double, 4>> corner(quads);
    if (values) {
        for (std::size_t e = 0; e < quads; ++e) {
            for (int c = 0; c < 4; ++c) {
                corner[e][static_cast<std::size_t>(c)] = scalars ? (*opt.elementScalars)[e] : elementQuantity(mesh, model, *result, e, c, opt.quantity) * info.scale;
            }
        }
        if (opt.smooth && !scalars && !info.nodal) {
            std::map<std::pair<int, int>, std::pair<double, int>> acc;
            for (std::size_t e = 0; e < quads; ++e) {
                for (int c = 0; c < 4; ++c) {
                    auto &a = acc[{mesh.quads[e].n[static_cast<std::size_t>(c)], mesh.quads[e].zone}];
                    a.first += corner[e][static_cast<std::size_t>(c)];
                    ++a.second;
                }
            }
            for (std::size_t e = 0; e < quads; ++e) {
                for (int c = 0; c < 4; ++c) {
                    const auto &a = acc[{mesh.quads[e].n[static_cast<std::size_t>(c)], mesh.quads[e].zone}];
                    corner[e][static_cast<std::size_t>(c)] = a.first / a.second;
                }
            }
        }
        double lo = 1e300, hi = -1e300;
        for (const auto &q : corner) {
            for (const double v : q) {
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
        }
        out.hasValues = true;
        out.minValue = opt.fixedRange ? opt.rangeMin : lo;
        out.maxValue = opt.fixedRange ? opt.rangeMax : hi;
    }
    const double span = out.maxValue - out.minValue;

    auto position = [&](int node) {
        std::array<double, 3> p = mesh.nodes[static_cast<std::size_t>(node)];
        if (result != nullptr && opt.deformScale != 0.0 && result->displacement.size() == mesh.nodes.size()) {
            const auto &d = result->displacement[static_cast<std::size_t>(node)];
            for (std::size_t i = 0; i < 3; ++i) {
                p[i] += opt.deformScale * d[i];
            }
        }
        return std::array<float, 3> {static_cast<float>(p[0] * opt.unitsPerMeter), static_cast<float>(p[1] * opt.unitsPerMeter), static_cast<float>(p[2] * opt.unitsPerMeter)};
    };

    out.points.reserve(quads * 4);
    for (std::size_t e = 0; e < quads; ++e) {
        const MeshQuad &q = mesh.quads[e];
        const Vec3 n = quadNormal(mesh, q);
        const int base = static_cast<int>(out.points.size());
        for (int c = 0; c < 4; ++c) {
            out.points.push_back(position(q.n[static_cast<std::size_t>(c)]));
            out.normals.push_back({static_cast<float>(n[0]), static_cast<float>(n[1]), static_cast<float>(n[2])});
            if (values) {
                const double t = span > 1e-30 ? (corner[e][static_cast<std::size_t>(c)] - out.minValue) / span : 0.5;
                out.colors.push_back(rainbow(t));
            } else if (q.object == opt.highlightObject) {
                out.colors.push_back(opt.highlightColor);
            } else {
                // concreto em cinza claro, aco em cinza azulado
                const bool concrete = q.material >= 0 && q.material < static_cast<int>(model.materials.size()) && model.materials[static_cast<std::size_t>(q.material)].concrete;
                out.colors.push_back(concrete ? std::array<unsigned char, 3> {196, 196, 188} : opt.plainColor);
            }
        }
        out.triangles.push_back({base, base + 1, base + 2});
        out.triangles.push_back({base, base + 2, base + 3});
        if (opt.edges) {
            for (int c = 0; c < 4; ++c) {
                out.edgePoints.push_back(out.points[static_cast<std::size_t>(base + c)]);
                out.edgePoints.push_back(out.points[static_cast<std::size_t>(base + (c + 1) % 4)]);
            }
        }
    }
    return out;
}

} // namespace Structura::Shell
