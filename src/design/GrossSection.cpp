#include "design/GrossSection.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design {

namespace {

struct Moments {
    double area {0.0};
    double sa {0.0};   ///< integral de a dA
    double sb {0.0};   ///< integral de b dA
    double iaa {0.0};  ///< integral de a^2 dA
    double ibb {0.0};  ///< integral de b^2 dA
    double iab {0.0};
};

double signedArea(const Loop &loop)
{
    double twice = 0.0;
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const auto &p = loop[i];
        const auto &q = loop[(i + 1) % loop.size()];
        twice += p[0] * q[1] - q[0] * p[1];
    }
    return twice / 2.0;
}

/// Momentos de area de um poligono (positivo se anti-horario), pelo teorema de Green.
Moments polygonMoments(const Loop &loop)
{
    Moments m;
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const auto &p = loop[i];
        const auto &q = loop[(i + 1) % loop.size()];
        const double cross = p[0] * q[1] - q[0] * p[1];
        m.area += cross / 2.0;
        m.sa += cross * (p[0] + q[0]) / 6.0;
        m.sb += cross * (p[1] + q[1]) / 6.0;
        m.iaa += cross * (p[0] * p[0] + p[0] * q[0] + q[0] * q[0]) / 12.0;
        m.ibb += cross * (p[1] * p[1] + p[1] * q[1] + q[1] * q[1]) / 12.0;
        m.iab += cross * (p[0] * q[1] + 2.0 * p[0] * p[1] + 2.0 * q[0] * q[1] + q[0] * p[1]) / 24.0;
    }
    return m;
}

/// Recorta o poligono pelo semiplano `coordinate >= limit` (axis 0 = a, axis 1 = b) ou `<= limit` (keepAbove=false).
Loop clip(const Loop &loop, int axis, double limit, bool keepAbove)
{
    Loop out;
    const std::size_t n = loop.size();
    auto inside = [&](const std::array<double, 2> &p) {
        return keepAbove ? p[axis] >= limit : p[axis] <= limit;
    };
    for (std::size_t i = 0; i < n; ++i) {
        const auto &cur = loop[i];
        const auto &nxt = loop[(i + 1) % n];
        const bool curIn = inside(cur);
        const bool nxtIn = inside(nxt);
        if (curIn) {
            out.push_back(cur);
        }
        if (curIn != nxtIn) {
            const double t = (limit - cur[axis]) / (nxt[axis] - cur[axis]);
            out.push_back({cur[0] + t * (nxt[0] - cur[0]), cur[1] + t * (nxt[1] - cur[1])});
        }
    }
    return out;
}

/// Area e momento de area (em relacao a `limit`) da parte da secao com coordenada > limit.
void sideProperties(const std::vector<Loop> &loops, const std::vector<double> &signs, int axis, double limit,
                    bool above, double &area, double &moment)
{
    area = 0.0;
    moment = 0.0;
    for (std::size_t i = 0; i < loops.size(); ++i) {
        const Loop part = clip(loops[i], axis, limit, above);
        if (part.size() < 3) {
            continue;
        }
        const Moments m = polygonMoments(part);
        const double sa = (axis == 0 ? m.sa : m.sb);
        // area e integral de |coord - limit| dA = |sa - limit * area| (invariantes a orientacao do poligono)
        area += signs[i] * std::abs(m.area);
        moment += signs[i] * std::abs(sa - limit * m.area);
    }
}

double plasticModulus(const std::vector<Loop> &loops, const std::vector<double> &signs, int axis, double lo, double hi,
                      double totalArea, double &pna)
{
    double a = lo;
    double b = hi;
    for (int it = 0; it < 100; ++it) {
        const double mid = 0.5 * (a + b);
        double areaAbove = 0.0;
        double moment = 0.0;
        sideProperties(loops, signs, axis, mid, true, areaAbove, moment);
        if (areaAbove > 0.5 * totalArea) {
            a = mid;
        } else {
            b = mid;
        }
    }
    pna = 0.5 * (a + b);
    double areaAbove = 0.0;
    double momentAbove = 0.0;
    double areaBelow = 0.0;
    double momentBelow = 0.0;
    sideProperties(loops, signs, axis, pna, true, areaAbove, momentAbove);
    sideProperties(loops, signs, axis, pna, false, areaBelow, momentBelow);
    return momentAbove + momentBelow;
}

} // namespace

GrossSection computeGrossSection(const std::vector<Loop> &input, std::size_t outerCount)
{
    GrossSection g;
    if (input.empty()) {
        return g;
    }

    std::vector<Loop> loops = input;
    std::vector<double> signs(loops.size(), 1.0);
    Moments total;
    for (std::size_t i = 0; i < loops.size(); ++i) {
        signs[i] = (i < outerCount) ? 1.0 : -1.0;
        const Moments m = polygonMoments(loops[i]);
        const double orient = signedArea(loops[i]) >= 0.0 ? 1.0 : -1.0;
        const double s = signs[i] * orient;
        total.area += s * m.area;
        total.sa += s * m.sa;
        total.sb += s * m.sb;
        total.iaa += s * m.iaa;
        total.ibb += s * m.ibb;
        total.iab += s * m.iab;
    }
    if (total.area <= 0.0) {
        return g;
    }

    const double ca = total.sa / total.area;
    const double cb = total.sb / total.area;
    for (auto &loop : loops) {
        for (auto &p : loop) {
            p[0] -= ca;
            p[1] -= cb;
        }
    }
    g.centroidA = ca;
    g.centroidB = cb;
    g.area = total.area;
    g.Ix = total.ibb - total.area * cb * cb;
    g.Iy = total.iaa - total.area * ca * ca;
    g.Ixy = total.iab - total.area * ca * cb;

    bool first = true;
    for (std::size_t li = 0; li < std::max<std::size_t>(1, outerCount) && li < loops.size(); ++li) {
        for (const auto &p : loops[li]) {
            if (first) {
                g.aMin = g.aMax = p[0];
                g.bMin = g.bMax = p[1];
                first = false;
            } else {
                g.aMin = std::min(g.aMin, p[0]);
                g.aMax = std::max(g.aMax, p[0]);
                g.bMin = std::min(g.bMin, p[1]);
                g.bMax = std::max(g.bMax, p[1]);
            }
        }
    }
    g.WxTop = g.Ix / std::abs(g.bMax);
    g.WxBottom = g.Ix / std::abs(g.bMin);
    g.WyRight = g.Iy / std::abs(g.aMax);
    g.WyLeft = g.Iy / std::abs(g.aMin);
    g.rx = std::sqrt(g.Ix / g.area);
    g.ry = std::sqrt(g.Iy / g.area);
    g.Zx = plasticModulus(loops, signs, 1, g.bMin, g.bMax, g.area, g.pnaX);
    g.Zy = plasticModulus(loops, signs, 0, g.aMin, g.aMax, g.area, g.pnaY);
    g.valid = true;
    return g;
}

} // namespace Structura::Design
