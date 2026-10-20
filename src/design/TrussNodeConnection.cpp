#include "design/TrussNodeConnection.h"

#include "design/ConnectionHelpers.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design::Connections {

using detail::fmtn;
using detail::kSteelDensity;
using detail::ratioN;
using detail::ratioNmm;

namespace {

constexpr double kDeg = 3.14159265358979323846 / 180.0;

/// Capacidade por mm de filete (NBR 8800:2024, Tabela 9): metal da solda e metal-base (N/mm), com a garganta efetiva 0,707 a.
struct FilletCapacity {
    double perMm {0.0};
    double weldMetal {0.0};
    double baseMetal {0.0};
};

FilletCapacity filletCapacity(double leg, double fw, const SteelProps &base, double thickness, double ga1, double gw2)
{
    FilletCapacity c;
    c.weldMetal = 0.6 * 0.707 * leg * fw / gw2;
    c.baseMetal = 0.6 * thickness * base.fy / ga1;
    c.perMm = std::min(c.weldMetal, c.baseMetal);
    return c;
}

} // namespace

TrussInterfaceForces trussInterfaceForces(const TrussNodeInput &in)
{
    TrussInterfaceForces r;
    for (const TrussBarData &b : in.bars) {
        if (!b.on) {
            continue;
        }
        const double side = b.toRight ? 1.0 : -1.0;
        // a barra tracionada puxa o no no sentido da propria barra (se afasta do banzo): componente normal positiva
        const double fx = b.force * std::cos(b.angleDeg * kDeg) * side;
        const double fy = b.force * std::sin(b.angleDeg * kDeg);
        r.fx += fx;
        r.fy += fy;
        (fy >= 0.0 ? r.pull : r.push) += std::abs(fy);
        r.moment += fx * in.ey - fy * in.ex * side;
    }
    return r;
}

