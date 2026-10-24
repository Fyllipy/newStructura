#include "design/DesignSection.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design {

namespace {

} // namespace

SectionKind classifyFamily(const std::string &family)
{
    if (family == "W Laminado" || family == "I Laminado" || family == "I Alma Variavel") {
        return SectionKind::I;
    }
    if (family == "U Laminado" || family == "U Inclinado Laminado") {
        return SectionKind::Channel;
    }
    if (family == "L Laminado") {
        return SectionKind::Angle;
    }
    if (family == "T Laminado") {
        return SectionKind::Tee;
    }
    if (family == "Tubular Retangular" || family == "Tubular Quadrado") {
        return SectionKind::RectTube;
    }
    if (family == "Tubular Circular") {
        return SectionKind::RoundTube;
    }
    if (family == "Barra Circular") {
        return SectionKind::SolidRound;
    }
    if (family == "Barra Quadrada" || family == "Barra Retangular") {
        return SectionKind::SolidRect;
    }
    if (family == "Dupla Cantoneira T") {
        return SectionKind::DoubleAngleT;
    }
    if (family == "Duplo U Costas") {
        return SectionKind::DoubleChannelBack;
    }
    if (family == "Duplo U Face a Face") {
        return SectionKind::DoubleChannelBox;
    }
    if (family == "U Formado") {
        return SectionKind::ColdChannel;
    }
    if (family == "C Formado") {
        return SectionKind::ColdLippedChannel;
    }
    if (family == "Z Formado") {
        return SectionKind::ColdZ;
    }
    if (family == "L Formado") {
        return SectionKind::ColdAngle;
    }
    if (family == "Cartola Formado") {
        return SectionKind::ColdHat;
    }
    return SectionKind::Unsupported;
}

Standard standardForKind(SectionKind kind, Standard rectangularTubeStandard)
{
    switch (kind) {
    case SectionKind::ColdChannel:
    case SectionKind::ColdLippedChannel:
    case SectionKind::ColdZ:
    case SectionKind::ColdAngle:
    case SectionKind::ColdHat:
        return Standard::NBR14762;
    case SectionKind::RectTube:
        return rectangularTubeStandard;
    default:
        return Standard::NBR8800;
    }
}

Standard automaticRectTubeStandard(const ProfileSpec &spec)
{
    return spec.param("t") >= kHotFinishedTubeMinThickness ? Standard::NBR8800 : Standard::NBR14762;
}

std::string kindName(SectionKind kind)
{
    switch (kind) {
    case SectionKind::I: return "Perfil I/H laminado";
    case SectionKind::Channel: return "Perfil U laminado";
    case SectionKind::Angle: return "Cantoneira laminada";
    case SectionKind::Tee: return "Perfil T laminado";
    case SectionKind::RectTube: return "Tubular retangular";
    case SectionKind::RoundTube: return "Tubular circular";
    case SectionKind::SolidRound: return "Barra circular macica";
    case SectionKind::SolidRect: return "Barra retangular macica";
    case SectionKind::DoubleAngleT: return "Barra composta: duas cantoneiras em T";
    case SectionKind::DoubleChannelBack: return "Barra composta: dois U em I (costas com costas)";
    case SectionKind::DoubleChannelBox: return "Barra composta: dois U face a face (caixa)";
    case SectionKind::ColdChannel: return "Perfil U simples (formado a frio)";
    case SectionKind::ColdLippedChannel: return "Perfil U enrijecido (formado a frio)";
    case SectionKind::ColdZ: return "Perfil Z (formado a frio)";
    case SectionKind::ColdAngle: return "Cantoneira (formada a frio)";
    case SectionKind::ColdHat: return "Perfil cartola (formado a frio)";
    default: return "Nao suportado";
    }
}

