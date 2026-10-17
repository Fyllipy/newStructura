#include "design/CompositeFloor.h"

#include "design/ConnectionHelpers.h"
#include "design/Nbr8800.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design::Composite {

using Connections::detail::fmtn;
using Connections::detail::kPi;
using Connections::detail::minimumRule;
using Connections::detail::rangeMax;
using Connections::detail::rangeMin;
using Connections::detail::ratioMPa;
using Connections::detail::ratioN;
using Connections::detail::ratioNmm;

// ── materiais e geometria ────────────────────────────────────────────────────

double ConcreteInput::ecs() const
{
    const double eci = fck <= 50.0 ? 5600.0 * std::sqrt(fck) : 21500.0 * std::cbrt(fck / 10.0 + 1.25);
    const double alphaI = std::min(1.0, 0.8 + 0.2 * fck / 80.0);
    return alphaI * eci;
}

double ConcreteInput::etaC() const
{
    return std::min(1.0, std::cbrt(40.0 / fck));
}

double ConcreteInput::ctkInf() const
{
    return 0.21 * std::pow(fck, 2.0 / 3.0);
}

double StudInput::area() const
{
    return kPi * diameter * diameter / 4.0;
}

double SteelIBeam::aa() const
{
    return area > 0.0 ? area : 2.0 * bf * tf + hw() * tw;
}

double SteelIBeam::ia() const
{
    if (inertia > 0.0) {
        return inertia;
    }
    return (bf * d * d * d - (bf - tw) * std::pow(hw(), 3.0)) / 12.0;
}

double effectiveWidth(double span, double leftSpacing, double rightSpacing)
{
    auto side = [&](double spacing) { return spacing > 0.0 ? std::min(span / 8.0, spacing / 2.0) : 0.0; };
    return side(leftSpacing) + side(rightSpacing);
}

StudResistance studResistance(const StudInput &studs, const SlabInput &slab, const ConcreteInput &concrete)
{
    StudResistance r;
    const double acs = studs.area();
    switch (slab.kind) {
    case SlabKind::Solid:
        r.rg = 1.0;
        r.rp = 0.75;
        break;
    case SlabKind::DeckPerpendicular:
        r.rg = studs.perRib <= 1 ? 1.0 : (studs.perRib == 2 ? 0.85 : 0.70);
        r.rp = studs.throughDeck ? (studs.emh >= 50.0 ? 0.75 : 0.60) : 0.75;
        break;
    case SlabKind::DeckParallel:
        r.rg = slab.hF > 0.0 && slab.bF / slab.hF >= 1.5 ? 1.0 : 0.85;
        r.rp = 0.75;
        break;
    }
    r.concreteCrushing = 0.5 * acs * std::sqrt(concrete.fck * concrete.ecs()) / studs.gammaCs;
    r.studRupture = r.rg * r.rp * acs * studs.fu / studs.gammaCs;
    r.qRd = std::min(r.concreteCrushing, r.studRupture);
    return r;
}

