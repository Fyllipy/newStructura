#include "design/CoverSplice.h"

#include "design/ConnectionHelpers.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design::Connections {

using detail::blockShearRd;
using detail::fmtn;
using detail::kSteelDensity;
using detail::ratioN;
using detail::ratioNmm;

double coverSpliceLength(const CoverSpliceInput &in)
{
    return in.gap + 2.0 * (in.endDistance + (std::max(1, in.rows) - 1) * in.pitch);
}

std::vector<Check> checkCoverSplice(const CoverSpliceInput &in)
{
    std::vector<Check> checks;
    const double N = std::abs(in.axial);
    const bool tension = in.axial >= 0.0;
    const int rows = std::max(1, in.rows), lines = std::max(1, in.lines);
    const double hole = standardHoleDiameter(in.db) + 2.0;
    const int planes = std::clamp(in.covers, 1, 2);
    const double lc = (rows - 1) * in.pitch;   // comprimento da ligacao

    // ---- parafusos de cada lado da junta ---------------------------------------------------------------------------------------
    BoltedInput b;
    b.grade = in.grade;
    b.db = in.db;
    b.threadInShearPlane = in.threadInShearPlane || !in.grade.highStrength;
    b.shearPlanes = planes;
    b.nx = rows;
    b.ny = lines;
    b.sx = in.pitch;
    b.sy = in.gauge;
    b.plateThickness = in.thickness;
    b.plateFu = in.bar.fu;
    b.endDistance = in.endDistance;
    b.edgeDistance = in.edgeDistance;
    b.shearX = N;
    b.grip = in.thickness + planes * in.coverThickness;
    b.connectionLength = lc;
    const auto boltChecks = checkBolted(b);
    detail::appendChecks(checks, boltChecks, "Cisalhamento", planes == 2 ? " - corte duplo" : " - corte simples");
    detail::appendChecks(checks, boltChecks, "Pressao de contato", " - barra");
    detail::appendChecks(checks, boltChecks, "Espacamento", "");
    detail::appendChecks(checks, boltChecks, "Distancia", "");
    BoltedInput c = b;
    c.plateThickness = planes * in.coverThickness;
    c.plateFu = in.cover.fu;
    detail::appendChecks(checks, checkBolted(c), "Pressao de contato", " - cobrejuntas");

    // ---- barra: secao bruta e liquida efetiva ---------------------------------------------------------------------------------
    const double ag = in.area > 0.0 ? in.area : in.thickness * in.width;
    const double an = ag - lines * hole * in.thickness;
    double ct = 1.0;
    if (in.shape == CoverSpliceInput::Angle && lc > 0.0) {
        ct = std::clamp(1.0 - in.eccentricity / std::max(lc + in.endDistance, 1.0), 0.6, 0.9);
    }
    const double ae = ct * an;
    if (tension) {
        const double yield = ag * in.bar.fy / in.gammaA1, rupture = ae * in.bar.fu / in.gammaA2;
        Check t = ratioN("Barra: tracao (escoamento da secao bruta e ruptura da liquida efetiva)", "5.2.2 / 5.2.5", N, std::min(yield, rupture));
        t.lines.push_back(fmtn("Ag = %.0f mm2; An = Ag - %d dh t = %.0f mm2; Ct = %.2f; Ae = Ct An = %.0f mm2", ag, lines, an, ct, ae));
        t.lines.push_back(fmtn("Nt,Rd = min(Ag fy/ga1 = %.2f kN ; Ae fu/ga2 = %.2f kN)", yield / 1000.0, rupture / 1000.0));
        checks.push_back(t);
        const detail::BlockShearResult bs = blockShearRd(in.thickness, in.bar.fy, in.bar.fu, lc + in.endDistance, rows - 0.5, (lines - 1) * in.gauge + in.edgeDistance, lines - 0.5 > 0.0 ? lines - 0.5 : 0.5, hole, in.gammaA2);
        Check s = ratioN("Barra: rasgamento (cisalhamento de bloco)", "6.5.6", N, bs.rd);
        s.lines.push_back(fmtn("Agv = %.0f; Anv = %.0f; Ant = %.0f mm2; Rr,Rd = %.2f kN", bs.agv, bs.anv, bs.ant, bs.rd / 1000.0));
        checks.push_back(s);
    } else {
        checks.push_back(ratioN("Barra: compressao (escoamento da secao bruta, trecho curto da junta)", "5.3", N, ag * in.bar.fy / in.gammaA1));
    }

    // ---- cobrejuntas ----------------------------------------------------------------------------------------------------------------------
    const double coverAg = planes * in.coverThickness * in.coverWidth;
    const double coverAn = coverAg - planes * lines * hole * in.coverThickness;
    if (tension) {
        const double yield = coverAg * in.cover.fy / in.gammaA1, rupture = std::min(coverAn, 0.85 * coverAg) * in.cover.fu / in.gammaA2;
        Check t = ratioN("Cobrejuntas: tracao (escoamento e ruptura da secao liquida)", "5.2.2 / 6.5.2", N, std::min(yield, rupture));
        t.lines.push_back(fmtn("%d cobrejunta(s) %.1f x %.0f mm: Ag = %.0f mm2; An = %.0f mm2 (<= 0,85 Ag)", planes, in.coverThickness, in.coverWidth, coverAg, coverAn));
        checks.push_back(t);
    } else {
        checks.push_back(ratioN("Cobrejuntas: compressao (escoamento da secao bruta)", "5.3", N, coverAg * in.cover.fy / in.gammaA1));
        const double free = in.gap + in.pitch;
        Check s = detail::rangeMax("Cobrejuntas: esbeltez do trecho livre (<= 13 t)", "5.3 / detalhamento", free, 13.0 * in.coverThickness, "mm");
        checks.push_back(s);
    }
    // cobrejunta simples (corte simples): flexao pela excentricidade entre a barra e a cobrejunta
    if (planes == 1) {
        const double e = (in.thickness + in.coverThickness) / 2.0;
        const double mrd = in.width * in.thickness * in.thickness / 4.0 * in.bar.fy / in.gammaA1;
        Check m = ratioNmm("Flexao secundaria pela excentricidade da cobrejunta simples", "5.4 / informativa", N * e * 0.5, mrd);
        m.lines.push_back(fmtn("e = (t + tc)/2 = %.1f mm; M = 0,5 N e", e));
        checks.push_back(m);
    }
    return checks;
}

