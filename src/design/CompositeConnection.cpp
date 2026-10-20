#include "design/CompositeConnection.h"

#include "design/ConnectionHelpers.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Structura::Design::Connections {

using detail::fmtn;
using detail::kPi;
using detail::ratioN;
using detail::ratioNmm;

namespace {

double sq(double x) { return x * x; }

/// Valores das Tabelas O.1 a O.3 (mrad): para cada betaVm e fy (350 e 250 MPa), cargas DL/1CL e 2CL, em funcao de L/dt.
struct RotationTable {
    double beta;
    std::vector<double> lOverD;
    std::vector<double> fy350Dl, fy350Tcl, fy250Dl, fy250Tcl;
};

const std::vector<RotationTable> &rotationTables()
{
    static const std::vector<RotationTable> tables = {
        {0.85, {15, 20, 25, 30}, {15, 23, 29, 34}, {22, 32, 40, 48}, {14, 18, 21, 25}, {19, 24, 29, 33}},
        {0.90, {15, 20, 25}, {22, 35, 43}, {32, 48, 60}, {21, 27, 32}, {28, 36, 43}},
        {0.95, {15, 20, 25, 30}, {29, 46, 57, 67}, {43, 64, 80, 95}, {28, 35, 42, 50}, {38, 48, 57, 67}},
    };
    return tables;
}

bool interpolate(const std::vector<double> &x, const std::vector<double> &y, double at, double &out)
{
    if (x.size() < 2 || at < x.front() - 1e-9 || at > x.back() + 1e-9) {
        return false;
    }
    for (std::size_t i = 0; i + 1 < x.size(); ++i) {
        if (at <= x[i + 1] + 1e-9) {
            const double t = (at - x[i]) / (x[i + 1] - x[i]);
            out = y[i] + t * (y[i + 1] - y[i]);
            return true;
        }
    }
    return false;
}

bool tableValue(const RotationTable &t, double lOverD, double fy, int loading, double &out)
{
    double v350, v250;
    if (!interpolate(t.lOverD, loading == 0 ? t.fy350Dl : t.fy350Tcl, lOverD, v350) || !interpolate(t.lOverD, loading == 0 ? t.fy250Dl : t.fy250Tcl, lOverD, v250)) {
        return false;
    }
    if (fy < 250.0 - 1e-9 || fy > 350.0 + 1e-9) {
        return false;
    }
    out = v250 + (fy - 250.0) / 100.0 * (v350 - v250);
    return true;
}

/// Rigidez de um conector na regiao de momento negativo (O.2.4.1), N/mm; 0 se a norma nao tabela o caso.
double connectorStiffness(const CompositeConnectionInput &in, std::string &basis)
{
    if (in.krOverride > 0.0) {
        basis = "informada pelo usuario";
        return in.krOverride * 1000.0;
    }
    const double dd = in.studs.diameter;
    if (in.slabKind == Composite::SlabKind::Solid) {
        if (std::abs(dd - 22.0) < 0.6) {
            basis = "120 kN/mm: d = 22 mm em laje macica";
            return 120000.0;
        }
        if (std::abs(dd - 19.0) < 0.6) {
            basis = "100 kN/mm: d = 19 mm em laje macica";
            return 100000.0;
        }
    } else if (in.slabKind == Composite::SlabKind::DeckParallel) {
        if (std::abs(dd - 19.0) < 0.6) {
            basis = "100 kN/mm: d = 19 mm em laje mista com nervuras paralelas";
            return 100000.0;
        }
    } else if (std::abs(dd - 19.0) < 0.6) {
        if (in.studs.perRib <= 1) {
            basis = "70 kN/mm: d = 19 mm, um conector por nervura perpendicular";
            return 70000.0;
        }
        basis = "50 kN/mm: d = 19 mm, dois conectores por nervura perpendicular";
        return 50000.0;
    }
    basis = "nao tabelado";
    return 0.0;
}

} // namespace

bool requiredRotation(double lOverD, double fy, double betaVm, int loading, double &mrad)
{
    const auto &tables = rotationTables();
    if (betaVm < 0.85 - 1e-9 || betaVm > 0.95 + 1e-9) {
        return false;
    }
    // L/dt acima de 25: a Tabela O.2 nao tem a coluna; interpola-se entre as Tabelas O.1 e O.3 (O.4, item 3)
    const bool useMiddle = lOverD <= 25.0 + 1e-9;
    std::vector<const RotationTable *> use;
    use.push_back(&tables[0]);
    if (useMiddle) {
        use.push_back(&tables[1]);
    }
    use.push_back(&tables[2]);
    for (std::size_t i = 0; i + 1 < use.size(); ++i) {
        if (betaVm <= use[i + 1]->beta + 1e-9) {
            double a, b;
            if (!tableValue(*use[i], lOverD, fy, loading, a) || !tableValue(*use[i + 1], lOverD, fy, loading, b)) {
                return false;
            }
            mrad = a + (betaVm - use[i]->beta) / (use[i + 1]->beta - use[i]->beta) * (b - a);
            return true;
        }
    }
    return false;
}