std::vector<Check> checkTrussNode(const TrussNodeInput &in)
{
    std::vector<Check> checks;
    const double g1 = in.gammaA1, g2 = in.gammaA2, gw = in.gammaW2;
    int index = 0;
    for (const TrussBarData &b : in.bars) {
        ++index;
        if (!b.on) {
            continue;
        }
        const std::string tag = fmtn(" - barra %d", double(index));
        const double N = std::abs(b.force);
        const bool tension = b.force >= 0.0;
        const double areaTotal = b.area * std::max(1, b.bars);
        if (in.chordMode == TrussNodeInput::Direct) {
            // a barra se solda direto ao banzo: o filete resiste ao esforco axial da barra
            const double L = b.lengthHeel + b.lengthToe;
            const FilletCapacity cap = filletCapacity(b.weldLeg, in.weldFw, b.steel, b.legThickness, g1, gw);
            const double Rd = cap.perMm * L * std::max(1, b.bars);
            Check c = ratioN("Solda barra - banzo (filetes)" + tag, "6.2.5", N, Rd);
            c.lines.push_back(fmtn("filetes %.1f mm: L = %.0f + %.0f mm; por mm: metal da solda %.1f N/mm, metal-base %.1f N/mm", b.weldLeg, b.lengthHeel, b.lengthToe, cap.weldMetal, cap.baseMetal));
            c.lines.push_back(fmtn("Rd = %.2f kN", Rd / 1000.0));
            checks.push_back(c);
        } else if (in.barMode == TrussNodeInput::Welded) {
            // filetes barra - chapa: a forca se divide entre o filete do calcanhar e o da ponta pela excentricidade do centroide
            const double bLeg = std::max(b.legWidth, 1.0);
            const double fHeel = N * (bLeg - b.eccentricity) / bLeg;
            const double fToe = N * b.eccentricity / bLeg;
            const FilletCapacity cap = filletCapacity(b.weldLeg, in.weldFw, b.steel, std::min(b.legThickness, in.gussetThickness), g1, gw);
            const int pieces = std::max(1, b.bars);
            Check h = ratioN("Solda barra - chapa: filete do calcanhar" + tag, "6.2.5", fHeel / pieces, cap.perMm * b.lengthHeel);
            h.lines.push_back(fmtn("F = N (b - ec)/b = %.2f kN (b = %.0f mm, ec = %.1f mm); Rd = %.1f N/mm x %.0f mm = %.2f kN", fHeel / 1000.0, bLeg, b.eccentricity, cap.perMm, b.lengthHeel, cap.perMm * b.lengthHeel / 1000.0));
            checks.push_back(h);
            Check t = ratioN("Solda barra - chapa: filete da ponta" + tag, "6.2.5", fToe / pieces, cap.perMm * b.lengthToe);
            t.lines.push_back(fmtn("F = N ec/b = %.2f kN; Rd = %.1f N/mm x %.0f mm = %.2f kN", fToe / 1000.0, cap.perMm, b.lengthToe, cap.perMm * b.lengthToe / 1000.0));
            checks.push_back(t);
            checks.push_back(detail::minimumRule("Comprimento minimo do filete (>= 4 a e >= 40 mm)" + tag, "6.2.6.2", std::min(b.lengthHeel, b.lengthToe), std::max(4.0 * b.weldLeg, 40.0)));
            // barra: escoamento e ruptura da secao efetiva (Ct = 1 - ec/lc, lc = comprimento do filete)
            const double lc = std::max(std::max(b.lengthHeel, b.lengthToe), 1.0);
            const double ct = std::clamp(1.0 - b.eccentricity / lc, 0.4, 1.0);
            const double yield = areaTotal * b.steel.fy / g1, rupture = ct * areaTotal * b.steel.fu / g2;
            if (tension) {
                Check bar = ratioN("Barra: tracao (escoamento e ruptura da secao efetiva)" + tag, "5.2.2 / 5.2.5", N, std::min(yield, rupture));
                bar.lines.push_back(fmtn("Ct = 1 - ec/lc = %.2f; Ag fy/ga1 = %.2f kN; Ae fu/ga2 = %.2f kN", ct, yield / 1000.0, rupture / 1000.0));
                checks.push_back(bar);
            } else {
                checks.push_back(ratioN("Barra: compressao no trecho ligado (escoamento da secao bruta)" + tag, "5.3", N, yield));
            }
            // chapa: secao de Whitmore na extremidade do filete
            const double lw = bLeg + 2.0 * std::max(b.lengthHeel, b.lengthToe) * std::tan(30.0 * kDeg);
            const double width = std::min(lw, in.gussetWidth);
            const double ag = in.gussetThickness * width;
            if (tension) {
                Check g = ratioN("Chapa: tracao na secao de Whitmore" + tag, "5.2.2 / 6.5.2", N, std::min(ag * in.gusset.fy / g1, ag * in.gusset.fu / g2));
                g.lines.push_back(fmtn("lw = b + 2 L tg30 = %.0f mm (limitada a %.0f); Ag = %.0f mm2", lw, in.gussetWidth, ag));
                checks.push_back(g);
            } else {
                const double r = in.gussetThickness / std::sqrt(12.0);
                const double lambda = 0.65 * std::max(in.freeLength, 20.0) / r;
                const double lambda0 = lambda / detail::kPi * std::sqrt(in.gusset.fy / detail::kModulusE);
                const double chi = detail::chiCompression(lambda0);
                Check g = ratioN("Chapa: compressao na secao de Whitmore" + tag, "5.3.3", N, chi * ag * in.gusset.fy / g1);
                g.lines.push_back(fmtn("K L / r = %.1f (K = 0,65, L = %.0f mm); chi = %.3f", lambda, in.freeLength, chi));
                checks.push_back(g);
            }
        } else {
            // barra parafusada na chapa: reaproveita as verificacoes do gousset parafusado
            GussetInput g;
            g.grade = in.grade;
            g.db = in.db;
            g.threadInShearPlane = in.threadInShearPlane;
            g.rows = b.rows;
            g.lines = b.lines;
            g.pitch = b.pitch;
            g.gauge = b.gauge;
            g.endDistLeg = b.endDistance;
            g.edgeDistLeg = b.edgeDistance;
            g.endDistGusset = b.endDistance;
            g.edgeDistGusset = b.edgeDistance + 10.0;
            g.bars = std::max(1, b.bars);
            g.barArea = b.area;
            g.barLegThickness = b.legThickness;
            g.barEccentricity = b.eccentricity;
            g.bar = b.steel;
            g.gussetThickness = in.gussetThickness;
            g.gussetWidth = in.gussetWidth;
            g.freeLength = in.freeLength;
            g.gusset = in.gusset;
            g.tension = tension ? N : 0.0;
            g.compression = tension ? 0.0 : N;
            g.gammaA1 = g1;
            g.gammaA2 = g2;
            g.gammaW2 = gw;
            for (Check c : checkGusset(g)) {
                if (c.name.rfind("Solda", 0) == 0 || c.name.rfind("Perna", 0) == 0) {
                    continue;   // a solda ao apoio e verificada com a resultante de todas as barras
                }
                c.name += tag;
                checks.push_back(std::move(c));
            }
        }
    }

    // ---- interface gousset - banzo, com a resultante das barras ------------------------------------------------------------------
    const TrussInterfaceForces r = trussInterfaceForces(in);
    if (in.chordMode != TrussNodeInput::Direct) {
        const double tg = in.gussetThickness, Lg = std::max(in.gussetWidth, 1.0);
        const double shear = std::abs(r.fx), normal = r.fy, M = std::abs(r.moment);
        if (in.chordMode == TrussNodeInput::ButtWeld) {
            // solda de penetracao total: a resistencia e a do metal-base da chapa (6.2.4)
            const double area = tg * Lg, z = tg * Lg * Lg / 6.0;
            const double sigma = std::abs(normal) / area + M / z;
            const double tau = shear / area;
            const double fN = in.gusset.fy / g1, fV = 0.6 * in.gusset.fy / g1;
            Check n = ratioN("Solda de topo gousset - banzo: tensao normal (N/Ag + M/W)", "6.2.4", sigma, fN);
            n.unit = "MPa";
            n.demand = sigma;
            n.resistance = fN;
            n.lines.push_back(fmtn("Fy = %.2f kN, M = %.2f kN.m, Ag = %.0f mm2, W = %.0f mm3", normal / 1000.0, M / 1e6, area, z));
            checks.push_back(n);
            Check s = ratioN("Solda de topo gousset - banzo: cisalhamento", "6.2.4", tau, fV);
            s.unit = "MPa";
            s.demand = tau;
            s.resistance = fV;
            checks.push_back(s);
            Check i = detail::dimensionless("Solda de topo gousset - banzo: interacao tensao normal x cisalhamento", "6.2.4", std::pow(sigma / fN, 2.0) + std::pow(tau / fV, 2.0));
            checks.push_back(i);
        } else if (in.chordMode == TrussNodeInput::FilletWeld) {
            const double te = 0.707 * in.chordWeldLeg;
            const double longL = 2.0 * Lg;
            const double endL = in.fourSides ? 2.0 * tg : tg;   // lados curtos soldados
            const double total = longL + endL;
            const double centroidShare = longL / std::max(total, 1.0);
            const double z = longL * Lg / 6.0 * 1.0 + endL * 0.0;   // modulo elastico (por mm de garganta) dos filetes longos
            const double fv = shear / total;
            const double fn = std::abs(normal) / total + (z > 0.0 ? M / z : 0.0);
            const double fr = std::hypot(fv, fn);
            const double cap = std::min(0.6 * te * in.weldFw / gw, 0.6 * std::min(tg, in.chordThickness) * std::min(in.gusset.fy, in.chord.fy) / g1);
            Check c = ratioN(in.fourSides ? "Solda gousset - banzo (filetes nos 4 lados, metodo elastico)" : "Solda gousset - banzo (filetes em 3 lados, metodo elastico)", "6.2.5", fr, cap);
            c.unit = "N/mm";
            c.demand = fr;
            c.resistance = cap;
            c.lines.push_back(fmtn("L total = %.0f mm (longos %.0f + curtos %.0f); fv = Fx/L = %.2f N/mm; fn = Fy/L + M/W = %.2f N/mm; compartilhamento dos longos = %.2f", total, longL, endL, fv, fn, centroidShare));
            c.lines.push_back(fmtn("fr = %.2f N/mm <= min(0,6 te fw / gw2 ; 0,6 t fy / ga1) = %.2f N/mm", fr, cap));
            checks.push_back(c);
            checks.push_back(detail::minimumRule("Perna minima do filete gousset - banzo", "6.2.6.2.1", in.chordWeldLeg, tg > 19.0 || in.chordThickness > 19.0 ? 8.0 : (std::min(tg, in.chordThickness) > 12.5 ? 6.0 : (std::min(tg, in.chordThickness) > 6.3 ? 5.0 : 3.0))));
        } else {
            BoltedInput b;
            b.grade = in.grade;
            b.db = in.db;
            b.threadInShearPlane = in.threadInShearPlane || !in.grade.highStrength;
            b.shearPlanes = 1;
            b.nx = 2;
            b.ny = std::max(1, in.chordBolts / 2);
            b.sx = 60.0;
            b.sy = in.chordBoltPitch;
            b.plateThickness = std::min(in.gussetThickness, in.chordThickness);
            b.plateFu = std::min(in.gusset.fu, in.chord.fu);
            b.endDistance = 35.0;
            b.edgeDistance = 35.0;
            b.shearX = shear;
            b.tension = std::max(normal, 0.0);
            b.momentZ = M;
            for (Check c : checkBolted(b)) {
                c.name += " - gousset ao banzo";
                checks.push_back(std::move(c));
            }
        }
    }
    return checks;
}

