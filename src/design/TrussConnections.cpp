#include "design/TrussConnections.h"

#include "design/ConnectionHelpers.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design::Connections {

using namespace detail;

namespace {

double boltMassOf(double db, double grip)
{
    return (0.25 * kPi * db * db * (grip + 10.0) / 1e9 * kSteelDensity) * 1.35 + 0.04;
}

BoltedInput boltGroup(const BoltGrade &grade, double db, bool thread, int planes, int nAlong, int nAcross, double pitch, double gauge, double thickness,
                      const SteelProps &steel, double endDist, double edgeDist)
{
    BoltedInput b;
    b.grade = grade;
    b.db = db;
    b.threadInShearPlane = thread || !grade.highStrength;
    b.shearPlanes = planes;
    b.nx = std::max(1, nAcross);
    b.ny = std::max(1, nAlong);
    b.sx = gauge;
    b.sy = pitch;
    b.plateThickness = thickness;
    b.plateFu = steel.fu;
    b.endDistance = endDist;
    b.edgeDistance = edgeDist;
    return b;
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Chapa de no (gusset)
// ---------------------------------------------------------------------------------------------------------------------

double whitmoreWidth(const GussetInput &in)
{
    const double groupLength = (std::max(1, in.rows) - 1) * in.pitch;
    return (std::max(1, in.lines) - 1) * in.gauge + 2.0 * groupLength * std::tan(30.0 * kPi / 180.0);
}

double gussetLength(const GussetInput &in)
{
    return in.freeLength + (std::max(1, in.rows) - 1) * in.pitch + in.endDistGusset;
}

std::vector<Check> checkGusset(const GussetInput &in)
{
    std::vector<Check> checks;
    const double nMax = std::max(in.tension, in.compression);
    const int rows = std::max(1, in.rows);
    const int lines = std::max(1, std::min(2, in.lines));
    const int bars = std::max(1, std::min(2, in.bars));
    const double dh = standardHoleDiameter(in.db);
    const double hole = dh + 2.0;
    const double g1 = in.gammaA1, g2 = in.gammaA2;

    // --- parafusos: cisalhamento e contato na chapa; contato na diagonal --------------------------------------
    {
        BoltedInput b = boltGroup(in.grade, in.db, in.threadInShearPlane, bars, rows, lines, in.pitch, in.gauge, in.gussetThickness, in.gusset, in.endDistGusset,
                                  in.edgeDistGusset);
        b.shearY = nMax;
        appendChecks(checks, checkBolted(b), "", " (chapa)");
        // diagonal: o mesmo grupo, contato na aba (espessura somada das pecas)
        BoltedInput d = b;
        d.plateThickness = bars * in.barLegThickness;
        d.plateFu = in.bar.fu;
        d.endDistance = in.endDistLeg;
        d.edgeDistance = in.edgeDistLeg;
        appendChecks(checks, checkBolted(d), "Pressao", " (diagonal)");
        appendChecks(checks, checkBolted(d), "Distancia", " (diagonal)");
    }

    // --- diagonal: tracao na secao liquida efetiva -------------------------------------------------------------
    if (in.tension > 0.0 && in.barArea > 0.0) {
        const double lc = (rows - 1) * in.pitch;
        const double ct = lc > 0.0 ? std::max(0.0, std::min(0.9, 1.0 - in.barEccentricity / lc)) : 0.5;
        const double ag = in.barArea * bars;
        const double an = ag - bars * lines * hole * in.barLegThickness;
        const double ae = ct * an;
        const double rd = std::min(ag * in.bar.fy / g1, ae * in.bar.fu / g2);
        Check c = ratioN("Diagonal: tracao (escoamento e ruptura da secao liquida efetiva)", "5.2.2 / 5.2.5", in.tension, rd);
        c.lines.push_back(fmtn("Ct = 1 - ec/lc = 1 - %.1f/%.1f = %.3f (limitado a 0,9)", in.barEccentricity, lc, ct));
        c.lines.push_back(fmtn("An = Ag - n furos (dh + 2) t = %.0f - %.0f = %.0f mm2; Ae = Ct An = %.0f mm2", ag, ag - an, an, ae));
        c.lines.push_back(fmtn("Nt,Rd = min(Ag fy/ga1 = %.2f kN ; Ae fu/ga2 = %.2f kN)", ag * in.bar.fy / g1 / 1000.0, ae * in.bar.fu / g2 / 1000.0));
        checks.push_back(c);
    }

    // --- diagonal: cisalhamento de bloco -----------------------------------------------------------------------
    if (in.tension > 0.0) {
        const double shearLength = (rows - 1) * in.pitch + in.endDistLeg;
        const double tensionLength = in.edgeDistLeg + (lines - 1) * in.gauge;
        const BlockShearResult bs = blockShearRd(bars * in.barLegThickness, in.bar.fy, in.bar.fu, shearLength, rows - 0.5, tensionLength, lines - 0.5, hole, g2);
        Check c = ratioN("Diagonal: cisalhamento de bloco", "6.5.6", in.tension, bs.rd);
        c.lines.push_back(fmtn("Agv = %.0f, Anv = %.0f, Ant = %.0f mm2 (cisalhamento %.1f mm; tracao %.1f mm)", bs.agv, bs.anv, bs.ant, shearLength, tensionLength));
        c.lines.push_back(fmtn("Rd = min[(0,6 fu Anv + Cts fu Ant) ; (0,6 fy Agv + Cts fu Ant)]/ga2 = %.2f kN", bs.rd / 1000.0));
        checks.push_back(c);
    }

    // --- chapa: secao de Whitmore ------------------------------------------------------------------------------
    const double lwFull = whitmoreWidth(in);
    const double lw = in.gussetWidth > 0.0 ? std::min(lwFull, in.gussetWidth) : lwFull;
    const double agw = lw * in.gussetThickness;
    const double anw = std::max(0.0, lw - lines * hole) * in.gussetThickness;
    if (in.tension > 0.0) {
        const double yielding = agw * in.gusset.fy / g1;
        const double rupture = std::min(anw, 0.85 * agw) * in.gusset.fu / g2;
        Check c = ratioN("Chapa: tracao na secao de Whitmore", "5.2.2 / 6.5.2", in.tension, std::min(yielding, rupture));
        c.lines.push_back(fmtn("lw = (linhas - 1) g + 2 L tan30 = %.1f mm (limitada a %.1f mm); Agw = %.0f mm2; Anw = %.0f mm2", lwFull, in.gussetWidth, agw, anw));
        c.lines.push_back(fmtn("Escoamento: Agw fy/ga1 = %.2f kN; ruptura: min(Anw ; 0,85 Agw) fu/ga2 = %.2f kN", yielding / 1000.0, rupture / 1000.0));
        checks.push_back(c);
    }
    if (in.compression > 0.0) {
        const double k = 0.65;
        const double r = in.gussetThickness / std::sqrt(12.0);
        const double kl = k * std::max(in.freeLength, 1.0);
        const double lambda0 = (kl / r) / kPi * std::sqrt(in.gusset.fy / kModulusE);
        const double chi = chiCompression(lambda0);
        const double rd = chi * agw * in.gusset.fy / g1;
        Check c = ratioN("Chapa: compressao na secao de Whitmore", "5.3.3", in.compression, rd);
        c.lines.push_back(fmtn("K = %.2f, L = %.1f mm, r = t/raiz(12) = %.2f mm: KL/r = %.1f; lambda0 = %.3f; chi = %.3f", k, in.freeLength, r, kl / r, lambda0, chi));
        c.lines.push_back(fmtn("Nc,Rd = chi Agw fy/ga1 = %.3f x %.0f x %.0f / 1,10 = %.2f kN (coluna de Whitmore, extremidade engastada com deslocamento lateral impedido, K = 0,65)",
                               chi, agw, in.gusset.fy, rd / 1000.0));
        checks.push_back(c);
    }

    // --- chapa: cisalhamento de bloco, ao longo das linhas extremas ----------------------------------------------
    if (in.tension > 0.0) {
        const double shearLength = (rows - 1) * in.pitch + in.endDistGusset;
        const double tensionLength = (lines - 1) * in.gauge;
        const BlockShearResult bs = blockShearRd(in.gussetThickness, in.gusset.fy, in.gusset.fu, 2.0 * shearLength, 2.0 * (rows - 0.5), tensionLength,
                                                 std::max(0, lines - 1) * 1.0, hole, g2);
        Check c = ratioN("Chapa: cisalhamento de bloco", "6.5.6", in.tension, bs.rd);
        c.lines.push_back(fmtn("2 planos de cisalhamento de %.1f mm; tracao %.1f mm: Agv = %.0f, Anv = %.0f, Ant = %.0f mm2", shearLength, tensionLength, bs.agv, bs.anv, bs.ant));
        c.lines.push_back(fmtn("Rd = %.2f kN", bs.rd / 1000.0));
        checks.push_back(c);
    }

    // --- solda da chapa ao apoio ---------------------------------------------------------------------------------
    {
        WeldInput w;
        w.leg = in.weldLeg;
        w.length = 2.0 * std::max(0.0, in.gussetWidth > 0.0 ? in.gussetWidth : lwFull);
        w.fw = in.weldFw;
        w.angleDeg = in.weldAngleDeg;
        w.plateThickness = std::min(in.gussetThickness, in.supportThickness);
        w.plateFy = std::min(in.gusset.fy, in.support.fy);
        w.plateFu = std::min(in.gusset.fu, in.support.fu);
        w.force = nMax;
        w.gammaW2 = in.gammaW2;
        w.gammaA1 = g1;
        w.gammaA2 = g2;
        appendChecks(checks, checkFilletWeld(w), "", " (solda chapa-apoio)");
        if (!checks.empty()) {
            checks.back().lines.push_back("A solda e verificada para a forca total da diagonal (metodo da forca uniforme conservador, sem decomposicao entre o banzo e os montantes).");
        }
    }
    return checks;
}

std::vector<Part> partsGusset(const GussetInput &in)
{
    std::vector<Part> parts;
    const double length = gussetLength(in);
    const double width = in.gussetWidth > 0.0 ? in.gussetWidth : whitmoreWidth(in) + 2.0 * in.edgeDistGusset;
    Part plate;
    plate.description = fmtn("Chapa de no %.1f x %.0f x %.0f mm", in.gussetThickness, width, length);
    plate.quantity = 1;
    plate.massEach = plateMass(length, width, in.gussetThickness);
    parts.push_back(plate);
    Part bolts;
    bolts.description = fmtn("Parafuso d = %.0f mm (+ porca e arruelas)", in.db);
    bolts.quantity = std::max(1, in.rows) * std::max(1, std::min(2, in.lines));
    bolts.massEach = boltMassOf(in.db, in.gussetThickness + std::max(1, in.bars) * in.barLegThickness);
    parts.push_back(bolts);
    Part weld;
    weld.description = fmtn("Filete de solda %.0f mm (2 x %.0f mm)", in.weldLeg, width);
    weld.quantity = 1;
    weld.length = 2.0 * width;
    weld.unit = "mm";
    weld.massEach = 0.5 * in.weldLeg * in.weldLeg * 2.0 * width / 1e9 * kSteelDensity;
    parts.push_back(weld);
    return parts;
}

GussetProposal proposeGusset(GussetInput base)
{
    GussetProposal best;
    double bestCost = 1e300;
    static const double diameters[] = {16.0, 20.0, 22.0, 24.0};
    static const double thicknesses[] = {6.3, 8.0, 9.5, 12.5, 16.0, 19.0, 25.0};
    static const double legs[] = {4.0, 5.0, 6.0, 8.0, 10.0};
    for (const double db : diameters) {
        for (int lines = 1; lines <= 2; ++lines) {
            for (int rows = 2; rows <= 8; ++rows) {
                for (const double t : thicknesses) {
                    for (const double leg : legs) {
                        GussetInput in = base;
                        in.db = db;
                        in.lines = lines;
                        in.rows = rows;
                        in.pitch = std::max(base.pitch, 3.0 * db);
                        in.gauge = std::max(base.gauge, 3.0 * db);
                        const double minEdge = minimumEdgeDistance(db);
                        // distancias a borda entre o minimo de 6.3.11 e o maximo de 6.3.12 (12 t, ate 150 mm)
                        const double maxLeg = std::max(minEdge, std::min(150.0, 12.0 * std::max(1, base.bars) * base.barLegThickness));
                        const double maxPlate = std::max(minEdge, std::min(150.0, 12.0 * t));
                        in.endDistLeg = std::min(std::max(base.endDistLeg, minEdge), maxLeg);
                        in.edgeDistLeg = std::min(std::max(base.edgeDistLeg, minEdge), maxLeg);
                        in.endDistGusset = std::min(std::max(base.endDistGusset, minEdge), maxPlate);
                        in.edgeDistGusset = std::min(std::max(base.edgeDistGusset, minEdge), maxPlate);
                        in.gussetThickness = t;
                        in.weldLeg = leg;
                        in.gussetWidth = std::max(base.gussetWidth, whitmoreWidth(in));
                        const auto checks = checkGusset(in);
                        const double worst = worstRatio(checks);
                        if (worst > 1.0 + 1e-9) {
                            continue;
                        }
                        double cost = 0.0;
                        for (const auto &p : partsGusset(in)) {
                            cost += p.totalMass();
                        }
                        cost += 0.01 * 2.0 * in.gussetWidth * leg * leg / 100.0;
                        if (cost < bestCost) {
                            bestCost = cost;
                            best.found = true;
                            best.input = in;
                            best.maxRatio = worst;
                        }
                    }
                }
            }
        }
    }
    return best;
}

// ---------------------------------------------------------------------------------------------------------------------
// Emenda de viga
// ---------------------------------------------------------------------------------------------------------------------

double spliceFlangePlateLength(const SpliceInput &in)
{
    return 2.0 * ((std::max(1, in.flangeRows) - 1) * in.flangePitch + in.flangeEnd) + in.gap;
}

double spliceWebPlateLength(const SpliceInput &in)
{
    return 2.0 * ((std::max(1, in.webCols) - 1) * in.webGauge + in.webEdgeH) + in.gap;
}

double spliceWebPlateHeight(const SpliceInput &in)
{
    return (std::max(1, in.webRows) - 1) * in.webPitch + 2.0 * in.webEdgeV;
}

std::vector<Check> checkSplice(const SpliceInput &in)
{
    std::vector<Check> checks;
    const double g1 = in.gammaA1, g2 = in.gammaA2;
    const double dh = standardHoleDiameter(in.db);
    const double hole = dh + 2.0;
    const double lever = std::max(1.0, in.beamDepth - in.flangeThickness);
    const double webHeight = in.webHeight > 0.0 ? in.webHeight : in.beamDepth - 2.0 * in.flangeThickness;
    const double flangeArea = in.flangeWidth * in.flangeThickness;
    const double areaTotal = in.beamArea > 0.0 ? in.beamArea : 2.0 * flangeArea + webHeight * in.webThickness;
    const double axialFlange = in.axial * flangeArea / areaTotal;   // parcela do esforco axial em cada mesa
    const double fTension = std::abs(in.moment) / lever + axialFlange;
    const double fCompression = std::abs(in.moment) / lever - axialFlange;
    const double fMax = std::max(std::abs(fTension), std::abs(fCompression));
    const int fRows = std::max(1, in.flangeRows);
    const int fLines = std::max(1, in.flangeLines);
    const bool innerPlates = in.innerPlateThickness > 0.0;
    const int planes = innerPlates ? 2 : 1;

    checks.push_back([&] {
        Check c = dimensionless("Distribuicao dos esforcos", "6.1", 0.0);
        c.unit = "-";
        c.lines.push_back(fmtn("Mesas: Ff = M/(d - tf) +- N Af/A = %.2f / %.1f +- %.2f = %.2f kN (tracao) e %.2f kN (compressao)",
                               std::abs(in.moment) / 1e6, lever / 1000.0, axialFlange / 1000.0, fTension / 1000.0, fCompression / 1000.0));
        c.lines.push_back(fmtn("Alma: V = %.2f kN com excentricidade ate o centro do grupo de parafusos", in.shear / 1000.0));
        return c;
    }());

    // --- talas e parafusos das mesas -----------------------------------------------------------------------------
    {
        BoltedInput b = boltGroup(in.grade, in.db, in.threadInShearPlane, planes, fRows, fLines, in.flangePitch, in.flangeGauge, in.flangeThickness, in.beam,
                                  in.flangeEnd, in.flangeEdge);
        b.shearY = fMax;
        appendChecks(checks, checkBolted(b), "", " (mesa)");
        BoltedInput p = b;
        p.plateThickness = innerPlates ? 2.0 * std::min(in.outerPlateThickness, in.innerPlateThickness) : in.outerPlateThickness;
        p.plateFu = in.plates.fu;
        appendChecks(checks, checkBolted(p), "Pressao", " (tala de mesa)");
    }
    {
        const double agOuter = in.outerPlateWidth * in.outerPlateThickness;
        const double agInner = innerPlates ? 2.0 * in.innerPlateWidth * in.innerPlateThickness : 0.0;
        const double ag = agOuter + agInner;
        const double an = ag - fLines * hole * (in.outerPlateThickness + (innerPlates ? in.innerPlateThickness : 0.0));
        const double anEff = std::min(an, 0.85 * ag);
        const double tensionRd = std::min(ag * in.plates.fy / g1, anEff * in.plates.fu / g2);
        Check t = ratioN("Talas de mesa: tracao", "5.2.2 / 6.5.2", std::max(0.0, fTension), tensionRd);
        t.lines.push_back(fmtn("Ag = %.0f mm2; An = Ag - %d furos (dh+2) t = %.0f mm2; Ae = min(An ; 0,85 Ag) = %.0f mm2", ag, fLines, an, anEff));
        t.lines.push_back(fmtn("Nt,Rd = min(Ag fy/ga1 = %.2f ; Ae fu/ga2 = %.2f) kN", ag * in.plates.fy / g1 / 1000.0, anEff * in.plates.fu / g2 / 1000.0));
        checks.push_back(t);
        Check c = ratioN("Talas de mesa: compressao (escoamento)", "5.3", std::max(0.0, fCompression), ag * in.plates.fy / g1);
        c.lines.push_back(fmtn("Talas curtas, com folga de %.0f mm entre as vigas: Nc,Rd = Ag fy/ga1 = %.2f kN (flambagem local da tala nao governa se a distancia entre parafusos <= 13 t)", in.gap,
                               ag * in.plates.fy / g1 / 1000.0));
        checks.push_back(c);
        Check s = rangeMax("Talas de mesa: esbeltez do trecho livre", "5.3", in.gap + in.flangePitch, 13.0 * in.outerPlateThickness, "mm");
        s.lines.push_back(fmtn("Trecho entre parafusos mais proximos da emenda = %.0f mm <= 13 t = %.0f mm", in.gap + in.flangePitch, 13.0 * in.outerPlateThickness));
        checks.push_back(s);
    }
    {
        const double ag = flangeArea;
        const double an = ag - fLines * hole * in.flangeThickness;
        const double anEff = std::min(an, 0.85 * ag);
        const double rd = std::min(ag * in.beam.fy / g1, anEff * in.beam.fu / g2);
        Check c = ratioN("Mesa da viga: tracao na secao liquida", "5.2.2", std::max(0.0, fTension), rd);
        c.lines.push_back(fmtn("An = %.0f mm2; Nt,Rd = min(%.2f ; %.2f) kN", an, ag * in.beam.fy / g1 / 1000.0, anEff * in.beam.fu / g2 / 1000.0));
        checks.push_back(c);
        const double shearLength = (fRows - 1) * in.flangePitch + in.flangeEnd;
        const double edge = (in.flangeWidth - (fLines - 1) * in.flangeGauge) / 2.0;
        const BlockShearResult bs = blockShearRd(in.flangeThickness, in.beam.fy, in.beam.fu, 2.0 * shearLength, 2.0 * (fRows - 0.5), (fLines - 1) * in.flangeGauge,
                                                 std::max(0, fLines - 1), hole, g2);
        Check b = ratioN("Mesa da viga: cisalhamento de bloco", "6.5.6", std::max(0.0, fTension), bs.rd);
        b.lines.push_back(fmtn("2 planos de %.1f mm e tracao de %.1f mm (borda = %.1f mm): Rd = %.2f kN", shearLength, (fLines - 1) * in.flangeGauge, edge, bs.rd / 1000.0));
        checks.push_back(b);
    }

    // --- almas: parafusos com excentricidade, talas ------------------------------------------------------------
    {
        const int wRows = std::max(1, in.webRows);
        const int wCols = std::max(1, in.webCols);
        const double eccentricity = in.gap / 2.0 + in.webEdgeH + (wCols - 1) * in.webGauge / 2.0;
        BoltedInput b = boltGroup(in.grade, in.db, in.threadInShearPlane, 2, wRows, wCols, in.webPitch, in.webGauge, in.webThickness, in.beam, in.webEdgeV,
                                  in.webEdgeH);
        b.shearY = std::abs(in.shear);
        b.shearX = 0.0;
        b.momentZ = std::abs(in.shear) * eccentricity;
        appendChecks(checks, checkBolted(b), "", " (alma)");
        if (!checks.empty()) {
            checks.back().lines.push_back(fmtn("Excentricidade da alma: e = gap/2 + distancia ao grupo = %.1f mm; M = V e = %.2f kN.m", eccentricity, b.momentZ / 1e6));
        }
        BoltedInput p = b;
        p.plateThickness = 2.0 * in.webPlateThickness;
        p.plateFu = in.plates.fu;
        appendChecks(checks, checkBolted(p), "Pressao", " (tala de alma)");

        const double h = spliceWebPlateHeight(in);
        const double agv = 2.0 * in.webPlateThickness * h;
        const double anv = 2.0 * in.webPlateThickness * (h - wRows * hole);
        const double rd = std::min(0.6 * agv * in.plates.fy / g1, 0.6 * anv * in.plates.fu / g2);
        Check c = ratioN("Talas de alma: cisalhamento", "5.4.3 / 6.5.5", std::abs(in.shear), rd);
        c.lines.push_back(fmtn("2 talas %.1f x %.0f mm: Agv = %.0f, Anv = %.0f mm2; V,Rd = min(0,6 Agv fy/ga1 ; 0,6 Anv fu/ga2) = %.2f kN", in.webPlateThickness, h, agv, anv, rd / 1000.0));
        checks.push_back(c);
        const BlockShearResult bs = blockShearRd(2.0 * in.webPlateThickness, in.plates.fy, in.plates.fu, (wRows - 1) * in.webPitch + in.webEdgeV, wRows - 0.5,
                                                 in.webEdgeH + (wCols - 1) * in.webGauge, wCols - 0.5, hole, g2);
        Check bsC = ratioN("Talas de alma: cisalhamento de bloco", "6.5.6", std::abs(in.shear), bs.rd);
        bsC.lines.push_back(fmtn("Rd = %.2f kN", bs.rd / 1000.0));
        checks.push_back(bsC);
        const double z = 2.0 * in.webPlateThickness * h * h / 4.0;
        Check m = ratioNmm("Talas de alma: flexao pela excentricidade", "5.4.2", std::abs(in.shear) * eccentricity, z * in.plates.fy / g1);
        m.lines.push_back(fmtn("Z = 2 t h^2/4 = %.0f mm3; Md = Z fy/ga1 = %.2f kN.m", z, z * in.plates.fy / g1 / 1e6));
        checks.push_back(m);
    }
    return checks;
}

std::vector<Part> partsSplice(const SpliceInput &in)
{
    std::vector<Part> parts;
    const bool innerPlates = in.innerPlateThickness > 0.0;
    const double fl = spliceFlangePlateLength(in);
    Part outer;
    outer.description = fmtn("Tala de mesa externa %.1f x %.0f x %.0f mm", in.outerPlateThickness, in.outerPlateWidth, fl);
    outer.quantity = 2;
    outer.massEach = plateMass(fl, in.outerPlateWidth, in.outerPlateThickness);
    parts.push_back(outer);
    if (innerPlates) {
        Part inner;
        inner.description = fmtn("Tala de mesa interna %.1f x %.0f x %.0f mm", in.innerPlateThickness, in.innerPlateWidth, fl);
        inner.quantity = 4;
        inner.massEach = plateMass(fl, in.innerPlateWidth, in.innerPlateThickness);
        parts.push_back(inner);
    }
    const double wl = spliceWebPlateLength(in);
    const double wh = spliceWebPlateHeight(in);
    Part web;
    web.description = fmtn("Tala de alma %.1f x %.0f x %.0f mm", in.webPlateThickness, wl, wh);
    web.quantity = 2;
    web.massEach = plateMass(wl, wh, in.webPlateThickness);
    parts.push_back(web);
    Part fb;
    fb.description = fmtn("Parafuso d = %.0f mm das mesas (+ porca e arruelas)", in.db);
    fb.quantity = 2 * 2 * std::max(1, in.flangeRows) * std::max(1, in.flangeLines);
    fb.massEach = boltMassOf(in.db, in.flangeThickness + in.outerPlateThickness + in.innerPlateThickness);
    parts.push_back(fb);
    Part wb;
    wb.description = fmtn("Parafuso d = %.0f mm da alma (+ porca e arruelas)", in.db);
    wb.quantity = 2 * std::max(1, in.webRows) * std::max(1, in.webCols);
    wb.massEach = boltMassOf(in.db, in.webThickness + 2.0 * in.webPlateThickness);
    parts.push_back(wb);
    return parts;
}

SpliceProposal proposeSplice(SpliceInput base)
{
    SpliceProposal best;
    double bestCost = 1e300;
    static const double diameters[] = {16.0, 20.0, 22.0, 24.0};
    static const double flangePlates[] = {8.0, 9.5, 12.5, 16.0, 19.0, 25.0};
    static const double webPlates[] = {6.3, 8.0, 9.5, 12.5};
    for (const double db : diameters) {
        for (int fRows = 2; fRows <= 6; ++fRows) {
            for (const double tp : flangePlates) {
                for (const bool inner : {false, true}) {
                    for (int wRows = 2; wRows <= 8; ++wRows) {
                        for (const double tw : webPlates) {
                            SpliceInput in = base;
                            in.db = db;
                            const double edge = minimumEdgeDistance(db);
                            in.flangeRows = fRows;
                            in.flangePitch = std::max(base.flangePitch, 3.0 * db);
                            in.flangeEnd = std::max(base.flangeEnd, edge);
                            in.flangeEdge = std::max(base.flangeEdge, edge);
                            in.flangeGauge = std::max(base.flangeGauge, 3.0 * db);
                            in.outerPlateThickness = tp;
                            in.outerPlateWidth = std::min(in.flangeWidth, std::max(base.outerPlateWidth, in.flangeGauge * (in.flangeLines - 1) + 2.0 * in.flangeEdge));
                            in.innerPlateThickness = inner ? tp : 0.0;
                            in.webRows = wRows;
                            in.webPitch = std::max(base.webPitch, 3.0 * db);
                            in.webEdgeH = std::max(base.webEdgeH, edge);
                            in.webEdgeV = std::max(base.webEdgeV, edge);
                            in.webGauge = std::max(base.webGauge, 3.0 * db);
                            in.webPlateThickness = tw;
                            const double room = (in.webHeight > 0.0 ? in.webHeight : in.beamDepth - 2.0 * in.flangeThickness) - 20.0;
                            if (spliceWebPlateHeight(in) > room) {
                                continue;
                            }
                            const auto checks = checkSplice(in);
                            const double worst = worstRatio(checks);
                            if (worst > 1.0 + 1e-9) {
                                continue;
                            }
                            double cost = 0.0;
                            for (const auto &p : partsSplice(in)) {
                                cost += p.totalMass();
                            }
                            if (cost < bestCost) {
                                bestCost = cost;
                                best.found = true;
                                best.input = in;
                                best.maxRatio = worst;
                            }
                        }
                    }
                }
            }
        }
    }
    return best;
}

} // namespace Structura::Design::Connections
