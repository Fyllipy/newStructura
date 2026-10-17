#include "design/BarDesign.h"

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
    c.ratio = resistance > 0.0 ? demand / resistance : (demand > 0.0 ? 1e9 : 0.0);
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
    out.summaryLines.push_back(fmt("Comprimento L = %.0f mm; Lx = %.0f mm, Ly = %.0f mm, Lz = %.0f mm; Lb = %.0f mm (%d trecho(s))", L, len.Lx, len.Ly, len.Lz, lb,
                                   segments));

    // ---- Resistencias independentes dos esforcos ----
    Nbr8800::TensionData tens = cold ? Nbr14762::tension(s, in.steel, o14) : Nbr8800::tension(s, in.steel, o8);
    Nbr8800::CompressionData comp = cold ? Nbr14762::compression(s, in.steel, len, o14) : Nbr8800::compression(s, in.steel, len, o8);

    const double rMin = std::min(s.g.rx, s.g.ry);
    const double lambdaTension = L / rMin;

    Collector col;

    // Placa de torcao em tubulares
    Nbr8800::TorsionData torsion;
    if (!cold) {
        torsion = Nbr8800::torsion(s, in.steel, L, o8);
    }

    bool anyTorsionIgnored = false;
    bool slendernessWarned = false;

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
            const double ma = combo.stations.front().e.*member;
            const double mb = combo.stations.back().e.*member;
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
        const double nexPlane = sq(kPi) * E * s.g.Ix / sq(L);
        const double neyPlane = sq(kPi) * E * s.g.Iy / sq(L);

        // Cb por trecho destravado
        std::vector<double> cbSegment(segments, 1.0);
        if (!in.cbEqualsOne && lb > 0.0) {
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

        for (const StationEffects &st : combo.stations) {
            Effects e = st.e;
            const std::string where = fmt("combinacao %s, x = %.0f mm", combo.name.c_str(), st.x);

            // Amplificacao dos momentos (Anexo C)
            double b1x = 1.0;
            double b1y = 1.0;
            if (in.amplifyB1 && e.N < 0.0) {
                const double nc = -e.N;
                b1x = nc >= nexPlane ? 1e6 : std::max(1.0, cmX / (1.0 - nc / nexPlane));
                b1y = nc >= neyPlane ? 1e6 : std::max(1.0, cmY / (1.0 - nc / neyPlane));
            }
            const double mxSd = e.Mx * b1x * in.b2 * combo.b2;
            const double mySd = e.My * b1y * in.b2 * combo.b2;
            const double absMx = std::abs(mxSd);
            const double absMy = std::abs(mySd);

            // --- tracao ---
            double nRd = comp.NcRd;
            double nSd = 0.0;
            if (e.N > 0.0) {
                nRd = tens.NtRd;
                nSd = e.N;
                std::vector<std::string> lines = tens.lines;
                lines.push_back(fmt("N_t,Sd = %s -> N_t,Sd / N_t,Rd = %.3f  (%s)", fmt("%.2f kN", nSd / kKn).c_str(), nSd / tens.NtRd, where.c_str()));
                col.offer(makeCheck("tension", "Tracao axial", cold ? clauseTag + ", 9.6" : clauseTag + ", 5.2", nSd, tens.NtRd, "kN", kKn, lines), combo.name, st.x);
            } else if (e.N < 0.0) {
                nSd = -e.N;
                std::vector<std::string> lines = comp.lines;
                lines.push_back(fmt("N_c,Sd = %.2f kN -> N_c,Sd / N_c,Rd = %.3f  (%s)", nSd / kKn, nSd / comp.NcRd, where.c_str()));
                col.offer(makeCheck("compression", "Compressao axial", cold ? clauseTag + ", 9.7" : clauseTag + ", 5.3", nSd, comp.NcRd, "kN", kKn, lines), combo.name, st.x);
            }

            // --- esbeltez ---
            if (e.N < 0.0 && in.slendernessGoverns) {
                const double limit = cold ? o14.maxSlendernessCompression : o8.maxSlendernessCompression;
                CheckResult c;
                c.id = "slenderness";
                c.title = "Limitacao de esbeltez (compressao)";
                c.clause = cold ? clauseTag + ", 9.7.4" : clauseTag + ", 5.3.7";
                c.demand = comp.slenderness;
                c.resistance = limit;
                c.ratio = comp.slenderness / limit;
                c.unit = "";
                c.lines.push_back(fmt("KL/r maximo = %.1f; limite = %.0f (%s)", comp.slenderness, limit, where.c_str()));
                col.offer(c, combo.name, st.x);
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

            // --- flexao ---
            double mxRd = 0.0;
            double myRd = 0.0;
            bool flexCovered = true;
            if (absMx > 1e-9 || absMy > 1e-9) {
                const double mxMax = absMx;
                const double myMax = absMy;
                if (mxMax > 1e-9) {
                    const auto &f = flexOf(Axis::X, mxSd > 0.0, lb, cb);
                    flexCovered = flexCovered && f.covered;
                    if (f.covered) {
                        mxRd = f.MRd;
                        std::vector<std::string> lines = f.lines;
                        lines.push_back(fmt("M_x,Sd = %.2f kN*m (B1 = %.3f) -> M_x,Sd / M_x,Rd = %.3f; governa: %s  (%s)", mxMax / kKnm, b1x, mxMax / f.MRd, f.governing.c_str(), where.c_str()));
                        col.offer(makeCheck("flexure_x", "Flexao em torno do eixo X (maior inercia)", cold ? clauseTag + ", 9.8.2" : clauseTag + ", 5.4.2 e Anexo D", mxMax, f.MRd, "kN*m", kKnm, lines), combo.name, st.x);
                    }
                }
                if (myMax > 1e-9) {
                    const auto &f = flexOf(Axis::Y, mySd > 0.0, 0.0, 1.0);
                    flexCovered = flexCovered && f.covered;
                    if (f.covered) {
                        myRd = f.MRd;
                        std::vector<std::string> lines = f.lines;
                        lines.push_back(fmt("M_y,Sd = %.2f kN*m (B1 = %.3f) -> M_y,Sd / M_y,Rd = %.3f; governa: %s  (%s)", myMax / kKnm, b1y, myMax / f.MRd, f.governing.c_str(), where.c_str()));
                        col.offer(makeCheck("flexure_y", "Flexao em torno do eixo Y (menor inercia)", cold ? clauseTag + ", 9.8.2" : clauseTag + ", 5.4.2 e Anexo D", myMax, f.MRd, "kN*m", kKnm, lines), combo.name, st.x);
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
                    std::vector<std::string> lines = v.lines;
                    lines.push_back(fmt("V_y,Sd = %.2f kN -> V_y,Sd / V_Rd = %.3f  (%s)", std::abs(e.Vy) / kKn, std::abs(e.Vy) / v.VRd, where.c_str()));
                    col.offer(makeCheck("shear_y", "Cortante na direcao da altura (alma)", cold ? clauseTag + ", 9.8.3" : clauseTag + ", 5.4.3", std::abs(e.Vy), v.VRd, "kN", kKn, lines), combo.name, st.x);
                }
            }
            if (std::abs(e.Vz) > 1e-9) {
                const auto &v = shearOf(Axis::X);
                if (v.covered) {
                    vzRd = v.VRd;
                    std::vector<std::string> lines = v.lines;
                    lines.push_back(fmt("V_z,Sd = %.2f kN -> V_z,Sd / V_Rd = %.3f  (%s)", std::abs(e.Vz) / kKn, std::abs(e.Vz) / v.VRd, where.c_str()));
                    col.offer(makeCheck("shear_z", "Cortante na direcao da largura (mesas)", cold ? clauseTag + ", 9.8.3" : clauseTag + ", 5.4.3", std::abs(e.Vz), v.VRd, "kN", kKn, lines), combo.name, st.x);
                }
            }

            // --- interacao N + M ---
            if ((absMx > 1e-9 || absMy > 1e-9) && flexCovered && (mxRd > 0.0 || myRd > 0.0)) {
                const double mRatioX = (absMx > 1e-9 && mxRd > 0.0) ? absMx / mxRd : 0.0;
                const double mRatioY = (absMy > 1e-9 && myRd > 0.0) ? absMy / myRd : 0.0;
                const double nRatio = nRd > 0.0 ? nSd / nRd : 0.0;
                double ratio;
                std::string expr;
                if (cold) {
                    ratio = nRatio + mRatioX + mRatioY;
                    expr = fmt("NBR 14762, 9.9: N_Sd/N_Rd + Mx,Sd/Mx,Rd + My,Sd/My,Rd = %.3f + %.3f + %.3f = %.3f", nRatio, mRatioX, mRatioY, ratio);
                } else if (nRatio >= 0.2) {
                    ratio = nRatio + 8.0 / 9.0 * (mRatioX + mRatioY);
                    expr = fmt("NBR 8800, 5.5.1.2-a: N_Sd/N_Rd + 8/9 (Mx,Sd/Mx,Rd + My,Sd/My,Rd) = %.3f + 8/9 (%.3f + %.3f) = %.3f", nRatio, mRatioX, mRatioY, ratio);
                } else {
                    ratio = nRatio / 2.0 + (mRatioX + mRatioY);
                    expr = fmt("NBR 8800, 5.5.1.2-b: N_Sd/(2 N_Rd) + (Mx,Sd/Mx,Rd + My,Sd/My,Rd) = %.3f/2 + (%.3f + %.3f) = %.3f", nRatio, mRatioX, mRatioY, ratio);
                }
                CheckResult c;
                c.id = "interaction";
                c.title = e.N == 0.0 ? "Flexao composta (momentos)" : (e.N > 0.0 ? "Flexo-tracao" : "Flexo-compressao");
                c.clause = cold ? clauseTag + ", 9.9" : clauseTag + ", 5.5.1.2";
                c.demand = ratio;
                c.resistance = 1.0;
                c.ratio = ratio;
                c.lines.push_back(expr + fmt("  (%s)", where.c_str()));
                col.offer(c, combo.name, st.x);
            }

            // --- momento e cortante combinados (14762, 9.8.4) ---
            if (cold) {
                auto mv = [&](double m, double mrd, double v, double vrd, const char *id, const char *title) {
                    if (m > 1e-9 && mrd > 0.0 && v > 1e-9 && vrd > 0.0) {
                        CheckResult c;
                        c.id = id;
                        c.title = title;
                        c.clause = clauseTag + ", 9.8.4";
                        c.demand = sq(m / mrd) + sq(v / vrd);
                        c.resistance = 1.0;
                        c.ratio = c.demand;
                        c.lines.push_back(fmt("(M_Sd/M_Rd)^2 + (V_Sd/V_Rd)^2 = (%.3f)^2 + (%.3f)^2 = %.3f  (%s)", m / mrd, v / vrd, c.ratio, where.c_str()));
                        col.offer(c, combo.name, st.x);
                    }
                };
                mv(absMx, mxRd, std::abs(e.Vy), vyRd, "mv_x", "Momento fletor e forca cortante combinados (eixo X)");
                mv(absMy, myRd, std::abs(e.Vz), vzRd, "mv_y", "Momento fletor e forca cortante combinados (eixo Y)");
            }

            // --- torcao ---
            if (std::abs(e.T) > 1e-9) {
                if (torsion.covered) {
                    const double tRatio = std::abs(e.T) / torsion.TRd;
                    CheckResult c;
                    c.id = "torsion";
                    c.title = "Torcao";
                    c.clause = clauseTag + ", 5.5.2";
                    c.demand = std::abs(e.T) / kKnm;
                    c.resistance = torsion.TRd / kKnm;
                    c.unit = "kN*m";
                    double ratio = tRatio;
                    c.lines = torsion.lines;
                    if (tRatio > 0.20) {
                        const double nr = nRd > 0.0 ? nSd / nRd : 0.0;
                        const double mr = (mxRd > 0.0 ? absMx / mxRd : 0.0) + (myRd > 0.0 ? absMy / myRd : 0.0);
                        const double vr = (vyRd > 0.0 ? std::abs(e.Vy) / vyRd : 0.0) + (vzRd > 0.0 ? std::abs(e.Vz) / vzRd : 0.0);
                        ratio = (nr + mr) + sq(vr + tRatio);
                        c.lines.push_back(fmt("T_Sd/T_Rd = %.3f > 0,20: (N/NRd + M/MRd) + (V/VRd + T/TRd)^2 = (%.3f + %.3f) + (%.3f + %.3f)^2 = %.3f  (%s)", tRatio, nr, mr, vr, tRatio, ratio, where.c_str()));
                    } else {
                        c.lines.push_back(fmt("T_Sd/T_Rd = %.3f <= 0,20: efeito da torcao desprezado nas interacoes (5.5.2.2)  (%s)", tRatio, where.c_str()));
                    }
                    c.ratio = ratio;
                    col.offer(c, combo.name, st.x);
                } else if (std::abs(e.T) > 1e5) {
                    anyTorsionIgnored = true;
                }
            }
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
        out.warnings.push_back("Ha momento de torcao em perfil aberto/macico: a verificacao de torcao nao e coberta (requer analise de empenamento).");
    }
    if (out.checks.empty()) {
        out.warnings.push_back("Sem esforcos nas combinacoes selecionadas: execute a analise e selecione combinacoes ultimas.");
    }
    return out;
}

} // namespace Structura::Design
