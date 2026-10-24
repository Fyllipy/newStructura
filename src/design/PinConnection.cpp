#include "design/PinConnection.h"

#include "design/ConnectionHelpers.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design::Connections {

using detail::fmtn;
using detail::kPi;

double pinBendingMoment(const PinPlateInput &in)
{
    const double t = in.plateThickness, t2 = in.otherThickness, g = in.gap;
    if (in.shearPlanes >= 2) {
        // chapa central (forca F, tensoes uniformes na espessura t) entre duas externas (F/2 cada, espessura t2): no plano de simetria,
        // M = F/2 (distancia do centroide da chapa externa ao eixo - distancia do centroide da meia chapa central) = F/2 (t/4 + g + t2/2)
        return 0.5 * in.tension * (t / 4.0 + g + t2 / 4.0 * 2.0);
    }
    // corte simples: duas chapas (t e t2) com a folga g; binario F com braco = distancia entre os centroides dos contatos, M = F (t + t2)/4 + F g/2
    return in.tension * ((t + t2) / 4.0 + g / 2.0);
}

std::vector<Check> checkPinPlate(const PinPlateInput &in)
{
    using namespace detail;
    std::vector<Check> checks;
    const double F = std::abs(in.tension);
    const double t = in.plateThickness;
    const double fyContact = std::min(in.plate.fy, in.pin.fy);

    // geometria efetiva (5.2.6.1)
    const double edgeToHole = 0.5 * (in.plateWidth - in.holeDiameter);   // furo na meia distancia das bordas
    const double bef = std::min(2.0 * t + 16.0, edgeToHole);
    const double asf = 2.0 * t * (in.endDistance + in.pinDiameter / 2.0);

    // a) escoamento da secao bruta (5.2.2-a)
    {
        const double ag = in.plateWidth * t;
        const double rd = ag * in.plate.fy / in.gammaA1;
        Check c = ratioN("Chapa ligada por pino - escoamento da secao bruta", "5.2.6.1-a / 5.2.2-a", F, rd);
        c.lines.push_back(fmtn("Ag = b t = %.1f x %.1f = %.0f mm2; Nt,Rd = Ag fy / gama_a1 = %.0f x %.0f / %.2f = %.2f kN", in.plateWidth, t, ag, ag, in.plate.fy, in.gammaA1, rd / 1000.0));
        checks.push_back(c);
    }
    // b) pressao de contato na area projetada do pino (6.6.2: furo mandrilado ou broqueado)
    {
        const double area = in.pinDiameter * t;
        const double rd = 1.8 * area * fyContact / in.gammaA2;
        Check c = ratioN("Chapa ligada por pino - pressao de contato", "5.2.6.1-b / 6.6.2", F, rd);
        c.lines.push_back(fmtn("A = dp t = %.1f x %.1f = %.0f mm2 (area projetada); fy = menor entre chapa e pino = %.0f MPa", in.pinDiameter, t, area, fyContact));
        c.lines.push_back(fmtn("Fc,Rd = 1,8 A fy / gama_a2 = 1,8 x %.0f x %.0f / %.2f = %.2f kN", area, fyContact, in.gammaA2, rd / 1000.0));
        if (!in.machinedHole) {
            c.lines.push_back("ATENCAO: furo nao mandrilado/broqueado - superficie nao usinada (6.6.3): a transmissao da pressao por pino so e coberta em furos usinados.");
            c.demand = std::max(c.demand, c.resistance * 1.0001);
        }
        checks.push_back(c);
    }
    // c) ruptura da secao liquida por tracao
    {
        const double rd = 2.0 * t * bef * in.plate.fu / in.gammaA2;
        Check c = ratioN("Chapa ligada por pino - ruptura da secao liquida (tracao)", "5.2.6.1-c", F, rd);
        c.lines.push_back(fmtn("bef = min(2 t + 16 ; borda do furo a borda mais proxima = (b - dh)/2) = min(%.1f ; %.1f) = %.1f mm", 2.0 * t + 16.0, edgeToHole, bef));
        c.lines.push_back(fmtn("Nt,Rd = 2 t bef fu / gama_a2 = 2 x %.1f x %.1f x %.0f / %.2f = %.2f kN", t, bef, in.plate.fu, in.gammaA2, rd / 1000.0));
        checks.push_back(c);
    }
    // d) ruptura da secao liquida por cisalhamento
    {
        const double rd = 0.60 * asf * in.plate.fu / in.gammaA2;
        Check c = ratioN("Chapa ligada por pino - ruptura da secao liquida (cisalhamento)", "5.2.6.1-d", F, rd);
        c.lines.push_back(fmtn("Asf = 2 t (a + dp/2) = 2 x %.1f x (%.1f + %.1f/2) = %.0f mm2", t, in.endDistance, in.pinDiameter, asf));
        c.lines.push_back(fmtn("Nt,Rd = 0,60 Asf fu / gama_a2 = 0,60 x %.0f x %.0f / %.2f = %.2f kN", asf, in.plate.fu, in.gammaA2, rd / 1000.0));
        checks.push_back(c);
    }
    // requisitos geometricos
    checks.push_back(minimumRule("Largura da chapa b >= 2 bef + dp", "5.2.6.1", in.plateWidth, 2.0 * bef + in.pinDiameter));
    checks.back().lines.push_back(fmtn("b = %.1f mm >= 2 x %.1f + %.1f = %.1f mm", in.plateWidth, bef, in.pinDiameter, 2.0 * bef + in.pinDiameter));
    checks.push_back(minimumRule("Distancia a >= 1,33 bef", "5.2.6.1", in.endDistance, 1.33 * bef));
    checks.back().lines.push_back(fmtn("a = %.1f mm >= 1,33 x %.1f = %.1f mm", in.endDistance, bef, 1.33 * bef));
    if (in.rotationAllowed) {
        checks.push_back(rangeMax("Folga do pino rotulado dh - dp <= 1,0 mm", "5.2.6.1", in.holeDiameter - in.pinDiameter, 1.0, "mm"));
        checks.back().lines.push_back(fmtn("dh - dp = %.2f mm <= 1,0 mm (o pino tambem permite rotacao relativa)", in.holeDiameter - in.pinDiameter));
    }
    checks.push_back(rangeMin("Furo maior ou igual ao pino", "detalhamento", in.holeDiameter, in.pinDiameter, "mm"));

    // pino (5.2.6.2 e 6.4)
    const double dp = in.pinDiameter;
    const double w = kPi * dp * dp * dp / 32.0;
    const double ag = kPi * dp * dp / 4.0;
    const double moment = pinBendingMoment(in);
    {
        const double rd = 1.2 * w * in.pin.fy / in.gammaA1;
        Check c = ratioNmm("Pino - momento fletor", "5.2.6.2 / 6.4.2.1", moment, rd);
        if (in.shearPlanes >= 2) {
            c.lines.push_back(fmtn("Corte duplo, tensoes de contato uniformes na espessura (6.4.1): M = (F/2)(t/4 + g + t2/2) = %.2f x (%.1f/4 + %.1f + %.1f/2) = %.2f kN.m", F / 2000.0, t, in.gap,
                                   in.otherThickness, moment / 1e6));
        } else {
            c.lines.push_back(fmtn("Corte simples, tensoes de contato uniformes na espessura (6.4.1): M = F ((t + t2)/4 + g/2) = %.2f x ((%.1f + %.1f)/4 + %.1f/2) = %.2f kN.m", F / 1000.0, t,
                                   in.otherThickness, in.gap, moment / 1e6));
        }
        c.lines.push_back(fmtn("W = pi dp^3/32 = %.0f mm3; MRd = 1,2 W fy / gama_a1 = 1,2 x %.0f x %.0f / %.2f = %.2f kN.m", w, w, in.pin.fy, in.gammaA1, rd / 1e6));
        checks.push_back(c);
    }
    {
        const double v = in.shearPlanes >= 2 ? F / 2.0 : F;
        const double aw = 0.75 * ag;
        const double rd = 0.60 * aw * in.pin.fy / in.gammaA1;
        Check c = ratioN("Pino - forca cortante", "5.2.6.2 / 6.4.2.2", v, rd);
        c.lines.push_back(fmtn("Aw = 0,75 Ag = 0,75 x %.0f = %.0f mm2; Fv,Rd = 0,60 Aw fy / gama_a1 = %.2f kN; V = %.2f kN (%s)", ag, aw, rd / 1000.0, v / 1000.0,
                               in.shearPlanes >= 2 ? "F/2: dois planos de corte" : "F: um plano de corte"));
        checks.push_back(c);
    }
    {
        // esmagamento do pino: cada parte transmite sua forca de contato (chapa central: F; externas: F/2 cada)
        struct Part3 {
            const char *name;
            double thickness;
            double force;
        };
        std::vector<Part3> parts;
        if (in.shearPlanes >= 2) {
            parts = {{"chapa central", t, F}, {"chapa externa", in.otherThickness, F / 2.0}};
        } else {
            parts = {{"chapa 1", t, F}, {"chapa 2", in.otherThickness, F}};
        }
        for (const auto &p : parts) {
            const double fy = std::min(in.pin.fy, in.plate.fy);
            const double rd = 1.5 * p.thickness * dp * fy / in.gammaA1;
            Check c = ratioN(std::string("Pino - esmagamento (") + p.name + ")", "6.4.2.3", p.force, rd);
            c.lines.push_back(fmtn("FR,d = 1,5 t d fy / gama_a1 = 1,5 x %.1f x %.1f x %.0f / %.2f = %.2f kN; forca de contato = %.2f kN", p.thickness, dp, fy, in.gammaA1, rd / 1000.0, p.force / 1000.0));
            checks.push_back(c);
        }
    }
    return checks;
}

