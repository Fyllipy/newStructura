#include "design/BeamExtras.h"

#include "design/Nbr8800.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design {

using Nbr8800::fmt;

namespace {

constexpr double kKn = 1e3;
constexpr double kKnm = 1e6;

/// Envoltoria (maximo em modulo entre as combinacoes) de uma grandeza nas secoes de verificacao.
struct Envelope {
    std::vector<double> x;
    std::vector<double> mx;   // |Mx|
    std::vector<double> vy;   // |Vy|
};

Envelope envelopeOf(const BarDesignInput &in)
{
    Envelope env;
    for (const ComboEffects &combo : in.combos) {
        if (combo.stations.empty()) {
            continue;
        }
        if (env.x.empty()) {
            for (const StationEffects &st : combo.stations) {
                env.x.push_back(st.x);
            }
            env.mx.assign(env.x.size(), 0.0);
            env.vy.assign(env.x.size(), 0.0);
        }
        for (std::size_t i = 0; i < combo.stations.size() && i < env.x.size(); ++i) {
            env.mx[i] = std::max(env.mx[i], std::abs(combo.stations[i].e.Mx));
            env.vy[i] = std::max(env.vy[i], std::abs(combo.stations[i].e.Vy));
        }
    }
    return env;
}

double interpolate(const std::vector<double> &x, const std::vector<double> &y, double at)
{
    if (x.empty()) {
        return 0.0;
    }
    if (at <= x.front()) {
        return y.front();
    }
    if (at >= x.back()) {
        return y.back();
    }
    const auto it = std::upper_bound(x.begin(), x.end(), at);
    const std::size_t i = static_cast<std::size_t>(it - x.begin());
    const double t = (at - x[i - 1]) / std::max(x[i] - x[i - 1], 1e-12);
    return y[i - 1] + t * (y[i] - y[i - 1]);
}

/// Resistencia de calculo (N/mm) de um filete de perna `leg` ligando uma parte de espessura `t`: metal da solda (6.2.5) e metal-base ao cisalhamento.
double filetPerMm(double leg, double fw, double t, double fy, double fu, const BeamExtraContext &c, double *weldMetal = nullptr, double *baseMetal = nullptr)
{
    const double metal = 0.60 * 0.707 * leg * fw / c.gammaW2;
    const double base = std::min(0.60 * t * fy / c.gammaA1, 0.60 * t * fu / c.gammaA2);
    if (weldMetal) {
        *weldMetal = metal;
    }
    if (baseMetal) {
        *baseMetal = base;
    }
    return std::min(metal, base);
}

CheckResult make(const std::string &id, const std::string &title, const std::string &clause, double demand, double resistance, const char *unit, double scale)
{
    CheckResult c;
    c.id = id;
    c.title = title;
    c.clause = clause;
    c.unit = unit;
    c.demand = demand / scale;
    c.resistance = resistance / scale;
    c.ratio = resistance > 0.0 ? demand / resistance : (demand > 0.0 ? 1e9 : 0.0);
    return c;
}

} // namespace

double plasticHingeUnbracedLength(bool boxSection, double e, double fy, double ry, double m1OverM2)
{
    if (boxSection) {
        return std::max((0.17 + 0.10 * m1OverM2) * (e / fy) * ry, 0.10 * (e / fy) * ry);
    }
    return (0.12 + 0.076 * m1OverM2) * (e / fy) * ry;
}