std::vector<Part> partsCoverSplice(const CoverSpliceInput &in)
{
    std::vector<Part> parts;
    const double len = coverSpliceLength(in);
    Part cover;
    cover.description = fmtn("Cobrejunta %.1f x %.0f x %.0f mm", in.coverThickness, in.coverWidth, len);
    cover.quantity = std::clamp(in.covers, 1, 2);
    cover.massEach = in.coverThickness * in.coverWidth * len * 1e-9 * kSteelDensity;
    parts.push_back(cover);
    Part bolts;
    bolts.description = fmtn("Parafuso d = %.0f mm (+ porca e arruelas)", in.db);
    bolts.quantity = 2 * std::max(1, in.rows) * std::max(1, in.lines);
    bolts.massEach = (0.25 * detail::kPi * in.db * in.db * (in.thickness + std::clamp(in.covers, 1, 2) * in.coverThickness + 10.0) * 1e-9 * kSteelDensity) * 1.35 + 0.04;
    parts.push_back(bolts);
    return parts;
}

// ---------------------------------------------------------------------------------------------------------------------
// Emenda de tubo com chapas de extremidade
// ---------------------------------------------------------------------------------------------------------------------

std::vector<Check> checkTubeFlange(const TubeFlangeInput &in)
{
    std::vector<Check> checks;
    const int n = std::max(3, in.bolts);
    const double bc = in.boltCircle;
    const double ab = 0.25 * detail::kPi * in.db * in.db;
    const double fub = in.grade.fub;
    const double ftRd = 0.75 * ab * fub / in.gammaA2;
    const double fvRd = (in.threadInShearPlane || !in.grade.highStrength ? 0.45 : 0.56) * ab * fub / in.gammaA2;
    // tracao por parafuso: axial + momento (grupo circular: maior fileira = 2 M / (n D) com os parafusos no circulo)
    const double nMax = in.axial / n + 2.0 * in.moment / (n * bc) * 1.0;
    const double ft0 = std::max(nMax, 0.0);
    Check t = ratioN("Parafusos tracionados (axial + momento)", "6.3.3.1", ft0, ftRd);
    t.lines.push_back(fmtn("Ft,Sd = N/n + 2 M/(n Dc) = %.2f kN (n = %d, Dc = %.0f mm); Ft,Rd = 0,75 Ab fub/ga2 = %.2f kN", ft0 / 1000.0, n, bc, ftRd / 1000.0));
    checks.push_back(t);
    Check v = ratioN("Parafusos: cisalhamento (cortante dividido entre os parafusos)", "6.3.3.2", std::abs(in.shear) / n, fvRd);
    checks.push_back(v);
    // efeito de alavanca: faixa tributaria do parafuso na chapa
    const double a = std::max(15.0, (in.plateDiameter - bc) / 2.0);
    const double b = std::max(15.0, (bc - in.tubeDiameter) / 2.0 - 0.8 * in.weldLeg);
    PryingInput p;
    p.db = in.db;
    p.tensionForce = ft0;
    p.boltResistance = ftRd;
    p.a = a;
    p.b = b;
    p.p = detail::kPi * bc / n;
    p.holeWidth = standardHoleDiameter(in.db) + 2.0;
    p.thickness = in.plateThickness;
    p.fu = in.plate.fu;
    p.gammaA1 = in.gammaA1;
    for (Check c : checkPrying(p)) {
        c.name += " - chapa de extremidade";
        checks.push_back(std::move(c));
    }
    // solda tubo - chapa: perimetro externo
    const double perimeter = detail::kPi * in.tubeDiameter;
    const double te = 0.707 * in.weldLeg;
    const double weldCap = 0.6 * te * in.weldFw / in.gammaW2;
    const double sigma = std::abs(in.axial) / perimeter + 4.0 * std::abs(in.moment) / (detail::kPi * in.tubeDiameter * in.tubeDiameter);
    const double tau = std::abs(in.shear) / perimeter;
    Check w = ratioN("Solda tubo - chapa (filete externo)", "6.2.5", std::hypot(sigma, tau), weldCap);
    w.unit = "N/mm";
    w.demand = std::hypot(sigma, tau);
    w.resistance = weldCap;
    w.lines.push_back(fmtn("fr = raiz(f_normal^2 + f_cisalhante^2) = %.2f N/mm; 0,6 te fw/gw2 = %.2f N/mm", w.demand, weldCap));
    checks.push_back(w);
    Check base = ratioN("Solda - metal-base do tubo", "6.2.5 / 6.5", std::hypot(sigma, tau), 0.6 * in.tubeThickness * in.tube.fy / in.gammaA1);
    base.unit = "N/mm";
    base.demand = std::hypot(sigma, tau);
    base.resistance = 0.6 * in.tubeThickness * in.tube.fy / in.gammaA1;
    checks.push_back(base);
    checks.push_back(detail::minimumRule("Perna minima do filete", "6.2.6.2.1 / Tab. 11", in.weldLeg, std::min(in.tubeThickness, in.plateThickness) > 12.5 ? 6.0 : 5.0));
    // disposicoes
    checks.push_back(detail::minimumRule("Distancia do furo a borda da chapa", "6.3.11", (in.plateDiameter - bc) / 2.0, minimumEdgeDistance(in.db)));
    checks.push_back(detail::minimumRule("Espacamento entre parafusos no circulo (>= 3 db)", "6.3.9", detail::kPi * bc / n, 3.0 * in.db));
    // tubo: resistencia da secao
    const double tubeArea = detail::kPi * (in.tubeDiameter - in.tubeThickness) * in.tubeThickness;
    Check s = ratioN("Tubo: tracao na secao bruta junto a emenda", "5.2.2", std::max(in.axial, 0.0), tubeArea * in.tube.fy / in.gammaA1);
    checks.push_back(s);
    return checks;
}

