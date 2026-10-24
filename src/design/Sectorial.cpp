#include "design/Sectorial.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace Structura::Design {

namespace {

using Pt = std::array<double, 2>;

struct Props {
    double area {0.0};
    double ca {0.0}, cb {0.0};         // centroide (coordenadas do desenho)
    double ix {0.0}, iy {0.0}, ixy {0.0};   // em relacao ao centroide: int b^2, int a^2, int a b
    double aMid {0.0}, bMid {0.0};     // centro da caixa envolvente em relacao ao centroide
};

Props propsOf(const std::vector<Pt> &pts, const CenterLine &line)
{
    Props p;
    double sa = 0.0, sb = 0.0;
    for (const auto &s : line.segments) {
        const Pt &p1 = pts[static_cast<std::size_t>(s.first)];
        const Pt &p2 = pts[static_cast<std::size_t>(s.second)];
        const double l = std::hypot(p2[0] - p1[0], p2[1] - p1[1]);
        const double w = s.t * l;
        p.area += w;
        sa += w * 0.5 * (p1[0] + p2[0]);
        sb += w * 0.5 * (p1[1] + p2[1]);
    }
    p.ca = sa / p.area;
    p.cb = sb / p.area;
    double amin = 1e300, amax = -1e300, bmin = 1e300, bmax = -1e300;
    for (const auto &q : pts) {
        amin = std::min(amin, q[0]);
        amax = std::max(amax, q[0]);
        bmin = std::min(bmin, q[1]);
        bmax = std::max(bmax, q[1]);
    }
    p.aMid = 0.5 * (amin + amax) - p.ca;
    p.bMid = 0.5 * (bmin + bmax) - p.cb;
    for (const auto &s : line.segments) {
        const double a1 = pts[static_cast<std::size_t>(s.first)][0] - p.ca, b1 = pts[static_cast<std::size_t>(s.first)][1] - p.cb;
        const double a2 = pts[static_cast<std::size_t>(s.second)][0] - p.ca, b2 = pts[static_cast<std::size_t>(s.second)][1] - p.cb;
        const double l = std::hypot(a2 - a1, b2 - b1);
        const double w = s.t * l;
        p.ix += w * (b1 * b1 + b1 * b2 + b2 * b2) / 3.0;
        p.iy += w * (a1 * a1 + a1 * a2 + a2 * a2) / 3.0;
        p.ixy += w * (2.0 * a1 * b1 + a1 * b2 + a2 * b1 + 2.0 * a2 * b2) / 6.0;
    }
    return p;
}

} // namespace

