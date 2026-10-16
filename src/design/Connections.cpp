#include "design/Connections.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Structura::Design::Connections {

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string fmt(const char *format, double a = 0, double b = 0, double c = 0, double d = 0, double e = 0)
{
    char buffer[256];
    std::snprintf(buffer, sizeof buffer, format, a, b, c, d, e);
    return buffer;
}

Check ratioCheck(const std::string &name, const std::string &clause, double demand, double resistance)
{
    Check c;
    c.name = name;
    c.clause = clause;
    c.demand = demand;
    c.resistance = resistance;
    c.unit = "kN";
    return c;
}

Check minimumCheck(const std::string &name, const std::string &clause, double adopted, double minimum)
{
    Check c;
    c.name = name;
    c.clause = clause;
    c.demand = adopted;
    c.resistance = minimum;
    c.unit = "mm";
    c.minimumRule = true;
    return c;
}

} // namespace

const std::vector<BoltGrade> &boltGrades()
{
    static const std::vector<BoltGrade> grades = {
        {"ASTM A307", 0.0, 415.0, false},
        {"ISO 4.6", 235.0, 400.0, false},
        {"ASTM A325 / F1852", 635.0, 830.0, true},
        {"ISO 8.8", 640.0, 800.0, true},
        {"ASTM A490 / F2280", 895.0, 1040.0, true},
        {"ISO 10.9", 900.0, 1000.0, true},
    };
    return grades;
}

double standardHoleDiameter(double db)
{
    // Tabela 14 (mm): 16 -> 18, 20 -> 22, 22 -> 24, 24 -> 27, 27 -> 30, 30 -> 33; db + 3 acima
    return db <= 22.5 ? db + 2.0 : db + 3.0;
}

double minimumEdgeDistance(double db)
{
    // Tabela 16 (furo-padrao, bordas cortadas - valores conservadores); acima de 36 mm: 1,25 db
    struct Row { double db; double distance; };
    static const Row rows[] = {{12.7, 22}, {16, 22}, {20, 26}, {22, 28}, {24, 30}, {27, 34}, {30, 38}, {36, 46}};
    if (db > 36.0) {
        return 1.25 * db;
    }
    for (const Row &r : rows) {
        if (db <= r.db + 1e-9) {
            return r.distance;
        }
    }
    return 1.25 * db;
}

