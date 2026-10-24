#include "design/Connections.h"
#include "design/ConnectionHelpers.h"

#include <algorithm>
#include <array>
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
    // Tabela 16 (NBR 8800:2024): 1/2" 19; 5/8" e 16 mm 22; 3/4" 25; 20 mm 26; 7/8" e 22 mm 28; 24 mm 30; 1" 32; 27 mm 34; 1 1/8" e 30 mm 38;
    // 1 1/4" 41; acima disso (e a partir de 36 mm) 1,25 db
    struct Row { double db; double distance; };
    static const Row rows[] = {{12.7, 19}, {15.875, 22}, {16, 22}, {19.05, 25}, {20, 26}, {22.225, 28}, {24, 30}, {25.4, 32}, {27, 34}, {30, 38}, {31.75, 41}};
    if (db > 31.75 + 1e-9) {
        return 1.25 * db;
    }
    for (const Row &r : rows) {
        if (db <= r.db + 1e-9) {
            return r.distance;
        }
    }
    return 1.25 * db;
}

double boltShearResistance(const BoltedInput &in)
{
    const double ab = 0.25 * kPi * in.db * in.db;
    const bool protectedThread = in.grade.highStrength && !in.threadInShearPlane;
    const double coef = protectedThread ? 0.56 : 0.45;
    const double excessGrip = in.pretensioned ? 0.0 : std::max(0.0, in.grip - 5.0 * in.db);
    const double gripFactor = std::max(0.0, 1.0 - 0.01 * excessGrip / 1.5);
    return coef * ab * in.grade.fub / in.gammaA2 * gripFactor;
}

