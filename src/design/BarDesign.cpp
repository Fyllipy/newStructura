#include "design/BarDesign.h"

#include "design/BeamExtras.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <tuple>

namespace Structura::Design {

using Nbr8800::fmt;

namespace {

double sq(double v) { return v * v; }

struct Candidate {
    CheckResult check;
    std::string combo;
    double x {0.0};
};

class Collector
{
public:
    /// A verificacao `id` com esse aproveitamento seria guardada por `offer`? (permite montar o memorial so quando necessario)
    [[nodiscard]] bool wants(const std::string &id, double ratio) const
    {
        const auto it = m_best.find(id);
        return it == m_best.end() || ratio > it->second.check.ratio;
    }
    void offer(CheckResult check, const std::string &combo, double x)
    {
        auto it = m_best.find(check.id);
        if (it == m_best.end()) {
            m_order.push_back(check.id);
            const std::string key = check.id;
            m_best[key] = Candidate {std::move(check), combo, x};
        } else if (check.ratio > it->second.check.ratio) {
            it->second = Candidate {std::move(check), combo, x};
        }
    }
    [[nodiscard]] const std::map<std::string, Candidate> &best() const { return m_best; }
    [[nodiscard]] const std::vector<std::string> &order() const { return m_order; }

private:
    std::map<std::string, Candidate> m_best;
    std::vector<std::string> m_order;
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

/// Aproveitamento demanda/resistencia (resistencia nula: 1e9 se ha demanda).
double ratioOf(double demand, double resistance)
{
    return resistance > 0.0 ? demand / resistance : (demand > 0.0 ? 1e9 : 0.0);
}

CheckResult makeCheck(const std::string &id, const std::string &title, const std::string &clause, double demand,
                      double resistance, const char *unit, double demandScale, std::vector<std::string> lines)
{
    CheckResult c;
    c.id = id;
    c.title = title;
    c.clause = clause;
    c.unit = unit;
    c.demand = demand / demandScale;
    c.resistance = resistance / demandScale;
    c.ratio = ratioOf(demand, resistance);
    c.lines = std::move(lines);
    return c;
}

constexpr double kKn = 1e3;
constexpr double kKnm = 1e6;

} // namespace

BarDesignOutput designBar(const BarDesignInput &in)
{
    BarDesignOutput out;
    if (!in.section || !in.section->supported() || in.length <= 0.0) {
        out.warnings.push_back("Secao nao suportada pelo dimensionamento (use perfis de aco do catalogo).");
        return out;
    }
    const DesignSection &s = *in.section;
    out.supported = true;
    const bool cold = (s.standard == Standard::NBR14762);
    out.standard = cold ? "ABNT NBR 14762:2010" : "ABNT NBR 8800:2024";
    const std::string clauseTag = cold ? "NBR 14762:2010" : "NBR 8800:2024";

    const double L = in.length;
    const double E = in.steel.E;

    Nbr8800::Options o8 = in.opt8800;
    o8.netAreaRatio = in.netAreaRatio;
    o8.stiffenerSpacing = in.stiffenerSpacing;
    o8.ct = in.ct;
    o8.compositeSpacing = in.compositeSpacing;
    o8.compositeWelded = in.compositeWelded;
    o8.barLength = in.length;
    o8.threadedRod = in.threadedEnds;
    o8.slendernessSection = in.slendernessSection;
    o8.ixOverride = in.ixEquivalent;
    Nbr14762::Options o14 = in.opt14762;
    o14.netAreaRatio = in.netAreaRatio;
    o14.panelTensionFlange = in.panelTensionFlange;
    o14.coldWork = in.coldWork;
    o14.ct = in.ct;

    const Nbr8800::Buckling len {in.lxOverride > 0.0 ? in.lxOverride : in.Kx * L,
                                 in.lyOverride > 0.0 ? in.lyOverride : in.Ky * L,
                                 in.lzOverride > 0.0 ? in.lzOverride : in.Kz * L};
    const int segments = std::max(1, in.lateralSegments);
    const double lb = in.compressionFlangeBraced ? 0.0 : (in.lbOverride > 0.0 ? in.lbOverride : L / segments);

    out.summaryLines.push_back(fmt("Perfil: %s (%s), A = %.0f mm2, Ix = %.3e mm4, Iy = %.3e mm4, peso = %.2f kg/m",
                                   s.name.c_str(), kindName(s.kind).c_str(), s.g.area, s.g.Ix, s.g.Iy, s.weightPerMeter()));
    out.summaryLines.push_back(fmt("Aco: %s (fy = %.0f MPa, fu = %.0f MPa, E = %.0f MPa)", in.steel.name.c_str(), in.steel.fy, in.steel.fu, E));
    for (const std::string &note : in.tensionNotes) {
        out.summaryLines.push_back(note);
    }
    if (!cold && (s.kind == SectionKind::I || s.kind == SectionKind::Channel) && (in.torsionReleasedStart || in.torsionReleasedEnd)) {
        out.warnings.push_back(fmt("NBR 8800, 5.7.8: extremidade %s sem restricao a rotacao em relacao ao eixo longitudinal: exigem-se enrijecedores transversais no apoio (soldados as mesas e a alma, em toda a altura), "
                                   "salvo se a alma estiver ligada a outras vigas ou pilares.", in.torsionReleasedStart && in.torsionReleasedEnd ? "inicial e final" : (in.torsionReleasedStart ? "inicial" : "final")));
    }
    out.summaryLines.push_back(fmt("Comprimento L = %.0f mm; Lx = %.0f mm, Ly = %.0f mm, Lz = %.0f mm; Lb = %.0f mm (%d trecho(s))", L, len.Lx, len.Ly, len.Lz, lb,
                                   segments));

    // ---- Resistencias independentes dos esforcos ----
    // Anexo G: barras de secao variavel usam a secao de menor altura na tracao (G.2) e na compressao (G.3)
    const DesignSection &sAxial = in.axialSection != nullptr ? *in.axialSection : s;
    Nbr8800::TensionData tens = cold ? Nbr14762::tension(s, in.steel, o14) : Nbr8800::tension(sAxial, in.steel, o8);
    Nbr8800::CompressionData comp = cold ? Nbr14762::compression(s, in.steel, len, o14) : Nbr8800::compression(sAxial, in.steel, len, o8);

    const double rMin = std::min(s.g.rx, s.g.ry);
    const double lambdaTension = L / rMin;

    Collector col;

    // Barras compostas (5.3.6.1, 5.3.6.2 e 5.2.8.2)
    if (!cold && s.composite() && s.rMin > 0.0) {
        bool anyCompression = false, anyTension = false;
        for (const ComboEffects &combo : in.combos) {
            for (const StationEffects &st : combo.stations) {
                anyCompression = anyCompression || st.e.N < -1e-9;
                anyTension = anyTension || st.e.N > 1e-9;
            }
        }
        const double spacing = in.compositeSpacing > 0.0 ? in.compositeSpacing : L / 3.0;
        const double perMin = spacing / s.rMin;
        const int intermediate = static_cast<int>(std::ceil(L / spacing - 1e-9)) - 1;
        if (anyCompression) {
            const double slender0 = comp.slenderness;   // (L/r)0: maior indice de esbeltez da barra composta atuando como unidade
            CheckResult c;
            c.id = "composite";
            c.title = "Barra composta: ligacoes entre os perfis";
            c.clause = clauseTag + ", 5.3.6.1";
            c.demand = perMin;
            c.resistance = 0.75 * slender0;
            c.ratio = c.resistance > 0.0 ? perMin / c.resistance : 0.0;
            const bool enough = intermediate >= 2;
            if (!enough) {
                c.ratio = std::max(c.ratio, 1.0 + 1e-6);
            }
            c.lines.push_back(fmt("l = %.0f mm (%s), rmin do perfil componente = %.1f mm -> l/rmin = %.1f; limite 3/4 (L/r)0 = 0,75 x %.1f = %.1f (5.3.6.1)", spacing,
                                  in.compositeSpacing > 0.0 ? "informado" : "duas ligacoes intermediarias, l = L/3", s.rMin, perMin, slender0, c.resistance));
            c.lines.push_back(fmt("Ligacoes intermediarias ao longo do comprimento: %d (minimo 2, uniformemente espacadas, 5.3.6.1)%s", intermediate, enough ? "" : " - INSUFICIENTE"));
            c.lines.push_back("As ligacoes de extremidade devem ser soldadas ou com parafusos protendidos contra superficies de contato Classe A ou B (5.3.6.2).");
            col.offer(std::move(c), "-", 0.0);
        }
        if (anyTension && perMin > o8.maxSlendernessTension) {
            out.warnings.push_back(fmt("Barra composta tracionada: l/rmin = %.0f acima do limite recomendado de %.0f entre ligacoes adjacentes (NBR 8800, 5.2.8.2).", perMin, o8.maxSlendernessTension));
        }
    }

    // Placa de torcao em tubulares
    Nbr8800::TorsionData torsion;
    if (!cold) {
        torsion = Nbr8800::torsion(s, in.steel, L, o8);
    }

    bool anyTorsionIgnored = false;
    bool slendernessWarned = false;
    // torcao em secoes abertas de paredes finas (5.5.2.3): tensoes pela Teoria da Elasticidade a partir da linha media (coordenada setorial,
    // centro de cisalhamento e Cw); o empenamento so entra quando a barra foi analisada com o 7o grau de liberdade (`in.warping`)
    const bool thinOpenCheck = !s.composite() && s.sectorial.valid && s.J > 0.0 && !s.sectorial.samples.empty();
    // secoes macicas: tensao de cisalhamento elastica de Saint-Venant
    const bool solidCheck = (s.kind == SectionKind::SolidRound && s.D > 0.0) || (s.kind == SectionKind::SolidRect && s.bf > 0.0 && s.d > 0.0);
    const bool warpingCheck = thinOpenCheck;
    bool anyOpenWithoutWarping = false;

    std::map<std::tuple<int, bool, long long, long long>, Nbr8800::FlexureData> flexCache;
    auto flexOf = [&](Axis ax, bool pos, double lbv, double cbv) -> const Nbr8800::FlexureData & {
        const auto key = std::make_tuple(static_cast<int>(ax), pos, std::llround(lbv * 10.0), std::llround(cbv * 1000.0));
        auto it = flexCache.find(key);
        if (it == flexCache.end()) {
            Nbr8800::FlexureData data = cold ? Nbr14762::flexure(s, in.steel, ax, pos, lbv, cbv, o14) : Nbr8800::flexure(s, in.steel, ax, pos, lbv, cbv, o8);
            if (!cold && data.covered && in.netAreaRatio < 1.0 - 1e-9) {
                // 5.4.2.6: furos na mesa tracionada (Afn/Afg adotado igual a An/Ag informado)
                const double yt = (in.steel.fy / in.steel.fu <= 0.8) ? 1.0 : 1.10;
                if (in.steel.fu * in.netAreaRatio < yt * in.steel.fy) {
                    const double wt = (ax == Axis::X) ? (pos ? s.g.WxBottom : s.g.WxTop) : (pos ? s.g.WyLeft : s.g.WyRight);
                    const double limit = in.steel.fu * in.netAreaRatio * wt / o8.gammaA1;
                    data.lines.push_back(fmt("Furos na mesa tracionada (5.4.2.6): fu Afn = %.0f < Yt fy Afg = %.0f (por mm2 de Afg): M_Rd <= fu (Afn/Afg) Wt / ga1 = %.2f kN*m", in.steel.fu * in.netAreaRatio,
                                             yt * in.steel.fy, limit / kKnm));
                    if (limit < data.MRd) {
                        data.MRd = limit;
                        data.governing = "ruptura da mesa tracionada (5.4.2.6)";
                    }
                }
            }
            it = flexCache.emplace(key, std::move(data)).first;
        }
        return it->second;
    };
    std::map<int, Nbr8800::ShearData> shearCache;
    auto shearOf = [&](Axis dir) -> const Nbr8800::ShearData & {
        const int key = static_cast<int>(dir);
        auto it = shearCache.find(key);
        if (it == shearCache.end()) {
            it = shearCache.emplace(key, cold ? Nbr14762::shear(s, in.steel, dir, o14) : Nbr8800::shear(s, in.steel, dir, o8, L / 2.0)).first;
        }
        return it->second;
    };

    if (cold) {
        const Nbr14762::GeometryLimits gl = Nbr14762::geometryLimits(s, in.steel);
        CheckResult c;
        c.id = "geometry";
        c.title = "Relacao largura-espessura maxima";
        c.clause = clauseTag + ", 9.1.2 (Tabela 4)";
        c.demand = gl.ratio;
        c.resistance = 1.0;
        c.ratio = gl.ratio;
        c.lines = gl.lines;
        col.offer(c, "-", 0.0);
    }

    for (const ComboEffects &combo : in.combos) {
        if (combo.stations.empty()) {
            continue;
        }

        // Cm (Anexo C) a partir dos momentos de extremidade
        auto cmFor = [&](double Effects::*member, bool transverse) {
            if (transverse) {
                return 1.0;
            }
            // M1/M2: momentos de extremidade da estrutura nt (Anexo C.2.2); sem separacao nt/lt, os da analise original
            const double ma = combo.hasNt ? combo.stations.front().nt.*member : combo.stations.front().e.*member;
            const double mb = combo.hasNt ? combo.stations.back().nt.*member : combo.stations.back().e.*member;
            const double m2 = std::max(std::abs(ma), std::abs(mb));
            if (m2 < 1e-9) {
                return 1.0;
            }
            const double m1 = std::min(std::abs(ma), std::abs(mb));
            const bool reverse = (ma * mb < 0.0);
            const double ratio = (reverse ? 1.0 : -1.0) * m1 / m2;
            return std::max(0.4, 0.60 - 0.40 * ratio);
        };
        const double cmX = cmFor(&Effects::Mx, combo.transverseLoadX);
        const double cmY = cmFor(&Effects::My, combo.transverseLoadY);
        // Ne de B1: comprimento real da barra e rigidez reduzida nas estruturas de media deslocabilidade (C.2.2, 4.10.7.1.2)
        const double nexPlane = sq(kPi) * E * (in.ixEquivalent > 0.0 ? in.ixEquivalent : s.g.Ix) / sq(L) * combo.bStiffness;
        const double neyPlane = sq(kPi) * E * s.g.Iy / sq(L) * combo.bStiffness;

        // Cb por trecho destravado
        std::vector<double> cbSegment(segments, 1.0);
        std::vector<std::string> cbNote(static_cast<std::size_t>(segments));
        const bool oneFlange = !cold && in.oneFlangeBraced != 0 && (s.kind == SectionKind::I || s.kind == SectionKind::Channel || s.kind == SectionKind::RectTube)
                               && in.section->symmetricAboutX;
        const bool bracedPositive = in.oneFlangeBraced == 1;
        if (in.cantilever && lb > 0.0 && !in.cbEqualsOne) {
            // 5.4.2.3-b: balanco com a extremidade livre sem contencao lateral: Cb = 1,0 (nos demais balancos a norma pede analise de estabilidade;
            // Cb = 1,0 e o limite inferior e fica a favor da seguranca)
            for (std::size_t k = 0; k < cbSegment.size(); ++k) {
                cbNote[k] = "Balanco (5.4.2.3-b): Cb = 1,00";
            }
        } else if (oneFlange && lb > 0.0 && !in.cbEqualsOne) {
            // 5.4.2.4: uma mesa livre e a outra com contencao lateral continua. Momentos positivos = traciona a mesa livre.
            auto freeSigned = [&](double x) {
                const double m = interpolateMoment(combo.stations, x, &Effects::Mx);
                return bracedPositive ? m : -m;
            };
            for (int k = 0; k < segments; ++k) {
                const double x0 = k * L / segments, x1 = (k + 1) * L / segments;
                const double f0 = freeSigned(x0), f1 = freeSigned(x1), fc = freeSigned(0.5 * (x0 + x1));
                const double m1 = std::min(f0, f1);                // maior momento que comprime a mesa livre nas extremidades (negativo)
                const double m2 = f0 <= f1 ? f1 : f0;              // momento na outra extremidade
                double cbk = 1.0;
                std::string text;
                const bool transverse = combo.transverseLoadX;
                if (m1 >= 0.0) {
                    if (in.forceAwayFromFreeFlange && transverse) {
                        cbk = 2.0;
                        text = "5.4.2.4-b: momentos nulos ou que tracionam a mesa livre nas extremidades, carga transversal oposta a mesa livre: Cb = 2,00";
                    } else {
                        text = "5.4.2.4-c: Cb = 1,00";
                    }
                } else if (in.forceAwayFromFreeFlange) {
                    text = "5.4.2.4-c: forcas transversais no sentido oposto a mesa livre: Cb = 1,00";
                } else {
                    const double second = (2.0 / 3.0) * m2 / m1;
                    const double third = m2 <= 0.0 ? (8.0 / 3.0) * fc / (m1 + m2) : 0.0;
                    cbk = std::min(3.0, 3.0 - second - third);
                    text = fmt("5.4.2.4-a: Cb = 3,00 - (2/3) M2/M1 - (8/3) MCL/(M1+M2) = 3,00 - %.3f - %.3f = %.2f (M1 = %.2f, M2 = %.2f, MCL = %.2f kN*m; negativo = comprime a mesa livre)",
                               second, third, cbk, m1 / kKnm, m2 / kKnm, fc / kKnm);
                }
                cbSegment[static_cast<std::size_t>(k)] = cbk;
                cbNote[static_cast<std::size_t>(k)] = text;
            }
        } else if (!in.cbEqualsOne && lb > 0.0) {
            for (int k = 0; k < segments; ++k) {
                const double x0 = k * L / segments;
                const double x1 = (k + 1) * L / segments;
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
                cbSegment[static_cast<std::size_t>(k)] = std::min(3.0, Nbr8800::cbFromQuarterMoments(mMax, ma, mb, mc));
            }
        }

        for (std::size_t stIndex = 0; stIndex < combo.stations.size(); ++stIndex) {
            const StationEffects &st = combo.stations[stIndex];
            if (in.onlyStation >= 0 && static_cast<int>(stIndex) != in.onlyStation) {
                continue;
            }
            Effects e = st.e;
            const auto whereOf = [&]() { return fmt("combinacao %s, x = %.0f mm", combo.name.c_str(), st.x); };

            // Amplificacao dos momentos (Anexo C)
            double b1x = 1.0;
            double b1y = 1.0;
            if (in.amplifyB1 && e.N < 0.0) {
                const double nc = -e.N;
                b1x = nc >= nexPlane ? 1e6 : std::max(1.0, cmX / (1.0 - nc / nexPlane));
                b1y = nc >= neyPlane ? 1e6 : std::max(1.0, cmY / (1.0 - nc / neyPlane));
            }
            const double b2All = in.b2 * combo.b2;
            double mxSd, mySd;
            std::string ampNote;
            if (combo.hasNt) {
                // Anexo C.2.1: M_Sd = B1 M_nt + B2 M_lt e N_Sd = N_nt + B2 N_lt (lt = estrutura original - nt)
                const Effects &nt = st.nt;
                mxSd = b1x * nt.Mx + b2All * (e.Mx - nt.Mx);
                mySd = b1y * nt.My + b2All * (e.My - nt.My);
                const double nSdAmp = nt.N + b2All * (e.N - nt.N);
                ampNote = fmt("Anexo C: M_Sd = B1 M_nt + B2 M_lt (Mx: %.2f x %.2f + %.3f x %.2f kN*m); N_Sd = N_nt + B2 N_lt = %.2f + %.3f x %.2f = %.2f kN (B1x = %.3f, B2 = %.3f%s)",
                              b1x, nt.Mx / kKnm, b2All, (e.Mx - nt.Mx) / kKnm, nt.N / kKn, b2All, (e.N - nt.N) / kKn, nSdAmp / kKn, b1x, b2All,
                              combo.bStiffness < 1.0 ? fmt("; rigidez reduzida a %.2f em Ne (4.10.7.1.2)", combo.bStiffness).c_str() : "");
                e.N = nSdAmp;
            } else {
                mxSd = e.Mx * b1x * b2All;
                mySd = e.My * b1y * b2All;
            }
            const double absMx = std::abs(mxSd);
            const double absMy = std::abs(mySd);

            // --- tracao ---
            double nRd = comp.NcRd;
            double nSd = 0.0;
            if (e.N > 0.0) {
                nRd = tens.NtRd;
                nSd = e.N;
                if (col.wants("tension", ratioOf(nSd, tens.NtRd))) {
                    std::vector<std::string> lines = tens.lines;
                    lines.push_back(fmt("N_t,Sd = %s -> N_t,Sd / N_t,Rd = %.3f  (%s)", fmt("%.2f kN", nSd / kKn).c_str(), nSd / tens.NtRd, whereOf().c_str()));
                    col.offer(makeCheck("tension", "Tracao axial", cold ? clauseTag + ", 9.6" : clauseTag + ", 5.2", nSd, tens.NtRd, "kN", kKn, std::move(lines)), combo.name, st.x);
                }
            } else if (e.N < 0.0) {
                nSd = -e.N;
                if (col.wants("compression", ratioOf(nSd, comp.NcRd))) {
                    std::vector<std::string> lines = comp.lines;
                    lines.push_back(fmt("N_c,Sd = %.2f kN -> N_c,Sd / N_c,Rd = %.3f  (%s)", nSd / kKn, nSd / comp.NcRd, whereOf().c_str()));
                    col.offer(makeCheck("compression", "Compressao axial", cold ? clauseTag + ", 9.7" : clauseTag + ", 5.3", nSd, comp.NcRd, "kN", kKn, std::move(lines)), combo.name, st.x);
                }
            }

            // --- esbeltez ---
            if (e.N < 0.0 && in.slendernessGoverns) {
                const double limit = cold ? o14.maxSlendernessCompression : o8.maxSlendernessCompression;
                const double slenderRatio = comp.slenderness / limit;
                if (col.wants("slenderness", slenderRatio)) {
                    CheckResult c;
                    c.id = "slenderness";
                    c.title = "Limitacao de esbeltez (compressao)";
                    c.clause = cold ? clauseTag + ", 9.7.4" : clauseTag + ", 5.3.7";
                    c.demand = comp.slenderness;
                    c.resistance = limit;
                    c.ratio = slenderRatio;
                    c.unit = "";
                    c.lines.push_back(fmt("lambda = KL / r = %.1f (maior indice de esbeltez entre os eixos)  (%s)", comp.slenderness, whereOf().c_str()));
                    c.lines.push_back(fmt("lambda <= %.0f (limite da norma)", limit));
                    col.offer(std::move(c), combo.name, st.x);
                }
            } else if (e.N > 0.0 && in.slendernessGoverns) {
                const double limit = cold ? o14.maxSlendernessTension : o8.maxSlendernessTension;
                if (lambdaTension > limit && !slendernessWarned) {
                    // Limite de esbeltez em tracao e apenas recomendado pelas normas: aviso, sem reprovar a barra.
                    slendernessWarned = true;
                    out.warnings.push_back(fmt("Esbeltez em tracao L/r = %.0f acima do limite recomendado %.0f (%s, %s).", lambdaTension, limit,
                                               cold ? "NBR 14762, 9.6.3" : "NBR 8800, 5.2.8", in.section->name.c_str()));
                }
            }

            // Segmento destravado
            const int seg = std::min(segments - 1, static_cast<int>(st.x / (L / segments) - 1e-9 < 0 ? 0 : static_cast<int>(st.x / (L / segments) - 1e-9)));
            const double cb = cbSegment[static_cast<std::size_t>(std::max(0, seg))];
            const std::string &cbExplain = cbNote[static_cast<std::size_t>(std::max(0, seg))];

            // --- flexao ---
            double mxRd = 0.0;
            double myRd = 0.0;
            bool flexCovered = true;
            if (absMx > 1e-9 || absMy > 1e-9) {
                const double mxMax = absMx;
                const double myMax = absMy;
                if (mxMax > 1e-9) {
                    // uma mesa com contencao continua: o momento que comprime essa mesa nao sofre FLT
                    const bool compressesBracedFlange = oneFlange && ((mxSd > 0.0) == bracedPositive);
                    const auto &f = flexOf(Axis::X, mxSd > 0.0, compressesBracedFlange ? 0.0 : lb, compressesBracedFlange ? 1.0 : cb);
                    flexCovered = flexCovered && f.covered;
                    if (f.covered) {
                        mxRd = f.MRd;
                        if (col.wants("flexure_x", ratioOf(mxMax, f.MRd))) {
                            std::vector<std::string> lines = f.lines;
                            if (!cbExplain.empty()) {
                                lines.push_back(cbExplain);
                            }
                            if (!ampNote.empty()) {
                                lines.push_back(ampNote);
                            }
                            if (compressesBracedFlange) {
                                lines.push_back("Momento que comprime a mesa com contencao lateral continua (5.4.2.4): sem FLT.");
                            }
                            lines.push_back(fmt("M_x,Sd = %.2f kN*m (B1 = %.3f) -> M_x,Sd / M_x,Rd = %.3f; governa: %s  (%s)", mxMax / kKnm, b1x, mxMax / f.MRd, f.governing.c_str(), whereOf().c_str()));
                            col.offer(makeCheck("flexure_x", "Flexao em torno do eixo X (maior inercia)", cold ? clauseTag + ", 9.8.2" : clauseTag + ", 5.4.2 e Anexo D", mxMax, f.MRd, "kN*m", kKnm, std::move(lines)), combo.name, st.x);
                        }
                    }
                }
                if (myMax > 1e-9) {
                    const auto &f = flexOf(Axis::Y, mySd > 0.0, 0.0, 1.0);
                    flexCovered = flexCovered && f.covered;
                    if (f.covered) {
                        myRd = f.MRd;
                        if (col.wants("flexure_y", ratioOf(myMax, f.MRd))) {
                            std::vector<std::string> lines = f.lines;
                            lines.push_back(fmt("M_y,Sd = %.2f kN*m (B1 = %.3f) -> M_y,Sd / M_y,Rd = %.3f; governa: %s  (%s)", myMax / kKnm, b1y, myMax / f.MRd, f.governing.c_str(), whereOf().c_str()));
                            col.offer(makeCheck("flexure_y", "Flexao em torno do eixo Y (menor inercia)", cold ? clauseTag + ", 9.8.2" : clauseTag + ", 5.4.2 e Anexo D", myMax, f.MRd, "kN*m", kKnm, std::move(lines)), combo.name, st.x);
                        }
                    }
                }
            }

            // --- cortante ---
            double vyRd = 0.0;
            double vzRd = 0.0;
            if (std::abs(e.Vy) > 1e-9) {
                const auto &v = shearOf(Axis::Y);
                if (v.covered) {
                    vyRd = v.VRd;
                    if (col.wants("shear_y", ratioOf(std::abs(e.Vy), v.VRd))) {
                        std::vector<std::string> lines = v.lines;
                        lines.push_back(fmt("V_y,Sd = %.2f kN -> V_y,Sd / V_Rd = %.3f  (%s)", std::abs(e.Vy) / kKn, std::abs(e.Vy) / v.VRd, whereOf().c_str()));
                        col.offer(makeCheck("shear_y", "Cortante na direcao da altura (alma)", cold ? clauseTag + ", 9.8.3" : clauseTag + ", 5.4.3", std::abs(e.Vy), v.VRd, "kN", kKn, std::move(lines)), combo.name, st.x);
                    }
                }
            }
            if (std::abs(e.Vz) > 1e-9) {
                const auto &v = shearOf(Axis::X);
                if (v.covered) {
                    vzRd = v.VRd;
                    if (col.wants("shear_z", ratioOf(std::abs(e.Vz), v.VRd))) {
                        std::vector<std::string> lines = v.lines;
                        lines.push_back(fmt("V_z,Sd = %.2f kN -> V_z,Sd / V_Rd = %.3f  (%s)", std::abs(e.Vz) / kKn, std::abs(e.Vz) / v.VRd, whereOf().c_str()));
                        col.offer(makeCheck("shear_z", "Cortante na direcao da largura (mesas)", cold ? clauseTag + ", 9.8.3" : clauseTag + ", 5.4.3", std::abs(e.Vz), v.VRd, "kN", kKn, std::move(lines)), combo.name, st.x);
                    }
                }
            }

            // --- interacao N + M ---
            if ((absMx > 1e-9 || absMy > 1e-9) && flexCovered && (mxRd > 0.0 || myRd > 0.0)) {
                const double mRatioX = (absMx > 1e-9 && mxRd > 0.0) ? absMx / mxRd : 0.0;
                const double mRatioY = (absMy > 1e-9 && myRd > 0.0) ? absMy / myRd : 0.0;
                const double nRatio = nRd > 0.0 ? nSd / nRd : 0.0;
                double ratio;
                if (cold) {
                    ratio = nRatio + mRatioX + mRatioY;
                } else if (nRatio >= 0.2) {
                    ratio = nRatio + 8.0 / 9.0 * (mRatioX + mRatioY);
                } else {
                    ratio = nRatio / 2.0 + (mRatioX + mRatioY);
                }
                if (col.wants("interaction", ratio)) {
                    std::string expr;
                    if (cold) {
                        expr = fmt("NBR 14762, 9.9: N_Sd/N_Rd + Mx,Sd/Mx,Rd + My,Sd/My,Rd = %.3f + %.3f + %.3f = %.3f", nRatio, mRatioX, mRatioY, ratio);
                    } else if (nRatio >= 0.2) {
                        expr = fmt("NBR 8800, 5.5.1.2-a: N_Sd/N_Rd + 8/9 (Mx,Sd/Mx,Rd + My,Sd/My,Rd) = %.3f + 8/9 (%.3f + %.3f) = %.3f", nRatio, mRatioX, mRatioY, ratio);
                    } else {
                        expr = fmt("NBR 8800, 5.5.1.2-b: N_Sd/(2 N_Rd) + (Mx,Sd/Mx,Rd + My,Sd/My,Rd) = %.3f/2 + (%.3f + %.3f) = %.3f", nRatio, mRatioX, mRatioY, ratio);
                    }
                    CheckResult c;
                    c.id = "interaction";
                    c.title = e.N == 0.0 ? "Flexao composta (momentos)" : (e.N > 0.0 ? "Flexo-tracao" : "Flexo-compressao");
                    c.clause = cold ? clauseTag + ", 9.9" : clauseTag + ", 5.5.1.2";
                    c.demand = ratio;
                    c.resistance = 1.0;
                    c.ratio = ratio;
                    c.lines.push_back(expr + fmt("  (%s)", whereOf().c_str()));
                    col.offer(std::move(c), combo.name, st.x);
                }
            }

            // --- momento e cortante combinados (14762, 9.8.4) ---
            if (cold) {
                auto mv = [&](double m, double mrd, double v, double vrd, const char *id, const char *title) {
                    if (m > 1e-9 && mrd > 0.0 && v > 1e-9 && vrd > 0.0) {
                        const double combined = sq(m / mrd) + sq(v / vrd);
                        if (!col.wants(id, combined)) {
                            return;
                        }
                        CheckResult c;
                        c.id = id;
                        c.title = title;
                        c.clause = clauseTag + ", 9.8.4";
                        c.demand = combined;
                        c.resistance = 1.0;
                        c.ratio = c.demand;
                        c.lines.push_back(fmt("(M_Sd/M_Rd)^2 + (V_Sd/V_Rd)^2 = (%.3f)^2 + (%.3f)^2 = %.3f  (%s)", m / mrd, v / vrd, c.ratio, whereOf().c_str()));
                        col.offer(std::move(c), combo.name, st.x);
                    }
                };
                mv(absMx, mxRd, std::abs(e.Vy), vyRd, "mv_x", "Momento fletor e forca cortante combinados (eixo X)");
                mv(absMy, myRd, std::abs(e.Vz), vzRd, "mv_y", "Momento fletor e forca cortante combinados (eixo Y)");
            }

            // --- torcao ---
            if (std::abs(e.T) > 1e-9) {
                if (torsion.covered) {
                    const double tRatio = std::abs(e.T) / torsion.TRd;
                    double ratio = tRatio;
                    double nr = 0.0, mr = 0.0, vr = 0.0;
                    if (tRatio > 0.20) {
                        nr = nRd > 0.0 ? nSd / nRd : 0.0;
                        mr = (mxRd > 0.0 ? absMx / mxRd : 0.0) + (myRd > 0.0 ? absMy / myRd : 0.0);
                        vr = (vyRd > 0.0 ? std::abs(e.Vy) / vyRd : 0.0) + (vzRd > 0.0 ? std::abs(e.Vz) / vzRd : 0.0);
                        ratio = (nr + mr) + sq(vr + tRatio);
                    }
                    if (col.wants("torsion", ratio)) {
                        CheckResult c;
                        c.id = "torsion";
                        c.title = "Torcao";
                        c.clause = clauseTag + ", 5.5.2";
                        c.demand = std::abs(e.T) / kKnm;
                        c.resistance = torsion.TRd / kKnm;
                        c.unit = "kN*m";
                        c.lines = torsion.lines;
                        if (tRatio > 0.20) {
                            c.lines.push_back(fmt("T_Sd/T_Rd = %.3f > 0,20: (N/NRd + M/MRd) + (V/VRd + T/TRd)^2 = (%.3f + %.3f) + (%.3f + %.3f)^2 = %.3f  (%s)", tRatio, nr, mr, vr, tRatio, ratio, whereOf().c_str()));
                        } else {
                            c.lines.push_back(fmt("T_Sd/T_Rd = %.3f <= 0,20: efeito da torcao desprezado nas interacoes (5.5.2.2)  (%s)", tRatio, whereOf().c_str()));
                        }
                        c.ratio = ratio;
                        col.offer(std::move(c), combo.name, st.x);
                    }
                } else if (std::abs(e.T) > 1e5 && !warpingCheck && !solidCheck) {
                    anyTorsionIgnored = true;
                }
            }

            // --- torcao em secoes abertas (5.5.2.3): tensoes normais e de cisalhamento pela Teoria da Elasticidade ---
            if (warpingCheck && (std::abs(e.T) > 1e-9 || std::abs(e.B) > 1e-9)) {
                const double gammaStress = cold ? o14.gammaFlexure : o8.gammaA1;
                const double fyd = in.steel.fy / gammaStress;
                const SectorialData &sec = s.sectorial;
                const bool withWarping = in.warping && sec.Cw > 0.0;
                if (!in.warping && std::abs(e.T) > 1e5) {
                    anyOpenWithoutWarping = true;
                }
                const double bimoment = withWarping ? e.B : 0.0;
                const double tw = withWarping ? e.Tw : 0.0;
                // tensao normal em todos os pontos de amostragem: sigma = N/A - Mz y/Iz + My z/Iy + B omega/Cw (eixos locais; Mz = Mx de projeto, My local = -My de projeto)
                double sigmaTension = -1e300, sigmaCompression = -1e300, sigmaWarpMax = 0.0;
                for (const SectorialData::Sample &p : sec.samples) {
                    const double sigmaW = withWarping ? bimoment * p.omega / sec.Cw : 0.0;
                    const double sigma = e.N / s.g.area - mxSd * p.y / s.g.Ix - mySd * p.z / s.g.Iy + sigmaW;
                    sigmaTension = std::max(sigmaTension, sigma);
                    sigmaCompression = std::max(sigmaCompression, -sigma);
                    sigmaWarpMax = std::max(sigmaWarpMax, std::abs(sigmaW));
                }
                // chi: menor fator entre os estados-limite de instabilidade ja avaliados (5.3 e 5.4), adotado para a tensao normal de compressao
                double chi = 1.0;
                std::string chiNote = "chi = 1,00 (sem compressao)";
                if (sigmaCompression > 0.0) {
                    std::string governing = "escoamento";
                    auto take = [&](double value, const char *name) {
                        if (value > 0.0 && value < chi) {
                            chi = value;
                            governing = name;
                        }
                    };
                    if (e.N < 0.0 && comp.NcRd > 0.0) {
                        take(comp.NcRd / (s.g.area * fyd), "compressao axial");
                    }
                    if (absMx > 0.0 && mxRd > 0.0) {
                        take(mxRd / (s.g.Wx() * fyd), "flexao eixo X");
                    }
                    if (absMy > 0.0 && myRd > 0.0) {
                        take(myRd / (s.g.Wy() * fyd), "flexao eixo Y");
                    }
                    chi = std::min(chi, 1.0);
                    chiNote = fmt("chi = %.3f (menor fator entre os estados-limite de instabilidade ja avaliados: %s)", chi, governing.c_str());
                }
                const double ratioNormal = std::max(sigmaTension / fyd, sigmaCompression / (chi * fyd));
                if (col.wants("warping_normal", ratioNormal)) {
                    CheckResult c;
                    c.id = "warping_normal";
                    c.title = in.warping ? "Torcao com empenamento: tensoes normais" : "Torcao: tensoes normais";
                    c.clause = clauseTag + ", 5.5.2.3-a e 5.5.2.3-c";
                    c.demand = std::max(sigmaTension, sigmaCompression);
                    c.resistance = fyd;
                    c.unit = "MPa";
                    c.ratio = ratioNormal;
                    c.lines.push_back(fmt("Linha media da secao (paredes finas): Cw = %.4g mm6; omega_max = %.0f mm2; centro de cisalhamento a (y_s, z_s) = (%.1f, %.1f) mm do centroide", sec.Cw, sec.omegaMax, sec.ys, sec.zs));
                    if (withWarping) {
                        c.lines.push_back(fmt("Bimomento B = %.4g kN*m2; sigma_w = B omega/Cw = %.1f MPa (maximo); N = %.2f kN, Mx = %.2f kN*m, My = %.2f kN*m", bimoment / 1e9, sigmaWarpMax, e.N / kKn,
                                              mxSd / kKnm, mySd / kKnm));
                    } else {
                        c.lines.push_back(fmt("Barra analisada sem empenamento (so Saint-Venant): tensoes de empenamento desprezadas; N = %.2f kN, Mx = %.2f kN*m, My = %.2f kN*m", e.N / kKn, mxSd / kKnm, mySd / kKnm));
                    }
                    c.lines.push_back(fmt("sigma_Sd,max: tracao %.1f MPa, compressao %.1f MPa; fy/gamma = %.1f MPa (escoamento, 5.5.2.3-a)", sigmaTension, sigmaCompression, fyd));
                    c.lines.push_back(chiNote + fmt("; compressao: sigma_Sd/(chi fy/gamma) = %.3f  (%s)", sigmaCompression / (chi * fyd), whereOf().c_str()));
                    col.offer(std::move(c), combo.name, st.x);
                }
                // tensoes de cisalhamento em todos os pontos de amostragem; parcelas somadas em modulo (sentidos relativos nao conhecidos): envoltoria conservadora
                const double tsv = e.T - tw;
                const double aw = (s.kind == SectionKind::I || s.kind == SectionKind::Channel) ? s.d * s.tw : s.g.area;
                const double chiV = (vyRd > 0.0 && aw > 0.0 && (s.kind == SectionKind::I || s.kind == SectionKind::Channel)) ? std::min(1.0, vyRd / (0.60 * fyd * aw)) : 1.0;
                double ratioShear = 0.0, tauMax = 0.0, tauSvAt = 0.0, tauVAt = 0.0, tauWAt = 0.0, limitAt = 0.60 * fyd;
                for (const SectorialData::Sample &p : sec.samples) {
                    const double tauSv = std::abs(tsv) * p.t / s.J;
                    const double tauV = std::abs(e.Vy) * std::abs(p.sy) / (s.g.Ix * p.t) + std::abs(e.Vz) * std::abs(p.sz) / (s.g.Iy * p.t);
                    const double tauW = withWarping ? std::abs(tw) * std::abs(p.sw) / (sec.Cw * p.t) : 0.0;
                    const double tau = tauSv + tauV + tauW;
                    const double limit = 0.60 * fyd * (p.alignedY > 0.7 ? chiV : 1.0);
                    if (tau / limit > ratioShear) {
                        ratioShear = tau / limit;
                        tauMax = tau;
                        tauSvAt = tauSv;
                        tauVAt = tauV;
                        tauWAt = tauW;
                        limitAt = limit;
                    }
                }
                if (col.wants("warping_shear", ratioShear)) {
                    CheckResult c;
                    c.id = "warping_shear";
                    c.title = in.warping ? "Torcao com empenamento: tensoes de cisalhamento" : "Torcao: tensoes de cisalhamento";
                    c.clause = clauseTag + ", 5.5.2.3-b e 5.5.2.3-d";
                    c.demand = tauMax;
                    c.resistance = limitAt;
                    c.unit = "MPa";
                    c.ratio = ratioShear;
                    c.lines.push_back(fmt("T = %.2f kN*m = T_sv + T_w = %.2f + %.2f kN*m (T_w: parcela de empenamento)", e.T / kKnm, tsv / kKnm, tw / kKnm));
                    c.lines.push_back(fmt("Ponto mais solicitado: tau = tau_sv (T_sv t/J = %.1f) + tau_w (T_w S_w/(Cw t) = %.1f) + tau_V (V S/(I t) = %.1f) = %.1f MPa", tauSvAt, tauWAt, tauVAt, tauMax));
                    c.lines.push_back(fmt("Limite: 0,60 chi fy/gamma = %.1f MPa (chi = %.3f da flambagem por cisalhamento da alma, VRd/Vpl, nas paredes paralelas ao eixo y, 5.5.2.3-d)  (%s)", limitAt, chiV, whereOf().c_str()));
                    col.offer(std::move(c), combo.name, st.x);
                }
            }

            // --- torcao em secoes macicas (5.5.2.3): tensao de cisalhamento elastica de Saint-Venant ---
            if (solidCheck && std::abs(e.T) > 1e-9) {
                const double fyd = in.steel.fy / (cold ? o14.gammaFlexure : o8.gammaA1);
                double tauT, tauV, area, factor;
                std::string how;
                if (s.kind == SectionKind::SolidRound) {
                    tauT = 16.0 * std::abs(e.T) / (kPi * s.D * s.D * s.D);
                    area = kPi * s.D * s.D / 4.0;
                    factor = 4.0 / 3.0;
                    how = fmt("tau_T = 16 T/(pi D^3) = %.1f MPa", tauT);
                } else {
                    const double bShort = std::min(s.bf, s.d), hLong = std::max(s.bf, s.d);
                    const double ratio = hLong / bShort;
                    // coeficiente alpha de torcao de retangulos macicos (Timoshenko): h/b = 1; 1,5; 2; 3; 4; 6; 10; infinito
                    static const double kRatio[] = {1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 10.0, 1e9};
                    static const double kAlpha[] = {0.208, 0.231, 0.246, 0.267, 0.282, 0.299, 0.312, 0.333};
                    double alpha = kAlpha[7];
                    for (int k = 0; k < 7; ++k) {
                        if (ratio <= kRatio[k + 1]) {
                            alpha = kAlpha[k] + (kAlpha[k + 1] - kAlpha[k]) * (std::min(ratio, kRatio[k + 1]) - kRatio[k]) / (kRatio[k + 1] - kRatio[k]);
                            break;
                        }
                    }
                    tauT = std::abs(e.T) / (alpha * hLong * bShort * bShort);
                    area = s.bf * s.d;
                    factor = 1.5;
                    how = fmt("tau_T = T/(alpha h b^2) = %.1f MPa (alpha = %.3f para h/b = %.2f)", tauT, alpha, ratio);
                }
                tauV = factor * std::hypot(e.Vy, e.Vz) / area;
                const double tau = tauT + tauV;
                const double ratioT = tau / (0.60 * fyd);
                if (col.wants("torsion", ratioT)) {
                    CheckResult c;
                    c.id = "torsion";
                    c.title = "Torcao em secao macica: tensoes de cisalhamento";
                    c.clause = clauseTag + ", 5.5.2.3-b";
                    c.demand = tau;
                    c.resistance = 0.60 * fyd;
                    c.unit = "MPa";
                    c.ratio = ratioT;
                    c.lines.push_back(how);
                    c.lines.push_back(fmt("tau_V = %.2f V/A = %.1f MPa; tau = tau_T + tau_V = %.1f MPa (soma em modulo, envoltoria); limite 0,60 fy/gamma = %.1f MPa  (%s)", factor, tauV, tau, 0.60 * fyd, whereOf().c_str()));
                    col.offer(std::move(c), combo.name, st.x);
                }
            }
        }
    }

    // --- NBR 8800: solda mesa-alma (5.4.5), lamelas (5.4.4) e comprimento destravado entre rotulas plasticas (4.11.5) ---
    if (!cold && (in.webWeldLeg > 0.0 || in.coverB > 0.0 || in.plasticAnalysis)) {
        BeamExtraContext ctx;
        ctx.length = L;
        ctx.segments = segments;
        ctx.lb = in.lbOverride > 0.0 ? in.lbOverride : L / std::max(1, segments);
        ctx.gammaA1 = o8.gammaA1;
        ctx.gammaA2 = o8.gammaA2;
        ctx.clauseTag = clauseTag;
        if (s.kind == SectionKind::I) {
            const auto &fx = flexOf(Axis::X, true, in.compressionFlangeBraced ? 0.0 : ctx.lb, in.cbEqualsOne ? 1.0 : 1.0);
            ctx.mxRdBare = fx.covered ? fx.MRd : 0.0;
        }
        for (CheckResult &extra : beamExtraChecks(in, ctx)) {
            const std::string id = extra.id;
            col.offer(std::move(extra), "envoltoria", 0.0);
            (void)id;
        }
    }

    for (const std::string &id : col.order()) {
        const Candidate &cand = col.best().at(id);
        out.checks.push_back(cand.check);
        if (cand.check.ratio > out.maxRatio) {
            out.maxRatio = cand.check.ratio;
            out.governingCheck = id;
            out.governingCombo = cand.combo;
            out.governingX = cand.x;
        }
    }
    // --- NBR 8800, Anexo H: fadiga (faixa de variacao das tensoes normais nas combinacoes selecionadas) ---
    if (!cold && in.fatigueCategory > 0) {
        struct Category {
            const char *name;
            double cf, th;
            bool shear;
        };
        static const Category kCategories[] = {{"A", 250e8, 165.0, false}, {"B", 120e8, 110.0, false}, {"B'", 61e8, 83.0, false}, {"C", 44e8, 69.0, false},
                                                {"D", 22e8, 48.0, false}, {"E", 11e8, 31.0, false}, {"E'", 3.9e8, 18.0, false}, {"F", 160e10, 55.0, true},
                                                {"G", 3.9e8, 48.0, false}};
        const Category &cat = kCategories[std::clamp(in.fatigueCategory, 1, 9) - 1];
        const double area = s.g.area, wx = s.g.Wx(), wy = s.g.Wy();
        double worstRange = 0.0;
        bool anyTension = false;
        for (const double sx : {-1.0, 1.0}) {
            for (const double sy : {-1.0, 1.0}) {
                double smax = -1e300, smin = 1e300;
                for (const ComboEffects &combo : in.combos) {
                    for (const StationEffects &st : combo.stations) {
                        const double sigma = (area > 0.0 ? st.e.N / area : 0.0) + (wx > 0.0 ? sx * st.e.Mx / wx : 0.0) + (wy > 0.0 ? sy * st.e.My / wy : 0.0);
                        smax = std::max(smax, sigma);
                        smin = std::min(smin, sigma);
                    }
                }
                if (smax > -1e299 && smax > 0.0) {   // H.2.5: sem verificacao se os extremos sao ambos de compressao
                    anyTension = true;
                    worstRange = std::max(worstRange, smax - smin);
                }
            }
        }
        CheckResult c;
        c.id = "fatigue";
        c.title = "Fadiga (faixa de variacao de tensoes)";
        c.clause = clauseTag + ", Anexo H";
        c.unit = "MPa";
        double adm = cat.th;
        if (in.fatigueCycles > 0.0) {
            adm = cat.shear ? std::pow(11e4 * cat.cf / in.fatigueCycles, 0.167) : std::pow(329.0 * cat.cf / in.fatigueCycles, 0.333);
            adm = std::max(adm, cat.th);
        }
        c.demand = worstRange;
        c.resistance = adm;
        if (in.fatigueCycles > 0.0 && in.fatigueCycles < 20000.0) {
            c.ratio = 0.0;
            c.lines.push_back(fmt("N = %.0f < 20 000 ciclos: verificacao de fadiga dispensada (H.2.6).", in.fatigueCycles));
        } else if (!anyTension) {
            c.ratio = 0.0;
            c.lines.push_back("Tensoes extremas apenas de compressao: verificacao de fadiga dispensada (H.2.5).");
        } else {
            c.ratio = adm > 0.0 ? worstRange / adm : 0.0;
            c.lines.push_back(fmt("Categoria %s: Cf = %.3g; sigma_TH = %.0f MPa", cat.name, cat.cf, cat.th));
            c.lines.push_back(in.fatigueCycles > 0.0 ? fmt("sigma_SR = %s = %.1f MPa (>= sigma_TH); N = %.3g ciclos", cat.shear ? "(11e4 Cf/N)^0,167" : "(329 Cf/N)^0,333", adm, in.fatigueCycles)
                                                      : fmt("N nao informado: vida infinita, sigma_SR = sigma_TH = %.0f MPa", adm));
            c.lines.push_back(fmt("Faixa calculada = maior (tensao maxima - tensao minima) nos quatro cantos da secao = %.1f MPa (sigma = N/A +- Mx/Wx +- My/Wy, combinacoes selecionadas; "
                                  "elas devem ser as combinacoes de fadiga de H.2.1)", worstRange));
            c.lines.push_back("A categoria deve ser coerente com o detalhe construtivo (Tabela H.1); parafusos em tracao: faixa sobre a area liquida efetiva, com efeito de alavanca (H.5).");
        }
        out.checks.push_back(c);
        if (c.ratio > out.maxRatio) {
            out.maxRatio = c.ratio;
            out.governingCheck = c.id;
            out.governingCombo.clear();
        }
    }

    // --- NBR 8800, Anexo F: abertura circular na alma, sem reforco ---
    if (!cold && in.openingDiameter > 0.0 && s.kind == SectionKind::I) {
        const double dOpen = in.openingDiameter;
        const double sqrtEf = std::sqrt(in.steel.E / in.steel.fy);
        std::vector<std::string> lines;
        int failures = 0;
        auto test = [&](bool ok, const std::string &text) {
            lines.push_back(std::string(ok ? "[ok] " : "[NAO ATENDE] ") + text);
            failures += ok ? 0 : 1;
        };
        const double slenderWeb = s.h / s.tw, limitWeb = 3.76 * sqrtEf;
        test(slenderWeb <= limitWeb, fmt("h/tw = %.1f <= 3,76 sqrt(E/fy) = %.1f", slenderWeb, limitWeb));
        const double slenderFlange = s.bf / (2.0 * s.tf), limitFlange = 0.38 * sqrtEf;
        test(slenderFlange <= limitFlange, fmt("bf/(2 tf) = %.1f <= 0,38 sqrt(E/fy) = %.1f", slenderFlange, limitFlange));
        test(std::abs(in.openingOffset) + dOpen / 2.0 <= s.d / 6.0, fmt("abertura dentro do terco medio da altura: |e| + D/2 = %.0f mm <= d/6 = %.0f mm", std::abs(in.openingOffset) + dOpen / 2.0, s.d / 6.0));
        test(in.openingPosition - dOpen / 2.0 >= L / 4.0 && in.openingPosition + dOpen / 2.0 <= 3.0 * L / 4.0,
             fmt("abertura nos dois quartos centrais do vao: de %.0f a %.0f mm, dentro de [%.0f ; %.0f] mm", in.openingPosition - dOpen / 2.0, in.openingPosition + dOpen / 2.0, L / 4.0, 3.0 * L / 4.0));
        if (in.openingNeighbourD > 0.0) {
            const double minSpacing = 2.5 * std::max(dOpen, in.openingNeighbourD);
            test(in.openingSpacing >= minSpacing, fmt("distancia entre centros = %.0f mm >= 2,5 x maior diametro = %.0f mm", in.openingSpacing, minSpacing));
        }
        const auto &vy = shearOf(Axis::Y);
        double vEnd = 0.0;
        for (const ComboEffects &combo : in.combos) {
            if (!combo.stations.empty()) {
                vEnd = std::max({vEnd, std::abs(combo.stations.front().e.Vy), std::abs(combo.stations.back().e.Vy)});
            }
        }
        if (vy.covered && vy.VRd > 0.0) {
            test(vEnd <= 0.5 * vy.VRd, fmt("cortante nos apoios V_Sd = %.1f kN <= 50 %% de V_Rd = %.1f kN", vEnd / kKn, 0.5 * vy.VRd / kKn));
        }
        lines.push_back("Condicoes de F.3 que o programa nao confere: viga biapoiada e prismatica, carregamento uniformemente distribuido, ausencia de reforcos.");
        CheckResult c;
        c.id = "web_opening";
        c.title = "Abertura circular na alma (sem reforco)";
        c.clause = clauseTag + ", Anexo F";
        c.unit = "-";
        c.demand = failures;
        c.resistance = 0.0;
        c.ratio = failures > 0 ? 1.0 + 0.1 * failures : 0.0;
        c.lines = lines;
        out.checks.push_back(c);
        if (c.ratio > out.maxRatio) {
            out.maxRatio = c.ratio;
            out.governingCheck = c.id;
            out.governingCombo.clear();
        }
    }

    // --- NBR 8800, 4.12: forca e rigidez minimas das contencoes de translacao (nas secoes entre pontos contidos) ---
    if (!cold && (in.lateralSegments > 1 || in.lbOverride > 0.0)) {
        CheckResult c;
        c.id = "bracing";
        c.title = "Contencoes laterais (requisitos)";
        c.clause = clauseTag + ", 4.12";
        c.unit = "-";
        c.ratio = 0.0;
        const double gammaR = 1.35;
        double nMax = 0.0, mMax = 0.0;
        for (const ComboEffects &combo : in.combos) {
            for (const StationEffects &st : combo.stations) {
                nMax = std::max(nMax, -st.e.N);
                mMax = std::max(mMax, std::abs(st.e.Mx));
            }
        }
        const double lbc = in.lbOverride > 0.0 ? in.lbOverride : L / std::max(1, in.lateralSegments);
        const int braces = std::max(1, in.lateralSegments - 1);
        if (nMax > 0.0) {
            const double fbr = 0.01 * nMax;
            const double sbr = 2.0 * (4.0 - 2.0 / braces) * gammaR * nMax / lbc;
            c.lines.push_back(fmt("Pilar (4.12.2.2), N_Sd = %.1f kN, Lbc = %.0f mm, n = %d contencoes: F_br = 0,01 N_Sd = %.2f kN; S_br = 2 (4 - 2/n) gr N_Sd / Lbc = %.3f kN/mm (gr = 1,35)",
                                  nMax / kKn, lbc, braces, fbr / kKn, sbr / kKn));
        }
        if (mMax > 0.0 && s.kind == SectionKind::I) {
            const double ho = s.d - s.tf;
            const double fbr = 0.02 * mMax / ho;
            const double sbr = 10.0 * gammaR * mMax / (lbc * ho);
            c.lines.push_back(fmt("Viga (4.12.3.3), M_Sd = %.1f kN*m, ho = %.0f mm, Lbb = %.0f mm, Cd = 1,0: F_br = 0,02 M_Sd Cd/ho = %.2f kN; S_br = 10 gr M_Sd Cd/(Lbb ho) = %.3f kN/mm "
                                  "(Cd = 2,0 para a contencao proxima ao ponto de inflexao em curvatura reversa)", mMax / kKnm, ho, lbc, fbr / kKn, sbr / kKn));
        }
        if (!c.lines.empty()) {
            c.lines.push_back("As contencoes adotadas (travessas, correntes, contraventamentos) devem ter resistencia e rigidez de calculo no minimo iguais a estes valores; nao e verificado pelo programa.");
            out.checks.push_back(c);
        }
    }

    std::sort(out.checks.begin(), out.checks.end(), [](const CheckResult &a, const CheckResult &b) { return a.ratio > b.ratio; });

    if (cold && (s.kind == SectionKind::ColdChannel || s.kind == SectionKind::ColdLippedChannel || s.kind == SectionKind::ColdZ || s.kind == SectionKind::ColdHat)) {
        // Reacao nas extremidades (cortante na alma): esmagamento da alma, Anexo D da NBR 14762, para um apoio de 50 mm
        double reaction = 0.0;
        for (const ComboEffects &combo : in.combos) {
            if (!combo.stations.empty()) {
                reaction = std::max({reaction, std::abs(combo.stations.front().e.Vy), std::abs(combo.stations.back().e.Vy)});
            }
        }
        if (reaction > 0.0 && in.bearingLength > 0.0) {
            // comprimento de apoio informado: verificacao efetiva da reacao de extremidade (Anexo D)
            const Nbr14762::WebCripplingData wc = Nbr14762::webCrippling(s, in.steel, true, std::max(in.bearingLength, 20.0), o14);
            if (wc.covered && wc.FRd > 0.0) {
                CheckResult c;
                c.id = "web_crippling";
                c.title = "Esmagamento da alma (reacao de extremidade)";
                c.clause = clauseTag + ", Anexo D";
                c.demand = reaction / kKn;
                c.resistance = wc.FRd / kKn;
                c.unit = "kN";
                c.ratio = reaction / wc.FRd;
                c.lines = wc.lines;
                c.lines.push_back(fmt("R_Sd = %.1f kN <= F_Rd = %.1f kN (c = %.0f mm)", reaction / kKn, wc.FRd / kKn, std::max(in.bearingLength, 20.0)));
                out.checks.push_back(c);
                if (c.ratio > out.maxRatio) {
                    out.maxRatio = c.ratio;
                    out.governingCheck = c.id;
                    out.governingCombo.clear();
                }
                std::sort(out.checks.begin(), out.checks.end(), [](const CheckResult &a, const CheckResult &b) { return a.ratio > b.ratio; });
            }
        } else if (reaction > 0.0) {
            const Nbr14762::WebCripplingData wc = Nbr14762::webCrippling(s, in.steel, true, 50.0, o14);
            if (wc.covered) {
                out.warnings.push_back(fmt("Esmagamento da alma (NBR 14762, Anexo D) nao e verificado automaticamente (o comprimento de apoio e desconhecido): "
                                           "para apoio de 50 mm na extremidade, F_Rd = %.1f kN (mesas nao conectadas) %s reacao maxima de %.1f kN.",
                                           wc.FRd / kKn, reaction > wc.FRd ? "E MENOR QUE a" : "contra a", reaction / kKn));
            }
        }
    }
    if (anyTorsionIgnored) {
        out.warnings.push_back("Ha momento de torcao em perfil sem verificacao de torcao disponivel (composto ou secao nao tratada).");
    }
    if (anyOpenWithoutWarping) {
        out.warnings.push_back("Torcao em perfil aberto verificada so por Saint-Venant: as tensoes de empenamento foram desprezadas. Ative \"Torcao com empenamento\" nos parametros de dimensionamento para considera-las.");
    }
    if (out.checks.empty()) {
        out.warnings.push_back("Sem esforcos nas combinacoes selecionadas: execute a analise e selecione combinacoes ultimas.");
    }
    return out;
}

} // namespace Structura::Design
