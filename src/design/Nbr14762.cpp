#include "design/Nbr14762.h"

#include "design/FiniteStrip.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Structura::Design::Nbr14762 {

using Nbr8800::fmt;
using Pt = std::array<double, 2>;

namespace {

double sq(double v) { return v * v; }
std::string kn(double n) { return fmt("%.2f kN", n / 1e3); }
std::string knm(double n) { return fmt("%.2f kN*m", n / 1e6); }

constexpr double kNu = 0.3;   // coeficiente de Poisson (9.2.2.1)

// ---------------------------------------------------------------------------------------------------------------------
// Geometria de parede fina
// ---------------------------------------------------------------------------------------------------------------------

struct Frame {
    double cA {0.0}, cB {0.0}, c {1.0}, sn {0.0};
    Pt to(double a, double b) const
    {
        a -= cA;
        b -= cB;
        return {-sn * b + c * a, c * b + sn * a};
    }
};

Frame frameOf(const DesignSection &s)
{
    Frame f;
    f.cA = s.nativeCentroidA;
    f.cB = s.nativeCentroidB;
    f.c = std::cos(s.principalAngleDeg * kPi / 180.0);
    f.sn = std::sin(s.principalAngleDeg * kPi / 180.0);
    return f;
}

void gauss3(double &x0, double &x1, double &x2, double &w0, double &w1, double &w2)
{
    x0 = 0.5 - 0.5 * std::sqrt(0.6);
    x1 = 0.5;
    x2 = 0.5 + 0.5 * std::sqrt(0.6);
    w0 = 5.0 / 18.0;
    w1 = 8.0 / 18.0;
    w2 = 5.0 / 18.0;
}

/// Propriedades de torcao (centro de cisalhamento, Cw) de uma secao aberta de parede fina de espessura constante.
void openSectionTorsion(const std::vector<Pt> &nodes, double t, Pt &centroid, Pt &shearCenter, double &cw)
{
    double gx[3], gw[3];
    double dummy0, dummy1, dummy2;
    gauss3(gx[0], gx[1], gx[2], dummy0, dummy1, dummy2);
    gauss3(dummy0, dummy1, dummy2, gw[0], gw[1], gw[2]);

    const std::size_t nSeg = nodes.size() - 1;
    double area = 0.0, sx = 0.0, sy = 0.0;
    for (std::size_t k = 0; k < nSeg; ++k) {
        const double len = std::hypot(nodes[k + 1][0] - nodes[k][0], nodes[k + 1][1] - nodes[k][1]);
        area += t * len;
        sx += t * len * 0.5 * (nodes[k][0] + nodes[k + 1][0]);
        sy += t * len * 0.5 * (nodes[k][1] + nodes[k + 1][1]);
    }
    centroid = {sx / area, sy / area};

    // coordenada setorial em relacao ao polo = centroide
    std::vector<double> omega(nodes.size(), 0.0);
    for (std::size_t k = 0; k < nSeg; ++k) {
        const double x1 = nodes[k][0] - centroid[0];
        const double y1 = nodes[k][1] - centroid[1];
        const double x2 = nodes[k + 1][0] - centroid[0];
        const double y2 = nodes[k + 1][1] - centroid[1];
        omega[k + 1] = omega[k] + (x1 * y2 - x2 * y1);
    }

    double ixx = 0.0, iyy = 0.0, ixy = 0.0, iwx = 0.0, iwy = 0.0;
    for (std::size_t k = 0; k < nSeg; ++k) {
        const double len = std::hypot(nodes[k + 1][0] - nodes[k][0], nodes[k + 1][1] - nodes[k][1]);
        for (int q = 0; q < 3; ++q) {
            const double r = gx[q];
            const double x = nodes[k][0] + r * (nodes[k + 1][0] - nodes[k][0]) - centroid[0];
            const double y = nodes[k][1] + r * (nodes[k + 1][1] - nodes[k][1]) - centroid[1];
            const double w = omega[k] + r * (omega[k + 1] - omega[k]);
            const double dA = t * len * gw[q];
            ixx += y * y * dA;
            iyy += x * x * dA;
            ixy += x * y * dA;
            iwx += w * x * dA;
            iwy += w * y * dA;
        }
    }
    // omega_S = omega_P - dx*y + dy*x:  Iwx - dx*Ixy + dy*Iyy = 0 ; Iwy - dx*Ixx + dy*Ixy = 0
    const double det = (-ixy) * (ixy) - (iyy) * (-ixx);
    double dx = 0.0;
    double dy = 0.0;
    if (std::abs(det) > 1e-12 * std::abs(ixx * iyy)) {
        const double r1 = -iwx;
        const double r2 = -iwy;
        dx = (r1 * ixy - iyy * r2) / det;
        dy = ((-ixy) * r2 - (-ixx) * r1) / det;
    }
    shearCenter = {centroid[0] + dx, centroid[1] + dy};

    double aw = 0.0;   // integral de omega_S dA
    for (std::size_t k = 0; k < nSeg; ++k) {
        const double len = std::hypot(nodes[k + 1][0] - nodes[k][0], nodes[k + 1][1] - nodes[k][1]);
        for (int q = 0; q < 3; ++q) {
            const double r = gx[q];
            const double x = nodes[k][0] + r * (nodes[k + 1][0] - nodes[k][0]) - centroid[0];
            const double y = nodes[k][1] + r * (nodes[k + 1][1] - nodes[k][1]) - centroid[1];
            const double w = omega[k] + r * (omega[k + 1] - omega[k]) - dx * y + dy * x;
            aw += w * t * len * gw[q];
        }
    }
    const double wMean = aw / area;
    cw = 0.0;
    for (std::size_t k = 0; k < nSeg; ++k) {
        const double len = std::hypot(nodes[k + 1][0] - nodes[k][0], nodes[k + 1][1] - nodes[k][1]);
        for (int q = 0; q < 3; ++q) {
            const double r = gx[q];
            const double x = nodes[k][0] + r * (nodes[k + 1][0] - nodes[k][0]) - centroid[0];
            const double y = nodes[k][1] + r * (nodes[k + 1][1] - nodes[k][1]) - centroid[1];
            const double w = omega[k] + r * (omega[k + 1] - omega[k]) - dx * y + dy * x - wMean;
            cw += w * w * t * len * gw[q];
        }
    }
}

} // namespace

