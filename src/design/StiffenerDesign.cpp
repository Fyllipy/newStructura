#include "design/StiffenerDesign.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace Structura::Design::Stiffeners {

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string fmt(const char *format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}

double chi(double lambda0)
{
    return lambda0 <= 1.5 ? std::pow(0.658, lambda0 * lambda0) : 0.877 / (lambda0 * lambda0);
}

StiffenerCheck make(const char *id, const char *title, const char *clause, double demand, double resistance, const char *unit, bool lowerLimit = false)
{
    StiffenerCheck c;
    c.id = id;
    c.title = title;
    c.clause = clause;
    c.demand = demand;
    c.resistance = resistance;
    c.unit = unit;
    if (lowerLimit) {
        c.ratio = demand > 1e-12 ? resistance / demand : 1e9;   // demand = valor disponivel, resistance = minimo exigido
    } else {
        c.ratio = resistance > 1e-12 ? demand / resistance : (demand > 0.0 ? 1e9 : 0.0);
    }
    return c;
}

} // namespace

double plateInertiaAboutWebPlane(int sides, double b, double t, double tw)
{
    const double inner = tw / 2.0;
    const double one = t * (std::pow(inner + b, 3) - std::pow(inner, 3)) / 3.0;
    return sides >= 2 ? 2.0 * one : one;
}

double webLocalResistance(const StiffenerInput &in, std::string *governing)
{
    const double k = std::max((in.d - in.h) / 2.0, in.tf);
    const double g = in.gammaA1;
    const double n = in.bearing;
    double best = 1e300;
    std::string name;
    auto take = [&](double value, const char *what) {
        if (value > 0.0 && value < best) {
            best = value;
            name = what;
        }
    };
    // escoamento local da alma (5.7.3)
    take(in.atEnd ? 1.10 * (2.5 * k + n) * in.fy * in.tw / g : 1.10 * (5.0 * k + n) * in.fy * in.tw / g, "escoamento local da alma (5.7.3)");
    if (in.tension) {
        double rd = 6.25 * in.tf * in.tf * in.fy / g;   // flexao local da mesa (5.7.2)
        if (in.atEnd) {
            rd *= 0.5;
        }
        take(rd, "flexao local da mesa (5.7.2)");
    } else {
        // enrugamento da alma (5.7.4)
        const double nd = n / in.d;
        const double ratioT = std::pow(in.tw / in.tf, 1.5);
        const double root = std::sqrt(in.E * in.fy * in.tf / in.tw);
        double rd;
        if (!in.atEnd) {
            rd = 0.66 * in.tw * in.tw * (1.0 + 3.0 * nd * ratioT) * root / g;
        } else if (nd <= 0.2) {
            rd = 0.33 * in.tw * in.tw * (1.0 + 3.0 * nd * ratioT) * root / g;
        } else {
            rd = 0.33 * in.tw * in.tw * (1.0 + (4.0 * nd - 0.2) * ratioT) * root / g;
        }
        take(rd, "enrugamento da alma (5.7.4)");
        // flambagem lateral da alma (5.7.5): mesa carregada com rotacao impedida nao suposta (conservador)
        if (in.unbracedLength > 0.0) {
            const double hh = std::max(in.h, 1.0);
            const double slender = (hh / in.tw) / (in.unbracedLength / in.bf);
            if (slender <= 1.70) {
                const double base = 16.0 * in.E * std::pow(in.tw, 3) * in.tf / (g * hh * hh);
                take(base * 0.37 * std::pow(slender, 3), "flambagem lateral da alma (5.7.5)");
            }
        }
    }
    if (governing) {
        *governing = name;
    }
    return best >= 1e299 ? 0.0 : best;
}

