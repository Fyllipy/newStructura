#include "design/ShearConnections.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Structura::Design::Connections {

namespace {

constexpr double kSteelDensity = 7850.0;   // kg/m3

std::string fmt(const char *format, double a = 0, double b = 0, double c = 0, double d = 0, double e = 0, double f = 0)
{
    char buffer[320];
    std::snprintf(buffer, sizeof buffer, format, a, b, c, d, e, f);
    return buffer;
}

Check ratio(const std::string &name, const std::string &clause, double demandN, double resistanceN)
{
    Check c;
    c.name = name;
    c.clause = clause;
    c.demand = demandN / 1000.0;
    c.resistance = resistanceN / 1000.0;
    c.unit = "kN";
    return c;
}

Check ratioMoment(const std::string &name, const std::string &clause, double demandNmm, double resistanceNmm)
{
    Check c;
    c.name = name;
    c.clause = clause;
    c.demand = demandNmm / 1e6;
    c.resistance = resistanceNmm / 1e6;
    c.unit = "kN*m";
    return c;
}

struct BlockShear {
    double rd {0.0};
    double agv {0.0}, anv {0.0}, ant {0.0};
};

/// 6.5.6: Rd = (0,6 fu Anv + Cts fu Ant)/ga2 <= (0,6 fy Agv + Cts fu Ant)/ga2, Cts = 1
BlockShear blockShear(double t, const SteelProps &s, double shearLengthGross, double shearHoles, double tensionLength, double tensionHoles,
                      double holeWidth, double ga2)
{
    BlockShear r;
    r.agv = t * shearLengthGross;
    r.anv = t * std::max(0.0, shearLengthGross - shearHoles * holeWidth);
    r.ant = t * std::max(0.0, tensionLength - tensionHoles * holeWidth);
    const double a = (0.6 * s.fu * r.anv + 1.0 * s.fu * r.ant) / ga2;
    const double b = (0.6 * s.fy * r.agv + 1.0 * s.fu * r.ant) / ga2;
    r.rd = std::min(a, b);
    return r;
}

std::vector<Check> pick(const std::vector<Check> &all, const char *prefix, const std::string &suffix)
{
    std::vector<Check> out;
    for (Check c : all) {
        if (c.name.rfind(prefix, 0) == 0) {
            if (!suffix.empty()) {
                c.name += suffix;
            }
            out.push_back(c);
        }
    }
    return out;
}

void append(std::vector<Check> &to, const std::vector<Check> &from)
{
    to.insert(to.end(), from.begin(), from.end());
}

BoltedInput boltsOf(const BoltGrade &grade, double db, bool thread, int planes, int rows, double pitch, double thickness, const SteelProps &steel,
                    double endDistance, double edgeDistance)
{
    BoltedInput b;
    b.grade = grade;
    b.db = db;
    b.threadInShearPlane = thread || !grade.highStrength;
    b.shearPlanes = planes;
    b.nx = 1;
    b.ny = std::max(1, rows);
    b.sx = 70.0;
    b.sy = pitch;
    b.plateThickness = thickness;
    b.plateFu = steel.fu;
    b.endDistance = endDistance;
    b.edgeDistance = edgeDistance;
    return b;
}

double steelMass(double tMm, double aMm, double bMm)
{
    return tMm * aMm * bMm / 1e9 * kSteelDensity;
}

double boltMass(double db, double grip)
{
    // haste + porca + duas arruelas, estimativa
    return (0.25 * 3.14159265358979 * db * db * (grip + 10.0) / 1e9 * kSteelDensity) * 1.35 + 0.04;
}

} // namespace

double plateHeight(const ShearTabInput &in)
{
    return (std::max(1, in.rows) - 1) * in.pitch + 2.0 * in.edgeV;
}

double angleLength(const DoubleAngleInput &in)
{
    return (std::max(1, in.rows) - 1) * in.pitch + 2.0 * in.edgeV;
}

double worstRatio(const std::vector<Check> &checks)
{
    double worst = 0.0;
    for (const auto &c : checks) {
        worst = std::max(worst, c.ratio());
    }
    return worst;
}