ThinWalled buildThinWalled(const DesignSection &s)
{
    ThinWalled tw;
    const Frame f = frameOf(s);
    const double t = s.t;
    const double d = s.d;
    const double bf = s.bf;
    const double c = s.lip;
    tw.t = t;

    auto add = [&](Pt p1, Pt p2, bool free1, bool free2, const char *name, double width = 0.0) {
        Element e;
        e.p1 = f.to(p1[0], p1[1]);
        e.p2 = f.to(p2[0], p2[1]);
        e.t = t;
        e.freeEnd1 = free1;
        e.freeEnd2 = free2;
        e.name = name;
        e.nominalWidth = width > 0.0 ? width : std::hypot(e.p2[0] - e.p1[0], e.p2[1] - e.p1[1]);
        tw.elements.push_back(e);
        return tw.elements.size() - 1;
    };
    auto path = [&](std::initializer_list<Pt> pts) {
        for (const Pt &p : pts) {
            tw.pathNodes.push_back(f.to(p[0], p[1]));
        }
    };

    switch (s.kind) {
    case SectionKind::ColdChannel: {
        const double x0 = -bf / 2.0;
        const double xw = x0 + t / 2.0;
        const double yt = d / 2.0 - t / 2.0;
        const double yb = -d / 2.0 + t / 2.0;
        add({xw, -d / 2.0 + 2.0 * t}, {xw, d / 2.0 - 2.0 * t}, false, false, "alma");
        add({x0 + 2.0 * t, yt}, {bf / 2.0, yt}, false, true, "mesa superior");
        add({x0 + 2.0 * t, yb}, {bf / 2.0, yb}, false, true, "mesa inferior");
        path({{bf / 2.0, yt}, {xw, yt}, {xw, yb}, {bf / 2.0, yb}});
        break;
    }
    case SectionKind::ColdLippedChannel: {
        const double x0 = -bf / 2.0;
        const double xw = x0 + t / 2.0;
        const double xl = bf / 2.0 - t / 2.0;
        const double yt = d / 2.0 - t / 2.0;
        const double yb = -d / 2.0 + t / 2.0;
        const double tipTop = d / 2.0 - t - c;
        const double tipBot = -d / 2.0 + t + c;
        add({xw, -d / 2.0 + 2.0 * t}, {xw, d / 2.0 - 2.0 * t}, false, false, "alma");
        const std::size_t top = add({x0 + 2.0 * t, yt}, {bf / 2.0 - 2.0 * t, yt}, false, false, "mesa superior");
        const std::size_t bot = add({x0 + 2.0 * t, yb}, {bf / 2.0 - 2.0 * t, yb}, false, false, "mesa inferior");
        add({xl, d / 2.0 - 2.0 * t}, {xl, tipTop}, false, true, "enrijecedor superior");
        add({xl, -d / 2.0 + 2.0 * t}, {xl, tipBot}, false, true, "enrijecedor inferior");
        for (std::size_t idx : {top, bot}) {
            tw.elements[idx].lipStiffened = true;
            tw.elements[idx].lipLength = c - t;
        }
        path({{xl, tipTop}, {xl, yt}, {xw, yt}, {xw, yb}, {xl, yb}, {xl, tipBot}});
        break;
    }
    case SectionKind::ColdZ: {
        const double yt = d / 2.0 - t / 2.0;
        const double yb = -d / 2.0 + t / 2.0;
        add({0.0, -d / 2.0 + 2.0 * t}, {0.0, d / 2.0 - 2.0 * t}, false, false, "alma");
        add({-1.5 * t, yb}, {-t / 2.0 - bf, yb}, false, true, "mesa inferior");
        add({1.5 * t, yt}, {t / 2.0 + bf, yt}, false, true, "mesa superior");
        path({{-t / 2.0 - bf, yb}, {0.0, yb}, {0.0, yt}, {t / 2.0 + bf, yt}});
        break;
    }
    case SectionKind::ColdAngle: {
        const double x0 = -bf / 2.0;
        const double xv = x0 + t / 2.0;
        const double yh = d / 2.0 - t / 2.0;
        add({xv, d / 2.0 - 2.0 * t}, {xv, -d / 2.0}, false, true, "aba vertical");
        add({x0 + 2.0 * t, yh}, {bf / 2.0, yh}, false, true, "aba horizontal");
        path({{xv, -d / 2.0}, {xv, yh}, {bf / 2.0, yh}});
        break;
    }
    case SectionKind::ColdHat: {
        const double xl = -bf / 2.0;
        const double xr = bf / 2.0;
        const double yT = d / 2.0 - t / 2.0;
        const double yB = -d / 2.0 + t / 2.0;
        add({xl - t, yT}, {xl - c, yT}, false, true, "aba superior esquerda");
        add({xr + t, yT}, {xr + c, yT}, false, true, "aba superior direita");
        add({xl + t / 2.0, d / 2.0 - 2.0 * t}, {xl + t / 2.0, -d / 2.0 + 2.0 * t}, false, false, "alma esquerda");
        add({xr - t / 2.0, d / 2.0 - 2.0 * t}, {xr - t / 2.0, -d / 2.0 + 2.0 * t}, false, false, "alma direita");
        add({xl + 2.0 * t, yB}, {xr - 2.0 * t, yB}, false, false, "mesa inferior");
        path({{xl - c, yT}, {xl + t / 2.0, yT}, {xl + t / 2.0, yB}, {xr - t / 2.0, yB}, {xr - t / 2.0, yT}, {xr + c, yT}});
        break;
    }
    case SectionKind::RectTube: {
        const double w = s.bf - t;
        const double h = s.d - t;
        add({-w / 2.0 + 2.0 * t, h / 2.0}, {w / 2.0 - 2.0 * t, h / 2.0}, false, false, "mesa superior");
        add({-w / 2.0 + 2.0 * t, -h / 2.0}, {w / 2.0 - 2.0 * t, -h / 2.0}, false, false, "mesa inferior");
        add({-w / 2.0, h / 2.0 - 2.0 * t}, {-w / 2.0, -h / 2.0 + 2.0 * t}, false, false, "alma esquerda");
        add({w / 2.0, h / 2.0 - 2.0 * t}, {w / 2.0, -h / 2.0 + 2.0 * t}, false, false, "alma direita");
        tw.closed = true;
        break;
    }
    default:
        break;
    }

    if (!tw.closed && tw.pathNodes.size() >= 2) {
        Pt centroid {};
        Pt shear {};
        openSectionTorsion(tw.pathNodes, t, centroid, shear, tw.Cw);
        tw.x0 = shear[0] - (0.0 - 0.0);   // coordenadas ja relativas ao centroide bruto (origem)
        tw.y0 = shear[1];
        // O modelo de linha media tem centroide proprio: expressa o centro de cisalhamento em relacao ao centroide bruto.
        tw.x0 = shear[0];
        tw.y0 = shear[1];
        double len = 0.0;
        for (std::size_t k = 0; k + 1 < tw.pathNodes.size(); ++k) {
            len += std::hypot(tw.pathNodes[k + 1][0] - tw.pathNodes[k][0], tw.pathNodes[k + 1][1] - tw.pathNodes[k][1]);
        }
        tw.J = len * t * t * t / 3.0;
        // Posicao do centro de cisalhamento em relacao ao centroide do modelo; converte para o centroide bruto (origem).
        tw.x0 = shear[0] - 0.0;
        tw.y0 = shear[1] - 0.0;
        // `shear` esta nas mesmas coordenadas do desenho (origem no centroide bruto), portanto ja e a posicao pedida.
    } else if (tw.closed) {
        const double am = (s.bf - t) * (s.d - t);
        const double per = 2.0 * ((s.bf - t) + (s.d - t));
        tw.J = 4.0 * am * am * t / per;
    }
    return tw;
}

