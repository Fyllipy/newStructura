#include "design/CompositeColumn.h"

#include "design/Nbr8800.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>

namespace Structura::Design {

using Nbr8800::fmt;

namespace {

constexpr double kKn = 1e3;
constexpr double kKnm = 1e6;

double sq(double v) { return v * v; }

/// Fator de reducao chi da curva de resistencia a compressao (5.3.3).
double chiOf(double lambda0)
{
    return lambda0 <= 1.5 ? std::pow(0.658, lambda0 * lambda0) : 0.877 / (lambda0 * lambda0);
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Secao
// ---------------------------------------------------------------------------------------------------------------------------------------------

double CompositeColumnSection::alphaC() const
{
    const double total = Ac + Aa + As;
    switch (type) {
    case Type::FilledRectangular:
    case Type::FilledCircular:
        return std::min(0.45 + 3.0 * (Aa + As) / total, 0.9);
    case Type::PartiallyEncased:
        return 0.60;
    case Type::FullyEncased:
        break;
    }
    return std::min(0.25 + 3.0 * (Aa + As) / total, 0.7);
}

double CompositeColumnSection::effectiveStiffness(bool axisX, double steelE) const
{
    return steelE * (axisX ? IaX : IaY) + alphaC() * Ec * (axisX ? IcX : IcY) + kEs * (axisX ? IsX : IsY);
}

double CompositeColumnSection::effectiveAxialStiffness(double steelE) const
{
    return steelE * Aa + Ec * Ac + kEs * As;
}

void CompositeColumnSection::ringOfBars(int perHorizontalFace, int perVerticalFace, double barArea, double cover)
{
    bars.clear();
    if (perHorizontalFace < 2 || barArea <= 0.0) {
        return;
    }
    const double xm = bc / 2.0 - cover, ym = hc / 2.0 - cover;
    for (int i = 0; i < perHorizontalFace; ++i) {
        const double x = -xm + 2.0 * xm * i / (perHorizontalFace - 1);
        bars.push_back({barArea, x, ym});
        bars.push_back({barArea, x, -ym});
    }
    for (int k = 1; k <= perVerticalFace; ++k) {
        const double y = -ym + 2.0 * ym * k / (perVerticalFace + 1);
        bars.push_back({barArea, xm, y});
        bars.push_back({barArea, -xm, y});
    }
}

void CompositeColumnSection::finalize()
{
    // modulo secante do concreto (NBR 6118): Eci = alphaE 5600 sqrt(fck) (fck <= 50 MPa) ou 21,5e3 alphaE (fck/10 + 1,25)^(1/3); Ecs = alphai Eci
    const double eci = fck <= 50.0 ? alphaE * 5600.0 * std::sqrt(fck) : 21500.0 * alphaE * std::cbrt(fck / 10.0 + 1.25);
    Ec = std::min(1.0, 0.8 + 0.2 * fck / 80.0) * eci;
    As = IsX = IsY = ZsX = ZsY = 0.0;
    for (const Bar &b : bars) {
        As += b.area;
        IsX += b.area * b.y * b.y;
        IsY += b.area * b.x * b.x;
        ZsX += b.area * std::abs(b.y);
        ZsY += b.area * std::abs(b.x);
    }
    switch (type) {
    case Type::FullyEncased:
    case Type::PartiallyEncased: {
        if (type == Type::PartiallyEncased) {
            hc = d;
            bc = bf;
        }
        const double web = d - 2.0 * tf;
        Aa = 2.0 * bf * tf + web * tw;
        IaX = bf * d * d * d / 12.0 - (bf - tw) * web * web * web / 12.0;
        IaY = 2.0 * tf * bf * bf * bf / 12.0 + web * tw * tw * tw / 12.0;
        ZaX = bf * tf * (d - tf) + tw * web * web / 4.0;
        ZaY = tf * bf * bf / 2.0 + web * tw * tw / 4.0;
        Ac = hc * bc - Aa - As;
        IcX = bc * hc * hc * hc / 12.0 - IaX - IsX;
        IcY = hc * bc * bc * bc / 12.0 - IaY - IsY;
        ZcX = bc * hc * hc / 4.0 - ZaX - ZsX;
        ZcY = hc * bc * bc / 4.0 - ZaY - ZsY;
        break;
    }
    case Type::FilledRectangular: {
        const double c1 = b1 - 2.0 * t, c2 = b2 - 2.0 * t, r = innerRadius;
        Aa = b1 * b2 - c1 * c2;
        IaX = (b2 * b1 * b1 * b1 - c2 * c1 * c1 * c1) / 12.0;
        IaY = (b1 * b2 * b2 * b2 - c1 * c2 * c2 * c2) / 12.0;
        ZaX = (b2 * b1 * b1 - c2 * c1 * c1) / 4.0;
        ZaY = (b1 * b2 * b2 - c1 * c2 * c2) / 4.0;
        Ac = c1 * c2 - (4.0 - kPi) * r * r - As;
        IcX = c2 * c1 * c1 * c1 / 12.0 - IsX;
        IcY = c1 * c2 * c2 * c2 / 12.0 - IsY;
        ZcX = c2 * c1 * c1 / 4.0 - 2.0 / 3.0 * r * r * r - r * r * (4.0 - kPi) * (b1 / 2.0 - t - r) - ZsX;
        ZcY = c1 * c2 * c2 / 4.0 - 2.0 / 3.0 * r * r * r - r * r * (4.0 - kPi) * (b2 / 2.0 - t - r) - ZsY;
        break;
    }
    case Type::FilledCircular: {
        const double D = b1, dc = b1 - 2.0 * t;
        Aa = kPi * (D * D - dc * dc) / 4.0;
        IaX = IaY = kPi * (std::pow(D, 4) - std::pow(dc, 4)) / 64.0;
        ZaX = ZaY = (D * D * D - dc * dc * dc) / 6.0;
        Ac = kPi * dc * dc / 4.0 - As;
        IcX = kPi * std::pow(dc, 4) / 64.0 - IsX;
        IcY = kPi * std::pow(dc, 4) / 64.0 - IsY;
        ZcX = dc * dc * dc / 6.0 - ZsX;
        ZcY = dc * dc * dc / 6.0 - ZsY;
        break;
    }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Momentos resistentes de plastificacao (M.5.5)
// ---------------------------------------------------------------------------------------------------------------------------------------------

namespace {

struct PlasticResult {
    double hn {0.0};
    double mpl {0.0};
    double mmax {0.0};
    std::string note;
};

PlasticResult plasticMoment(const CompositeColumnSection &s, bool axisX, double fyd, double fcd1, double fsd)
{
    PlasticResult out;
    const double za = axisX ? s.ZaX : s.ZaY, zc = axisX ? s.ZcX : s.ZcY, zs = axisX ? s.ZsX : s.ZsY;
    out.mmax = fyd * za + 0.5 * fcd1 * zc + fsd * zs;   // M.5.5.2
    double hn = 0.0, zan = 0.0, zsn = 0.0, zcn = 0.0;
    // barras na regiao de altura 2 hn em torno do eixo de flexao: Asn e Zsn
    auto rebar = [&](double hnTrial, double &asn, double &zsnTrial) {
        asn = zsnTrial = 0.0;
        for (const auto &b : s.bars) {
            const double e = axisX ? std::abs(b.y) : std::abs(b.x);   // distancia ao eixo de flexao
            if (e <= hnTrial) {
                asn += b.area;
                zsnTrial += b.area * e;
            }
        }
    };
    if (s.filled()) {
        const double h = axisX ? s.b1 : s.b2, w = axisX ? s.b2 : s.b1, t = s.t;
        const double ac = s.Ac;
        double asn = 0.0;
        for (int it = 0; it < 30; ++it) {
            const double denominator = 2.0 * w * fcd1 + 4.0 * t * (2.0 * fyd - fcd1);
            hn = (ac * fcd1 - asn * (2.0 * fsd - fcd1)) / denominator;   // M.5.5.4-a-1 (circular: b2 -> D)
            double newAsn, newZsn;
            rebar(std::abs(hn), newAsn, newZsn);
            if (std::abs(newAsn - asn) < 1e-9) {
                zsn = newZsn;
                break;
            }
            asn = newAsn;
            zsn = newZsn;
        }
        hn = std::min(std::abs(hn), h / 2.0 - t);
        rebar(hn, asn, zsn);
        zcn = (w - 2.0 * t) * hn * hn - zsn;
        zan = w * hn * hn - zcn - zsn;
        out.hn = hn;
        out.mpl = fyd * (za - zan) + 0.5 * fcd1 * (zc - zcn) + fsd * (zs - zsn);
        return out;
    }
    // I/H revestido: tres casos conforme a posicao da linha neutra plastica
    const double d = s.d, bf = s.bf, tw = s.tw, tf = s.tf, hc = s.hc, bc = s.bc, aa = s.Aa, ac = s.Ac;
    for (int pass = 1; pass <= 3; ++pass) {
        if (pass == 3 && s.type != CompositeColumnSection::Type::FullyEncased) {
            break;
        }
        double asn = 0.0, candidate = 0.0;
        bool valid = false;
        for (int it = 0; it < 40; ++it) {
            double num, den, upper, lower;
            if (axisX) {
                if (pass == 1) {
                    num = ac * fcd1 - asn * (2.0 * fsd - fcd1);
                    den = 2.0 * bc * fcd1 + 2.0 * tw * (2.0 * fyd - fcd1);
                    lower = 0.0;
                    upper = d / 2.0 - tf;
                } else if (pass == 2) {
                    num = ac * fcd1 - asn * (2.0 * fsd - fcd1) + (bf - tw) * (d - 2.0 * tf) * (2.0 * fyd - fcd1);
                    den = 2.0 * bc * fcd1 + 2.0 * bf * (2.0 * fyd - fcd1);
                    lower = d / 2.0 - tf;
                    upper = d / 2.0;
                } else {
                    num = ac * fcd1 - asn * (2.0 * fsd - fcd1) - aa * (2.0 * fyd - fcd1);
                    den = 2.0 * bc * fcd1;
                    lower = d / 2.0;
                    upper = hc / 2.0;
                }
            } else {
                if (pass == 1) {
                    num = ac * fcd1 - asn * (2.0 * fsd - fcd1);
                    den = 2.0 * hc * fcd1 + 2.0 * d * (2.0 * fyd - fcd1);
                    lower = 0.0;
                    upper = tw / 2.0;
                } else if (pass == 2) {
                    num = ac * fcd1 - asn * (2.0 * fsd - fcd1) + tw * (2.0 * tf - d) * (2.0 * fyd - fcd1);
                    den = 2.0 * hc * fcd1 + 4.0 * tf * (2.0 * fyd - fcd1);
                    lower = tw / 2.0;
                    upper = bf / 2.0;
                } else {
                    num = ac * fcd1 - asn * (2.0 * fsd - fcd1) - aa * (2.0 * fyd - fcd1);
                    den = 2.0 * hc * fcd1;
                    lower = bf / 2.0;
                    upper = bc / 2.0;
                }
            }
            candidate = num / den;
            double newAsn, newZsn;
            rebar(candidate, newAsn, newZsn);
            valid = candidate >= lower - 1e-9 && candidate <= upper + 1e-9;
            if (std::abs(newAsn - asn) < 1e-9) {
                zsn = newZsn;
                break;
            }
            asn = newAsn;
            zsn = newZsn;
        }
        if (!valid) {
            continue;
        }
        hn = candidate;
        if (axisX) {
            zan = pass == 1 ? tw * hn * hn : (pass == 2 ? bf * hn * hn - (bf - tw) * (d - 2.0 * tf) * (d - 2.0 * tf) / 4.0 : za);
            zcn = bc * hn * hn - zan - zsn;
        } else {
            zan = pass == 1 ? d * hn * hn : (pass == 2 ? 2.0 * tf * hn * hn + (d - 2.0 * tf) * tw * tw / 4.0 : za);
            zcn = hc * hn * hn - zan - zsn;
        }
        out.hn = hn;
        out.mpl = fyd * (za - zan) + 0.5 * fcd1 * (zc - zcn) + fsd * (zs - zsn);
        return out;
    }
    // sem solucao valida (secao muito esbelta em armadura): usa o maior momento de plastificacao como limite
    out.mpl = out.mmax;
    out.note = "linha neutra plastica fora das faixas de M.5.5.3: usado M_max,pl,Rd";
    return out;
}

/// Fibras de um tubo preenchido em flexao: profundidade y (mm) a partir da fibra mais comprimida, largura do aco e do concreto.
struct Fibers {
    double height {0.0};
    std::vector<double> y, steel, concrete;   // por faixa: centro da faixa (mm), largura do aco, largura do concreto
    double dy {0.0};
};

Fibers fibersOf(const CompositeColumnSection &s, bool axisX)
{
    Fibers f;
    const double h = s.circular() ? s.b1 : (axisX ? s.b1 : s.b2), w = s.circular() ? s.b1 : (axisX ? s.b2 : s.b1), t = s.t;
    f.height = h;
    constexpr int n = 600;
    f.dy = h / n;
    for (int i = 0; i < n; ++i) {
        const double y = (i + 0.5) * f.dy;   // a partir da fibra comprimida
        double steel, concrete;
        if (s.circular()) {
            const double ro = h / 2.0, ri = ro - t, u = ro - y;   // distancia ao centro
            const double outer = 2.0 * std::sqrt(std::max(ro * ro - u * u, 0.0));
            const double inner = std::abs(u) < ri ? 2.0 * std::sqrt(ri * ri - u * u) : 0.0;
            steel = outer - inner;
            concrete = inner;
        } else {
            const bool flange = y < t || y > h - t;
            steel = flange ? w : 2.0 * t;
            concrete = flange ? 0.0 : w - 2.0 * t;
        }
        f.y.push_back(y);
        f.steel.push_back(steel);
        f.concrete.push_back(concrete);
    }
    return f;
}

/// Momento (N.mm) com a distribuicao de tensoes de M.5.5.1 (figuras M.4 e M.5): `slender` = tensao critica sigma_cr,Rd na mesa comprimida (M_cr,Rd), senao inicio da plastificacao (M_e,Rd, N = 0).
double elasticLimitMoment(const CompositeColumnSection &s, bool axisX, bool slender, double sigmaCr, double fyd, double fcd2, double fsd)
{
    const Fibers f = fibersOf(s, axisX);
    const double h = f.height;
    auto barY = [&](const CompositeColumnSection::Bar &b) { return h / 2.0 - (axisX ? b.y : b.x); };   // profundidade a partir da fibra mais comprimida (barra acima do centro = comprimida)
    std::function<void(double, double &, double &)> integrate;
    if (!slender) {
        // aco: sigma = fyd clamp(1 - y/ay, -1, 1); concreto: compressao triangular fcd2 (1 - y/ay); armadura como o aco com fsd
        integrate = [&](double ay, double &force, double &moment) {
            force = moment = 0.0;
            for (std::size_t i = 0; i < f.y.size(); ++i) {
                const double law = std::clamp(1.0 - f.y[i] / ay, -1.0, 1.0);
                const double steelStress = fyd * law;
                const double concreteStress = f.y[i] < ay ? fcd2 * (1.0 - f.y[i] / ay) : 0.0;
                const double dForce = (steelStress * f.steel[i] + concreteStress * f.concrete[i]) * f.dy;
                force += dForce;
                moment += dForce * (h / 2.0 - f.y[i]);
            }
            for (const auto &b : s.bars) {
                const double y = barY(b), law = std::clamp(1.0 - y / ay, -1.0, 1.0);
                const double dForce = fsd * law * b.area;
                force += dForce;
                moment += dForce * (h / 2.0 - y);
            }
        };
        double lo = 1e-3 * h, hi = h / 2.0, fLo, mLo, fHi, mHi;
        integrate(lo, fLo, mLo);
        integrate(hi, fHi, mHi);
        if (fLo * fHi > 0.0) {
            return std::abs(fLo) < std::abs(fHi) ? mLo : mHi;
        }
        for (int it = 0; it < 80; ++it) {
            const double mid = 0.5 * (lo + hi);
            double fm, mm;
            integrate(mid, fm, mm);
            if (fLo * fm <= 0.0) {
                hi = mid;
            } else {
                lo = mid;
                fLo = fm;
            }
        }
        double fr, mr;
        integrate(0.5 * (lo + hi), fr, mr);
        return mr;
    }
    // M_cr,Rd: tensao do aco variando linearmente de sigma_cr (compressao, na mesa comprimida) a fyd (tracao, na mesa tracionada)
    const double acr = h * sigmaCr / (sigmaCr + fyd);
    double moment = 0.0;
    for (std::size_t i = 0; i < f.y.size(); ++i) {
        const double steelStress = sigmaCr - (sigmaCr + fyd) * f.y[i] / h;   // compressao positiva
        const double concreteStress = f.y[i] < acr ? fcd2 * (1.0 - f.y[i] / acr) : 0.0;
        moment += (steelStress * f.steel[i] + concreteStress * f.concrete[i]) * f.dy * (h / 2.0 - f.y[i]);
    }
    for (const auto &b : s.bars) {
        const double y = barY(b);
        const double stress = (sigmaCr - (sigmaCr + fyd) * y / h) * fsd / fyd;
        moment += stress * b.area * (h / 2.0 - y);
    }
    return moment;
}

} // namespace

double compositeInterfaceShearStrength(const CompositeColumnSection &s, double steelFy, double steelE, bool alternative)
{
    if (!s.filled()) {
        return 0.0;
    }
    if (s.circular()) {
        const double dt = s.b1 / s.t;
        if (alternative) {
            return dt <= 0.10 * steelE / steelFy ? 0.27 : 0.0;
        }
        return std::min(2650.0 * s.t / (s.b1 * s.b1), 0.7) * 1.0 > 0.0 ? std::min(2650.0 * s.t / (s.b1 * s.b1), 0.7) : 0.0;
    }
    const double bi = std::max(s.b1, s.b2), ti = s.t;
    if (alternative) {
        return bi / ti <= 1.78 * std::sqrt(steelE / steelFy) ? 0.20 : 0.0;
    }
    return std::min(1050.0 * ti / bi, 0.35);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Resistencias
// ---------------------------------------------------------------------------------------------------------------------------------------------

CompositeColumnResistance compositeColumnResistance(const CompositeColumnSection &s, double fy, double E, double gammaA1, double gammaC, double gammaS, double Lx, double Ly)
{
    CompositeColumnResistance r;
    if (s.Aa <= 0.0 || (s.Ac <= 0.0)) {
        return r;
    }
    r.valid = true;
    r.fyd = fy / gammaA1;
    r.fcd = s.fck / gammaC;
    r.fsd = s.fsk / gammaS;
    const double alpha = s.circular() ? 0.95 : 0.85;
    const double etaC = s.filled() ? 1.0 : std::min(std::cbrt(40.0 / s.fck), 1.0);
    r.fcd1 = alpha * etaC * r.fcd;
    r.fcd2 = 0.7 * r.fcd;
    const double esEc = CompositeColumnSection::kEs / s.Ec;
    // M.4
    r.Npl_a = s.Aa * r.fyd;
    r.Npl_c = s.Ac * r.fcd1;
    r.Npl_s = s.filled() ? s.As * r.fcd1 * esEc : s.As * r.fsd;
    r.Npl = r.Npl_a + r.Npl_c + r.Npl_s;
    r.delta = r.Npl_a / r.Npl;
    const double fcd1R = alpha * etaC * s.fck;
    r.Npl_R = s.Aa * fy + s.Ac * fcd1R + (s.filled() ? s.As * fcd1R * esEc : s.As * s.fsk);
    r.Ny = r.fyd * s.Aa + r.fcd2 * (s.Ac + s.As * esEc);
    r.lines.push_back(fmt("Materiais: fyd = fy/ga1 = %.0f/%.2f = %.1f MPa; fcd = fck/gc = %.0f/%.2f = %.2f MPa; fsd = fsk/gs = %.0f/%.2f = %.1f MPa; Ec = %.0f MPa (secante, NBR 6118)", fy, gammaA1, r.fyd, s.fck, gammaC, r.fcd,
                          s.fsk, gammaS, r.fsd, s.Ec));
    r.lines.push_back(fmt("fcd1 = alfa eta_c fcd = %.2f x %.3f x %.2f = %.2f MPa; fcd2 = 0,7 fcd = %.2f MPa", alpha, etaC, r.fcd, r.fcd1, r.fcd2));
    r.lines.push_back(fmt("M.4: Npl,a,Rd = Aa fyd = %.1f kN; Npl,c,Rd = Ac fcd1 = %.1f kN; Npl,s,Rd = %.1f kN -> Npl,Rd = %.1f kN; delta = Aa fyd / Npl,Rd = %.3f", r.Npl_a / kKn, r.Npl_c / kKn, r.Npl_s / kKn, r.Npl / kKn, r.delta));

    // classificacao da parede/mesa (M.1.4)
    const double rootE = std::sqrt(E / fy);
    int classAxial = 0;
    double lambdaA = 0.0, lamP = 0.0, lamR = 0.0, lamU = 0.0;
    if (s.type == CompositeColumnSection::Type::FilledRectangular) {
        lambdaA = std::max(s.b1, s.b2) / s.t;
        lamP = 2.26 * rootE;
        lamR = 3.00 * rootE;
        lamU = 5.00 * rootE;
    } else if (s.type == CompositeColumnSection::Type::FilledCircular) {
        lambdaA = s.b1 / s.t;
        lamP = 0.15 * E / fy;
        lamR = 0.19 * E / fy;
        lamU = 0.31 * E / fy;
    }
    if (s.filled()) {
        classAxial = lambdaA <= lamP ? 0 : (lambdaA <= lamR ? 1 : 2);
        r.lambdaAxial = lambdaA;
        r.lambdaAxialP = lamP;
        r.lambdaAxialR = lamR;
        r.lines.push_back(fmt("M.1.4.3 (Tabela M.1, forca axial): lambda = %.1f; lambda_p = %.1f; lambda_r = %.1f; limite superior = %.1f -> secao %s", lambdaA, lamP, lamR, lamU,
                              classAxial == 0 ? "compacta" : (classAxial == 1 ? "semicompacta" : "esbelta")));
    }
    r.classAxial = classAxial;
    r.Np = r.Npl;
    if (classAxial == 1) {
        r.Np = r.Npl - (r.Npl - r.Ny) * sq(lambdaA - lamP) / sq(lamR - lamP);
        r.lines.push_back(fmt("Secao semicompacta: Np,Rd = Npl,Rd - (Npl,Rd - Ny,Rd)(lambda - lambda_p)^2/(lambda_r - lambda_p)^2 = %.1f kN (Ny,Rd = %.1f kN)", r.Np / kKn, r.Ny / kKn));
    } else if (classAxial == 2) {
        const double sigmaCr = s.circular() ? 0.72 * r.fyd / std::pow(lambdaA * fy / E, 0.2) : 9.0 * E / (sq(lambdaA) * gammaA1);
        r.Np = sigmaCr * s.Aa + r.fcd2 * (s.Ac + s.As * esEc);
        r.lines.push_back(fmt("Secao esbelta: sigma_cr,Rd = %.1f MPa; Np,Rd = sigma_cr,Rd Aa + fcd2 (Ac + As Es/Ec) = %.1f kN", sigmaCr, r.Np / kKn));
    }

    // rigidezes, esbeltez reduzida e N_Rd (M.3)
    r.EIx = s.effectiveStiffness(true, E);
    r.EIy = s.effectiveStiffness(false, E);
    r.EA = s.effectiveAxialStiffness(E);
    r.Ne_x = Lx > 0.0 ? sq(kPi) * r.EIx / sq(Lx) : 1e30;
    r.Ne_y = Ly > 0.0 ? sq(kPi) * r.EIy / sq(Ly) : 1e30;
    r.lambda0_x = std::sqrt(r.Npl_R / r.Ne_x);
    r.lambda0_y = std::sqrt(r.Npl_R / r.Ne_y);
    r.chi_x = chiOf(r.lambda0_x);
    r.chi_y = chiOf(r.lambda0_y);
    r.NRd_x = r.chi_x * r.Np;
    r.NRd_y = r.chi_y * r.Np;
    r.lines.push_back(fmt("M.3.4: alfa_c = %.3f; (EI)e,x = Ea Ia + alfa_c Ec Ic + Es Is = %.4g N.mm2; (EI)e,y = %.4g N.mm2; (EA)e = %.4g N", s.alphaC(), r.EIx, r.EIy, r.EA));
    r.lines.push_back(fmt("M.3.2: Ne,x = pi2 (EI)e,x / Lx2 = %.1f kN; Ne,y = %.1f kN; Np,R = %.1f kN -> lambda0,m,x = sqrt(Np,R/Ne) = %.3f; lambda0,m,y = %.3f", r.Ne_x / kKn, r.Ne_y / kKn, r.Npl_R / kKn, r.lambda0_x, r.lambda0_y));
    r.lines.push_back(fmt("M.3.1: chi,x = %.3f; chi,y = %.3f -> NRd,x = chi Np,Rd = %.1f kN; NRd,y = %.1f kN", r.chi_x, r.chi_y, r.NRd_x / kKn, r.NRd_y / kKn));

    // momentos resistentes (M.5.5)
    for (const bool axisX : {true, false}) {
        const PlasticResult p = plasticMoment(s, axisX, r.fyd, r.fcd1, r.fsd);
        const double za = axisX ? s.ZaX : s.ZaY, zc = axisX ? s.ZcX : s.ZcY, zs = axisX ? s.ZsX : s.ZsY;
        double mrd = p.mpl, me = 0.0;
        int cls = 0;
        double lam = 0.0, lamPf = 0.0, lamRf = 0.0;
        if (s.type == CompositeColumnSection::Type::FilledRectangular) {
            // mesa: dimensao paralela ao eixo de flexao; alma: perpendicular (Tabela M.1)
            const double parallel = axisX ? s.b2 : s.b1, perpendicular = axisX ? s.b1 : s.b2;
            const double lamFlange = parallel / s.t, lamWeb = perpendicular / s.t;
            const int clsFlange = lamFlange <= 2.26 * rootE ? 0 : (lamFlange <= 3.00 * rootE ? 1 : 2);
            const int clsWeb = lamWeb <= 3.00 * rootE ? 0 : (lamWeb <= 5.70 * rootE ? 1 : 2);
            cls = std::max(clsFlange, clsWeb);
            if (clsFlange >= clsWeb) {
                lam = lamFlange;
                lamPf = 2.26 * rootE;
                lamRf = 3.00 * rootE;
            } else {
                lam = lamWeb;
                lamPf = 3.00 * rootE;
                lamRf = 5.70 * rootE;
            }
        } else if (s.type == CompositeColumnSection::Type::FilledCircular) {
            lam = s.b1 / s.t;
            lamPf = 0.09 * E / fy;
            lamRf = 0.31 * E / fy;
            cls = lam <= lamPf ? 0 : (lam <= lamRf ? 1 : 2);
        }
        if (cls >= 1) {
            if (cls == 1) {
                me = elasticLimitMoment(s, axisX, false, 0.0, r.fyd, r.fcd2, r.fsd);
                mrd = p.mpl - (p.mpl - me) * (lam - lamPf) / (lamRf - lamPf);
            } else {
                const double sigmaCr = s.circular() ? 0.72 * r.fyd / std::pow(lam * fy / E, 0.2) : 9.0 * E / (sq(lam) * gammaA1);
                me = elasticLimitMoment(s, axisX, true, sigmaCr, r.fyd, r.fcd2, r.fsd);
                mrd = me;
            }
        }
        (axisX ? r.Mpl_x : r.Mpl_y) = p.mpl;
        (axisX ? r.MmaxPl_x : r.MmaxPl_y) = p.mmax;
        (axisX ? r.Me_x : r.Me_y) = me;
        (axisX ? r.MRd_x : r.MRd_y) = mrd;
        (axisX ? r.classFlexX : r.classFlexY) = cls;
        (axisX ? r.hnX : r.hnY) = p.hn;
        r.lines.push_back(fmt("M.5.5.%s (eixo %s): Za = %.0f, Zc = %.0f, Zs = %.0f mm3; hn = %.1f mm; M_pl,Rd = fyd (Za - Zan) + 0,5 fcd1 (Zc - Zcn) + fsd (Zs - Zsn) = %.2f kN*m; M_max,pl,Rd = fyd Za + 0,5 fcd1 Zc + fsd Zs = %.2f kN*m%s",
                              s.filled() ? "4" : "3", axisX ? "x" : "y", za, zc, zs, p.hn, p.mpl / kKnm, p.mmax / kKnm, p.note.empty() ? "" : (" (" + p.note + ")").c_str()));
        if (cls == 1) {
            r.lines.push_back(fmt("Secao semicompacta em flexao (eixo %s): M_e,Rd = %.2f kN*m; M_Rd = M_pl,Rd - (M_pl,Rd - M_e,Rd)(lambda - lambda_p)/(lambda_r - lambda_p) = %.2f kN*m", axisX ? "x" : "y", me / kKnm, mrd / kKnm));
        } else if (cls == 2) {
            r.lines.push_back(fmt("Secao esbelta em flexao (eixo %s): M_cr,Rd = %.2f kN*m (mesa comprimida na tensao critica)", axisX ? "x" : "y", mrd / kKnm));
        }
    }
    return r;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Verificacao da barra
// ---------------------------------------------------------------------------------------------------------------------------------------------

namespace {

struct Best {
    CheckResult check;
    std::string combo;
    double x {0.0};
};

class Keeper
{
public:
    void offer(CheckResult c, const std::string &combo, double x)
    {
        const std::string id = c.id;
        const auto it = m_best.find(id);
        if (it == m_best.end()) {
            m_order.push_back(id);
            m_best.emplace(id, Best {std::move(c), combo, x});
        } else if (c.ratio > it->second.check.ratio) {
            it->second = Best {std::move(c), combo, x};
        }
    }
    [[nodiscard]] const std::vector<std::string> &order() const { return m_order; }
    [[nodiscard]] const Best &best(const std::string &id) const { return m_best.at(id); }
    [[nodiscard]] bool has(const std::string &id) const { return m_best.count(id) > 0; }

private:
    std::map<std::string, Best> m_best;
    std::vector<std::string> m_order;
};

CheckResult makeCheck(const std::string &id, const std::string &title, const std::string &clause, double demand, double resistance, const char *unit, double scale, std::vector<std::string> lines)
{
    CheckResult c;
    c.id = id;
    c.title = title;
    c.clause = clause;
    c.unit = unit;
    c.demand = demand / scale;
    c.resistance = resistance / scale;
    c.ratio = resistance > 0.0 ? demand / resistance : (demand > 0.0 ? 1e9 : 0.0);
    c.lines = std::move(lines);
    return c;
}

} // namespace

BarDesignOutput designCompositeColumn(const BarDesignInput &in, int model)
{
    BarDesignOutput out;
    if (in.compositeColumn == nullptr) {
        out.warnings.push_back("Pilar misto sem dados da secao mista.");
        return out;
    }
    const CompositeColumnSection &s = *in.compositeColumn;
    out.supported = true;
    out.standard = "ABNT NBR 8800:2024";
    const std::string tag = "NBR 8800:2024";
    const double L = in.length;
    const double Lx = in.lxOverride > 0.0 ? in.lxOverride : in.Kx * L;
    const double Ly = in.lyOverride > 0.0 ? in.lyOverride : in.Ky * L;
    const double gammaA1 = in.opt8800.gammaA1, gammaC = 1.4, gammaS = 1.15;
    const double fy = in.steel.fy, E = in.steel.E;
    const CompositeColumnResistance r = compositeColumnResistance(s, fy, E, gammaA1, gammaC, gammaS, Lx, Ly);
    if (!r.valid) {
        out.warnings.push_back("Pilar misto: dimensoes da secao invalidas (areas nao positivas).");
        return out;
    }
    const char *typeName = s.type == CompositeColumnSection::Type::FullyEncased ? "totalmente revestida" : (s.type == CompositeColumnSection::Type::PartiallyEncased ? "parcialmente revestida"
                                                                                                          : (s.circular() ? "tubular circular preenchida" : "tubular retangular preenchida"));
    out.summaryLines.push_back(fmt("Pilar misto de aco e concreto, secao %s (NBR 8800:2024, Anexo M, metodo simplificado): Aa = %.0f mm2, Ac = %.0f mm2, As = %.0f mm2; fck = %.0f MPa, fy = %.0f MPa, fsk = %.0f MPa", typeName, s.Aa, s.Ac, s.As,
                                   s.fck, fy, s.fsk));
    out.summaryLines.push_back(fmt("Comprimento L = %.0f mm; Lx = %.0f mm, Ly = %.0f mm; modelo de calculo %d (M.5.%d)", L, Lx, Ly, model, model + 1));
    Keeper keeper;

    // ---- M.1.3 e M.1.4: limites de aplicabilidade --------------------------------------------------------------------------------------
    {
        std::vector<std::string> lines;
        int violations = 0;
        auto rule = [&](bool ok, const std::string &text) {
            lines.push_back(std::string(ok ? "OK: " : "NAO ATENDE: ") + text);
            violations += ok ? 0 : 1;
        };
        const double total = s.Aa + s.Ac + s.As;
        rule(s.Aa / total >= 0.01, fmt("M.1.3-b: Aa/A total = %.3f >= 0,01", s.Aa / total));
        rule(r.delta >= 0.1 && r.delta <= 0.9, fmt("M.1.3-d: delta = Aa fyd / Npl,Rd = %.3f entre 0,1 e 0,9%s", r.delta, r.delta < 0.1 ? " (dimensionar como pilar de concreto, NBR 6118)" : (r.delta > 0.9 ? " (dimensionar como pilar de aco)" : "")));
        rule(std::max(r.lambda0_x, r.lambda0_y) <= 2.0, fmt("M.1.3-e: lambda0,m = %.3f <= 2,0", std::max(r.lambda0_x, r.lambda0_y)));
        if (!s.filled()) {
            rule(s.As >= 0.004 * s.Ac, fmt("M.1.3-f: As = %.0f mm2 >= 0,4 %% de Ac = %.0f mm2 (armadura longitudinal)", s.As, 0.004 * s.Ac));
        }
        const double aspect = s.filled() ? (s.circular() ? 1.0 : s.b1 / s.b2) : s.hc / s.bc;
        rule(aspect >= 0.2 && aspect <= 5.0, fmt("M.1.3-g: relacao altura/largura = %.2f entre 0,2 e 5,0", aspect));
        if (s.type == CompositeColumnSection::Type::FullyEncased) {
            const double cy = (s.bc - s.bf) / 2.0, cx = (s.hc - s.d) / 2.0;
            rule(cy >= std::max(s.bf / 6.0, 40.0), fmt("M.1.3-h: cobrimento cy = %.1f mm >= max(bf/6 ; 40) = %.1f mm", cy, std::max(s.bf / 6.0, 40.0)));
            rule(cx >= std::max(s.bf / 6.0, 40.0), fmt("M.1.3-h: cobrimento cx = %.1f mm >= max(bf/6 ; 40) = %.1f mm", cx, std::max(s.bf / 6.0, 40.0)));
        }
        if (s.type == CompositeColumnSection::Type::PartiallyEncased) {
            const double lam = s.bf / s.tf, lim = 1.49 * std::sqrt(E / fy);
            rule(lam <= lim, fmt("M.1.4.2: secao parcialmente revestida compacta: bf/tf = %.1f <= 1,49 sqrt(E/fy) = %.1f", lam, lim));
        }
        if (s.filled()) {
            const double lamU = s.circular() ? 0.31 * E / fy : 5.00 * std::sqrt(E / fy);
            rule(r.lambdaAxial <= lamU, fmt("M.1.4.3: lambda = %.1f <= limite superior da Tabela M.1 = %.1f", r.lambdaAxial, lamU));
        }
        lines.push_back("M.1.3-c: concreto de densidade normal; M.1.3-i: se o pilar for concretado ja montado, o perfil de aco deve resistir isoladamente as acoes aplicadas antes de o concreto atingir 75 % de fck; "
                        "M.1.3-j: armaduras longitudinal e transversal e conectores ao longo do comprimento (espacamento <= 600 mm e <= 32 diametros); M.1.3-k: armaduras conforme a NBR 6118 (nao verificadas aqui).");
        CheckResult c = makeCheck("cc_limits", "Pilar misto: limites de aplicabilidade", tag + ", M.1.3 e M.1.4", violations > 0 ? 1.0 + 1e-6 : 0.0, 1.0, "", 1.0, lines);
        c.ratio = violations > 0 ? 1.0 + 1e-6 : 0.0;
        keeper.offer(std::move(c), "-", 0.0);
    }

    // ---- esforcos por secao e combinacao -----------------------------------------------------------------------------------------------
    const double neX = r.Ne_x, neY = r.Ne_y;
    const double ne2X = neX, ne2Y = neY;   // M.5.3.2: N_e2 = pi2 (EI)e / L2 com (EI)e de M.3.4
    // vetor de rigidez reduzida na analise (4.10.7.1.2 e M.3.4): 0,64 (EI)e, usada na amplificacao B1 dos momentos de primeira ordem
    const double neStarX = 0.64 * neX, neStarY = 0.64 * neY;
    for (const ComboEffects &combo : in.combos) {
        if (combo.stations.empty()) {
            continue;
        }
        const double m1x = combo.stations.front().e.Mx, m2x = combo.stations.back().e.Mx;
        const double m1y = combo.stations.front().e.My, m2y = combo.stations.back().e.My;
        auto cmOf = [&](double a, double b, bool transverse) {
            if (transverse) {
                return 1.0;
            }
            const double small = std::abs(a) <= std::abs(b) ? a : b, big = std::abs(a) <= std::abs(b) ? b : a;
            if (std::abs(big) < 1e-9) {
                return 1.0;
            }
            const double ratio = (a * b < 0.0 ? -1.0 : 1.0) * std::abs(small) / std::abs(big);   // positiva em curvatura simples (convencao de Cm)
            return std::max(0.4, 0.6 - 0.4 * (ratio >= 0.0 ? -ratio : std::abs(ratio)));
        };
        const double cmX = cmOf(m1x, m2x, combo.transverseLoadX), cmY = cmOf(m1y, m2y, combo.transverseLoadY);
        for (const StationEffects &st : combo.stations) {
            const Effects &e = st.e;
            const double nSd = std::max(0.0, -e.N);   // compressao
            const double tSd = std::max(0.0, e.N);    // tracao
            auto where = [&]() { return fmt("combinacao %s, x = %.0f mm", combo.name.c_str(), st.x); };
            // momentos de primeira ordem amplificados (modelo I: 4.10.7 / Anexo C)
            const double b1x = nSd > 0.0 ? (nSd >= neStarX ? 1e6 : std::max(1.0, cmX / (1.0 - nSd / neStarX))) : 1.0;
            const double b1y = nSd > 0.0 ? (nSd >= neStarY ? 1e6 : std::max(1.0, cmY / (1.0 - nSd / neStarY))) : 1.0;
            const double b2 = in.b2 * combo.b2;
            const double mx1 = std::abs(e.Mx) * b1x * b2, my1 = std::abs(e.My) * b1y * b2;
            const double nRd = std::min(r.NRd_x, r.NRd_y);

            // tracao: so o perfil de aco (a armadura e o concreto nao sao considerados)
            if (tSd > 0.0) {
                const double rd = s.Aa * r.fyd;
                keeper.offer(makeCheck("tension", "Tracao axial (so o perfil de aco)", tag + ", 5.2.2 (Anexo M nao trata tracao)", tSd, rd, "kN", kKn,
                                       {fmt("Nt,Rd = Aa fyd = %.0f x %.1f = %.1f kN (concreto e armadura desprezados); Nt,Sd = %.1f kN  (%s)", s.Aa, r.fyd, rd / kKn, tSd / kKn, where().c_str())}),
                             combo.name, st.x);
            }
            // compressao (M.3)
            if (nSd > 0.0) {
                keeper.offer(makeCheck("compression", "Compressao axial do pilar misto", tag + ", M.3", nSd, nRd, "kN", kKn, [&] {
                    std::vector<std::string> lines = r.lines;
                    lines.push_back(fmt("N_Rd = min(chi_x Np,Rd ; chi_y Np,Rd) = min(%.1f ; %.1f) = %.1f kN; Nc,Sd = %.1f kN -> %.3f  (%s)", r.NRd_x / kKn, r.NRd_y / kKn, nRd / kKn, nSd / kKn, nSd / nRd, where().c_str()));
                    return lines;
                }()),
                             combo.name, st.x);
            }
            // efeitos de retracao e fluencia (M.3.3) quando a esbeltez reduzida supera o limite
            double addX = 0.0, addY = 0.0;
            if (nSd > 0.0) {
                const double lambdaLim = 90.0 / kPi * std::sqrt(r.Npl_R / r.EA);
                for (const bool axisX : {true, false}) {
                    const double lambda0 = axisX ? r.lambda0_x : r.lambda0_y;
                    if (lambda0 <= lambdaLim) {
                        continue;
                    }
                    const double ne = axisX ? neX : neY, length = axisX ? Lx : Ly, mSd = axisX ? std::abs(e.Mx) : std::abs(e.My);
                    const double denominator = ne / nSd - 0.45;
                    const double m = (s.filled() ? 0.27 : 1.08) / std::max(denominator, 1e-6);
                    const double ecc = (mSd / nSd + length / 300.0) * (std::exp(m) - 1.0);
                    const double mcc = denominator > 0.0 ? nSd * ecc : 1e12;
                    (axisX ? addX : addY) = mcc;
                    keeper.offer(makeCheck("cc_creep", "Retracao e fluencia do concreto (momento adicional)", tag + ", M.3.3", mcc, std::max(r.MRd_x, r.MRd_y) * 0.25, "kN*m", kKnm,
                                           {fmt("lambda0,m = %.3f > lambda0,lim = (90/pi) sqrt(Np,R/(EA)e) = %.3f: efeitos de retracao e fluencia (M.3.3)", lambda0, lambdaLim),
                                            fmt("m = %s / (Ne/NSd - 0,45) = %.3f; e_cc = (MSd/NSd + L/300)(e^m - 1) = %.1f mm; Mcc = NSd e_cc = %.2f kN*m somado ao momento do plano %s (phi = %s, NSg/NSd = 0,45)", s.filled() ? "0,27" : "1,08", m, ecc, mcc / kKnm,
                                                axisX ? "x" : "y", s.filled() ? "0,6" : "2,4")}),
                                 combo.name, st.x);
                }
            }
            const double mx1c = mx1 + addX, my1c = my1 + addY;

            // flexao: M_Rd por eixo (M.5.5)
            if (mx1c > 0.0) {
                keeper.offer(makeCheck("flexure_x", "Flexao em torno do eixo x do pilar misto", tag + ", M.5.5.1", mx1c, r.MRd_x, "kN*m", kKnm,
                                       {fmt("M_Rd,x = %.2f kN*m (%s); M_x,Sd = %.2f kN*m (amplificado, B1 = %.3f)  (%s)", r.MRd_x / kKnm, r.classFlexX == 0 ? "secao compacta: M_pl,Rd" : (r.classFlexX == 1 ? "semicompacta" : "esbelta: M_cr,Rd"),
                                             mx1c / kKnm, b1x, where().c_str())}),
                             combo.name, st.x);
            }
            if (my1c > 0.0) {
                keeper.offer(makeCheck("flexure_y", "Flexao em torno do eixo y do pilar misto", tag + ", M.5.5.1", my1c, r.MRd_y, "kN*m", kKnm,
                                       {fmt("M_Rd,y = %.2f kN*m (%s); M_y,Sd = %.2f kN*m (amplificado, B1 = %.3f)  (%s)", r.MRd_y / kKnm, r.classFlexY == 0 ? "secao compacta: M_pl,Rd" : (r.classFlexY == 1 ? "semicompacta" : "esbelta: M_cr,Rd"),
                                             my1c / kKnm, b1y, where().c_str())}),
                             combo.name, st.x);
            }

            // interacao
            if (nSd > 0.0 || mx1c > 0.0 || my1c > 0.0) {
                double ratio = 0.0;
                std::vector<std::string> lines;
                if (model == 1) {
                    const double n = nSd / nRd;
                    const double sum = (r.MRd_x > 0.0 ? mx1c / r.MRd_x : 0.0) + (r.MRd_y > 0.0 ? my1c / r.MRd_y : 0.0);
                    ratio = n >= 0.2 ? n + 8.0 / 9.0 * sum : n / 2.0 + sum;
                    lines.push_back(fmt("Modelo de calculo I (M.5.2, 5.5.1.2-%s): NSd/NRd = %.3f; Mx,Sd/Mx,Rd + My,Sd/My,Rd = %.3f -> %s = %.3f  (%s)", n >= 0.2 ? "a" : "b", n, sum,
                                        n >= 0.2 ? "N/NRd + 8/9 (...)" : "N/(2 NRd) + (...)", ratio, where().c_str()));
                } else if (model == 2) {
                    // M.5.3: so secoes compactas
                    const double reduction = fy <= 350.0 ? 0.9 : 0.8;
                    const double mcx = reduction * r.Mpl_x, mcy = reduction * r.Mpl_y;
                    const double mdx = std::max(0.8 * r.MmaxPl_x, mcx), mdy = std::max(0.8 * r.MmaxPl_y, mcy);
                    const double npc = r.Npl_c;
                    auto mu = [&](double md, double mc) {
                        if (nSd >= npc) {
                            return 1.0 - (nSd - npc) / (r.Npl - npc);
                        }
                        if (nSd >= 0.5 * npc) {
                            return (1.0 - md / mc) * (2.0 * nSd / npc - 1.0) + md / mc;
                        }
                        return 1.0 + 2.0 * nSd / npc * (md / mc - 1.0);
                    };
                    const double mux = mu(mdx, mcx), muy = mu(mdy, mcy);
                    // M.5.3.2: momentos totais; so a imperfeicao de um eixo por vez (a mais desfavoravel)
                    auto total = [&](bool imperfectionInX) {
                        const double mix = imperfectionInX && nSd > 0.0 ? nSd * Lx / (300.0 * (1.0 - nSd / ne2X)) : 0.0;
                        const double miy = !imperfectionInX && nSd > 0.0 ? nSd * Ly / (300.0 * (1.0 - nSd / ne2Y)) : 0.0;
                        const double sdx = std::abs(e.Mx) + mix + addX, sdy = std::abs(e.My) + miy + addY;
                        return (sdx / (mux * mcx)) + (sdy / (muy * mcy));
                    };
                    ratio = std::max(total(true), total(false));
                    if (nSd > 0.0 && (nSd >= ne2X || nSd >= ne2Y)) {
                        ratio = 1e6;
                    }
                    lines.push_back(fmt("Modelo de calculo II (M.5.3): Mc,x = %.2f, Mc,y = %.2f kN*m (%.0f %% de Mpl,Rd); Md,x = %.2f, Md,y = %.2f kN*m (0,8 Mmax,pl,Rd); mu_x = %.3f, mu_y = %.3f", mcx / kKnm, mcy / kKnm,
                                        reduction * 100.0, mdx / kKnm, mdy / kKnm, mux, muy));
                    lines.push_back(fmt("Mx,tot,Sd/(mu_x Mc,x) + My,tot,Sd/(mu_y Mc,y) = %.3f, com os momentos totais de M.5.3.2 (imperfeicao ao longo do pilar M = NSd L/(300 (1 - NSd/Ne2)), um eixo por vez)  (%s)", ratio, where().c_str()));
                    if (r.classAxial != 0 || r.classFlexX != 0 || r.classFlexY != 0) {
                        ratio = std::max(ratio, 1.0 + 1e-6);
                        lines.push_back("O modelo de calculo II so e valido para secoes compactas (M.5.1.3): use o modelo I.");
                    }
                } else {
                    // M.5.4: tubulares (c_p, c_m, c_sr)
                    double cp = 0.0, cm = 1.0;
                    const double csr = (s.Aa * fy + s.As * s.fsk) / (s.Ac * s.fck);
                    if (s.circular()) {
                        cp = 0.27 / std::pow(csr, 0.4);
                        cm = csr >= 0.5 ? std::max(1.10 / std::pow(csr, 0.08), 1.0) : std::min(0.95 / std::pow(csr, 0.32), 1.67);
                    } else {
                        cp = 0.17 / std::pow(csr, 0.4);
                        cm = csr >= 0.5 ? std::max(1.06 / std::pow(csr, 0.11), 1.0) : std::min(0.90 / std::pow(csr, 0.36), 1.67);
                    }
                    const double n = nSd / nRd;
                    const double sum = (r.MRd_x > 0.0 ? mx1c / r.MRd_x : 0.0) + (r.MRd_y > 0.0 ? my1c / r.MRd_y : 0.0);
                    ratio = n >= cp ? n + (1.0 - cp) / cm * sum : n * (1.0 - cm) / cp + sum;
                    lines.push_back(fmt("Modelo de calculo III (M.5.4): c_sr = (Aa fy + As fys)/(Ac fck) = %.3f; c_p = %.3f; c_m = %.3f; NSd/NRd = %.3f %s c_p -> %s = %.3f  (%s)", csr, cp, cm, n, n >= cp ? ">=" : "<",
                                        n >= cp ? "N/NRd + ((1 - c_p)/c_m)(Mx/Mx,Rd + My/My,Rd)" : "(N/NRd)((1 - c_m)/c_p) + (Mx/Mx,Rd + My/My,Rd)", ratio, where().c_str()));
                    if (!s.filled()) {
                        ratio = std::max(ratio, 1.0 + 1e-6);
                        lines.push_back("O modelo de calculo III so se aplica a secoes tubulares preenchidas com concreto (M.5.1.3).");
                    }
                }
                keeper.offer(makeCheck("interaction", nSd > 0.0 ? "Flexo-compressao do pilar misto" : "Flexao composta do pilar misto", tag + ", M.5", ratio, 1.0, "", 1.0, lines), combo.name, st.x);
            }

            // esforco cortante (M.5.1.2-a: resistencia do perfil de aco)
            auto shear = [&](bool alongY, double v) {
                double aw;
                if (s.filled()) {
                    aw = s.circular() ? 2.0 * (s.b1 - 2.0 * s.t) * s.t : 2.0 * ((alongY ? s.b1 : s.b2) - 2.0 * s.t) * s.t;
                } else {
                    aw = alongY ? s.d * s.tw : 2.0 * s.bf * s.tf;
                }
                const double rd = 0.60 * aw * r.fyd;
                return makeCheck(alongY ? "shear_y" : "shear_z", alongY ? "Cortante na direcao da altura do pilar misto" : "Cortante na direcao da largura do pilar misto", tag + ", M.5.1.2-a", v, rd, "kN", kKn,
                                 {fmt("VRd = 0,60 Aw fyd = 0,60 x %.0f x %.1f = %.1f kN (so o perfil de aco; o concreto armado nao e considerado); V_Sd = %.1f kN  (%s)", aw, r.fyd, rd / kKn, v / kKn, where().c_str())});
            };
            if (std::abs(e.Vy) > 0.0) {
                keeper.offer(shear(true, std::abs(e.Vy)), combo.name, st.x);
            }
            if (std::abs(e.Vz) > 0.0) {
                keeper.offer(shear(false, std::abs(e.Vz)), combo.name, st.x);
            }
        }
    }

    // ---- M.2: cisalhamento nas superficies de contato (introducao de cargas nas extremidades) -------------------------------------------
    {
        double vEnd = 0.0, nEnd = 0.0;
        for (const ComboEffects &combo : in.combos) {
            if (combo.stations.empty()) {
                continue;
            }
            for (const StationEffects *st : {&combo.stations.front(), &combo.stations.back()}) {
                vEnd = std::max(vEnd, std::max(std::abs(st->e.Vy), std::abs(st->e.Vz)));
                nEnd = std::max(nEnd, std::abs(st->e.N));
            }
        }
        if (vEnd > 0.0) {
            const double vl = vEnd * (1.0 - r.Npl_a / r.Npl);   // M.2.1.2-a: viga ligada so ao perfil de aco (conservador); e a outra alinea para viga so no concreto
            const double ell = 2.0 * (s.filled() ? (s.circular() ? s.b1 : std::min(s.b1, s.b2)) : std::min(s.hc, s.bc));   // comprimento de introducao de carga: 2 x menor dimensao (M.2.1.1)
            if (s.filled()) {
                const double perimeter = s.circular() ? kPi * (s.b1 - 2.0 * s.t) : 2.0 * ((s.b1 - 2.0 * s.t) + (s.b2 - 2.0 * s.t));
                const bool slender = r.classAxial == 2;
                const double tauRd = slender ? 0.0 : compositeInterfaceShearStrength(s, fy, E, false);
                const double tauAlt = slender ? 0.0 : compositeInterfaceShearStrength(s, fy, E, true);
                const double tauAdopted = std::max(tauRd, tauAlt);
                const double tau = vl / (perimeter * ell);
                keeper.offer(makeCheck("cc_interface", "Cisalhamento nas superficies de contato aco-concreto", tag + ", M.2", tau, tauAdopted, "MPa", 1.0,
                                       {fmt("Regiao de introducao de cargas (M.2.1.1): comprimento 2 x menor dimensao = %.0f mm; V_l,Sd = V_Sd (1 - Npl,a,Rd/Npl,Rd) = %.1f kN (M.2.1.2-a: viga ligada so ao perfil)", ell, vl / kKn),
                                        fmt("tau,Rd = Tabela M.2: %.3f MPa; alternativa M.2.1.8: %.3f MPa -> adotado %.3f MPa%s", tauRd, tauAlt, tauAdopted, slender ? " (secao esbelta: tau,Rd = 0)" : ""),
                                        fmt("tau,Sd = V_l,Sd / (perimetro de contato %.0f mm x comprimento de introducao %.0f mm) = %.3f MPa (distribuicao uniforme: hipotese; a norma manda usar as propriedades elasticas da secao com concreto nao fissurado); se tau,Sd > tau,Rd: conectores de cisalhamento (M.2.1.3)", perimeter, ell, tau)}),
                             "extremidades", 0.0);
            } else {
                CheckResult c = makeCheck("cc_interface", "Cisalhamento nas superficies de contato aco-concreto", tag + ", M.2", 0.0, 1.0, "", 1.0, {});
                c.lines.push_back(fmt("Secao revestida: tau,Rd = 0 (Tabela M.2); conectores de cisalhamento obrigatorios nas regioes de introducao de cargas (comprimento %.0f mm acima e abaixo da ligacao, M.2.1.1) e em pelo menos duas faces opostas "
                                      "do perfil nos trechos intermediarios (M.2.2.3), espacamento <= 600 mm (M.1.3-j); V_l,Sd = %.1f kN a transferir (M.2.1.2-a). O numero de conectores segue L.4.2.1 (nao calculado aqui).", 2.0 * std::min(s.hc, s.bc), vl / kKn));
                keeper.offer(std::move(c), "extremidades", 0.0);
            }
        }
    }

    for (const std::string &id : keeper.order()) {
        const Best &best = keeper.best(id);
        out.checks.push_back(best.check);
        if (best.check.ratio > out.maxRatio) {
            out.maxRatio = best.check.ratio;
            out.governingCheck = id;
            out.governingCombo = best.combo;
            out.governingX = best.x;
        }
    }
    if (out.checks.empty()) {
        out.warnings.push_back("Sem esforcos nas combinacoes selecionadas: execute a analise e selecione combinacoes ultimas.");
    }
    return out;
}

} // namespace Structura::Design
