#include "AreaLoadService.h"

#include <algorithm>
#include <cmath>

namespace Structura::App::Services {

namespace {

using Vec3 = AreaLoadService::Vec3;

Vec3 sub(const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double dot(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3 &a, const Vec3 &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double norm(const Vec3 &a) { return std::sqrt(dot(a, a)); }
Vec3 unit(const Vec3 &a)
{
    const double n = norm(a);
    return n > 1e-12 ? Vec3 {a[0] / n, a[1] / n, a[2] / n} : Vec3 {0, 0, 0};
}

} // namespace

void AreaLoadService::meteorologicalParameters(int category, char cls, double &b, double &fr, double &p)
{
    static const double bTable[5] = {1.10, 1.00, 0.94, 0.86, 0.74};
    static const double pTable[5][3] = {{0.06, 0.065, 0.07}, {0.085, 0.09, 0.10}, {0.10, 0.105, 0.115}, {0.12, 0.125, 0.135}, {0.15, 0.16, 0.175}};
    static const double frTable[3] = {1.00, 0.98, 0.95};
    const int c = std::clamp(category, 1, 5) - 1;
    const int k = cls == 'A' ? 0 : (cls == 'C' ? 2 : 1);
    b = bTable[c];
    p = pTable[c][k];
    fr = frTable[k];
}

double AreaLoadService::s2(const WindParameters &w)
{
    double b, fr, p;
    meteorologicalParameters(w.category, w.cls, b, fr, p);
    return b * fr * std::pow(std::max(w.height, 1.0) / 10.0, p);
}

double AreaLoadService::characteristicSpeed(const WindParameters &w)
{
    return w.v0 * w.s1 * s2(w) * w.s3;
}

double AreaLoadService::dynamicPressure(const WindParameters &w)
{
    const double vk = characteristicSpeed(w);
    return 0.613 * vk * vk / 1000.0;
}

AreaLoadService::Vec3 AreaLoadService::surfaceNormal(const QVector<BarSegment> &bars)
{
    // maior produto vetorial entre as direcoes das barras
    Vec3 best {0, 0, 0};
    double bestNorm = 0.0;
    for (int i = 0; i < bars.size(); ++i) {
        const Vec3 ui = unit(sub(bars[i].b, bars[i].a));
        for (int j = i + 1; j < bars.size(); ++j) {
            const Vec3 c = cross(ui, unit(sub(bars[j].b, bars[j].a)));
            if (norm(c) > bestNorm) {
                bestNorm = norm(c);
                best = c;
            }
        }
    }
    return bestNorm > 0.05 ? unit(best) : Vec3 {0, 0, 0};
}

QVector<AreaLoadService::Panel> AreaLoadService::panels(const QVector<BarSegment> &bars, const Options &options)
{
    QVector<Panel> result;
    if (bars.isEmpty()) {
        return result;
    }
    const Vec3 d = unit(options.direction);
    const Vec3 normal = surfaceNormal(bars);
    struct Item {
        int index;
        double t {0.0};
        Vec3 a, b;
    };
    std::vector<std::vector<Item>> groups;
    std::vector<Vec3> axes;
    for (int i = 0; i < bars.size(); ++i) {
        Vec3 u = unit(sub(bars[i].b, bars[i].a));
        bool placed = false;
        for (std::size_t g = 0; g < groups.size(); ++g) {
            const double c = dot(axes[g], u);
            if (std::abs(c) > std::cos(5.0 * 3.14159265358979 / 180.0)) {
                Item item {i, 0.0, bars[i].a, bars[i].b};
                if (c < 0.0) {
                    std::swap(item.a, item.b);   // orienta no sentido do grupo
                }
                groups[g].push_back(item);
                placed = true;
                break;
            }
        }
        if (!placed) {
            groups.push_back({{i, 0.0, bars[i].a, bars[i].b}});
            axes.push_back(u);
        }
    }
    for (std::size_t g = 0; g < groups.size(); ++g) {
        auto &group = groups[g];
        const Vec3 u = axes[g];
        Vec3 n = normal;
        if (norm(n) < 1e-9) {
            const Vec3 perp {d[0] - dot(d, u) * u[0], d[1] - dot(d, u) * u[1], d[2] - dot(d, u) * u[2]};
            n = unit(perp);
        }
        const Vec3 w = unit(cross(n, u));
        for (auto &item : group) {
            const Vec3 mid {0.5 * (item.a[0] + item.b[0]), 0.5 * (item.a[1] + item.b[1]), 0.5 * (item.a[2] + item.b[2])};
            item.t = dot(mid, w);
        }
        std::sort(group.begin(), group.end(), [](const Item &x, const Item &y) { return x.t < y.t; });
        Panel panel;
        for (const auto &item : group) {
            panel.members.append(bars[item.index].id);
        }
        auto shifted = [&](const Vec3 &p, double amount) { return Vec3 {p[0] + w[0] * amount, p[1] + w[1] * amount, p[2] + w[2] * amount}; };
        double lowOffset = -options.overhang;
        double highOffset = options.overhang;
        if (group.size() == 1) {
            const double half = 0.5 * (options.manualWidth > 0.0 ? options.manualWidth : 1.0);
            lowOffset = -half;
            highOffset = half;
        }
        const Item &first = group.front();
        const Item &last = group.back();
        panel.polygon = {shifted(first.a, lowOffset), shifted(first.b, lowOffset), shifted(last.b, highOffset), shifted(last.a, highOffset)};
        result.append(panel);
    }
    return result;
}

QVector<AreaLoadService::BarLoad> AreaLoadService::distribute(const QVector<BarSegment> &bars, const Options &options)
{
    QVector<BarLoad> result;
    if (bars.isEmpty()) {
        return result;
    }
    const Vec3 d = unit(options.direction);
    Vec3 normal = surfaceNormal(bars);

    // grupos de barras paralelas
    struct Item {
        int index;
        Vec3 u;
        double t {0.0};
    };
    std::vector<std::vector<Item>> groups;
    for (int i = 0; i < bars.size(); ++i) {
        const Vec3 u = unit(sub(bars[i].b, bars[i].a));
        bool placed = false;
        for (auto &g : groups) {
            if (std::abs(dot(g.front().u, u)) > std::cos(5.0 * 3.14159265358979 / 180.0)) {
                g.push_back({i, u, 0.0});
                placed = true;
                break;
            }
        }
        if (!placed) {
            groups.push_back({{i, u, 0.0}});
        }
    }

    result.resize(bars.size());
    for (auto &g : groups) {
        const Vec3 u = g.front().u;
        // normal da superficie para este grupo: a global, ou a componente de `d` perpendicular a barra
        Vec3 n = normal;
        if (norm(n) < 1e-9) {
            Vec3 perp {d[0] - dot(d, u) * u[0], d[1] - dot(d, u) * u[1], d[2] - dot(d, u) * u[2]};
            n = unit(perp);
        }
        const Vec3 w = unit(cross(n, u));   // dentro da superficie, perpendicular a barra
        for (auto &item : g) {
            const Vec3 mid {0.5 * (bars[item.index].a[0] + bars[item.index].b[0]), 0.5 * (bars[item.index].a[1] + bars[item.index].b[1]),
                            0.5 * (bars[item.index].a[2] + bars[item.index].b[2])};
            item.t = dot(mid, w);
        }
        std::sort(g.begin(), g.end(), [](const Item &a, const Item &b) { return a.t < b.t; });
        const std::size_t count = g.size();
        for (std::size_t k = 0; k < count; ++k) {
            double width = options.manualWidth;
            if (width <= 0.0) {
                if (count == 1) {
                    width = 1.0;   // sem vizinhas: 1 m (informe a largura manualmente)
                } else {
                    const double before = k == 0 ? 0.0 : (g[k].t - g[k - 1].t);
                    const double after = k + 1 == count ? 0.0 : (g[k + 1].t - g[k].t);
                    width = 0.5 * before + 0.5 * after;
                    if (k == 0 || k + 1 == count) {
                        width += options.overhang;
                    }
                }
            }
            double factor = 1.0;
            if (options.projected && norm(n) > 1e-9) {
                factor = std::abs(dot(n, d));
            }
            const double magnitude = options.pressure * width * factor;
            BarLoad load;
            load.id = bars[g[k].index].id;
            load.q = {magnitude * d[0], magnitude * d[1], magnitude * d[2]};
            load.width = width;
            result[g[k].index] = load;
        }
    }
    return result;
}

} // namespace Structura::App::Services
