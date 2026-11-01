#include "design/Nbr14323.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <tuple>

namespace Structura::Design::Nbr14323 {

using Nbr8800::fmt;

namespace {

constexpr double kKn = 1e3;
constexpr double kKnm = 1e6;
constexpr double kRho = 7850.0;   // 5.1.1.3

double sq(double v) { return v * v; }

struct Row {
    double theta;
    double ky, kE, kSigma;
};

// Tabelas 1 e 2 da NBR 14323:2013
constexpr Row kRows[] = {
    {20.0, 1.000, 1.000, 1.000},   {100.0, 1.000, 1.000, 1.000}, {200.0, 1.000, 0.900, 0.890}, {300.0, 1.000, 0.800, 0.780}, {400.0, 1.000, 0.700, 0.650},
    {500.0, 0.780, 0.600, 0.530},  {600.0, 0.470, 0.310, 0.300},  {700.0, 0.230, 0.130, 0.130}, {800.0, 0.110, 0.090, 0.070}, {900.0, 0.060, 0.068, 0.050},
    {1000.0, 0.040, 0.045, 0.030}, {1100.0, 0.020, 0.023, 0.020}, {1200.0, 0.000, 0.000, 0.000}};

double interpolateRow(double theta, double Row::*member)
{
    constexpr std::size_t n = sizeof(kRows) / sizeof(kRows[0]);
    if (theta <= kRows[0].theta) {
        return kRows[0].*member;
    }
    for (std::size_t i = 1; i < n; ++i) {
        if (theta <= kRows[i].theta) {
            const double t = (theta - kRows[i - 1].theta) / (kRows[i].theta - kRows[i - 1].theta);
            return kRows[i - 1].*member + t * (kRows[i].*member - kRows[i - 1].*member);
        }
    }
    return 0.0;
}

double ratioOf(double demand, double resistance)
{
    if (resistance <= 1e-12) {
        return demand > 1e-9 ? 1e9 : 0.0;
    }
    return demand / resistance;
}

/// Perimetro (mm) e dimensoes do retangulo envolvente do contorno externo da secao.
struct Outline {
    double perimeter {0.0};
    double width {0.0};
    double height {0.0};
};

Outline outlineOf(const DesignSection &s)
{
    Outline o;
    const Loop &loop = s.outline;
    if (loop.size() >= 3) {
        double aMin = loop.front()[0], aMax = aMin, bMin = loop.front()[1], bMax = bMin;
        for (std::size_t i = 0; i < loop.size(); ++i) {
            const auto &p = loop[i];
            const auto &q = loop[(i + 1) % loop.size()];
            o.perimeter += std::hypot(q[0] - p[0], q[1] - p[1]);
            aMin = std::min(aMin, p[0]);
            aMax = std::max(aMax, p[0]);
            bMin = std::min(bMin, p[1]);
            bMax = std::max(bMax, p[1]);
        }
        o.width = aMax - aMin;
        o.height = bMax - bMin;
        return o;
    }
    o.width = s.g.aMax - s.g.aMin;
    o.height = s.g.bMax - s.g.bMin;
    if (s.kind == SectionKind::RoundTube || s.kind == SectionKind::SolidRound) {
        o.perimeter = kPi * s.D;
    } else if (s.kind == SectionKind::I) {
        o.perimeter = 4.0 * s.bf + 2.0 * s.d - 2.0 * s.tw;
    } else {
        o.perimeter = 2.0 * (o.width + o.height);
    }
    return o;
}

/// Fator de reducao chi_fi (8.4.2.1.5).
double chiFire(double lambda0, double E, double fy)
{
    const double alpha = 0.022 * std::sqrt(E / fy);
    const double lambda = lambda0 / 0.85;
    const double phi = 0.5 * (1.0 + alpha * lambda + sq(lambda));
    return 1.0 / (phi + std::sqrt(std::max(0.0, sq(phi) - sq(lambda))));
}

} // namespace

double ky(double theta) { return interpolateRow(theta, &Row::ky); }
double kE(double theta) { return interpolateRow(theta, &Row::kE); }
double kSigma(double theta) { return interpolateRow(theta, &Row::kSigma); }

double standardFireGas(double minutes)
{
    return 20.0 + 345.0 * std::log10(8.0 * std::max(minutes, 0.0) + 1.0);
}

double steelSpecificHeat(double theta)
{
    if (theta < 600.0) {
        return 425.0 + 7.73e-1 * theta - 1.69e-3 * sq(theta) + 2.22e-6 * theta * sq(theta);
    }
    if (theta < 735.0) {
        return 666.0 + 13002.0 / (738.0 - theta);
    }
    if (theta < 900.0) {
        return 545.0 + 17820.0 / (theta - 731.0);
    }
    return 650.0;
}

Thermal steelTemperature(const DesignSection &s, const Protection &p, bool threeSides, double trrfMinutes)
{
    Thermal r;
    const Outline o = outlineOf(s);
    const double ag = s.g.area / 1e6;   // m2
    if (ag <= 0.0 || o.perimeter <= 0.0) {
        r.lines.push_back("Secao sem geometria valida: temperatura do aco igual a 20 C.");
        return r;
    }
    const bool protectedSteel = p.type != 0 && p.thickness > 0.0 && p.conductivity > 0.0;
    const double dt = 5.0;   // 8.5.1.1.4 (sem revestimento: <= 5 s) e 8.5.1.2.3 (com revestimento: <= 30 s)
    const double total = std::max(trrfMinutes, 0.0) * 60.0;
    double theta = 20.0;

    if (!protectedSteel) {
        const double exposed = (threeSides ? o.perimeter - o.width : o.perimeter) / 1000.0;
        double um = exposed / ag;
        double ksh = 1.0;
        std::string shadow = "ksh = 1,0";
        if (s.kind == SectionKind::I) {
            // 8.5.1.1.2: fator de sombreamento das secoes I ou H
            const double boxPerimeter = (threeSides ? 2.0 * o.height + o.width : 2.0 * (o.height + o.width)) / 1000.0;
            ksh = 0.9 * (boxPerimeter / ag) / um;
            shadow = fmt("ksh = 0,9 (u/Ag)b / (u/Ag) = 0,9 x %.1f / %.1f = %.3f", boxPerimeter / ag, um, ksh);
        }
        const double umUsed = std::max(um, 10.0);   // 8.5.1.1.5
        for (double t = 0.0; t < total - 1e-9; t += dt) {
            const double step = std::min(dt, total - t);
            const double gas = standardFireGas(t / 60.0);
            const double flux = 25.0 * (gas - theta) + 5.67e-8 * 0.7 * (std::pow(gas + 273.0, 4) - std::pow(theta + 273.0, 4));
            theta += ksh * umUsed / (steelSpecificHeat(theta) * kRho) * flux * step;
        }
        r.massivity = umUsed;
        r.ksh = ksh;
        r.lines.push_back(fmt("Aco sem revestimento (8.5.1.1), %s: perimetro exposto u = %.3f m, Ag = %.3e m2, u/Ag = %.1f m-1 (minimo 10); %s; alfa_c = 25 W/m2 C, eps_res = 0,7, dt = %.0f s", threeSides ? "tres lados expostos" : "todos os lados expostos",
                              exposed, ag, um, shadow.c_str(), dt));
    } else {
        double perimeter;
        if (p.type == 2) {   // tipo caixa: perimetro do retangulo envolvente
            perimeter = (threeSides ? 2.0 * o.height + o.width : 2.0 * (o.height + o.width)) / 1000.0;
        } else {             // tipo contorno
            perimeter = (threeSides ? o.perimeter - o.width : o.perimeter) / 1000.0;
        }
        const double um = perimeter / ag;
        const double tm = p.thickness / 1000.0;
        for (double t = 0.0; t < total - 1e-9; t += dt) {
            const double step = std::min(dt, total - t);
            const double gas = standardFireGas(t / 60.0);
            const double gasNext = standardFireGas((t + step) / 60.0);
            const double dGas = gasNext - gas;
            const double ca = steelSpecificHeat(theta);
            const double xi = p.specificHeat * p.density / (ca * kRho) * tm * um;
            double d = p.conductivity * um / (tm * kRho * ca) * (gas - theta) / (1.0 + xi / 4.0) * step - dGas / (4.0 / xi + 1.0);
            if (d < 0.0 && dGas > 0.0) {
                d = 0.0;
            }
            theta += d;
        }
        r.massivity = um;
        r.lines.push_back(fmt("Aco com revestimento tipo %s (8.5.1.2): espessura tm = %.1f mm, lambda_m = %.3f W/m C, rho_m = %.0f kg/m3, c_m = %.0f J/kg C; %s; um/Ag = %.1f m-1, dt = %.0f s",
                              p.type == 2 ? "caixa" : "contorno", p.thickness, p.conductivity, p.density, p.specificHeat, threeSides ? "tres lados expostos" : "todos os lados expostos", um, dt));
    }
    r.theta = std::clamp(theta, 20.0, 1200.0);
    r.lines.push_back(fmt("Temperatura do aco apos %.0f min de incendio-padrao (theta_g = %.0f C): theta_a = %.0f C", trrfMinutes, standardFireGas(trrfMinutes), r.theta));
    return r;
}

// ---------------------------------------------------------------------------------------------------------------------

namespace {

struct Candidate {
    CheckResult check;
    std::string combo;
    double x {0.0};
};

double interpolateMoment(const std::vector<StationEffects> &st, double x, double Effects::*member)
{
    if (st.empty()) {
        return 0.0;
    }
    if (x <= st.front().x) {
        return st.front().e.*member;
    }
    for (std::size_t i = 1; i < st.size(); ++i) {
        if (x <= st[i].x) {
            const double t = (x - st[i - 1].x) / std::max(1e-12, st[i].x - st[i - 1].x);
            return st[i - 1].e.*member + t * (st[i].e.*member - st[i - 1].e.*member);
        }
    }
    return st.back().e.*member;
}

} // namespace

BarDesignOutput designBarFire(const BarDesignInput &in, const FireInput &fire)
{
    BarDesignOutput out;
    out.standard = "ABNT NBR 14323:2013";
    if (!in.section || !in.section->supported() || in.length <= 0.0) {
        out.warnings.push_back("Incendio: secao nao suportada pelo dimensionamento.");
        return out;
    }
    out.supported = true;
    const DesignSection &s = *in.section;
    const bool cold = (s.standard == Standard::NBR14762);
    const double L = in.length;
    const double E = in.steel.E;
    const double fy = in.steel.fy;

    const Thermal thermal = steelTemperature(s, fire.protection, fire.threeSides, fire.trrf);
    const double theta = thermal.theta;
    const double kyT = ky(theta), kET = kE(theta), ksT = kSigma(theta);
    const bool protectedSteel = fire.protection.type != 0 && fire.protection.thickness > 0.0;
    const double kappa = fire.threeSides ? (protectedSteel ? 1.15 : 1.40) : 1.0;

    out.summaryLines.push_back(fmt("Incendio (NBR 14323:2013, metodo simplificado), TRRF = %.0f min.", fire.trrf));
    for (const std::string &line : thermal.lines) {
        out.summaryLines.push_back(line);
    }
    out.summaryLines.push_back(fmt("Fatores de reducao a %.0f C (Tabelas 1 e 2): ky = %.3f, kE = %.3f, k_sigma = %.3f; resistencias com coeficiente de ponderacao unitario (6.4).", theta, kyT, kET, ksT));
    out.summaryLines.push_back(fire.basis);

    // Opcoes sem coeficientes de ponderacao (6.4); a ruptura da secao liquida nao e verificada em incendio (8.3.1)
    Nbr8800::Options o8 = in.opt8800;
    o8.gammaA1 = 1.0;
    o8.gammaA2 = 1.0;
    o8.netAreaRatio = 1.0;
    o8.ct = 1.0;
    o8.stiffenerSpacing = in.stiffenerSpacing;
    o8.compositeSpacing = in.compositeSpacing;
    o8.compositeWelded = in.compositeWelded;
    o8.barLength = in.length;
    o8.threadedRod = false;
    o8.slendernessSection = in.slendernessSection;
    o8.ixOverride = in.ixEquivalent;
    Nbr14762::Options o14 = in.opt14762;
    o14.gammaTension = o14.gammaRuptureFar = o14.gammaRuptureConn = o14.gammaCompression = o14.gammaFlexure = 1.0;
    o14.netAreaRatio = 1.0;
    o14.ct = 1.0;
    o14.coldWork = false;
    o14.panelTensionFlange = in.panelTensionFlange;

    auto steelWith = [&](double fyScale, double eScale) {
        SteelGrade g = in.steel;
        g.fy *= fyScale;
        g.E *= eScale;
        g.G *= eScale;
        return g;
    };
    constexpr double kLimit = 0.85 * 0.85;   // (b/t)lim e lambda_p,lim de incendio = 0,85 dos limites a frio: equivale a E x 0,85^2 nas expressoes ~ sqrt(E/fy)
    const SteelGrade steelLocal = steelWith(1.0, kLimit);

    const Nbr8800::Buckling len {in.lxOverride > 0.0 ? in.lxOverride : in.Kx * L, in.lyOverride > 0.0 ? in.lyOverride : in.Ky * L, in.lzOverride > 0.0 ? in.lzOverride : in.Kz * L};
    const Nbr8800::Buckling noLen {0.0, 0.0, 0.0};
    const int segments = std::max(1, in.lateralSegments);
    const double lb = in.compressionFlangeBraced ? 0.0 : (in.lbOverride > 0.0 ? in.lbOverride : L / segments);

    // ---- Tracao (8.4.1.2) ----
    const double nTensionRd = kyT * s.g.area * fy;

    // ---- Compressao (8.4.2) ----
    const Nbr8800::CompressionData c0 = cold ? Nbr14762::compression(s, in.steel, len, o14) : Nbr8800::compression(s, in.steel, len, o8);
    // flambagem local em incendio: algum elemento com (b/t) > 0,85 (b/t)lim, ou lambda_p > 0,85 lambda_p,lim (8.4.2.1.2 e 8.4.2.1.3); avaliada com E reduzido a 0,85^2 E e sigma = fy
    const Nbr8800::CompressionData cLocal = cold ? Nbr14762::compression(s, steelLocal, noLen, o14) : Nbr8800::compression(s, steelLocal, noLen, o8);
    const Nbr8800::CompressionData cFull = cold ? Nbr14762::compression(s, in.steel, noLen, o14) : Nbr8800::compression(s, in.steel, noLen, o8);
    const bool localBuckling = cLocal.covered && cLocal.Aef < s.g.area * (1.0 - 1e-6);
    const double q = localBuckling ? std::clamp(cFull.Aef / s.g.area, 0.05, 1.0) : 1.0;
    const double lambdaFire0 = c0.lambda0 * std::sqrt(q);
    const double chiFi = chiFire(lambdaFire0, E, fy);
    double nCompressionRd;
    std::vector<std::string> compLines;
    if (localBuckling) {
        nCompressionRd = chiFi * ksT * q * s.g.area * fy;
        compLines.push_back(fmt("Flambagem local em incendio (8.4.2.2): elemento acima de 0,85 do limite; Aef = %.0f mm2 (Q = %.3f); N_fi,Rd = chi_fi k_sigma Aef fy = %.3f x %.3f x %.0f x %.0f = %.2f kN", q * s.g.area, q, chiFi,
                                ksT, q * s.g.area, fy, nCompressionRd / kKn));
    } else {
        nCompressionRd = chiFi * kyT * s.g.area * fy;
        compLines.push_back(fmt("Sem flambagem local em incendio (8.4.2.1): N_fi,Rd = chi_fi ky Ag fy = %.3f x %.3f x %.0f x %.0f = %.2f kN", chiFi, kyT, s.g.area, fy, nCompressionRd / kKn));
    }
    compLines.push_back(fmt("chi_fi (8.4.2.1.5): lambda0 = %.3f (a frio, %s), lambda0,fi = lambda0/0,85 = %.3f, alfa = 0,022 sqrt(E/fy) = %.3f", lambdaFire0, c0.mode.empty() ? "sem flambagem global" : c0.mode.c_str(),
                            lambdaFire0 / 0.85, 0.022 * std::sqrt(E / fy)));
    if (cold && c0.covered && c0.NcRd > 0.0) {
        // 8.4.2.1.7 e 8.4.2.2.3: a flambagem distorcional (ja incluida na resistencia a frio) tambem limita, com k_sigma
        const double capDistortional = ksT * c0.NcRd;
        if (capDistortional < nCompressionRd) {
            nCompressionRd = capDistortional;
            compLines.push_back(fmt("Limitada por k_sigma x N_c,Rd a frio (inclui a flambagem distorcional, 8.4.2.1.7) = %.2f kN", nCompressionRd / kKn));
        }
    }

    // ---- Flexao (8.4.3) ----
    struct FireFlexure {
        double M {std::numeric_limits<double>::infinity()};
        std::string how;
    };
    auto flexCall = [&](const SteelGrade &steel, Axis ax, bool pos, double lbv, double cbv) {
        return cold ? Nbr14762::flexure(s, steel, ax, pos, lbv, cbv, o14) : Nbr8800::flexure(s, steel, ax, pos, lbv, cbv, o8);
    };
    std::map<std::tuple<int, bool>, std::pair<Nbr8800::FlexureData, Nbr8800::FlexureData>> localCache;
    std::map<std::tuple<int, bool, long long>, FireFlexure> flexCache;
    auto flexOf = [&](Axis ax, bool pos, double cbv) -> const FireFlexure & {
        const auto key = std::make_tuple(static_cast<int>(ax), pos, std::llround(cbv * 1000.0));
        auto found = flexCache.find(key);
        if (found != flexCache.end()) {
            return found->second;
        }
        FireFlexure r;
        const auto lkey = std::make_tuple(static_cast<int>(ax), pos);
        auto lit = localCache.find(lkey);
        if (lit == localCache.end()) {
            lit = localCache.emplace(lkey, std::make_pair(flexCall(in.steel, ax, pos, 0.0, 1.0), flexCall(steelLocal, ax, pos, 0.0, 1.0))).first;
        }
        const Nbr8800::FlexureData &fd0 = lit->second.first;
        const Nbr8800::FlexureData &fdLocal = lit->second.second;
        if (!fd0.covered) {
            return flexCache.emplace(key, r).first->second;
        }
        const double w = ax == Axis::X ? s.g.Wx() : s.g.Wy();
        const double my = w * fy;
        const double tol = 1e-6;
        auto flt = [&]() { return lb > 0.0 ? flexCall(in.steel, ax, pos, lb, cbv) : fd0; };
        if (cold) {
            const bool noLocal = fdLocal.fullyEffective;
            const double fa = noLocal ? kyT : ksT;
            double m = fa * fd0.MRd;
            r.how = fmt("%s: M_fi,Rd = %s x M_Rd(a frio, secao efetiva) = %.3f x %.2f = %.2f kN*m (8.4.3.3.%s)", noLocal ? "Sem flambagem local em incendio" : "Flambagem local em incendio", noLocal ? "ky" : "k_sigma", fa,
                        fd0.MRd / kKnm, m / kKnm, noLocal ? "4" : "2");
            if (lb > 0.0) {
                const Nbr8800::FlexureData fdL = flt();
                bool hasFlt = false;
                for (const std::string &line : fdL.lines) {
                    hasFlt = hasFlt || line.find("FLT") != std::string::npos;
                }
                if (hasFlt) {
                    const double factor = std::min(fa, kET);
                    const double mFlt = factor * fdL.MRd;
                    if (mFlt < m) {
                        m = mFlt;
                        r.how += fmt("; FLT: min(%s, kE) x M_Rd(a frio) = %.3f x %.2f = %.2f kN*m (aproximacao conservadora de 8.4.3.3.2)", noLocal ? "ky" : "k_sigma", factor, fdL.MRd / kKnm, mFlt / kKnm);
                    }
                }
            }
            r.M = m;
        } else {
            const double hOverTw = (s.kind == SectionKind::I && s.tw > 0.0 && s.h > 0.0) ? s.h / s.tw : 0.0;
            if (hOverTw > 4.84 * std::sqrt(E / fy)) {
                // 8.4.3.2.1: I soldado de alma esbelta: Anexo H com fy -> k_sigma fy e E -> kE E
                const Nbr8800::FlexureData fdS = flexCall(steelWith(ksT, kET), ax, pos, lb, cbv);
                r.M = fdS.MRd;
                r.how = fmt("Alma esbelta (h/tw = %.1f > 4,84 sqrt(E/fy)): resistencia a frio com fy -> k_sigma fy e E -> kE E (8.4.3.2.1): M_fi,Rd = %.2f kN*m", hOverTw, r.M / kKnm);
            } else {
                double term;
                if (fdLocal.MRd >= fd0.Mpl * (1.0 - tol)) {
                    term = kappa * kyT * fd0.Mpl;
                    r.how = fmt("lambda <= 0,85 lambda_p: M_fi,Rd = kappa ky Mpl = %.2f x %.3f x %.2f = %.2f kN*m (8.4.3.2.3)", kappa, kyT, fd0.Mpl / kKnm, term / kKnm);
                } else if (fdLocal.MRd >= 0.7 * my * (1.0 - tol)) {
                    term = kyT * my;
                    r.how = fmt("0,85 lambda_p < lambda <= 0,85 lambda_r: M_fi,Rd = ky My = %.3f x %.2f = %.2f kN*m (8.4.3.2.3)", kyT, my / kKnm, term / kKnm);
                } else {
                    term = ksT * my;
                    r.how = fmt("lambda > 0,85 lambda_r: M_fi,Rd = k_sigma My = %.3f x %.2f = %.2f kN*m (8.4.3.2.3)", ksT, my / kKnm, term / kKnm);
                }
                if (lb > 0.0 && ax == Axis::X) {
                    const Nbr8800::FlexureData fdL = flt();
                    bool hasFlt = false;
                    for (const std::string &line : fdL.lines) {
                        hasFlt = hasFlt || line.find("FLT") != std::string::npos;
                    }
                    if (hasFlt) {
                        const double mcr = Nbr8800::lateralBucklingMoment(s, in.steel, lb, cbv);
                        double mFlt;
                        if (mcr > 0.0) {
                            const double lambda0 = std::sqrt(fd0.Mpl / mcr);
                            const double chi = chiFire(lambda0, E, fy);
                            mFlt = kappa * chi * kyT * fd0.Mpl;
                            if (mFlt < term) {
                                term = mFlt;
                                r.how = fmt("FLT (8.4.3.2.3): Mcr = %.2f kN*m (Cb = %.2f, Lb = %.0f mm), lambda0 = sqrt(Mpl/Mcr) = %.3f, chi_fi = %.3f: M_fi,Rd = kappa chi_fi ky Mpl = %.2f x %.3f x %.3f x %.2f = %.2f kN*m",
                                            mcr / kKnm, cbv, lb, lambda0, chi, kappa, chi, kyT, fd0.Mpl / kKnm, term / kKnm);
                            }
                        } else {
                            mFlt = std::min(ksT, kET) * fdL.MRd;
                            if (mFlt < term) {
                                term = mFlt;
                                r.how = fmt("FLT: min(k_sigma, kE) x M_Rd(a frio) = %.3f x %.2f = %.2f kN*m (aproximacao conservadora; Mcr nao disponivel para essa secao)", std::min(ksT, kET), fdL.MRd / kKnm, term / kKnm);
                            }
                        }
                    }
                }
                r.M = term;
            }
        }
        return flexCache.emplace(key, r).first->second;
    };

    // ---- Cortante (8.4.3.4) ----
    struct FireShear {
        double V {std::numeric_limits<double>::infinity()};
        std::string how;
    };
    std::map<int, FireShear> shearCache;
    auto shearOf = [&](Axis dir) -> const FireShear & {
        const int key = static_cast<int>(dir);
        auto it = shearCache.find(key);
        if (it != shearCache.end()) {
            return it->second;
        }
        FireShear r;
        auto call = [&](double fyScale, double eScale) {
            const SteelGrade g = steelWith(fyScale, eScale);
            return cold ? Nbr14762::shear(s, g, dir, o14) : Nbr8800::shear(s, g, dir, o8, L / 2.0);
        };
        const double eFire = kET * kLimit;   // lambda_p,fi = 0,85 lambda_p e E x kE
        const Nbr8800::ShearData v1 = call(kyT, eFire);
        if (v1.covered) {
            const Nbr8800::ShearData v2 = call(kyT, eFire * 1.1);
            const bool elastic = v1.VRd > 0.0 && v2.VRd / v1.VRd > 1.075;   // V proporcional a E: flambagem elastica da alma (lambda > lambda_r,fi): usa k_sigma
            if (elastic) {
                const Nbr8800::ShearData v3 = call(ksT, eFire);
                r.V = v3.VRd;
                r.how = fmt("lambda > lambda_r,fi: resistencia a frio com fy -> k_sigma fy, E -> kE E, limites x 0,85: V_fi,Rd = %.2f kN", r.V / kKn);
            } else {
                r.V = v1.VRd;
                r.how = fmt("lambda <= lambda_r,fi: resistencia a frio com fy -> ky fy, E -> kE E, limites x 0,85: V_fi,Rd = %.2f kN", r.V / kKn);
            }
        }
        return shearCache.emplace(key, r).first->second;
    };

    // ---- Verificacoes por combinacao e secao ----
    std::map<std::string, Candidate> best;
    auto offer = [&](const std::string &id, const std::string &title, const std::string &clause, double demand, double resistance, const char *unit, double unitFactor, const std::string &comboName, double x,
                     const std::function<std::vector<std::string>()> &lines) {
        const double ratio = ratioOf(demand, resistance);
        auto it = best.find(id);
        if (it != best.end() && it->second.check.ratio >= ratio) {
            return;
        }
        Candidate c;
        c.check.id = id;
        c.check.title = title;
        c.check.clause = clause;
        c.check.demand = demand / unitFactor;
        c.check.resistance = std::isinf(resistance) ? 0.0 : resistance / unitFactor;
        c.check.ratio = ratio;
        c.check.unit = unit;
        c.check.lines = lines();
        c.combo = comboName;
        c.x = x;
        best[id] = std::move(c);
    };

    bool anyTorsion = false;
    for (const ComboEffects &combo : fire.combos) {
        if (combo.stations.empty()) {
            continue;
        }
        const double b2All = in.b2 * combo.b2;
        // Cm e Ne para B1 (efeito P-delta local; propriedades a temperatura ambiente, 8.2.1)
        auto cmFor = [&](double Effects::*member, bool transverse) {
            if (transverse) {
                return 1.0;
            }
            const double ma = combo.stations.front().e.*member;
            const double mb = combo.stations.back().e.*member;
            const double m2 = std::max(std::abs(ma), std::abs(mb));
            if (m2 < 1e-9) {
                return 1.0;
            }
            const double m1 = std::min(std::abs(ma), std::abs(mb));
            const double ratio = (ma * mb < 0.0 ? 1.0 : -1.0) * m1 / m2;
            return 0.60 - 0.40 * ratio;
        };
        const double cmX = cmFor(&Effects::Mx, combo.transverseLoadX);
        const double cmY = cmFor(&Effects::My, combo.transverseLoadY);
        const double neX = sq(kPi) * E * (in.ixEquivalent > 0.0 ? in.ixEquivalent : s.g.Ix) / sq(L) * combo.bStiffness;
        const double neY = sq(kPi) * E * s.g.Iy / sq(L) * combo.bStiffness;

        // Cb por trecho destravado (Cb = 1,0 quando adotado pelo usuario ou em balanco)
        std::vector<double> cb(static_cast<std::size_t>(segments), 1.0);
        if (!in.cbEqualsOne && !in.cantilever && lb > 0.0) {
            for (int k = 0; k < segments; ++k) {
                const double x0 = k * L / segments, x1 = (k + 1) * L / segments;
                double mMax = 0.0;
                for (const StationEffects &st : combo.stations) {
                    if (st.x >= x0 - 1e-6 && st.x <= x1 + 1e-6) {
                        mMax = std::max(mMax, std::abs(st.e.Mx));
                    }
                }
                mMax = std::max({mMax, std::abs(interpolateMoment(combo.stations, x0, &Effects::Mx)), std::abs(interpolateMoment(combo.stations, x1, &Effects::Mx))});
                const double ma = interpolateMoment(combo.stations, x0 + 0.25 * (x1 - x0), &Effects::Mx);
                const double mb = interpolateMoment(combo.stations, x0 + 0.50 * (x1 - x0), &Effects::Mx);
                const double mc = interpolateMoment(combo.stations, x0 + 0.75 * (x1 - x0), &Effects::Mx);
                cb[static_cast<std::size_t>(k)] = std::min(3.0, Nbr8800::cbFromQuarterMoments(mMax, ma, mb, mc));
            }
        }

        for (const StationEffects &st : combo.stations) {
            const Effects &e = st.e;
            anyTorsion = anyTorsion || std::abs(e.T) > 1e3;
            const std::string where = fmt("combinacao %s, x = %.0f mm", combo.name.c_str(), st.x);
            double b1x = 1.0, b1y = 1.0;
            double unstable = 0.0;
            if (in.amplifyB1 && e.N < 0.0) {
                const double nc = -e.N;
                auto amplify = [&](double ne, double cm, double &b1) {
                    const double allowed = (1.0 - cm / 20.0) * ne;
                    if (nc >= allowed) {
                        unstable = std::max(unstable, allowed > 0.0 ? nc / allowed : 1e9);
                    } else {
                        b1 = std::max(1.0, cm / (1.0 - nc / ne));
                    }
                };
                amplify(neX, cmX, b1x);
                amplify(neY, cmY, b1y);
            }
            const double mx = e.Mx * b1x * b2All;
            const double my = e.My * b1y * b2All;
            const double nAbs = std::abs(e.N);
            const bool compressed = e.N < 0.0;
            const double nRd = compressed ? nCompressionRd : nTensionRd;

            if (unstable >= 1.0) {
                offer("fire_interaction", "Interacao de esforcos em incendio", "NBR 14323:2013, 8.4.4", unstable, 1.0, "", 1.0, combo.name, st.x, [&]() {
                    return std::vector<std::string>{fmt("N_Sd >= N_e (B1 > 20): barra instavel no plano de flexao (%s)", where.c_str())};
                });
            }
            if (nAbs > 1e-9) {
                if (compressed) {
                    offer("fire_compression", "Compressao axial em incendio", "NBR 14323:2013, 8.4.2", nAbs, nRd, "kN", kKn, combo.name, st.x, [&]() {
                        std::vector<std::string> l = compLines;
                        l.push_back(fmt("N_fi,Sd = %.2f kN; N_fi,Rd = %.2f kN (%s)", nAbs / kKn, nRd / kKn, where.c_str()));
                        return l;
                    });
                } else {
                    offer("fire_tension", "Tracao axial em incendio (escoamento da secao bruta)", "NBR 14323:2013, 8.4.1.2", nAbs, nRd, "kN", kKn, combo.name, st.x, [&]() {
                        return std::vector<std::string>{fmt("N_fi,Rd = ky Ag fy = %.3f x %.0f x %.0f = %.2f kN; N_fi,Sd = %.2f kN (%s)", kyT, s.g.area, fy, nRd / kKn, nAbs / kKn, where.c_str())};
                    });
                }
            }

            const int seg = std::min(segments - 1, static_cast<int>(st.x / (L / segments)));
            const double cbHere = cb[static_cast<std::size_t>(seg)];
            double mxRd = std::numeric_limits<double>::infinity(), myRd = mxRd;
            if (std::abs(mx) > 1e-9) {
                const FireFlexure &f = flexOf(Axis::X, mx > 0.0, cbHere);
                mxRd = f.M;
                if (!std::isinf(f.M)) {
                    offer("fire_flexure_x", "Flexao em torno de X em incendio", "NBR 14323:2013, 8.4.3", std::abs(mx), f.M, "kN*m", kKnm, combo.name, st.x, [&]() {
                        return std::vector<std::string>{f.how, fmt("M_fi,Sd = %.2f kN*m (B1 = %.3f, B2 = %.3f); M_fi,Rd = %.2f kN*m (%s)", std::abs(mx) / kKnm, b1x, b2All, f.M / kKnm, where.c_str())};
                    });
                }
            }
            if (std::abs(my) > 1e-9) {
                const FireFlexure &f = flexOf(Axis::Y, my > 0.0, 1.0);
                myRd = f.M;
                if (!std::isinf(f.M)) {
                    offer("fire_flexure_y", "Flexao em torno de Y em incendio", "NBR 14323:2013, 8.4.3", std::abs(my), f.M, "kN*m", kKnm, combo.name, st.x, [&]() {
                        return std::vector<std::string>{f.how, fmt("M_fi,Sd = %.2f kN*m (B1 = %.3f, B2 = %.3f); M_fi,Rd = %.2f kN*m (%s)", std::abs(my) / kKnm, b1y, b2All, f.M / kKnm, where.c_str())};
                    });
                }
            }
            if (std::abs(e.Vy) > 1e-9) {
                const FireShear &v = shearOf(Axis::Y);
                if (!std::isinf(v.V)) {
                    offer("fire_shear_y", "Cortante (paralelo a alma) em incendio", "NBR 14323:2013, 8.4.3.4", std::abs(e.Vy), v.V, "kN", kKn, combo.name, st.x,
                          [&]() { return std::vector<std::string>{v.how, fmt("V_fi,Sd = %.2f kN (%s)", std::abs(e.Vy) / kKn, where.c_str())}; });
                }
            }
            if (std::abs(e.Vz) > 1e-9) {
                const FireShear &v = shearOf(Axis::X);
                if (!std::isinf(v.V)) {
                    offer("fire_shear_z", "Cortante (paralelo as mesas) em incendio", "NBR 14323:2013, 8.4.3.4", std::abs(e.Vz), v.V, "kN", kKn, combo.name, st.x,
                          [&]() { return std::vector<std::string>{v.how, fmt("V_fi,Sd = %.2f kN (%s)", std::abs(e.Vz) / kKn, where.c_str())}; });
                }
            }

            // Interacao N + M (8.4.4)
            if (nAbs > 1e-9 && (std::abs(mx) > 1e-9 || std::abs(my) > 1e-9)) {
                const double nr = ratioOf(nAbs, nRd);
                const double fx = std::isinf(mxRd) ? 0.0 : ratioOf(std::abs(mx), mxRd);
                const double fyy = std::isinf(myRd) ? 0.0 : ratioOf(std::abs(my), myRd);
                double r;
                std::string formula;
                if (cold) {
                    r = nr + fx + fyy;
                    formula = "NBR 14762 (8.4.4.2): N/N_Rd + Mx/Mx,Rd + My/My,Rd";
                } else if (nr >= 0.2) {
                    r = nr + 8.0 / 9.0 * (fx + fyy);
                    formula = "8.4.4.1 (N_Sd/N_Rd >= 0,2): N/N_Rd + 8/9 (Mx/Mx,Rd + My/My,Rd)";
                } else {
                    r = 0.5 * nr + fx + fyy;
                    formula = "8.4.4.1 (N_Sd/N_Rd < 0,2): N/(2 N_Rd) + Mx/Mx,Rd + My/My,Rd";
                }
                offer("fire_interaction", "Interacao de esforcos em incendio", "NBR 14323:2013, 8.4.4", r, 1.0, "", 1.0, combo.name, st.x, [&]() {
                    return std::vector<std::string>{formula, fmt("N/N_Rd = %.3f, Mx/Mx,Rd = %.3f, My/My,Rd = %.3f -> %.3f (%s)", nr, fx, fyy, r, where.c_str())};
                });
            } else if (nAbs <= 1e-9 && (std::abs(mx) > 1e-9 || std::abs(my) > 1e-9)) {
                const double fx = std::isinf(mxRd) ? 0.0 : ratioOf(std::abs(mx), mxRd);
                const double fyy = std::isinf(myRd) ? 0.0 : ratioOf(std::abs(my), myRd);
                offer("fire_interaction", "Interacao de esforcos em incendio", "NBR 14323:2013, 8.4.4", fx + fyy, 1.0, "", 1.0, combo.name, st.x, [&]() {
                    return std::vector<std::string>{fmt("Flexao composta sem forca axial: Mx/Mx,Rd + My/My,Rd = %.3f + %.3f (%s)", fx, fyy, where.c_str())};
                });
            }
        }
    }
    if (anyTorsion) {
        out.warnings.push_back("Incendio: a torcao nao e coberta pelo metodo simplificado da NBR 14323 (8.4); o torque nao entra nas verificacoes de incendio.");
    }
    if (best.empty()) {
        out.warnings.push_back("Incendio: nao ha esforcos solicitantes em situacao de incendio para esta barra.");
    }
    for (auto &entry : best) {
        Candidate &c = entry.second;
        c.check.lines.push_back(fmt("Aproveitamento = %.3f", c.check.ratio));
        if (c.check.ratio > out.maxRatio) {
            out.maxRatio = c.check.ratio;
            out.governingCheck = c.check.id;
            out.governingCombo = c.combo;
            out.governingX = c.x;
        }
        out.checks.push_back(std::move(c.check));
    }
    return out;
}

} // namespace Structura::Design::Nbr14323