SectorialData computeSectorial(const CenterLine &canonical, const GrossSection &drawn, double angleDeg)
{
    SectorialData out;
    if (canonical.points.empty() || canonical.segments.empty() || !drawn.valid) {
        return out;
    }
    // 1) reflexao que reproduz a secao desenhada
    const double width = std::max(drawn.aMax - drawn.aMin, 1e-9), height = std::max(drawn.bMax - drawn.bMin, 1e-9);
    const double inertiaScale = std::sqrt(std::max(drawn.Ix * drawn.Iy, 1e-30));
    double bestScore = 1e300;
    std::vector<Pt> best;
    for (const double sa : {1.0, -1.0}) {
        for (const double sb : {1.0, -1.0}) {
            std::vector<Pt> pts = canonical.points;
            for (auto &q : pts) {
                q[0] *= sa;
                q[1] *= sb;
            }
            const Props p = propsOf(pts, canonical);
            // a orientacao e decidida pelo produto de inercia e pela posicao do centroide na caixa envolvente; as inercias so servem de verificacao
            // grosseira (a linha media nao reproduz raios de concordancia nem mesas inclinadas)
            const double score = std::abs(p.ixy - drawn.Ixy) / inertiaScale + std::abs(p.aMid - 0.5 * (drawn.aMin + drawn.aMax)) / width +
                                 std::abs(p.bMid - 0.5 * (drawn.bMin + drawn.bMax)) / height;
            const bool plausible = p.ix > 0.5 * drawn.Ix && p.ix < 2.0 * drawn.Ix && p.iy > 0.5 * drawn.Iy && p.iy < 2.0 * drawn.Iy;
            if (!plausible) {
                continue;
            }
            if (score < bestScore - 1e-12) {
                bestScore = score;
                best = pts;
            }
        }
    }
    out.matchScore = bestScore;
    if (best.empty() || bestScore > 0.5) {
        return out;   // a linha media nao representa a secao (espessuras variaveis, raios grandes...)
    }

    // 2) giro ate os eixos principais, centroide na origem e eixos locais (y = b, z = -a)
    const double c = std::cos(angleDeg * kPi / 180.0), sn = std::sin(angleDeg * kPi / 180.0);
    for (auto &q : best) {
        const double a = q[0], b = q[1];
        q[0] = -sn * b + c * a;
        q[1] = c * b + sn * a;
    }
    const Props pr = propsOf(best, canonical);
    std::vector<Pt> local(best.size());   // (y, z)
    for (std::size_t i = 0; i < best.size(); ++i) {
        local[i] = {best[i][1] - pr.cb, -(best[i][0] - pr.ca)};
    }
    const std::size_t n = local.size();

    // 3) arvore: raiz no vertice 0
    std::vector<std::vector<int>> adjacent(n);
    for (std::size_t k = 0; k < canonical.segments.size(); ++k) {
        adjacent[static_cast<std::size_t>(canonical.segments[k].first)].push_back(static_cast<int>(k));
        adjacent[static_cast<std::size_t>(canonical.segments[k].second)].push_back(static_cast<int>(k));
    }
    struct Edge {
        int parent {0}, child {0};
        double t {0.0}, length {0.0};
    };
    std::vector<Edge> edges;
    std::vector<int> order;   // vertices na ordem de visita
    std::vector<int> parentEdge(n, -1);
    std::vector<bool> seen(n, false);
    order.push_back(0);
    seen[0] = true;
    for (std::size_t head = 0; head < order.size(); ++head) {
        const int v = order[head];
        for (const int k : adjacent[static_cast<std::size_t>(v)]) {
            const auto &s = canonical.segments[static_cast<std::size_t>(k)];
            const int other = s.first == v ? s.second : s.first;
            if (seen[static_cast<std::size_t>(other)]) {
                continue;
            }
            seen[static_cast<std::size_t>(other)] = true;
            Edge e;
            e.parent = v;
            e.child = other;
            e.t = s.t;
            e.length = std::hypot(local[static_cast<std::size_t>(other)][0] - local[static_cast<std::size_t>(v)][0], local[static_cast<std::size_t>(other)][1] - local[static_cast<std::size_t>(v)][1]);
            parentEdge[static_cast<std::size_t>(other)] = static_cast<int>(edges.size());
            edges.push_back(e);
            order.push_back(other);
        }
    }
    if (order.size() != n || edges.size() != canonical.segments.size()) {
        return out;   // nao e uma arvore (secao fechada ou desconexa)
    }

    // 4) coordenada setorial com polo no centroide
    std::vector<double> omega(n, 0.0);
    for (const int v : order) {
        if (parentEdge[static_cast<std::size_t>(v)] < 0) {
            continue;
        }
        const Edge &e = edges[static_cast<std::size_t>(parentEdge[static_cast<std::size_t>(v)])];
        const auto &pi = local[static_cast<std::size_t>(e.parent)];
        const auto &pj = local[static_cast<std::size_t>(e.child)];
        omega[static_cast<std::size_t>(v)] = omega[static_cast<std::size_t>(e.parent)] + (pi[0] * pj[1] - pj[0] * pi[1]);
    }
    // integrais de produtos de funcoes lineares (Simpson, exato para quadraticas)
    auto product = [&](const Edge &e, const std::vector<double> &f, const std::vector<double> &g) {
        const double f1 = f[static_cast<std::size_t>(e.parent)], f2 = f[static_cast<std::size_t>(e.child)];
        const double g1 = g[static_cast<std::size_t>(e.parent)], g2 = g[static_cast<std::size_t>(e.child)];
        return e.t * e.length * (f1 * g1 + 4.0 * 0.5 * (f1 + f2) * 0.5 * (g1 + g2) + f2 * g2) / 6.0;
    };
    std::vector<double> yv(n), zv(n);
    for (std::size_t i = 0; i < n; ++i) {
        yv[i] = local[i][0];
        zv[i] = local[i][1];
    }
    double area = 0.0, iz = 0.0, iy = 0.0, iyz = 0.0, iwy = 0.0, iwz = 0.0;
    for (const Edge &e : edges) {
        area += e.t * e.length;
        iz += product(e, yv, yv);
        iy += product(e, zv, zv);
        iyz += product(e, yv, zv);
        iwy += product(e, omega, yv);
        iwz += product(e, omega, zv);
    }
    // centro de cisalhamento: int(omega_S y) = int(omega_S z) = 0 com omega_S = omega - ys z + zs y (vale tambem com pequeno produto de inercia)
    const double determinant = -iz * iy + iyz * iyz;
    if (std::abs(determinant) < 1e-30) {
        return out;
    }
    const double zs = (iwy * iy - iyz * iwz) / determinant;
    const double ys = (-iz * iwz + iyz * iwy) / determinant;
    std::vector<double> omegaS(n);
    double meanNumerator = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        omegaS[i] = omega[i] - ys * zv[i] + zs * yv[i];
    }
    for (const Edge &e : edges) {
        meanNumerator += e.t * e.length * 0.5 * (omegaS[static_cast<std::size_t>(e.parent)] + omegaS[static_cast<std::size_t>(e.child)]);
    }
    const double mean = meanNumerator / area;
    std::vector<double> omegaN(n);
    for (std::size_t i = 0; i < n; ++i) {
        omegaN[i] = omegaS[i] - mean;
    }
    double cw = 0.0;
    for (const Edge &e : edges) {
        cw += product(e, omegaN, omegaN);
    }

    // 5) momentos estaticos da parte alem de cada ponto (a favor da arvore, a partir da raiz)
    struct Sub {
        double sy {0.0}, sz {0.0}, sw {0.0};
    };
    std::vector<Sub> subtree(n);
    auto edgeIntegral = [&](const Edge &e, const std::vector<double> &f) { return e.t * e.length * 0.5 * (f[static_cast<std::size_t>(e.parent)] + f[static_cast<std::size_t>(e.child)]); };
    for (std::size_t k = order.size(); k-- > 1;) {
        const int v = order[k];
        const Edge &e = edges[static_cast<std::size_t>(parentEdge[static_cast<std::size_t>(v)])];
        Sub &parent = subtree[static_cast<std::size_t>(e.parent)];
        const Sub &child = subtree[static_cast<std::size_t>(v)];
        parent.sy += child.sy + edgeIntegral(e, yv);
        parent.sz += child.sz + edgeIntegral(e, zv);
        parent.sw += child.sw + edgeIntegral(e, omegaN);
    }
    out.Cw = cw;
    out.ys = ys;
    out.zs = zs;
    for (const double w : omegaN) {
        out.omegaMax = std::max(out.omegaMax, std::abs(w));
    }
    constexpr int kSamples = 9;
    for (const Edge &e : edges) {
        const Sub &child = subtree[static_cast<std::size_t>(e.child)];
        const double y1 = yv[static_cast<std::size_t>(e.parent)], y2 = yv[static_cast<std::size_t>(e.child)];
        const double z1 = zv[static_cast<std::size_t>(e.parent)], z2 = zv[static_cast<std::size_t>(e.child)];
        const double w1 = omegaN[static_cast<std::size_t>(e.parent)], w2 = omegaN[static_cast<std::size_t>(e.child)];
        const double aligned = e.length > 0.0 ? std::abs(y2 - y1) / e.length : 0.0;
        for (int k = 0; k < kSamples; ++k) {
            const double u = static_cast<double>(k) / (kSamples - 1);
            SectorialData::Sample s;
            s.y = y1 + (y2 - y1) * u;
            s.z = z1 + (z2 - z1) * u;
            s.omega = w1 + (w2 - w1) * u;
            // parte do segmento de u ate o fim (lado do filho) mais a subarvore do filho
            auto tail = [&](double q1, double q2) { return e.t * e.length * (q1 * (1.0 - u) + (q2 - q1) * (1.0 - u * u) / 2.0); };
            s.sy = tail(y1, y2) + child.sy;
            s.sz = tail(z1, z2) + child.sz;
            s.sw = tail(w1, w2) + child.sw;
            s.t = e.t;
            s.alignedY = aligned;
            out.samples.push_back(s);
        }
    }
    out.valid = cw >= 0.0;
    return out;
}

} // namespace Structura::Design
