#include "shell/ShellVisual.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
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

int colorBand(double t)
{
    if (!(t >= 0.0)) {
        return 0;
    }
    return std::clamp(static_cast<int>(std::floor(t * kColorBands)), 0, kColorBands - 1);
}

std::array<unsigned char, 3> bandColor(double t)
{
    return rainbow(static_cast<double>(colorBand(t)) / static_cast<double>(kColorBands - 1));
}

double colorPosition(double value, double lo, double hi)
{
    const double span = hi - lo;
    const double scale = std::max(std::abs(lo), std::abs(hi));
    // faixa constante (ou diferenca no nivel do ruido numerico): cor do meio, em vez de amplificar ruido de arredondamento
    if (!(span > 1e-9 * scale) || !(span > 1e-300)) {
        return 0.5;
    }
    return (value - lo) / span;
}

namespace {

std::uint64_t mixBits(std::uint64_t h, std::uint64_t v)
{
    h ^= v;
    return h * 1099511628211ULL;
}

std::uint64_t geometryHash(const RenderMesh &m)
{
    std::uint64_t h = 1469598103934665603ULL;
    auto addFloats = [&](const std::vector<std::array<float, 3>> &v) {
        h = mixBits(h, v.size());
        for (const auto &p : v) {
            for (const float c : p) {
                std::uint32_t bits;
                std::memcpy(&bits, &c, sizeof(bits));
                h = mixBits(h, bits);
            }
        }
    };
    addFloats(m.points);
    addFloats(m.edgePoints);
    h = mixBits(h, m.triangles.size());
    for (const auto &t : m.triangles) {
        for (const int i : t) {
            h = mixBits(h, static_cast<std::uint64_t>(i));
        }
    }
    return h;
}

} // namespace

RenderMesh buildRenderMesh(const Mesh &mesh, const Model &model, const CaseResult *result, const RenderOptions &opt)
{
    RenderMesh out;
    const std::size_t quads = mesh.quads.size();
    const bool scalars = opt.elementScalars != nullptr && opt.elementScalars->size() == quads;
    const bool values = scalars || (result != nullptr && result->elements.size() == quads && result->displacement.size() == mesh.nodes.size());
    const QuantityInfo &info = quantityInfo(opt.quantity);

    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    // valores por canto de elemento (NaN = invalido; infinito tambem e tratado como invalido)
    std::vector<std::array<double, 4>> corner(quads);
    std::vector<std::array<double, 4>> own;
    if (values) {
        for (std::size_t e = 0; e < quads; ++e) {
            for (int c = 0; c < 4; ++c) {
                const double v = scalars ? (*opt.elementScalars)[e] : elementQuantity(mesh, model, *result, e, c, opt.quantity) * info.scale;
                corner[e][static_cast<std::size_t>(c)] = std::isfinite(v) ? v : kNaN;
            }
        }
        own = corner;   // valores do proprio elemento, antes de qualquer alisamento
        if (opt.smooth && !scalars && !info.nodal) {
            std::map<std::pair<int, int>, std::pair<double, int>> acc;
            for (std::size_t e = 0; e < quads; ++e) {
                for (int c = 0; c < 4; ++c) {
                    const double v = corner[e][static_cast<std::size_t>(c)];
                    if (std::isfinite(v)) {
                        auto &a = acc[{mesh.quads[e].n[static_cast<std::size_t>(c)], mesh.quads[e].zone}];
                        a.first += v;
                        ++a.second;
                    }
                }
            }
            for (std::size_t e = 0; e < quads; ++e) {
                for (int c = 0; c < 4; ++c) {
                    const auto it = acc.find({mesh.quads[e].n[static_cast<std::size_t>(c)], mesh.quads[e].zone});
                    // vertice invalido no proprio elemento continua invalido (nao herda valor dos vizinhos)
                    if (it != acc.end() && it->second.second > 0 && std::isfinite(corner[e][static_cast<std::size_t>(c)])) {
                        corner[e][static_cast<std::size_t>(c)] = it->second.first / it->second.second;
                    }
                }
            }
        }
        double lo = 1e300, hi = -1e300;
        bool any = false;
        for (const auto &q : corner) {
            for (const double v : q) {
                if (std::isfinite(v)) {
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                    any = true;
                }
            }
        }
        if (any) {
            out.hasValues = true;
            out.minValue = opt.fixedRange ? opt.rangeMin : lo;
            out.maxValue = opt.fixedRange ? opt.rangeMax : hi;
        }
        // nenhum valor valido: a malha e desenhada sem resultados (sem legenda nem probe)
    }
    const bool colored = out.hasValues;
    out.fixedRange = opt.fixedRange && colored;
    out.smoothed = opt.smooth && !scalars && !info.nodal && colored;
    out.unitsPerMeter = opt.unitsPerMeter;
    if (opt.overrideLabels) {
        out.title = opt.title;
        out.unit = opt.unit;
    } else {
        out.title = info.name;
        out.unit = info.unit;
    }

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

    const bool deformed = result != nullptr && opt.deformScale != 0.0 && result->displacement.size() == mesh.nodes.size();
    out.points.reserve(quads * 4);
    out.quadObject.reserve(quads);
    out.quadElement.reserve(quads);
    out.vertexNode.reserve(quads * 4);
    if (colored) {
        out.scalars.reserve(quads * 4);
        out.solverScalars.reserve(quads * 4);
    }
    if (deformed) {
        out.restPoints.reserve(quads * 4);
    }
    for (const Object &o : model.objects) {
        out.objectIds.push_back(o.id);
        out.objectNames.push_back(o.name);
    }
    for (std::size_t e = 0; e < quads; ++e) {
        const MeshQuad &q = mesh.quads[e];
        out.quadObject.push_back(q.object >= 0 && q.object < static_cast<int>(model.objects.size()) ? q.object : -1);
        out.quadElement.push_back(static_cast<int>(e));
        const Vec3 n = quadNormal(mesh, q);
        const int base = static_cast<int>(out.points.size());
        for (int c = 0; c < 4; ++c) {
            out.points.push_back(position(q.n[static_cast<std::size_t>(c)]));
            out.vertexNode.push_back(q.n[static_cast<std::size_t>(c)]);
            if (deformed) {
                const auto &r = mesh.nodes[static_cast<std::size_t>(q.n[static_cast<std::size_t>(c)])];
                out.restPoints.push_back({static_cast<float>(r[0] * opt.unitsPerMeter), static_cast<float>(r[1] * opt.unitsPerMeter), static_cast<float>(r[2] * opt.unitsPerMeter)});
            }
            out.normals.push_back({static_cast<float>(n[0]), static_cast<float>(n[1]), static_cast<float>(n[2])});
            if (colored) {
                const double v = corner[e][static_cast<std::size_t>(c)];
                out.scalars.push_back(v);
                out.solverScalars.push_back(own[e][static_cast<std::size_t>(c)]);
                out.colors.push_back(std::isfinite(v) ? rainbow(colorPosition(v, out.minValue, out.maxValue)) : kInvalidValueColor);
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
    out.geometryRevision = geometryHash(out);
    return out;
}

} // namespace Structura::Shell