namespace {

// ---------------------------------------------------------------------------------------------------------------------
// Metodo da largura efetiva (9.2)
// ---------------------------------------------------------------------------------------------------------------------

struct Loss {
    double area {0.0};     ///< mm2 de area removida
    Pt center {};          ///< posicao do centro da area removida
    Pt dir {};             ///< direcao do elemento (unitaria)
    double length {0.0};
};

struct ElementOutcome {
    double bef {0.0};
    double lossLength {0.0};
};

/// Largura efetiva pela expressao basica de 9.2.2.1.
double basicEffectiveWidth(double b, double t, double k, double sigma, double E, double &lambdaP)
{
    lambdaP = (b / t) / (0.95 * std::sqrt(k * E / sigma));
    if (lambdaP <= 0.673) {
        return b;
    }
    return b * (1.0 - 0.22 / lambdaP) / lambdaP;
}

/// Perdas de largura efetiva de um elemento (sem enrijecedor de borda) para tensoes extremas s1, s2 nas
/// extremidades p1 e p2 (compressao positiva). `E` em MPa.
void elementLosses(const Element &e, double s1, double s2, double E, std::vector<Loss> &out, std::vector<std::string> *lines)
{
    const double b = std::hypot(e.p2[0] - e.p1[0], e.p2[1] - e.p1[1]);
    if (b <= 0.0) {
        return;
    }
    const Pt dir {(e.p2[0] - e.p1[0]) / b, (e.p2[1] - e.p1[1]) / b};
    auto at = [&](double distanceFromP1) -> Pt { return {e.p1[0] + dir[0] * distanceFromP1, e.p1[1] + dir[1] * distanceFromP1}; };
    auto push = [&](double from, double to) {
        if (to - from > 1e-9) {
            out.push_back({(to - from) * e.t, at(0.5 * (from + to)), dir, to - from});
        }
    };

    const bool free = e.freeEnd1 || e.freeEnd2;
    // tensoes: sigmaA em p1, sigmaB em p2
    if (s1 <= 0.0 && s2 <= 0.0) {
        return;   // totalmente tracionado: efetivo
    }
    double lambdaP = 0.0;
    if (!free) {
        // Elemento AA (Tabela 5): sigma1 = maior compressao
        const bool firstIsHigh = s1 >= s2;
        const double sig1 = std::max(s1, s2);
        const double sig2 = std::min(s1, s2);
        const double psi = sig2 / sig1;
        const double k = 4.0 + 2.0 * (1.0 - psi) + 2.0 * std::pow(1.0 - psi, 3);
        const double bef = basicEffectiveWidth(b, e.t, k, sig1, E, lambdaP);
        if (bef >= b - 1e-9) {
            return;
        }
        double bef1 = bef / (3.0 - psi);
        double bef2 = bef - bef1;
        if (psi <= -0.236) {
            bef2 = 0.5 * bef;
            const double bc = b * sig1 / (sig1 - sig2);
            if (bef1 + bef2 > bc) {
                bef2 = std::max(0.0, bc - bef1);
            }
        }
        // medidos a partir da extremidade de maior tensao
        double from = bef1;
        double to = b - bef2;
        if (!firstIsHigh) {
            const double f1 = b - to;
            const double t1 = b - from;
            from = f1;
            to = t1;
        }
        push(from, to);
        if (lines) {
            lines->push_back(fmt("%s (AA): psi = %.2f, k = %.2f, lambda_p = %.3f, bef = %.1f mm (b = %.1f mm)", e.name.c_str(), psi, k, lambdaP, bef, b));
        }
        return;
    }
    // Elemento AL (Tabela 6): extremidade livre
    const bool freeIsEnd2 = e.freeEnd2;
    const double sigSupported = freeIsEnd2 ? s1 : s2;
    const double sigFree = freeIsEnd2 ? s2 : s1;
    double k;
    double sigmaRef;
    bool inversion = false;
    if (sigSupported >= sigFree) {
        sigmaRef = sigSupported;
        if (sigmaRef <= 0.0) {
            return;
        }
        const double psi = sigFree / sigSupported;
        if (psi >= 1.0 - 1e-9) {
            k = 0.43;
        } else if (psi >= 0.0) {
            k = 0.578 / (psi + 0.34);
        } else {
            k = 1.7 - 5.0 * psi + 17.1 * psi * psi;
            inversion = true;
        }
    } else {
        sigmaRef = sigFree;
        const double psi = sigSupported / sigFree;
        k = 0.57 - 0.21 * psi + 0.07 * psi * psi;
    }
    const double bc = inversion ? b * sigSupported / (sigSupported - sigFree) : b;
    double beff = basicEffectiveWidth(b, e.t, k, sigmaRef, E, lambdaP);
    if (inversion) {
        beff = lambdaP <= 0.673 ? bc : bc * (1.0 - 0.22 / lambdaP) / lambdaP;
        beff = std::min(beff, bc);
    }
    if (beff >= b - 1e-9) {
        return;
    }
    // a parte efetiva comeca na borda apoiada; a perda fica do lado livre
    const double supportedDistance = freeIsEnd2 ? 0.0 : b;
    const double from = freeIsEnd2 ? beff : 0.0;
    const double to = freeIsEnd2 ? b : b - beff;
    (void)supportedDistance;
    push(from, to);
    if (lines) {
        lines->push_back(fmt("%s (AL): k = %.3f, lambda_p = %.3f, bef = %.1f mm (b = %.1f mm)", e.name.c_str(), k, lambdaP, beff, b));
    }
}

/// Perdas do elemento com enrijecedor de borda simples (9.2.3) sob tensao uniforme `sigma`.
void lippedFlangeLosses(const Element &flange, const Element &lip, double sigma, double E, std::vector<Loss> &out, std::vector<std::string> *lines)
{
    const double b = std::hypot(flange.p2[0] - flange.p1[0], flange.p2[1] - flange.p1[1]);
    const double t = flange.t;
    const double dLip = flange.lipLength;
    const double dNominal = dLip + t;
    const Pt dir {(flange.p2[0] - flange.p1[0]) / b, (flange.p2[1] - flange.p1[1]) / b};
    auto at = [&](double distanceFromP1) -> Pt { return {flange.p1[0] + dir[0] * distanceFromP1, flange.p1[1] + dir[1] * distanceFromP1}; };

    const double lambdaP0 = (b / t) / (0.623 * std::sqrt(E / sigma));
    // largura efetiva do enrijecedor (AL, uniforme)
    double lipLambda = 0.0;
    const double lipB = std::hypot(lip.p2[0] - lip.p1[0], lip.p2[1] - lip.p1[1]);
    const double def = basicEffectiveWidth(lipB, t, 0.43, sigma, E, lipLambda);
    double ds = def;
    double bef;
    double bef1 = 0.0;
    double bef2 = 0.0;
    double isOverIa = 1.0;
    if (lambdaP0 <= 0.673) {
        bef = b;
        bef1 = bef / 2.0;
        bef2 = bef / 2.0;
    } else {
        const double Is = t * std::pow(dLip, 3) * 1.0 / 12.0;   // sen(theta) = 1 (theta = 90 graus)
        const double IaRaw = 399.0 * std::pow(t, 4) * std::pow(0.487 * lambdaP0 - 0.328, 3);
        const double Ia = std::min(IaRaw, std::pow(t, 4) * (56.0 * lambdaP0 + 5.0));
        isOverIa = std::min(1.0, Is / Ia);
        const double n = std::max(1.0 / 3.0, 0.582 - 0.122 * lambdaP0);
        const double dOverB = dNominal / b;
        double k;
        if (dOverB <= 0.25) {
            k = 3.57 * std::pow(isOverIa, n) + 0.43;
        } else {
            k = (4.82 - 5.0 * dOverB) * std::pow(isOverIa, n) + 0.43;
        }
        k = std::min(k, 4.0);
        double lp = 0.0;
        bef = basicEffectiveWidth(b, t, k, sigma, E, lp);
        bef1 = std::min(isOverIa * bef / 2.0, bef / 2.0);
        bef2 = bef - bef1;
        ds = std::min(def, isOverIa * def);
    }
    // perda no elemento plano: entre bef2 (lado da alma = p1) e bef1 (lado do enrijecedor = p2)
    if (bef < b - 1e-9) {
        const double from = bef2;
        const double to = b - bef1;
        if (to - from > 1e-9) {
            out.push_back({(to - from) * t, at(0.5 * (from + to)), dir, to - from});
        }
    }
    // perda no enrijecedor: parte nao efetiva na extremidade livre
    if (ds < lipB - 1e-9) {
        const Pt ldir {(lip.p2[0] - lip.p1[0]) / lipB, (lip.p2[1] - lip.p1[1]) / lipB};
        const double from = ds;
        const double to = lipB;
        const Pt center {lip.p1[0] + ldir[0] * 0.5 * (from + to), lip.p1[1] + ldir[1] * 0.5 * (from + to)};
        out.push_back({(to - from) * t, center, ldir, to - from});
    }
    if (lines) {
        lines->push_back(fmt("%s com enrijecedor (9.2.3): lambda_p0 = %.3f, Is/Ia = %.3f, bef = %.1f mm (b = %.1f), ds = %.1f mm (d = %.1f)",
                             flange.name.c_str(), lambdaP0, isOverIa, bef, b, ds, lipB));
    }
}

struct Gross {
    double area {0.0};
    double ix {0.0};    ///< integral de b^2
    double iy {0.0};    ///< integral de a^2
    double ixy {0.0};
};

struct Effective {
    double area {0.0};
    Pt centroid {};
    double ix {0.0};    ///< em relacao ao centroide efetivo
    double iy {0.0};
    double ixy {0.0};
    std::vector<Loss> losses;
};

Effective applyLosses(const DesignSection &s, const std::vector<Loss> &losses)
{
    Effective e;
    e.losses = losses;
    double lost = 0.0, la = 0.0, lb = 0.0;
    for (const Loss &l : losses) {
        lost += l.area;
        la += l.area * l.center[0];
        lb += l.area * l.center[1];
    }
    e.area = s.g.area - lost;
    e.centroid = {-la / e.area, -lb / e.area};
    double ix = s.g.Ix;
    double iy = s.g.Iy;
    double ixy = s.g.Ixy;
    for (const Loss &l : losses) {
        const double a = l.center[0];
        const double b = l.center[1];
        const double self = l.length * l.length * l.length * (l.area / l.length) / 12.0;   // t L^3 / 12
        ix -= l.area * b * b + self * l.dir[1] * l.dir[1];
        iy -= l.area * a * a + self * l.dir[0] * l.dir[0];
        ixy -= l.area * a * b + self * l.dir[0] * l.dir[1];
    }
    e.ix = ix - e.area * e.centroid[1] * e.centroid[1];
    e.iy = iy - e.area * e.centroid[0] * e.centroid[0];
    e.ixy = ixy - e.area * e.centroid[0] * e.centroid[1];
    return e;
}

/// Perdas totais da secao para um campo de tensoes definido por `stressAt(a, b)` (compressao positiva).
template <typename StressFn>
std::vector<Loss> sectionLosses(const ThinWalled &tw, double E, StressFn stressAt, std::vector<std::string> *lines)
{
    std::vector<Loss> losses;
    // elementos com enrijecedor: associa cada mesa enrijecida ao enrijecedor mais proximo da extremidade p2
    std::vector<bool> consumedLip(tw.elements.size(), false);
    for (std::size_t i = 0; i < tw.elements.size(); ++i) {
        const Element &e = tw.elements[i];
        if (!e.lipStiffened) {
            continue;
        }
        // procura o enrijecedor (elemento AL) cujo ponto de apoio coincide com p2 da mesa
        for (std::size_t j = 0; j < tw.elements.size(); ++j) {
            const Element &lip = tw.elements[j];
            if (!(lip.freeEnd1 || lip.freeEnd2) || lip.lipStiffened) {
                continue;
            }
            const Pt supported = lip.freeEnd2 ? lip.p1 : lip.p2;
            // o apoio do enrijecedor esta proximo (mesma extremidade, 2t ao longo da mesa) do p2 da mesa
            if (std::hypot(supported[0] - e.p2[0], supported[1] - e.p2[1]) < 4.0 * e.t + 1e-6 &&
                std::hypot(e.p2[0] - supported[0], e.p2[1] - supported[1]) < 4.0 * e.t + 1e-6) {
                const double sigma = std::max({stressAt(e.p1[0], e.p1[1]), stressAt(e.p2[0], e.p2[1]), 1e-6});
                lippedFlangeLosses(e, lip, sigma, E, losses, lines);
                consumedLip[j] = true;
                break;
            }
        }
    }
    for (std::size_t i = 0; i < tw.elements.size(); ++i) {
        const Element &e = tw.elements[i];
        if (e.lipStiffened || consumedLip[i]) {
            continue;
        }
        elementLosses(e, stressAt(e.p1[0], e.p1[1]), stressAt(e.p2[0], e.p2[1]), E, losses, lines);
    }
    return losses;
}

struct BendingSection {
    double Wc {0.0};      ///< Ief / distancia a fibra comprimida extrema
    double Wt {0.0};
    double Ief {0.0};
    double naOffset {0.0};
    double area {0.0};
    std::vector<std::string> lines;
};

/// Secao efetiva em flexao simples em torno de `axis` com tensao maxima de compressao `sigmaMax`.
BendingSection effectiveBending(const DesignSection &s, const ThinWalled &tw, Axis axis, bool posCompressed, double sigmaMax, double E)
{
    BendingSection r;
    const bool alongB = (axis == Axis::X);
    // extremos brutos
    const double extPos = alongB ? s.g.bMax : s.g.aMax;
    const double extNeg = alongB ? s.g.bMin : s.g.aMin;
    const double extC = posCompressed ? extPos : extNeg;
    const double extT = posCompressed ? extNeg : extPos;
    double na = 0.0;   // posicao da linha neutra (no eixo de flexao)
    Effective eff;
    for (int it = 0; it < 40; ++it) {
        auto stressAt = [&](double a, double b) {
            const double x = alongB ? b : a;
            return sigmaMax * (x - na) / (extC - na);   // compressao positiva
        };
        std::vector<std::string> lines;
        const std::vector<Loss> losses = sectionLosses(tw, E, stressAt, &lines);
        eff = applyLosses(s, losses);
        const double newNa = alongB ? eff.centroid[1] : eff.centroid[0];
        r.lines = lines;
        if (std::abs(newNa - na) < 1e-6) {
            na = newNa;
            break;
        }
        na = newNa;
    }
    const double inertia = alongB ? eff.ix : eff.iy;
    r.Ief = inertia;
    r.naOffset = na;
    r.area = eff.area;
    r.Wc = inertia / std::abs(extC - na);
    r.Wt = inertia / std::abs(na - extT);
    return r;
}

/// Momento de inicio de escoamento da secao efetiva (9.8.2.1): a fibra que primeiro atinge fy.
double yieldMomentEffective(const DesignSection &s, const ThinWalled &tw, Axis axis, bool posCompressed, double fy, double E, BendingSection *outSection)
{
    double sigmaC = fy;
    BendingSection sec = effectiveBending(s, tw, axis, posCompressed, sigmaC, E);
    for (int it = 0; it < 6; ++it) {
        // se a fibra tracionada ultrapassa fy com a comprimida em sigmaC, reduz sigmaC
        const double ratioTension = sec.Wc / sec.Wt;   // sigma_t / sigma_c = (dist_c)/(dist_t)
        // sigma_t = sigmaC * (distT / distC) = sigmaC * Wc / Wt
        const double sigmaT = sigmaC * ratioTension;
        if (sigmaT <= fy + 1e-6) {
            break;
        }
        sigmaC = fy / ratioTension;
        sec = effectiveBending(s, tw, axis, posCompressed, sigmaC, E);
    }
    if (outSection) {
        *outSection = sec;
    }
    return std::min(sec.Wc * sigmaC, sec.Wt * fy);
}

// ---------------------------------------------------------------------------------------------------------------------
// Flambagem distorcional via faixas finitas
// ---------------------------------------------------------------------------------------------------------------------

struct DistortionalResult {
    bool available {false};
    double factor {0.0};     ///< multiplicador da tensao de referencia (MPa)
    double wavelength {0.0};
    std::string note;
};

DistortionalResult distortionalFactor(const DesignSection &s, const ThinWalled &tw, double E, bool bending, Axis axis, bool posCompressed)
{
    DistortionalResult r;
    FiniteStripModel m;
    m.E = E;
    m.nu = kNu;
    if (tw.closed && s.kind == SectionKind::RectTube) {
        const double w = s.bf - tw.t, h = s.d - tw.t;
        m.nodes = {{-w / 2.0, h / 2.0}, {w / 2.0, h / 2.0}, {w / 2.0, -h / 2.0}, {-w / 2.0, -h / 2.0}};
        for (int i = 0; i < 4; ++i) {
            m.strips.push_back({i, (i + 1) % 4});
            m.thickness.push_back(tw.t);
        }
    } else {
        if (tw.pathNodes.size() < 2) {
            r.note = "secao sem linha media definida para a analise de faixas finitas.";
            return r;
        }
        m.nodes = tw.pathNodes;
        for (std::size_t i = 0; i + 1 < m.nodes.size(); ++i) {
            m.strips.push_back({static_cast<int>(i), static_cast<int>(i + 1)});
            m.thickness.push_back(tw.t);
        }
    }
    const bool alongB = (axis == Axis::X);
    const double extPos = alongB ? s.g.bMax : s.g.aMax;
    const double extNeg = alongB ? s.g.bMin : s.g.aMin;
    const double extC = posCompressed ? extPos : extNeg;
    for (const Pt &p : m.nodes) {
        if (!bending) {
            m.nodeStress.push_back(1.0);
        } else {
            const double x = alongB ? p[1] : p[0];
            m.nodeStress.push_back(x / extC);
        }
    }
    double minLen = 0.3 * tw.t;
    double maxDim = 0.0;
    for (const Pt &p : m.nodes) {
        maxDim = std::max({maxDim, std::abs(p[0]), std::abs(p[1])});
    }
    minLen = std::max(minLen, 0.2 * s.t * 5.0);
    const std::vector<FiniteStripMinimum> minima = finiteStripMinima(m, std::max(5.0, tw.t * 3.0), std::max(3000.0, 30.0 * maxDim), 70);
    if (minima.size() < 2) {
        r.note = tw.closed ? "secao fechada: a curva de assinatura so tem o modo local (nao ha modo distorcional)."
                          : "a curva de assinatura (analise de faixas finitas) so apresenta o modo local e o global: nao ha modo distorcional.";
        return r;
    }
    // segundo minimo: modo distorcional (comprimento de meia-onda maior que o do modo local)
    const FiniteStripMinimum &local = minima[0];
    for (std::size_t i = 1; i < minima.size(); ++i) {
        if (minima[i].halfWaveLength > 2.0 * local.halfWaveLength) {
            r.available = true;
            r.factor = minima[i].loadFactor;
            r.wavelength = minima[i].halfWaveLength;
            return r;
        }
    }
    r.note = "Nenhum minimo distorcional identificado na curva de assinatura.";
    return r;
}

double interpolateTable(const double table[9][5], double bfOverBw, double bwOverT)
{
    static const double rows[9] = {0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0};
    static const double cols[5] = {250.0, 200.0, 125.0, 100.0, 50.0};
    const double r = std::clamp(bfOverBw, rows[0], rows[8]);
    const double c = std::clamp(bwOverT, cols[4], cols[0]);
    int ri = 0;
    while (ri < 7 && r > rows[ri + 1]) {
        ++ri;
    }
    int ci = 0;
    while (ci < 3 && c < cols[ci + 1]) {
        ++ci;
    }
    const double tr = (r - rows[ri]) / (rows[ri + 1] - rows[ri]);
    const double tc = (c - cols[ci]) / (cols[ci + 1] - cols[ci]);
    const double v00 = table[ri][ci];
    const double v01 = table[ri][ci + 1];
    const double v10 = table[ri + 1][ci];
    const double v11 = table[ri + 1][ci + 1];
    return (1 - tr) * ((1 - tc) * v00 + tc * v01) + tr * ((1 - tc) * v10 + tc * v11);
}

const double kTable11[9][5] = {
    {0.02, 0.03, 0.04, 0.04, 0.08}, {0.03, 0.04, 0.06, 0.06, 0.15}, {0.05, 0.06, 0.08, 0.10, 0.22},
    {0.06, 0.07, 0.10, 0.12, 0.27}, {0.06, 0.07, 0.12, 0.15, 0.27}, {0.06, 0.08, 0.12, 0.15, 0.27},
    {0.07, 0.08, 0.12, 0.15, 0.27}, {0.07, 0.08, 0.12, 0.15, 0.27}, {0.07, 0.08, 0.12, 0.15, 0.27}};
const double kTable14[9][5] = {
    {0.05, 0.06, 0.10, 0.12, 0.25}, {0.05, 0.06, 0.10, 0.12, 0.25}, {0.05, 0.06, 0.09, 0.12, 0.22},
    {0.05, 0.06, 0.09, 0.11, 0.22}, {0.05, 0.06, 0.09, 0.11, 0.20}, {0.05, 0.06, 0.09, 0.10, 0.20},
    {0.05, 0.06, 0.09, 0.10, 0.20}, {0.05, 0.06, 0.09, 0.10, 0.19}, {0.05, 0.06, 0.09, 0.10, 0.19}};

double chiCompression(double lambda0)
{
    return lambda0 <= 1.5 ? std::pow(0.658, sq(lambda0)) : 0.877 / sq(lambda0);
}

double chiDistortionalCompression(double lambda)
{
    if (lambda <= 0.561) {
        return 1.0;
    }
    const double p = std::pow(lambda, 1.2);
    return (1.0 - 0.25 / p) / p;
}

double chiDistortionalFlexure(double lambda)
{
    if (lambda <= 0.673) {
        return 1.0;
    }
    return (1.0 - 0.22 / lambda) / lambda;
}

double asymmetricCubic(double nex, double ney, double nez, double x0, double y0, double r0)
{
    const double px = sq(x0 / r0);
    const double py = sq(y0 / r0);
    auto f = [&](double n) { return (n - nex) * (n - ney) * (n - nez) - sq(n) * (n - ney) * px - sq(n) * (n - nex) * py; };
    double lo = 0.0;
    double hi = std::min({nex, ney, nez});
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        (f(mid) < 0.0 ? lo : hi) = mid;
    }
    return 0.5 * (lo + hi);
}