double boltBearingResistance(const BoltedInput &in)
{
    const double dh = standardHoleDiameter(in.db);
    double pitch = 1e18;
    if (in.nx > 1) pitch = std::min(pitch, in.sx);
    if (in.ny > 1) pitch = std::min(pitch, in.sy);
    const double lfEdge = in.endDistance - dh / 2.0;
    const double lfPitch = pitch < 1e17 ? pitch - dh : 1e18;
    const double lf = std::min(lfEdge, lfPitch);
    const double k1 = in.deformationLimit ? 1.2 : 1.5;
    const double k2 = in.deformationLimit ? 2.4 : 3.0;
    return std::max(0.0, std::min(k1 * lf * in.plateThickness * in.plateFu, k2 * in.db * in.plateThickness * in.plateFu) / in.gammaA2);
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
    // 6.3.8: emenda por contato de barra tracionada com mais de 950 mm na direcao da forca
    const bool longJoint = in.connectionLength > 950.0;
    const double fvSd = worst * (longJoint ? 1.20 : 1.0);       // por parafuso (N)
    const double fvSdPlane = fvSd / std::max(1, in.shearPlanes);
    const double ftSd = in.tension / n;

    // cisalhamento (6.3.3.2)
    const bool protectedThread = in.grade.highStrength && !in.threadInShearPlane;
    const double coef = protectedThread ? 0.56 : 0.45;
    // 6.3.7: pega acima de 5 db reduz Fv,Rd em 1 % por 1,5 mm adicionais (nao se aplica a parafuso protendido)
    const double excessGrip = in.pretensioned ? 0.0 : std::max(0.0, in.grip - 5.0 * in.db);
    const double gripFactor = std::max(0.0, 1.0 - 0.01 * excessGrip / 1.5);
    const double fvRd = boltShearResistance(in);
    {
        Check c = ratioCheck("Cisalhamento do parafuso", "6.3.3.2", fvSdPlane / 1000.0, fvRd / 1000.0);
        c.lines.push_back(fmt("Ab = 0,25 pi db^2 = %.1f mm2", ab));
        c.lines.push_back(fmt("Fv,Rd = %.2f Ab fub / ga2 = %.2f kN por plano de corte", coef, coef * ab * fub / g / 1000.0));
        if (gripFactor < 1.0) {
            c.clause = "6.3.3.2 / 6.3.7";
            c.lines.push_back(fmt("Pega longa: %.1f mm > 5 db = %.1f mm; reducao de 1 %% por 1,5 mm -> fator %.3f; Fv,Rd = %.2f kN", in.grip, 5.0 * in.db, gripFactor, fvRd / 1000.0));
        }
        if (longJoint) {
            c.clause = "6.3.3.2 / 6.3.8";
            c.lines.push_back(fmt("Ligacao de grande comprimento (%.0f mm > 950 mm): Fv,Sd e Fc,Sd multiplicados por 1,20", in.connectionLength));
        }
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
    const double k1 = in.deformationLimit ? 1.2 : 1.5;
    const double k2 = in.deformationLimit ? 2.4 : 3.0;
    const double fcRd = boltBearingResistance(in);
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
        Check clear = minimumCheck("Distancia livre entre bordas dos furos", "6.3.9", pitch - dh, in.db);
        clear.lines.push_back(fmt("p - dh = %.1f - %.1f = %.1f mm >= db = %.1f mm", pitch, dh, pitch - dh, in.db));
        checks.push_back(clear);
        const double maxPitch = std::min(24.0 * in.plateThickness, 300.0);
        Check mx = minimumCheck("Espacamento maximo entre parafusos", "6.3.10", maxPitch, pitch);
        mx.lines.push_back(fmt("p = %.1f mm <= min(24 t ; 300) = %.1f mm (elementos pintados ou sem corrosao; 14 t / 180 mm em aco patinavel nao pintado)", pitch, maxPitch));
        checks.push_back(mx);
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

double minimumPretension(const BoltGrade &grade, double d)
{
    struct Row { double mm, a325, a490; };
    static const Row table[] = {{12.7, 53, 67}, {15.875, 85, 106}, {16, 91, 114}, {19.05, 125, 157}, {20, 142, 178}, {22, 176, 221}, {22.225, 173, 217},
                                {24, 205, 257}, {25.4, 227, 285}, {27, 267, 334}, {28.575, 286, 358}, {30, 326, 408}, {31.75, 363, 455}, {34.925, 433, 542},
                                {36, 475, 595}, {38.1, 527, 660}};
    const bool a490 = grade.name.find("A490") != std::string::npos;
    const bool a325 = grade.name.find("A325") != std::string::npos;
    if (!a490 && !a325) {
        return 0.0;
    }
    for (const Row &r : table) {
        if (std::abs(r.mm - d) < 0.6) {
            return a490 ? r.a490 : r.a325;
        }
    }
    return 0.0;
}

std::vector<Check> checkSlip(const SlipInput &in)
{
    std::vector<Check> checks;
    const double ftb = minimumPretension(in.grade, in.db) * 1000.0;
    if (ftb <= 0.0) {
        Check c = ratioCheck("Deslizamento (ligacao por atrito)", "6.3.4", 0.0, 1.0);
        c.lines.push_back("A forca de protensao minima (Tabela 19) so e fornecida para parafusos ASTM F3125 A325/A490 nos diametros tabelados: verificacao nao executada.");
        c.unit = "-";
        checks.push_back(c);
        return checks;
    }
    const double mu = in.surfaceClass == 1 ? 0.50 : (in.surfaceClass == 2 ? 0.20 : 0.30);
    const double ce = in.fillers ? 0.85 : 1.0;
    const int n = std::max(1, in.bolts);
    const double fvSd = in.shear / n;
    const double ftSd = in.tension / n;
    const double ns = std::max(1, in.planes);
    if (in.oversized) {
        const double gammaE = 1.20;   // Tabela 13, combinacoes normais
        const double rd = 1.13 * mu * ce * ftb * ns / gammaE * std::max(0.0, 1.0 - ftSd / (1.13 * ftb));
        Check c = ratioCheck("Deslizamento (ELU, furos alargados)", "6.3.4.3", fvSd / 1000.0, rd / 1000.0);
        c.lines.push_back(detail::fmtn("Ff,Rd = 1,13 mu Ce FTb ns (1 - Ft,Sd/(1,13 FTb))/ge = 1,13 x %.2f x %.2f x %.0f x %.0f (1 - %.0f/%.0f)/1,20 = %.2f kN", mu, ce, ftb / 1000.0, ns, ftSd / 1000.0, 1.13 * ftb / 1000.0, rd / 1000.0));
        c.lines.push_back(detail::fmtn("Ff,Sd por parafuso = %.2f kN (FTb = %.0f kN, Tab. 19)", fvSd / 1000.0, ftb / 1000.0));
        checks.push_back(c);
    } else {
        const double ftSk = 0.7 * ftSd;
        const double rk = 0.80 * mu * ce * ftb * ns * std::max(0.0, 1.0 - ftSk / (0.80 * ftb));
        const double sk = 0.7 * fvSd;
        Check c = ratioCheck("Deslizamento (ELS, furos-padrao)", "6.3.4.4", sk / 1000.0, rk / 1000.0);
        c.lines.push_back(detail::fmtn("Ff,Rk = 0,80 mu Ce FTb ns (1 - Ft,Sk/(0,80 FTb)) = 0,80 x %.2f x %.2f x %.0f x %.0f (1 - %.0f/%.0f) = %.2f kN", mu, ce, ftb / 1000.0, ns, ftSk / 1000.0, 0.8 * ftb / 1000.0, rk / 1000.0));
        c.lines.push_back(detail::fmtn("Forca de servico por parafuso = 70%% do cortante de calculo = %.2f kN (FTb = %.0f kN, Tab. 19)", sk / 1000.0, ftb / 1000.0));
        checks.push_back(c);
    }
    return checks;
}

std::vector<Check> checkFilletWeld(const WeldInput &in)
{
    std::vector<Check> checks;
    const double aw = 0.707 * in.leg * in.length;
    const double theta = in.angleDeg * kPi / 180.0;
    const double angleFactor = in.useAngleFactor ? 1.0 + 0.5 * std::pow(std::sin(theta), 1.5) : 1.0;
    {
        const bool mixed = in.lengthTransverse > 0.0 && in.lengthTransverse < in.length;
        double fwRd = 0.6 * aw * in.fw * angleFactor / in.gammaW2;
        if (mixed) {
            // 6.2.5.2-c: filetes longitudinais + transversais carregados concentricamente (cada parcela pela Tabela 9)
            const double awT = 0.707 * in.leg * in.lengthTransverse, awL = aw - awT;
            const double fl = 0.6 * awL * in.fw / in.gammaW2, ft = 0.6 * awT * in.fw / in.gammaW2;
            fwRd = weldCombinedResistance(fl, ft);
            Check c = ratioCheck("Metal da solda (filete, grupo longitudinal + transversal)", "6.2.5.2-c", in.force / 1000.0, fwRd / 1000.0);
            c.lines.push_back(fmt("Awl = %.0f mm2; Awt = %.0f mm2; Fwl = 0,6 Awl fw / gw2 = %.2f kN; Fwt = 0,6 Awt fw / gw2 = %.2f kN", awL, awT, fl / 1000.0, ft / 1000.0));
            c.lines.push_back(fmt("Fw,Rd = max(Fwl + Fwt ; 0,85 Fwl + 1,5 Fwt) = max(%.2f ; %.2f) = %.2f kN", (fl + ft) / 1000.0, (0.85 * fl + 1.5 * ft) / 1000.0, fwRd / 1000.0));
            checks.push_back(c);
        } else {
            Check c = ratioCheck("Metal da solda (filete)", "6.2.5", in.force / 1000.0, fwRd / 1000.0);
            c.lines.push_back(fmt("Aw = 0,707 a L = 0,707 x %.1f x %.1f = %.0f mm2", in.leg, in.length, aw));
            c.lines.push_back(fmt("Fw,Rd = 0,6 Aw fw (1 + 0,5 sen^1,5 theta) / gw2 = 0,6 x %.0f x %.0f x %.3f / 1,35 = %.2f kN", aw, in.fw, angleFactor, fwRd / 1000.0));
            checks.push_back(c);
        }
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

double weldCombinedResistance(double longitudinal, double transverse)
{
    return std::max(longitudinal + transverse, 0.85 * longitudinal + 1.5 * transverse);
}

IcrResult weldGroupIcr(const IcrInput &in)
{
    IcrResult result;
    struct Element {
        double x, y, dx, dy, length;
    };
    std::vector<Element> elements;
    double sumL = 0.0, cx = 0.0, cy = 0.0;
    constexpr int kDivisions = 40;
    for (const WeldSegment &s : in.segments) {
        const double length = std::hypot(s.x2 - s.x1, s.y2 - s.y1);
        if (length <= 0.0) {
            continue;
        }
        const double dx = (s.x2 - s.x1) / length, dy = (s.y2 - s.y1) / length, piece = length / kDivisions;
        for (int k = 0; k < kDivisions; ++k) {
            const double t = (k + 0.5) * piece;
            elements.push_back({s.x1 + dx * t, s.y1 + dy * t, dx, dy, piece});
            cx += (s.x1 + dx * t) * piece;
            cy += (s.y1 + dy * t) * piece;
            sumL += piece;
        }
    }
    if (elements.empty() || in.leg <= 0.0) {
        return result;
    }
    cx /= sumL;
    cy /= sumL;
    const double alpha = in.loadAngleDeg * kPi / 180.0;
    const double ux = std::cos(alpha), uy = std::sin(alpha);   // direcao da carga
    const double nx = -uy, ny = ux;                              // perpendicular: o CIR fica nesta reta, pelo centroide
    const double te = 0.707 * in.leg, unit = 0.6 * in.fw / in.gammaW2;
    double extent = 0.0;
    for (const Element &e : elements) {
        extent = std::max(extent, std::hypot(e.x - cx, e.y - cy));
    }
    extent = std::max(extent, 1.0);

    // resultante dos filetes para um CIR em (px, py)
    struct Balance {
        double along {0.0};    ///< soma das componentes das forcas dos filetes na direcao da carga
        double moment {0.0};   ///< soma de f_i r_i
        double arm {0.0};      ///< braco da carga em relacao ao CIR
    };
    auto balance = [&](double px, double py) {
        Balance b;
        const double cross = (in.loadX - px) * uy - (in.loadY - py) * ux;
        b.arm = std::abs(cross);
        const double sign = cross >= 0.0 ? 1.0 : -1.0;
        // deformacao do elemento mais critico: menor relacao Delta_u,i / r_i
        double ratioMin = 1e300;
        std::vector<double> radius(elements.size()), theta(elements.size());
        std::vector<std::array<double, 2>> direction(elements.size());
        for (std::size_t i = 0; i < elements.size(); ++i) {
            const Element &e = elements[i];
            const double rx = e.x - px, ry = e.y - py;
            const double r = std::max(std::hypot(rx, ry), 1e-9);
            radius[i] = r;
            direction[i] = {sign * -ry / r, sign * rx / r};   // forca do filete: perpendicular ao raio, no sentido do momento da carga
            const double cosT = std::clamp(std::abs(direction[i][0] * e.dx + direction[i][1] * e.dy), 0.0, 1.0);
            theta[i] = std::acos(cosT) * 180.0 / kPi;
            const double deltaU = std::min(1.087 * std::pow(theta[i] + 6.0, -0.65), 0.17) * in.leg;
            ratioMin = std::min(ratioMin, deltaU / r);
        }
        for (std::size_t i = 0; i < elements.size(); ++i) {
            const double deltaM = 0.209 * std::pow(theta[i] + 2.0, -0.32) * in.leg;
            const double p = ratioMin * radius[i] / deltaM;
            const double f = std::pow(std::max(0.0, p * (1.9 - 0.9 * p)), 0.3);
            const double sinT = std::sin(theta[i] * kPi / 180.0);
            const double force = unit * te * elements[i].length * (1.0 + 0.5 * std::pow(sinT, 1.5)) * f;
            b.along += force * (direction[i][0] * ux + direction[i][1] * uy);
            b.moment += force * radius[i];
        }
        return b;
    };
    // equilibrio: soma das componentes na direcao da carga = momento resistente / braco da carga
    auto residual = [&](double s) {
        const Balance b = balance(cx + s * nx, cy + s * ny);
        return b.arm > 1e-9 ? b.along - b.moment / b.arm : 1e300;
    };
    double best = 1e300, bestS = 0.0;
    double previousS = 0.0, previousG = 0.0;
    bool havePrevious = false;
    for (int k = -400; k <= 400; ++k) {
        if (k == 0) {
            havePrevious = false;
            continue;
        }
        const double s = (k < 0 ? -1.0 : 1.0) * 0.001 * extent * std::pow(1.025, std::abs(k));
        const double g = residual(s);
        if (g > 1e299) {
            havePrevious = false;
            continue;
        }
        if (havePrevious && ((g < 0.0) != (previousG < 0.0)) && (s > 0.0) == (previousS > 0.0)) {
            double lo = previousS, hi = s, glo = previousG;
            for (int it = 0; it < 80; ++it) {
                const double mid = 0.5 * (lo + hi);
                const double gm = residual(mid);
                if ((gm < 0.0) == (glo < 0.0)) {
                    lo = mid;
                    glo = gm;
                } else {
                    hi = mid;
                }
            }
            const double root = 0.5 * (lo + hi);
            const Balance b = balance(cx + root * nx, cy + root * ny);
            const double force = b.arm > 1e-9 ? b.moment / b.arm : 0.0;
            if (force > 0.0 && force < best) {
                best = force;
                bestS = root;
            }
        }
        previousS = s;
        previousG = g;
        havePrevious = true;
    }
    if (best < 1e299) {
        result.converged = true;
        result.forceRd = best;
        result.icrX = cx + bestS * nx;
        result.icrY = cy + bestS * ny;
    }
    return result;
}

std::vector<Check> checkPrying(const PryingInput &in)
{
    std::vector<Check> checks;
    if (in.tensionForce <= 0.0 || in.p <= 0.0 || in.fu <= 0.0) {
        return checks;
    }
    const double bEff = in.b - 0.5 * in.db;
    const double aEff = std::min(in.a, 1.25 * in.b);
    if (bEff <= 0.0) {
        Check bad = minimumCheck("Efeito de alavanca - geometria", "6.3.5.2", in.b, 0.5 * in.db + 1.0);
        bad.lines.push_back(fmt("b = %.1f mm <= 0,5 db = %.1f mm: parafuso encostado na alma/aba, aumente a distancia b", in.b, 0.5 * in.db));
        checks.push_back(bad);
        return checks;
    }
    const double delta = std::clamp(1.0 - in.holeWidth / in.p, 0.05, 1.0);
    const double beta = ((aEff + 0.5 * in.db) / bEff) * (in.boltResistance / in.tensionForce - 1.0);
    // placa rigida (6.3.5.3): efeito alavanca nulo
    const double tRigid = std::sqrt(4.0 * bEff * in.tensionForce * in.gammaA1 / (in.p * in.fu));
    if (beta < 0.0) {
        // parafuso com resistencia abaixo da tracao aplicada: a norma exige alterar o parafuso
        Check c = ratioCheck("Efeito de alavanca - parafuso", "6.3.5.2", in.tensionForce / 1000.0, in.boltResistance / 1000.0);
        c.lines.push_back(fmt("beta < 0 (Ft,Rd = %.2f kN < Ft,0,Sd = %.2f kN): o parafuso deve ser alterado", in.boltResistance / 1000.0, in.tensionForce / 1000.0));
        checks.push_back(c);
        return checks;
    }
    double alpha = 1.0;
    if (beta < 1.0) {
        alpha = std::min(1.0, (1.0 / delta) * (beta / (1.0 - beta)));
    }
    const double tFlexible = std::sqrt(4.0 * bEff * in.tensionForce * in.gammaA1 / (in.p * in.fu * (1.0 + delta * alpha)));
    Check c = minimumCheck("Efeito de alavanca - espessura da placa (flexivel)", "6.3.5.4", in.thickness, tFlexible);
    c.lines.push_back(fmt("b - 0,5 db = %.1f mm; a = min(a ; 1,25 b) = %.1f mm; delta = 1 - df/p = %.3f", bEff, aEff, delta));
    c.lines.push_back(fmt("beta = [(a + 0,5 db)/(b - 0,5 db)] (Ft,Rd/Ft,0,Sd - 1) = %.3f; alfa = %.3f", beta, alpha));
    c.lines.push_back(fmt("t >= sqrt[4 (b - 0,5 db) Ft,0,Sd ga1 / (p fu (1 + delta alfa))] = %.2f mm (adotada %.2f mm)", tFlexible, in.thickness));
    c.lines.push_back(fmt("Se a placa fosse rigida (sem alavanca), 6.3.5.3 exigiria t >= %.2f mm", tRigid));
    checks.push_back(c);
    return checks;
}

} // namespace Structura::Design::Connections