namespace {

/// Linha media (orientacao canonica) das secoes abertas de paredes finas; vazia nos demais casos.
CenterLine centerLineOf(const DesignSection &s)
{
    CenterLine line;
    auto add = [&](double a, double b) {
        line.points.push_back({a, b});
        return static_cast<int>(line.points.size()) - 1;
    };
    auto seg = [&](int i, int j, double t) { line.segments.push_back({i, j, t}); };
    switch (s.kind) {
    case SectionKind::I: {
        if (s.d <= 0 || s.bf <= 0 || s.tw <= 0 || s.tf <= 0) {
            break;
        }
        const double h = s.d - s.tf;
        const int p0 = add(0, -h / 2), p1 = add(0, 0), p2 = add(0, h / 2), p3 = add(-s.bf / 2, h / 2), p4 = add(s.bf / 2, h / 2), p5 = add(-s.bf / 2, -h / 2), p6 = add(s.bf / 2, -h / 2);
        seg(p0, p1, s.tw);
        seg(p1, p2, s.tw);
        seg(p2, p3, s.tf);
        seg(p2, p4, s.tf);
        seg(p0, p5, s.tf);
        seg(p0, p6, s.tf);
        break;
    }
    case SectionKind::Channel: {
        if (s.d <= 0 || s.bf <= 0 || s.tw <= 0 || s.tf <= 0) {
            break;
        }
        const double hm = s.d - s.tf, bfm = s.bf - 0.5 * s.tw;
        const int p0 = add(0, -hm / 2), p1 = add(0, hm / 2), p2 = add(bfm, hm / 2), p3 = add(bfm, -hm / 2);
        seg(p0, p1, s.tw);
        seg(p1, p2, s.tf);
        seg(p0, p3, s.tf);
        break;
    }
    case SectionKind::Tee: {
        if (s.d <= 0 || s.bf <= 0 || s.tw <= 0 || s.tf <= 0) {
            break;
        }
        const double yf = s.d - 0.5 * s.tf;
        const int p0 = add(0, 0), p1 = add(0, yf), p2 = add(-s.bf / 2, yf), p3 = add(s.bf / 2, yf);
        seg(p0, p1, s.tw);
        seg(p1, p2, s.tf);
        seg(p1, p3, s.tf);
        break;
    }
    case SectionKind::Angle: {
        if (s.leg <= 0 || s.t <= 0) {
            break;
        }
        const double l = s.leg - 0.5 * s.t;
        const int p0 = add(0, 0), p1 = add(l, 0), p2 = add(0, l);
        seg(p0, p1, s.t);
        seg(p0, p2, s.t);
        break;
    }
    case SectionKind::ColdChannel: {
        if (s.d <= 0 || s.bf <= 0 || s.t <= 0) {
            break;
        }
        const double hm = s.d - s.t, bfm = s.bf - 0.5 * s.t;
        const int p0 = add(0, -hm / 2), p1 = add(0, hm / 2), p2 = add(bfm, hm / 2), p3 = add(bfm, -hm / 2);
        seg(p0, p1, s.t);
        seg(p1, p2, s.t);
        seg(p0, p3, s.t);
        break;
    }
    case SectionKind::ColdLippedChannel: {
        if (s.d <= 0 || s.bf <= 0 || s.t <= 0 || s.lip <= 0) {
            break;
        }
        const double hm = s.d - s.t, bfc = s.bf - s.t, lip = s.lip - 0.5 * s.t;
        const int p0 = add(0, -hm / 2), p1 = add(0, hm / 2), p2 = add(bfc, hm / 2), p3 = add(bfc, hm / 2 - lip), p4 = add(bfc, -hm / 2), p5 = add(bfc, -hm / 2 + lip);
        seg(p0, p1, s.t);
        seg(p1, p2, s.t);
        seg(p2, p3, s.t);
        seg(p0, p4, s.t);
        seg(p4, p5, s.t);
        break;
    }
    case SectionKind::ColdZ: {
        if (s.d <= 0 || s.bf <= 0 || s.t <= 0) {
            break;
        }
        const double hm = s.d - s.t, fl = s.bf + 0.5 * s.t;
        const int p0 = add(0, -hm / 2), p1 = add(0, hm / 2), p2 = add(fl, hm / 2), p3 = add(-fl, -hm / 2);
        seg(p0, p1, s.t);
        seg(p1, p2, s.t);
        seg(p0, p3, s.t);
        break;
    }
    case SectionKind::ColdAngle: {
        if (s.d <= 0 || s.bf <= 0 || s.t <= 0) {
            break;
        }
        const int p0 = add(0, 0), p1 = add(s.bf - 0.5 * s.t, 0), p2 = add(0, -(s.d - 0.5 * s.t));
        seg(p0, p1, s.t);
        seg(p0, p2, s.t);
        break;
    }
    case SectionKind::ColdHat: {
        if (s.d <= 0 || s.bf <= 0 || s.t <= 0 || s.lip <= 0) {
            break;
        }
        const double wb = s.bf - s.t, hc = s.d - s.t, ear = s.lip - 0.5 * s.t;
        const int p0 = add(-wb / 2, 0), p1 = add(wb / 2, 0), p2 = add(-wb / 2, hc), p3 = add(wb / 2, hc), p4 = add(-wb / 2 - ear, hc), p5 = add(wb / 2 + ear, hc);
        seg(p0, p1, s.t);
        seg(p0, p2, s.t);
        seg(p1, p3, s.t);
        seg(p2, p4, s.t);
        seg(p3, p5, s.t);
        break;
    }
    default:
        break;
    }
    return line;
}

} // namespace