namespace {

/// Propriedades da secao mista homogeneizada (concreto acima da forma; o concreto das nervuras e desprezado).
struct Homogenised {
    double itr {0.0};
    double yBar {0.0};      ///< do centro... da face inferior do perfil ate o CG
    double wBottom {0.0};   ///< modulo elastico inferior (face inferior do perfil)
    double wTopConcrete {0.0};   ///< modulo elastico superior (topo da laje)
};

Homogenised homogenise(const SteelIBeam &s, const SlabInput &slab, double bEff, double alphaE)
{
    Homogenised h;
    const double aa = s.aa();
    const double ia = s.ia();
    const double ac = bEff * slab.tc / alphaE;
    const double yc = s.d + slab.hF + slab.tc / 2.0;
    h.yBar = (aa * s.d / 2.0 + ac * yc) / (aa + ac);
    h.itr = ia + aa * std::pow(h.yBar - s.d / 2.0, 2.0) + bEff * std::pow(slab.tc, 3.0) / (12.0 * alphaE) + ac * std::pow(yc - h.yBar, 2.0);
    h.wBottom = h.itr / h.yBar;
    h.wTopConcrete = h.itr / (s.d + slab.hF + slab.tc - h.yBar);
    return h;
}

/// Parte do perfil acima de yp (medido do topo): area e centroide a partir do topo (compressao) e abaixo: centroide a partir da base.
struct PlasticSplit {
    double yc {0.0};   ///< CG da parte comprimida ate a face superior
    double yt {0.0};   ///< CG da parte tracionada ate a face inferior
};

PlasticSplit splitSteel(const SteelIBeam &s, double yp)
{
    const double tf = s.tf, bf = s.bf, tw = s.tw, hw = s.hw(), d = s.d;
    PlasticSplit p;
    if (yp <= 1e-9) {
        p.yc = 0.0;
        p.yt = d / 2.0;   // secao inteira tracionada (CG da secao duplamente simetrica)
        return p;
    }
    if (yp <= tf) {
        p.yc = yp / 2.0;
        const double aBottom = bf * tf, aWeb = hw * tw, aTop = bf * (tf - yp);
        p.yt = (aBottom * tf / 2.0 + aWeb * (tf + hw / 2.0) + aTop * (d - (tf + yp) / 2.0)) / (aBottom + aWeb + aTop);
    } else {
        const double web = std::min(hw, yp - tf);
        const double aTopFlange = bf * tf, aWebC = tw * web;
        p.yc = (aTopFlange * tf / 2.0 + aWebC * (tf + web / 2.0)) / (aTopFlange + aWebC);
        const double webT = hw - web;
        const double aBottom = bf * tf, aWebT = tw * webT;
        // alma tracionada: de baixo (tf) ate tf + webT
        p.yt = aBottom + aWebT > 0.0 ? (aBottom * tf / 2.0 + aWebT * (tf + webT / 2.0)) / (aBottom + aWebT) : tf / 2.0;
    }
    return p;
}

double shearResistance(const SteelIBeam &s, const DesignSection *section)
{
    if (section && section->supported()) {
        Nbr8800::Options opt;
        opt.gammaA1 = s.gammaA1;
        SteelGrade steel;
        steel.name = "viga";
        steel.fy = s.fy;
        steel.fu = s.fu;
        steel.E = s.E;
        const auto data = Nbr8800::shear(*section, steel, Axis::Y, opt);
        if (data.covered && data.VRd > 0.0) {
            return data.VRd;
        }
    }
    const double kv = 5.0;
    const double lambda = s.hw() / s.tw;
    const double lambdaP = 1.10 * std::sqrt(kv * s.E / s.fy);
    const double lambdaR = 1.37 * std::sqrt(kv * s.E / s.fy);
    const double vpl = 0.60 * s.d * s.tw * s.fy;
    if (lambda <= lambdaP) {
        return vpl / s.gammaA1;
    }
    if (lambda <= lambdaR) {
        return lambdaP / lambda * vpl / s.gammaA1;
    }
    return 1.24 * std::pow(lambdaP / lambda, 2.0) * vpl / s.gammaA1;
}

double steelMomentResistance(const SteelIBeam &s, const DesignSection *section, double lb)
{
    if (section && section->supported()) {
        Nbr8800::Options opt;
        opt.gammaA1 = s.gammaA1;
        SteelGrade steel;
        steel.name = "viga";
        steel.fy = s.fy;
        steel.fu = s.fu;
        steel.E = s.E;
        const auto data = Nbr8800::flexure(*section, steel, Axis::X, true, lb, 1.0, opt);
        if (data.covered && data.MRd > 0.0) {
            return data.MRd;
        }
    }
    const double zpl = s.zpl > 0.0 ? s.zpl : s.bf * s.tf * (s.d - s.tf) + s.tw * std::pow(s.hw(), 2.0) / 4.0;
    const double lambdaF = s.bf / (2.0 * s.tf), lambdaW = s.hw() / s.tw;
    const bool compact = lambdaF <= 0.38 * std::sqrt(s.E / s.fy) && lambdaW <= 3.76 * std::sqrt(s.E / s.fy);
    return (compact ? zpl : s.wa()) * s.fyd();
}

} // namespace

// ── viga mista ───────────────────────────────────────────────────────────────

