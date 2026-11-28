#include "design/TStub.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace Structura::Design::TStub {

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string fmt(const char *format, double a = 0, double b = 0, double c = 0, double d = 0, double e = 0, double f = 0)
{
    char buf[512];
    std::snprintf(buf, sizeof buf, format, a, b, c, d, e, f);
    return buf;
}

bool positive(double v)
{
    return std::isfinite(v) && v > 0.0;
}

} // namespace

double boltStressArea(double db)
{
    static const std::map<int, double> table {{12, 84.3}, {14, 115.0}, {16, 157.0}, {18, 192.0}, {20, 245.0}, {22, 303.0}, {24, 353.0}, {27, 459.0}, {30, 561.0}, {33, 694.0}, {36, 817.0}};
    const auto it = table.find(static_cast<int>(std::lround(db)));
    return it == table.end() ? 0.0 : it->second;
}

HeadWidth boltHeadWidth(double db)
{
    static const std::map<int, double> acrossFlats {{12, 18.0}, {16, 24.0}, {20, 30.0}, {22, 34.0}, {24, 36.0}, {27, 41.0}, {30, 46.0}, {36, 55.0}};
    const auto it = acrossFlats.find(static_cast<int>(std::lround(db)));
    HeadWidth w;
    w.tabulated = it != acrossFlats.end();
    w.acrossPoints = (w.tabulated ? it->second : 1.5 * db) * 1.1547005;
    return w;
}

double boltHeadAcrossPoints(double db)
{
    return boltHeadWidth(db).acrossPoints;
}

double boltTensionResistance(double fub, double stressArea, const EnCoefficients &c)
{
    return c.k2 * fub * stressArea / c.gammaM2;
}

Modes resistance(const Geometry &g, const EffectiveLength &leff, double tf, const Material &steel, const Bolts &bolts, const EnCoefficients &c, Method method,
                 const std::string &label)
{
    Modes out;
    out.leff = leff;
    if (!positive(g.m) || !positive(g.e) || !positive(tf) || !positive(steel.fy) || !positive(leff.circular) || !positive(leff.nonCircular) || !positive(bolts.ftRd) || bolts.count < 1 ||
        !positive(c.gammaM0) || !positive(c.gammaM2) || !positive(c.k2)) {
        out.error = "T-stub: dados invalidos (m, e, tf, fy, comprimentos efetivos, Ft,Rd ou coeficientes nao positivos)";
        return out;
    }
    const double emin = g.emin > 0.0 ? g.emin : g.e;
    out.n = std::min(emin, 1.25 * g.m);
    out.mpl1 = 0.25 * leff.mode1() * tf * tf * steel.fy / c.gammaM0;
    out.mpl2 = 0.25 * leff.mode2() * tf * tf * steel.fy / c.gammaM0;
    const double sumFt = bolts.total();
    if (method == Method::One) {
        out.f1 = 4.0 * out.mpl1 / g.m;
    } else {
        const double den = 2.0 * g.m * out.n - g.ew * (g.m + out.n);
        if (!(den > 0.0)) {
            out.error = "T-stub: denominador do modo 1 (metodo 2) nao positivo; verifique m, n e ew";
            return out;
        }
        out.f1 = (8.0 * out.n - 2.0 * g.ew) * out.mpl1 / den;
    }
    out.f2 = (2.0 * out.mpl2 + out.n * sumFt) / (g.m + out.n);
    out.f3 = sumFt;
    out.resistance = std::min({out.f1, out.f2, out.f3});
    out.governing = out.resistance == out.f1 ? 1 : (out.resistance == out.f2 ? 2 : 3);
    out.valid = true;
    out.lines.push_back(label + fmt(": m = %.2f mm; e = %.2f mm; n = min(emin ; 1,25 m) = %.2f mm", g.m, g.e, out.n));
    out.lines.push_back(fmt("  leff,1 = min(cp %.1f ; nc %.1f) = %.1f mm; leff,2 = nc = %.1f mm", leff.circular, leff.nonCircular, leff.mode1(), leff.mode2()) + (leff.source.empty() ? "" : " (" + leff.source + ")"));
    out.lines.push_back(fmt("  Mpl,1,Rd = 0,25 leff,1 tf^2 fy/gM0 = %.0f N.mm; Mpl,2,Rd = %.0f N.mm (fy = %.0f MPa; tf = %.2f mm; gM0 = %.2f)", out.mpl1, out.mpl2, steel.fy, tf, c.gammaM0));
    if (method == Method::One) {
        out.lines.push_back(fmt("  Modo 1 (metodo 1) = 4 Mpl,1/m = %.1f kN", out.f1 / 1e3));
    } else {
        out.lines.push_back(fmt("  Modo 1 (metodo 2) = (8 n - 2 ew) Mpl,1 / [2 m n - ew (m + n)] = %.1f kN (ew = %.2f mm)", out.f1 / 1e3, g.ew));
    }
    out.lines.push_back(fmt("  Modo 2 = (2 Mpl,2 + n SFt,Rd)/(m + n) = %.1f kN; Modo 3 = SFt,Rd = %.1f kN (%.0f parafusos x %.1f kN)", out.f2 / 1e3, out.f3 / 1e3, static_cast<double>(bolts.count), bolts.ftRd / 1e3));
    out.lines.push_back(fmt("  FT,Rd = min(modos) = %.1f kN: modo %.0f governa", out.resistance / 1e3, static_cast<double>(out.governing)));
    return out;
}

