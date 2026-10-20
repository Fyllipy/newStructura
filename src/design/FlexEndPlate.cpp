#include "design/FlexEndPlate.h"

#include "design/ConnectionHelpers.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design::Connections {

using detail::fmtn;
using detail::kPi;
using detail::ratioN;

double flexEndPlateHeight(const FlexEndPlateInput &in)
{
    return in.plateHeight > 0.0 ? in.plateHeight : (std::max(in.rows, 1) - 1) * in.pitch + 2.0 * in.edgeV;
}

double flexEndPlateWidth(const FlexEndPlateInput &in)
{
    return in.gauge + 2.0 * in.edgeH;
}

std::vector<Check> checkFlexEndPlate(const FlexEndPlateInput &in)
{
    std::vector<Check> out;
    const double V = std::abs(in.shear);
    const double N = in.axial;
    const double hp = flexEndPlateHeight(in);
    const double dh = standardHoleDiameter(in.db);
    const int rows = std::max(in.rows, 1);

    // geometria
    if (in.beamDepth > 0.0) {
        const double clear = in.beamDepth - 2.0 * in.beamTf - 2.0 * in.beamFillet;
        Check c = detail::rangeMax("Altura da chapa <= trecho reto da alma da viga", "NBR 8800:2024, 6.2.6 (disposicao)", hp, clear, "mm");
        c.lines.push_back(fmtn("hp = %.0f mm; alma reta = d - 2 tf - 2 r = %.0f mm", hp, clear));
        out.push_back(c);
    }

    // parafusos no apoio: duas colunas simetricas em relacao a alma (cortante concentrico), tracao axial dividida igualmente
    {
        BoltedInput b;
        b.grade = in.grade;
        b.db = in.db;
        b.threadInShearPlane = in.threadInShearPlane || !in.grade.highStrength;
        b.shearPlanes = 1;
        b.nx = 2;
        b.ny = rows;
        b.sx = in.gauge;
        b.sy = in.pitch;
        b.endDistance = in.edgeV;
        b.edgeDistance = in.edgeH;
        b.shearY = V;
        b.tension = std::max(N, 0.0);
        b.gammaA2 = in.gammaA2;
        b.grip = in.plateThickness + in.supportThickness;
        BoltedInput onPlate = b;
        onPlate.plateThickness = in.plateThickness;
        onPlate.plateFu = in.plate.fu;
        BoltedInput onSupport = b;
        onSupport.plateThickness = in.supportThickness;
        onSupport.plateFu = in.support_.fu;
        const auto plateChecks = checkBolted(onPlate);
        const auto supportChecks = checkBolted(onSupport);
        detail::appendChecks(out, plateChecks, "Cisalhamento", " - parafusos ao apoio");
        detail::appendChecks(out, plateChecks, "Tra", " - parafusos ao apoio");
        detail::appendChecks(out, plateChecks, "Intera", " - parafusos ao apoio");
        detail::appendChecks(out, plateChecks, "Pressao de contato", " - chapa");
        detail::appendChecks(out, supportChecks, "Pressao de contato", " - mesa/alma do apoio");
        detail::appendChecks(out, plateChecks, "Espacamento", "");
        detail::appendChecks(out, plateChecks, "Distancia", "");
    }

    // chapa: cada metade (do lado de uma coluna de parafusos) recebe V/2 da alma
    {
        const double halfShear = V / 2.0;
        const double vyield = 0.6 * in.plate.fy * hp * in.plateThickness / in.gammaA1;
        Check c = ratioN("Chapa: escoamento por cisalhamento (secao da alma, altura total)", "NBR 8800:2024, 6.5.5", V, vyield);
        c.lines.push_back(fmtn("Rd = 0,6 fy Agv/ga1 = 0,6 x %.0f x %.0f x %.1f /%.2f = %.1f kN", in.plate.fy, hp, in.plateThickness, in.gammaA1, vyield / 1000.0));
        out.push_back(c);
        const double anv = (hp - rows * dh) * in.plateThickness;
        const double vrupt = 0.6 * in.plate.fu * anv / in.gammaA2;
        Check r = ratioN("Chapa: ruptura por cisalhamento na linha de furos (cada metade, V/2)", "NBR 8800:2024, 6.5.5", halfShear, vrupt);
        r.lines.push_back(fmtn("Anv = (hp - n dh) t = (%.0f - %d x %.1f) x %.1f = %.0f mm2; Rd = 0,6 fu Anv/ga2 = %.1f kN", hp, rows, dh, in.plateThickness, anv, vrupt / 1000.0));
        out.push_back(r);
        const double lv = (rows - 1) * in.pitch + in.edgeV;
        const detail::BlockShearResult bs = detail::blockShearRd(in.plateThickness, in.plate.fy, in.plate.fu, lv, rows - 0.5, in.edgeH, 0.5, dh, in.gammaA2);
        Check b = ratioN("Chapa: rasgamento (bloco de cisalhamento) em cada coluna de parafusos", "NBR 8800:2024, 6.5.6", halfShear, bs.rd);
        b.lines.push_back(fmtn("Agv = %.0f; Anv = %.0f; Ant = %.0f mm2; Rd = min(0,6 fu Anv + fu Ant ; 0,6 fy Agv + fu Ant)/ga2 = %.1f kN", bs.agv, bs.anv, bs.ant, bs.rd / 1000.0));
        out.push_back(b);
        if (N > 0.0) {
            // tracao axial: flexao da chapa entre a alma e cada coluna de parafusos (balanco de comprimento a = (g - tw)/2)
            const double lever = std::max((in.gauge - in.webThickness) / 2.0, 1.0);
            const double mPerBolt = (N / (2.0 * rows)) * lever;
            const double width = std::max(in.pitch, 2.0 * dh + 1.0) - dh;   // largura efetiva da chapa por parafuso, descontado o furo
            const double mrd = width * in.plateThickness * in.plateThickness / 4.0 * in.plate.fy / in.gammaA1;
            Check f = ratioN("Chapa: flexao pela tracao axial (balanco entre a alma e a coluna de parafusos)", "NBR 8800:2024, 5.4.2", mPerBolt / 1000.0, mrd / 1000.0);
            f.unit = "kN*m";
            f.lines.push_back(fmtn("M = (N/n) a = %.1f kN x %.0f mm por parafuso; largura efetiva = p - dh = %.0f mm; Mrd = Z fy/ga1 com Z = b t2/4", N / (2.0 * rows) / 1000.0, lever, width));
            out.push_back(f);
        }
    }

    // solda chapa - alma (filetes dos dois lados da alma, comprimento hp cada)
    {
        WeldInput w;
        w.leg = in.weldLeg;
        w.length = 2.0 * hp;
        w.fw = in.weldFw;
        w.force = std::hypot(V, std::max(N, 0.0));
        w.angleDeg = std::atan2(std::max(N, 0.0), std::max(V, 1e-9)) * 180.0 / kPi;   // carga axial e transversal ao eixo dos filetes
        w.useAngleFactor = true;
        w.plateThickness = std::min(in.plateThickness, in.webThickness);
        w.plateFy = std::min(in.plate.fy, in.web.fy);
        w.plateFu = std::min(in.plate.fu, in.web.fu);
        w.gammaW2 = in.gammaW2;
        w.gammaA1 = in.gammaA1;
        w.gammaA2 = in.gammaA2;
        detail::appendChecks(out, checkFilletWeld(w), "", " - chapa / alma");
        // alma da viga na regiao soldada
        const double vweb = 0.6 * in.web.fy * hp * in.webThickness / in.gammaA1;
        Check c = ratioN("Alma da viga: escoamento por cisalhamento no comprimento soldado", "NBR 8800:2024, 5.4.3", V, vweb);
        c.lines.push_back(fmtn("Rd = 0,6 fy hp tw/ga1 = %.1f kN", vweb / 1000.0));
        out.push_back(c);
        if (N > 0.0) {
            Check t = ratioN("Alma da viga: tracao axial nos filetes (escoamento da alma, 2 filetes de comprimento hp)", "NBR 8800:2024, 5.2.2", N, std::max(in.web.fy * in.webThickness * 2.0 * hp / in.gammaA1, 1.0));
            out.push_back(t);
        }
    }
    return out;
}