BeamResult checkBeam(const BeamInput &in, const BeamDemand &dm, const DesignSection *section)
{
    BeamResult r;
    const SteelIBeam &s = in.steel;
    const double aa = s.aa();
    const double ia = s.ia();
    r.fyd = s.fyd();
    r.fcd = in.concrete.fcd();
    r.etaC = in.concrete.etaC();
    r.ecs = in.concrete.ecs();
    r.alphaE = s.E / r.ecs;
    r.bEff = effectiveWidth(in.span, in.leftSpacing, in.rightSpacing);
    if (r.bEff <= 0.0 || in.slab.tc <= 0.0) {
        r.notes.push_back("Largura efetiva ou espessura da laje nula: sem laje colaborante.");
        return r;
    }
    const auto stud = studResistance(in.studs, in.slab, in.concrete);
    r.qRd = stud.qRd;
    r.rg = stud.rg;
    r.rp = stud.rp;

    const double tMax = aa * r.fyd;
    const double cSlabMax = 0.85 * r.etaC * r.fcd * r.bEff * in.slab.tc;
    r.fhd = std::min(tMax, cSlabMax);
    const int perHalf = in.studs.spacing > 0.0 ? static_cast<int>(std::floor(in.span / (2.0 * in.studs.spacing) + 1e-9)) : 0;
    r.connectorsHalfSpan = perHalf * std::max(1, in.studs.rows);
    r.sumQ = r.connectorsHalfSpan * r.qRd;
    r.connectorsFullInteraction = static_cast<int>(std::ceil(r.fhd / std::max(1.0, r.qRd)));
    r.interaction = std::min(1.0, r.sumQ / r.fhd);

    // ── momento resistente (L.2.3.1.1.1): interacao completa e parcial pela mesma formulacao ──
    auto momentFor = [&](double ccd) {
        const double ccdUsed = std::min({ccd, tMax, cSlabMax});
        const double cad = 0.5 * (tMax - ccdUsed);
        const double a = ccdUsed / (0.85 * r.etaC * r.fcd * r.bEff);
        double yp = 0.0;
        if (cad > 0.0) {
            const double afc = s.bf * s.tf * r.fyd;
            yp = cad <= afc ? cad / afc * s.tf : s.tf + s.hw() * (cad - afc) / (s.hw() * s.tw * r.fyd);
        }
        const auto split = splitSteel(s, yp);
        const double m = cad * (s.d - split.yt - split.yc) + ccdUsed * (in.slab.tc - a / 2.0 + in.slab.hF + s.d - split.yt);
        return in.betaVm * m;
    };
    r.mRdFull = momentFor(r.fhd);
    r.mRd = momentFor(r.sumQ);

    // grau minimo de interacao (L.2.3.1.1.2)
    const double rho = 0.0;   // perfil duplamente simetrico: Afb = Aft
    const double leM = in.span / 1000.0;
    const double eta0 = 1.0 - s.E / (580.0 * s.fy) * (0.75 - 0.45 * rho - (0.030 - 0.015 * rho) * leM);
    const double cr = std::max(0.7, dm.msd / std::max(1.0, r.mRdFull));
    const double cp = in.shored ? 1.0 : 0.85;
    const double etaLim = 0.4 * cr * (1.0 + 0.8 * rho);
    r.interactionMin = std::min(1.0, std::max(cp * cr * cr * eta0, etaLim));

    const auto hom = homogenise(s, in.slab, r.bEff, r.alphaE);
    r.itr = hom.itr;
    r.iEf = ia + std::sqrt(r.interaction) * (hom.itr - ia);
    r.wEf = s.wa() + std::sqrt(r.interaction) * (hom.wBottom - s.wa());

    // ── verificacoes ──
    {
        Check c = ratioNmm("Momento fletor positivo (viga mista)", "NBR 8800:2024, L.2.3.1", dm.msd, r.mRd);
        c.lines.push_back(fmtn("b_ef = %.0f mm; eta_c = %.3f; f_cd = %.2f MPa; f_yd = %.1f MPa; Ecs = %.0f MPa; alfa_E = %.2f", r.bEff, r.etaC, r.fcd, r.fyd, r.ecs, r.alphaE));
        c.lines.push_back(fmtn("Q_Rd = min(0,5 Acs sqrt(fck Ec)/g = %.1f kN; Rg Rp Acs fu/g = %.1f kN) = %.1f kN (Rg = %.2f, Rp = %.2f)", stud.concreteCrushing / 1e3,
                               stud.studRupture / 1e3, r.qRd / 1e3, r.rg, r.rp));
        c.lines.push_back(fmtn("F_hd = min(Aa fyd = %.0f kN; 0,85 eta_c fcd b tc = %.0f kN) = %.0f kN; sum Q_Rd = %d x %.1f = %.0f kN", tMax / 1e3, cSlabMax / 1e3, r.fhd / 1e3,
                               r.connectorsHalfSpan, r.qRd / 1e3, r.sumQ / 1e3));
        c.lines.push_back(fmtn("M_Rd (interacao %.0f%%) = %.1f kN.m; com interacao completa = %.1f kN.m", r.interaction * 100.0, r.mRd / 1e6, r.mRdFull / 1e6));
        r.checks.push_back(c);
    }
    {
        Check c = minimumRule("Grau de interacao minimo", "NBR 8800:2024, L.2.3.1.1.2", r.interaction, r.interactionMin, "-");
        c.lines.push_back(fmtn("eta_0 = 1 - E/(580 fy) [0,75 - 0,03 Le] = %.3f (Le = %.2f m); c_r = max(0,7; Msd/MRd) = %.2f; c_p = %.2f", eta0, leM, cr, cp));
        c.lines.push_back(fmtn("eta_min = max(c_p c_r^2 eta_0; 0,4 c_r) = %.3f; eta_i = sum Q_Rd / F_hd = %.3f", r.interactionMin, r.interaction));
        r.checks.push_back(c);
    }
    r.vRd = shearResistance(s, section);
    {
        Check c = ratioN("Forca cortante (so o perfil de aco)", "NBR 8800:2024, L.3.1 e 5.4.3", dm.vsd, r.vRd);
        c.lines.push_back(fmtn("V_Rd = %.1f kN (h/tw = %.1f)", r.vRd / 1e3, s.webSlenderness()));
        r.checks.push_back(c);
    }
    // fase de construcao (L.2.3.2)
    r.mRdSteel = steelMomentResistance(s, section, in.lbConstruction);
    if (!in.shored) {
        Check c = ratioNmm("Perfil de aco na fase de construcao", "NBR 8800:2024, L.2.3.2-a e 5.4", dm.mGaSd, r.mRdSteel);
        c.lines.push_back(fmtn("M_a,Rd = %.1f kN.m (Lb = %.0f mm: %s)", r.mRdSteel / 1e6, in.lbConstruction, in.lbConstruction > 0.0 ? "perfil destravado" : "mesa travada pela forma"));
        r.checks.push_back(c);
        const double lambdaW = s.webSlenderness();
        if (lambdaW <= 3.76 * std::sqrt(s.E / s.fy)) {
            Check t = ratioMPa("Tensao na mesa inferior (servico, construcao nao escorada)", "NBR 8800:2024, L.2.3.2-b", dm.mGaS / s.wa() + dm.mLS / r.wEf, s.fy);
            t.lines.push_back(fmtn("sigma = M_Ga,S/Wa + M_L,S/Wef = %.1f + %.1f MPa (Wa = %.0f mm3; Wef = %.0f mm3)", dm.mGaS / s.wa(), dm.mLS / r.wEf, s.wa(), r.wEf));
            r.checks.push_back(t);
        } else if (lambdaW <= 5.70 * std::sqrt(s.E / s.fy)) {
            Check t = ratioMPa("Tensao na mesa inferior (calculo, alma semi-esbelta)", "NBR 8800:2024, L.2.3.2-c", dm.mGaSd / s.wa() + std::max(0.0, dm.msd - dm.mGaSd) / r.wEf, r.fyd);
            r.checks.push_back(t);
        }
    } else {
        r.notes.push_back("Construcao escorada: o perfil de aco nao e verificado isoladamente (L.1.1.2-g: sem solicitacao ate a retirada do escoramento).");
    }
    // disposicoes dos conectores
    const double totalSlab = in.slab.hF + in.slab.tc;
    const bool deck = in.slab.kind != SlabKind::Solid;
    {
        double maxSpacing = 8.0 * totalSlab;
        if (in.slab.kind == SlabKind::DeckPerpendicular) {
            maxSpacing = std::min(maxSpacing, 915.0);
        }
        Check c = rangeMax("Espacamento maximo dos conectores", "NBR 8800:2024, L.4.3.3", in.studs.spacing, maxSpacing, "mm");
        c.lines.push_back(fmtn("s <= 8 x espessura total da laje (%.0f mm)%s", 8.0 * totalSlab, in.slab.kind == SlabKind::DeckPerpendicular ? " e 915 mm (forma perpendicular)" : ""));
        r.checks.push_back(c);
        const double minSpacing = (deck ? 4.0 : 6.0) * in.studs.diameter;
        Check m = rangeMin("Espacamento minimo dos conectores", "NBR 8800:2024, L.4.3.4", in.studs.spacing, minSpacing, "mm");
        m.lines.push_back(fmtn("s >= %.0f d = %.0f mm ao longo do vao%s", deck ? 4.0 : 6.0, minSpacing, deck ? " (laje com forma)" : ""));
        r.checks.push_back(m);
        r.checks.push_back(rangeMax("Diametro do conector", "NBR 8800:2024, L.4.4.1", in.studs.diameter, 2.5 * s.tf, "mm"));
    }
    if (deck) {
        r.checks.push_back(rangeMax("Diametro do conector na laje com forma", "NBR 8800:2024, L.2.6.1-c", in.studs.diameter, 19.0, "mm"));
        r.checks.push_back(rangeMax("Altura da nervura da forma", "NBR 8800:2024, L.2.6.1-a", in.slab.hF, 75.0, "mm"));
        r.checks.push_back(rangeMin("Largura media da nervura sobre o perfil", "NBR 8800:2024, L.2.6.1-b", in.slab.bF, 50.0, "mm"));
        r.checks.push_back(rangeMin("Concreto acima da forma", "NBR 8800:2024, L.2.6.1-e", in.slab.tc, 50.0, "mm"));
        if (in.studs.height > 0.0) {
            r.checks.push_back(rangeMin("Projecao do conector acima da forma", "NBR 8800:2024, L.2.6.1-d", in.studs.height - in.slab.hF, 40.0, "mm"));
        }
        if (in.slab.kind == SlabKind::DeckPerpendicular) {
            r.notes.push_back("Forma perpendicular: ancorar a forma ao perfil a intervalos <= 450 mm (L.2.6.2-b); o concreto abaixo do topo da forma e desprezado.");
        }
    }
    // flecha (L.1.2): fase isolada + longa duracao + curta duracao
    {
        const double L = in.span;
        const double e = s.E;
        const auto homLt = homogenise(s, in.slab, r.bEff, 3.0 * r.alphaE);
        const double ieLt = ia + std::sqrt(r.interaction) * (homLt.itr - ia);
        double qSteel = dm.qBefore, qLong = dm.qPermanent + dm.psi2 * dm.qVariable;
        if (in.shored) {
            qLong += qSteel;
            qSteel = 0.0;
        }
        const double qShort = (1.0 - dm.psi2) * dm.qVariable;
        const double k = 5.0 * std::pow(L, 4.0) / (384.0 * e);
        const double d1 = k * qSteel / ia, d2 = k * qLong / ieLt, d3 = k * qShort / r.iEf;
        r.deflection = d1 + d2 + d3 - dm.camber;
        r.deflectionLimitMm = L / dm.deflectionLimit;
        Check c;
        c.name = "Flecha (servico)";
        c.clause = "NBR 8800:2024, L.1.2 e Anexo B";
        c.demand = r.deflection;
        c.resistance = r.deflectionLimitMm;
        c.unit = "mm";
        c.lines.push_back(fmtn("I_a = %.0f cm4; I_tr(alfa_E) = %.0f cm4; I_tr(3 alfa_E) = %.0f cm4; I_ef = %.0f cm4 (longa duracao: %.0f cm4)", ia / 1e4, hom.itr / 1e4, homLt.itr / 1e4, r.iEf / 1e4, ieLt / 1e4));
        c.lines.push_back(fmtn("d = %.1f (perfil) + %.1f (longa duracao) + %.1f (curta duracao) - %.1f (contraflecha) = %.1f mm; limite L/%.0f = %.1f mm", d1, d2, d3, dm.camber, r.deflection,
                               dm.deflectionLimit, r.deflectionLimitMm));
        r.checks.push_back(c);
    }
    r.notes.push_back("Nao verificados aqui: armadura de costura e cisalhamento longitudinal da laje (L.1.3.4), fissuracao (L.5), pontos de cargas concentradas (n_P, L.4.3.1).");

    r.ok = true;
    for (const auto &c : r.checks) {
        if (c.ratio() > r.maxRatio) {
            r.maxRatio = c.ratio();
            r.governing = c.name;
        }
    }
    return r;
}