std::vector<Part> partsPinPlate(const PinPlateInput &in)
{
    std::vector<Part> parts;
    Part plate;
    const double length = in.endDistance + in.pinDiameter / 2.0 + 150.0;
    plate.description = fmtn("Chapa %.1f x %.0f x %.0f mm com furo %.1f mm (a = %.0f mm)", in.plateThickness, in.plateWidth, length, in.holeDiameter, in.endDistance);
    plate.quantity = 1;
    plate.massEach = in.plateThickness * in.plateWidth * length * 1e-9 * detail::kSteelDensity;
    parts.push_back(plate);
    Part pin;
    const double pinLength = in.shearPlanes >= 2 ? in.plateThickness + 2.0 * in.otherThickness + 2.0 * in.gap + 20.0 : in.plateThickness + in.otherThickness + in.gap + 20.0;
    pin.description = fmtn("Pino d = %.0f mm, comprimento %.0f mm", in.pinDiameter, pinLength);
    pin.quantity = 1;
    pin.massEach = detail::kPi * in.pinDiameter * in.pinDiameter / 4.0 * pinLength * 1e-9 * detail::kSteelDensity;
    parts.push_back(pin);
    return parts;
}

std::vector<Check> checkBoltFatigue(const BoltFatigueInput &in)
{
    using namespace detail;
    std::vector<Check> checks;
    const double abe = kPi / 4.0 * std::pow(in.db - 0.9382 * in.pitch, 2);   // H.5-b
    const double cf = 3.9e8;     // categoria G (Tabela H.1, caso 8.5)
    const double threshold = 48.0;
    double range = in.forceRange;
    std::string how = "faixa de forca informada";
    if (range <= 0.0 && in.preloaded && in.staticDesignForce > 0.0) {
        range = 0.20 * in.staticDesignForce;   // alternativa de H.5: 20 % da forca da combinacao de H.2.1
        how = "alternativa de H.5: 20 % da forca da combinacao de H.2.1";
    }
    const double stressRange = range / (std::max(1, in.bolts) * abe);
    double allowed = threshold;
    if (in.cycles > 0.0) {
        allowed = std::max(std::cbrt(cf / in.cycles), threshold);
    }
    Check c = ratioMPa("Fadiga do parafuso/barra rosqueada - faixa de tensoes de tracao", "H.5-b / Tabela H.1 (categoria G)", stressRange, allowed);
    c.lines.push_back(fmtn("Abe = pi/4 (db - 0,9382 p)^2 = pi/4 x (%.1f - 0,9382 x %.2f)^2 = %.1f mm2", in.db, in.pitch, abe));
    c.lines.push_back(fmtn("sigma_SR = DF / (n Abe) = %.2f kN / (%d x %.1f) = %.1f MPa (%s)", range / 1000.0, std::max(1, in.bolts), abe, stressRange, how.c_str()));
    if (in.cycles > 0.0) {
        c.lines.push_back(fmtn("sigma_adm = max[(Cf / N)^0,333 ; sigma_TH] = max[(%.2g / %.3g)^0,333 ; %.0f] = %.1f MPa (Cf = 3,9x10^8; sigma_TH = 48 MPa)", cf, in.cycles, threshold, allowed));
    } else {
        c.lines.push_back(fmtn("Vida infinita: sigma_adm = sigma_TH = %.0f MPa", threshold));
    }
    checks.push_back(c);
    if (!in.preloaded) {
        Check n;
        n.name = "Fadiga do parafuso - protensao inicial";
        n.clause = "H.5";
        n.demand = 0.0;
        n.resistance = 1.0;
        n.unit = "";
        n.lines.push_back("Sem protensao inicial (Tabela 19) ou com material nao metalico na pega: a forca axial e o momento, com efeito de alavanca (H.3.2), sao transmitidos exclusivamente pelos parafusos.");
        checks.push_back(n);
    }
    return checks;
}