StiffenerResult checkStiffener(const StiffenerInput &in)
{
    StiffenerResult out;
    out.webResistance = webLocalResistance(in, &out.webGoverning);
    out.required = in.force > out.webResistance + 1e-9;
    const double root = std::sqrt(in.E / in.fy);

    // ---- geometria ----
    {
        const double lim = 0.56 * root;
        StiffenerCheck c = make("stiffener_slenderness", "Enrijecedor: relacao largura-espessura", "NBR 8800:2024, 5.4.3.1.3-b", in.b / in.t, lim, "");
        c.lines.push_back(fmt("b/t = %.1f / %.1f = %.2f <= 0,56 sqrt(E/fy) = %.2f", in.b, in.t, in.b / in.t, lim));
        out.checks.push_back(std::move(c));
    }
    {
        const double need = in.bf / 3.0;
        StiffenerCheck c = make("stiffener_width", "Enrijecedor: largura minima", "NBR 8800:2024, 5.7.9.5-a", in.b + in.tw, need, "mm", true);
        c.lines.push_back(fmt("b + tw = %.1f + %.1f = %.1f mm >= bf/3 = %.1f mm", in.b, in.tw, in.b + in.tw, need));
        out.checks.push_back(std::move(c));
    }
    {
        const double need = std::max(in.tf / 2.0, in.b / 16.0);
        StiffenerCheck c = make("stiffener_thickness", "Enrijecedor: espessura minima", "NBR 8800:2024, 5.7.9.5-b", in.t, need, "mm", true);
        c.lines.push_back(fmt("t = %.1f mm >= max(tf/2 = %.2f; b/16 = %.2f) = %.2f mm", in.t, in.tf / 2.0, in.b / 16.0, need));
        out.checks.push_back(std::move(c));
    }
    if (in.cut >= in.b) {
        StiffenerCheck c = make("stiffener_cut", "Enrijecedor: recorte de canto", "NBR 8800:2024, 5.4.3.1.3-a", in.cut, in.b, "mm");
        c.lines.push_back("O recorte de canto deve ser menor que a largura do enrijecedor.");
        out.checks.push_back(std::move(c));
    }

    const double plates = std::max(1, in.sides) * in.b * in.t;
    if (in.tension) {
        // 5.7.9.1: tracao na alma: area bruta do enrijecedor e area liquida efetiva (sem os recortes); soldado a mesa carregada e a alma
        const double ag = plates;
        const double ae = std::max(1, in.sides) * std::max(in.b - in.cut, 0.0) * in.t;
        const double yield = ag * in.fy / in.gammaA1;
        const double rupture = ae * in.fu / in.gammaA2;
        const double rd = std::min(yield, rupture);
        StiffenerCheck c = make("stiffener_tension", "Enrijecedor: tracao", "NBR 8800:2024, 5.7.9.1 e 5.2", in.force, rd, "kN");
        c.demand /= 1e3;
        c.resistance /= 1e3;
        c.ratio = rd > 0.0 ? in.force / rd : 1e9;
        c.lines.push_back(fmt("Ag = %.0f mm2 (fy Ag/ga1 = %.1f kN); Ae = %.0f mm2 (fu Ae/ga2 = %.1f kN); N_t,Rd = %.1f kN; F_Sd = %.1f kN", ag, yield / 1e3, ae, rupture / 1e3, rd / 1e3, in.force / 1e3));
        c.lines.push_back("Estender-se ao menos ate a metade da altura da alma e ser soldado a mesa carregada e a alma (5.7.9.1).");
        out.checks.push_back(std::move(c));
    } else {
        // 5.7.9.4: barra comprimida: enrijecedores + faixa de alma de 12 tw (extremidade) ou 25 tw (interna); comprimento de calculo 0,75 h
        const double strip = (in.interior ? 25.0 : 12.0) * in.tw;
        const double aStrip = strip * in.tw;
        double a = plates + aStrip;
        // centroide e inercia em relacao ao eixo no plano medio da alma (distancia medida a partir do plano medio, para o lado da chapa)
        double yBar = 0.0;
        double inertia;
        if (in.sides >= 2) {
            inertia = plateInertiaAboutWebPlane(2, in.b, in.t, in.tw) + strip * std::pow(in.tw, 3) / 12.0;
        } else {
            const double yPlate = in.tw / 2.0 + in.b / 2.0;
            yBar = plates * yPlate / a;
            inertia = in.t * std::pow(in.b, 3) / 12.0 + plates * std::pow(yPlate - yBar, 2) + strip * std::pow(in.tw, 3) / 12.0 + aStrip * yBar * yBar;
        }
        const double r = std::sqrt(inertia / a);
        const double length = 0.75 * in.h;
        const double ne = kPi * kPi * in.E * inertia / (length * length);
        const double lambda0 = std::sqrt(a * in.fy / ne);
        const double x = chi(lambda0);
        const double rd = x * a * in.fy / in.gammaA1;
        StiffenerCheck c = make("stiffener_buckling", "Enrijecedor: flambagem como barra comprimida", "NBR 8800:2024, 5.7.9.4 e 5.3", in.force, rd, "kN");
        c.demand /= 1e3;
        c.resistance /= 1e3;
        c.ratio = rd > 0.0 ? in.force / rd : 1e9;
        c.lines.push_back(fmt("Secao: %s de %.1f x %.1f mm + faixa de alma de %.0f tw = %.1f mm (%s): A = %.0f mm2, I = %.4g mm4, r = %.2f mm", in.sides >= 2 ? "par de chapas" : "chapa", in.b, in.t,
                              in.interior ? 25.0 : 12.0, strip, in.interior ? "secao interna" : "extremidade", a, inertia, r));
        c.lines.push_back(fmt("K L = 0,75 h = 0,75 x %.1f = %.1f mm; Ne = %.1f kN; lambda0 = sqrt(A fy/Ne) = %.3f; chi = %.3f; N_c,Rd = chi A fy/ga1 = %.1f kN; F_Sd = %.1f kN", in.h, length, ne / 1e3, lambda0,
                              x, rd / 1e3, in.force / 1e3));
        out.checks.push_back(std::move(c));
        if (in.fitted) {
            const double contact = std::max(1, in.sides) * std::max(in.b - in.cut, 0.0) * in.t;
            const double bearing = 1.8 * contact * in.fy / in.gammaA2;
            StiffenerCheck bc = make("stiffener_bearing", "Enrijecedor: esmagamento da extremidade ajustada", "NBR 8800:2024, 6.6.2", in.force, bearing, "kN");
            bc.demand /= 1e3;
            bc.resistance /= 1e3;
            bc.ratio = bearing > 0.0 ? in.force / bearing : 1e9;
            bc.lines.push_back(fmt("A = %d x (%.1f - %.1f) x %.1f = %.0f mm2; R_c,Rd = 1,8 A fy/ga2 = %.1f kN; F_Sd = %.1f kN", std::max(1, in.sides), in.b, in.cut, in.t, contact, bearing / 1e3, in.force / 1e3));
            out.checks.push_back(std::move(bc));
        } else {
            StiffenerCheck bc = make("stiffener_flange_weld", "Enrijecedor: extremidade soldada a mesa carregada", "NBR 8800:2024, 5.7.9.2", 0.0, 1.0, "");
            bc.lines.push_back("Extremidade soldada (nao ajustada) a mesa carregada: a solda deve suportar a diferenca entre a forca solicitante e a resistente da alma; verifique-a em Ligacoes (6.2).");
            out.checks.push_back(std::move(bc));
        }
    }

    // ---- solda enrijecedor-alma ----
    if (in.weldLeg > 0.0) {
        const double diff = std::max(in.force - out.webResistance, 0.0);
        const double length = std::max(in.h - 2.0 * in.cut, 0.0);
        const int welds = 2 * std::max(1, in.sides);   // filete dos dois lados de cada chapa
        const double perMm = 0.60 * in.fw * 0.707 * in.weldLeg / in.gammaW2;
        const double rd = perMm * length * welds;
        StiffenerCheck c = make("stiffener_web_weld", "Enrijecedor: solda a alma", "NBR 8800:2024, 5.7.9.2/5.7.9.3 e 6.2", diff, rd, "kN");
        c.demand /= 1e3;
        c.resistance /= 1e3;
        c.ratio = rd > 0.0 ? diff / rd : (diff > 0.0 ? 1e9 : 0.0);
        c.lines.push_back(fmt("Forca transferida a alma = F_Sd - F_Rd,alma = %.1f - %.1f = %.1f kN (alma: %s)", in.force / 1e3, out.webResistance / 1e3, diff / 1e3, out.webGoverning.c_str()));
        c.lines.push_back(fmt("%d filetes de perna %.1f mm e %.0f mm de comprimento: R = 0,60 fw (0,707 a) L / gw2 = %.1f kN", welds, in.weldLeg, length, rd / 1e3));
        out.checks.push_back(std::move(c));
    }
    for (const StiffenerCheck &c : out.checks) {
        out.maxRatio = std::max(out.maxRatio, c.ratio);
    }
    return out;
}