double positiveMomentResistance(const CompositeConnectionInput &in, std::string *memo)
{
    Composite::BeamInput beam;
    beam.steel.d = in.d;
    beam.steel.bf = in.bf;
    beam.steel.tf = in.tf;
    beam.steel.tw = in.tw;
    beam.steel.area = in.area;
    beam.steel.inertia = in.inertia;
    beam.steel.fy = in.fy;
    beam.steel.fu = in.fu;
    beam.steel.E = in.steelE;
    beam.steel.gammaA1 = in.gammaA1;
    beam.concrete = in.concrete;
    beam.slab.kind = in.slabKind;
    beam.slab.tc = in.tc;
    beam.slab.hF = in.hF;
    beam.slab.bF = in.ribWidth;
    beam.studs = in.studs;
    beam.studs.spacing = in.positiveSpacing;
    // regiao de momento positivo: 7/10 do vao se ha vao adjacente dos dois lados (interno), 4/5 caso contrario (extremo) - L.2.2.2-a
    const bool internal = in.leftSpan > 0.0 && in.rightSpan > 0.0;
    beam.span = (internal ? 0.7 : 0.8) * in.span;
    beam.leftSpacing = in.leftSpacing;
    beam.rightSpacing = in.rightSpacing;
    beam.shored = in.shored;
    beam.betaVm = in.betaVm;
    const auto result = Composite::checkBeam(beam, {});
    if (memo) {
        *memo = fmtn("M+Rd = %.1f kN.m (L.2.3.1): b_ef = %.0f mm (distancia entre momentos nulos = %.2f L = %.0f mm), interacao eta = %.2f (%d conectores por meio vao, QRd = %.1f kN), betaVm = %.2f",
                     result.mRd / 1e6, result.bEff, internal ? 0.7 : 0.8, beam.span, result.interaction, result.connectorsHalfSpan, result.qRd / 1e3, in.betaVm);
    }
    return result.mRd;
}

