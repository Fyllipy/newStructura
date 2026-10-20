#include "design/WebShearConnection.h"

#include "design/ConnectionHelpers.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design::Connections {

using detail::blockShearRd;
using detail::fmtn;
using detail::kSteelDensity;
using detail::ratioN;
using detail::ratioNmm;

namespace {

double minLegFor(double thin)
{
    if (thin > 19.0) {
        return 8.0;
    }
    if (thin > 12.5) {
        return 6.0;
    }
    if (thin > 6.3) {
        return 5.0;
    }
    return 3.0;
}

void legChecks(std::vector<Check> &checks, const std::string &label, double leg, double thinnest, double plateThickness)
{
    const double minLeg = minLegFor(thinnest);
    checks.push_back(detail::minimumRule("Perna minima do filete - " + label, "6.2.6.2.1 / Tab. 11", leg, minLeg));
    const double maxLeg = plateThickness < 6.3 ? plateThickness : plateThickness - 1.5;
    Check m;
    m.name = "Perna maxima do filete - " + label;
    m.clause = "6.2.6.2.2";
    m.unit = "mm";
    m.minimumRule = true;
    m.demand = maxLeg;
    m.resistance = leg;
    m.lines.push_back(fmtn("a = %.1f mm <= %.1f mm (borda da chapa)", leg, maxLeg));
    checks.push_back(m);
}

double clearWeb(const WebShearInput &in)
{
    if (in.beamDepth <= 0.0 || in.beamTf <= 0.0) {
        return 0.0;
    }
    return std::max(0.0, in.beamDepth - 2.0 * in.beamTf - 2.0 * in.beamFillet);
}

} // namespace

double webShearPlateHeight(const WebShearInput &in)
{
    if (!in.webWelded) {
        return (std::max(1, in.rows) - 1) * in.pitch + 2.0 * in.edgeV;
    }
    if (in.plateHeight > 0.0) {
        return in.plateHeight;
    }
    const double clear = clearWeb(in);
    return clear > 0.0 ? std::floor(0.8 * clear / 5.0) * 5.0 : 150.0;
}

double webShearPlateLength(const WebShearInput &in)
{
    if (!in.webWelded) {
        return in.boltLineToSupport + (std::max(1, in.lines) - 1) * in.gauge + in.edgeH;
    }
    return in.gap + std::max(in.plateLength, 20.0);
}

double webShearEccentricity(const WebShearInput &in)
{
    if (!in.webWelded) {
        return in.boltLineToSupport + (std::max(1, in.lines) - 1) * in.gauge / 2.0;
    }
    return in.gap + std::max(in.plateLength, 20.0) / 2.0;
}

CopeSize webShearCope(const WebShearInput &in)
{
    CopeSize cope;
    if (in.support != WebShearInput::BeamWeb || in.supBf <= 0.0) {
        return cope;
    }
    cope.length = std::max(0.0, (in.supBf - in.supTw) / 2.0 + in.copeClearance);
    cope.depth = in.beamTf + in.copeClearance;
    // as duas mesas da viga ligada encontram as da viga principal quando as alturas sao proximas
    cope.both = in.supD <= 0.0 || in.beamDepth >= in.supD - 2.0 * (in.supTf + in.copeClearance);
    return cope;
}

