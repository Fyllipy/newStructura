#include "design/LatticedColumn.h"

#include "design/Nbr8800.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace Structura::Design {

using Nbr8800::fmt;

namespace {

constexpr double kKn = 1e3;

struct Best {
    CheckResult check;
    std::string combo;
    double x {0.0};
};

/// Guarda a pior ocorrencia de cada verificacao, na ordem de insercao.
class Keeper
{
public:
    void offer(CheckResult c, const std::string &combo, double x)
    {
        const std::string id = c.id;
        auto it = m_best.find(id);
        if (it == m_best.end()) {
            m_order.push_back(id);
            m_best[id] = {std::move(c), combo, x};
        } else if (c.ratio > it->second.check.ratio) {
            it->second = {std::move(c), combo, x};
        }
    }
    const std::vector<std::string> &order() const { return m_order; }
    const Best &best(const std::string &id) const { return m_best.at(id); }

private:
    std::vector<std::string> m_order;
    std::map<std::string, Best> m_best;
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

LatticeGeometry latticeGeometry(const DesignSection &section, const DesignSection &chord)
{
    LatticeGeometry g;
    g.chordArea = chord.g.area;
    // as inercias proprias do banzo em relacao a eixos paralelos aos da secao: para a cantoneira de abas iguais Ix' = Iy' = (Ix + Iy)/2 em qualquer par de eixos ortogonais
    g.chordIx = g.chordIy = 0.5 * (chord.g.Ix + chord.g.Iy);
    g.chordRMin = chord.rMin > 0.0 ? chord.rMin : std::sqrt(std::min(chord.g.Ix, chord.g.Iy) / std::max(chord.g.area, 1e-9));
    const double b = chord.leg > 0.0 ? chord.leg : section.leg;
    const double t = chord.t > 0.0 ? chord.t : section.t;
    // distancia do centroide ao dorso: da propria cantoneira (inclui os raios); formula de cantos vivos se a geometria nao existir
    const double yBar = chord.g.valid && chord.g.aMin < 0.0 ? -chord.g.aMin : (b * b + b * t - t * t) / (2.0 * (2.0 * b - t));
    g.h0x = section.bf - 2.0 * yBar;
    g.h0y = section.d - 2.0 * yBar;
    return g;
}

LatticeAxisResult latticeAxis(const LatticeGeometry &g, const LatticeSystem &sys, double diagonalArea, double E, double effectiveLength, bool bendingAboutX)
{
    LatticeAxisResult r;
    r.h0 = bendingAboutX ? g.h0y : g.h0x;
    const double ich = bendingAboutX ? g.chordIx : g.chordIy;
    r.panel = sys.panel > 0.0 ? sys.panel : std::max(g.h0x, g.h0y);
    const double a = r.panel;
    const double h0 = r.h0;
    const double i1 = g.chordArea * h0 * h0 + 4.0 * ich;   // 4 banzos a +-h0/2 com as proprias inercias
    if (sys.type == 2) {
        // presilhas: mu da EN 1993-1-1 6.4.3.1 (1 para esbeltez i0 <= 75; 0 acima de 150)
        const double i0 = std::sqrt(i1 / (4.0 * g.chordArea));
        const double lambda = effectiveLength / i0;
        r.mu = lambda <= 75.0 ? 1.0 : (lambda >= 150.0 ? 0.0 : 2.0 - lambda / 75.0);
        const double ib = sys.battenThickness * std::pow(sys.battenDepth, 3) / 12.0;
        const double n = 2.0;   // planos de presilhas
        const double sv = 24.0 * E * ich / (a * a * (1.0 + 2.0 * ich / (n * ib) * h0 / a));
        r.shearStiffness = std::min(sv, 2.0 * kPi * kPi * E * ich / (a * a));
    } else {
        r.diagonalLength = std::sqrt(a * a + h0 * h0);
        const double d = r.diagonalLength;
        const double planes = 2.0;
        const double single = planes * E * diagonalArea * a * h0 * h0 / (d * d * d);
        r.shearStiffness = sys.type == 1 ? 2.0 * single : single;
    }
    r.inertiaEff = g.chordArea * h0 * h0 + 4.0 * r.mu * ich;
    r.euler = kPi * kPi * E * r.inertiaEff / (effectiveLength * effectiveLength);
    r.Ne = r.shearStiffness > 0.0 ? r.euler / (1.0 + r.euler / r.shearStiffness) : r.euler;
    return r;
}

BarDesignOutput designLatticedColumn(const BarDesignInput &in, const LatticeSystem &sys)
{
    BarDesignOutput out;
    if (in.section == nullptr || in.latticeChord == nullptr) {
        out.warnings.push_back("Pilar de quatro cantoneiras sem a cantoneira dos banzos.");
        return out;
    }
    const DesignSection &s = *in.section;
    const DesignSection &chord = *in.latticeChord;
    if (!s.supported() || !chord.supported() || s.bf <= 0.0 || s.d <= 0.0) {
        out.warnings.push_back("Pilar de quatro cantoneiras: dimensoes invalidas.");
        return out;
    }
    const LatticeGeometry g = latticeGeometry(s, chord);
    if (g.h0x <= 0.0 || g.h0y <= 0.0) {
        out.warnings.push_back("Pilar de quatro cantoneiras: as cantoneiras se sobrepoem (afastamento entre banzos nulo).");
        return out;
    }
    out.supported = true;
    out.standard = "ABNT NBR 8800:2024";
    const std::string tag = "NBR 8800:2024";
    const double L = in.length;
    const double Lx = in.lxOverride > 0.0 ? in.lxOverride : in.Kx * L;
    const double Ly = in.lyOverride > 0.0 ? in.lyOverride : in.Ky * L;
    const double E = in.steel.E, fy = in.steel.fy;
    const double gammaA1 = in.opt8800.gammaA1;
    const double diagArea = (sys.type != 2 && in.latticeDiagonal != nullptr) ? in.latticeDiagonal->g.area : 0.0;
    if (sys.type != 2 && diagArea <= 0.0) {
        out.warnings.push_back("Trelica sem a cantoneira das diagonais: informe o perfil das diagonais.");
        return out;
    }
    const LatticeAxisResult ax = latticeAxis(g, sys, diagArea, E, Lx, true);
    const LatticeAxisResult ay = latticeAxis(g, sys, diagArea, E, Ly, false);
    const double area = s.g.area;
    const char *systemName = sys.type == 0 ? "trelica simples (zigue-zague)" : (sys.type == 1 ? "trelica dupla em X" : "presilhas");
    out.summaryLines.push_back(fmt("Pilar de quatro cantoneiras (%s): banzo A = %.0f mm2, I proprio = %.0f mm4, rmin = %.1f mm; h0x = %.1f mm, h0y = %.1f mm; painel a = %.0f mm", systemName, g.chordArea, g.chordIx, g.chordRMin,
                                   g.h0x, g.h0y, ax.panel));
    out.summaryLines.push_back(fmt("Procedimento aceito pela comunidade tecnico-cientifica (NBR 8800:2024, 5.3.6.4): rigidez ao cisalhamento da ligacao (Timoshenko; EN 1993-1-1, 6.4), imperfeicao equivalente e0 = L/500, forca nos banzos "
                                   "N_ch = N/4 +- M h0 A/(2 Ief)"));
    out.summaryLines.push_back(fmt("Eixo X (flexao em torno de X): Lx = %.0f mm, Ief = %.4g mm4, Sv = %.1f kN, Ne,0 = %.1f kN, Ne,m = %.1f kN%s", Lx, ax.inertiaEff, ax.shearStiffness / kKn, ax.euler / kKn, ax.Ne / kKn,
                                   sys.type == 2 ? fmt(" (mu = %.2f)", ax.mu).c_str() : ""));
    out.summaryLines.push_back(fmt("Eixo Y (flexao em torno de Y): Ly = %.0f mm, Ief = %.4g mm4, Sv = %.1f kN, Ne,0 = %.1f kN, Ne,m = %.1f kN%s", Ly, ay.inertiaEff, ay.shearStiffness / kKn, ay.euler / kKn, ay.Ne / kKn,
                                   sys.type == 2 ? fmt(" (mu = %.2f)", ay.mu).c_str() : ""));

    // resistencias das cantoneiras (banzo e diagonais): compressao como barra entre pontos de ligacao, K = 1
    Nbr8800::Buckling chordLen;
    chordLen.Lx = chordLen.Ly = chordLen.Lz = ax.panel;
    const Nbr8800::CompressionData chordComp = Nbr8800::compression(chord, in.steel, chordLen, in.opt8800);
    const double chordNcRd = chordComp.NcRd;
    const double chordNtRd = chord.g.area * fy / gammaA1;
    double diagNcRdX = 0.0, diagNcRdY = 0.0, diagNtRd = 0.0;
    if (sys.type != 2) {
        Nbr8800::Buckling lx, ly;
        lx.Lx = lx.Ly = lx.Lz = ax.diagonalLength;
        ly.Lx = ly.Ly = ly.Lz = ay.diagonalLength;
        diagNcRdX = Nbr8800::compression(*in.latticeDiagonal, in.steel, lx, in.opt8800).NcRd;
        diagNcRdY = Nbr8800::compression(*in.latticeDiagonal, in.steel, ly, in.opt8800).NcRd;
        diagNtRd = in.latticeDiagonal->g.area * fy / gammaA1;
    }
    Keeper keeper;

    // ---- limites de 5.3.6.1 e 5.3.7 ------------------------------------------------------------------------------------------------
    {
        const double rx = std::sqrt(ax.inertiaEff / area), ry = std::sqrt(ay.inertiaEff / area);
        const double slender0 = std::max(Lx / rx, Ly / ry);
        const double chordSlender = ax.panel / g.chordRMin;
        const double limit = 0.75 * slender0;
        std::vector<std::string> lines;
        lines.push_back(fmt("5.3.6.1: l/rmin = %.0f / %.1f = %.1f <= 0,75 (L/r)0 = 0,75 x %.1f = %.1f  [(L/r)0 = maior entre Lx/rx e Ly/ry com r = sqrt(Ief/A) = (%.1f ; %.1f) mm]", ax.panel, g.chordRMin, chordSlender, slender0, limit, rx, ry));
        const int panels = static_cast<int>(std::ceil(std::max(L / ax.panel, 1.0)));
        lines.push_back(fmt("5.3.6.1: pelo menos duas ligacoes entre os perfis ao longo do comprimento, uniformemente espacadas: L/a = %.1f paineis", L / ax.panel));
        const double thetaDeg = std::atan2(std::min(g.h0x, g.h0y), ax.panel) * 180.0 / kPi;
        if (sys.type != 2) {
            lines.push_back(fmt("Inclinacao da diagonal na face menor: %.1f graus (recomendado entre 30 e 60 graus)", thetaDeg));
        }
        CheckResult c = makeCheck("lat_limits", "Pilar de quatro cantoneiras: limites de esbeltez dos banzos", tag + ", 5.3.6.1", chordSlender, limit, "", 1.0, lines);
        if (panels < 3) {
            c.ratio = std::max(c.ratio, 1.0 + 1e-6);
        }
        keeper.offer(std::move(c), "-", 0.0);
        const double equivalent = kPi * std::sqrt(E * area / std::min(ax.Ne, ay.Ne));
        keeper.offer(makeCheck("lat_slenderness", "Pilar de quatro cantoneiras: esbeltez da barra como unidade", tag + ", 5.3.7.1", equivalent, 200.0, "", 1.0,
                               {fmt("lambda_eq = pi sqrt(E A / Ne,m,min) = %.1f <= 200 (recomendacao)", equivalent)}),
                     "-", 0.0);
    }

    for (const ComboEffects &combo : in.combos) {
        for (const StationEffects &st : combo.stations) {
            const Effects &e = st.e;
            const double nSd = std::max(0.0, -e.N);
            const double tSd = std::max(0.0, e.N);
            auto where = [&]() { return fmt("combinacao %s, x = %.0f mm", combo.name.c_str(), st.x); };
            // tracao axial
            if (tSd > 0.0) {
                const double rd = area * fy / gammaA1;
                keeper.offer(makeCheck("tension", "Tracao axial do pilar de quatro cantoneiras", tag + ", 5.2.2", tSd, rd, "kN", kKn,
                                       {fmt("Nt,Rd = A fy / ga1 = %.0f x %.0f / %.2f = %.1f kN; Nt,Sd = %.1f kN  (%s)", area, fy, gammaA1, rd / kKn, tSd / kKn, where().c_str())}),
                             combo.name, st.x);
            }
            // flambagem global (sem momentos): chi com Ne,m do eixo mais fraco
            if (nSd > 0.0) {
                const double neMin = std::min(ax.Ne, ay.Ne);
                const double lambda0 = std::sqrt(area * fy / neMin);
                const double chi = lambda0 <= 1.5 ? std::pow(0.658, lambda0 * lambda0) : 0.877 / (lambda0 * lambda0);
                const double rd = chi * area * fy / gammaA1;
                keeper.offer(makeCheck("lat_global", "Pilar de quatro cantoneiras: flambagem global (Ne,m)", tag + ", 5.3.3 e 5.3.6.4", nSd, rd, "kN", kKn,
                                       {fmt("Ne,m,min = %.1f kN (X: %.1f; Y: %.1f); lambda0 = sqrt(A fy/Ne,m) = %.3f; chi = %.3f; Nc,Rd = chi A fy/ga1 = %.1f kN; Nc,Sd = %.1f kN  (%s)", neMin / kKn, ax.Ne / kKn, ay.Ne / kKn,
                                            lambda0, chi, rd / kKn, nSd / kKn, where().c_str())}),
                             combo.name, st.x);
            }
            // momentos de segunda ordem com imperfeicao L/500 (so ha sentido na compressao)
            auto amplified = [&](double moment, double ne, double length) {
                const double first = std::abs(moment) + nSd * length / 500.0;
                if (nSd <= 0.0) {
                    return std::abs(moment);
                }
                return nSd >= ne ? 1e12 : first / (1.0 - nSd / ne);
            };
            const double mx = amplified(e.Mx, ax.Ne, Lx);
            const double my = amplified(e.My, ay.Ne, Ly);
            const double termX = mx * (g.h0y / 2.0) * g.chordArea / ax.inertiaEff;   // contribuicao do momento em torno de X
            const double termY = my * (g.h0x / 2.0) * g.chordArea / ay.inertiaEff;
            const double axial = -e.N / 4.0;   // compressao positiva
            const double chordMax = axial + termX + termY;
            const double chordMin = axial - termX - termY;
            if (chordMax > 0.0) {
                keeper.offer(makeCheck("lat_chord", "Pilar de quatro cantoneiras: banzo comprimido", tag + ", 5.3 e 5.3.6.4", chordMax, chordNcRd, "kN", kKn,
                                       {fmt("N_ch,Ed = N/4 + Mx,tot (h0y/2) A/Ief,x + My,tot (h0x/2) A/Ief,y = %.1f + %.1f + %.1f = %.1f kN", axial / kKn, termX / kKn, termY / kKn, chordMax / kKn),
                                        fmt("Momentos de 2a ordem com e0 = L/500: Mx,tot = (Mx + N e0)/(1 - N/Ne,m) = %.2f kN*m; My,tot = %.2f kN*m", mx / 1e6, my / 1e6),
                                        fmt("N_c,Rd do banzo (cantoneira entre ligacoes, K L = %.0f mm, 5.3): %.1f kN  (%s)", ax.panel, chordNcRd / kKn, where().c_str())}),
                             combo.name, st.x);
            }
            if (chordMin < 0.0) {
                keeper.offer(makeCheck("lat_chord_tension", "Pilar de quatro cantoneiras: banzo tracionado", tag + ", 5.2", -chordMin, chordNtRd, "kN", kKn,
                                       {fmt("Banzo mais tracionado: %.1f kN; Nt,Rd = A fy/ga1 = %.1f kN  (%s)", -chordMin / kKn, chordNtRd / kKn, where().c_str())}),
                             combo.name, st.x);
            }
            // cortante da ligacao: V = V aplicado + pi M / L (EN 1993-1-1, 6.4.1)
            const double vx = std::abs(e.Vy) + kPi * mx / Lx;   // cortante na direcao Y (flexao em torno de X): faces paralelas a Y, h0 = h0y
            const double vy = std::abs(e.Vz) + kPi * my / Ly;
            for (const bool aboutX : {true, false}) {
                const LatticeAxisResult &r = aboutX ? ax : ay;
                const double v = aboutX ? vx : vy;
                if (v <= 0.0) {
                    continue;
                }
                const char *axisName = aboutX ? "X" : "Y";
                if (sys.type != 2) {
                    const double perDiagonal = v * r.diagonalLength / (2.0 * r.h0) / (sys.type == 1 ? 2.0 : 1.0);   // 2 planos; em X cada diagonal leva a metade
                    const double nc = aboutX ? diagNcRdX : diagNcRdY;
                    const double governing = std::min(nc, diagNtRd);
                    keeper.offer(makeCheck("lat_diagonal", "Pilar de quatro cantoneiras: diagonais", tag + ", 5.2 e 5.3", perDiagonal, governing, "kN", kKn,
                                           {fmt("Eixo %s: V_Ed = V + pi M/L = %.1f kN; F_d = V_Ed d / (2 h0)%s = %.1f x %.0f / (2 x %.1f)%s = %.1f kN (d = %.0f mm)", axisName, v / kKn, sys.type == 1 ? " / 2" : "", v / kKn, r.diagonalLength, r.h0,
                                                 sys.type == 1 ? " / 2" : "", perDiagonal / kKn, r.diagonalLength),
                                            fmt("Resistencias da diagonal: compressao (K L = %.0f mm) = %.1f kN; tracao A fy/ga1 = %.1f kN -> %.1f kN  (%s)", r.diagonalLength, nc / kKn, diagNtRd / kKn, governing / kKn, where().c_str())}),
                                 combo.name, st.x);
                } else {
                    const double bendingMoment = v * r.panel / 4.0;                 // V a / (2 n), n = 2 planos
                    const double shear = v * r.panel / (2.0 * r.h0);                // V a / (n h0)
                    const double w = sys.battenThickness * sys.battenDepth * sys.battenDepth / 6.0;
                    const double mRd = w * fy / gammaA1;
                    const double vRd = 0.60 * fy * sys.battenDepth * sys.battenThickness / gammaA1;
                    const double ratioM = mRd > 0.0 ? bendingMoment / mRd : 1e9;
                    const double ratioV = vRd > 0.0 ? shear / vRd : 1e9;
                    const bool momentGoverns = ratioM >= ratioV;
                    keeper.offer(makeCheck("lat_batten", "Pilar de quatro cantoneiras: presilhas", tag + ", 5.4", momentGoverns ? bendingMoment : shear, momentGoverns ? mRd : vRd, momentGoverns ? "kN*m" : "kN",
                                           momentGoverns ? 1e6 : kKn,
                                           {fmt("Eixo %s: V_Ed = %.1f kN; M_b = V_Ed a/(2 n) = %.2f kN*m; V_b = V_Ed a/(n h0) = %.1f kN (n = 2 planos)", axisName, v / kKn, bendingMoment / 1e6, shear / kKn),
                                            fmt("Presilha %.0f x %.1f mm: M_Rd = W fy/ga1 = %.2f kN*m; V_Rd = 0,60 fy hb tb/ga1 = %.1f kN; razoes %.3f (flexao) e %.3f (cortante)  (%s)", sys.battenDepth, sys.battenThickness, mRd / 1e6, vRd / kKn,
                                                 ratioM, ratioV, where().c_str()),
                                            "A ligacao das presilhas aos banzos (soldas ou parafusos) deve resistir a M_b e V_b (nao verificada aqui: use Ligacoes)."}),
                                 combo.name, st.x);
                }
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
    if (in.combos.empty()) {
        out.warnings.push_back("Sem esforcos nas combinacoes selecionadas: execute a analise e selecione combinacoes ultimas.");
    }
    return out;
}

} // namespace Structura::Design