bool isColdFormed(SectionKind kind)
{
    return kind == SectionKind::ColdChannel || kind == SectionKind::ColdLippedChannel || kind == SectionKind::ColdZ || kind == SectionKind::ColdAngle
        || kind == SectionKind::ColdHat || kind == SectionKind::RectTube;
}

bool isMonoSymmetricX(const DesignSection &s) { return s.kind == SectionKind::ColdChannel || s.kind == SectionKind::ColdLippedChannel; }
bool isMonoSymmetricY(const DesignSection &s) { return s.kind == SectionKind::ColdHat; }

} // namespace

GeometryLimits geometryLimits(const DesignSection &s, const SteelGrade &steel)
{
    GeometryLimits g;
    const ThinWalled tw = buildThinWalled(s);
    auto consider = [&](const std::string &what, double bOverT, double limit) {
        const double r = bOverT / limit;
        g.ratio = std::max(g.ratio, r);
        g.lines.push_back(fmt("%s: b/t = %.1f; (b/t)max = %.0f (Tabela 4) -> %.3f", what.c_str(), bOverT, limit, r));
    };
    for (const Element &e : tw.elements) {
        const double b = std::hypot(e.p2[0] - e.p1[0], e.p2[1] - e.p1[1]);
        const double bt = b / e.t;
        const bool isWeb = e.name.rfind("alma", 0) == 0;
        if (tw.closed) {
            consider(e.name + " (AA, bordas em AA)", bt, 500.0);
        } else if (e.freeEnd1 || e.freeEnd2) {
            consider(e.name + " (AL)", bt, 60.0);
        } else if (e.lipStiffened) {
            // 90 se o enrijecedor tem Is >= Ia (com sigma = fy), senao 60
            const double lambdaP0 = bt / (0.623 * std::sqrt(steel.E / steel.fy));
            const double is = e.t * std::pow(e.lipLength, 3) / 12.0;
            const double ia = lambdaP0 <= 0.673 ? 0.0 : std::min(399.0 * std::pow(e.t, 4) * std::pow(0.487 * lambdaP0 - 0.328, 3), std::pow(e.t, 4) * (56.0 * lambdaP0 + 5.0));
            consider(e.name + (is >= ia ? " (AA com enrijecedor Is >= Ia)" : " (AA com enrijecedor Is < Ia)"), bt, is >= ia ? 90.0 : 60.0);
        } else if (isWeb && s.kind == SectionKind::ColdChannel) {
            consider(e.name + " (alma de U nao enrijecido)", bt, 90.0);
        } else if (isWeb) {
            consider(e.name + " (alma sem enrijecedores transversais)", bt, 200.0);
        } else {
            consider(e.name + " (AA)", bt, 500.0);
        }
    }
    return g;
}

