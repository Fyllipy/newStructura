#include "design/ColdFormedConnections.h"

#include "design/ConnectionHelpers.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design::Connections {

using detail::fmtn;
using detail::kSteelDensity;
using detail::ratioN;

namespace {

constexpr double kMaxThickness = 4.75;   // 10.2.1 e 10.3.1

double sq(double x) { return x * x; }

double holeDiameter(double d)
{
    return d < 12.5 ? d + 0.8 : d + 1.5;   // Tabela 15, furo-padrao
}

void applicabilityChecks(const ColdFormedInput &in, std::vector<Check> &out)
{
    const double tMin = std::min(in.t1, in.t2);
    Check c = detail::rangeMax("Espessura da parte mais fina", "NBR 14762:2010, 10.2.1 / 10.3.1", tMin, kMaxThickness, "mm");
    c.lines.push_back(fmtn("t = %.2f mm <= 4,75 mm; acima disso aplicam-se as exigencias da NBR 8800.", tMin));
    out.push_back(std::move(c));
}

void minimumForce(const ColdFormedInput &in, double &force, std::vector<Check> &out)
{
    if (in.memberResistance <= 0.0) {
        return;
    }
    const double minimum = 0.5 * in.memberResistance;
    Check c = detail::minimumRule("Forca minima de projeto da ligacao", "NBR 14762:2010, 10.1-a", std::max(in.force, minimum), minimum, "kN");
    c.demand /= 1000.0;
    c.resistance /= 1000.0;
    c.lines.push_back(fmtn("Ligacao dimensionada para max(F_Sd = %.1f kN ; 50%% da resistencia da barra = %.1f kN).", in.force / 1e3, minimum / 1e3));
    out.push_back(std::move(c));
    force = std::max(force, minimum);
}

void bolts(const ColdFormedInput &in, double force, std::vector<Check> &out)
{
    const int count = std::max(1, in.n) * std::max(1, in.lines);
    const double perBolt = force / count;
    const double dh = holeDiameter(in.d);
    const double ab = detail::kPi * sq(in.d) / 4.0;

    // 10.3.3.1 espacamentos minimos
    {
        Check c = detail::minimumRule("Distancia livre entre furos", "NBR 14762:2010, 10.3.3.1", std::min(in.pitch, in.lines > 1 ? in.gauge : in.pitch) - dh, 2.0 * in.d);
        c.lines.push_back(fmtn("Distancia livre = s - dh = %.1f mm >= 2 d = %.1f mm (dh = %.1f mm, Tab. 15).", c.demand, c.resistance, dh));
        if (in.n > 1 || in.lines > 1) {
            out.push_back(std::move(c));
        }
    }
    {
        const double edgeMin = std::min({in.end1, in.end2, in.edge}) - dh / 2.0;
        Check c = detail::minimumRule("Distancia do furo a borda", "NBR 14762:2010, 10.3.3.1", edgeMin, in.d);
        c.lines.push_back(fmtn("Borda do furo a extremidade = e - dh/2 = %.1f mm >= d = %.1f mm.", edgeMin, in.d));
        out.push_back(std::move(c));
    }

    // 10.3.7 cisalhamento do parafuso
    {
        const double fv = (in.thread ? 0.4 : 0.5) * ab * in.fub / 1.35 * std::max(1, in.planes);
        Check c = ratioN("Cisalhamento do parafuso", "NBR 14762:2010, 10.3.7", perBolt, fv);
        c.lines.push_back(fmtn("F_v,Rd = %.2f Ab fub / 1,35 x %d plano(s) = %.1f kN por parafuso (Ab = %.1f mm2); F_v,Sd = %.1f kN", in.thread ? 0.4 : 0.5, std::max(1, in.planes), fv / 1e3, ab,
                               perBolt / 1e3));
        out.push_back(std::move(c));
    }
    // 10.3.4 rasgamento e 10.3.5 pressao de contato, para o perfil (1) e para a chapa (2)
    for (int part = 1; part <= 2; ++part) {
        const double t = part == 1 ? in.t1 : in.t2;
        const double fu = part == 1 ? in.fu1 : in.fu2;
        const double e = part == 1 ? in.end1 : in.end2;
        const char *who = part == 1 ? "perfil" : "chapa";
        const double tear = t * e * fu / 1.45;
        Check c = ratioN(std::string("Rasgamento entre furo e borda (") + who + ")", "NBR 14762:2010, 10.3.4", perBolt, tear);
        c.lines.push_back(fmtn("F_Rd = t e fu / 1,45 = %.2f x %.1f x %.0f / 1,45 = %.1f kN; F_Sd por parafuso = %.1f kN", t, e, fu, tear / 1e3, perBolt / 1e3));
        out.push_back(std::move(c));
        const double alphaE = 0.183 * t + 1.53;
        const double bearing = alphaE * in.d * t * fu / 1.55;
        Check b = ratioN(std::string("Pressao de contato (") + who + ")", "NBR 14762:2010, 10.3.5", perBolt, bearing);
        b.lines.push_back(fmtn("alpha_e = 0,183 t + 1,53 = %.3f; F_Rd = alpha_e d t fu / 1,55 = %.1f kN; F_Sd por parafuso = %.1f kN", alphaE, bearing / 1e3, perBolt / 1e3));
        out.push_back(std::move(b));
    }
    // 10.3.6 tracao e 10.3.8 combinacao
    if (in.tension > 0.0) {
        const double ft = 0.75 * ab * in.fub / 1.35;
        const double tSd = in.tension / count;
        Check c = ratioN("Tracao do parafuso", "NBR 14762:2010, 10.3.6", tSd, ft);
        c.lines.push_back(fmtn("F_t,Rd = 0,75 Ab fub / 1,35 = %.1f kN; F_t,Sd = %.1f kN", ft / 1e3, tSd / 1e3));
        out.push_back(std::move(c));
        const double fv = (in.thread ? 0.4 : 0.5) * ab * in.fub / 1.35 * std::max(1, in.planes);
        Check k = detail::dimensionless("Tracao e cisalhamento combinados", "NBR 14762:2010, 10.3.8", sq(tSd / ft) + sq(perBolt / fv));
        k.lines.push_back(fmtn("(Ft,Sd/Ft,Rd)^2 + (Fv,Sd/Fv,Rd)^2 = %.3f^2 + %.3f^2 = %.3f (parafuso com qualificacao estrutural)", tSd / ft, perBolt / fv, k.demand));
        out.push_back(std::move(k));
    }
    // 10.4 colapso por rasgamento do perfil (tracao), na extremidade do perfil
    if (in.lines >= 1 && in.n >= 1) {
        const double lv = in.end1 + (in.n - 1) * in.pitch;
        const double lt = (in.lines - 1) * in.gauge;
        const double agv = 2.0 * lv * in.t1;
        const double anv = 2.0 * (lv - (in.n - 0.5) * dh) * in.t1;
        const double ant = std::max(0.0, lt - (in.lines - 1) * dh) * in.t1;
        const double a = (0.6 * in.fu1 * anv + in.fu1 * ant) / 1.65;
        const double b = (0.6 * in.fy1 * agv + in.fu1 * ant) / 1.65;
        const double rd = std::min(a, b);
        Check c = ratioN("Colapso por rasgamento do perfil", "NBR 14762:2010, 10.4", force, rd);
        c.lines.push_back(fmtn("Agv = %.0f, Anv = %.0f, Ant = %.0f mm2; F_Rd = min[(0,6 fu Anv + fu Ant) ; (0,6 fy Agv + fu Ant)]/1,65 = %.1f kN", agv, anv, ant, rd / 1e3));
        if (in.lines >= 2) {
            out.push_back(std::move(c));
        }
    }
}

void weld(const ColdFormedInput &in, double force, std::vector<Check> &out)
{
    const double t = std::min(in.t1, in.t2);
    const double w1 = std::min(in.leg, in.t1);   // juntas por sobreposicao: w1 <= t1
    const double tef = 0.7 * std::min(w1, in.leg);
    auto baseLong = [&](double L) {
        if (L <= 0.0) {
            return 0.0;
        }
        return (L / t < 25.0) ? (1.0 - 0.01 * L / t) * t * L * in.fu1 / 1.65 : 0.75 * t * L * in.fu1 / 2.00;
    };
    auto weldRupture = [&](double L) { return t > 2.5 ? 0.75 * tef * L * in.fw / 1.65 : 1e18; };
    const double fLong = std::min(baseLong(in.lengthLong), weldRupture(in.lengthLong));
    const double fTrans = in.lengthTrans > 0.0 ? std::min(t * in.lengthTrans * in.fu1 / 1.55, weldRupture(in.lengthTrans)) : 0.0;
    Check c = ratioN("Solda de filete (superficie plana)", "NBR 14762:2010, 10.2.3", force, fLong + fTrans);
    c.lines.push_back(fmtn("t = %.2f mm; perna adotada w = %.1f mm (w1 <= t1); t_ef = 0,7 w = %.2f mm", t, w1, tef));
    if (in.lengthLong > 0.0) {
        c.lines.push_back(fmtn("Filetes paralelos: L = %.0f mm, L/t = %.1f -> metal-base %.1f kN%s; ruptura da solda 0,75 t_ef L fw/1,65 = %.1f kN (t > 2,5 mm) -> %.1f kN", in.lengthLong,
                               in.lengthLong / t, baseLong(in.lengthLong) / 1e3, in.lengthLong / t < 25.0 ? " [(1 - 0,01 L/t) t L fu/1,65]" : " [0,75 t L fu/2,00]",
                               t > 2.5 ? weldRupture(in.lengthLong) / 1e3 : 0.0, fLong / 1e3));
    }
    if (in.lengthTrans > 0.0) {
        c.lines.push_back(fmtn("Filetes normais: L = %.0f mm -> metal-base t L fu/1,55 = %.1f kN; ruptura da solda = %.1f kN -> %.1f kN", in.lengthTrans,
                               t * in.lengthTrans * in.fu1 / 1.55 / 1e3, t > 2.5 ? weldRupture(in.lengthTrans) / 1e3 : 0.0, fTrans / 1e3));
    }
    c.lines.push_back(fmtn("F_Rd total = %.1f kN; F_Sd = %.1f kN", (fLong + fTrans) / 1e3, force / 1e3));
    out.push_back(std::move(c));
    const double base = in.lengthLong + in.lengthTrans;
    Check len = detail::minimumRule("Comprimento total de solda", "NBR 14762:2010, 10.2.3", base, 4.0 * in.leg, "mm");
    len.lines.push_back("Comprimento minimo adotado como 4 vezes a perna do filete (boa pratica; a NBR 14762 nao fixa o valor).");
    out.push_back(std::move(len));
}

void screws(const ColdFormedInput &in, double force, std::vector<Check> &out)
{
    const int count = std::max(1, in.n) * std::max(1, in.lines);
    const double perScrew = force / count;
    const double gamma = 2.0;
    {
        Check c = detail::rangeMax("Diametro do parafuso", "NBR 14762:2010, 10.5", in.d, 6.35, "mm");
        c.lines.push_back(fmtn("2,00 mm <= d = %.2f mm <= 6,35 mm", in.d));
        out.push_back(std::move(c));
    }
    {
        Check c = detail::minimumRule("Distancia entre centros", "NBR 14762:2010, 10.5.1", std::min(in.pitch, in.lines > 1 ? in.gauge : in.pitch), 3.0 * in.d);
        c.lines.push_back(fmtn("s = %.1f mm >= 3 d = %.1f mm", c.demand, c.resistance));
        if (count > 1) {
            out.push_back(std::move(c));
        }
        const double edgeMin = std::min({in.end1, in.end2, in.edge});
        Check e = detail::minimumRule("Distancia do centro a borda", "NBR 14762:2010, 10.5.1", edgeMin, 1.5 * in.d);
        e.lines.push_back(fmtn("e = %.1f mm >= 1,5 d = %.1f mm", edgeMin, 1.5 * in.d));
        out.push_back(std::move(e));
    }
    // 10.5.2.1 metal-base (t1 em contato com a cabeca; t2 o outro elemento)
    const double ratioT = in.t2 / in.t1;
    double fc;
    std::string formula;
    const double a = 4.2 * std::sqrt(in.t2 * in.t2 * in.t2 * in.d) * in.fu2;
    const double b1 = 2.7 * in.t1 * in.d * in.fu1;
    const double b2 = 2.7 * in.t2 * in.d * in.fu2;
    if (ratioT <= 1.0) {
        fc = std::min({a, b1, b2}) / gamma;
        formula = "t2/t1 <= 1,0: min[4,2 (t2^3 d)^0,5 fu2 ; 2,7 t1 d fu1 ; 2,7 t2 d fu2]/2,00";
    } else if (ratioT >= 2.5) {
        fc = std::min(b1, b2) / gamma;
        formula = "t2/t1 >= 2,5: min[2,7 t1 d fu1 ; 2,7 t2 d fu2]/2,00";
    } else {
        const double f10 = std::min({a, b1, b2});
        const double f25 = std::min(b1, b2);
        fc = (f10 + (f25 - f10) * (ratioT - 1.0) / 1.5) / gamma;
        formula = "1,0 < t2/t1 < 2,5: interpolacao linear entre os dois casos";
    }
    const double tear = std::min(in.t1 * in.end1 * in.fu1, in.t2 * in.end2 * in.fu2) / gamma;   // 10.5.2.1: F = t e fu / gamma
    {
        Check c = ratioN("Pressao no metal-base", "NBR 14762:2010, 10.5.2.1", perScrew, fc);
        c.lines.push_back(fmtn("%s: F_c,Rd = %.1f kN; F_Sd por parafuso = %.1f kN (t1 = %.2f, t2 = %.2f mm)", formula.c_str(), fc / 1e3, perScrew / 1e3, in.t1, in.t2));
        out.push_back(std::move(c));
        Check r = ratioN("Rasgamento entre furo e borda", "NBR 14762:2010, 10.5.2.1", perScrew, tear);
        r.lines.push_back(fmtn("F_c,Rd <= t e fu / 2,00 = %.1f kN", tear / 1e3));
        out.push_back(std::move(r));
    }
    if (in.fssRk > 0.0) {
        const double fss = in.fssRk / gamma;
        Check c = ratioN("Cisalhamento do parafuso", "NBR 14762:2010, 10.5.2.2", perScrew, fss);
        c.lines.push_back(fmtn("F_ss,Rd = F_ss,Rk / 2,00 = %.1f / 2,00 = %.1f kN (F_ss,Rk do fabricante)", in.fssRk / 1e3, fss / 1e3));
        out.push_back(std::move(c));
    } else {
        Check c = detail::dimensionless("Cisalhamento do parafuso", "NBR 14762:2010, 10.5.2.2", 0.0);
        c.lines.push_back("F_ss,Rk (resistencia nominal ao cisalhamento do parafuso) nao informado: informe o valor do fabricante para verificar este estado-limite.");
        out.push_back(std::move(c));
    }
    if (in.tension > 0.0) {
        const double tSd = in.tension / count;
        const double tc = std::min(in.t2, 1e9);
        const double pullOut = 0.85 * tc * in.d * in.fu2 / gamma;
        const double dw = in.washerDiameter > 0.0 ? in.washerDiameter : in.headDiameter;
        const double dwe = in.washerDiameter > 0.0 ? std::min(in.headDiameter + 2.0 * in.washerThickness + in.t1, in.washerDiameter) : std::min(in.headDiameter, 12.7);
        const double pullOver = 1.5 * in.t1 * std::min(dwe, 16.0) * in.fu1 / gamma;
        double ft = std::min(pullOut, pullOver);
        std::string txt = fmtn("F_a,Rd (arrancamento) = 0,85 tc d fu2 / 2,00 = %.1f kN; F_r,Rd (rasgamento do metal-base) = 1,5 t1 dwe fu1 / 2,00 = %.1f kN (dwe = %.1f mm)", pullOut / 1e3,
                               pullOver / 1e3, std::min(dwe, 16.0));
        if (in.ftsRk > 0.0) {
            ft = std::min(ft, in.ftsRk / gamma);
            txt += fmtn("; F_ts,Rd = %.1f kN (fabricante)", in.ftsRk / gamma / 1e3);
        }
        Check c = ratioN("Tracao do parafuso", "NBR 14762:2010, 10.5.3", tSd, ft);
        c.lines.push_back(txt);
        c.lines.push_back(fmtn("F_t,Sd = %.1f kN por parafuso", tSd / 1e3));
        out.push_back(std::move(c));
        // 10.5.4: cisalhamento + pull-over (validade: 0,72 <= t1 <= 1,13 mm, d = 5,5 ou 6,3 mm, dw <= 19 mm, fu1 <= 483 MPa, t2/t1 >= 2,5)
        const bool valid = in.t1 >= 0.72 && in.t1 <= 1.13 && (std::abs(in.d - 5.5) < 0.05 || std::abs(in.d - 6.3) < 0.05) && dw <= 19.0 && in.fu1 <= 483.0 && ratioT >= 2.5;
        if (valid) {
            const double fc0 = 2.7 * in.t1 * in.d * in.fu1 / 1.55;
            const double fr0 = 1.5 * in.t1 * dw * in.fu1 / 1.55;
            Check k = detail::dimensionless("Cisalhamento e arrancamento combinados", "NBR 14762:2010, 10.5.4", perScrew / fc0 + 0.71 * tSd / fr0);
            k.resistance = 1.10;
            k.lines.push_back(fmtn("Fv,Sd/Fc0,Rd + 0,71 Ft,Sd/Fr0,Rd = %.3f + 0,71 x %.3f = %.3f <= 1,10", perScrew / fc0, tSd / fr0, k.demand));
            out.push_back(std::move(k));
        } else {
            Check k = detail::dimensionless("Cisalhamento e arrancamento combinados", "NBR 14762:2010, 10.5.4", 0.0);
            k.lines.push_back("Expressao de interacao da 10.5.4 fora do campo de validade (0,72 <= t1 <= 1,13 mm; d = 5,5 ou 6,3 mm; dw <= 19 mm; fu1 <= 483 MPa; t2/t1 >= 2,5): "
                              "verifique cisalhamento e tracao separadamente (ou por ensaio, secao 11).");
            out.push_back(std::move(k));
        }
    }
}

} // namespace

