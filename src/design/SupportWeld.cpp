#include "design/SupportWeld.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Structura::Design::Connections {

namespace {

std::string fmt(const char *format, double a = 0.0, double b = 0.0, double c = 0.0, double d = 0.0, double e = 0.0, double f = 0.0)
{
    char buffer[512];
    std::snprintf(buffer, sizeof buffer, format, a, b, c, d, e, f);
    return buffer;
}

Check make(const std::string &name, const std::string &clause, double demand, double resistance, const std::string &unit)
{
    Check c;
    c.name = name;
    c.clause = clause;
    c.demand = demand;
    c.resistance = resistance;
    c.unit = unit;
    return c;
}

} // namespace

double minimumFilletLeg(double t)
{
    if (t <= 6.35) {
        return 3.0;
    }
    if (t <= 12.5) {
        return 5.0;
    }
    if (t <= 19.0) {
        return 6.0;
    }
    return 8.0;
}

double maximumFilletLeg(double t)
{
    return t < 6.35 ? t : t - 1.5;
}

std::vector<Check> checkSupportWeld(const SupportWeldInput &in)
{
    std::vector<Check> checks;
    const double d = in.depth, bf = in.flangeWidth, tf = in.flangeThickness, tw = in.webThickness;
    const double lever = std::max(d - tf, 1.0);
    const double flangeForce = std::abs(in.moment) / lever + std::abs(in.axial) / 2.0;
    const double webHeight = std::max(d - 2.0 * tf, 1.0);
    const double thinnestFlange = in.supportThickness > 0.0 ? std::min(tf, in.supportThickness) : tf;

    // ---- mesas
    if (in.flangeFullPenetration) {
        const double rd = bf * tf * in.beamFy / in.gammaA1;
        Check c = make("Solda viga-apoio - mesas (penetracao total)", "6.2.4", flangeForce / 1000.0, rd / 1000.0, "kN");
        c.lines.push_back(fmt("Solda de penetracao total: a resistencia e a do metal-base da mesa da viga (6.2.4): bf tf fy / ga1 = %.0f x %.1f x %.0f / %.2f = %.2f kN", bf, tf, in.beamFy, in.gammaA1, rd / 1000.0));
        c.lines.push_back(fmt("F = M/(d - tf) + N/2 = %.2f kN.m / %.3f m + %.2f kN / 2 = %.2f kN (a mesa tracionada governa; a comprimida e atendida pela mesma solda)", std::abs(in.moment) / 1e6, lever / 1000.0,
                              std::abs(in.axial) / 1000.0, flangeForce / 1000.0));
        checks.push_back(c);
    } else {
        const double length = std::max(2.0 * bf - tw, bf);   // face externa + as duas faces internas da mesa
        const double te = 0.707 * in.flangeLeg;
        const double rdWeld = 0.60 * te * length * in.fw * 1.5 / in.gammaW2;   // filetes transversais (theta = 90 graus: 1,0 + 0,5 sen^1,5 theta = 1,5)
        const double rdBase = std::min(0.60 * tf * length * in.beamFy / in.gammaA1, 0.60 * tf * length * in.beamFu / in.gammaA2);
        const double rd = std::min(rdWeld, rdBase);
        Check c = make("Solda viga-apoio - mesas (filete)", "6.2.5 e Tabela 9", flangeForce / 1000.0, rd / 1000.0, "kN");
        c.lines.push_back(fmt("Filetes de %.1f mm (te = %.2f mm) na face externa e nas duas faces internas da mesa: L = 2 bf - tw = %.0f mm", in.flangeLeg, te, length));
        c.lines.push_back(fmt("Fw,Rd = 0,60 te L fw (1,0 + 0,50 sen^1,5 90) / gw2 = 0,60 x %.2f x %.0f x %.0f x 1,5 / %.2f = %.2f kN; metal-base = %.2f kN", te, length, in.fw, in.gammaW2, rdWeld / 1000.0,
                              rdBase / 1000.0));
        c.lines.push_back(fmt("F = M/(d - tf) + N/2 = %.2f kN", flangeForce / 1000.0));
        checks.push_back(c);
        const double minLeg = minimumFilletLeg(thinnestFlange), maxLeg = maximumFilletLeg(tf);
        Check lo = make("Solda viga-apoio - perna minima das mesas", "Tabela 11", in.flangeLeg, minLeg, "mm");
        lo.minimumRule = true;
        lo.lines.push_back(fmt("perna adotada %.1f mm >= minima %.1f mm (parte mais fina = %.1f mm)", in.flangeLeg, minLeg, thinnestFlange));
        checks.push_back(lo);
        Check hi = make("Solda viga-apoio - perna maxima das mesas", "6.2.5.2", in.flangeLeg, maxLeg, "mm");
        hi.rangeRule = true;
        hi.lines.push_back(fmt("perna adotada %.1f mm <= maxima %.1f mm na borda da mesa (tf = %.1f mm)", in.flangeLeg, maxLeg, tf));
        checks.push_back(hi);
    }

    // ---- alma: dois filetes longitudinais ao cortante
    {
        const double te = 0.707 * in.webLeg;
        const double fv = std::abs(in.shear) / (2.0 * webHeight);   // N/mm por filete
        const double perLength = 0.60 * te * in.fw / in.gammaW2;
        Check c = make("Solda viga-apoio - alma (filetes)", "6.2.5", fv, perLength, "N/mm");
        c.lines.push_back(fmt("2 filetes de %.1f mm (te = %.2f mm) x %.0f mm; fv = V/(2 hw) = %.1f kN/(2 x %.0f mm) = %.2f N/mm", in.webLeg, te, webHeight, std::abs(in.shear) / 1000.0, webHeight, fv));
        c.lines.push_back(fmt("fv <= 0,60 te fw / gw2 = 0,60 x %.2f x %.0f / %.2f = %.2f N/mm", te, in.fw, in.gammaW2, perLength));
        checks.push_back(c);
        const double base = std::min(0.60 * tw * in.beamFy / in.gammaA1, 0.60 * tw * in.beamFu / in.gammaA2);
        Check b = make("Solda viga-apoio - alma (metal-base)", "6.2.5 e 5.4.3", 2.0 * fv, base, "N/mm");
        b.lines.push_back(fmt("2 fv = %.2f N/mm <= min(0,6 tw fy/ga1 ; 0,6 tw fu/ga2) = %.2f N/mm (cisalhamento do metal-base da alma)", 2.0 * fv, base));
        checks.push_back(b);
        const double minLeg = minimumFilletLeg(in.supportThickness > 0.0 ? std::min(tw, in.supportThickness) : tw), maxLeg = maximumFilletLeg(tw);
        Check lo = make("Solda viga-apoio - perna minima da alma", "Tabela 11", in.webLeg, minLeg, "mm");
        lo.minimumRule = true;
        lo.lines.push_back(fmt("perna adotada %.1f mm >= minima %.1f mm", in.webLeg, minLeg));
        checks.push_back(lo);
        Check hi = make("Solda viga-apoio - perna maxima da alma", "6.2.5.2", in.webLeg, maxLeg, "mm");
        hi.rangeRule = true;
        hi.lines.push_back(fmt("perna adotada %.1f mm <= maxima %.1f mm na borda da alma (tw = %.1f mm)", in.webLeg, maxLeg, tw));
        checks.push_back(hi);
    }

    // ---- metal-base do apoio (so se informado)
    if (in.supportThickness > 0.0) {
        const double fv = std::abs(in.shear) / (2.0 * webHeight);
        const double base = std::min(0.60 * in.supportThickness * in.supportFy / in.gammaA1, 0.60 * in.supportThickness * in.supportFu / in.gammaA2);
        Check c = make("Solda viga-apoio - metal-base do apoio (alma da viga)", "6.2.5", 2.0 * fv, base, "N/mm");
        c.lines.push_back(fmt("2 fv = %.2f N/mm <= min(0,6 t fy/ga1 ; 0,6 t fu/ga2) do apoio (t = %.1f mm) = %.2f N/mm", 2.0 * fv, in.supportThickness, base));
        checks.push_back(c);
    }
    return checks;
}

} // namespace Structura::Design::Connections
