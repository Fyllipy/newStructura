#include "app/services/WindDynamicsService.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace Structura::App::Services {

double WindDynamicsService::exponentP(int category)
{
    static const double table[] = {0.095, 0.15, 0.185, 0.23, 0.31};
    return table[std::clamp(category, 0, 4)];
}

double WindDynamicsService::factorBm(int category)
{
    static const double table[] = {1.23, 1.00, 0.86, 0.71, 0.50};
    return table[std::clamp(category, 0, 4)];
}

WindDynamicsService::Result WindDynamicsService::compute(const DynamicsService::Basis &basis, const Input &in)
{
    Result r;
    if (!basis.model || !basis.modal || in.mode < 0 || static_cast<std::size_t>(in.mode) >= basis.modal->modalShapes.size()) {
        r.message = QStringLiteral("Calcule os modos de vibração antes.");
        return r;
    }
    const auto &nodes = basis.model->nodes;
    const auto &shape = basis.modal->modalShapes[static_cast<std::size_t>(in.mode)];
    r.frequency = basis.modal->modalOmega[static_cast<std::size_t>(in.mode)] / (2.0 * 3.14159265358979323846);
    r.p = exponentP(in.category);
    r.bm = factorBm(in.category);
    r.vp = 0.69 * in.v0 * in.s1 * in.s3;
    r.q0 = 0.613 * r.vp * r.vp / 1000.0;

    double zMin = nodes.empty() ? 0.0 : nodes.front().z;
    for (const auto &n : nodes) {
        zMin = std::min(zMin, n.z);
    }
    std::map<long, std::vector<std::size_t>> byLevel;   // cota em mm -> nos
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].z - zMin > 0.01) {
            byLevel[std::lround(nodes[i].z * 100.0)].push_back(i);
        }
    }
    if (byLevel.size() < 2) {
        r.message = QStringLiteral("São necessários ao menos dois níveis acima da base.");
        return r;
    }
    const int dir = std::clamp(in.direction, 0, 1);
    for (const auto &[key, list] : byLevel) {
        Level lv;
        lv.z = static_cast<double>(key) / 100.0 - zMin;
        double weighted = 0.0, plain = 0.0;
        for (const std::size_t i : list) {
            const double m = i < basis.mass.size() ? basis.mass[i] : 0.0;
            const double phi = i * 6 + static_cast<std::size_t>(dir) < shape.size() ? shape[i * 6 + static_cast<std::size_t>(dir)] : 0.0;
            lv.mass += m;
            weighted += m * phi;
            plain += phi;
        }
        lv.shape = lv.mass > 0.0 ? weighted / lv.mass : plain / static_cast<double>(list.size());
        lv.nodes = list;
        r.levels.push_back(std::move(lv));
    }
    // area de influencia: metade das alturas dos pavimentos vizinhos
    const std::size_t n = r.levels.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double below = i == 0 ? r.levels[0].z : r.levels[i].z - r.levels[i - 1].z;
        const double above = i + 1 < n ? r.levels[i + 1].z - r.levels[i].z : below;
        r.levels[i].area = in.width * 0.5 * (below + above);
    }
    // sinal: topo positivo
    if (r.levels.back().shape < 0.0) {
        for (auto &lv : r.levels) {
            lv.shape = -lv.shape;
        }
    }
    double m0 = 0.0, a0 = 0.0;
    for (const auto &lv : r.levels) {
        m0 = std::max(m0, lv.mass);
        a0 = std::max(a0, lv.area);
    }
    if (m0 <= 0.0 || a0 <= 0.0) {
        r.message = QStringLiteral("Massas nodais nulas: defina cargas gravitacionais (massa) ou inclua a massa dos materiais.");
        return r;
    }
    double num = 0.0, den = 0.0;
    for (const auto &lv : r.levels) {
        const double beta = in.drag * (lv.area / a0) * std::pow(lv.z / 10.0, r.p);
        num += beta * lv.shape;
        den += lv.mass / m0 * lv.shape * lv.shape;
    }
    if (den <= 0.0) {
        r.message = QStringLiteral("A forma modal não tem deslocamento na direção do vento.");
        return r;
    }
    r.fh = r.q0 * r.bm * r.bm * a0 * num / den * in.xi;
    r.along.assign(nodes.size(), {0.0, 0.0, 0.0});
    r.cross.assign(nodes.size(), {0.0, 0.0, 0.0});
    const int crossDir = 1 - dir;
    for (auto &lv : r.levels) {
        lv.mean = in.fluctuatingOnly ? 0.0 : r.q0 * r.bm * r.bm * in.drag * lv.area * std::pow(lv.z / 10.0, 2.0 * r.p);
        lv.fluctuating = r.fh * lv.mass / m0 * lv.shape;
        r.baseShearMean += lv.mean;
        r.baseShearFluctuating += lv.fluctuating;
        const double total = lv.mean + lv.fluctuating;
        for (const std::size_t i : lv.nodes) {
            const double share = lv.mass > 0.0 && i < basis.mass.size() ? basis.mass[i] / lv.mass : 1.0 / static_cast<double>(lv.nodes.size());
            r.along[i][static_cast<std::size_t>(dir)] = total * share;
            r.cross[i][static_cast<std::size_t>(crossDir)] = total * share * in.crossFactor;
        }
    }
    r.ok = true;
    return r;
}

} // namespace Structura::App::Services
