#include "design/EndPlateConnection.h"

#include "design/ConnectionHelpers.h"
#include "design/SupportChecks.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design::Connections {

using detail::fmtn;
using detail::kModulusE;
using detail::kPi;
using detail::ratioN;

namespace {

double sq(double x) { return x * x; }

double extensionTop(const EndPlateInput &in)
{
    return in.firstRow + (std::max(1, in.tensionRows) - 1) * in.pitch + in.edgeVertical;
}

double extensionBottom(const EndPlateInput &in)
{
    return in.firstRow + (std::max(1, in.shearRows) - 1) * in.pitch + in.edgeVertical;
}

} // namespace

double endPlateHeight(const EndPlateInput &in)
{
    return in.beamDepth + extensionTop(in) + extensionBottom(in);
}

std::vector<Check> checkEndPlate(const EndPlateInput &in)
{
    std::vector<Check> out;
    const double z = std::max(in.beamDepth - in.flangeThickness, 1.0);
    const double mAbs = std::abs(in.moment);
    const double ft = std::max(0.0, mAbs / z + in.axial / 2.0);    // forca na mesa tracionada
    const double fc = std::max(0.0, mAbs / z - in.axial / 2.0);    // forca na mesa comprimida
    const double ab = 0.25 * kPi * sq(in.db);
    const double dh = standardHoleDiameter(in.db);
    const int tensionBolts = 2 * std::max(1, in.tensionRows);
    const int shearBolts = 2 * std::max(1, in.shearRows);

    // parafusos tracionados (distribuicao igual entre as fileiras)
    const double ft0 = ft / tensionBolts;
    const double ftRd = 0.75 * ab * in.grade.fub / in.gammaA2;
    {
        Check c = ratioN("Parafusos tracionados", "NBR 8800:2024, 6.3.3.1", ft0, ftRd);
        c.lines.push_back(fmtn("F_t = M/(d - tf) %s N/2 = %.1f kN na mesa tracionada; %d parafusos: F_t,0,Sd = %.1f kN (sem alavanca)", in.axial >= 0.0 ? "+" : "-", ft / 1e3, tensionBolts, ft0 / 1e3));
        c.lines.push_back(fmtn("F_t,Rd = 0,75 Ab fub / ga2 = %.1f kN; z = d - tf = %.0f mm", ftRd / 1e3, z));
        out.push_back(std::move(c));
    }
    // efeito de alavanca (6.3.5): b ate a face da mesa da viga (descontada a solda), a ate a borda, p = largura tributaria
    {
        PryingInput pr;
        pr.db = in.db;
        pr.tensionForce = ft0;
        pr.boltResistance = ftRd;
        const double worstRow = in.firstRow + (std::max(1, in.tensionRows) - 1) * in.pitch;
        pr.b = worstRow - 0.8 * in.weldFlangeLeg;
        pr.a = in.edgeVertical;
        pr.p = std::min(in.plateWidth / 2.0, in.gauge);
        pr.holeWidth = dh;
        pr.thickness = in.plateThickness;
        pr.fu = in.plate.fu;
        pr.gammaA1 = in.gammaA1;
        for (Check c : checkPrying(pr)) {
            c.name += " - chapa de extremidade";
            out.push_back(std::move(c));
        }
    }
    // parafusos de cortante (zona comprimida): cortante + pressao de contato
    {
        BoltedInput b;
        b.grade = in.grade;
        b.db = in.db;
        b.threadInShearPlane = in.threadInShearPlane || !in.grade.highStrength;
        b.shearPlanes = 1;
        b.nx = 2;
        b.ny = std::max(1, in.shearRows);
        b.sx = in.gauge;
        b.sy = in.pitch;
        b.plateThickness = std::min(in.plateThickness, in.colFlangeThickness);
        b.plateFu = in.plateThickness <= in.colFlangeThickness ? in.plate.fu : in.column.fu;
        b.endDistance = in.edgeVertical;
        b.edgeDistance = (in.plateWidth - in.gauge) / 2.0;
        b.shearY = std::abs(in.shear);
        b.grip = in.plateThickness + in.colFlangeThickness;
        const auto bolted = checkBolted(b);
        detail::appendChecks(out, bolted, "Cisalhamento", " - parafusos da zona comprimida");
        detail::appendChecks(out, bolted, "Pressao de contato", " - chapa / mesa do pilar");
        detail::appendChecks(out, bolted, "Espacamento", "");
        detail::appendChecks(out, bolted, "Distancia", "");
    }
    // soldas da viga a chapa
    {
        WeldInput w;
        w.leg = in.weldFlangeLeg;
        w.length = std::max(1.0, 2.0 * in.flangeWidth - in.webThickness);
        w.fw = in.weldFw;
        w.force = std::max(ft, fc);
        w.plateThickness = std::min(in.flangeThickness, in.plateThickness);
        w.plateFy = in.beam.fy;
        w.plateFu = in.beam.fu;
        w.useAngleFactor = false;
        w.gammaW2 = in.gammaW2;
        w.gammaA1 = in.gammaA1;
        w.gammaA2 = in.gammaA2;
        auto weldChecks = checkFilletWeld(w);
        detail::appendChecks(out, weldChecks, "Metal da solda", " - mesa da viga");
        detail::appendChecks(out, weldChecks, "Perna minima", " - mesa");
        WeldInput web = w;
        web.leg = in.weldWebLeg;
        web.length = std::max(1.0, 2.0 * (in.beamDepth - 2.0 * in.flangeThickness - 2.0 * in.weldFlangeLeg));
        web.force = std::abs(in.shear);
        web.plateThickness = std::min(in.webThickness, in.plateThickness);
        web.plateFy = in.beam.fy;
        web.plateFu = in.beam.fu;
        web.angleDeg = 0.0;
        auto webChecks = checkFilletWeld(web);
        detail::appendChecks(out, webChecks, "Metal da solda", " - alma da viga");
        detail::appendChecks(out, webChecks, "Perna minima", " - alma");
    }
    // detalhamento
    {
        Check c = detail::minimumRule("Largura da chapa x mesa da viga", "detalhamento", in.plateWidth, in.flangeWidth);
        c.lines.push_back(fmtn("b_chapa = %.0f mm >= largura da mesa = %.0f mm", in.plateWidth, in.flangeWidth));
        out.push_back(std::move(c));
        Check g = detail::minimumRule("Gabarito dos parafusos", "NBR 8800:2024, 6.3.9", in.gauge, 3.0 * in.db);
        g.lines.push_back(fmtn("g = %.0f mm >= 3 db = %.0f mm", in.gauge, 3.0 * in.db));
        out.push_back(std::move(g));
        Check e = detail::minimumRule("Distancia do parafuso a borda lateral", "NBR 8800:2024, 6.3.11", (in.plateWidth - in.gauge) / 2.0, minimumEdgeDistance(in.db));
        e.lines.push_back(fmtn("(b - g)/2 = %.1f mm >= %.1f mm (Tabela 16)", (in.plateWidth - in.gauge) / 2.0, minimumEdgeDistance(in.db)));
        out.push_back(std::move(e));
        Check f = detail::minimumRule("Distancia do parafuso a mesa da viga", "detalhamento", in.firstRow, 0.8 * in.weldFlangeLeg + 0.5 * in.db + 10.0);
        f.lines.push_back(fmtn("firstRow = %.0f mm >= solda + 0,5 db + folga = %.0f mm", in.firstRow, 0.8 * in.weldFlangeLeg + 0.5 * in.db + 10.0));
        out.push_back(std::move(f));
    }

    // pilar: mesa e alma sob a forca da mesa da viga (5.7.2 a 5.7.5), ou enrijecedores (5.7.9)
    SupportInput sup;
    sup.face = SupportInput::Flange;
    sup.d = in.colDepth;
    sup.bf = in.colFlangeWidth;
    sup.tf = in.colFlangeThickness;
    sup.tw = in.colWebThickness;
    sup.k = in.colFlangeThickness + in.colRadius;
    sup.fy = in.column.fy;
    sup.fu = in.column.fu;
    sup.atMemberEnd = false;
    sup.tension = ft;
    sup.compression = fc;
    sup.lengthAlong = in.flangeThickness + 2.0 * in.weldFlangeLeg + 2.0 * in.plateThickness;
    sup.lengthTransverse = in.flangeWidth;
    sup.unbracedLength = in.colUnbraced;
    sup.gammaA1 = in.gammaA1;
    const std::vector<Check> local = checkSupportMember(sup);
    if (!in.continuityPlates) {
        detail::appendChecks(out, local, "", "");
        // 5.7.7: o painel de alma sem reforco
    } else {
        // 5.7.9: os enrijecedores absorvem a diferenca entre a forca solicitante e a resistente de cada estado-limite local
        double diff = 0.0;
        for (const Check &c : local) {
            if (c.demand > c.resistance) {
                diff = std::max(diff, (c.demand - c.resistance) * 1000.0);
            }
        }
        const double stiffArea = 2.0 * in.stiffenerWidth * in.stiffenerThickness;
        const double stiffRd = stiffArea * in.column.fy / in.gammaA1;
        Check s = ratioN("Enrijecedores horizontais do pilar", "NBR 8800:2024, 5.7.9", diff, stiffRd);
        s.lines.push_back(fmtn("Diferenca a absorver = max(F_Sd - F_Rd) nos estados-limite locais (5.7.2 a 5.7.5) = %.1f kN", diff / 1e3));
        s.lines.push_back(fmtn("2 enrijecedores %.0f x %.1f mm: A fy / ga1 = %.1f kN (flambagem do enrijecedor, 5.7.9.4, e solda a conferir no detalhamento)", in.stiffenerWidth, in.stiffenerThickness, stiffRd / 1e3));
        out.push_back(std::move(s));
        Check w = detail::minimumRule("Largura dos enrijecedores", "NBR 8800:2024, 5.7.9.5-a", 2.0 * in.stiffenerWidth + in.colWebThickness, in.flangeWidth / 3.0);
        w.lines.push_back(fmtn("2 bs + tw = %.1f mm >= bf/3 = %.1f mm", 2.0 * in.stiffenerWidth + in.colWebThickness, in.flangeWidth / 3.0));
        out.push_back(std::move(w));
        const double tMin = std::max(in.flangeThickness / 2.0, in.stiffenerWidth / 16.0);
        Check t = detail::minimumRule("Espessura dos enrijecedores", "NBR 8800:2024, 5.7.9.5-b", in.stiffenerThickness, tMin);
        t.lines.push_back(fmtn("ts = %.1f mm >= max(tf_viga/2 ; bs/16) = %.1f mm", in.stiffenerThickness, tMin));
        out.push_back(std::move(t));
        // as demais verificacoes locais sem estado-limite violado continuam informativas
        for (const Check &c : local) {
            if (c.demand <= c.resistance) {
                Check copy = c;
                copy.name += " (sem enrijecedor)";
                out.push_back(std::move(copy));
            }
        }
    }
    // painel de alma do pilar: cisalhamento (5.7.7.1)
    {
        ColumnPanelInput pi;
        pi.d = in.colDepth;
        pi.bf = in.colFlangeWidth;
        pi.tf = in.colFlangeThickness;
        pi.tw = in.colWebThickness;
        pi.radius = in.colRadius;
        pi.fy = in.column.fy;
        pi.doubler = in.doublerThickness;
        pi.beamForce = std::max(ft, fc);
        pi.panelShear = in.panelShear;
        pi.axial = in.colAxial;
        pi.gammaA1 = in.gammaA1;
        for (Check c : checkColumnPanel(pi)) {
            out.push_back(std::move(c));
        }
    }
    return out;
}