std::vector<Check> checkColdFormed(const ColdFormedInput &in)
{
    std::vector<Check> out;
    applicabilityChecks(in, out);
    double force = in.force;
    minimumForce(in, force, out);
    switch (in.mode) {
    case ColdFormedInput::Weld: weld(in, force, out); break;
    case ColdFormedInput::Screws: screws(in, force, out); break;
    default: bolts(in, force, out); break;
    }
    return out;
}

std::vector<Part> partsColdFormed(const ColdFormedInput &in)
{
    std::vector<Part> parts;
    const int count = std::max(1, in.n) * std::max(1, in.lines);
    Part plate;
    const double length = in.end2 + (std::max(1, in.n) - 1) * in.pitch + in.end1 + 20.0;
    const double width = (std::max(1, in.lines) - 1) * in.gauge + 2.0 * in.edge;
    plate.description = fmtn("Chapa de ligacao t = %.2f mm, %.0f x %.0f mm", in.t2, length, width);
    plate.quantity = 1;
    plate.massEach = detail::plateMass(length, width, in.t2);
    parts.push_back(plate);
    if (in.mode == ColdFormedInput::Weld) {
        Part w;
        w.description = fmtn("Solda de filete, perna %.1f mm", in.leg);
        w.quantity = 1;
        w.unit = "m";
        w.length = in.lengthLong + in.lengthTrans;
        w.massEach = 0.0;
        parts.push_back(w);
    } else {
        Part b;
        b.description = fmtn(in.mode == ColdFormedInput::Screws ? "Parafuso autoatarraxante d = %.2f mm" : "Parafuso d = %.0f mm", in.d);
        b.quantity = count;
        b.massEach = detail::kPi * sq(in.d) / 4.0 * (in.t1 + in.t2 + 10.0) * 1e-9 * kSteelDensity;
        parts.push_back(b);
    }
    return parts;
}

} // namespace Structura::Design::Connections
