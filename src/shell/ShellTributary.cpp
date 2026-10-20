#include "shell/ShellTributary.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace Structura::Shell {

namespace {

using Vec2 = std::array<double, 2>;

double dot3(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 sub3(const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 cross3(const Vec3 &a, const Vec3 &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double norm3(const Vec3 &a) { return std::sqrt(dot3(a, a)); }

double segmentDistance(const Vec2 &p, const Vec2 &a, const Vec2 &b)
{
    const double dx = b[0] - a[0], dy = b[1] - a[1];
    const double len2 = dx * dx + dy * dy;
    double t = len2 > 0.0 ? ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / len2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(p[0] - (a[0] + t * dx), p[1] - (a[1] + t * dy));
}

bool inside(const std::vector<Vec2> &poly, const Vec2 &p)
{
    bool in = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        if (((poly[i][1] > p[1]) != (poly[j][1] > p[1])) && (p[0] < (poly[j][0] - poly[i][0]) * (p[1] - poly[i][1]) / (poly[j][1] - poly[i][1]) + poly[i][0])) {
            in = !in;
        }
    }
    return in;
}

} // namespace

bool surfaceForce(const Model &model, const Object &plate, const Load &load, Vec3 &force)
{
    if (plate.kind != Kind::Plate || plate.polygon.size() < 3) {
        return false;
    }
    Vec3 n {0.0, 0.0, 0.0};
    for (std::size_t i = 0; i < plate.polygon.size(); ++i) {
        const Vec3 &a = plate.polygon[i], &b = plate.polygon[(i + 1) % plate.polygon.size()];
        n[0] += (a[1] - b[1]) * (a[2] + b[2]);
        n[1] += (a[2] - b[2]) * (a[0] + b[0]);
        n[2] += (a[0] - b[0]) * (a[1] + b[1]);
    }
    const double len = norm3(n);
    if (len < 1e-12) {
        return false;
    }
    for (double &c : n) {
        c /= len;
    }
    switch (load.type) {
    case Load::Pressure: force = {n[0] * load.v[0], n[1] * load.v[0], n[2] * load.v[0]}; return true;
    case Load::SelfWeight: {
        const double gamma = model.materials[static_cast<std::size_t>(std::clamp(plate.material, 0, static_cast<int>(model.materials.size()) - 1))].unitWeight;
        force = {0.0, 0.0, -gamma * plate.param("t") * load.v[0]};
        return true;
    }
    case Load::Area: force = {load.v[0], load.v[1], load.v[2]}; return true;
    default: return false;
    }
}

std::vector<TributaryLoad> tributaryLoads(const Object &plate, const Vec3 &force, const std::vector<TributaryBar> &bars, double tol,
                                          std::vector<std::string> *warnings)
{
    std::vector<TributaryLoad> out;
    const std::size_t count = plate.polygon.size();
    if (plate.kind != Kind::Plate || count < 3) {
        return out;
    }
    // base do plano da chapa
    Vec3 normal {0.0, 0.0, 0.0};
    for (std::size_t i = 0; i < count; ++i) {
        const Vec3 &a = plate.polygon[i], &b = plate.polygon[(i + 1) % count];
        normal[0] += (a[1] - b[1]) * (a[2] + b[2]);
        normal[1] += (a[2] - b[2]) * (a[0] + b[0]);
        normal[2] += (a[0] - b[0]) * (a[1] + b[1]);
    }
    const double nlen = norm3(normal);
    if (nlen < 1e-12) {
        return out;
    }
    for (double &c : normal) {
        c /= nlen;
    }
    const Vec3 origin = plate.polygon[0];
    Vec3 e1 = sub3(plate.polygon[1], origin);
    e1 = {e1[0] - normal[0] * dot3(e1, normal), e1[1] - normal[1] * dot3(e1, normal), e1[2] - normal[2] * dot3(e1, normal)};
    const double l1 = norm3(e1);
    if (l1 < 1e-12) {
        return out;
    }
    for (double &c : e1) {
        c /= l1;
    }
    const Vec3 e2 = cross3(normal, e1);
    auto to2 = [&](const Vec3 &p) {
        const Vec3 d = sub3(p, origin);
        return Vec2 {dot3(d, e1), dot3(d, e2)};
    };
    auto planeDistance = [&](const Vec3 &p) { return std::abs(dot3(sub3(p, origin), normal)); };
    std::vector<Vec2> poly;
    for (const Vec3 &p : plate.polygon) {
        poly.push_back(to2(p));
    }
    // componente da forca ao longo da normal (pressao) e no plano: a carga das barras segue o vetor `force`
    // arestas apoiadas e cobertura de cada barra (intervalo [s0, s1] em metros ao longo da aresta)
    struct Cover {
        int bar;
        double s0, s1;
    };
    std::vector<std::vector<Cover>> covers(count);
    std::vector<double> edgeLength(count, 0.0);
    for (std::size_t i = 0; i < count; ++i) {
        const Vec2 a = poly[i], b = poly[(i + 1) % count];
        edgeLength[i] = std::hypot(b[0] - a[0], b[1] - a[1]);
    }
    for (const TributaryBar &bar : bars) {
        if (planeDistance(bar.a) > tol || planeDistance(bar.b) > tol) {
            continue;
        }
        const Vec2 pa = to2(bar.a), pb = to2(bar.b);
        for (std::size_t i = 0; i < count; ++i) {
            const Vec2 a = poly[i], b = poly[(i + 1) % count];
            if (edgeLength[i] < 1e-9) {
                continue;
            }
            const double dx = (b[0] - a[0]) / edgeLength[i], dy = (b[1] - a[1]) / edgeLength[i];
            // barra paralela e sobre a reta da aresta
            const double da = std::abs((pa[0] - a[0]) * dy - (pa[1] - a[1]) * dx);
            const double db = std::abs((pb[0] - a[0]) * dy - (pb[1] - a[1]) * dx);
            if (da > tol || db > tol) {
                continue;
            }
            double s0 = (pa[0] - a[0]) * dx + (pa[1] - a[1]) * dy;
            double s1 = (pb[0] - a[0]) * dx + (pb[1] - a[1]) * dy;
            if (s0 > s1) {
                std::swap(s0, s1);
            }
            s0 = std::max(s0, 0.0);
            s1 = std::min(s1, edgeLength[i]);
            if (s1 - s0 > 1e-6) {
                covers[i].push_back({bar.id, s0, s1});
            }
        }
    }
    std::vector<char> supported(count, 0);
    int supportedCount = 0;
    for (std::size_t i = 0; i < count; ++i) {
        supported[i] = covers[i].empty() ? 0 : 1;
        supportedCount += supported[i];
    }
    if (supportedCount == 0) {
        if (warnings) {
            warnings->push_back(plate.name + ": nenhuma barra sobre as arestas da chapa (a carga nao chega a barras).");
        }
        return out;
    }
    // profundidade do quinhao h(s) de cada aresta apoiada: marcha perpendicular a aresta ate a regiao de outra aresta apoiada
    double diagonal = 0.0;
    for (const Vec2 &p : poly) {
        for (const Vec2 &q : poly) {
            diagonal = std::max(diagonal, std::hypot(p[0] - q[0], p[1] - q[1]));
        }
    }
    std::map<int, std::pair<double, std::pair<double, double>>> barLoad;   // barra -> (soma de q*comprimento, soma comprimento) por aresta (acumulado)
    std::map<int, Vec3> perBar;
    std::map<int, double> perBarLength;
    for (std::size_t i = 0; i < count; ++i) {
        if (!supported[i]) {
            continue;
        }
        const Vec2 a = poly[i], b = poly[(i + 1) % count];
        const double len = edgeLength[i];
        const double dx = (b[0] - a[0]) / len, dy = (b[1] - a[1]) / len;
        // normal para o interior
        Vec2 inward {-dy, dx};
        {
            const Vec2 probe {0.5 * (a[0] + b[0]) + 1e-4 * inward[0], 0.5 * (a[1] + b[1]) + 1e-4 * inward[1]};
            if (!inside(poly, probe)) {
                inward = {dy, -dx};
            }
        }
        const int samples = std::clamp(static_cast<int>(len / 0.04), 40, 400);
        std::vector<double> depth(static_cast<std::size_t>(samples) + 1, 0.0);
        auto ownedBy = [&](const Vec2 &p) {
            const double own = segmentDistance(p, a, b);
            for (std::size_t j = 0; j < count; ++j) {
                if (j == i || !supported[j]) {
                    continue;
                }
                if (segmentDistance(p, poly[j], poly[(j + 1) % count]) < own - 1e-9) {
                    return false;
                }
            }
            return true;
        };
        for (int k = 0; k <= samples; ++k) {
            const double s = len * k / samples;
            const Vec2 base {a[0] + dx * s, a[1] + dy * s};
            const double step = diagonal / 600.0;
            double lo = 0.0, hi = -1.0;
            for (double tau = step; tau <= diagonal; tau += step) {
                const Vec2 p {base[0] + inward[0] * tau, base[1] + inward[1] * tau};
                if (!inside(poly, p) || !ownedBy(p)) {
                    hi = tau;
                    break;
                }
                lo = tau;
            }
            if (hi < 0.0) {
                hi = lo;
            }
            for (int it = 0; it < 24; ++it) {
                const double mid = 0.5 * (lo + hi);
                const Vec2 p {base[0] + inward[0] * mid, base[1] + inward[1] * mid};
                if (inside(poly, p) && ownedBy(p)) {
                    lo = mid;
                } else {
                    hi = mid;
                }
            }
            depth[static_cast<std::size_t>(k)] = lo;
        }
        // quinhao medio de cada barra sobre o trecho que cobre (a barra cobre [s0, s1] da aresta)
        for (const Cover &c : covers[i]) {
            double sum = 0.0, weight = 0.0;
            for (int k = 0; k < samples; ++k) {
                const double s0 = len * k / samples, s1 = len * (k + 1) / samples;
                const double lo = std::max(s0, c.s0), hi = std::min(s1, c.s1);
                if (hi <= lo) {
                    continue;
                }
                const double h = 0.5 * (depth[static_cast<std::size_t>(k)] + depth[static_cast<std::size_t>(k) + 1]);
                sum += h * (hi - lo);
                weight += hi - lo;
            }
            if (weight <= 0.0) {
                continue;
            }
            const double mean = sum / weight;   // profundidade media (m): carga linear = forca por area x profundidade
            Vec3 &q = perBar[c.bar];
            for (std::size_t k = 0; k < 3; ++k) {
                q[k] += force[k] * mean * (weight);   // acumula q*L; dividido pelo comprimento total abaixo
            }
            perBarLength[c.bar] += weight;
        }
    }
    for (auto &[id, q] : perBar) {
        const double length = perBarLength[id];
        TributaryLoad load;
        load.barId = id;
        for (std::size_t k = 0; k < 3; ++k) {
            load.q[k] = length > 0.0 ? q[k] / length : 0.0;
        }
        out.push_back(load);
    }
    return out;
}

} // namespace Structura::Shell
