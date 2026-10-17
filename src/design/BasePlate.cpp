#include "design/BasePlate.h"

#include "design/ConnectionHelpers.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design::Connections {

using namespace detail;

const std::vector<AnchorDetail> &anchorTable()
{
    // ABNT NBR 8800:2024, Tabela 18 (chumbadores ASTM A36)
    static const std::vector<AnchorDetail> table = {
        {19, "3/4", 40, 80, 120, 450, 150, 175, 50, 33, 6.3, 50, 40, 20, 900},
        {22, "7/8", 45, 90, 140, 465, 200, 225, 50, 40, 6.3, 65, 50, 20, 900},
        {25, "1", 50, 100, 160, 465, 200, 225, 50, 40, 8.0, 75, 50, 25, 900},
        {32, "1 1/4", 65, 130, 190, 525, 225, 250, 60, 50, 9.5, 75, 60, 25, 1100},
        {38, "1 1/2", 80, 160, 230, 610, 250, 275, 70, 60, 9.5, 90, 70, 25, 1300},
        {44, "1 3/4", 90, 180, 270, 700, 300, 325, 70, 70, 12.5, 100, 80, 25, 1600},
        {50, "2", 100, 200, 300, 850, 350, 375, 100, 80, 16.0, 125, 90, 30, 1800},
    };
    return table;
}

const AnchorDetail &anchorFor(double db)
{
    const auto &table = anchorTable();
    for (const auto &row : table) {
        if (row.db >= db - 1e-6) {
            return row;
        }
    }
    return table.back();
}

namespace {

Check lengthRatio(const std::string &name, const std::string &clause, double demandMm, double resistanceMm)
{
    Check c;
    c.name = name;
    c.clause = clause;
    c.demand = demandMm;
    c.resistance = resistanceMm;
    c.unit = "mm";
    return c;
}

double columnArea(const BasePlateInput &in)
{
    return 2.0 * in.bf * in.tf + (in.d - 2.0 * in.tf) * in.tw;
}

} // namespace

BasePlateGeometry basePlateGeometry(const BasePlateInput &in)
{
    BasePlateGeometry g;
    const AnchorDetail &t = anchorFor(in.anchorDiameter);
    const int nb = std::max(4, std::min(8, in.anchors));
    g.lx = in.d + 4.0 * t.a1;
    g.ly = (0.5 * nb - 1.0) * t.a2 + 2.0 * t.a1;
    g.a = g.lx / 2.0 - t.a1;
    g.m = (g.lx - 0.95 * in.d) / 2.0;
    g.n = (g.ly - 0.80 * in.bf) / 2.0;
    g.n0 = std::sqrt(in.d * in.bf) / 4.0;
    g.blockN = in.blockN > 0.0 ? in.blockN : std::max({t.blockMin, g.lx + 2.0 * t.en, g.lx + 2.0 * (t.a3 - t.a1)});
    g.blockB = in.blockB > 0.0 ? in.blockB : std::max(g.ly + 2.0 * t.en, g.ly + 2.0 * (t.a3 - t.a1));
    const double ratio = std::sqrt(std::max(1.0, g.blockN * g.blockB / (g.lx * g.ly)));
    g.sigmaCRd = std::min(0.85 * in.fck / in.gammaC * ratio, 1.7 * in.fck / in.gammaC);
    return g;
}

