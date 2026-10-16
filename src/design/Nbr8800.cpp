#include "design/Nbr8800.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <limits>

namespace Structura::Design::Nbr8800 {

namespace {

constexpr double kSigmaRFactor = 0.30;   // D.2.8-e: tensao residual = 30 % de fy

double sq(double v) { return v * v; }

std::string kn(double newtons) { return fmt("%.2f kN", newtons / 1e3); }
std::string knm(double nmm) { return fmt("%.2f kN*m", nmm / 1e6); }

/// (b/t)lim da Tabela 4 e coeficientes c1, c2 da Tabela 5.
struct PlateLimits {
    double bOverTLim {0.0};
    double c1 {0.18};
    double c2 {1.31};
};

PlateLimits plateLimits(const DesignSection &s, const PlateElement &e, const SteelGrade &steel)
{
    const double root = std::sqrt(steel.E / steel.fy);
    PlateLimits l;
    switch (e.group) {
    case 1: l.bOverTLim = 1.40 * root; break;
    case 2: l.bOverTLim = 1.49 * root; break;
    case 3: l.bOverTLim = 0.45 * root; break;
    case 4: l.bOverTLim = 0.56 * root; break;
    case 5: {
        const double kc = std::clamp(4.0 / std::sqrt(std::max(s.h, 1.0) / std::max(s.tw, 1.0)), 0.35, 0.76);
        l.bOverTLim = 0.64 * std::sqrt(steel.E / (steel.fy / kc));
        break;
    }
    default: l.bOverTLim = 0.75 * root; break;
    }
    if (e.group >= 3) {
        l.c1 = 0.22;
        l.c2 = 1.49;
    } else if (s.kind == SectionKind::RectTube) {
        l.c1 = 0.20;
        l.c2 = 1.38;
    }
    return l;
}

/// Area efetiva (5.3.4) para o fator de reducao `chi`. Preenche `lines`.
double effectiveArea(const DesignSection &s, const SteelGrade &steel, double chi, std::vector<std::string> &lines)
{
    if (s.kind == SectionKind::RoundTube) {
        const double dt = s.D / s.t;
        const double lim1 = 0.11 * steel.E / steel.fy;
        const double lim2 = 0.45 * steel.E / steel.fy;
        if (dt <= lim1) {
            lines.push_back(fmt("D/t = %.1f <= 0,11E/fy = %.1f: Aef = Ag (5.3.4.3)", dt, lim1));
            return s.g.area;
        }
        const double qs = 0.038 * steel.E / (steel.fy * dt) + 2.0 / 3.0;
        lines.push_back(fmt("D/t = %.1f > 0,11E/fy = %.1f: Aef = (0,038E/(fy D/t) + 2/3) Ag = %.3f Ag (5.3.4.3)%s", dt, lim1, qs,
                            dt > lim2 ? "  [D/t acima de 0,45E/fy: fora do campo da norma]" : ""));
        return qs * s.g.area;
    }
    double lost = 0.0;
    bool slender = false;
    for (const PlateElement &e : s.elements) {
        if (e.b <= 0.0 || e.t <= 0.0) {
            continue;
        }
        const PlateLimits lim = plateLimits(s, e, steel);
        const double bt = e.b / e.t;
        if (bt <= lim.bOverTLim / std::sqrt(chi)) {
            lines.push_back(fmt("%s: b/t = %.1f <= (b/t)lim/sqrt(chi) = %.1f: bef = b", e.name.c_str(), bt, lim.bOverTLim / std::sqrt(chi)));
            continue;
        }
        slender = true;
        const double sigmaEl = sq(lim.c2 * lim.bOverTLim / bt) * steel.fy;
        const double ratio = std::sqrt(sigmaEl / (chi * steel.fy));
        const double bef = e.b * (1.0 - lim.c1 * ratio) * ratio;
        lost += e.count * (e.b - bef) * e.t;
        lines.push_back(fmt("%s: b/t = %.1f > %.1f -> sigma_el = %.1f MPa, bef = %.1f mm (b = %.1f mm) (5.3.4.2)", e.name.c_str(), bt,
                            lim.bOverTLim / std::sqrt(chi), sigmaEl, bef, e.b));
    }
    if (!slender) {
        lines.push_back("Todos os elementos com b/t <= (b/t)lim: Aef = Ag (5.3.4.1)");
    }
    return s.g.area - lost;
}

double chiFactor(double lambda0)
{
    return lambda0 <= 1.5 ? std::pow(0.658, sq(lambda0)) : 0.877 / sq(lambda0);
}

/// Menor raiz positiva do cubico de flambagem de perfis assimetricos (5.3.5.3).
double asymmetricBuckling(double nex, double ney, double nez, double x0, double y0, double r0)
{
    // (Ne-Nex)(Ne-Ney)(Ne-Nez) - Ne^2 (Ne-Ney)(x0/r0)^2 - Ne^2 (Ne-Nex)(y0/r0)^2 = 0
    const double px = sq(x0 / r0);
    const double py = sq(y0 / r0);
    auto f = [&](double n) {
        return (n - nex) * (n - ney) * (n - nez) - sq(n) * (n - ney) * px - sq(n) * (n - nex) * py;
    };
    double lo = 0.0;
    double hi = std::min({nex, ney, nez});
    // f(0) < 0 e f(min) >= 0: raiz entre 0 e o menor dos tres
    if (f(hi) < 0.0) {
        hi *= 1.0 + 1e-9;
    }
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (f(mid) < 0.0) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return 0.5 * (lo + hi);
}

} // namespace

std::string fmt(const char *format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}

double cbFromQuarterMoments(double mMax, double mA, double mB, double mC, double rm)
{
    mMax = std::abs(mMax);
    mA = std::abs(mA);
    mB = std::abs(mB);
    mC = std::abs(mC);
    const double den = 2.5 * mMax + 3.0 * mA + 4.0 * mB + 3.0 * mC;
    if (den <= 1e-12) {
        return 1.0;
    }
    return 12.5 * mMax / den * rm;
}

// ---------------------------------------------------------------------------------------------------------------------
// Tracao (5.2)
// ---------------------------------------------------------------------------------------------------------------------

TensionData tension(const DesignSection &s, const SteelGrade &steel, const Options &o)
{
    TensionData t;
    const double ag = s.g.area;
    const double ae = o.ct * o.netAreaRatio * ag;
    t.yieldRd = ag * steel.fy / o.gammaA1;
    t.ruptureRd = ae * steel.fu / o.gammaA2;
    t.NtRd = std::min(t.yieldRd, t.ruptureRd);
    t.lines.push_back(fmt("Escoamento da secao bruta: Ag fy / gama_a1 = %.1f x %.0f / %.2f = %s", ag, steel.fy, o.gammaA1, kn(t.yieldRd).c_str()));
    t.lines.push_back(fmt("Ruptura da secao liquida: Ae fu / gama_a2 = (Ct An) fu / %.2f = (%.2f x %.2f x Ag) x %.0f / %.2f = %s", o.gammaA2,
                          o.ct, o.netAreaRatio, steel.fu, o.gammaA2, kn(t.ruptureRd).c_str()));
    return t;
}

// ---------------------------------------------------------------------------------------------------------------------
// Compressao (5.3)
// ---------------------------------------------------------------------------------------------------------------------

CompressionData compression(const DesignSection &s, const SteelGrade &steel, const Buckling &len, const Options &o)
{
    CompressionData c;
    const double E = steel.E;
    const double G = steel.G;
    const double ag = s.g.area;
    const double ix = s.g.Ix;
    const double iy = s.g.Iy;
    const double rx = s.g.rx;
    const double ry = s.g.ry;

    c.slenderness = std::max(len.Lx > 0 ? len.Lx / rx : 0.0, len.Ly > 0 ? len.Ly / ry : 0.0);

    const double inf = std::numeric_limits<double>::infinity();
    const double nex = len.Lx > 0 ? sq(kPi) * E * ix / sq(len.Lx) : inf;
    const double ney = len.Ly > 0 ? sq(kPi) * E * iy / sq(len.Ly) : inf;
    const double r0sq = sq(rx) + sq(ry) + sq(s.x0) + sq(s.y0);
    const double r0 = std::sqrt(r0sq);
    const bool torsionOpen = s.Cw > 0.0 || s.J > 0.0;
    const double nez = (len.Lz > 0 && torsionOpen)
        ? (sq(kPi) * E * s.Cw / sq(len.Lz) + G * s.J) / r0sq
        : (torsionOpen && s.J > 0.0 ? G * s.J / r0sq : inf);

    double ne = std::min({nex, ney, nez});
    c.mode = (ne == nex) ? "flexao em torno de X" : (ne == ney) ? "flexao em torno de Y" : "torcao";

    c.lines.push_back(fmt("Nex = pi2 E Ix / Lx2 = %s;  Ney = pi2 E Iy / Ly2 = %s;  Nez = [pi2 E Cw / Lz2 + G J] / r0^2 = %s",
                          std::isinf(nex) ? "inf" : kn(nex).c_str(), std::isinf(ney) ? "inf" : kn(ney).c_str(),
                          std::isinf(nez) ? "inf" : kn(nez).c_str()));

    const bool monoSym = (s.kind == SectionKind::Channel || s.kind == SectionKind::Tee);
    if (monoSym) {
        // Eixo de simetria: X para o U (alma vertical), Y para o T. Flexo-torcao acopla a flexao em torno do eixo de simetria.
        const bool symX = (s.kind == SectionKind::Channel);
        const double nsym = symX ? nex : ney;
        const double nother = symX ? ney : nex;
        const double offset = symX ? s.x0 : s.y0;
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
        c.lines.push_back(fmt("Secao monossimetrica (5.3.5.2): Nflexo-torcao = %s (x0 = %.1f mm, y0 = %.1f mm, r0 = %.1f mm); Ne = min(Nflexao, Nflexo-torcao) = %s",
                              std::isinf(nft) ? "inf" : kn(nft).c_str(), s.x0, s.y0, r0, kn(ne).c_str()));
    } else if (s.kind == SectionKind::Angle) {
        // Cantoneira simples: 5.3.5.4 (conectada por uma aba) ou flexo-torcao (5.3.5.3)
        const double legTest = s.leg / s.t;
        const double ixLeg = 0.5 * (s.I1 + s.I2);   // abas iguais: inercia em torno do eixo paralelo a uma aba
        const double rLeg = std::sqrt(ixLeg / ag);
        const double lx = len.Lx;
        const double lambdaLeg = lx / rLeg;
        double leq = (lambdaLeg <= 80.0) ? 72.0 * rLeg + 0.75 * lx : 32.0 * rLeg + 1.25 * lx;
        const bool okConditions = legTest <= 0.71 * std::sqrt(E / steel.fy) && leq / rLeg <= 200.0;
        if (okConditions) {
            ne = sq(kPi) * E * ixLeg / sq(leq);
            c.mode = "cantoneira conectada por uma aba (5.3.5.4)";
            c.slenderness = leq / rLeg;
            c.lines.push_back(fmt("Cantoneira simples (5.3.5.4.2, trelica plana): L/r = %.1f -> Lx1,eq = %.1f mm; Ne = pi2 E Ix1 / Lx1,eq2 = %s",
                                  lambdaLeg, leq, kn(ne).c_str()));
        } else {
            const double nex2 = len.Lx > 0 ? sq(kPi) * E * s.g.Ix / sq(len.Lx) : inf;
            const double ney2 = len.Ly > 0 ? sq(kPi) * E * s.g.Iy / sq(len.Ly) : inf;
            const double nez2 = G * s.J / r0sq;
            if (!std::isinf(nex2) && !std::isinf(ney2)) {
                ne = asymmetricBuckling(nex2, ney2, nez2, s.x0, s.y0, r0);
            } else {
                ne = std::min({nex2, ney2, nez2});
            }
            c.mode = "flexo-torcao (secao assimetrica, 5.3.5.3)";
            c.lines.push_back(fmt("Cantoneira fora das condicoes de 5.3.5.4 (b/t = %.1f, KL/r = %.1f): menor raiz do cubico de 5.3.5.3 = %s", legTest,
                                  leq / rLeg, kn(ne).c_str()));
        }
    }

    if (std::isinf(ne)) {
        c.covered = true;
        c.Ne = ne;
        c.lambda0 = 0.0;
        c.chi = 1.0;
        c.Aef = effectiveArea(s, steel, 1.0, c.lines);
        c.NcRd = c.Aef * steel.fy / o.gammaA1;
        c.lines.push_back("Comprimentos de flambagem nulos: sem reducao por flambagem global (chi = 1,0).");
        return c;
    }

    c.Ne = ne;
    c.lambda0 = std::sqrt(ag * steel.fy / ne);
    c.chi = chiFactor(c.lambda0);
    c.lines.push_back(fmt("Ne = %s (%s); lambda0 = sqrt(Ag fy / Ne) = %.3f; chi = %.3f (5.3.3)", kn(ne).c_str(), c.mode.c_str(), c.lambda0, c.chi));
    c.Aef = effectiveArea(s, steel, c.chi, c.lines);
    c.NcRd = c.chi * c.Aef * steel.fy / o.gammaA1;
    c.lines.push_back(fmt("Nc,Rd = chi Aef fy / gama_a1 = %.3f x %.1f x %.0f / %.2f = %s", c.chi, c.Aef, steel.fy, o.gammaA1, kn(c.NcRd).c_str()));
    return c;
}

// ---------------------------------------------------------------------------------------------------------------------
// Flexao (5.4 e Anexos D e E)
// ---------------------------------------------------------------------------------------------------------------------

namespace {

struct LimitState {
    std::string name;
    double MRd {0.0};
    std::string detail;
};

/// Aplica D.2.1/D.2.2: M_Rd conforme a esbeltez λ em relacao a λp e λr.
double interpolate(double lambda, double lambdaP, double lambdaR, double mpl, double mr, double mcr, double gamma, bool mcrAllowed = true)
{
    if (lambda <= lambdaP) {
        return mpl / gamma;
    }
    if (lambda <= lambdaR) {
        return (mpl - (mpl - mr) * (lambda - lambdaP) / (lambdaR - lambdaP)) / gamma;
    }
    return mcrAllowed ? mcr / gamma : mr / gamma;
}

FlexureData finish(std::vector<LimitState> states, double mpl, double wMin, const SteelGrade &steel, const Options &o, std::vector<std::string> lines)
{
    FlexureData f;
    f.Mpl = mpl;
    f.MRd = std::numeric_limits<double>::infinity();
    for (const LimitState &st : states) {
        lines.push_back(st.detail);
        if (st.MRd < f.MRd) {
            f.MRd = st.MRd;
            f.governing = st.name;
        }
    }
    const double cap = 1.5 * wMin * steel.fy / o.gammaA1;   // 5.4.2.2
    if (f.MRd > cap) {
        lines.push_back(fmt("Limite 5.4.2.2: M_Rd <= 1,50 W fy / gama_a1 = %s", knm(cap).c_str()));
        f.MRd = cap;
        f.governing += " (limitado por 5.4.2.2)";
    }
    f.lines = std::move(lines);
    return f;
}

double mcrFlt(const DesignSection &s, const SteelGrade &steel, double lb, double cb)
{
    // D.2.8-a: Mcr = Cb pi2 E Iy / Lb2 * sqrt(Cw/Iy (1 + 0,039 J Lb2 / Cw))
    const double E = steel.E;
    return cb * sq(kPi) * E * s.g.Iy / sq(lb) * std::sqrt(s.Cw / s.g.Iy * (1.0 + 0.039 * s.J * sq(lb) / s.Cw));
}

double lambdaRFlt(const DesignSection &s, const SteelGrade &steel, double cb, double w)
{
    // D.2.8-a: lambda_r = 1,38 Cb sqrt(Iy J) / (ry J beta1) * sqrt(1 + sqrt(1 + 27 Cw beta1^2 / (Cb^2 Iy)))
    const double E = steel.E;
    const double sigmaR = kSigmaRFactor * steel.fy;
    const double beta1 = (steel.fy - sigmaR) * w / (E * s.J);
    return 1.38 * cb * std::sqrt(s.g.Iy * s.J) / (s.g.ry * s.J * beta1) *
           std::sqrt(1.0 + std::sqrt(1.0 + 27.0 * s.Cw * sq(beta1) / (sq(cb) * s.g.Iy)));
}

/// Viga I/H de alma esbelta (Anexo E).
FlexureData slenderWebI(const DesignSection &s, const SteelGrade &steel, double lb, double cb, const Options &o)
{
    std::vector<std::string> lines;
    const double E = steel.E;
    const double fy = steel.fy;
    const double sigmaR = kSigmaRFactor * fy;
    const double wxc = s.g.Wx();
    const double hc = s.h;                      // duplamente simetrica: hc = h
    const double afc = s.bf * s.tf;
    const double ar = std::min(10.0, hc * s.tw / afc);
    const double kpg = std::min(1.0, 1.0 - ar / (1200.0 + 300.0 * ar) * (hc / s.tw - 5.70 * std::sqrt(E / fy)));
    std::vector<LimitState> states;

    const double mYield = wxc * fy / o.gammaA1;
    states.push_back({"escoamento da mesa tracionada", mYield, fmt("E.2.1: M_Rd = Wxt fy / gama_a1 = %s", knm(mYield).c_str())});

    const double my = kpg * fy * wxc;
    const double mr = kpg * (fy - sigmaR) * wxc;
    if (lb > 0.0) {
        const double ryc = s.g.ry * 1.0;   // aproximacao: ry da secao (mesa comprimida + 1/3 da alma)
        const double lambda = lb / ryc;
        const double ly = 1.10 * std::sqrt(E / fy);
        const double lr = kPi * std::sqrt(E * cb / (fy - sigmaR));
        const double mcr = cb * kpg * sq(kPi) * E * wxc / sq(lambda);
        const double m = interpolate(lambda, ly, lr, my, mr, mcr, o.gammaA1);
        states.push_back({"FLT", m, fmt("E.2.2 (FLT): lambda = %.1f, lambda_y = %.1f, lambda_r = %.1f, kpg = %.3f -> M_Rd = %s", lambda, ly, lr, kpg, knm(m).c_str())});
    }
    {
        const double lambda = s.bf / (2.0 * s.tf);
        const double ly = 0.38 * std::sqrt(E / fy);
        const double kc = std::clamp(4.0 / std::sqrt(s.h / s.tw), 0.35, 0.76);
        const double lr = 0.95 * std::sqrt(kc * E / (fy - sigmaR));
        const double mcr = 0.90 * kpg * E * kc * wxc / sq(lambda);
        const double m = interpolate(lambda, ly, lr, my, mr, mcr, o.gammaA1);
        states.push_back({"FLM", m, fmt("E.2.3 (FLM): lambda = %.1f, lambda_y = %.1f, lambda_r = %.1f -> M_Rd = %s", lambda, ly, lr, knm(m).c_str())});
    }
    lines.push_back(fmt("Alma esbelta (h/tw = %.1f > 5,70 sqrt(E/fy) = %.1f): Anexo E.", s.h / s.tw, 5.70 * std::sqrt(E / fy)));
    return finish(states, s.g.Zx * fy, wxc, steel, o, lines);
}

} // namespace

FlexureData flexure(const DesignSection &s, const SteelGrade &steel, Axis axis, bool positiveSideCompressed, double lb, double cb, const Options &o)
{
    const double E = steel.E;
    const double fy = steel.fy;
    const double sigmaR = kSigmaRFactor * fy;
    const double gamma = o.gammaA1;
    std::vector<LimitState> states;
    std::vector<std::string> lines;
    FlexureData none;
    none.covered = false;

    const bool major = (axis == Axis::X);
    const double w = major ? s.g.Wx() : s.g.Wy();
    const double mpl = (major ? s.g.Zx : s.g.Zy) * fy;

    switch (s.kind) {
    case SectionKind::I:
    case SectionKind::Channel: {
        const bool isU = (s.kind == SectionKind::Channel);
        if (major) {
            const double web = s.h / s.tw;
            if (!isU && web > 5.70 * std::sqrt(E / fy)) {
                return slenderWebI(s, steel, lb, cb, o);
            }
            if (lb > 0.0) {
                const double lambda = lb / s.g.ry;
                const double lp = 1.76 * std::sqrt(E / fy);
                const double lr = lambdaRFlt(s, steel, cb, w);
                const double mr = (fy - sigmaR) * w;
                const double mcr = mcrFlt(s, steel, lb, cb);
                const double m = interpolate(lambda, lp, lr, mpl, mr, mcr, gamma);
                states.push_back({"FLT", m, fmt("FLT (D.2.1, Tab. D.1): Lb = %.0f mm, lambda = Lb/ry = %.1f, lambda_p = %.1f, lambda_r = %.1f, Cb = %.2f, Mcr = %s -> M_Rd = %s",
                                                lb, lambda, lp, lr, cb, knm(mcr).c_str(), knm(m).c_str())});
            } else {
                lines.push_back("FLT: barra com mesa comprimida contida lateralmente (Lb = 0): estado-limite nao aplicavel.");
            }
            {
                const double lambda = (isU ? s.bf : s.bf / 2.0) / s.tf;
                const double lp = 0.38 * std::sqrt(E / fy);
                const double lr = 0.83 * std::sqrt(E / (fy - sigmaR));
                const double mr = (fy - sigmaR) * w;
                const double mcr = 0.69 * E * w / sq(lambda);
                const double m = interpolate(lambda, lp, lr, mpl, mr, mcr, gamma);
                states.push_back({"FLM", m, fmt("FLM (D.2.2): b/t = %.1f, lambda_p = %.1f, lambda_r = %.1f -> M_Rd = %s", lambda, lp, lr, knm(m).c_str())});
            }
            {
                const double lambda = web;
                const double lp = 3.76 * std::sqrt(E / fy);
                const double lr = 5.70 * std::sqrt(E / fy);
                const double mr = fy * w;
                const double m = interpolate(lambda, lp, lr, mpl, mr, mr, gamma, false);
                states.push_back({"FLA", m, fmt("FLA (D.2.2): h/tw = %.1f, lambda_p = %.1f, lambda_r = %.1f -> M_Rd = %s", lambda, lp, lr, knm(m).c_str())});
            }
        } else {
            // Eixo de menor inercia: FLM (mesas) e, no U, FLA da alma quando comprimida
            const double lambda = (isU ? s.bf : s.bf / 2.0) / s.tf;
            const double lp = 0.38 * std::sqrt(E / fy);
            const double lr = 0.83 * std::sqrt(E / (fy - sigmaR));
            const double mr = (fy - sigmaR) * w;
            const double mcr = 0.69 * E * w / sq(lambda);
            const double m = interpolate(lambda, lp, lr, mpl, mr, mcr, gamma);
            // No U, a ponta livre das mesas so e comprimida quando a abertura esta do lado comprimido.
            bool flangeTipsCompressed = true;
            if (isU) {
                const bool openOnPositiveSide = s.webLeft;   // abertura para +a
                flangeTipsCompressed = (positiveSideCompressed == openOnPositiveSide);
            }
            if (flangeTipsCompressed) {
                states.push_back({"FLM", m, fmt("FLM (D.2.2, eixo de menor inercia): b/t = %.1f, lambda_p = %.1f, lambda_r = %.1f -> M_Rd = %s", lambda, lp, lr, knm(m).c_str())});
            } else {
                lines.push_back("FLM nao se aplica: pontas das mesas do U tracionadas (D.2.8-c).");
            }
            if (isU && !flangeTipsCompressed) {
                // Alma comprimida: FLA com Wef (D.2.8-d), lambda_p = 1,12, lambda_r = 1,40
                const double lambdaW = s.h / s.tw;
                const double lpW = 1.12 * std::sqrt(E / fy);
                const double lrW = 1.40 * std::sqrt(E / fy);
                const double mrW = fy * w;
                const double m2 = interpolate(lambdaW, lpW, lrW, mpl, mrW, mrW, gamma, false);
                states.push_back({"FLA", m2, fmt("FLA (D.2.2, alma do U comprimida): h/tw = %.1f, lambda_p = %.1f, lambda_r = %.1f -> M_Rd = %s", lambdaW, lpW, lrW, knm(m2).c_str())});
            }
            if (states.empty()) {
                states.push_back({"plastificacao", mpl / gamma, fmt("Nenhum estado-limite de flambagem aplicavel: M_Rd = Mpl / gama_a1 = %s", knm(mpl / gamma).c_str())});
            }
        }
        return finish(states, mpl, w, steel, o, lines);
    }
    case SectionKind::Tee: {
        if (!major) {
            // Flexao da T em torno do eixo vertical: FLM das mesas
            const double lambda = (s.bf / 2.0) / s.tf;
            const double lp = 0.38 * std::sqrt(E / fy);
            const double lr = 0.83 * std::sqrt(E / (fy - sigmaR));
            const double mr = (fy - sigmaR) * w;
            const double mcr = 0.7 * E * w / sq(lambda);
            const double m = interpolate(lambda, lp, lr, mpl, mr, mcr, gamma);
            states.push_back({"FLM", m, fmt("FLM (D.2.3): b/t = %.1f -> M_Rd = %s", lambda, knm(m).c_str())});
            return finish(states, mpl, w, steel, o, lines);
        }
        const bool flangeCompressed = (positiveSideCompressed == s.flangeUp);
        const double wc = positiveSideCompressed ? s.g.WxTop : s.g.WxBottom;
        const double wt = positiveSideCompressed ? s.g.WxBottom : s.g.WxTop;
        const double mplT = s.g.Zx * fy;
        if (flangeCompressed) {
            const double lambda = (s.bf / 2.0) / s.tf;
            const double lp = 0.38 * std::sqrt(E / fy);
            const double lr = 0.83 * std::sqrt(E / (fy - sigmaR));
            const double mr = std::min((fy - sigmaR) * wc, fy * wt);
            const double mcr = 0.7 * E * wc / sq(lambda);
            const double m = interpolate(lambda, lp, lr, mplT, mr, mcr, gamma);
            states.push_back({"FLM", m, fmt("FLM (D.2.3, mesa comprimida): b/t = %.1f, lambda_p = %.1f, lambda_r = %.1f -> M_Rd = %s", lambda, lp, lr, knm(m).c_str())});
        } else {
            const double lambda = s.d / s.tw;
            const double ly = 0.84 * std::sqrt(E / fy);
            const double lr = 1.52 * std::sqrt(E / fy);
            const double wx = wc;
            double m;
            if (lambda <= ly) {
                m = fy * wx / gamma;
            } else if (lambda <= lr) {
                m = (1.43 - 0.515 * lambda * std::sqrt(fy / E)) * fy * wx / gamma;
            } else {
                m = 1.52 * E * wx / sq(lambda) / gamma;
            }
            states.push_back({"FLA", m, fmt("FLA (D.2.3, ponta livre da alma comprimida): d/tw = %.1f, lambda_y = %.1f, lambda_r = %.1f -> M_Rd = %s", lambda, ly, lr, knm(m).c_str())});
        }
        if (lb > 0.0) {
            const double bParam = 2.3 * (s.d / lb) * std::sqrt(s.g.Iy / s.J);
            const double mcrBase = 1.95 * E / lb * std::sqrt(s.g.Iy * s.J);
            const double wx = wc;
            if (flangeCompressed) {
                // alma tracionada
                const double mcr = mcrBase * (bParam + std::sqrt(1.0 + sq(bParam)));
                const double lambda = lb / s.g.ry;
                const double lp = 1.76 * std::sqrt(E / fy);
                const double lr = 1.95 * E / (s.g.ry * fy) * std::sqrt(s.g.Iy * s.J) / wx * std::sqrt(2.36 * fy * s.d * wx / (E * s.J) + 1.0);
                const double mr = fy * wx;
                const double m = interpolate(lambda, lp, lr, mplT, mr, mcr, gamma);
                states.push_back({"FLT", m, fmt("FLT (D.2.3, alma tracionada): Lb = %.0f mm, lambda = %.1f, lambda_p = %.1f, lambda_r = %.1f, Mcr = %s -> M_Rd = %s", lb, lambda, lp, lr, knm(mcr).c_str(), knm(m).c_str())});
            } else {
                const double mcr = mcrBase * (-bParam + std::sqrt(1.0 + sq(bParam)));
                const double m = std::min(mcr, fy * wx) / gamma;
                states.push_back({"FLT", m, fmt("FLT (D.2.3, alma comprimida): Mcr = %s <= fy Wx -> M_Rd = %s", knm(mcr).c_str(), knm(m).c_str())});
            }
        } else {
            lines.push_back("FLT: Lb = 0, estado-limite nao aplicavel.");
        }
        return finish(states, mplT, std::min(s.g.WxTop, s.g.WxBottom), steel, o, lines);
    }
    case SectionKind::RectTube: {
        const double wall = major ? s.flatWidthB : s.flatWidthH;   // parede comprimida paralela ao eixo
        const double other = major ? s.flatWidthH : s.flatWidthB;  // parede perpendicular (alma)
        {
            const double lambda = wall / s.t;
            const double lp = 1.12 * std::sqrt(E / fy);
            const double lr = 1.40 * std::sqrt(E / fy);
            double m;
            if (lambda <= lp) {
                m = mpl / gamma;
            } else if (lambda <= lr) {
                m = (mpl - (mpl - fy * w) * (3.57 * lambda * std::sqrt(fy / E) - 4.0)) / gamma;
            } else {
                // Wef com b_ef (5.3.4.2) para lambda_r e chi = 1
                const double blim = 1.40 * std::sqrt(E / fy);
                const double sigmaEl = sq(1.38 * blim / lambda) * fy;
                const double ratio = std::sqrt(sigmaEl / fy);
                const double bef = wall * (1.0 - 0.20 * ratio) * ratio;
                const double loss = (wall - bef) * s.t;
                const double yTop = (major ? s.g.bMax : s.g.aMax) - s.t / 2.0;
                const double wef = w - loss * sq(yTop) / (major ? s.g.bMax : s.g.aMax);
                m = fy * wef / gamma;
            }
            states.push_back({"FLM", m, fmt("FLM (D.2.7): b/t = %.1f, lambda_p = %.1f, lambda_r = %.1f -> M_Rd = %s", lambda, lp, lr, knm(m).c_str())});
        }
        {
            const double lambda = other / s.t;
            const double lp = 2.42 * std::sqrt(E / fy);
            const double lr = 5.70 * std::sqrt(E / fy);
            const double mr = fy * w;
            double m = interpolate(lambda, lp, lr, mpl, mr, mr, gamma, false);
            if (lambda > lr) {
                m = mr / gamma * 0.0;   // alma esbelta: Anexo E nao implementado para tubulares
                states.push_back({"FLA", 0.0, fmt("FLA: h/t = %.1f > lambda_r = %.1f: alma esbelta (Anexo E nao implementado para tubulares)", lambda, lr)});
            } else {
                states.push_back({"FLA", m, fmt("FLA (D.2.7): h/t = %.1f, lambda_p = %.1f, lambda_r = %.1f -> M_Rd = %s", lambda, lp, lr, knm(m).c_str())});
            }
        }
        if (major && lb > 0.0 && s.g.Ix > s.g.Iy) {
            const double lambda = lb / s.g.ry;
            const double ja = std::sqrt(s.J * s.g.area);
            const double lp = 0.13 * E * ja / mpl;
            const double mr = (fy - sigmaR) * w;
            const double lr = 2.0 * E * cb * ja / mr;
            const double mcr = 2.0 * E * cb * ja / lambda;
            const double m = interpolate(lambda, lp, lr, mpl, mr, mcr, gamma);
            states.push_back({"FLT", m, fmt("FLT (Tab. D.1, caixao/tubular): lambda = %.1f, lambda_p = %.1f, lambda_r = %.1f -> M_Rd = %s", lambda, lp, lr, knm(m).c_str())});
        }
        return finish(states, mpl, w, steel, o, lines);
    }
    case SectionKind::RoundTube: {
        const double dt = s.D / s.t;
        const double lp = 0.07 * E / fy;
        const double lr = 0.31 * E / fy;
        double m;
        if (dt <= lp) {
            m = mpl / gamma;
        } else if (dt <= lr) {
            m = (0.021 * E / dt + fy) * w / gamma;
        } else {
            m = 0.33 * E / dt * w / gamma;
        }
        states.push_back({"FLM", m, fmt("Parede do tubo (D.2.6): D/t = %.1f, lambda_p = %.1f, lambda_r = %.1f -> M_Rd = %s%s", dt, lp, lr, knm(m).c_str(),
                                        dt > 0.45 * E / fy ? "  [D/t > 0,45E/fy: fora do campo da norma]" : "")});
        return finish(states, mpl, w, steel, o, lines);
    }
    case SectionKind::SolidRound: {
        states.push_back({"plastificacao", mpl / gamma, fmt("Secao macica circular (D.2.5): M_Rd = Mpl / gama_a1 = %s", knm(mpl / gamma).c_str())});
        return finish(states, mpl, w, steel, o, lines);
    }
    case SectionKind::SolidRect: {
        if (major && lb > 0.0 && s.g.Ix > s.g.Iy) {
            const double lambda = lb / s.g.ry;
            const double ja = std::sqrt(s.J * s.g.area);
            const double lp = 0.13 * E * ja / mpl;
            const double mr = fy * w;
            const double lr = 2.0 * E * cb * ja / mr;
            const double mcr = 2.0 * E * cb * ja / lambda;
            const double m = interpolate(lambda, lp, lr, mpl, mr, mcr, gamma);
            states.push_back({"FLT", m, fmt("FLT (Tab. D.1, macica retangular): lambda = %.1f, lambda_p = %.1f, lambda_r = %.1f -> M_Rd = %s", lambda, lp, lr, knm(m).c_str())});
        } else {
            states.push_back({"plastificacao", mpl / gamma, fmt("Secao macica retangular (D.2.5): M_Rd = Mpl / gama_a1 = %s", knm(mpl / gamma).c_str())});
        }
        return finish(states, mpl, w, steel, o, lines);
    }
    default:
        return none;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Cortante (5.4.3)
// ---------------------------------------------------------------------------------------------------------------------

namespace {

ShearData plateShear(double h, double tw, double aw, double kv, const SteelGrade &steel, const Options &o, const char *label)
{
    ShearData v;
    const double E = steel.E;
    const double fy = steel.fy;
    const double lambda = h / tw;
    const double lp = 1.10 * std::sqrt(kv * E / fy);
    const double lr = 1.37 * std::sqrt(kv * E / fy);
    const double vpl = 0.60 * aw * fy;
    if (lambda <= lp) {
        v.VRd = vpl / o.gammaA1;
    } else if (lambda <= lr) {
        v.VRd = lp / lambda * vpl / o.gammaA1;
    } else {
        v.VRd = 1.24 * sq(lp / lambda) * vpl / o.gammaA1;
    }
    v.lines.push_back(fmt("%s: lambda = h/tw = %.1f, kv = %.2f, lambda_p = %.1f, lambda_r = %.1f, Vpl = 0,60 Aw fy = %s -> V_Rd = %s", label, lambda, kv, lp, lr,
                          kn(vpl).c_str(), kn(v.VRd).c_str()));
    return v;
}

} // namespace

ShearData shear(const DesignSection &s, const SteelGrade &steel, Axis direction, const Options &o, double lv)
{
    const bool alongB = (direction == Axis::Y);   // forca paralela a b
    switch (s.kind) {
    case SectionKind::I:
    case SectionKind::Channel: {
        if (alongB) {
            return plateShear(s.h, s.tw, s.d * s.tw, s.h / s.tw > 0 ? 5.34 : 5.34, steel, o, "Cortante na alma (5.4.3.1.1)");
        }
        const double h = (s.kind == SectionKind::I) ? s.bf / 2.0 : s.bf;
        return plateShear(h, s.tf, 2.0 * s.bf * s.tf, 1.2, steel, o, "Cortante nas mesas (5.4.3.5)");
    }
    case SectionKind::Tee: {
        if (alongB) {
            return plateShear(s.d, s.tw, s.d * s.tw, 1.2, steel, o, "Cortante na alma do T (5.4.3.3)");
        }
        return plateShear(s.bf / 2.0, s.tf, s.bf * s.tf, 1.2, steel, o, "Cortante nas mesas do T");
    }
    case SectionKind::RectTube: {
        const double h = alongB ? s.flatWidthH : s.flatWidthB;
        return plateShear(h, s.t, 2.0 * h * s.t, 5.0, steel, o, "Cortante nas paredes do tubular (5.4.3.2)");
    }
    case SectionKind::RoundTube: {
        ShearData v;
        const double dt = s.D / s.t;
        const double lvOverD = lv > 0.0 ? lv / s.D : 1e6;   // sem Lv informado, so a segunda expressao governa
        const double t1 = 1.60 * steel.E / (std::sqrt(lvOverD) * std::pow(dt, 1.25));
        const double t2 = 0.78 * steel.E / std::pow(dt, 1.5);
        const double tau = std::min(std::max(t1, t2), 0.60 * steel.fy);
        v.VRd = 0.5 * tau * s.g.area / o.gammaA1;
        v.lines.push_back(fmt("Tubular circular (5.4.3.6): tau_cr = %.1f MPa (Lv/D = %.1f) -> V_Rd = 0,5 tau_cr Ag / gama_a1 = %s", tau, lvOverD, kn(v.VRd).c_str()));
        return v;
    }
    case SectionKind::SolidRound:
    case SectionKind::SolidRect: {
        ShearData v;
        // Nao ha item especifico para secoes macicas: tensao cisalhante maxima (teoria da elasticidade) <= 0,60 fy / gama_a1 (5.5.2.3-b)
        v.VRd = 0.60 * steel.fy / o.gammaA1 * s.g.area * 3.0 / 4.0;
        v.lines.push_back(fmt("Secao macica (5.5.2.3-b): tau_max = (4/3) V / A <= 0,60 fy / gama_a1 -> V_Rd = %s", kn(v.VRd).c_str()));
        return v;
    }
    case SectionKind::Angle: {
        ShearData v;
        // Forca cortante segundo um eixo principal: as duas abas a 45 graus a resistem. Cada aba, com tensao
        // maxima parabolica (tau_max = 1,5 V_aba / (b t)) limitada a 0,60 fy / gama_a1 (5.5.2.3-b).
        const double perLeg = (2.0 / 3.0) * 0.60 * steel.fy * s.leg * s.t / o.gammaA1;
        v.VRd = std::sqrt(2.0) * perLeg;
        v.lines.push_back(fmt("Cantoneira (5.5.2.3-b): V_Rd = sqrt(2) x (2/3) x 0,60 fy b t / gama_a1 = %s", kn(v.VRd).c_str()));
        return v;
    }
    default:
        break;
    }
    ShearData none;
    none.covered = false;
    return none;
}

// ---------------------------------------------------------------------------------------------------------------------
// Torcao (5.5.2.1) - tubulares
// ---------------------------------------------------------------------------------------------------------------------

TorsionData torsion(const DesignSection &s, const SteelGrade &steel, double barLength, const Options &o)
{
    TorsionData t;
    const double E = steel.E;
    const double fy = steel.fy;
    if (s.kind == SectionKind::RoundTube) {
        const double wt = kPi * sq(s.D - s.t) * s.t / 2.0;
        const double dt = s.D / s.t;
        const double a = 1.23 * wt * E / (std::pow(dt, 1.25) * std::sqrt(barLength / s.D));
        const double b = 0.60 * wt * E / std::pow(dt, 1.5);
        t.TRd = std::min(std::max(a, b) / o.gammaA1, 0.60 * wt * fy / o.gammaA1);
        t.covered = true;
        t.lines.push_back(fmt("Torcao em tubular circular (5.5.2.1.2): WT = %.0f mm3 -> T_Rd = %s", wt, knm(t.TRd).c_str()));
    } else if (s.kind == SectionKind::RectTube) {
        const double b = s.bf;
        const double h = s.d;
        const double hmax = std::max(b, h);
        const double bmin = std::min(b, h);
        const double wt = 2.0 * (bmin - s.t) * (hmax - s.t) * s.t - 4.5 * (4.0 - kPi) * std::pow(s.t, 3);
        const double ht = std::max(s.flatWidthB, s.flatWidthH) / s.t;
        const double lim1 = 2.45 * std::sqrt(E / fy);
        const double lim2 = 3.07 * std::sqrt(E / fy);
        if (ht <= lim1) {
            t.TRd = 0.60 * wt * fy / o.gammaA1;
        } else if (ht <= lim2) {
            t.TRd = 0.60 * wt * fy * lim1 / ht / o.gammaA1;
        } else {
            t.TRd = 0.46 * sq(kPi) * wt * E / sq(ht) / o.gammaA1;
        }
        t.covered = true;
        t.lines.push_back(fmt("Torcao em tubular retangular (5.5.2.1.3): h/t = %.1f, WT = %.0f mm3 -> T_Rd = %s", ht, wt, knm(t.TRd).c_str()));
    }
    return t;
}

} // namespace Structura::Design::Nbr8800