std::vector<Check> checkMaterialFatigue(const MaterialFatigueInput &in)
{
    using namespace detail;
    struct Category {
        const char *name;
        const char *tableCase;
        double cf, threshold;
    };
    static const Category kCategories[] = {{"B", "2.1/2.2", 120e8, 110.0}, {"C", "2.3", 44e8, 69.0}, {"E", "2.4", 11e8, 31.0}};
    const Category &cat = kCategories[std::clamp(in.tableCase, 1, 3) - 1];
    double allowed = cat.threshold;
    if (in.cycles > 0.0) {
        allowed = std::max(std::cbrt(cat.cf / in.cycles), cat.threshold);
    }
    Check c = ratioMPa("Fadiga do metal-base da ligacao - faixa de tensoes", std::string("H.5-a / Tabela H.1 caso ") + cat.tableCase + " (categoria " + cat.name + ")", in.stressRange, allowed);
    c.lines.push_back(fmtn("Categoria %s (caso %s): Cf = %.3g; sigma_TH = %.0f MPa", cat.name, cat.tableCase, cat.cf, cat.threshold));
    if (in.cycles > 0.0) {
        c.lines.push_back(fmtn("sigma_adm = max[(Cf / N)^0,333 ; sigma_TH] = max[(%.3g / %.3g)^0,333 ; %.0f] = %.1f MPa; sigma_SR = %.1f MPa", cat.cf, in.cycles, cat.threshold, allowed, in.stressRange));
    } else {
        c.lines.push_back(fmtn("Vida infinita: sigma_adm = sigma_TH = %.0f MPa; sigma_SR = %.1f MPa", cat.threshold, in.stressRange));
    }
    return {c};
}