// ---------------------------------------------------------------------------------------------------------------------
// Tracao (9.6)
// ---------------------------------------------------------------------------------------------------------------------

TensionData tension(const DesignSection &s, const SteelGrade &steel, const Options &o)
{
    TensionData t;
    const double a = s.g.area;
    t.yieldRd = a * steel.fy / o.gammaTension;
    const double farRupture = a * steel.fu / o.gammaRuptureFar;
    const double connRupture = o.ct * o.netAreaRatio * a * steel.fu / o.gammaRuptureConn;
    t.ruptureRd = std::min(farRupture, connRupture);
    t.NtRd = std::min(t.yieldRd, t.ruptureRd);
    t.lines.push_back(fmt("Escoamento da secao bruta (9.6.2-a): A fy / %.2f = %s", o.gammaTension, kn(t.yieldRd).c_str()));
    t.lines.push_back(fmt("Ruptura fora da ligacao (9.6.2-b): An0 fu / %.2f = %s", o.gammaRuptureFar, kn(farRupture).c_str()));
    t.lines.push_back(fmt("Ruptura na ligacao (9.6.2-c): Ct An fu / %.2f = %.2f x %.2f x A x %.0f / %.2f = %s", o.gammaRuptureConn, o.ct, o.netAreaRatio, steel.fu,
                          o.gammaRuptureConn, kn(connRupture).c_str()));
    return t;
}