std::vector<StiffenerCheck> checkShearStiffener(const StiffenerInput &in, double a)
{
    std::vector<StiffenerCheck> out;
    if (a <= 0.0 || in.h <= 0.0) {
        return out;
    }
    {
        const double lim = 0.56 * std::sqrt(in.E / in.fy);
        StiffenerCheck c = make("shear_stiffener_slenderness", "Enrijecedor de cortante: largura-espessura", "NBR 8800:2024, 5.4.3.1.3-b", in.b / in.t, lim, "");
        c.lines.push_back(fmt("b/t = %.2f <= 0,56 sqrt(E/fy) = %.2f", in.b / in.t, lim));
        out.push_back(std::move(c));
    }
    {
        const double j = std::max(2.5 / std::pow(a / in.h, 2) - 2.0, 0.5);
        const double need = a * std::pow(in.tw, 3) * j;
        const double have = plateInertiaAboutWebPlane(in.sides, in.b, in.t, in.tw);
        StiffenerCheck c = make("shear_stiffener_inertia", "Enrijecedor de cortante: inercia minima", "NBR 8800:2024, 5.4.3.1.3-c", need, have, "mm4");
        c.lines.push_back(fmt("j = max(2,5/(a/h)^2 - 2; 0,5) = %.3f; I_min = a tw^3 j = %.0f x %.1f^3 x %.3f = %.4g mm4; I = %.4g mm4", j, a, in.tw, j, need, have));
        out.push_back(std::move(c));
    }
    {
        StiffenerCheck c = make("shear_stiffener_weld_gap", "Enrijecedor de cortante: soldas", "NBR 8800:2024, 5.4.3.1.3-a", 0.0, 1.0, "");
        c.lines.push_back(fmt("Soldado a alma e as mesas; do lado da mesa tracionada pode ser interrompido de modo que a distancia entre a solda mesa-alma e a do enrijecedor fique entre 4 tw = %.1f mm e 6 tw = %.1f mm.", 4.0 * in.tw, 6.0 * in.tw));
        out.push_back(std::move(c));
    }
    return out;
}