CompositeConnectionResult checkCompositeConnection(const CompositeConnectionInput &in)
{
    CompositeConnectionResult r;
    auto &checks = r.checks;
    const double E = in.steelE;
    const double fyd = in.fy / in.gammaA1;
    const double hw = in.d - 2.0 * in.tf;
    const double afi = in.bf * in.tf;
    const double aSteel = in.area > 0.0 ? in.area : 2.0 * afi + hw * in.tw;
    const double ia = in.inertia > 0.0 ? in.inertia : (in.bf * std::pow(in.d, 3.0) - (in.bf - in.tw) * std::pow(hw, 3.0)) / 12.0;
    const double msd = std::abs(in.moment);

    // ---- armadura (O.2.2, O.2.3) -------------------------------------------------------------------------------------------------------------
    const double as = in.bars * 0.25 * kPi * sq(in.barDiameter);
    const double fsd = in.fys / in.gammaS;
    const double fsRd = fsd * as;
    r.as = as;
    r.fsRd = fsRd;
    const double zeroMomentLength = (in.leftSpan + in.rightSpan) / 4.0;   // L.2.2.2-b: 1/4 da soma dos vaos adjacentes
    const double bLeft = in.leftSpacing > 0.0 ? std::min(zeroMomentLength / 8.0, in.leftSpacing / 2.0) : 0.0;
    const double bRight = in.rightSpacing > 0.0 ? std::min(zeroMomentLength / 8.0, in.rightSpacing / 2.0) : 0.0;
    const double bEff = bLeft + bRight;
    r.bEff = bEff;
    {
        Check c = detail::dimensionless("Largura efetiva da laje (regiao de momento negativo)", "NBR 8800:2024, L.2.2.1.1 e L.2.2.2-b", 0.0);
        c.unit = "mm";
        c.demand = 0.0;
        c.resistance = bEff;
        c.lines.push_back(fmtn("Distancia entre pontos de momento nulo = 1/4 (L1 + L2) = %.0f mm; cada lado: min(L/8 ; e/2) -> %.0f + %.0f = %.0f mm", zeroMomentLength, bLeft, bRight, bEff));
        checks.push_back(c);
    }
    {
        const double width = (in.bars - 1) * in.barSpacing;
        Check c = detail::rangeMax("Armadura longitudinal dentro da largura efetiva", "NBR 8800:2024, O.2.1 / O.2.2", width, bEff, "mm");
        c.lines.push_back(fmtn("%d barras de %.1f mm a cada %.0f mm ocupam %.0f mm; As = %.0f mm2", in.bars, in.barDiameter, in.barSpacing, width, as));
        checks.push_back(c);
        if (in.support == CompositeConnectionInput::Column) {
            Check lat = detail::rangeMax("Armadura a ate 2,5 bc da linha de centro do pilar (cada lado)", "NBR 8800:2024, O.2.2", width / 2.0, 2.5 * in.bc, "mm");
            lat.lines.push_back(fmtn("bc = %.0f mm; limite 2,5 bc = %.0f mm", in.bc, 2.5 * in.bc));
            checks.push_back(lat);
            Check tr = detail::minimumRule("Armadura transversal de cada lado do pilar (>= 50 % de As)", "NBR 8800:2024, O.2.2", in.transverseArea, 0.5 * as, "mm2");
            checks.push_back(tr);
        }
        checks.push_back(detail::minimumRule("Diametro minimo das barras (CA-50)", "NBR 8800:2024, O.2.3.3", in.barDiameter, 12.5));
        checks.push_back(detail::minimumRule("Primeiro conector - face do apoio (a)", "NBR 8800:2024, O.2.3.3", in.a, 100.0));
        checks.push_back(detail::minimumRule("Primeiro conector - centro do apoio (a + ha/2)", "NBR 8800:2024, O.2.3.3", in.a + in.ha / 2.0, 200.0));
    }

    // rigidez e capacidade de deformacao da armadura
    const double ks = as * in.rebarE / (in.ha / 2.0);   // O.2.3.1
    const double ecs = in.concrete.ecs();
    const double fctm = 0.3 * std::pow(in.concrete.fck, 2.0 / 3.0);   // L.5.3.1
    // coeficiente kc (L.5.2.2): y0 = CG da laje - CG da secao mista homogeneizada (concreto nao fissurado, sem armadura, sem longa duracao)
    const double alphaE = E / ecs;
    const double acH = bEff * in.tc / alphaE;
    const double zSlab = in.d / 2.0 + in.hF + in.tc / 2.0;   // CG da laje acima do CG do perfil
    const double yBar = acH * zSlab / (aSteel + acH);
    const double y0 = std::max(zSlab - yBar, 1.0);
    const double kc = std::min(1.0 / (1.0 + in.tc / (2.0 * y0)) + 0.3, 1.0);
    const double ac = bEff * in.tc;
    const double rho = ac > 0.0 ? as / ac : 0.0;
    const double dEpsSr = rho > 0.0 ? fctm * kc / (rho * in.rebarE) : 0.0;
    const double sigmaSr = rho > 0.0 ? fctm * kc / rho * (1.0 + rho * in.rebarE / ecs) : 0.0;
    const double epsSy = in.fys / in.rebarE;
    const double epsSmu = epsSy - 0.4 * dEpsSr + 0.8 * (1.0 - sigmaSr / in.fys) * (in.epsSu - epsSy);
    const double lRef = 200.0;
    const double dUs = lRef * epsSmu;
    {
        Check c = detail::dimensionless("Armadura: rigidez, forca resistente e capacidade de deformacao", "NBR 8800:2024, O.2.3", 0.0);
        c.lines.push_back(fmtn("ks = As Es/(ha/2) = %.0f x %.0f/%.0f = %.1f kN/mm; Fs,Rd = fsd As = %.1f x %.0f = %.1f kN", as, in.rebarE, in.ha / 2.0, ks / 1000.0, fsd, as, fsRd / 1000.0));
        c.lines.push_back(fmtn("fctm = 0,3 fck^(2/3) = %.2f MPa; Ecs = %.0f MPa; alfaE = %.2f; y0 = %.0f mm; kc = 1/(1 + tc/(2 y0)) + 0,3 = %.3f (<= 1,0)", fctm, ecs, alphaE, y0, kc));
        c.lines.push_back(fmtn("delta_s = As/Ac = %.5f (Ac = bef tc = %.0f mm2); D_eps_sr = fctm kc/(ds Es) = %.6f; sigma_sr = %.1f MPa", rho, ac, dEpsSr, sigmaSr));
        c.lines.push_back(fmtn("eps_smu = eps_sy - 0,4 D_eps_sr + 0,8 (1 - sigma_sr/fys)(eps_su - eps_sy) = %.5f; D_us = L eps_smu = %.0f x %.5f = %.2f mm", epsSmu, lRef, epsSmu, dUs));
        checks.push_back(c);
    }

    // ---- conectores na regiao de momento negativo (O.2.4) ------------------------------------------------------------------------
    Composite::SlabInput slab;
    slab.kind = in.slabKind;
    slab.tc = in.tc;
    slab.hF = in.hF;
    slab.bF = in.ribWidth;
    const auto stud = Composite::studResistance(in.studs, slab, in.concrete);
    const double qRd = stud.qRd;
    const double l1 = in.l1 > 0.0 ? in.l1 : 0.15 * in.span;
    const int rowsStuds = std::max(1, in.studs.rows);
    const int nConn = in.connectors > 0 ? in.connectors : rowsStuds * std::max(1, static_cast<int>(std::floor(l1 / std::max(in.connectorSpacing, 1.0))));
    const double fcsRd = nConn * qRd;
    r.fcsRd = fcsRd;
    std::string krBasis;
    const double kr = connectorStiffness(in, krBasis);
    if (kr <= 0.0) {
        Check c = detail::rangeMin("Rigidez do conector kr: caso tabelado em O.2.4.1", "NBR 8800:2024, O.2.4.1", 0.0, 1.0);
        c.lines.push_back("A norma so tabela kr para pinos de 22 mm (laje macica) e 19 mm; informe kr de ensaio no campo proprio para outros casos.");
        checks.push_back(c);
    }
    {
        Check c = ratioN("Conectores na regiao de momento negativo: Fcs,Rd = n QRd >= Fs,Rd", "NBR 8800:2024, O.2.4.2", fsRd, fcsRd);
        c.lines.push_back(fmtn("QRd (L.4.2.1.1) = %.2f kN (esmagamento %.2f ; ruptura %.2f kN; Rg = %.2f, Rp = %.2f); n = %d conectores entre o momento maximo e o nulo (L1 = %.0f mm)", qRd / 1000.0, stud.concreteCrushing / 1000.0, stud.studRupture / 1000.0, stud.rg, stud.rp, nConn, l1));
        c.lines.push_back(fmtn("Fcs,Rd = %d x %.2f = %.1f kN >= Fs,Rd = %.1f kN (os conectores devem ser mais resistentes que a armadura)", nConn, qRd / 1000.0, fcsRd / 1000.0, fsRd / 1000.0));
        checks.push_back(c);
    }
    double kcs = 0.0, sB = 0.0;
    if (kr > 0.0) {
        const double ds = in.d / 2.0 + in.y;
        const double xi = ia / (sq(ds) * as);
        const double nu = std::sqrt((xi + 1.0) * nConn * kr * l1 * sq(ds) / (in.steelE * ia));
        const double alpha = nu - (nu - 1.0) * (in.d + in.y) / (ds * (xi + 1.0));
        kcs = alpha > 0.0 ? nConn * kr / alpha : 0.0;
        const double qRk = in.studs.gammaCs * qRd;
        const double sA = 0.7 * qRk / kr;
        const double fsA = kcs * sA;
        const double fsB = as * in.fys;
        sB = fsA > 0.0 ? 2.0 * sA * fsB / fsA : 0.0;
        Check c = detail::dimensionless("Conectores: rigidez e capacidade de deformacao", "NBR 8800:2024, O.2.4.1 e O.2.4.3", 0.0);
        c.lines.push_back(fmtn("kr = %.0f kN/mm (%s); ds = d/2 + y = %.0f mm; xi = Ia/(ds2 As) = %.3f; nu = %.3f; alfa = nu - (nu - 1)(d + y)/(ds (xi + 1)) = %.3f", kr / 1000.0, krBasis.c_str(), ds, xi, nu, alpha));
        c.lines.push_back(fmtn("kcs = n kr/alfa = %.1f kN/mm; QRk = gcs QRd = %.2f kN; s(A) = 0,7 QRk/kr = %.3f mm; Fs(A) = kcs s(A) = %.1f kN; Fs(B) = As fys = %.1f kN -> s(B) = 2 s(A) Fs(B)/Fs(A) = %.3f mm", kcs / 1000.0, qRk / 1000.0, sA, fsA / 1000.0, fsB / 1000.0, sB));
        checks.push_back(c);
    }

    // ---- ligacao metalica da mesa inferior (O.2.5.2) -----------------------------------------------------------------------------
    double ki = std::numeric_limits<double>::infinity();
    double dUi = 0.0;
    double fiRd = 0.0;
    if (in.variant == CompositeConnectionInput::EndPlate) {
        fiRd = 1.25 * fyd * afi;
        Check c = ratioN("Mesa inferior (chapa de extremidade): Fi,Rd = 1,25 fyd Afi >= Fs,Rd", "NBR 8800:2024, O.2.5.2.2.2", fsRd, fiRd);
        c.lines.push_back(fmtn("Afi = bf tf = %.0f mm2; Fi,Rd = 1,25 x %.1f x %.0f = %.1f kN; rigidez infinita e deformacao nula (O.2.5.2.2.1 e .3)", afi, fyd, afi, fiRd / 1000.0));
        checks.push_back(c);
        if (in.endPlateCjp) {
            Check w = detail::dimensionless("Solda da mesa inferior a chapa: penetracao total", "NBR 8800:2024, O.2.5.2.2.1", 0.0);
            w.lines.push_back("Solda de penetracao total: atende O.2.5.2.2.1.");
            checks.push_back(w);
        } else {
            const double te = 0.707 * in.endPlateWeldLeg;
            const double weldRd = 2.0 * in.bf * 0.6 * te * in.weldFw / in.gammaW2;   // filete duplo na largura da mesa
            Check w = ratioN("Solda de filete duplo da mesa inferior: >= 1,2 x resistencia da mesa ao escoamento", "NBR 8800:2024, O.2.5.2.2.1", 1.2 * afi * fyd, weldRd);
            w.lines.push_back(fmtn("Rd,solda = 2 bf 0,6 te fw/gw2 = %.1f kN; exigido 1,2 Afi fyd = %.1f kN (rigidez infinita da ligacao)", weldRd / 1000.0, 1.2 * afi * fyd / 1000.0));
            checks.push_back(w);
        }
    } else {
        dUi = 3.0;
        const int nb = 2 * std::max(1, in.rowsL);
        BoltedInput bolt;
        bolt.grade = in.grade;
        bolt.db = in.db;
        bolt.threadInShearPlane = in.threadInShearPlane || !in.grade.highStrength;
        bolt.shearPlanes = 1;
        bolt.nx = std::max(1, in.rowsL);
        bolt.ny = 2;
        bolt.sx = in.boltSpacing;
        bolt.sy = in.boltGauge;
        bolt.endDistance = in.boltEnd;
        bolt.gammaA2 = in.gammaA2;
        BoltedInput onAngle = bolt, onFlange = bolt;
        onAngle.plateThickness = in.angleT;
        onAngle.plateFu = in.angleFu;
        onFlange.plateThickness = in.tf;
        onFlange.plateFu = in.fu;
        const double fv = boltShearResistance(bolt);
        const double fcAngle = boltBearingResistance(onAngle);
        const double fcFlange = boltBearingResistance(onFlange);
        const double fbRd = std::min({fv, fcAngle, fcFlange});
        const double al = in.angleLeg * in.angleT;
        const double fLyd = in.angleFy / in.gammaA1;
        fiRd = std::min({nb * fbRd, 1.25 * fyd * afi, fLyd * al});
        Check cb = ratioN("Parafusos da mesa inferior: nb Fb,Rd >= Fs,Rd", "NBR 8800:2024, O.2.5.2.3.2 / 6.3.3.2 / 6.3.3.3", fsRd, nb * fbRd);
        cb.lines.push_back(fmtn("Fv,Rd = %.2f kN (cisalhamento, 1 plano); Fc,Rd = %.2f kN (cantoneira) e %.2f kN (mesa do perfil); Fb,Rd = %.2f kN; nb = %d", fv / 1000.0, fcAngle / 1000.0, fcFlange / 1000.0, fbRd / 1000.0, nb));
        checks.push_back(cb);
        Check cf = ratioN("Mesa inferior do perfil ao esmagamento: 1,25 fyd Afi >= Fs,Rd", "NBR 8800:2024, O.2.5.2.3.2", fsRd, 1.25 * fyd * afi);
        checks.push_back(cf);
        Check ca = ratioN("Aba horizontal da cantoneira: fL,yd AL >= Fs,Rd", "NBR 8800:2024, O.2.5.2.3.2", fsRd, fLyd * al);
        ca.lines.push_back(fmtn("AL = %.0f x %.1f = %.0f mm2; fL,yd = %.1f MPa", in.angleLeg, in.angleT, al, fLyd));
        checks.push_back(ca);
        // rigidez inicial (O.2.5.2.3.1)
        const double dm = 16.0;
        const double ksH = in.rowsL > 1 ? std::min(in.boltSpacing / (4.0 * in.db) + 0.375, 1.25) : 1.25;
        const double kt1 = std::min(1.5 * in.angleT / dm, 2.5);
        const double kt2 = std::min(1.5 * in.tf / dm, 2.5);
        const double kp1 = 24.0 * ksH * kt1 * in.db * in.angleFu;
        const double kp2 = 24.0 * ksH * kt2 * in.db * in.fu;
        const double kb = 16.0 * in.grade.fub * sq(in.db) / dm;
        ki = in.rowsL / (1.0 / kp1 + 1.0 / kp2 + 1.0 / kb);
        Check ck = detail::dimensionless("Ligacao inferior parafusada: rigidez inicial e capacidade de deformacao", "NBR 8800:2024, O.2.5.2.3.1 e O.2.5.2.3.3", 0.0);
        ck.lines.push_back(fmtn("ks = S/(4 db) + 0,375 = %.3f (<= 1,25); kt1 = %.3f; kt2 = %.3f; kp1 = %.1f kN/mm; kp2 = %.1f kN/mm; kb = %.1f kN/mm", ksH, kt1, kt2, kp1 / 1000.0, kp2 / 1000.0, kb / 1000.0));
        ck.lines.push_back(fmtn("ki = n_l/(1/kp1 + 1/kp2 + 1/kb) = %d/(...) = %.1f kN/mm; D_ui = 3 mm", in.rowsL, ki / 1000.0));
        checks.push_back(ck);
    }
    r.fiRd = fiRd;
    checks.push_back(ratioN("Ligacao da mesa inferior: Fi,Rd >= Fs,Rd", "NBR 8800:2024, O.3.2-a", fsRd, fiRd));

    // O.4 e O.2.5.2.3.2: cantoneira so na mesa inferior transmite tambem o cortante
    if (in.variant == CompositeConnectionInput::BottomAngleOnly) {
        const double v = std::abs(in.shear);
        BoltedInput vb;
        vb.grade = in.grade;
        vb.db = in.db;
        vb.threadInShearPlane = in.threadInShearPlane || !in.grade.highStrength;
        vb.shearPlanes = 1;
        vb.nx = 1;
        vb.ny = std::max(1, in.verticalBolts);
        vb.sx = 0.0;
        vb.sy = in.boltGauge;
        vb.plateThickness = std::min(in.angleT, in.columnTf);
        vb.plateFu = std::min(in.angleFu, in.fu);
        vb.endDistance = in.boltEnd;
        vb.edgeDistance = in.boltEnd;
        vb.shearY = v;
        vb.gammaA2 = in.gammaA2;
        auto vc = checkBolted(vb);
        detail::appendChecks(checks, vc, "Cisalhamento", " - aba vertical da cantoneira (cortante, O.2.5.2.3.2)");
        detail::appendChecks(checks, vc, "Pressao de contato", " - aba vertical da cantoneira (cortante)");
        const double zAng = 0.25 * in.angleLeg * sq(in.angleT) * in.angleFy / in.gammaA1;   // Z fy/ga1 da aba (largura x espessura)
        const double mHeel = v * in.verticalLever;
        Check fl = ratioNmm("Cantoneira inferior: flexao da aba no canto (V x excentricidade)", "NBR 8800:2024, O.2.5.2.3.2 / 5.4.2", mHeel, zAng);
        fl.lines.push_back(fmtn("M = V e = %.1f kN x %.0f mm; Mrd = Z fy/ga1 com Z = b t2/4 = %.0f mm3", v / 1000.0, in.verticalLever, 0.25 * in.angleLeg * sq(in.angleT)));
        checks.push_back(fl);
        checks.push_back(ratioN("Cantoneira inferior: cisalhamento da aba", "NBR 8800:2024, 5.4.3", v, 0.6 * in.angleLeg * in.angleT * in.angleFy / in.gammaA1));
        // o efeito do cortante sobrepoe-se ao da compressao Fs: parafusos da aba horizontal com Fs e a aba vertical com V (verificados em separado)
    }

    // ---- conjunto: rigidez, resistencia e capacidade de rotacao (O.3) ---------------------------------------------------------------------
    const double lever = in.d + in.y;
    const double mRd = fsRd * lever;
    r.mRd = mRd;
    {
        Check c = ratioNmm("Momento resistente da ligacao mista: Msd <= M-Rd = fsd As (d + y)", "NBR 8800:2024, O.3.2", msd, mRd);
        c.lines.push_back(fmtn("M-Rd = %.1f x %.0f x (%.0f + %.0f) = %.1f kN.m; Msd = %.1f kN.m (Fs,Sd = Msd/(d + y) = %.1f kN)", fsd, as, in.d, in.y, mRd / 1e6, msd / 1e6, msd / lever / 1000.0));
        checks.push_back(c);
    }
    {
        // condicoes de O.3.2
        const double lamF = in.bf / (2.0 * in.tf);
        const double limF = 0.38 * std::sqrt(E / in.fy);
        checks.push_back(detail::rangeMax("Mesa inferior: b/t <= 0,38 raiz(E/fy)", "NBR 8800:2024, O.3.2-c", lamF, limF));
        const double compArea = (aSteel + fsRd / fyd) / 2.0;   // area comprimida do perfil na LNP plastica (armadura tracionada incluida)
        const double hcp = compArea > afi ? std::min((compArea - afi) / in.tw, hw) : 0.0;
        const double lamW = (2.0 * hcp - 2.0 * in.radius) / in.tw;
        const double limW = 3.76 * std::sqrt(E / in.fy);
        Check w = detail::rangeMax("Alma comprimida: (2 hcp - 2 r)/tw <= 3,76 raiz(E/fy)", "NBR 8800:2024, O.3.2-c", std::max(lamW, 0.0), limW);
        w.lines.push_back(fmtn("LNP plastica (momento negativo): area comprimida do perfil = (A + Fs,Rd/fyd)/2 = %.0f mm2 -> altura comprimida da alma hcp = %.1f mm", compArea, hcp));
        checks.push_back(w);
        if (in.welded) {
            const double rw = 2.0 * 0.6 * 0.707 * in.weldLeg * in.weldFw / in.gammaW2 * in.d;
            const double vweb = 0.6 * in.fy * in.d * in.tw / in.gammaA1;
            Check s = ratioN("Solda mesa-alma do perfil soldado (comprimento d) >= cisalhamento resistente da alma", "NBR 8800:2024, O.3.2-b", vweb, rw);
            s.lines.push_back(fmtn("Rd,solda = 2 x 0,6 te fw/gw2 x d = %.1f kN; Vrd,alma = 0,6 fy d tw/ga1 = %.1f kN", rw / 1000.0, vweb / 1000.0));
            checks.push_back(s);
        }
        Check n = detail::dimensionless("Forca axial na viga apoiada desprezivel", "NBR 8800:2024, O.3.2-e", 0.0);
        n.lines.push_back(fmtn("N = %.1f kN (a norma exige forca axial desprezivel; a ligacao nao considera a interacao M-N)", in.axial / 1000.0));
        checks.push_back(n);
        Check dist = detail::dimensionless("Flambagem por distorcao da viga mista junto a ligacao", "NBR 8800:2024, O.3.2-d", 0.0);
        dist.lines.push_back("A reducao do momento resistente por flambagem por distorcao deve ser verificada pelo projetista (L.3.2 e anexo de vigas mistas); nao ha procedimento em O.");
        checks.push_back(dist);
    }
    const double dUsB = dUs + dUi + sB;
    const double thetaU = dUsB > 0.0 ? dUsB / lever : 0.0;
    const double yLnp = dUsB > 0.0 ? lever * dUi / dUsB : 0.0;
    r.thetaU = thetaU;
    r.yLnp = yLnp;
    const double invSum = 1.0 / ks + (kcs > 0.0 ? 1.0 / kcs : 0.0) + (std::isinf(ki) ? 0.0 : 1.0 / ki);
    r.si = kcs > 0.0 ? sq(lever) / invSum : 0.0;
    {
        Check c = detail::dimensionless("Rigidez inicial e capacidade de rotacao da ligacao", "NBR 8800:2024, O.3.1 e O.3.3", 0.0);
        c.lines.push_back(fmtn("Si = (d + y)2/(1/ks + 1/kcs + 1/ki) = %.0f kN.m/rad (ks = %.1f; kcs = %.1f; ki = %s kN/mm)", r.si / 1e6, ks / 1000.0, kcs / 1000.0, std::isinf(ki) ? "inf" : fmtn("%.1f", ki / 1000.0).c_str()));
        c.lines.push_back(fmtn("theta_u = (D_us + D_ui + s(B))/(d + y) = (%.2f + %.2f + %.3f)/%.0f = %.1f mrad; yLNP = (d + y) D_ui/(D_us + D_ui + s(B)) = %.1f mm", dUs, dUi, sB, lever, thetaU * 1000.0, yLnp));
        checks.push_back(c);
    }
    // rotacao necessaria (O.4)
    {
        const double total = in.d + in.tc + in.hF;
        const double lOverD = in.span / total;
        double need = 0.0;
        if (!requiredRotation(lOverD, in.fy, in.betaVm, in.loading, need)) {
            Check c = detail::rangeMin("Rotacao necessaria: dentro da faixa das Tabelas O.1 a O.3", "NBR 8800:2024, O.4", 0.0, 1.0);
            c.lines.push_back(fmtn("L/dt = %.1f (15 a 30; 15 a 25 se 0,85 < betaVm < 0,95), fy = %.0f MPa (250 a 350), betaVm = %.2f (0,85 a 0,95): fora da faixa tabelada.", lOverD, in.fy, in.betaVm));
            checks.push_back(c);
        } else {
            const double requiredUsed = in.shored ? 0.70 * need : need;
            r.thetaRequired = requiredUsed / 1000.0;
            const double available = thetaU * 1000.0 * (in.shored ? 1.0 : 1.10);
            Check c = ratioN("Capacidade de rotacao >= rotacao necessaria", "NBR 8800:2024, O.3.3 e O.4", requiredUsed, available);
            c.unit = "mrad";
            c.demand = requiredUsed;
            c.resistance = available;
            c.lines.push_back(fmtn("L/dt = %.1f (L = %.0f mm; dt = d + tc + hF = %.0f mm); betaVm = %.2f; fy = %.0f MPa; %s -> theta_nec(tabela) = %.1f mrad%s", lOverD, in.span, total, in.betaVm, in.fy, in.loading == 0 ? "carga distribuida ou 1 concentrada" : "2 concentradas", need, in.shored ? "; construcao escorada: x 0,70" : ""));
            c.lines.push_back(fmtn("theta_u = %.1f mrad%s", thetaU * 1000.0, in.shored ? "" : " x 1,10 (construcao nao escorada)"));
            checks.push_back(c);
        }
        std::string memo;
        const bool automatic = in.positiveMoment <= 0.0;
        const double mPositive = automatic ? positiveMomentResistance(in, &memo) : in.positiveMoment;
        if (mPositive > 0.0) {
            Check c = detail::rangeMin("Momento resistente da ligacao >= 30 % de M+Rd da viga mista", "NBR 8800:2024, O.4", mRd, 0.30 * mPositive, "N.mm");
            c.lines.push_back(fmtn("M-Rd = %.1f kN.m; 0,30 M+Rd = %.1f kN.m (as Tabelas O.1 a O.3 so valem acima desse limite)", mRd / 1e6, 0.30 * mPositive / 1e6));
            c.lines.push_back(automatic ? memo : fmtn("M+Rd = %.1f kN.m informado", mPositive / 1e6));
            checks.push_back(c);
        } else {
            Check c = detail::rangeMin("M+Rd da viga mista nao calculado (laje ou largura efetiva nula)", "NBR 8800:2024, O.4", 0.0, 1.0);
            checks.push_back(c);
        }
    }

    // ---- ligacao da alma: so cortante (O.2.5.1) ------------------------------------------------------------------------------------------
    if (in.variant != CompositeConnectionInput::BottomAngleOnly) {
        const std::string label = in.variant == CompositeConnectionInput::EndPlate ? "Espessura da chapa de extremidade" : (in.variant == CompositeConnectionInput::AnglesWeb ? "Espessura das cantoneiras da alma" : "Espessura da chapa simples da alma");
        checks.push_back(detail::rangeMax(label + " <= 0,5 db", "NBR 8800:2024, O.2.5.1", in.webT, 0.5 * in.webDb, "mm"));
        checks.push_back(detail::rangeMax("Escoamento dos elementos da ligacao da alma <= 350 MPa", "NBR 8800:2024, O.2.5.1", in.webFy, 350.0, "MPa"));
        if (in.variant == CompositeConnectionInput::AnglesWeb) {
            const double bottom = in.webBottom;
            Check c = detail::minimumRule("Posicao da LNP (Figura O.6): yLNP - borda inferior das cantoneiras >= Lc/5", "NBR 8800:2024, O.2.5.1", yLnp - bottom, in.webLc / 5.0);
            c.lines.push_back(fmtn("yLNP = %.1f mm da face inferior do perfil; borda inferior das cantoneiras a %.0f mm; Lc/5 = %.0f mm", yLnp, bottom, in.webLc / 5.0));
            checks.push_back(c);
        }
        if (in.variant == CompositeConnectionInput::PlateWeb) {
            Check s = detail::dimensionless("Furos da chapa simples pouco alongados no eixo da viga", "NBR 8800:2024, O.2.5.1 / Tabela 14", in.webSlotted ? 0.0 : 2.0);
            s.lines.push_back(in.webSlotted ? "Furos pouco alongados: atende." : "Os furos devem ser pouco alongados na direcao do eixo da viga.");
            checks.push_back(s);
            checks.push_back(detail::minimumRule("Furo - extremidade da chapa >= 2 db (direcao do eixo)", "NBR 8800:2024, O.2.5.1", in.webEndDistance, 2.0 * in.webDb));
            Check pt = detail::dimensionless("Parafusos da chapa com pre-tensao (nao com protensao inicial 'snug')", "NBR 8800:2024, O.2.5.1", in.webPretensioned ? 0.0 : 2.0);
            pt.lines.push_back(in.webPretensioned ? "Montados em condicao de pre-tensao: atende." : "Devem ser montados em condicao de pre-tensao.");
            checks.push_back(pt);
            checks.push_back(detail::rangeMax("Menor espessura (chapa ou alma) <= 0,5 db", "NBR 8800:2024, O.2.5.1", std::min(in.webT, in.tw), 0.5 * in.webDb, "mm"));
            checks.push_back(detail::minimumRule("Filete da chapa simples ao apoio >= 5/8 da espessura da chapa", "NBR 8800:2024, O.2.5.1", in.webPlateWeldLeg, 0.625 * in.webT));
        }
    }

    // ---- apoio: enrijecedores na alma do pilar (O.2.5.2.1) ------------------------------------------------------------------------
    if (in.support == CompositeConnectionInput::Column) {
        if (in.stiffeners) {
            Check c = detail::minimumRule("Enrijecedores da alma do pilar: area do par >= area da mesa inferior", "NBR 8800:2024, O.2.5.2.1", in.stiffenerArea, afi, "mm2");
            c.lines.push_back("Dimensionar os enrijecedores pela 5.7.9. Sem enrijecedores, rigidez, resistencia e deformacao da ligacao inferior devem ser recalculadas.");
            checks.push_back(c);
        } else {
            Check c = detail::dimensionless("Enrijecedores da alma do pilar no nivel da mesa inferior", "NBR 8800:2024, O.2.5.2.1", 2.0);
            c.lines.push_back("A norma considera um par de enrijecedores com area >= a da mesa inferior da viga; sem eles os parametros da ligacao inferior devem ser alterados (nao ha procedimento em O).");
            checks.push_back(c);
        }
    }
    r.notes.push_back("A ligacao da alma e verificada a parte somente para a forca cortante (O.1.4 e O.2.5.1).");
    return r;
}