Check fatigueFabricationRequirements()
{
    Check c;
    c.name = "Fadiga - requisitos de fabricacao e montagem (H.6)";
    c.clause = "H.6.1 a H.6.7";
    c.demand = 0.0;
    c.resistance = 1.0;
    c.unit = "";
    c.lines = {"H.6.1: chapas de espera longitudinais, se usadas, continuas; emendas em juntas longas com solda de penetracao total, excesso esmerilhado longitudinalmente; filetes que as ligam, se nao removidas, continuos.",
               "H.6.2: em juntas transversais em T ou de canto com solda de penetracao total, filete de reforco nao menor que 6 mm nos cantos reentrantes.",
               "H.6.3: rugosidade superficial de bordas cortadas termicamente, sujeitas a faixas de tensao, nao superior a 25 micrometros (ASME B46.1).",
               "H.6.4: cantos reentrantes em cortes, recortes e aberturas para acesso de soldagem com raio nao menor que o da Tabela H.1 (casos 1.3 e 1.4), furo com broca ou punção de diametro menor, usinado ate o raio.",
               "H.6.5: juntas transversais com penetracao total em regioes tracionadas: prolongadores removidos e extremidade esmerilhada ate facear; sem limitadores nas extremidades.",
               "H.6.6: ver 6.2.6.2.6 (retornos de soldas de filete sujeitas a carregamentos ciclicos).",
               "H.6.7: faixas de tensao admissiveis de soldas de penetracao total valem so para soldas ensaiadas por ultrassom ou radiografia e aprovadas pelos requisitos de fadiga da AWS D1.1."};
    return c;
}

} // namespace Structura::Design::Connections