// ---------------------------------------------------------------------------------------------------------------------
// Compressao (9.7)
// ---------------------------------------------------------------------------------------------------------------------

CompressionData compression(const DesignSection &s, const SteelGrade &steel, const Buckling &len, const Options &o)
{
    CompressionData c;
    const double E = steel.E;
    const double G = steel.G;
    const double fy = steel.fy;
    const ThinWalled tw = buildThinWalled(s);
    const double ag = s.g.area;
    const double rx = s.g.rx;
    const double ry = s.g.ry;
    const double inf = std::numeric_limits<double>::infinity();

    c.slenderness = std::max(len.Lx > 0 ? len.Lx / rx : 0.0, len.Ly > 0 ? len.Ly / ry : 0.0);
    const double nex = len.Lx > 0 ? sq(kPi) * E * s.g.Ix / sq(len.Lx) : inf;
    const double ney = len.Ly > 0 ? sq(kPi) * E * s.g.Iy / sq(len.Ly) : inf;
    const double x0 = tw.closed ? 0.0 : tw.x0;
    const double y0 = tw.closed ? 0.0 : tw.y0;
    const double r0sq = sq(rx) + sq(ry) + sq(x0) + sq(y0);
    const double r0 = std::sqrt(r0sq);
    double nez = inf;
    if (len.Lz > 0.0) {
        nez = (sq(kPi) * E * tw.Cw / sq(len.Lz) + G * tw.J) / r0sq;
    }
    double ne = std::min({nex, ney, nez});
    c.mode = (ne == nex) ? "flexao em torno de X" : (ne == ney) ? "flexao em torno de Y" : "torcao";
    c.lines.push_back(fmt("Nex = %s, Ney = %s, Nez = %s (Cw = %.3e mm6, J = %.1f mm4, x0 = %.1f mm, y0 = %.1f mm, r0 = %.1f mm)", std::isinf(nex) ? "inf" : kn(nex).c_str(),
                          std::isinf(ney) ? "inf" : kn(ney).c_str(), std::isinf(nez) ? "inf" : kn(nez).c_str(), tw.Cw, tw.J, x0, y0, r0));

    if (isMonoSymmetricX(s) || isMonoSymmetricY(s)) {
        const bool symX = isMonoSymmetricX(s);
        const double nsym = symX ? nex : ney;
        const double nother = symX ? ney : nex;
        const double offset = symX ? x0 : y0;
        const double ratio = 1.0 - sq(offset / r0);
        const double sum = nsym + nez;
        double nft = inf;
        if (!std::isinf(nsym) && !std::isinf(nez)) {
            nft = sum / (2.0 * ratio) * (1.0 - std::sqrt(std::max(0.0, 1.0 - 4.0 * nsym * nez * ratio / sq(sum))));
        } else if (!std::isinf(nez)) {
            nft = nez;
        }
        ne = std::min(nother, nft);
        c.mode = (ne == nother) ? (symX ? "flexao em torno de Y" : "flexao em torno de X") : "flexo-torcao";
        c.lines.push_back(fmt("Perfil monossimetrico (9.7.2.2): N flexo-torcao = %s; Ne = %s", std::isinf(nft) ? "inf" : kn(nft).c_str(), kn(ne).c_str()));
    } else if (s.kind == SectionKind::ColdAngle) {
        if (!std::isinf(nex) && !std::isinf(ney) && !std::isinf(nez)) {
            ne = asymmetricCubic(nex, ney, nez, x0, y0, r0);
            c.mode = "flexo-torcao (perfil assimetrico, 9.7.2.3)";
            c.lines.push_back(fmt("Perfil assimetrico (9.7.2.3): menor raiz do cubico = %s", kn(ne).c_str()));
        }
    }

    if (std::isinf(ne)) {
        c.Ne = ne;
        c.chi = 1.0;
        c.lambda0 = 0.0;
    } else {
        c.Ne = ne;
        c.lambda0 = std::sqrt(ag * fy / ne);
        c.chi = chiCompression(c.lambda0);
    }
    c.lines.push_back(fmt("Ne = %s (%s); lambda0 = sqrt(A fy / Ne) = %.3f; chi = %.3f (9.7.2)", std::isinf(ne) ? "inf" : kn(ne).c_str(), c.mode.c_str(), c.lambda0, c.chi));

    // Area efetiva (MLE) com sigma = chi fy
    const double sigma = c.chi * fy;
    std::vector<std::string> lines;
    auto stress = [&](double, double) { return sigma; };
    const std::vector<Loss> losses = sectionLosses(tw, E, stress, &lines);
    double lost = 0.0;
    for (const Loss &l : losses) {
        lost += l.area;
    }
    c.Aef = ag - lost;
    for (const std::string &line : lines) {
        c.lines.push_back(line);
    }
    c.lines.push_back(fmt("Aef = A - perdas = %.1f - %.1f = %.1f mm2 (MLE, sigma = chi fy = %.1f MPa)", ag, lost, c.Aef, sigma));
    const double ncRdGlobal = c.chi * c.Aef * fy / o.gammaCompression;
    c.NcRd = ncRdGlobal;
    c.lines.push_back(fmt("Nc,Rd (flambagem global e local, 9.7.2) = chi Aef fy / %.2f = %s", o.gammaCompression, kn(ncRdGlobal).c_str()));

    // Flambagem distorcional (9.7.3)
    if (o.checkDistortional && isColdFormed(s.kind)) {
        bool dispensed = false;
        if (s.kind == SectionKind::ColdLippedChannel) {
            const double table = interpolateTable(kTable11, s.bf / s.d, s.d / s.t);
            if (s.lip / s.d >= table) {
                dispensed = true;
                c.lines.push_back(fmt("Flambagem distorcional dispensada (Tab. 11): D/bw = %.3f >= %.3f (bf/bw = %.2f, bw/t = %.0f)", s.lip / s.d, table, s.bf / s.d, s.d / s.t));
            }
        }
        if (!dispensed) {
            const DistortionalResult dr = distortionalFactor(s, tw, E, false, Axis::X, true);
            if (dr.available) {
                const double ndist = dr.factor * ag;
                const double lambdaD = std::sqrt(ag * fy / ndist);
                const double chiD = chiDistortionalCompression(lambdaD);
                const double ncRdDist = chiD * ag * fy / o.gammaCompression;
                c.lines.push_back(fmt("Flambagem distorcional (9.7.3): Ndist = %s (analise de estabilidade elastica, meia-onda %.0f mm); lambda_dist = %.3f; chi_dist = %.3f; Nc,Rd = %s",
                                      kn(ndist).c_str(), dr.wavelength, lambdaD, chiD, kn(ncRdDist).c_str()));
                c.NcRd = std::min(c.NcRd, ncRdDist);
            } else if (!dr.note.empty()) {
                c.lines.push_back("Flambagem distorcional: " + dr.note);
            }
        }
    }
    return c;
}