double proposeSpacing(const BeamInput &input, const BeamDemand &demand, const DesignSection *section)
{
    const double d = input.studs.diameter;
    const bool deck = input.slab.kind != SlabKind::Solid;
    const double totalSlab = input.slab.hF + input.slab.tc;
    double maxSpacing = 8.0 * totalSlab;
    if (input.slab.kind == SlabKind::DeckPerpendicular) {
        maxSpacing = std::min(maxSpacing, 915.0);
    }
    const double minSpacing = (deck ? 4.0 : 6.0) * d;
    for (double s = std::ceil(minSpacing / 25.0) * 25.0; s <= maxSpacing + 1e-9; s += 25.0) {
        BeamInput trial = input;
        trial.studs.spacing = s;
        const auto result = checkBeam(trial, demand, section);
        bool good = true;
        for (const auto &c : result.checks) {
            if (c.name.find("Momento") == 0 || c.name.find("Grau") == 0) {
                good = good && c.ratio() <= 1.0 + 1e-9;
            }
        }
        if (good) {
            // procura o MAIOR espacamento que ainda atende (menos conectores)
            double best = s;
            for (double t = s + 25.0; t <= maxSpacing + 1e-9; t += 25.0) {
                trial.studs.spacing = t;
                const auto again = checkBeam(trial, demand, section);
                bool ok = true;
                for (const auto &c : again.checks) {
                    if (c.name.find("Momento") == 0 || c.name.find("Grau") == 0) {
                        ok = ok && c.ratio() <= 1.0 + 1e-9;
                    }
                }
                if (!ok) {
                    break;
                }
                best = t;
            }
            return best;
        }
    }
    return 0.0;
}