std::vector<Check> checkBolted(const BoltedInput &in)
{
    std::vector<Check> checks;
    const int n = std::max(1, in.nx) * std::max(1, in.ny);
    const double ab = 0.25 * kPi * in.db * in.db;
    const double abe = 0.75 * ab;
    const double g = in.gammaA2;
    const double fub = in.grade.fub;

    // distribuicao elastica no plano: cortante direto + torcao em torno do centroide
    double sumR2 = 0.0;
    double maxX = 0.0, maxY = 0.0;
    struct Pos { double x, y; };
    std::vector<Pos> bolts;
    for (int i = 0; i < std::max(1, in.nx); ++i) {
        for (int j = 0; j < std::max(1, in.ny); ++j) {
            const double x = (i - (std::max(1, in.nx) - 1) / 2.0) * in.sx;
            const double y = (j - (std::max(1, in.ny) - 1) / 2.0) * in.sy;
            bolts.push_back({x, y});
            sumR2 += x * x + y * y;
        }
    }
    double worst = 0.0;
    for (const Pos &p : bolts) {
        double fx = in.shearX / n, fy = in.shearY / n;
        if (sumR2 > 0.0) {
            fx += -in.momentZ * p.y / sumR2;
            fy += in.momentZ * p.x / sumR2;
        }
        worst = std::max(worst, std::hypot(fx, fy));
        maxX = std::max(maxX, std::abs(p.x));
        maxY = std::max(maxY, std::abs(p.y));
    }
    const double fvSd = worst;                                  // por parafuso (N)
    const double fvSdPlane = fvSd / std::max(1, in.shearPlanes);
    const double ftSd = in.tension / n;

    // cisalhamento (6.3.3.2)
    const bool protectedThread = in.grade.highStrength && !in.threadInShearPlane;
    const double coef = protectedThread ? 0.56 : 0.45;
    const double fvRd = coef * ab * fub / g;
    {
        Check c = ratioCheck("Cisalhamento do parafuso", "6.3.3.2", fvSdPlane / 1000.0, fvRd / 1000.0);
        c.lines.push_back(fmt("Ab = 0,25 pi db^2 = %.1f mm2", ab));
        c.lines.push_back(fmt("Fv,Rd = %.2f Ab fub / ga2 = %.2f kN por plano de corte", coef, fvRd / 1000.0));
        c.lines.push_back(fmt("Fv,Sd (parafuso mais solicitado) = %.2f kN; %.0f plano(s) -> %.2f kN por plano", fvSd / 1000.0, double(in.shearPlanes), fvSdPlane / 1000.0));
        checks.push_back(c);
    }

    // pressao de contato (6.3.3.3)
    const double dh = standardHoleDiameter(in.db);
    double pitch = 1e18;
    if (in.nx > 1) pitch = std::min(pitch, in.sx);
    if (in.ny > 1) pitch = std::min(pitch, in.sy);
    const double lfEdge = in.endDistance - dh / 2.0;
    const double lfPitch = pitch < 1e17 ? pitch - dh : 1e18;
    const double lf = std::min(lfEdge, lfPitch);
    const double k1 = in.deformationLimit ? 1.2 : 1.5;
    const double k2 = in.deformationLimit ? 2.4 : 3.0;
    const double fcRd = std::min(k1 * lf * in.plateThickness * in.plateFu, k2 * in.db * in.plateThickness * in.plateFu) / g;
    {
        Check c = ratioCheck("Pressao de contato / rasgamento", "6.3.3.3", fvSd / 1000.0, std::max(0.0, fcRd) / 1000.0);
        c.lines.push_back(fmt("dh = %.1f mm; lf = min(%.1f, %.1f) mm (conservador: aplicado a todos os parafusos)", dh, lfEdge, lfPitch > 1e17 ? 0.0 : lfPitch));
        c.lines.push_back(fmt("Fc,Rd = min(%.1f lf t fu ; %.1f db t fu) / ga2 = %.2f kN", k1, k2, fcRd / 1000.0));
        checks.push_back(c);
    }

    // tracao (6.3.3.1) e interacao (6.3.3.4)
    if (in.tension > 0.0) {
        const double ftRd = abe * fub / g;
        Check c = ratioCheck("Tracao no parafuso", "6.3.3.1", ftSd / 1000.0, ftRd / 1000.0);
        c.lines.push_back(fmt("Abe = 0,75 Ab = %.1f mm2", abe));
        c.lines.push_back(fmt("Ft,Rd = Abe fub / ga2 = %.2f kN; Ft,Sd = %.2f kN por parafuso (sem efeito alavanca)", ftRd / 1000.0, ftSd / 1000.0));
        checks.push_back(c);
        const double interaction = std::pow(ftSd / ftRd, 2) + std::pow(fvSdPlane / fvRd, 2);
        Check ci = ratioCheck("Tracao e cisalhamento combinados", "6.3.3.4", interaction, 1.0);
        ci.unit = "-";
        ci.lines.push_back(fmt("(Ft,Sd/Ft,Rd)^2 + (Fv,Sd/Fv,Rd)^2 = (%.3f)^2 + (%.3f)^2 = %.3f <= 1,0", ftSd / ftRd, fvSdPlane / fvRd, interaction));
        checks.push_back(ci);
    }

    // espacamentos
    if (n > 1) {
        Check c = minimumCheck("Espacamento minimo entre furos", "6.3.9", pitch, 3.0 * in.db);
        c.lines.push_back(fmt("p = %.1f mm >= 3 db = %.1f mm (recomendado 2,7 db)", pitch, 3.0 * in.db));
        checks.push_back(c);
    }
    {
        const double minEdge = minimumEdgeDistance(in.db);
        Check c = minimumCheck("Distancia minima furo-borda", "6.3.11 / Tab. 16", std::min(in.endDistance, in.edgeDistance), minEdge);
        c.lines.push_back(fmt("min(le = %.1f, lt = %.1f) mm >= %.1f mm (valor conservador da tabela; menores distancias se 6.3.3.3 for atendida)", in.endDistance, in.edgeDistance, minEdge));
        checks.push_back(c);
        const double maxEdge = std::min(12.0 * in.plateThickness, 150.0);
        Check m = minimumCheck("Distancia maxima furo-borda", "6.3.12", maxEdge, std::max(in.endDistance, in.edgeDistance));
        m.lines.push_back(fmt("max(le, lt) = %.1f mm <= min(12 t ; 150) = %.1f mm", std::max(in.endDistance, in.edgeDistance), maxEdge));
        checks.push_back(m);
    }
    (void)maxX;
    (void)maxY;
    return checks;
}

