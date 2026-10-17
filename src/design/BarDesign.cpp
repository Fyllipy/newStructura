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
    std::sort(out.checks.begin(), out.checks.end(), [](const CheckResult &a, const CheckResult &b) { return a.ratio > b.ratio; });

    if (cold && (s.kind == SectionKind::ColdChannel || s.kind == SectionKind::ColdLippedChannel || s.kind == SectionKind::ColdZ || s.kind == SectionKind::ColdHat)) {
        // Reacao nas extremidades (cortante na alma): esmagamento da alma, Anexo D da NBR 14762, para um apoio de 50 mm
        double reaction = 0.0;
        for (const ComboEffects &combo : in.combos) {
            if (!combo.stations.empty()) {
                reaction = std::max({reaction, std::abs(combo.stations.front().e.Vy), std::abs(combo.stations.back().e.Vy)});
            }
        }
        if (reaction > 0.0) {
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