std::vector<Check> checkBasePlate(const BasePlateInput &in)
{
    std::vector<Check> checks;
    BasePlateGeometry g = basePlateGeometry(in);
    const AnchorDetail &t = anchorFor(in.anchorDiameter);
    const int nb = std::max(4, std::min(8, in.anchors));
    const double fyd = in.plate.fy / in.gammaA1;
    const double nc = std::max(0.0, -in.axial);       // compressao
    const double nt = std::max(0.0, in.axial);        // tracao
    const double M = std::abs(in.moment);
    const double V = std::abs(in.shear);
    const double sRd = g.sigmaCRd;
    const double tauRd = std::min(0.2 * in.fck / in.gammaC, 4.0);
    const double areaPlate = g.lx * g.ly;
    const double kX = 4.0 * in.d * in.bf / std::pow(in.d + in.bf, 2);

    // --- caso (Figuras 22 e 23) ---------------------------------------------------------------------------------
    double lc = g.lx;
    double sigmaSd = 0.0;
    double ftSd = 0.0;
    double lmax = 0.0;
    double vRd = 0.0;
    double tpMin = 0.0;
    bool equilibriumOk = true;
    double equilibriumRatio = 0.0;
    double x = 0.0;
    double lambda = 0.0;
    double reaction = 0.0;
    std::vector<std::string> notes;
    const double e = nc > 0.0 ? M / nc : (nt > 0.0 ? M / nt : 1e12);

    auto lmaxFor = [&](double lcValue) {
        if (lcValue >= g.m) {
            return std::max({g.m, g.n, lambda * g.n0});
        }
        const double p = std::sqrt(std::max(0.0, lcValue * (2.0 * g.m - lcValue)));
        return std::max(p, g.n);
    };

    if (nc > 0.0) {
        x = kX * nc / (areaPlate * sRd);
        lambda = x <= 1.0 ? std::min(1.0, 2.0 * std::sqrt(x) / (1.0 + std::sqrt(1.0 - x))) : 1.0;
        const double limit = 0.5 * (g.lx - nc / (sRd * g.ly));
        if (M <= 1e-6 * std::max(1.0, nc)) {
            g.caseName = "C1";
            sigmaSd = nc / areaPlate;
            lc = g.lx;
            lmax = std::max({g.m, g.n, lambda * g.n0});
            tpMin = lmax * std::sqrt(2.0 * sigmaSd / fyd);
            vRd = std::min(in.frictionCoefficient * sigmaSd * g.lx * g.ly / in.gammaA2, tauRd * areaPlate);
            reaction = nc;
        } else if (e <= limit) {
            g.caseName = "C2";
            lc = g.lx - 2.0 * e;
            sigmaSd = nc / (lc * g.ly);
            lmax = lmaxFor(lc);
            tpMin = lmax * std::sqrt(2.0 * sigmaSd / fyd);
            vRd = std::min(in.frictionCoefficient * sigmaSd * lc * g.ly / in.gammaA2, tauRd * areaPlate);
            reaction = nc;
        } else {
            g.caseName = "C3";
            const double half = g.lx / 2.0 + g.a;
            const double rhs = 2.0 * (M + nc * g.a) / (sRd * g.ly);   // 2 N (e + a)/(sigma ly)
            equilibriumRatio = rhs / (half * half);
            equilibriumOk = equilibriumRatio <= 1.0;
            lc = half - std::sqrt(std::max(0.0, half * half - rhs));
            reaction = sRd * lc * g.ly;
            ftSd = std::max(0.0, 2.0 * (reaction - nc) / nb);
            sigmaSd = sRd;
            lmax = lmaxFor(lc);
            const double tp1 = lmax * std::sqrt(2.0 * sRd / fyd);
            const double tp2 = std::sqrt(std::max(0.0, 2.0 * nb * ftSd * (g.m - t.a1) / (g.ly * fyd)));
            tpMin = std::max(tp1, tp2);
            vRd = std::min(in.frictionCoefficient * sRd * lc * g.ly / in.gammaA2, tauRd * areaPlate);
        }
    } else {
        // tracao (ou momento puro)
        if (M <= 1e-6 * std::max(1.0, nt) && nt > 0.0) {
            g.caseName = "T1";
            ftSd = nt / nb;
            tpMin = std::sqrt(std::max(0.0, 2.0 * nb * ftSd * (g.m - t.a1) / (g.ly * fyd)));
        } else if (nt > 0.0 && e <= g.a) {
            g.caseName = "T2";
            ftSd = nt / nb + M / (g.a * nb);
            tpMin = std::sqrt(std::max(0.0, 2.0 * nb * ftSd * (g.m - t.a1) / (g.ly * fyd)));
        } else {
            g.caseName = "T3";
            const double half = g.lx / 2.0 + g.a;
            const double rhs = 2.0 * std::max(0.0, M - nt * g.a) / (sRd * g.ly);   // 2 N (e - a)/(sigma ly)
            equilibriumRatio = rhs / (half * half);
            equilibriumOk = equilibriumRatio <= 1.0;
            lc = half - std::sqrt(std::max(0.0, half * half - rhs));
            reaction = sRd * lc * g.ly;
            ftSd = 2.0 * (reaction + nt) / nb;
            x = kX * lc / g.lx;
            lambda = x <= 1.0 ? std::min(1.0, 2.0 * std::sqrt(x) / (1.0 + std::sqrt(1.0 - x))) : 1.0;
            lmax = lmaxFor(lc);
            const double tp1 = lmax * std::sqrt(2.0 * sRd / fyd);
            const double tp2 = std::sqrt(std::max(0.0, 2.0 * nb * ftSd * (g.m - t.a1) / (g.ly * fyd)));
            tpMin = std::max(tp1, tp2);
            sigmaSd = sRd;
            vRd = std::min(in.frictionCoefficient * sRd * lc * g.ly / in.gammaA2, tauRd * areaPlate);
        }
    }
    const double dbMin = ftSd > 0.0 ? std::sqrt(4.0 * ftSd / (0.75 * kPi * in.anchorFub / in.gammaA2)) : 0.0;

    // --- verificacoes -------------------------------------------------------------------------------------------
    {
        Check c = dimensionless("Caso de calculo", "6.7.1.4", 0.0);
        c.unit = "-";
        c.lines.push_back(fmtn("e = M/N = %.1f mm; lx = d + 4 a1 = %.0f mm; ly = (0,5 nb - 1) a2 + 2 a1 = %.0f mm; a = %.0f mm", std::min(e, 1e9), g.lx, g.ly, g.a));
        c.lines.push_back("Caso " + g.caseName + (g.caseName[0] == 'C' ? " (forca axial de compressao, Figura 22)" : " (forca axial de tracao, Figura 23)"));
        c.lines.push_back(fmtn("sigma_c,Rd = min(0,85 fck/gc sqrt(A2/A1) ; 1,7 fck/gc) = %.2f MPa (A2 = %.0f x %.0f mm do bloco)", sRd, g.blockN, g.blockB));
        c.lines.push_back(fmtn("m = (lx - 0,95 d)/2 = %.1f; n = (ly - 0,8 bf)/2 = %.1f; n0 = raiz(d bf)/4 = %.1f mm", g.m, g.n, g.n0));
        checks.push_back(c);
    }
    {
        Check c = lengthRatio("Placa de base: espessura minima (tp,min)", "6.7.2.2", tpMin, in.plateThickness);
        c.lines.push_back(fmtn("lc = %.1f mm; lambda = %.3f; lmax = %.1f mm; fy/ga1 = %.1f MPa", lc, lambda, lmax, fyd));
        c.lines.push_back(fmtn("tp,min = %.2f mm <= tp = %.1f mm", tpMin, in.plateThickness));
        checks.push_back(c);
    }
    if (nc > 0.0) {
        Check c = ratioMPa("Pressao de contato do concreto (sigma_c,Sd <= sigma_c,Rd)", "6.7.1.5-d / 6.6.5", sigmaSd, sRd);
        c.lines.push_back(fmtn("sigma_c,Sd = %.3f MPa; sigma_c,Rd = %.3f MPa", sigmaSd, sRd));
        checks.push_back(c);
        Check xc = dimensionless("Condicao X <= 1", "6.7.2.1", x);
        xc.lines.push_back(fmtn("X = [4 d bf/(d + bf)^2] N/(lx ly sigma_c,Rd) = %.3f", x));
        checks.push_back(xc);
    }
    if (g.caseName == "C3" || g.caseName == "T3") {
        Check c = dimensionless("Equilibrio: (lx/2 + a)^2 >= 2 N (e +- a)/(sigma_c,Rd ly)", "6.7.2.2", equilibriumRatio);
        c.lines.push_back(fmtn("Relacao entre os dois lados = %.3f (deve ser <= 1; caso contrario, alterar a ligacao)", equilibriumRatio));
        checks.push_back(c);
        (void)equilibriumOk;
    }
    if (ftSd > 0.0) {
        Check c = lengthRatio("Chumbador: diametro minimo (db,min)", "6.7.2.2", dbMin, in.anchorDiameter);
        c.lines.push_back(fmtn("Ft,Sd = %.2f kN por chumbador; db,min = raiz(4 Ft,Sd / (0,75 pi fub/ga2)) = %.1f mm <= db = %.1f mm", ftSd / 1000.0, dbMin, in.anchorDiameter));
        checks.push_back(c);
    }
    {
        Check c = ratioN("Cisalhamento por atrito na placa de base", "6.7.2.2 / 6.7.2.3", V, vRd);
        c.lines.push_back(fmtn("V,Rd = mu sigma lc ly/ga2 = %.2f x %.3f x %.0f x %.0f / %.2f = %.2f kN (limite tau,c,Rd lx ly com tau,c,Rd = %.2f MPa)", in.frictionCoefficient, sigmaSd, lc,
                               g.ly, in.gammaA2, vRd / 1000.0, tauRd));
        if (vRd <= 0.0 && V > 0.0) {
            c.lines.push_back("Sem pressao de contato: o cortante exige dispositivo especial (placa de cisalhamento ou arruelas especiais soldadas, 6.7.2.3 a 6.7.2.5).");
        } else if (V > vRd) {
            c.lines.push_back("V,Sd > V,Rd: colocar placa de cisalhamento ou arruelas especiais com furos-padrao soldadas a placa de base (6.7.2.3 a 6.7.2.5).");
        }
        checks.push_back(c);
    }
    {
        Check c = rangeMin("Largura minima da placa (ly >= bf + 25 mm)", "6.7.2.1", g.ly, in.bf + 25.0, "mm");
        c.lines.push_back(fmtn("ly = %.0f mm >= bf + 25 = %.0f mm", g.ly, in.bf + 25.0));
        checks.push_back(c);
        Check k = rangeMin("Resistencia do concreto (fck >= fck,min)", "Tab. 18", in.fck, t.fckMin, "MPa");
        k.lines.push_back(fmtn("fck = %.0f MPa >= fck,min = %.0f MPa", in.fck, t.fckMin));
        checks.push_back(k);
        const double minN = std::max({t.blockMin, g.lx + 2.0 * t.en, g.lx + 2.0 * (t.a3 - t.a1)});
        const double minB = std::max(g.ly + 2.0 * t.en, g.ly + 2.0 * (t.a3 - t.a1));
        Check bn = rangeMin("Bloco de concreto: comprimento Nb", "Tab. 18 (nota e)", g.blockN, minN, "mm");
        bn.lines.push_back(fmtn("Nb = %.0f mm >= max(Nb,min = %.0f ; lx + 2 en = %.0f ; lx + 2 (a3 - a1) = %.0f) mm", g.blockN, t.blockMin, g.lx + 2.0 * t.en, g.lx + 2.0 * (t.a3 - t.a1)));
        checks.push_back(bn);
        Check bb = rangeMin("Bloco de concreto: largura Bb", "Tab. 18 (nota e)", g.blockB, minB, "mm");
        bb.lines.push_back(fmtn("Bb = %.0f mm >= max(ly + 2 en = %.0f ; ly + 2 (a3 - a1) = %.0f) mm", g.blockB, g.ly + 2.0 * t.en, g.ly + 2.0 * (t.a3 - t.a1)));
        checks.push_back(bb);
        Check n = rangeMin("Numero de chumbadores (4 a 8)", "6.7.2.1", nb, 4.0, "un");
        n.lines.push_back(fmtn("nb = %d", nb));
        checks.push_back(n);
    }
    // controle do bloco pela NBR 6118 (21.2.1): F_Rd = Ac0 fcd sqrt(Ac1/Ac0) <= 3,3 fcd Ac0
    if (nc > 0.0 || reaction > 0.0) {
        const double fcd = in.fck / in.gammaC;
        const double ratio = std::sqrt(std::max(1.0, g.blockN * g.blockB / areaPlate));
        const double sigma6118 = std::min(fcd * ratio, 3.3 * fcd);
        const double actual = reaction / std::max(1.0, lc * g.ly);
        Check c = ratioMPa("Bloco de concreto: pressao em area reduzida (NBR 6118)", "NBR 6118:2023, 21.2.1", actual, sigma6118);
        c.lines.push_back(fmtn("F,Rd = Ac0 fcd sqrt(Ac1/Ac0) <= 3,3 fcd Ac0: fcd = %.2f MPa; sqrt(Ac1/Ac0) = %.2f -> sigma = %.2f MPa; atuante = %.2f MPa", fcd, ratio, sigma6118, actual));
        c.lines.push_back("Armadura de fretagem e de fendilhamento do bloco: dimensionar conforme a ABNT NBR 6118 (Tab. 18 da NBR 8800 fornece o minimo).");
        checks.push_back(c);
    }
    // solda pilar-placa
    {
        const double fFlange = M / std::max(1.0, in.d - in.tf) + std::abs(in.axial) * (in.bf * in.tf) / columnArea(in);
        WeldInput w;
        w.leg = in.weldLeg;
        w.length = 4.0 * in.bf - 2.0 * in.tw;
        w.fw = in.weldFw;
        w.angleDeg = 90.0;
        w.plateThickness = std::min(in.tf, in.plateThickness);
        w.plateFy = std::min(in.column.fy, in.plate.fy);
        w.plateFu = std::min(in.column.fu, in.plate.fu);
        w.force = fFlange;
        w.gammaW2 = in.gammaW2;
        w.gammaA1 = in.gammaA1;
        w.gammaA2 = in.gammaA2;
        appendChecks(checks, checkFilletWeld(w), "Metal", " (mesa-placa)");
        WeldInput wv = w;
        wv.length = 2.0 * (in.d - 2.0 * in.tf);
        wv.angleDeg = 0.0;
        wv.plateThickness = std::min(in.tw, in.plateThickness);
        wv.force = V;
        appendChecks(checks, checkFilletWeld(wv), "Metal", " (alma-placa, cortante)");
        appendChecks(checks, checkFilletWeld(w), "Perna", " (solda pilar-placa)");
        appendChecks(checks, checkFilletWeld(w), "Comprimento", " (solda pilar-placa)");
    }
    return checks;
}