std::vector<Check> checkWebShear(const WebShearInput &in)
{
    std::vector<Check> checks;
    const int plates = std::clamp(in.plates, 1, 2);
    const int n = std::max(1, in.rows);
    const int lines = std::clamp(in.lines, 1, 2);
    const double V = std::abs(in.shear);
    const double N = in.axial;
    const double H = webShearPlateHeight(in);
    const double a = webShearEccentricity(in);
    const double M = V * a;
    const double dh = standardHoleDiameter(in.db);
    const double hole = dh + 2.0;
    const double Vp = V / plates;   // cortante por tala
    const double Mp = M / plates;
    const double tPlate = in.plateThickness;

    // ---- ligacao com a alma da viga ------------------------------------------------------------------------------------------
    if (!in.webWelded) {
        BoltedInput b;
        b.grade = in.grade;
        b.db = in.db;
        b.threadInShearPlane = in.threadInShearPlane || !in.grade.highStrength;
        b.shearPlanes = plates;
        b.nx = lines;
        b.ny = n;
        b.sx = in.gauge;
        b.sy = in.pitch;
        b.plateThickness = plates * tPlate;   // contato nas talas (as duas contribuem)
        b.plateFu = in.plate.fu;
        b.endDistance = std::min(in.edgeV, in.edgeH);
        b.edgeDistance = in.edgeH;
        b.shearY = V;
        b.shearX = std::abs(N);
        b.momentZ = M;
        b.grip = plates * tPlate + in.webThickness;
        const auto boltChecks = checkBolted(b);
        detail::appendChecks(checks, boltChecks, "Cisalhamento", plates == 2 ? " - corte duplo" : "");
        detail::appendChecks(checks, boltChecks, "Pressao de contato", plates == 2 ? " - talas" : " - tala");
        detail::appendChecks(checks, boltChecks, "Espacamento", "");
        detail::appendChecks(checks, boltChecks, "Distancia", "");
        BoltedInput w = b;
        w.plateThickness = in.webThickness;
        w.plateFu = in.web.fu;
        const double lh = std::max(1.0, in.boltLineToSupport - in.gap);   // primeira linha - extremidade da viga
        w.endDistance = std::min(lh, std::max(1.0, clearWeb(in) > 0.0 ? (clearWeb(in) - (n - 1) * in.pitch) / 2.0 : in.edgeV));
        detail::appendChecks(checks, checkBolted(w), "Pressao de contato", " - alma da viga");
        // rasgamento da alma da viga e da(s) tala(s)
        const double edgeWeb = clearWeb(in) > 0.0 ? std::max(1.0, (clearWeb(in) - (n - 1) * in.pitch) / 2.0) : in.edgeV;
        const auto web = blockShearRd(in.webThickness, in.web.fy, in.web.fu, (n - 1) * in.pitch + edgeWeb, n - 0.5, lh, 0.5, hole, in.gammaA2);
        Check cw = ratioN("Alma da viga - rasgamento", "6.5.6", V, web.rd);
        cw.lines.push_back(fmtn("lh = %.1f mm; Agv = %.0f; Anv = %.0f; Ant = %.0f mm2; Rr,Rd = %.2f kN", lh, web.agv, web.anv, web.ant, web.rd / 1000.0));
        checks.push_back(cw);
        const auto pl = blockShearRd(tPlate, in.plate.fy, in.plate.fu, (n - 1) * in.pitch + in.edgeV, n - 0.5, in.edgeH, 0.5, hole, in.gammaA2);
        Check cp = ratioN(plates == 2 ? "Tala (cada) - rasgamento" : "Tala - rasgamento (cisalhamento de bloco)", "6.5.6", Vp, pl.rd);
        cp.lines.push_back(fmtn("Agv = %.0f; Anv = %.0f; Ant = %.0f mm2; Rr,Rd = %.2f kN", pl.agv, pl.anv, pl.ant, pl.rd / 1000.0));
        checks.push_back(cp);
    } else {
        const double lw = std::max(in.plateLength, 20.0);
        IcrInput icr;
        icr.leg = in.weldLegWeb;
        icr.fw = in.weldFw;
        icr.gammaW2 = in.gammaW2;
        icr.segments.push_back({in.gap, H / 2.0, in.gap + lw, H / 2.0});
        icr.segments.push_back({in.gap, -H / 2.0, in.gap + lw, -H / 2.0});
        if (in.weldEnd) {
            icr.segments.push_back({in.gap + lw, -H / 2.0, in.gap + lw, H / 2.0});
        }
        icr.loadX = 0.0;
        icr.loadY = 0.0;
        const double resultant = std::hypot(Vp, std::abs(N) / plates);
        icr.loadAngleDeg = std::atan2(Vp, std::abs(N) / plates) * 180.0 / detail::kPi;
        const IcrResult res = weldGroupIcr(icr);
        Check c = ratioN(plates == 2 ? "Solda tala - alma da viga (cada tala, grupo excentrico)" : "Solda tala - alma da viga (grupo excentrico)", "6.2.5.2-b (CIR)", resultant, res.converged ? res.forceRd : 0.0);
        c.lines.push_back(fmtn("filetes %.1f mm: 2 x %.0f mm%s; excentricidade da face do apoio ao centroide = %.1f mm", in.weldLegWeb, lw, in.weldEnd ? " + vertical de extremidade" : "", a));
        c.lines.push_back(fmtn("Fr,Rd (metodo do centro instantaneo de rotacao) = %.2f kN", res.forceRd / 1000.0));
        checks.push_back(c);
        // metal-base da alma e da tala junto a solda
        const double te = 0.707 * in.weldLegWeb;
        (void)te;
        const double webShear = std::min(0.6 * in.webThickness * lw * in.web.fy / in.gammaA1, 0.6 * in.webThickness * lw * in.web.fu / in.gammaA2);
        Check bm = ratioN("Alma da viga - cisalhamento do metal-base na solda", "6.2.5 / 6.5.5", Vp * 0.5, webShear);
        bm.lines.push_back(fmtn("metade do cortante por tala sobre o comprimento soldado: 0,6 tw Lw fy / ga1 = %.2f kN", webShear / 1000.0));
        checks.push_back(bm);
        legChecks(checks, "tala - alma da viga", in.weldLegWeb, std::min(tPlate, in.webThickness), tPlate);
    }

    // ---- tala(s): escoamento, ruptura, flexao e axial -------------------------------------------------------------------------
    {
        const double ag = tPlate * H;
        const double holes = in.webWelded ? 0.0 : n * hole;
        const double an = tPlate * (H - holes);
        Check y = ratioN(plates == 2 ? "Tala (cada) - escoamento ao cisalhamento" : "Tala - escoamento ao cisalhamento", "6.5.5-a", Vp, 0.6 * ag * in.plate.fy / in.gammaA1);
        y.lines.push_back(fmtn("Ag = t H = %.1f x %.1f = %.0f mm2; VRd = 0,6 Ag fy / ga1 = %.2f kN", tPlate, H, ag, 0.6 * ag * in.plate.fy / in.gammaA1 / 1000.0));
        checks.push_back(y);
        Check r = ratioN(plates == 2 ? "Tala (cada) - ruptura ao cisalhamento" : "Tala - ruptura ao cisalhamento", "6.5.5-b", Vp, 0.6 * an * in.plate.fu / in.gammaA2);
        r.lines.push_back(fmtn("Anv = t (H - n (dh + 2)) = %.0f mm2; VRd = 0,6 Anv fu / ga2 = %.2f kN", an, 0.6 * an * in.plate.fu / in.gammaA2 / 1000.0));
        checks.push_back(r);
        const double mrd = tPlate * H * H * in.plate.fy / (4.0 * in.gammaA1);
        Check m = ratioNmm(plates == 2 ? "Tala (cada) - flexao (excentricidade)" : "Tala - flexao (excentricidade)", "5.4 / 6.5", Mp, mrd);
        m.lines.push_back(fmtn("M = V a / n_talas = %.2f kN x %.1f mm / %.0f = %.2f kN.m; MRd = t H^2 fy / (4 ga1) = %.2f kN.m", V / 1000.0, a, double(plates), Mp / 1e6, mrd / 1e6));
        checks.push_back(m);
        if (std::abs(N) > 0.0) {
            const double yield = ag * in.plate.fy / in.gammaA1;
            const double rupture = std::min(an, 0.85 * ag) * in.plate.fu / in.gammaA2;
            Check x = ratioN(N > 0 ? "Tala - tracao axial" : "Tala - compressao axial (curta)", "6.5.3 / 6.5.4", std::abs(N) / plates, N > 0 ? std::min(yield, rupture) : yield);
            checks.push_back(x);
        }
    }

    // ---- solda da(s) tala(s) ao apoio -----------------------------------------------------------------------------------------
    {
        const double te = 0.707 * in.weldLegSupport;
        const double fv = Vp / (2.0 * H);
        const double fn = (std::abs(N) / plates) / (2.0 * H) + Mp / (H * H / 3.0);
        const double fr = std::hypot(fv, fn);
        const double perLength = 0.6 * te * in.weldFw / in.gammaW2;
        Check s = ratioN("Solda tala - apoio (metodo elastico)", "6.2.5", fr, perLength);
        s.unit = "N/mm";
        s.demand = fr;
        s.resistance = perLength;
        s.lines.push_back(fmtn("%s: 2 filetes de %.1f mm (te = %.2f mm) x %.0f mm; fv = %.2f N/mm; fn = N/(2H) + M/(H^2/3) = %.2f N/mm", plates == 2 ? "cada tala" : "tala", in.weldLegSupport, te, H, fv, fn));
        s.lines.push_back(fmtn("fr = %.2f N/mm <= 0,6 te fw / gw2 = %.2f N/mm", fr, perLength));
        checks.push_back(s);
        const double basePlate = std::min(0.6 * tPlate * in.plate.fy / in.gammaA1, 0.6 * tPlate * in.plate.fu / in.gammaA2);
        const double baseSupport = std::min(0.6 * in.supportThickness * in.support_.fy / in.gammaA1, 0.6 * in.supportThickness * in.support_.fu / in.gammaA2);
        Check b = ratioN("Solda tala - apoio - metal-base", "6.2.5 / 6.5", 2.0 * fr, std::min(basePlate, baseSupport));
        b.unit = "N/mm";
        b.demand = 2.0 * fr;
        b.resistance = std::min(basePlate, baseSupport);
        b.lines.push_back(fmtn("2 fr = %.2f N/mm <= min(tala %.2f ; apoio %.2f) N/mm", 2.0 * fr, basePlate, baseSupport));
        checks.push_back(b);
        legChecks(checks, "tala - apoio", in.weldLegSupport, std::min(tPlate, in.supportThickness), tPlate);
    }

    // ---- alma da viga, geometria e recortes ------------------------------------------------------------------------------------
    const double clear = clearWeb(in);
    if (clear > 0.0) {
        Check h = detail::rangeMax("Altura da tala x alma da viga entre mesas", "detalhamento", H, clear, "mm");
        h.lines.push_back(fmtn("H = %.0f mm <= T = d - 2 tf - 2 r = %.0f mm", H, clear));
        checks.push_back(h);
        Check low = detail::rangeMin("Altura minima da tala (>= 0,5 d)", "detalhamento", H, 0.5 * in.beamDepth, "mm");
        checks.push_back(low);
    }
    const CopeSize cope = webShearCope(in);
    double webHeight = in.beamDepth;
    if (cope.length > 0.0) {
        webHeight = in.beamDepth - (cope.both ? 2.0 : 1.0) * cope.depth;
        Check c = detail::rangeMax("Recorte nas mesas da viga - profundidade <= 0,2 d", "detalhamento", cope.depth, 0.2 * in.beamDepth, "mm");
        c.lines.push_back(fmtn("recorte %.0f x %.0f mm em %s mesa(s) da viga ligada", cope.length, cope.depth, cope.both ? "ambas as" : "uma"));
        checks.push_back(c);
        if (webHeight > 0.0) {
            checks.push_back(detail::rangeMin("Altura da alma remanescente >= altura da tala", "detalhamento", webHeight, H, "mm"));
        }
    }
    if (webHeight > 0.0) {
        const double aw = in.webThickness * webHeight;
        Check vw = ratioN("Alma da viga - cisalhamento na secao da ligacao", "5.4.3", V, 0.6 * aw * in.web.fy / in.gammaA1);
        vw.lines.push_back(fmtn("altura da alma na ligacao = %.0f mm; VRd = 0,6 tw h fy / ga1 = %.2f kN", webHeight, 0.6 * aw * in.web.fy / in.gammaA1 / 1000.0));
        checks.push_back(vw);
    }
    if (in.support == WebShearInput::ColumnWeb && in.supD > 0.0 && in.beamBf > 0.0) {
        const double clearWidth = in.supD - 2.0 * in.supTf - 2.0 * in.supFillet;
        Check g = detail::rangeMin("Alma do pilar: largura livre >= mesa da viga + folgas", "detalhamento", clearWidth, in.beamBf + 2.0 * in.copeClearance, "mm");
        g.lines.push_back(fmtn("d - 2 tf - 2 r = %.0f mm >= bf viga + 2 folga = %.0f mm", clearWidth, in.beamBf + 2.0 * in.copeClearance));
        checks.push_back(g);
    }
    return checks;
}