std::vector<Check> checkFilletWeld(const WeldInput &in)
{
    std::vector<Check> checks;
    const double aw = 0.707 * in.leg * in.length;
    const double theta = in.angleDeg * kPi / 180.0;
    const double angleFactor = in.useAngleFactor ? 1.0 + 0.5 * std::pow(std::sin(theta), 1.5) : 1.0;
    {
        const double fwRd = 0.6 * aw * in.fw * angleFactor / in.gammaW2;
        Check c = ratioCheck("Metal da solda (filete)", "6.2.5", in.force / 1000.0, fwRd / 1000.0);
        c.lines.push_back(fmt("Aw = 0,707 a L = 0,707 x %.1f x %.1f = %.0f mm2", in.leg, in.length, aw));
        c.lines.push_back(fmt("Fw,Rd = 0,6 Aw fw (1 + 0,5 sen^1,5 theta) / gw2 = 0,6 x %.0f x %.0f x %.3f / 1,35 = %.2f kN", aw, in.fw, angleFactor, fwRd / 1000.0));
        checks.push_back(c);
    }
    {
        const double shearArea = in.plateThickness * in.length;
        const double rupture = 0.6 * shearArea * in.plateFu / in.gammaA2;
        const double yielding = 0.6 * shearArea * in.plateFy / in.gammaA1;
        const double rd = std::min(rupture, yielding);
        Check c = ratioCheck("Metal-base (cisalhamento)", "6.5", in.force / 1000.0, rd / 1000.0);
        c.lines.push_back(fmt("Escoamento: 0,6 Agv fy / ga1 = %.2f kN; ruptura: 0,6 Anv fu / ga2 = %.2f kN (Agv = Anv = t L)", yielding / 1000.0, rupture / 1000.0));
        checks.push_back(c);
    }
    {
        double minLeg = 3.0;
        if (in.plateThickness > 19.0) minLeg = 8.0;
        else if (in.plateThickness > 12.5) minLeg = 6.0;
        else if (in.plateThickness > 6.3) minLeg = 5.0;
        Check c = minimumCheck("Perna minima do filete", "6.2.6.2.1 / Tab. 11", in.leg, minLeg);
        c.lines.push_back(fmt("a = %.1f mm >= %.1f mm (parte mais fina %.1f mm)", in.leg, minLeg, in.plateThickness));
        checks.push_back(c);
        const double maxLeg = in.plateThickness < 6.3 ? in.plateThickness : in.plateThickness - 1.5;
        Check m = minimumCheck("Perna maxima ao longo de bordas", "6.2.6.2.2", maxLeg, in.leg);
        m.lines.push_back(fmt("a = %.1f mm <= %.1f mm", in.leg, maxLeg));
        checks.push_back(m);
        const double minLength = std::max(4.0 * in.leg, 40.0);
        Check l = minimumCheck("Comprimento minimo efetivo", "6.2.6.2.3", in.length, minLength);
        l.lines.push_back(fmt("L = %.1f mm >= max(4a ; 40) = %.1f mm", in.length, minLength));
        checks.push_back(l);
    }
    return checks;
}

} // namespace Structura::Design::Connections