std::vector<Part> partsBasePlate(const BasePlateInput &in)
{
    std::vector<Part> parts;
    const BasePlateGeometry g = basePlateGeometry(in);
    const AnchorDetail &t = anchorFor(in.anchorDiameter);
    const int nb = std::max(4, std::min(8, in.anchors));
    Part plate;
    plate.description = fmtn("Placa de base %.1f x %.0f x %.0f mm", in.plateThickness, g.ly, g.lx);
    plate.quantity = 1;
    plate.massEach = plateMass(g.lx, g.ly, in.plateThickness);
    parts.push_back(plate);
    Part anchor;
    anchor.description = fmtn("Chumbador d = %.0f mm, ASTM A36, comprimento %.0f mm (+ 2 porcas)", in.anchorDiameter, t.h1 + 100.0);
    anchor.quantity = nb;
    anchor.massEach = 0.25 * kPi * in.anchorDiameter * in.anchorDiameter * (t.h1 + 100.0) / 1e9 * kSteelDensity * 1.15;
    parts.push_back(anchor);
    Part washer;
    washer.description = fmtn("Arruela especial %.1f x %.0f x %.0f mm", t.washerThickness, t.washerSize, t.washerSize);
    washer.quantity = nb;
    washer.massEach = plateMass(t.washerSize, t.washerSize, t.washerThickness);
    parts.push_back(washer);
    Part weld;
    weld.description = fmtn("Filete de solda %.0f mm pilar-placa", in.weldLeg);
    weld.quantity = 1;
    weld.length = 4.0 * in.bf - 2.0 * in.tw + 4.0 * (in.d - 2.0 * in.tf);
    weld.unit = "mm";
    weld.massEach = 0.5 * in.weldLeg * in.weldLeg * weld.length / 1e9 * kSteelDensity;
    parts.push_back(weld);
    return parts;
}

BasePlateProposal proposeBasePlate(BasePlateInput base)
{
    BasePlateProposal best;
    double bestCost = 1e300;
    static const double thicknesses[] = {16.0, 19.0, 22.0, 25.0, 32.0, 38.0, 45.0, 50.0, 63.0, 75.0};
    static const double legs[] = {5.0, 6.0, 8.0, 10.0, 12.0};
    for (const auto &row : anchorTable()) {
        for (int nb = 4; nb <= 8; ++nb) {
            for (const double tp : thicknesses) {
                for (const double leg : legs) {
                    BasePlateInput in = base;
                    in.anchorDiameter = row.db;
                    in.anchors = nb;
                    in.plateThickness = tp;
                    in.weldLeg = leg;
                    if (in.fck < row.fckMin) {
                        continue;
                    }
                    const auto checks = checkBasePlate(in);
                    const double worst = worstRatio(checks);
                    if (worst > 1.0 + 1e-9) {
                        continue;
                    }
                    double cost = 0.0;
                    for (const auto &p : partsBasePlate(in)) {
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
    return best;
}

} // namespace Structura::Design::Connections