std::vector<Part> partsCompositeConnection(const CompositeConnectionInput &in)
{
    std::vector<Part> parts;
    if (in.variant != CompositeConnectionInput::EndPlate) {
        Part ang;
        ang.description = fmtn("Cantoneira inferior L %.0f x %.0f x %.1f, comprimento %.0f mm", in.angleLeg, in.angleLegVertical, in.angleT, in.angleLeg);
        ang.quantity = 1;
        ang.massEach = (in.angleLeg + in.angleLegVertical - in.angleT) * in.angleT * in.angleLeg * 1e-9 * detail::kSteelDensity;
        parts.push_back(ang);
        Part bolts;
        bolts.description = fmtn("Parafuso d = %.0f mm (aba horizontal)", in.db);
        bolts.quantity = 2 * std::max(1, in.rowsL);
        bolts.massEach = 0.25 * kPi * sq(in.db) * (in.angleT + in.tf + 1.5 * in.db) * 1e-9 * detail::kSteelDensity * 1.35 + 0.04;
        parts.push_back(bolts);
    }
    if (in.support == CompositeConnectionInput::Column && in.stiffeners && in.stiffenerArea > 0.0) {
        Part st;
        st.description = fmtn("Par de enrijecedores da alma do pilar (area %.0f mm2)", in.stiffenerArea);
        st.quantity = 1;
        st.massEach = in.stiffenerArea * in.columnD * 1e-9 * detail::kSteelDensity;
        parts.push_back(st);
    }
    Part bars;
    bars.description = fmtn("Armadura longitudinal %d d%.1f mm (CA-50)", in.bars, in.barDiameter);
    bars.quantity = in.bars;
    bars.length = 2.0 * 0.15 * in.span;
    bars.unit = "mm";
    bars.massEach = 0.25 * kPi * sq(in.barDiameter) * bars.length * 1e-9 * detail::kSteelDensity;
    parts.push_back(bars);
    return parts;
}

} // namespace Structura::Design::Connections