std::vector<Part> partsTrussNode(const TrussNodeInput &in)
{
    std::vector<Part> parts;
    if (in.chordMode != TrussNodeInput::Direct) {
        Part g;
        g.description = fmtn("Chapa de nó (gousset) %.1f x %.0f x %.0f mm", in.gussetThickness, in.gussetWidth, in.gussetHeight);
        g.quantity = 1;
        g.massEach = in.gussetThickness * in.gussetWidth * in.gussetHeight * 1e-9 * kSteelDensity;
        parts.push_back(g);
        if (in.chordMode == TrussNodeInput::FilletWeld || in.chordMode == TrussNodeInput::ButtWeld) {
            Part w;
            const double L = 2.0 * in.gussetWidth + (in.fourSides ? 2.0 : 1.0) * in.gussetThickness;
            w.description = in.chordMode == TrussNodeInput::ButtWeld ? fmtn("Solda de topo gousset - banzo (L = %.0f mm)", in.gussetWidth) : fmtn("Filete %.0f mm gousset - banzo", in.chordWeldLeg);
            w.quantity = 1;
            w.length = in.chordMode == TrussNodeInput::ButtWeld ? in.gussetWidth : L;
            w.unit = "mm";
            w.massEach = 0.5 * in.chordWeldLeg * in.chordWeldLeg * w.length * 1e-9 * kSteelDensity;
            parts.push_back(w);
        } else {
            Part b;
            b.description = fmtn("Parafuso d = %.0f mm gousset - banzo", in.db);
            b.quantity = in.chordBolts;
            b.massEach = 0.4;
            parts.push_back(b);
        }
    }
    int index = 0;
    for (const TrussBarData &bar : in.bars) {
        ++index;
        if (!bar.on) {
            continue;
        }
        if (in.barMode == TrussNodeInput::Welded || in.chordMode == TrussNodeInput::Direct) {
            Part w;
            w.description = fmtn("Filete %.0f mm barra %d (%.0f + %.0f mm)", bar.weldLeg, double(index), bar.lengthHeel, bar.lengthToe);
            w.quantity = std::max(1, bar.bars);
            w.length = bar.lengthHeel + bar.lengthToe;
            w.unit = "mm";
            w.massEach = 0.5 * bar.weldLeg * bar.weldLeg * w.length * 1e-9 * kSteelDensity;
            parts.push_back(w);
        } else {
            Part b;
            b.description = fmtn("Parafuso d = %.0f mm barra %d", in.db, double(index));
            b.quantity = std::max(1, bar.rows) * std::max(1, bar.lines);
            b.massEach = 0.4;
            parts.push_back(b);
        }
    }
    return parts;
}

} // namespace Structura::Design::Connections