namespace {

StiffenerCheck coldCheck(const char *id, const char *title, const char *clause, double demand, double resistance, const char *unit, std::vector<std::string> lines)
{
    StiffenerCheck c;
    c.id = id;
    c.title = title;
    c.clause = clause;
    c.demand = demand;
    c.resistance = resistance;
    c.unit = unit;
    c.ratio = resistance > 0.0 ? demand / resistance : (demand > 0.0 ? 1e9 : 0.0);
    c.lines = std::move(lines);
    return c;
}

} // namespace

StiffenerResult checkColdStiffener(const ColdStiffenerInput &in)
{
    StiffenerResult result;
    const double pi = 3.14159265358979323846;
    const double sides = std::max(1, std::min(in.sides, 2));
    const double as = sides * in.b * in.ts;
    const double strip = (in.interior ? 18.0 : 10.0) * in.t;
    const double area = (in.interior ? 18.0 : 10.0) * in.t * in.t + as;
    const double lim = 0.42 * std::sqrt(in.E / in.fy);
    {
        const double ratio = in.ts > 0.0 ? in.b / in.ts : 1e9;
        result.checks.push_back(coldCheck("cold_stiffener_slenderness", "Esbeltez do enrijecedor (b/t)", "NBR 14762:2010, 9.5.1", ratio, lim, "",
                                          {fmt("b/ts = %.1f / %.1f = %.1f <= 0,42 sqrt(E/fy) = %.1f (elemento AL)", in.b, in.ts, ratio, lim)}));
    }
    if (in.tension) {
        const double rd = as * in.fy / in.gamma;
        result.checks.push_back(coldCheck("cold_stiffener_axial", "Enrijecedor tracionado", "NBR 14762:2010, 9.5.1-a", in.force, rd, "N",
                                          {fmt("Ns,Rd = As fy/gamma = %.0f x %.0f / %.2f = %.1f kN; Ns,Sd = %.1f kN", as, in.fy, in.gamma, rd / 1e3, in.force / 1e3)}));
    } else {
        // inercia em relacao ao eixo no plano medio da alma: chapas (de um ou dos dois lados) e a faixa de alma
        const double plates = sides * in.ts * (std::pow(in.t / 2.0 + in.b, 3) - std::pow(in.t / 2.0, 3)) / 3.0;
        const double inertia = plates + strip * std::pow(in.t, 3) / 12.0;
        const double ne = pi * pi * in.E * inertia / (in.h * in.h);
        const double lambda0 = std::sqrt(area * in.fy / ne);
        const double chi = lambda0 <= 1.5 ? std::pow(0.658, lambda0 * lambda0) : 0.877 / (lambda0 * lambda0);
        const double rd = chi * area * in.fy / in.gamma;
        result.checks.push_back(coldCheck("cold_stiffener_axial", "Enrijecedor comprimido", "NBR 14762:2010, 9.5.1-b e 9.7.2", in.force, rd, "N",
                                          {fmt("A = %s + As = %.0f + %.0f = %.0f mm2 (faixa de alma de %.0f t = %.1f mm); I = %.4g mm4 em relacao ao eixo no plano medio da alma", in.interior ? "18 t2" : "10 t2",
                                               area - as, as, area, in.interior ? 18.0 : 10.0, strip, inertia),
                                           fmt("Ne = pi2 E I/h2 = %.1f kN (KL = h = %.0f mm); lambda0 = %.3f; chi = %.3f", ne / 1e3, in.h, lambda0, chi),
                                           fmt("Ns,Rd = chi A fy/gamma = %.1f kN; Ns,Sd = %.1f kN", rd / 1e3, in.force / 1e3)}));
    }
    for (const StiffenerCheck &c : result.checks) {
        result.maxRatio = std::max(result.maxRatio, c.ratio);
    }
    return result;
}