std::vector<Part> partsTubeFlange(const TubeFlangeInput &in)
{
    std::vector<Part> parts;
    Part plate;
    plate.description = fmtn("Chapa de extremidade Ø%.0f x %.1f mm (furada)", in.plateDiameter, in.plateThickness);
    plate.quantity = 2;
    plate.massEach = 0.25 * detail::kPi * in.plateDiameter * in.plateDiameter * in.plateThickness * 1e-9 * kSteelDensity;
    parts.push_back(plate);
    Part bolts;
    bolts.description = fmtn("Parafuso d = %.0f mm (+ porca e arruelas)", in.db);
    bolts.quantity = std::max(3, in.bolts);
    bolts.massEach = (0.25 * detail::kPi * in.db * in.db * (2.0 * in.plateThickness + 10.0) * 1e-9 * kSteelDensity) * 1.35 + 0.04;
    parts.push_back(bolts);
    Part weld;
    weld.description = fmtn("Filete %.0f mm tubo - chapa", in.weldLeg);
    weld.quantity = 2;
    weld.length = detail::kPi * in.tubeDiameter;
    weld.unit = "mm";
    weld.massEach = 0.5 * in.weldLeg * in.weldLeg * weld.length * 1e-9 * kSteelDensity;
    parts.push_back(weld);
    return parts;
}

} // namespace Structura::Design::Connections