DesignSection makeDesignSection(const ProfileSpec &spec, const std::vector<Loop> &loops, double jApp,
                                Standard rectangularTubeStandard, double principalAngleDeg, std::size_t outerCount)
{
    DesignSection s;
    s.family = spec.family;
    s.kind = classifyFamily(spec.family);
    s.standard = standardForKind(s.kind, rectangularTubeStandard);
    if (s.kind == SectionKind::Unsupported) {
        return s;
    }
    s.g = computeGrossSection(loops, outerCount);
    if (!s.g.valid) {
        s.kind = SectionKind::Unsupported;
        return s;
    }
    s.J = jApp;
    const GrossSection drawn = s.g;   // eixos do desenho
    s.nativeCentroidA = drawn.centroidA;
    s.nativeCentroidB = drawn.centroidB;
    s.spec = spec;
    s.principalAngleDeg = principalAngleDeg;
    s.outline = loops.front();
    for (auto &p : s.outline) {
        p[0] -= drawn.centroidA;
        p[1] -= drawn.centroidB;
    }

    const double tol = 1e-6 * std::max(s.g.aMax - s.g.aMin, s.g.bMax - s.g.bMin);
    s.symmetricAboutX = std::abs(s.g.WxTop - s.g.WxBottom) < 1e-4 * s.g.WxTop && std::abs(s.g.bMax + s.g.bMin) < tol + 1e-4 * (s.g.bMax - s.g.bMin);
    s.symmetricAboutY = std::abs(s.g.WyRight - s.g.WyLeft) < 1e-4 * s.g.WyRight && std::abs(s.g.aMax + s.g.aMin) < tol + 1e-4 * (s.g.aMax - s.g.aMin);

    switch (s.kind) {
    case SectionKind::I: {
        s.d = spec.family == "I Alma Variavel" ? 0.5 * (spec.param("d1") + spec.param("d2")) : spec.param("d");
        s.bf = spec.param("bf");
        s.tw = spec.param("tw");
        s.tf = spec.param("tf");
        s.r = std::max(spec.param("r"), spec.param("r1"));
        s.h = std::max(0.0, s.d - 2.0 * s.tf - 2.0 * s.r);
        s.Cw = s.g.Iy * std::pow(s.d - s.tf, 2) / 4.0;   // NBR 8800:2024, D.2.8-a
        s.elements = {{"mesa", s.bf / 2.0, s.tf, 4, false, 4}, {"alma", s.h, s.tw, 2, true, 1}};
        break;
    }
    case SectionKind::Channel: {
        s.d = spec.param("d");
        s.bf = spec.param("bf");
        s.tw = spec.param("tw");
        s.tf = spec.param("tf");
        s.r = spec.param("r");
        s.h = std::max(0.0, s.d - 2.0 * s.tf - 2.0 * s.r);
        const double bfm = s.bf - 0.5 * s.tw;
        const double hm = s.d - s.tf;
        s.Cw = s.tf * std::pow(bfm, 3) * hm * hm / 12.0 * (3.0 * bfm * s.tf + 2.0 * hm * s.tw) /
               (6.0 * bfm * s.tf + hm * s.tw);   // D.2.8-a (secao U)
        s.webLeft = std::abs(s.g.aMin) < s.g.aMax;
        // Centro de cisalhamento: afastado da linha media da alma, para fora, de e0.
        const double e0 = 3.0 * bfm * bfm * s.tf / (6.0 * bfm * s.tf + hm * s.tw);
        const double webCenterline = s.webLeft ? (s.g.aMin + 0.5 * s.tw) : (s.g.aMax - 0.5 * s.tw);
        s.x0 = webCenterline + (s.webLeft ? -e0 : e0);
        s.y0 = 0.0;
        s.elements = {{"mesa", s.bf, s.tf, 4, false, 2}, {"alma", s.h, s.tw, 2, true, 1}};
        break;
    }
    case SectionKind::Angle: {
        s.leg = spec.param("b");
        s.t = spec.param("t");
        // Eixos principais
        const double mean = 0.5 * (s.g.Ix + s.g.Iy);
        const double radius = std::sqrt(std::pow(0.5 * (s.g.Ix - s.g.Iy), 2) + s.g.Ixy * s.g.Ixy);
        s.I1 = mean + radius;
        s.I2 = mean - radius;
        s.principalAngleDeg = 0.5 * std::atan2(-2.0 * s.g.Ixy, s.g.Ix - s.g.Iy) * 180.0 / kPi;
        s.rMin = std::sqrt(s.I2 / s.g.area);
        // Centro de cisalhamento no encontro das linhas medias das abas
        s.x0 = s.g.aMin + 0.5 * s.t;
        s.y0 = s.g.bMin + 0.5 * s.t;
        s.Cw = 0.0;
        s.elements = {{"aba", s.leg, s.t, 3, false, 2}};
        break;
    }
    case SectionKind::DoubleAngleT: {
        // dois perfis L iguais com as abas verticais costas com costas (afastamento e) e as horizontais para fora: secao T monossimetrica
        s.leg = spec.param("b");
        s.t = spec.param("t");
        s.gapE = spec.param("e");
        s.d = s.leg;                        // altura total do T
        s.bf = 2.0 * s.leg + s.gapE;        // largura total da mesa
        s.flangeUp = std::abs(s.g.bMax) < std::abs(s.g.bMin);
        s.y0 = s.flangeUp ? (s.g.bMax - 0.5 * s.t) : (s.g.bMin + 0.5 * s.t);   // centro de cisalhamento: encontro das linhas medias da mesa e da alma
        s.x0 = 0.0;
        s.Cw = std::pow(s.leg * s.t, 3) / 9.0;   // soma das constantes de empenamento dos quatro retangulos (b t)^3/36
        const GrossSection one = computeGrossSection({loops.front()});
        if (one.valid) {
            const double mean = 0.5 * (one.Ix + one.Iy);
            const double radius = std::sqrt(std::pow(0.5 * (one.Ix - one.Iy), 2) + one.Ixy * one.Ixy);
            s.rMin = std::sqrt(std::max(mean - radius, 1e-9) / one.area);
            s.componentArea = one.area;
        }
        s.elements = {{"aba", s.leg, s.t, 3, false, 4}};
        break;
    }
    case SectionKind::DoubleChannelBack:
    case SectionKind::DoubleChannelBox: {
        s.d = spec.param("d");
        s.tw = spec.param("tw");
        s.tf = spec.param("tf");
        s.r = spec.param("r");
        s.gapE = spec.param("e");
        const double bfOne = spec.param("bf");
        s.bf = 2.0 * bfOne + s.gapE;   // largura total
        s.h = std::max(0.0, s.d - 2.0 * s.tf - 2.0 * s.r);
        const double bfm = bfOne - 0.5 * s.tw;
        const double hm = s.d - s.tf;
        // Cw de cada U (D.2.8-a); sem acao conjunta dos perfis ligados a intervalos
        const double cwOne = s.tf * std::pow(bfm, 3) * hm * hm / 12.0 * (3.0 * bfm * s.tf + 2.0 * hm * s.tw) / (6.0 * bfm * s.tf + hm * s.tw);
        s.Cw = 2.0 * cwOne;
        s.x0 = 0.0;
        s.y0 = 0.0;
        const GrossSection one = computeGrossSection({loops.front()});
        if (one.valid) {
            s.rMin = std::min(one.rx, one.ry);
            s.componentArea = one.area;
        }
        s.elements = {{"mesa", bfOne, s.tf, 4, false, 4}, {"alma", s.h, s.tw, 2, true, 2}};
        break;
    }
    case SectionKind::Tee: {
        s.d = spec.param("d");
        s.bf = spec.param("bf");
        s.tw = spec.param("tw");
        s.tf = spec.param("tf");
        s.flangeUp = std::abs(s.g.bMax) < std::abs(s.g.bMin);
        const double flangeMid = s.flangeUp ? (s.g.bMax - 0.5 * s.tf) : (s.g.bMin + 0.5 * s.tf);
        s.y0 = flangeMid;
        s.x0 = 0.0;
        s.Cw = std::pow(s.tf * s.bf, 3) / 144.0 + std::pow((s.d - 0.5 * s.tf) * s.tw, 3) / 36.0;
        s.elements = {{"mesa", s.bf / 2.0, s.tf, 4, false, 2}, {"alma", s.d, s.tw, 6, false, 1}};
        break;
    }
    case SectionKind::RectTube: {
        const double w = spec.param("b");
        const double hh = spec.family == "Tubular Quadrado" ? w : spec.param("h");
        s.t = spec.param("t");
        s.bf = w;
        s.d = hh;
        s.flatWidthB = std::max(0.0, w - 4.0 * s.t);
        s.flatWidthH = std::max(0.0, hh - 4.0 * s.t);
        s.elements = {{"mesa", s.flatWidthB, s.t, 1, true, 2}, {"alma", s.flatWidthH, s.t, 1, true, 2}};
        break;
    }
    case SectionKind::RoundTube: {
        s.D = spec.param("D");
        s.t = spec.param("t");
        break;
    }
    case SectionKind::SolidRound: {
        s.D = spec.param("d");
        break;
    }
    case SectionKind::SolidRect: {
        s.bf = spec.param("b");
        s.d = spec.family == "Barra Quadrada" ? s.bf : spec.param("h");
        break;
    }
    case SectionKind::ColdChannel:
    case SectionKind::ColdLippedChannel:
    case SectionKind::ColdZ:
    case SectionKind::ColdAngle:
    case SectionKind::ColdHat: {
        s.d = spec.param("d");
        s.bf = spec.param("bf");
        s.t = spec.param("t");
        s.lip = spec.param("c");
        s.webLeft = std::abs(s.g.aMin) < s.g.aMax;
        break;
    }
    default:
        break;
    }

    if (std::abs(principalAngleDeg) > 1e-9) {
        // Reexpressa a secao nos eixos principais: b' = c b + s a ; a' = -s b + c a.
        const double c = std::cos(principalAngleDeg * kPi / 180.0);
        const double sn = std::sin(principalAngleDeg * kPi / 180.0);
        std::vector<Loop> rotated = loops;
        for (auto &loop : rotated) {
            for (auto &p : loop) {
                const double a = p[0];
                const double b = p[1];
                p[0] = -sn * b + c * a;
                p[1] = c * b + sn * a;
            }
        }
        const GrossSection principal = computeGrossSection(rotated);
        if (principal.valid) {
            s.outline = rotated.front();
            for (auto &p : s.outline) {
                p[0] -= principal.centroidA;
                p[1] -= principal.centroidB;
            }
            const double x0 = s.x0;
            const double y0 = s.y0;
            s.x0 = -sn * y0 + c * x0;
            s.y0 = c * y0 + sn * x0;
            s.g = principal;
            s.symmetricAboutX = false;
            s.symmetricAboutY = false;
        }
        (void)drawn;
    }
    s.sectorial = computeSectorial(centerLineOf(s), drawn, principalAngleDeg);
    return s;
}

} // namespace Structura::Design