std::vector<StiffenerCheck> checkColdShearStiffener(const ColdStiffenerInput &in, double a)
{
    std::vector<StiffenerCheck> checks;
    if (a <= 0.0 || in.h <= 0.0 || in.t <= 0.0) {
        return checks;
    }
    const double limitA = std::min(3.0, std::pow(260.0 / (in.h / in.t), 2.0));
    checks.push_back(coldCheck("cold_shear_spacing", "Distancia entre enrijecedores de cortante", "NBR 14762:2010, 9.5.2", a / in.h, limitA, "",
                               {fmt("a/h = %.0f / %.0f = %.2f <= min(3,0 ; [260/(h/t)]2 = %.2f)", a, in.h, a / in.h, std::pow(260.0 / (in.h / in.t), 2.0))}));
    const double sides = std::max(1, std::min(in.sides, 2));
    const double is = sides * in.ts * (std::pow(in.t / 2.0 + in.b, 3) - std::pow(in.t / 2.0, 3)) / 3.0;
    const double isMin = std::max(5.0 * in.h * std::pow(in.t, 3) * (in.h / a - 0.7 * a / in.h), in.t * std::pow(in.h / 50.0, 4));
    checks.push_back(coldCheck("cold_shear_inertia", "Inercia do enrijecedor de cortante", "NBR 14762:2010, 9.5.2", isMin, is, "mm4",
                               {fmt("Is,min = max(5 h t3 (h/a - 0,7 a/h) = %.0f ; t (h/50)4 = %.0f) = %.0f mm4; Is = %.0f mm4 (eixo no plano medio da alma)", 5.0 * in.h * std::pow(in.t, 3) * (in.h / a - 0.7 * a / in.h),
                                    in.t * std::pow(in.h / 50.0, 4), isMin, is)}));
    return checks;
}

StiffenerProposal proposeStiffener(StiffenerInput in)
{
    StiffenerProposal best;
    double bestArea = 1e300;
    const double bMin = std::max(25.0, std::ceil((in.bf / 3.0 - in.tw) / 5.0) * 5.0);
    const double bMax = std::max(bMin, in.bf / 2.0);
    for (const double t : {6.3, 8.0, 9.5, 12.5, 16.0, 19.0, 22.4, 25.0, 31.5}) {
        for (double b = bMin; b <= bMax + 1e-9; b += 5.0) {
            if (b <= in.cut + 1.0) {
                continue;
            }
            StiffenerInput candidate = in;
            candidate.b = b;
            candidate.t = t;
            const StiffenerResult r = checkStiffener(candidate);
            if (r.maxRatio <= 1.0 + 1e-9) {
                const double area = std::max(1, in.sides) * b * t;
                if (area < bestArea) {
                    bestArea = area;
                    best = {b, t, true};
                }
            }
        }
    }
    return best;
}

} // namespace Structura::Design::Stiffeners