std::vector<Part> partsWebShear(const WebShearInput &in)
{
    std::vector<Part> parts;
    const double H = webShearPlateHeight(in);
    const double W = webShearPlateLength(in);
    const int plates = std::clamp(in.plates, 1, 2);
    Part plate;
    plate.description = fmtn("Tala %.1f x %.0f x %.0f mm", in.plateThickness, W, H);
    plate.quantity = plates;
    plate.massEach = in.plateThickness * H * W * 1e-9 * kSteelDensity;
    parts.push_back(plate);
    if (!in.webWelded) {
        Part bolts;
        bolts.description = fmtn("Parafuso d = %.0f mm (+ porca e arruelas)", in.db);
        bolts.quantity = std::max(1, in.rows) * std::clamp(in.lines, 1, 2);
        bolts.massEach = (0.25 * detail::kPi * in.db * in.db * (plates * in.plateThickness + in.webThickness + 10.0) * 1e-9 * kSteelDensity) * 1.35 + 0.04;
        parts.push_back(bolts);
    } else {
        Part weld;
        weld.description = fmtn("Filete %.0f mm tala-alma (2 x %.0f mm%s) por tala", in.weldLegWeb, std::max(in.plateLength, 20.0), in.weldEnd ? " + extremidade" : "");
        weld.quantity = plates;
        weld.length = 2.0 * std::max(in.plateLength, 20.0) + (in.weldEnd ? H : 0.0);
        weld.unit = "mm";
        weld.massEach = 0.5 * in.weldLegWeb * in.weldLegWeb * weld.length * 1e-9 * kSteelDensity;
        parts.push_back(weld);
    }
    Part support;
    support.description = fmtn("Filete %.0f mm tala-apoio (2 x %.0f mm) por tala", in.weldLegSupport, H);
    support.quantity = plates;
    support.length = 2.0 * H;
    support.unit = "mm";
    support.massEach = 0.5 * in.weldLegSupport * in.weldLegSupport * support.length * 1e-9 * kSteelDensity;
    parts.push_back(support);
    return parts;
}

} // namespace Structura::Design::Connections
