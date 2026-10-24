#include "design/TaperedMember.h"

#include "design/Nbr8800.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Structura::Design {

using Nbr8800::fmt;

namespace {

/// Menor autovalor de uma matriz simetrica 3 x 3 (rotacoes de Jacobi).
double smallestEigenvalue(std::array<std::array<double, 3>, 3> a)
{
    for (int sweep = 0; sweep < 60; ++sweep) {
        double off = 0.0;
        for (int i = 0; i < 3; ++i) {
            for (int j = i + 1; j < 3; ++j) {
                off += a[i][j] * a[i][j];
            }
        }
        if (off < 1e-24 * (std::abs(a[0][0]) + std::abs(a[1][1]) + std::abs(a[2][2]) + 1e-300)) {
            break;
        }
        for (int p = 0; p < 3; ++p) {
            for (int q = p + 1; q < 3; ++q) {
                if (std::abs(a[p][q]) < 1e-300) {
                    continue;
                }
                const double theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
                const double t = (theta >= 0.0 ? 1.0 : -1.0) / (std::abs(theta) + std::sqrt(theta * theta + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0), s = t * c;
                for (int k = 0; k < 3; ++k) {
                    const double akp = a[k][p], akq = a[k][q];
                    a[k][p] = c * akp - s * akq;
                    a[k][q] = s * akp + c * akq;
                }
                for (int k = 0; k < 3; ++k) {
                    const double apk = a[p][k], aqk = a[q][k];
                    a[p][k] = c * apk - s * aqk;
                    a[q][k] = s * apk + c * aqk;
                }
            }
        }
    }
    return std::min({a[0][0], a[1][1], a[2][2]});
}

} // namespace

double TaperedGeometry::strongInertia(double d) const
{
    const double web = std::max(d - 2.0 * tf, 0.0);
    return bf * d * d * d / 12.0 - (bf - tw) * web * web * web / 12.0;
}

double taperedEquivalentInertia(const TaperedGeometry &g, double length)
{
    if (length <= 0.0) {
        return g.strongInertia(0.5 * (g.d1 + g.d2));
    }
    // K_ij / E = int I(x) y_i'' y_j'' dx; G_ij = int y_i' y_j' dx = delta_ij (i pi/L)^2 L/2 (senos ortogonais); P = autovalor de G^-1/2 K G^-1/2
    constexpr int kIntervals = 400;
    std::array<std::array<double, 3>, 3> k {};
    const double h = length / kIntervals;
    for (int n = 0; n <= kIntervals; ++n) {
        const double x = n * h;
        const double weight = (n == 0 || n == kIntervals) ? 1.0 : (n % 2 == 1 ? 4.0 : 2.0);
        const double inertia = g.strongInertia(g.depthAt(x, length));
        std::array<double, 3> curvature {};
        for (int i = 0; i < 3; ++i) {
            const double w = (i + 1) * kPi / length;
            curvature[static_cast<std::size_t>(i)] = w * w * std::sin(w * x);
        }
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                k[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] += weight * h / 3.0 * inertia * curvature[static_cast<std::size_t>(i)] * curvature[static_cast<std::size_t>(j)];
            }
        }
    }
    std::array<std::array<double, 3>, 3> m {};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            const double gi = std::pow((i + 1) * kPi / length, 2) * length / 2.0, gj = std::pow((j + 1) * kPi / length, 2) * length / 2.0;
            m[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = k[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] / std::sqrt(gi * gj);
        }
    }
    const double p = smallestEigenvalue(m);   // P / E
    return p * length * length / (kPi * kPi);
}

BarDesignOutput designTaperedBar(const BarDesignInput &base, const TaperedGeometry &geometry, const std::vector<DesignSection> &stations, const DesignSection &smallest,
                                 const DesignSection &largest)
{
    BarDesignOutput merged;
    if (stations.empty()) {
        return merged;
    }
    const double ixEq = taperedEquivalentInertia(geometry, base.length);
    bool first = true;
    for (std::size_t i = 0; i < stations.size(); ++i) {
        BarDesignInput in = base;
        in.section = &stations[i];
        in.axialSection = &smallest;
        in.slendernessSection = &largest;
        in.onlyStation = static_cast<int>(i);
        in.cbEqualsOne = true;   // G.4.1: Cb = 1,0 (conservador, sem analise de estabilidade elastica)
        in.ixEquivalent = ixEq;
        in.webWeldLeg = 0.0;     // itens de barra prismatica de perfil soldado nao se aplicam a esta chamada por secao
        in.coverB = 0.0;
        in.plasticAnalysis = false;
        BarDesignOutput r = designBar(in);
        if (!r.supported) {
            continue;
        }
        if (first) {
            merged = r;
            first = false;
            continue;
        }
        for (CheckResult &check : r.checks) {
            const auto it = std::find_if(merged.checks.begin(), merged.checks.end(), [&](const CheckResult &c) { return c.id == check.id; });
            if (it == merged.checks.end()) {
                merged.checks.push_back(std::move(check));
            } else if (check.ratio > it->ratio) {
                *it = std::move(check);
            }
        }
        for (std::string &w : r.warnings) {
            if (std::find(merged.warnings.begin(), merged.warnings.end(), w) == merged.warnings.end()) {
                merged.warnings.push_back(std::move(w));
            }
        }
        if (r.maxRatio > merged.maxRatio) {
            merged.maxRatio = r.maxRatio;
            merged.governingCheck = r.governingCheck;
            merged.governingCombo = r.governingCombo;
            merged.governingX = r.governingX;
        }
    }
    // recalcula o maximo e a verificacao governante a partir das verificacoes reunidas
    double maxRatio = 0.0;
    for (const CheckResult &c : merged.checks) {
        if (c.ratio > maxRatio) {
            maxRatio = c.ratio;
            merged.governingCheck = c.id;
        }
    }
    merged.maxRatio = std::max(merged.maxRatio, maxRatio);
    merged.summaryLines.push_back(fmt("Barra de secao variavel (NBR 8800:2024, Anexo G): altura %.0f mm (inicio) a %.0f mm (fim), mesas %.0f x %.1f mm, alma %.1f mm; secoes verificadas uma a uma.", geometry.d1, geometry.d2,
                                      geometry.bf, geometry.tf, geometry.tw));
    merged.summaryLines.push_back(fmt("G.2/G.3: tracao e compressao com a secao de menor altura (A = %.0f mm2); flambagem em relacao ao eixo perpendicular a alma por analise racional: Ix equivalente = %.4g mm4 (Rayleigh-Ritz, 3 semiondas) contra %.4g mm4 "
                                      "da menor e %.4g mm4 da maior secao; o comprimento efetivo segue por Kx.", smallest.g.area, ixEq, smallest.g.Ix, largest.g.Ix));
    merged.summaryLines.push_back(fmt("G.4: flambagem lateral com torcao com Cb = 1,0 (conservador), parametros de esbeltez da secao de maior altura (h/tw = %.1f); o momento resistente de cada secao e comparado ao solicitante da mesma secao, o que verifica a de maior tensao nas mesas.",
                                      largest.h / std::max(largest.tw, 1e-9)));
    return merged;
}

} // namespace Structura::Design