const Check *governing(const std::vector<Check> &checks)
{
    const Check *best = nullptr;
    for (const auto &c : checks) {
        if (!best || c.ratio() > best->ratio()) {
            best = &c;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------------------------------------------------
// Chapa de cisalhamento
// ---------------------------------------------------------------------------------------------------------------------

std::vector<Check> checkShearTab(const ShearTabInput &in)
{
    std::vector<Check> checks;
    const int n = std::max(1, in.rows);
    const double V = std::abs(in.shear);
    const double N = in.axial;
    const double H = plateHeight(in);
    const double a = in.boltLineToWeld;
    const double dh = standardHoleDiameter(in.db);
    const double hole = dh + 2.0;   // 5.2.4.1-a
    const double M = V * a;         // excentricidade da cortante em relacao a linha de furos

    // parafusos: cortante + excentricidade; contato na chapa e na alma
    {
        BoltedInput b = boltsOf(in.grade, in.db, in.threadInShearPlane, 1, n, in.pitch, in.plateThickness, in.plate, std::min(in.edgeV, in.edgeH), in.edgeH);
        b.shearY = V;
        b.shearX = std::abs(N);
        b.momentZ = M;
        const auto plateChecks = checkBolted(b);
        append(checks, pick(plateChecks, "Cisalhamento", ""));
        append(checks, pick(plateChecks, "Pressao de contato", " - chapa"));
        append(checks, pick(plateChecks, "Espacamento", ""));
        append(checks, pick(plateChecks, "Distancia", ""));
        BoltedInput w = b;
        w.plateThickness = in.webThickness;
        w.plateFu = in.web.fu;
        w.endDistance = std::min(in.edgeV, std::max(1.0, in.boltLineToWeld - in.beamSetback));
        auto webChecks = pick(checkBolted(w), "Pressao de contato", " - alma da viga");
        append(checks, webChecks);
    }

    // chapa: cisalhamento (6.5.5), rasgamento (6.5.6), flexao e axial
    {
        const double ag = in.plateThickness * H;
        const double an = in.plateThickness * (H - n * hole);
        Check y = ratio("Chapa - escoamento ao cisalhamento", "6.5.5-a", V, 0.6 * ag * in.plate.fy / in.gammaA1);
        y.lines.push_back(fmt("Ag = t H = %.1f x %.1f = %.0f mm2; VRd = 0,6 Ag fy / ga1 = %.2f kN", in.plateThickness, H, ag, 0.6 * ag * in.plate.fy / in.gammaA1 / 1000.0));
        checks.push_back(y);
        Check r = ratio("Chapa - ruptura ao cisalhamento", "6.5.5-b", V, 0.6 * an * in.plate.fu / in.gammaA2);
        r.lines.push_back(fmt("Anv = t (H - n (dh + 2)) = %.0f mm2; VRd = 0,6 Anv fu / ga2 = %.2f kN", an, 0.6 * an * in.plate.fu / in.gammaA2 / 1000.0));
        checks.push_back(r);
        const BlockShear bs = blockShear(in.plateThickness, in.plate, (n - 1) * in.pitch + in.edgeV, n - 0.5, in.edgeH, 0.5, hole, in.gammaA2);
        Check b = ratio("Chapa - rasgamento (cisalhamento de bloco)", "6.5.6", V, bs.rd);
        b.lines.push_back(fmt("Agv = %.0f; Anv = %.0f; Ant = %.0f mm2 (Cts = 1,0)", bs.agv, bs.anv, bs.ant));
        b.lines.push_back(fmt("Rr,Rd = min[(0,6 fu Anv + Cts fu Ant) ; (0,6 fy Agv + Cts fu Ant)] / ga2 = %.2f kN", bs.rd / 1000.0));
        checks.push_back(b);
        const double mrd = in.plateThickness * H * H * in.plate.fy / (4.0 * in.gammaA1);
        Check m = ratioMoment("Chapa - flexao (excentricidade)", "5.4 / 6.5", M, mrd);
        m.lines.push_back(fmt("M = V a = %.2f kN x %.1f mm = %.2f kN.m", V / 1000.0, a, M / 1e6));
        m.lines.push_back(fmt("MRd = Z fy / ga1 = t H^2 fy / (4 ga1) = %.2f kN.m", mrd / 1e6));
        checks.push_back(m);
        if (std::abs(N) > 0.0) {
            const double yield = ag * in.plate.fy / in.gammaA1;
            const double rupture = std::min(an, 0.85 * ag) * in.plate.fu / in.gammaA2;
            Check x = ratio(N > 0 ? "Chapa - tracao axial" : "Chapa - compressao axial (curta)", "6.5.3 / 6.5.4", std::abs(N), N > 0 ? std::min(yield, rupture) : yield);
            x.lines.push_back(fmt("Ag fy / ga1 = %.2f kN; Ae fu / ga2 = %.2f kN (Ae = An <= 0,85 Ag)", yield / 1000.0, rupture / 1000.0));
            checks.push_back(x);
        }
    }

    // alma da viga: rasgamento
    {
        const double lhw = std::max(1.0, in.boltLineToWeld - in.beamSetback);   // linha de furos ate a extremidade da viga
        const BlockShear bs = blockShear(in.webThickness, in.web, (n - 1) * in.pitch + in.edgeV, n - 0.5, lhw, 0.5, hole, in.gammaA2);
        Check b = ratio("Alma da viga - rasgamento", "6.5.6", V, bs.rd);
        b.lines.push_back(fmt("lh = a - folga = %.1f mm; Agv = %.0f; Anv = %.0f; Ant = %.0f mm2; Rr,Rd = %.2f kN", lhw, bs.agv, bs.anv, bs.ant, bs.rd / 1000.0));
        checks.push_back(b);
    }

    // solda: dois filetes verticais, cortante direta + flexao (+ axial), metodo elastico
    {
        const double te = 0.707 * in.weldLeg;
        const double fv = V / (2.0 * H);                                   // N/mm por filete
        const double fn = std::abs(N) / (2.0 * H) + M / (H * H / 3.0);     // normal ao filete (flexao no plano + axial)
        const double fr = std::hypot(fv, fn);
        const double perLength = 0.6 * te * in.weldFw / in.gammaW2;
        Check s = ratio("Solda de filete (metodo elastico)", "6.2.5", fr, perLength);
        s.unit = "N/mm";
        s.demand = fr;
        s.resistance = perLength;
        s.lines.push_back(fmt("2 filetes de %.1f mm (te = %.2f mm) x %.0f mm; fv = V/(2H) = %.2f N/mm; fn = N/(2H) + M/(H^2/3) = %.2f N/mm", in.weldLeg, te, H, fv, fn));
        s.lines.push_back(fmt("fr = %.2f N/mm <= 0,6 te fw / gw2 = %.2f N/mm", fr, perLength));
        checks.push_back(s);
        const double basePlate = std::min(0.6 * in.plateThickness * in.plate.fy / in.gammaA1, 0.6 * in.plateThickness * in.plate.fu / in.gammaA2);
        const double baseSupport = std::min(0.6 * in.supportThickness * in.support.fy / in.gammaA1, 0.6 * in.supportThickness * in.support.fu / in.gammaA2);
        Check b = ratio("Solda - metal-base (chapa e apoio)", "6.2.5 / 6.5", 2.0 * fr, std::min(basePlate, baseSupport));
        b.unit = "N/mm";
        b.demand = 2.0 * fr;
        b.resistance = std::min(basePlate, baseSupport);
        b.lines.push_back(fmt("2 fr = %.2f N/mm <= min(chapa %.2f ; apoio %.2f) N/mm", 2.0 * fr, basePlate, baseSupport));
        checks.push_back(b);
        const double thin = std::min(in.plateThickness, in.supportThickness);
        double minLeg = 3.0;
        if (thin > 19.0) minLeg = 8.0;
        else if (thin > 12.5) minLeg = 6.0;
        else if (thin > 6.3) minLeg = 5.0;
        Check l;
        l.name = "Perna minima do filete";
        l.clause = "6.2.6.2.1 / Tab. 11";
        l.unit = "mm";
        l.minimumRule = true;
        l.demand = in.weldLeg;
        l.resistance = minLeg;
        l.lines.push_back(fmt("a = %.1f mm >= %.1f mm (parte mais fina %.1f mm)", in.weldLeg, minLeg, thin));
        checks.push_back(l);
        const double maxLeg = in.plateThickness < 6.3 ? in.plateThickness : in.plateThickness - 1.5;
        Check m;
        m.name = "Perna maxima do filete (borda da chapa)";
        m.clause = "6.2.6.2.2";
        m.unit = "mm";
        m.minimumRule = true;
        m.demand = maxLeg;
        m.resistance = in.weldLeg;
        m.lines.push_back(fmt("a = %.1f mm <= %.1f mm", in.weldLeg, maxLeg));
        checks.push_back(m);
    }

    // altura da chapa em relacao a viga
    if (in.beamDepth > 0.0) {
        Check c;
        c.name = "Altura da chapa x altura da viga";
        c.clause = "detalhamento";
        c.unit = "mm";
        c.minimumRule = true;
        c.demand = in.beamDepth;
        c.resistance = H;
        c.lines.push_back(fmt("H = %.0f mm <= altura da viga = %.0f mm", H, in.beamDepth));
        checks.push_back(c);
    }
    return checks;
}

std::vector<Part> partsShearTab(const ShearTabInput &in)
{
    std::vector<Part> parts;
    const double H = plateHeight(in);
    const double W = in.boltLineToWeld + in.edgeH;
    Part plate;
    plate.description = fmt("Chapa %.1f x %.0f x %.0f mm", in.plateThickness, W, H);
    plate.quantity = 1;
    plate.massEach = steelMass(in.plateThickness, H, W);
    parts.push_back(plate);
    Part bolts;
    bolts.description = fmt("Parafuso d = %.0f mm (+ porca e arruelas)", in.db);
    bolts.quantity = std::max(1, in.rows);
    bolts.massEach = boltMass(in.db, in.plateThickness + in.webThickness);
    parts.push_back(bolts);
    Part weld;
    weld.description = fmt("Filete de solda %.0f mm (2 x %.0f mm)", in.weldLeg, H);
    weld.quantity = 1;
    weld.length = 2.0 * H;
    weld.unit = "mm";
    weld.massEach = 0.5 * in.weldLeg * in.weldLeg * 2.0 * H / 1e9 * kSteelDensity;
    parts.push_back(weld);
    return parts;
}

// ---------------------------------------------------------------------------------------------------------------------
// Cantoneira dupla
// ---------------------------------------------------------------------------------------------------------------------

std::vector<Check> checkDoubleAngle(const DoubleAngleInput &in)
{
    std::vector<Check> checks;
    const int n = std::max(1, in.rows);
    const double V = std::abs(in.shear);
    const double N = in.axial;
    const double L = angleLength(in);
    const double dh = standardHoleDiameter(in.db);
    const double hole = dh + 2.0;
    const double a = in.gaugeSupport;   // excentricidade entre a linha de furos do apoio e a da alma (aprox.: gauge da aba do apoio)
    const double M = V * a;

    // lado da alma: corte duplo, V com excentricidade
    {
        BoltedInput w = boltsOf(in.grade, in.db, in.threadInShearPlane, 2, n, in.pitch, in.webThickness, in.web, in.edgeV, in.gaugeWeb);
        w.shearY = V;
        w.shearX = std::abs(N);
        w.momentZ = M;
        const auto webChecks = checkBolted(w);
        for (Check c : pick(webChecks, "Cisalhamento", " - lado da alma (corte duplo)")) {
            checks.push_back(c);
        }
        append(checks, pick(webChecks, "Pressao de contato", " - alma da viga"));
        append(checks, pick(webChecks, "Espacamento", ""));
        BoltedInput g = w;
        g.plateThickness = 2.0 * in.angleThickness;   // o parafuso atravessa as duas cantoneiras
        g.plateFu = in.angle.fu;
        auto angleChecks = pick(checkBolted(g), "Pressao de contato", " - cantoneiras (2 t)");
        append(checks, angleChecks);
    }
    // lado do apoio: cada cantoneira recebe V/2, corte simples; N/2 de tracao por cantoneira
    {
        BoltedInput s = boltsOf(in.grade, in.db, in.threadInShearPlane, 1, n, in.pitch, in.angleThickness, in.angle, in.edgeV, in.supportEdgeDistance);
        s.shearY = V / 2.0;
        s.tension = N > 0.0 ? N / 2.0 : 0.0;
        const auto supChecks = checkBolted(s);
        append(checks, pick(supChecks, "Cisalhamento", " - lado do apoio (por cantoneira)"));
        append(checks, pick(supChecks, "Pressao de contato", " - cantoneira (apoio)"));
        append(checks, pick(supChecks, "Tracao", " - lado do apoio"));
        BoltedInput g = s;
        g.plateThickness = in.supportThickness;
        g.plateFu = in.support.fu;
        append(checks, pick(checkBolted(g), "Pressao de contato", " - apoio"));
    }
    // cantoneira (cada uma recebe V/2 e momento V/2 * a)
    {
        const double Vc = V / 2.0;
        const double ag = in.angleThickness * L;
        const double an = in.angleThickness * (L - n * hole);
        Check y = ratio("Cantoneira - escoamento ao cisalhamento", "6.5.5-a", Vc, 0.6 * ag * in.angle.fy / in.gammaA1);
        y.lines.push_back(fmt("Ag = t L = %.1f x %.1f mm; VRd = %.2f kN (por cantoneira, V/2 = %.2f kN)", in.angleThickness, L, 0.6 * ag * in.angle.fy / in.gammaA1 / 1000.0, Vc / 1000.0));
        checks.push_back(y);
        Check r = ratio("Cantoneira - ruptura ao cisalhamento", "6.5.5-b", Vc, 0.6 * an * in.angle.fu / in.gammaA2);
        r.lines.push_back(fmt("Anv = %.0f mm2; VRd = 0,6 Anv fu / ga2 = %.2f kN", an, 0.6 * an * in.angle.fu / in.gammaA2 / 1000.0));
        checks.push_back(r);
        const BlockShear bs = blockShear(in.angleThickness, in.angle, (n - 1) * in.pitch + in.edgeV, n - 0.5, in.gaugeWeb, 0.5, hole, in.gammaA2);
        Check b = ratio("Cantoneira - rasgamento (aba da alma)", "6.5.6", Vc, bs.rd);
        b.lines.push_back(fmt("Agv = %.0f; Anv = %.0f; Ant = %.0f mm2; Rr,Rd = %.2f kN", bs.agv, bs.anv, bs.ant, bs.rd / 1000.0));
        checks.push_back(b);
        const double mrd = in.angleThickness * L * L * in.angle.fy / (4.0 * in.gammaA1);
        Check m = ratioMoment("Cantoneira - flexao (excentricidade)", "5.4 / 6.5", Vc * a, mrd);
        m.lines.push_back(fmt("M = (V/2) a = %.2f kN.m; MRd = t L^2 fy / (4 ga1) = %.2f kN.m", Vc * a / 1e6, mrd / 1e6));
        checks.push_back(m);
    }
    // alma: rasgamento
    {
        const BlockShear bs = blockShear(in.webThickness, in.web, (n - 1) * in.pitch + in.edgeV, n - 0.5, in.webEndDistance, 0.5, hole, in.gammaA2);
        Check b = ratio("Alma da viga - rasgamento", "6.5.6", V, bs.rd);
        b.lines.push_back(fmt("Agv = %.0f; Anv = %.0f; Ant = %.0f mm2; Rr,Rd = %.2f kN", bs.agv, bs.anv, bs.ant, bs.rd / 1000.0));
        checks.push_back(b);
    }
    if (in.beamDepth > 0.0) {
        Check c;
        c.name = "Comprimento da cantoneira x altura da viga";
        c.clause = "detalhamento";
        c.unit = "mm";
        c.minimumRule = true;
        c.demand = in.beamDepth;
        c.resistance = L;
        c.lines.push_back(fmt("L = %.0f mm <= altura da viga = %.0f mm", L, in.beamDepth));
        checks.push_back(c);
    }
    return checks;
}

std::vector<Part> partsDoubleAngle(const DoubleAngleInput &in)
{
    std::vector<Part> parts;
    const double L = angleLength(in);
    const double leg1 = in.gaugeWeb + in.webEndDistance;
    const double leg2 = in.gaugeSupport + in.supportEdgeDistance;
    Part angle;
    angle.description = fmt("Cantoneira %.1f mm, abas %.0f x %.0f, comprimento %.0f mm", in.angleThickness, leg1, leg2, L);
    angle.quantity = 2;
    angle.massEach = steelMass(in.angleThickness, L, leg1 + leg2 - in.angleThickness);
    parts.push_back(angle);
    Part web;
    web.description = fmt("Parafuso d = %.0f mm na alma (corte duplo)", in.db);
    web.quantity = std::max(1, in.rows);
    web.massEach = boltMass(in.db, 2.0 * in.angleThickness + in.webThickness);
    parts.push_back(web);
    Part support;
    support.description = fmt("Parafuso d = %.0f mm no apoio", in.db);
    support.quantity = 2 * std::max(1, in.rows);
    support.massEach = boltMass(in.db, in.angleThickness + in.supportThickness);
    parts.push_back(support);
    return parts;
}

// ---------------------------------------------------------------------------------------------------------------------
// Proposta automatica
// ---------------------------------------------------------------------------------------------------------------------

ShearTabProposal proposeShearTab(ShearTabInput base)
{
    ShearTabProposal best;
    double bestCost = 1e300;
    static const double diameters[] = {16.0, 20.0, 22.0, 24.0};
    static const double thicknesses[] = {6.3, 8.0, 9.5, 12.5, 16.0, 19.0};
    static const double legs[] = {4.0, 5.0, 6.0, 8.0, 10.0};
    for (const double db : diameters) {
        for (int rows = 2; rows <= 10; ++rows) {
            for (const double t : thicknesses) {
                for (const double leg : legs) {
                    ShearTabInput in = base;
                    in.db = db;
                    in.rows = rows;
                    in.pitch = std::max(base.pitch, 3.0 * db);
                    in.edgeV = std::max(base.edgeV, minimumEdgeDistance(db));
                    in.edgeH = std::max(base.edgeH, minimumEdgeDistance(db));
                    in.boltLineToWeld = std::max(base.boltLineToWeld, in.beamSetback + 2.0 * db);
                    in.plateThickness = t;
                    in.weldLeg = leg;
                    if (in.beamDepth > 0.0 && plateHeight(in) > in.beamDepth) {
                        continue;
                    }
                    const auto checks = checkShearTab(in);
                    const double worst = worstRatio(checks);
                    if (worst > 1.0 + 1e-9) {
                        continue;
                    }
                    double cost = 0.0;
                    for (const auto &p : partsShearTab(in)) {
                        cost += p.totalMass() + (p.length > 0.0 ? 0.0 : 0.0);
                    }
                    cost += 0.01 * 2.0 * plateHeight(in) * leg * leg / 100.0;   // solda penaliza levemente
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
    return best;
}

DoubleAngleProposal proposeDoubleAngle(DoubleAngleInput base)
{
    DoubleAngleProposal best;
    double bestCost = 1e300;
    static const double diameters[] = {16.0, 20.0, 22.0, 24.0};
    static const double thicknesses[] = {6.3, 8.0, 9.5, 12.5, 16.0};
    for (const double db : diameters) {
        for (int rows = 2; rows <= 10; ++rows) {
            for (const double t : thicknesses) {
                DoubleAngleInput in = base;
                in.db = db;
                in.rows = rows;
                in.pitch = std::max(base.pitch, 3.0 * db);
                in.edgeV = std::max(base.edgeV, minimumEdgeDistance(db));
                in.gaugeWeb = std::max(base.gaugeWeb, minimumEdgeDistance(db) + 10.0);
                in.gaugeSupport = std::max(base.gaugeSupport, minimumEdgeDistance(db) + 10.0);
                in.webEndDistance = std::max(base.webEndDistance, minimumEdgeDistance(db));
                in.supportEdgeDistance = std::max(base.supportEdgeDistance, minimumEdgeDistance(db));
                in.angleThickness = t;
                if (in.beamDepth > 0.0 && angleLength(in) > in.beamDepth) {
                    continue;
                }
                const auto checks = checkDoubleAngle(in);
                const double worst = worstRatio(checks);
                if (worst > 1.0 + 1e-9) {
                    continue;
                }
                double cost = 0.0;
                for (const auto &p : partsDoubleAngle(in)) {
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
    return best;
}

} // namespace Structura::Design::Connections