std::vector<Part> partsFlexEndPlate(const FlexEndPlateInput &in)
{
    std::vector<Part> parts;
    const double hp = flexEndPlateHeight(in), wp = flexEndPlateWidth(in);
    Part plate;
    plate.description = fmtn("Chapa de extremidade %.1f mm, %.0f x %.0f mm", in.plateThickness, wp, hp);
    plate.quantity = 1;
    plate.massEach = detail::plateMass(hp, wp, in.plateThickness);
    parts.push_back(plate);
    Part bolts;
    bolts.description = fmtn("Parafuso d = %.0f mm", in.db);
    bolts.quantity = 2 * std::max(in.rows, 1);
    bolts.massEach = 0.25 * kPi * in.db * in.db * (in.plateThickness + in.supportThickness + 1.5 * in.db) * 1e-9 * detail::kSteelDensity * 1.35 + 0.04;
    parts.push_back(bolts);
    Part weld;
    weld.description = fmtn("Filete de solda %.0f mm (chapa - alma, 2 x %.0f mm)", in.weldLeg, hp);
    weld.quantity = 1;
    weld.unit = "m";
    weld.length = 2.0 * hp;
    weld.massEach = 0.5 * in.weldLeg * in.weldLeg * weld.length * 1e-9 * detail::kSteelDensity;
    parts.push_back(weld);
    return parts;
}

} // namespace Structura::Design::Connections