std::vector<CheckResult> beamExtraChecks(const BarDesignInput &in, const BeamExtraContext &ctx)
{
    std::vector<CheckResult> out;
    const DesignSection &s = *in.section;
    const bool iSection = s.kind == SectionKind::I && s.d > 0.0 && s.bf > 0.0 && s.tw > 0.0 && s.tf > 0.0;
    const Envelope env = envelopeOf(in);
    if (env.x.empty() || in.steel.coldFormed) {
        return out;
    }
    const double fy = in.steel.fy, fu = in.steel.fu, E = in.steel.E;

    // ---- 5.4.5: solda mesa-alma de perfil soldado ----------------------------------------------------------------------------------
    if (iSection && in.webWeldLeg > 0.0) {
        const double vMax = *std::max_element(env.vy.begin(), env.vy.end());
        const double q = vMax * (s.bf * s.tf * (s.d - s.tf) / 2.0) / s.g.Ix;   // cisalhamento horizontal da flexao, N/mm (soma dos dois filetes)
        const double w = std::max(0.0, in.flangeLineLoad);
        const double perWeld = std::hypot(q, w) / 2.0;
        double metal = 0.0, base = 0.0;
        const double fq = filetPerMm(in.webWeldLeg, in.webWeldFw, s.tw, fy, fu, ctx, &metal, &base);
        CheckResult c = make("web_flange_weld", "Solda mesa-alma do perfil soldado", ctx.clauseTag + ", 5.4.5", perWeld, fq, "N/mm", 1.0);
        c.lines.push_back(fmt("q = V_Sd Q / Ix = %.2f kN x (bf tf (d - tf)/2 = %.0f mm3) / %.4g mm4 = %.2f N/mm (cisalhamento horizontal total da flexao; V_Sd maximo = %.2f kN)",
                              vMax / kKn, s.bf * s.tf * (s.d - s.tf) / 2.0, s.g.Ix, q, vMax / kKn));
        if (w > 0.0) {
            c.lines.push_back(fmt("Forca aplicada diretamente na mesa: w = %.2f N/mm, transmitida a alma pela solda; resultante por mm das duas soldas = sqrt(q^2 + w^2) = %.2f N/mm", w, std::hypot(q, w)));
        }
        c.lines.push_back(fmt("Por filete (dois filetes): r = %.2f N/mm; resistencia: metal da solda 0,60 (0,707 a) fw / ga_w = 0,60 x 0,707 x %.1f x %.0f / %.2f = %.2f N/mm; metal-base (alma) 0,60 tw min(fy/ga1 ; fu/ga2) = %.2f N/mm",
                              perWeld, in.webWeldLeg, in.webWeldFw, ctx.gammaW2, metal, base));
        out.push_back(std::move(c));
        // perna minima (Tabela 11) pela parte mais fina
        const double thin = std::min(s.tw, s.tf);
        const double minLeg = thin > 19.0 ? 8.0 : (thin > 12.5 ? 6.0 : (thin > 6.3 ? 5.0 : 3.0));
        CheckResult m = make("web_flange_weld_min", "Perna minima do filete mesa-alma", ctx.clauseTag + ", 6.2.6.2.1 / Tab. 11", minLeg, in.webWeldLeg, "mm", 1.0);
        m.lines.push_back(fmt("a = %.1f mm >= perna minima %.1f mm (parte mais fina %.1f mm)", in.webWeldLeg, minLeg, thin));
        out.push_back(std::move(m));
    }

    // ---- 5.4.4: lamelas ---------------------------------------------------------------------------------------------------------------
    if (iSection && in.coverB > 0.0 && in.coverT > 0.0 && in.coverEnd > in.coverStart && in.coverWeldLeg > 0.0 && ctx.mxRdBare > 0.0) {
        const double d = s.d, b = in.coverB, t = in.coverT;
        const double ap = b * t, yTop = d / 2.0 + t / 2.0;
        const bool two = in.coverFlanges != 1;
        double ic, yc = 0.0, wc;
        if (two) {
            ic = s.g.Ix + 2.0 * ap * yTop * yTop + 2.0 * b * t * t * t / 12.0;
            wc = ic / (d / 2.0 + t);
        } else {
            const double atot = s.g.area + ap;
            yc = ap * yTop / atot;
            ic = s.g.Ix + s.g.area * yc * yc + ap * (yTop - yc) * (yTop - yc) + b * t * t * t / 12.0;
            wc = ic / std::max(d / 2.0 + t - yc, d / 2.0 + yc);
        }
        const double armPlate = ap * (yTop - yc) / ic;   // N por N.mm de momento (resultante das tensoes normais na lamela)
        const double fq = filetPerMm(in.coverWeldLeg, in.webWeldFw, std::min(t, s.tf), fy, fu, ctx);
        const double mRd = ctx.mxRdBare;

        // secoes de transicao: onde o momento solicitante iguala o resistente do perfil sem lamela
        double xa = -1.0, xb = -1.0;
        for (std::size_t i = 0; i + 1 < env.x.size(); ++i) {
            const double m0 = env.mx[i], m1 = env.mx[i + 1];
            if ((m0 - mRd) * (m1 - mRd) < 0.0 || (m0 > mRd && i == 0)) {
                const double at = (m0 == m1) ? env.x[i] : env.x[i] + (mRd - m0) / (m1 - m0) * (env.x[i + 1] - env.x[i]);
                if (xa < 0.0) {
                    xa = (m0 > mRd && i == 0) ? env.x[0] : at;
                }
                xb = at;
            }
        }
        if (env.mx.front() > mRd && xa < 0.0) {
            xa = env.x.front();
        }
        if (env.mx.back() > mRd) {
            xb = env.x.back();
        }
        if (xa < 0.0 || xb < 0.0 || xb < xa) {
            CheckResult c = make("cover_plate", "Lamela sobreposta a mesa", ctx.clauseTag + ", 5.4.4", 0.0, 1.0, "", 1.0);
            c.lines.push_back(fmt("O momento solicitante nao supera Mx,Rd do perfil sem lamela (%.2f kN*m) em nenhuma secao: a lamela nao e necessaria.", mRd / kKnm));
            out.push_back(std::move(c));
        } else {
            // (a) 5.4.4.1: prolongamento alem da secao de transicao ligado por solda (ou parafusos de atrito) a forca da lamela no momento da transicao
            const double fTransition = mRd * armPlate;
            const double extLeft = xa - in.coverStart, extRight = in.coverEnd - xb;
            auto extensionCheck = [&](const char *side, double ext) {
                const double weld = (2.0 * std::max(ext, 0.0) + (in.coverEndWeld ? b : 0.0)) * fq;
                CheckResult c = make(std::string("cover_plate_transition_") + side, std::string("Lamela: prolongamento alem da secao de transicao (") + side + ")", ctx.clauseTag + ", 5.4.4.1", fTransition,
                                     ext > 0.0 ? weld : 0.0, "kN", kKn);
                c.lines.push_back(fmt("Secao de transicao (momento solicitante = Mx,Rd do perfil sem lamela = %.2f kN*m) em x = %.0f mm; extremidade da lamela em x = %.0f mm: prolongamento = %.0f mm%s", mRd / kKnm,
                                      side[0] == 'e' ? xa : xb, side[0] == 'e' ? in.coverStart : in.coverEnd, ext, ext <= 0.0 ? " (INSUFICIENTE: a lamela termina antes da secao de transicao)" : ""));
                c.lines.push_back(fmt("Forca na lamela F = M Ap (d/2 + t/2 - yc) / Ic = %.2f kN*m x %.0f mm2 x %.1f mm / %.4g mm4 = %.2f kN (Ic = %.4g mm4 da secao com lamela)", mRd / kKnm, ap, yTop - yc, ic, fTransition / kKn, ic));
                c.lines.push_back(fmt("Solda: 2 filetes de %.1f mm x %.0f mm%s, %.2f N/mm cada: %.2f kN", in.coverWeldLeg, std::max(ext, 0.0), in.coverEndWeld ? " + extremidade" : "", fq, weld / kKn));
                return c;
            };
            out.push_back(extensionCheck("esquerda", extLeft));
            out.push_back(extensionCheck("direita", extRight));
            // (b) 5.4.4.2: solda longitudinal das extremidades, no comprimento a_l
            const double leg75 = in.coverWeldLeg >= 0.75 * t;
            const double factor = in.coverEndWeld ? (leg75 ? 1.0 : 1.5) : 2.0;
            const double al = factor * b;
            auto endCheck = [&](const char *side, double xEnd, double dir) {
                const double xs = xEnd + dir * al;
                const double mAl = interpolate(env.x, env.mx, xs);
                const double force = mAl * armPlate;
                const double weld = (2.0 * al + (in.coverEndWeld ? b : 0.0)) * fq;
                CheckResult c = make(std::string("cover_plate_end_") + side, std::string("Lamela: solda da extremidade no comprimento a_l (") + side + ")", ctx.clauseTag + ", 5.4.4.2", force, weld, "kN", kKn);
                c.lines.push_back(fmt("a_l = %.1f b = %.1f x %.0f = %.0f mm (%s)", factor, factor, b,
                                      al, in.coverEndWeld ? (leg75 ? "filete continuo com perna >= 0,75 t ao longo das bordas e da extremidade" : "filete continuo com perna < 0,75 t") : "sem solda na extremidade, so filetes longitudinais"));
                c.lines.push_back(fmt("Momento solicitante a a_l da extremidade (x = %.0f mm): %.2f kN*m; F = M Ap (...)/Ic = %.2f kN; solda de %.0f mm nas duas bordas%s = %.2f kN", xs, mAl / kKnm, force / kKn, al,
                                      in.coverEndWeld ? " e da extremidade" : "", weld / kKn));
                return c;
            };
            out.push_back(endCheck("esquerda", in.coverStart, +1.0));
            out.push_back(endCheck("direita", in.coverEnd, -1.0));
        }
        // flexao na regiao com lamela: elastica (secao composta soldada), conservadora
        double mPlated = 0.0;
        for (std::size_t i = 0; i < env.x.size(); ++i) {
            if (env.x[i] >= in.coverStart && env.x[i] <= in.coverEnd) {
                mPlated = std::max(mPlated, env.mx[i]);
            }
        }
        if (mPlated > 0.0) {
            const double mRdPlated = wc * fy / ctx.gammaA1;
            CheckResult c = make("cover_plate_flexure", "Flexao da secao com lamela", ctx.clauseTag + ", 5.4.2 (elastica)", mPlated, mRdPlated, "kN*m", kKnm);
            c.lines.push_back(fmt("Secao com lamela %.0f x %.1f mm (%s): Ic = %.4g mm4, Wc = %.0f mm3; Mx,Rd = Wc fy / ga1 = %.0f x %.0f / %.2f = %.2f kN*m (elastico: contencao lateral e classificacao nao consideradas); M_Sd = %.2f kN*m",
                                  b, t, two ? "duas mesas" : "mesa superior", ic, wc, wc, fy, ctx.gammaA1, mRdPlated / kKnm, mPlated / kKnm));
            out.push_back(std::move(c));
        }
    }

    // ---- 4.11.5: comprimento destravado entre rotulas plasticas ----------------------------------------------------------------------
    const bool boxSection = s.kind == SectionKind::RectTube || s.kind == SectionKind::SolidRect;
    if (in.plasticAnalysis && ((iSection && s.doublySymmetric()) || boxSection)) {
        const double ry = s.g.ry;
        const double lb = ctx.lb > 0.0 ? ctx.lb : ctx.length / std::max(1, ctx.segments);
        double worstRatio = 0.0, worstLpd = 0.0, worstRatioM = 0.0;
        int worstSegment = 0;
        for (const ComboEffects &combo : in.combos) {
            if (combo.stations.size() < 2) {
                continue;
            }
            std::vector<double> xs, ms;
            for (const StationEffects &st : combo.stations) {
                xs.push_back(st.x);
                ms.push_back(st.e.Mx);
            }
            for (int k = 0; k < std::max(1, ctx.segments); ++k) {
                const double xA = ctx.length * k / std::max(1, ctx.segments), xB = ctx.length * (k + 1) / std::max(1, ctx.segments);
                const double mA = interpolate(xs, ms, xA), mB = interpolate(xs, ms, xB);
                const double big = std::max(std::abs(mA), std::abs(mB)), small = std::min(std::abs(mA), std::abs(mB));
                if (big < 1e-6) {
                    continue;
                }
                // curvatura reversa (momentos de sinais opostos): positiva; simples: negativa
                const double ratioM = (mA * mB < 0.0 ? 1.0 : -1.0) * small / big;
                const double lpd = plasticHingeUnbracedLength(boxSection, E, fy, ry, ratioM);
                const double ratio = lpd > 0.0 ? lb / lpd : 1e9;
                if (ratio > worstRatio) {
                    worstRatio = ratio;
                    worstLpd = lpd;
                    worstRatioM = ratioM;
                    worstSegment = k + 1;
                }
            }
        }
        if (worstRatio > 0.0) {
            CheckResult c = make("plastic_hinge", "Comprimento destravado entre rotulas plasticas", ctx.clauseTag + ", 4.11.5", lb, worstLpd, "mm", 1.0);
            c.lines.push_back(boxSection ? fmt("Lpd = [0,17 + 0,10 M1/M2] (E/fy) ry >= 0,10 (E/fy) ry = [0,17 + 0,10 (%.3f)] x (%.0f/%.0f) x %.1f = %.0f mm (alinea b)", worstRatioM, E, fy, ry, worstLpd)
                                         : fmt("Lpd = [0,12 + 0,076 M1/M2] (E/fy) ry = [0,12 + 0,076 (%.3f)] x (%.0f/%.0f) x %.1f = %.0f mm (alinea a; secao I duplamente simetrica)", worstRatioM, E, fy, ry, worstLpd));
            c.lines.push_back(fmt("M1/M2 = relacao entre o menor e o maior dos momentos solicitantes nas extremidades do trecho %d (positiva em curvatura reversa); trecho destravado Lb = %.0f mm", worstSegment, lb));
            c.lines.push_back("4.11.3/4.11.4: a secao deve ser compacta (FLM e FLA), simetrica em relacao ao plano da alma, e as ligacoes nas rotulas com resistencia >= 120 % da viga; nao ha limite para flexao em torno do eixo de menor inercia.");
            if (iSection) {
                const double lambdaF = (s.bf / 2.0) / s.tf, lambdaW = s.h / s.tw;
                const double limF = 0.38 * std::sqrt(E / fy), limW = 3.76 * std::sqrt(E / fy);
                c.lines.push_back(fmt("Compacidade (Tabela D.1): mesa b/t = %.1f %s 0,38 sqrt(E/fy) = %.1f; alma h/tw = %.1f %s 3,76 sqrt(E/fy) = %.1f", lambdaF, lambdaF <= limF ? "<=" : ">", limF, lambdaW,
                                      lambdaW <= limW ? "<=" : ">", limW));
                if (lambdaF > limF || lambdaW > limW) {
                    c.ratio = std::max(c.ratio, 1.0 + 1e-6);   // secao nao compacta: sem capacidade de rotacao (4.11.4)
                }
            }
            out.push_back(std::move(c));
        }
    }
    return out;
}

} // namespace Structura::Design
