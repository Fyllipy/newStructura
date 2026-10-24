#include "design/MomentConnection.h"

#include "design/ConnectionHelpers.h"
#include "design/SupportChecks.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design::Connections {

using detail::fmtn;
using detail::kSteelDensity;
using detail::ratioN;

std::vector<Check> checkMomentConnection(const MomentConnectionInput &in)
{
    std::vector<Check> checks;
    const double arm = std::max(in.beamDepth - in.beamTf, 1.0);
    const double M = std::abs(in.moment);
    const double V = std::abs(in.shear);
    const double N = in.axial;
    const double ft = std::max(M / arm + N / 2.0, 0.0);   // mesa tracionada
    const double fc = std::max(M / arm - N / 2.0, 0.0);   // mesa comprimida
    const double fMax = std::max(ft, fc);
    Check couple = detail::dimensionless("Binario das mesas: Ft = M/(d - tf) + N/2 ; Fc = M/(d - tf) - N/2", "5.7 / 6.2", 0.0);
    couple.unit = "kN";
    couple.demand = 0.0;
    couple.lines.push_back(fmtn("M = %.2f kN.m, braco d - tf = %.1f mm: Ft = %.2f kN ; Fc = %.2f kN", M / 1e6, arm, ft / 1000.0, fc / 1000.0));
    checks.push_back(couple);

    // ---- mesas da viga ------------------------------------------------------------------------------------------------------------------------
    if (in.flangeMode == MomentConnectionInput::Welded) {
        const double ag = in.beamBf * in.beamTf;
        if (!in.flangeFillet) {
            Check w = ratioN("Solda de penetracao total das mesas: resistencia da mesa da viga", "6.2.4", fMax, ag * in.beam.fy / in.gammaA1);
            w.lines.push_back(fmtn("a solda de penetracao total tem a resistencia do metal-base: Ag fy/ga1 = %.2f kN", ag * in.beam.fy / in.gammaA1 / 1000.0));
            checks.push_back(w);
        } else {
            WeldInput w;
            w.leg = in.flangeWeldLeg;
            w.length = 2.0 * in.beamBf - in.beamTw;   // filetes dos dois lados da mesa
            w.fw = in.weldFw;
            w.force = fMax;
            w.useAngleFactor = false;   // a resultante e normal ao eixo dos filetes dos dois lados: nao se aplica 6.2.5.2-a
            w.plateThickness = std::min(in.beamTf, in.colTf);
            w.plateFy = in.beam.fy;
            w.plateFu = in.beam.fu;
            w.gammaW2 = in.gammaW2;
            w.gammaA1 = in.gammaA1;
            w.gammaA2 = in.gammaA2;
            detail::appendChecks(checks, checkFilletWeld(w), "", " - mesas da viga");
        }
    } else {
        // perfis T parafusados: cada T leva a forca da mesa
        const double F = fMax;
        const double ab = 0.25 * detail::kPi * in.db * in.db;
        const double ftRd = 0.75 * ab * in.grade.fub / in.gammaA2;
        const int nColumnBolts = std::max(1, in.stubColumnBoltsPerRow * in.stubColumnRows * 2);   // dos dois lados da alma do T (aba)
        const double ft0 = F / nColumnBolts;
        Check t = ratioN("T: parafusos tracionados (ligados ao pilar)", "6.3.3.1", ft0, ftRd);
        t.lines.push_back(fmtn("F = %.2f kN / %d parafusos = %.2f kN; Ft,Rd = %.2f kN", F / 1000.0, nColumnBolts, ft0 / 1000.0, ftRd / 1000.0));
        checks.push_back(t);
        PryingInput p;
        p.db = in.db;
        p.tensionForce = ft0;
        p.boltResistance = ftRd;
        p.a = in.stubA;
        p.b = in.stubB;
        p.p = std::max(in.stubWidth / std::max(1, in.stubColumnBoltsPerRow), in.db * 3.0);
        p.holeWidth = standardHoleDiameter(in.db) + 2.0;
        p.thickness = in.stubFlangeThickness;
        p.fu = in.stub.fu;
        p.gammaA1 = in.gammaA1;
        for (Check c : checkPrying(p)) {
            c.name += " - aba do T";
            checks.push_back(std::move(c));
        }
        // alma do T e mesa da viga na ligacao parafusada entre eles (duas colunas de parafusos, F transmitida por contato nos furos)
        const double hole = standardHoleDiameter(in.db) + 2.0;
        const int rowsBeam = std::max(1, in.stubBeamBolts / 2);
        {
            const double ag = in.stubWidth * in.stubStemThickness;
            const double an = (in.stubWidth - 2.0 * hole) * in.stubStemThickness;
            Check stem = ratioN("T: alma tracionada (escoamento da seção bruta e ruptura da seção líquida)", "NBR 8800:2024, 5.2.2", F, std::min(ag * in.stub.fy / in.gammaA1, an * in.stub.fu / in.gammaA2));
            stem.lines.push_back(fmtn("Ag = %.0f mm2; An = (b - 2 df) t = %.0f mm2 (Ct = 1,0: a força passa por todos os elementos da seção); Rd = min(Ag fy/ga1 ; An fu/ga2)", ag, an));
            checks.push_back(stem);
            const double shearLen = 2.0 * (in.stubBeamEnd + (rowsBeam - 1) * in.stubBeamPitch);
            const detail::BlockShearResult bs = detail::blockShearRd(in.stubStemThickness, in.stub.fy, in.stub.fu, shearLen, 2.0 * (rowsBeam - 0.5), in.stubBeamGauge, 1.0, hole, in.gammaA2);
            Check b = ratioN("T: rasgamento da alma (bloco de cisalhamento)", "NBR 8800:2024, 6.5.6", F, bs.rd);
            b.lines.push_back(fmtn("Agv = %.0f; Anv = %.0f; Ant = %.0f mm2; Rd = min(0,6 fu Anv + fu Ant ; 0,6 fy Agv + fu Ant)/ga2", bs.agv, bs.anv, bs.ant));
            checks.push_back(b);
            const double flAn = (in.beamBf - 2.0 * hole) * in.beamTf;
            Check fl = ratioN("Mesa da viga: ruptura da seção líquida nos furos do T", "NBR 8800:2024, 5.2.2", F, std::min(in.beamBf * in.beamTf * in.beam.fy / in.gammaA1, flAn * in.beam.fu / in.gammaA2));
            fl.lines.push_back(fmtn("An = (bf - 2 df) tf = %.0f mm2", flAn));
            checks.push_back(fl);
            const double flShear = 2.0 * (in.stubBeamEnd + (rowsBeam - 1) * in.stubBeamPitch);
            const detail::BlockShearResult bf = detail::blockShearRd(in.beamTf, in.beam.fy, in.beam.fu, flShear, 2.0 * (rowsBeam - 0.5), in.stubBeamGauge, 1.0, hole, in.gammaA2);
            checks.push_back(ratioN("Mesa da viga: rasgamento nos parafusos do T", "NBR 8800:2024, 6.5.6", F, bf.rd));
        }
        // parafusos do T na mesa da viga (corte simples + pressao de contato)
        BoltedInput b;
        b.grade = in.grade;
        b.db = in.db;
        b.threadInShearPlane = in.threadInShearPlane || !in.grade.highStrength;
        b.shearPlanes = 1;
        b.nx = rowsBeam;
        b.ny = 2;
        b.sx = in.stubBeamPitch;
        b.sy = in.stubBeamGauge;
        b.plateThickness = std::min(in.stubStemThickness, in.beamTf);
        b.plateFu = std::min(in.stub.fu, in.beam.fu);
        b.endDistance = in.stubBeamEnd;
        b.edgeDistance = std::min((in.stubWidth - in.stubBeamGauge) / 2.0, (in.beamBf - in.stubBeamGauge) / 2.0);
        b.shearX = F;
        b.grip = in.stubStemThickness + in.beamTf;
        for (Check c : checkBolted(b)) {
            c.name += " - T na mesa da viga";
            checks.push_back(std::move(c));
        }
    }

    // ---- alma da viga -------------------------------------------------------------------------------------------------------------------------
    if (in.webMode == MomentConnectionInput::DirectWeld) {
        const double hw = std::max(in.beamDepth - 2.0 * in.beamTf, 1.0);
        WeldInput w;
        w.leg = in.webWeldLeg;
        w.length = 2.0 * hw;
        w.fw = in.weldFw;
        w.force = V;
        // cortante paralelo aos filetes verticais da alma (theta = 0)
        w.useAngleFactor = false;
        w.plateThickness = std::min(in.beamTw, in.colTf);
        w.plateFy = in.beam.fy;
        w.plateFu = in.beam.fu;
        w.gammaW2 = in.gammaW2;
        w.gammaA1 = in.gammaA1;
        w.gammaA2 = in.gammaA2;
        detail::appendChecks(checks, checkFilletWeld(w), "", " - alma ao pilar");
    } else {
        WebShearInput web = in.web;
        web.shear = V;
        web.axial = 0.0;
        for (Check c : checkWebShear(web)) {
            c.name += " (alma)";
            checks.push_back(std::move(c));
        }
    }

    // ---- pilar: efeitos locais e painel de alma -----------------------------------------------------------------------------------
    SupportInput s;
    s.face = SupportInput::Flange;
    s.d = in.colDepth;
    s.bf = in.colBf;
    s.tf = in.colTf;
    s.tw = in.colTw;
    s.k = in.colTf + in.colRadius;
    s.fy = in.column.fy;
    s.fu = in.column.fu;
    s.atMemberEnd = false;
    s.tension = ft;
    s.compression = fc;
    s.lengthAlong = std::max(in.beamBf * 0.8, 40.0);
    s.lengthTransverse = in.beamTf + 2.0 * in.flangeWeldLeg;
    s.unbracedLength = in.colUnbraced;
    s.gammaA1 = in.gammaA1;
    s.pairForce = in.beamOnBothSides ? fc : 0.0;
    for (Check c : checkSupportMember(s)) {
        checks.push_back(std::move(c));
    }
    ColumnPanelInput pi;
    pi.d = in.colDepth;
    pi.bf = in.colBf;
    pi.tf = in.colTf;
    pi.tw = in.colTw;
    pi.radius = in.colRadius;
    pi.fy = in.column.fy;
    pi.doubler = in.doubler;
    pi.stiffenerSpacing = in.panelStiffened ? arm : 0.0;
    pi.axial = in.colAxial;
    pi.beamForce = fMax;
    pi.columnShear = in.colShear;
    pi.panelShear = in.panelShear;
    pi.gammaA1 = in.gammaA1;
    for (Check c : checkColumnPanel(pi)) {
        checks.push_back(std::move(c));
    }
    return checks;
}

