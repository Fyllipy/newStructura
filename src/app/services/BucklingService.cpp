#include "BucklingService.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Structura::App::Services {

using Vec3 = BucklingService::Vec3;

namespace {
Vec3 sub(const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 add(const Vec3 &a, const Vec3 &b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 mul(const Vec3 &a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3 &a, const Vec3 &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
} // namespace

SolverBridge::Result BucklingService::run(SolverBridge::ModelData model, int modes)
{
    model.secondOrder = false;
    model.bucklingModes = std::max(1, modes);
    SolverBridge solver;
    return solver.solve(model);
}

double BucklingService::maxTranslation(const std::vector<double> &shape)
{
    double best = 0.0;
    for (std::size_t i = 0; i + 2 < shape.size(); i += 6) {
        best = std::max(best, std::sqrt(shape[i] * shape[i] + shape[i + 1] * shape[i + 1] + shape[i + 2] * shape[i + 2]));
    }
    return best;
}

std::vector<std::vector<Vec3>> BucklingService::shapeLines(const SolverBridge::ModelData &model, const std::vector<double> &shape,
                                                           double maxDisplacement, int samples)
{
    std::vector<std::vector<Vec3>> lines;
    if (shape.size() < model.nodes.size() * 6U) {
        return lines;
    }
    const double peak = maxTranslation(shape);
    const double scale = peak > 1e-12 ? maxDisplacement / peak : 0.0;
    std::unordered_map<int, std::size_t> index;
    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        index[model.nodes[i].id] = i;
    }
    for (const auto &member : model.members) {
        const auto ii = index.find(member.nodeI);
        const auto jj = index.find(member.nodeJ);
        if (ii == index.end() || jj == index.end()) {
            continue;
        }
        const auto &a = model.nodes[ii->second];
        const auto &b = model.nodes[jj->second];
        const Vec3 pi {a.x, a.y, a.z}, pj {b.x, b.y, b.z};
        const Vec3 chord = sub(pj, pi);
        const double length = std::sqrt(dot(chord, chord));
        if (length < 1e-12) {
            continue;
        }
        const Vec3 e = mul(chord, 1.0 / length);
        const std::size_t oi = ii->second * 6, oj = jj->second * 6;
        const Vec3 ui = mul({shape[oi], shape[oi + 1], shape[oi + 2]}, scale);
        const Vec3 uj = mul({shape[oj], shape[oj + 1], shape[oj + 2]}, scale);
        // rotacoes escalam junto com as translacoes (mesmo fator), mantendo a forma consistente
        const Vec3 ti = mul({shape[oi + 3], shape[oi + 4], shape[oi + 5]}, scale);
        const Vec3 tj = mul({shape[oj + 3], shape[oj + 4], shape[oj + 5]}, scale);
        const Vec3 du = sub(uj, ui);
        const Vec3 chordSlope = mul(sub(du, mul(e, dot(du, e))), 1.0 / length);
        const Vec3 si = sub(cross(ti, e), chordSlope);
        const Vec3 sj = sub(cross(tj, e), chordSlope);
        std::vector<Vec3> line;
        for (int s = 0; s <= samples; ++s) {
            const double t = static_cast<double>(s) / samples;
            Vec3 point = add(pi, mul(chord, t));
            point = add(point, add(mul(ui, 1.0 - t), mul(uj, t)));
            const double h1 = t - 2.0 * t * t + t * t * t;
            const double h2 = -t * t + t * t * t;
            point = add(point, add(mul(si, length * h1), mul(sj, length * h2)));
            line.push_back(point);
        }
        lines.push_back(std::move(line));
    }
    return lines;
}

} // namespace Structura::App::Services