// ---------------------------------------------------------------------------------------------------------------------
// Flexao (9.8.2)
// ---------------------------------------------------------------------------------------------------------------------

FlexureData flexure(const DesignSection &s, const SteelGrade &steel, Axis axis, bool posCompressed, double lb, double cb, const Options &o)
{
    FlexureData f;
    const double E = steel.E;
    const double G = steel.G;
    const double fy = steel.fy;
    const ThinWalled tw = buildThinWalled(s);
    const bool major = (axis == Axis::X);

    // 9.8.2.1: inicio de escoamento da secao efetiva
    BendingSection bs;
    const double my = yieldMomentEffective(s, tw, axis, posCompressed, fy, E, &bs);
    const double mYieldRd = my / o.gammaFlexure;
    f.lines.insert(f.lines.end(), bs.lines.begin(), bs.lines.end());
    f.lines.push_back(fmt("Inicio de escoamento da secao efetiva (9.8.2.1): Wef = %.0f mm3 (W bruto = %.0f mm3) -> M_Rd = Wef fy / %.2f = %s",
                          std::min(bs.Wc, bs.Wt), (major ? s.g.Wx() : s.g.Wy()), o.gammaFlexure, knm(mYieldRd).c_str()));
    f.MRd = mYieldRd;
    f.governing = "escoamento da secao efetiva";
    f.Mpl = my;

    // 9.8.2.2: flambagem lateral com torcao (flexao em torno do eixo de maior inercia)
    const bool lateralApplicable = lb > 0.0 && major && s.g.Ix > s.g.Iy;
    if (lateralApplicable) {
        const double ey = sq(kPi) * E * s.g.Iy / sq(lb);
        const double x0 = tw.closed ? 0.0 : tw.x0;
        const double y0 = tw.closed ? 0.0 : tw.y0;
        const double r0sq = sq(s.g.rx) + sq(s.g.ry) + sq(x0) + sq(y0);
        const double r0 = std::sqrt(r0sq);
        const double nez = (sq(kPi) * E * tw.Cw / sq(lb) + G * tw.J) / r0sq;
        double me = 0.0;
        bool covered = true;
        std::string how;
        if (tw.closed) {
            me = cb * std::sqrt(ey * G * tw.J);
            how = "secao fechada: Me = Cb sqrt(Ney G J)";
        } else if (isMonoSymmetricX(s)) {
            me = cb * r0 * std::sqrt(ey * nez);
            how = "flexao em torno do eixo de simetria: Me = Cb r0 sqrt(Ney Nez)";
        } else if (s.kind == SectionKind::ColdZ) {
            me = 0.5 * cb * r0 * std::sqrt(ey * nez);
            how = "Z ponto-simetrico: Me = 0,5 Cb r0 sqrt(Ney Nez)";
        } else if (isMonoSymmetricY(s)) {
            // Anexo E: monossimetrica fletida em torno do eixo perpendicular ao de simetria. j via integracao numerica.
            const double nexM = sq(kPi) * E * s.g.Ix / sq(lb);
            // integral de x^3 dA + x y^2 dA ao longo da linha media; eixo x = eixo de simetria (a), y = b
            double int3 = 0.0;
            for (std::size_t k = 0; k + 1 < tw.pathNodes.size(); ++k) {
                const Pt &p = tw.pathNodes[k];
                const Pt &q = tw.pathNodes[k + 1];
                const double len = std::hypot(q[0] - p[0], q[1] - p[1]);
                const double gx[3] = {0.5 - 0.5 * std::sqrt(0.6), 0.5, 0.5 + 0.5 * std::sqrt(0.6)};
                const double gw[3] = {5.0 / 18.0, 8.0 / 18.0, 5.0 / 18.0};
                for (int qd = 0; qd < 3; ++qd) {
                    const double a = p[0] + gx[qd] * (q[0] - p[0]);
                    const double b = p[1] + gx[qd] * (q[1] - p[1]);
                    int3 += (a * a * a + a * b * b) * tw.t * len * gw[qd];
                }
            }
            // o eixo de simetria desta secao e o eixo Y do projeto (b): usa os papeis trocados (a <-> b)
            double int3b = 0.0;
            for (std::size_t k = 0; k + 1 < tw.pathNodes.size(); ++k) {
                const Pt &p = tw.pathNodes[k];
                const Pt &q = tw.pathNodes[k + 1];
                const double len = std::hypot(q[0] - p[0], q[1] - p[1]);
                const double gx[3] = {0.5 - 0.5 * std::sqrt(0.6), 0.5, 0.5 + 0.5 * std::sqrt(0.6)};
                const double gw[3] = {5.0 / 18.0, 8.0 / 18.0, 5.0 / 18.0};
                for (int qd = 0; qd < 3; ++qd) {
                    const double a = p[0] + gx[qd] * (q[0] - p[0]);
                    const double b = p[1] + gx[qd] * (q[1] - p[1]);
                    int3b += (b * b * b + b * a * a) * tw.t * len * gw[qd];
                }
            }
            (void)int3;
            // Eixo de simetria = Y (b); a "coordenada x" do Anexo E corresponde a b; o momento flete em torno do eixo X do projeto.
            const double ixSym = s.g.Ix;   // inercia em relacao ao eixo perpendicular ao de simetria (= "Iy" do anexo)
            const double jParam = int3b / (2.0 * ixSym) + y0;
            const double nezA = (sq(kPi) * E * tw.Cw / sq(lb) + G * tw.J) / r0sq;
            // N_ex do anexo = flexao em torno do eixo de simetria (Y do projeto)
            const double nexAnnex = sq(kPi) * E * s.g.Iy / sq(lb);
            // Cs = +1 se a compressao ocorre do lado do centro de cisalhamento (eixo de simetria apontando para o lado oposto)
            const double sideOfShearCenter = (y0 >= 0.0) ? 1.0 : -1.0;
            const double cs = ((posCompressed ? 1.0 : -1.0) * sideOfShearCenter > 0.0) ? 1.0 : -1.0;
            const double cm = 1.0;
            const double jj = jParam * sideOfShearCenter;   // x_m e x_0 sempre positivos (E.1)
            me = cs * nexAnnex / cm * (cs * 0.0 + (jj + cs * std::sqrt(sq(jj) + sq(r0) * (nezA / nexAnnex))));
            me = std::abs(me);
            how = "Anexo E (monossimetrica fletida em torno do eixo perpendicular ao de simetria)";
            (void)nexM;
        } else {
            covered = false;
            how = "Me nao disponivel na norma para esta secao: FLT nao verificada";
        }
        if (covered && me > 0.0) {
            const double wc = posCompressed ? (major ? s.g.WxTop : s.g.WyRight) : (major ? s.g.WxBottom : s.g.WyLeft);
            const double lambda0 = std::sqrt(wc * fy / me);
            double chi;
            if (lambda0 <= 0.6) {
                chi = 1.0;
            } else if (lambda0 < 1.336) {
                chi = 1.11 * (1.0 - 0.278 * sq(lambda0));
            } else {
                chi = 1.0 / sq(lambda0);
            }
            const double sigmaFlt = chi * fy;
            const BendingSection bf = effectiveBending(s, tw, axis, posCompressed, sigmaFlt, E);
            const double mRd = chi * bf.Wc * fy / o.gammaFlexure;
            f.lines.push_back(fmt("FLT (9.8.2.2): %s; Me = %s, Cb = %.2f, lambda0 = %.3f, chi_FLT = %.3f, Wc,ef = %.0f mm3 -> M_Rd = %s", how.c_str(), knm(me).c_str(), cb,
                                  lambda0, chi, bf.Wc, knm(mRd).c_str()));
            if (mRd < f.MRd) {
                f.MRd = mRd;
                f.governing = "FLT";
            }
        } else {
            f.lines.push_back("FLT: " + how);
        }
    } else if (lb <= 0.0 && major) {
        f.lines.push_back("FLT: mesa comprimida contida lateralmente (Lb = 0): nao aplicavel.");
    }

    // 9.8.2.3: flambagem distorcional
    if (o.checkDistortional && isColdFormed(s.kind)) {
        bool dispensed = false;
        if (s.kind == SectionKind::ColdLippedChannel && major) {
            const double table = interpolateTable(kTable14, s.bf / s.d, s.d / s.t);
            if (s.lip / s.d >= table) {
                dispensed = true;
                f.lines.push_back(fmt("Flambagem distorcional dispensada (Tab. 14): D/bw = %.3f >= %.3f", s.lip / s.d, table));
            }
        }
        if (!dispensed) {
            const DistortionalResult dr = distortionalFactor(s, tw, E, true, axis, posCompressed);
            if (dr.available) {
                const double w = major ? (posCompressed ? s.g.WxTop : s.g.WxBottom) : (posCompressed ? s.g.WyRight : s.g.WyLeft);
                const double mdist = dr.factor * w;
                const double lambdaD = std::sqrt(w * fy / mdist);
                const double chiD = chiDistortionalFlexure(lambdaD);
                const double mRd = chiD * w * fy / o.gammaFlexure;
                f.lines.push_back(fmt("Flambagem distorcional (9.8.2.3): Mdist = %s (meia-onda %.0f mm); lambda_dist = %.3f; chi_dist = %.3f -> M_Rd = %s", knm(mdist).c_str(),
                                      dr.wavelength, lambdaD, chiD, knm(mRd).c_str()));
                if (mRd < f.MRd) {
                    f.MRd = mRd;
                    f.governing = "flambagem distorcional";
                }
            } else if (!dr.note.empty()) {
                f.lines.push_back("Flambagem distorcional: " + dr.note);
            }
        }
    }
    return f;
}