std::vector<Part> partsEndPlate(const EndPlateInput &in)
{
    std::vector<Part> parts;
    Part plate;
    const double height = endPlateHeight(in);
    plate.description = fmtn("Chapa de extremidade %.1f mm, %.0f x %.0f mm", in.plateThickness, in.plateWidth, height);
    plate.quantity = 1;
    plate.massEach = detail::plateMass(height, in.plateWidth, in.plateThickness);
    parts.push_back(plate);
    Part bolts;
    bolts.description = fmtn("Parafuso d = %.0f mm", in.db);
    bolts.quantity = 2 * (std::max(1, in.tensionRows) + std::max(1, in.shearRows));
    bolts.massEach = kPi * sq(in.db) / 4.0 * (in.plateThickness + in.colFlangeThickness + 1.5 * in.db) * 1e-9 * detail::kSteelDensity * 1.35 + 0.04;
    parts.push_back(bolts);
    Part weld;
    weld.description = fmtn("Filete de solda %.0f mm (mesas) e %.0f mm (alma)", in.weldFlangeLeg, in.weldWebLeg);
    weld.quantity = 1;
    weld.unit = "m";
    weld.length = 2.0 * (2.0 * in.flangeWidth - in.webThickness) + 2.0 * (in.beamDepth - 2.0 * in.flangeThickness);
    parts.push_back(weld);
    if (in.continuityPlates) {
        Part st;
        st.description = fmtn("Enrijecedor horizontal %.1f mm, %.0f x %.0f mm", in.stiffenerThickness, in.stiffenerWidth, in.colDepth - 2.0 * in.colFlangeThickness);
        st.quantity = 4;
        st.massEach = detail::plateMass(in.stiffenerWidth, in.colDepth - 2.0 * in.colFlangeThickness, in.stiffenerThickness);
        parts.push_back(st);
    }
    if (in.doublerThickness > 0.0) {
        Part d;
        d.description = fmtn("Chapa de reforco da alma %.1f mm", in.doublerThickness);
        d.quantity = 1;
        d.massEach = detail::plateMass(in.colDepth - 2.0 * in.colFlangeThickness, in.colFlangeWidth, in.doublerThickness);
        parts.push_back(d);
    }
    return parts;
}

} // namespace Structura::Design::Connections