// ── laje mista com forma de aco incorporada (Anexo N) ────────────────────────

namespace {

struct ContinuityCoefficients {
    double moment;       ///< M = c q L^2
    double deflection;   ///< delta = c q L^4 / (E I)
    double shear;        ///< V = c q L
    double equivalentSpan;   ///< vao simplesmente apoiado equivalente (N.3.1.2.3)
};

ContinuityCoefficients coefficientsFor(int continuity)
{
    if (continuity >= 3) {
        return {0.100, 0.00677, 0.600, 0.8};
    }
    if (continuity == 2) {
        return {0.125, 0.00922, 0.625, 0.9};
    }
    return {0.125, 5.0 / 384.0, 0.500, 1.0};
}

} // namespace

DeckResult checkDeck(const DeckInput &d, const DeckLoads &ld)
{
    DeckResult r;
    const auto cc = coefficientsFor(d.continuity);
    const double fyFd = d.fyF / d.gammaA1;
    const double fcd = d.concrete.fcd();
    const double etaC = d.concrete.etaC();
    const double gammaConcrete = (d.concrete.density + 100.0) * 9.80665 / 1000.0;   // kN/m3
    // espessura equivalente do concreto por metro de laje
    const double equivalentThickness = d.tc + d.hF * d.bb / std::max(1.0, d.bn);   // mm
    r.slabWeight = gammaConcrete * equivalentThickness / 1000.0;   // kN/m2

    const double L = d.spanF;
    const double lMeters = L / 1000.0;
    // ── fase inicial (N.2) ──
    {
        const double gDeck = d.selfWeight;
        const double qEd = ld.gammaG * gDeck + ld.gammaQ * (r.slabWeight + ld.construction);   // kN/m2 (concreto fresco = acao variavel)
        const double qSv = gDeck + r.slabWeight;
        r.mInitial = cc.moment * qEd * lMeters * lMeters * 1e6;   // N.mm por metro
        Check m = ratioNmm("Forma de aco na fase inicial (flexao)", "NBR 8800:2024, N.2.1 e NBR 14762", r.mInitial, d.wEfBottom * fyFd);
        m.lines.push_back(fmtn("q_d = %.1f x %.2f + %.1f x (%.2f + %.2f) = %.2f kN/m2; M_Sd = %.4f q L2 = %.2f kN.m/m; M_Rd = Wef fyd = %.2f kN.m/m", ld.gammaG, gDeck, ld.gammaQ, r.slabWeight, ld.construction,
                               qEd, cc.moment, r.mInitial / 1e6, d.wEfBottom * fyFd / 1e6));
        r.checks.push_back(m);
        const double delta = cc.deflection * qSv * std::pow(L, 4.0) / (d.steelModulus * d.ie);   // q em N/mm por mm de largura: kN/m2 = 1e-3 N/mm2
        const double limit = std::min(L / 180.0, 20.0);
        Check f;
        f.name = "Forma de aco na fase inicial (flecha)";
        f.clause = "NBR 8800:2024, N.2.2";
        f.demand = delta;
        f.resistance = limit;
        f.unit = "mm";
        f.lines.push_back(fmtn("d = %.5f q L4/(E I) = %.2f mm; limite min(L/180; 20 mm) = %.1f mm", cc.deflection, delta, limit));
        r.checks.push_back(f);
        if (delta > L / 250.0) {
            r.notes.push_back(fmtn("Flecha da forma (%.1f mm) acima de L/250: considerar o empoçamento do concreto (acrescimo de 70%% da flecha na espessura, N.4.1.4).", delta));
        }
    }
    // ── fase final (N.3) ──
    const double gFinal = d.selfWeight + r.slabWeight + ld.permanent;
    const double qEdFinal = ld.gammaG * gFinal + ld.gammaQ * ld.variable;   // kN/m2
    const double leq = cc.equivalentSpan * L;
    const double mSd = qEdFinal * std::pow(leq / 1000.0, 2.0) / 8.0 * 1e6;   // N.mm/m (vao simples equivalente)
    {
        // momento positivo (N.3.1.1.2)
        const double b = 1000.0;
        const double htot = d.ht();
        const double dF = htot - d.e;
        const double npa = d.afEf * fyFd;
        const double a = npa / (0.85 * etaC * fcd * b);
        double mRd = 0.0;
        std::string how;
        if (a <= d.tc) {
            mRd = npa * (dF - 0.5 * a);
            how = fmtn("LN acima da forma: a = %.1f mm; M_Rd = Npa (dF - 0,5 a)", a);
        } else {
            const double ncf = 0.85 * b * d.tc * etaC * fcd;
            const double mpa = d.wPlastic * fyFd;
            const double mpr = std::min(mpa, 1.25 * mpa * (1.0 - ncf / npa));
            const double y = htot - 0.5 * d.tc - d.ep + (d.ep - d.e) * ncf / npa;
            mRd = ncf * y + mpr;
            how = fmtn("LN na forma: Ncf = %.0f kN/m; y = %.1f mm; M_pr = %.2f kN.m/m; M_Rd = Ncf y + M_pr", ncf / 1e3, y, mpr / 1e6);
        }
        r.mRdPositive = mRd;
        Check c = ratioNmm("Momento fletor positivo da laje mista (por metro)", "NBR 8800:2024, N.3.1.1", mSd, mRd);
        c.lines.push_back(fmtn("q_d = %.2f x %.2f + %.2f x %.2f = %.2f kN/m2; vao equivalente = %.2f L = %.2f m; M_Sd = %.2f kN.m/m", ld.gammaG, gFinal, ld.gammaQ, ld.variable, qEdFinal, cc.equivalentSpan, leq / 1000.0, mSd / 1e6));
        c.lines.push_back(how + fmtn(" = %.2f kN.m/m (dF = %.1f mm; Npa = %.0f kN/m)", mRd / 1e6, dF, npa / 1e3));
        r.checks.push_back(c);

        // cisalhamento longitudinal (N.3.1.2): metodo m-k
        const double ls = leq / 4.0;
        r.vlRd = b * dF / d.gammaSl * (d.mFactor * d.afEf / (b * ls) + d.kFactor);
        const double vSd = qEdFinal * (leq / 1000.0) / 2.0 * 1e3;   // N/m
        Check v = ratioN("Cisalhamento longitudinal (m-k)", "NBR 8800:2024, N.3.1.2", vSd, r.vlRd);
        v.lines.push_back(fmtn("L_s = L_F/4 = %.0f mm; V_l,Rd = b dF/g [m A_F,ef/(b L_s) + k] = %.1f kN/m; V_Sd = q L/2 = %.1f kN/m", ls, r.vlRd / 1e3, vSd / 1e3));
        r.checks.push_back(v);

        // cisalhamento vertical (N.3.1.3)
        const double av = d.tc * d.bn + d.hF * d.bb;   // area resistente (mm2) por passo de nervura
        const double eta = std::min(1.0, 0.3 + 0.7 * d.concrete.density / 2400.0);
        const double fctd = eta * d.concrete.ctkInf() / d.concrete.gammaC;
        const double tauRd = 0.25 * fctd;
        const double vcRd = 1000.0 * tauRd * 1.0 * 1.2 * av / d.bn;
        const double vMax = 1000.0 * 0.285 * std::sqrt(d.concrete.fck) * av / d.bn;
        r.vvRd = std::min(vMax, d.vvF + vcRd);
        const double vSdVert = qEdFinal * (L / 1000.0) * cc.shear * 1e3;   // N/m
        Check w = ratioN("Cisalhamento vertical da laje", "NBR 8800:2024, N.3.1.3", vSdVert, r.vvRd);
        w.lines.push_back(fmtn("A_v = %.0f mm2; V_c,Rd = 1000 tau_Rd k_v 1,2 A_v/b_n = %.1f kN/m; V_max = %.1f kN/m; V_F,Rd = %.1f kN/m", av, vcRd / 1e3, vMax / 1e3, d.vvF / 1e3));
        r.checks.push_back(w);

        // flecha (N.3.2.2): so acoes variaveis, limite L/350, inercia media entre secao nao fissurada e fissurada
        const double n = d.steelModulus / d.concrete.ecs();
        const double ribFraction = d.bb / std::max(1.0, d.bn);
        const double acTop = b * d.tc;
        const double acRib = b * d.hF * ribFraction;
        const double yTop = d.tc / 2.0, yRib = d.tc + d.hF / 2.0, yDeck = dF;
        const double aF = d.afEf;
        const double atot = acTop / n + acRib / n + aF;
        const double yBar = (acTop / n * yTop + acRib / n * yRib + aF * yDeck) / atot;
        const double iUnc = b * std::pow(d.tc, 3.0) / (12.0 * n) + acTop / n * std::pow(yTop - yBar, 2.0) + b * std::pow(d.hF, 3.0) * ribFraction / (12.0 * n)
            + acRib / n * std::pow(yRib - yBar, 2.0) + d.ie + aF * std::pow(yDeck - yBar, 2.0);
        // fissurada: concreto so acima da LN (largura b), forma tracionada
        const double qa = b / (2.0 * n), qb = aF, qc = -aF * dF;
        const double x = (-qb + std::sqrt(qb * qb - 4.0 * qa * qc)) / (2.0 * qa);
        const double iCr = b * std::pow(x, 3.0) / (3.0 * n) + d.ie + aF * std::pow(dF - x, 2.0);
        r.inertiaEffective = 0.5 * (iUnc + iCr);
        const double deflection = cc.deflection * ld.variable * std::pow(L, 4.0) / (d.steelModulus * r.inertiaEffective);
        Check f;
        f.name = "Flecha da laje mista (sobrecarga)";
        f.clause = "NBR 8800:2024, N.3.2.2";
        f.demand = deflection;
        f.resistance = L / 350.0;
        f.unit = "mm";
        f.lines.push_back(fmtn("n = %.1f; I_nao fissurada = %.0f cm4/m; I_fissurada = %.0f cm4/m; I_ef = media = %.0f cm4/m; d = %.2f mm; limite L/350 = %.1f mm", n, iUnc / 1e4, iCr / 1e4,
                               r.inertiaEffective / 1e4, deflection, L / 350.0));
        r.checks.push_back(f);
    }
    r.checks.push_back(rangeMin("Concreto acima da forma", "NBR 8800:2024, N.5-a", d.tc, 50.0, "mm"));
    r.notes.push_back("Dados da forma (A_F,ef, I, W, m, k) devem vir do fabricante/ensaios (EN 1994-1-1, CSSBI ou ANSI/ASCE 3); confira os valores adotados.");
    r.notes.push_back("Nao verificados: puncao (N.3.1.4), cargas concentradas/lineares (N.6) e fissuracao sobre apoios continuos (N.3.2.1).");
    r.ok = true;
    for (const auto &c : r.checks) {
        if (c.ratio() > r.maxRatio) {
            r.maxRatio = c.ratio();
            r.governing = c.name;
        }
    }
    return r;
}

} // namespace Structura::Design::Composite