// ---------------------------------------------------------------------------------------------------------------------
// Cortante (9.8.3)
// ---------------------------------------------------------------------------------------------------------------------

ShearData shear(const DesignSection &s, const SteelGrade &steel, Axis direction, const Options &o)
{
    ShearData v;
    const double E = steel.E;
    const double fy = steel.fy;
    const double t = s.t;
    const bool alongB = (direction == Axis::Y);
    double h = 0.0;
    double webs = 1.0;
    switch (s.kind) {
    case SectionKind::ColdChannel:
    case SectionKind::ColdLippedChannel:
    case SectionKind::ColdZ:
        if (!alongB) {
            v.covered = false;
            return v;
        }
        h = s.d - 4.0 * t;
        webs = 1.0;
        break;
    case SectionKind::ColdHat:
        if (!alongB) {
            v.covered = false;
            return v;
        }
        h = s.d - 4.0 * t;
        webs = 2.0;
        break;
    case SectionKind::RectTube:
        h = alongB ? s.flatWidthH : s.flatWidthB;
        webs = 2.0;
        break;
    case SectionKind::ColdAngle:
        h = s.d - 2.0 * t;
        webs = std::sqrt(2.0);   // as duas abas a 45 graus em relacao aos eixos principais
        break;
    default:
        v.covered = false;
        return v;
    }
    const double kv = 5.0;
    const double lambda = h / t;
    const double l1 = 1.08 * std::sqrt(E * kv / fy);
    const double l2 = 1.4 * std::sqrt(E * kv / fy);
    double per;
    if (lambda <= l1) {
        per = 0.6 * fy * h * t / o.gammaFlexure;
    } else if (lambda <= l2) {
        per = 0.65 * t * t * std::sqrt(kv * fy * E) / o.gammaFlexure;
    } else {
        per = 0.905 * E * kv * t * t * t / h / o.gammaFlexure;
    }
    v.VRd = webs * per;
    v.lines.push_back(fmt("Cortante (9.8.3): h/t = %.1f, kv = %.1f, limites %.1f e %.1f -> V_Rd por alma = %s x %.2f alma(s) = %s", lambda, kv, l1, l2, kn(per).c_str(), webs,
                          kn(v.VRd).c_str()));
    return v;
}

} // namespace Structura::Design::Nbr14762