EffectiveLength columnFlangeRow(double m, double e)
{
    return {2.0 * kPi * m, 4.0 * m + 1.25 * e, "EN 1993-1-8, Tabela 6.4, mesa nao enrijecida, fileira isolada"};
}

EffectiveLength columnFlangeGroup(double m, double e, const std::vector<double> &spacings)
{
    const int n = static_cast<int>(spacings.size()) + 1;
    EffectiveLength out;
    out.source = "EN 1993-1-8, Tabela 6.4, mesa nao enrijecida, grupo de fileiras";
    for (int i = 0; i < n; ++i) {
        if (i == 0 || i == n - 1) {
            const double p = i == 0 ? spacings.front() : spacings.back();
            out.circular += kPi * m + p;
            out.nonCircular += 2.0 * m + 0.625 * e + 0.5 * p;
        } else {
            const double p = 0.5 * (spacings[static_cast<std::size_t>(i) - 1] + spacings[static_cast<std::size_t>(i)]);
            out.circular += 2.0 * p;
            out.nonCircular += p;
        }
    }
    return out;
}

EffectiveLength endPlateExtensionRow(double mx, double ex, double e, double w, double bp)
{
    EffectiveLength out;
    out.circular = std::min({2.0 * kPi * mx, kPi * mx + w, kPi * mx + 2.0 * e});
    out.nonCircular = std::min({4.0 * mx + 1.25 * ex, e + 2.0 * mx + 0.625 * ex, 0.5 * bp, 0.5 * w + 2.0 * mx + 0.625 * ex});
    out.source = "EN 1993-1-8, Tabela 6.6, chapa de extremidade, fileira na extensao";
    return out;
}

ColumnFlangeResult columnFlange(const ColumnFlangeInput &in, const EnCoefficients &c, Method method)
{
    ColumnFlangeResult out;
    if (in.rows < 1 || static_cast<int>(in.rowSpacings.size()) != in.rows - 1 || !positive(in.m) || !positive(in.e) || !positive(in.tf) || !positive(in.fy) || !positive(in.boltFtRd)) {
        out.error = "Mesa do pilar: dados invalidos para o T-stub (m, e, tf, fy, Ft,Rd ou espacamentos entre fileiras)";
        return out;
    }
    for (double p : in.rowSpacings) {
        if (!positive(p)) {
            out.error = "Mesa do pilar: espacamento entre fileiras nao positivo";
            return out;
        }
    }
    Geometry g;
    g.m = in.m;
    g.e = in.e;
    g.emin = in.emin;
    g.ew = in.ew;
    const Material steel {in.fy};
    // fileiras isoladas
    for (int r = 0; r < in.rows; ++r) {
        RowResult row;
        row.index = r + 1;
        row.isolated = resistance(g, columnFlangeRow(in.m, in.e), in.tf, steel, {in.boltFtRd, 2}, c, method, "Fileira " + std::to_string(r + 1) + " (isolada)");
        if (!row.isolated.valid) {
            out.error = row.isolated.error;
            return out;
        }
        out.rows.push_back(row);
    }
    // grupos de fileiras consecutivas (j..r)
    std::map<std::pair<int, int>, Modes> groups;
    for (int j = 0; j < in.rows; ++j) {
        for (int r = j + 1; r < in.rows; ++r) {
            const std::vector<double> sp(in.rowSpacings.begin() + j, in.rowSpacings.begin() + r);
            const std::string label = "Grupo " + std::to_string(j + 1) + "-" + std::to_string(r + 1);
            Modes gm = resistance(g, columnFlangeGroup(in.m, in.e, sp), in.tf, steel, {in.boltFtRd, 2 * (r - j + 1)}, c, method, label);
            if (!gm.valid) {
                out.error = gm.error;
                return out;
            }
            groups[{j, r}] = gm;
            out.groups.emplace_back(std::to_string(j + 1) + "-" + std::to_string(r + 1), gm);
        }
    }
    // distribuicao efetiva (procedimento do SCI P398): Ft_r = min(R_r ; min_j [R_grupo(j..r) - soma de F_j..F_{r-1}])
    for (int r = 0; r < in.rows; ++r) {
        RowResult &row = out.rows[static_cast<std::size_t>(r)];
        double best = row.isolated.resistance;
        row.governingGroup = "isolada";
        for (int j = 0; j < r; ++j) {
            double previous = 0.0;
            for (int k = j; k < r; ++k) {
                previous += out.rows[static_cast<std::size_t>(k)].effective;
            }
            const double limit = groups[{j, r}].resistance - previous;
            if (limit < best) {
                best = limit;
                row.governingGroup = "grupo " + std::to_string(j + 1) + "-" + std::to_string(r + 1);
            }
        }
        row.effective = std::max(0.0, best);
        out.total += row.effective;
    }
    out.valid = true;
    for (const RowResult &row : out.rows) {
        for (const std::string &line : row.isolated.lines) {
            out.lines.push_back(line);
        }
    }
    for (const auto &g2 : out.groups) {
        for (const std::string &line : g2.second.lines) {
            out.lines.push_back(line);
        }
    }
    for (const RowResult &row : out.rows) {
        out.lines.push_back(fmt("Fileira %.0f: resistencia efetiva = %.1f kN (", static_cast<double>(row.index), row.effective / 1e3) + row.governingGroup + ")");
    }
    out.lines.push_back(fmt("Resistencia total da mesa do pilar a tracao (soma das fileiras) = %.1f kN", out.total / 1e3));
    return out;
}

} // namespace Structura::Design::TStub