std::vector<Part> partsMomentConnection(const MomentConnectionInput &in)
{
    std::vector<Part> parts;
    if (in.flangeMode == MomentConnectionInput::Welded) {
        Part w;
        w.description = in.flangeFillet ? fmtn("Filete %.0f mm das mesas (2 x %.0f mm)", in.flangeWeldLeg, 2.0 * in.beamBf - in.beamTw) : fmtn("Solda de penetracao total das mesas (2 x %.0f mm)", in.beamBf);
        w.quantity = 2;
        w.length = in.flangeFillet ? 2.0 * in.beamBf - in.beamTw : in.beamBf;
        w.unit = "mm";
        w.massEach = 0.5 * std::max(in.flangeWeldLeg, in.beamTf) * std::max(in.flangeWeldLeg, in.beamTf) * w.length * 1e-9 * kSteelDensity;
        parts.push_back(w);
    } else {
        Part t;
        t.description = fmtn("Perfil T: aba %.1f x %.0f mm, alma %.1f mm", in.stubFlangeThickness, in.stubWidth, in.stubStemThickness);
        t.quantity = 2;
        t.massEach = 0.4 * (in.stubFlangeThickness * in.stubWidth * 200.0 + in.stubStemThickness * in.stubWidth * 150.0) * 1e-9 * kSteelDensity;
        parts.push_back(t);
        Part cb;
        cb.description = fmtn("Parafuso d = %.0f mm aba do T - pilar", in.db);
        cb.quantity = 2 * 2 * std::max(1, in.stubColumnBoltsPerRow * in.stubColumnRows);
        cb.massEach = 0.45;
        parts.push_back(cb);
        Part bb;
        bb.description = fmtn("Parafuso d = %.0f mm alma do T - mesa da viga", in.db);
        bb.quantity = 2 * std::max(2, in.stubBeamBolts);
        bb.massEach = 0.45;
        parts.push_back(bb);
    }
    if (in.webMode == MomentConnectionInput::DirectWeld) {
        Part w;
        w.description = fmtn("Filete %.0f mm da alma (2 x %.0f mm)", in.webWeldLeg, in.beamDepth - 2.0 * in.beamTf);
        w.quantity = 1;
        w.length = 2.0 * (in.beamDepth - 2.0 * in.beamTf);
        w.unit = "mm";
        w.massEach = 0.5 * in.webWeldLeg * in.webWeldLeg * w.length * 1e-9 * kSteelDensity;
        parts.push_back(w);
    } else {
        for (auto &p : partsWebShear(in.web)) {
            parts.push_back(std::move(p));
        }
    }
    return parts;
}

} // namespace Structura::Design::Connections
