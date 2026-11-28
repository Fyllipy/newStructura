#include "design/JointComponents.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <type_traits>

namespace Structura::Design::JointComponents {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kInf = 1.0e30;

template <typename T>
inline auto fa(T value)
{
    if constexpr (std::is_integral_v<T>) {
        return static_cast<double>(value);
    } else {
        return value;
    }
}

template <typename... Args>
std::string f(const char *format, Args... args)
{
    char buf[700];
    std::snprintf(buf, sizeof buf, format, fa(args)...);
    return buf;
}

double inverse(const Component &c) { return c.infinite || c.k <= 0.0 ? 0.0 : 1.0 / c.k; }

struct Geometry {
    double dc {0.0};      ///< altura livre da alma do pilar
    double avc {0.0};     ///< area de cisalhamento do pilar
    double beffC {0.0};   ///< largura efetiva da alma comprimida (6.2.6.2)
    double beffT {0.0};   ///< largura efetiva da alma tracionada, ligacao soldada (6.2.6.3(2))
    double sp {0.0};
};

Geometry geometryOf(const ModelInput &in)
{
    Geometry g;
    g.dc = in.colDepth - 2.0 * in.colTf - 2.0 * in.colRadius;
    const double area = in.colArea > 0.0 ? in.colArea : 2.0 * in.colBf * in.colTf + (in.colDepth - 2.0 * in.colTf) * in.colTw + (4.0 - kPi) * in.colRadius * in.colRadius;
    g.avc = area - 2.0 * in.colBf * in.colTf + (in.colTw + 2.0 * in.colRadius) * in.colTf;   // EN 1993-1-1, 6.2.6(3)
    g.avc += in.doublerThickness * (in.colDepth - 2.0 * in.colTf);                            // chapa de reforco da alma (6.2.6.1(6))
    const double common = in.beamTf + 2.0 * std::sqrt(2.0) * in.flangeThroat + 5.0 * (in.colTf + in.colRadius);
    g.beffT = common;
    g.sp = in.kind == ModelInput::ExtendedEndPlate ? std::clamp(std::min(2.0 * in.plateThickness, in.plateBelowFlange), in.plateThickness, 2.0 * in.plateThickness) : 0.0;
    g.beffC = common + g.sp;   // (6.10) soldada; (6.11) chapa: + sp (dispersao a 45 graus pela chapa)
    return g;
}

/// omega da Tabela 6.3: reducao pela interacao com o cisalhamento do painel.
double omegaOf(double beta, double beff, double twc, double avc)
{
    const double r = beff * twc / avc;
    const double w1 = 1.0 / std::sqrt(1.0 + 1.3 * r * r);
    const double w2 = 1.0 / std::sqrt(1.0 + 5.2 * r * r);
    if (beta <= 0.5) {
        return 1.0;
    }
    if (beta < 1.0) {
        return w1 + 2.0 * (1.0 - beta) * (1.0 - w1);
    }
    if (beta == 1.0) {
        return w1;
    }
    if (beta < 2.0) {
        return w1 + (beta - 1.0) * (w2 - w1);
    }
    return w2;
}

struct TStub {
    double resistance {0.0};
    std::string mode;
    std::vector<std::string> lines;
};

/// Resistencia de um T-stub (Tabela 6.2, Metodo 1, com possibilidade de alavanca): modos 1, 2 e 3. `sumFt` = soma de Ft,Rd dos parafusos do T-stub.
TStub tStub(double leff1, double leff2, double tf, double fy, double gamma, double m, double e, double sumFt, const std::string &what)
{
    TStub t;
    const double n = std::min(e, 1.25 * m);
    const double mpl1 = 0.25 * leff1 * tf * tf * fy / gamma;
    const double mpl2 = 0.25 * leff2 * tf * tf * fy / gamma;
    const double f1 = 4.0 * mpl1 / m;
    const double f2 = (2.0 * mpl2 + n * sumFt) / (m + n);
    const double f3 = sumFt;
    t.resistance = std::min({f1, f2, f3});
    t.mode = t.resistance == f1 ? "modo 1 (escoamento completo da mesa)" : t.resistance == f2 ? "modo 2 (ruptura do parafuso com escoamento da mesa)" : "modo 3 (ruptura dos parafusos)";
    t.lines.push_back(what + f(": leff,1 = %.1f; leff,2 = %.1f; m = %.2f; n = min(e; 1,25 m) = %.2f mm", leff1, leff2, m, n));
    t.lines.push_back(f("  Mpl,1,Rd = 0,25 leff,1 t^2 fy/g = %.0f N.mm;  modo 1 = 4 Mpl,1/m = %.0f N;  modo 2 = (2 Mpl,2 + n SFt)/(m+n) = %.0f N;  modo 3 = SFt = %.0f N", mpl1, f1, f2, f3));
    return t;
}

double plasticModulusOf(const ModelInput &in)
{
    if (in.beamPlasticModulus > 0.0) {
        return in.beamPlasticModulus;
    }
    return in.beamBf * in.beamTf * (in.beamDepth - in.beamTf) + in.beamTw * std::pow(in.beamDepth - 2.0 * in.beamTf, 2.0) / 4.0;   // sem os raios de concordancia (conservador)
}

double boltResistance(const ModelInput &in)
{
    return in.boltTensionResistance > 0.0 ? in.boltTensionResistance : 0.9 * 800.0 * in.boltStressArea / 1.25;
}

} // namespace

double BoltRow::leffColumn() const { return std::min(leffColumnCircular, leffColumnNonCircular); }
double BoltRow::leffPlate() const { return std::min(leffPlateCircular, leffPlateNonCircular); }

std::vector<BoltRow> deriveExtensionRows(const ModelInput &in)
{
    std::vector<BoltRow> rows;
    const int count = std::max(1, in.tensionRows);
    const double w = in.gauge;
    const double mc = (w - in.colTw - 1.6 * in.colRadius) / 2.0;   // m da mesa do pilar (Fig. 6.8, laminado: 0,8 rc)
    const double ec = std::max((in.colBf - w) / 2.0, 1.0);
    const double ep = std::max((in.plateWidth - w) / 2.0, 1.0);
    for (int r = 1; r <= count; ++r) {   // r = 1: a mais proxima da mesa; a numeracao da EN comeca na mais afastada
        BoltRow row;
        const double x = in.firstRow + (r - 1) * in.pitch;   // da face externa da mesa da viga ao eixo da fileira
        const double ex = in.edgeVertical + (count - r) * in.pitch;
        const double mx = x - 0.8 * in.weldLeg;              // m da chapa na extensao: mx (Fig. 6.10), 0,8 sqrt(2) ap = 0,8 perna
        row.h = in.beamDepth - in.beamTf / 2.0 + x;
        row.mColumn = mc;
        row.eColumn = ec;
        row.leffColumnCircular = 2.0 * kPi * mc;                 // Tabela 6.4, fileira interna isolada
        row.leffColumnNonCircular = 4.0 * mc + 1.25 * ec;
        row.mPlate = mx;
        row.ePlate = ep;
        row.leffPlateCircular = 2.0 * kPi * mx;                  // Tabela 6.6, fileira na extensao
        row.leffPlateNonCircular = std::min({4.0 * mx + 1.25 * ex, ep + 2.0 * mx + 0.625 * ex, 0.5 * in.plateWidth, 0.5 * w + 2.0 * mx + 0.625 * ex});
        rows.push_back(row);
    }
    std::sort(rows.begin(), rows.end(), [](const BoltRow &a, const BoltRow &b) { return a.h > b.h; });
    for (std::size_t i = 0; i < rows.size(); ++i) {
        rows[i].row = static_cast<int>(i) + 1;
    }
    return rows;
}

StiffnessResult computeStiffness(const ModelInput &in)
{
    StiffnessResult r;
    const double E = in.youngModulus;
    const Geometry g = geometryOf(in);
    if (E <= 0.0 || (!in.beamSplice && (g.dc <= 0.0 || in.colTw <= 0.0 || in.colTf <= 0.0)) || in.beamDepth <= in.beamTf || in.beamTf <= 0.0) {
        r.error = "Geometria invalida (altura livre da alma do pilar, espessuras ou E).";
        return r;
    }
    const bool endPlate = in.kind == ModelInput::ExtendedEndPlate;
    if (in.beamSplice && !endPlate) {
        r.error = "Emenda de viga: exige o modelo de chapa de extremidade (kind = ExtendedEndPlate).";
        return r;
    }
    std::vector<BoltRow> rows;
    if (endPlate) {
        rows = in.rows.empty() ? deriveExtensionRows(in) : in.rows;
        if (rows.empty() || in.plateThickness <= 0.0 || in.boltStressArea <= 0.0) {
            r.error = "Dados da chapa de extremidade ou dos parafusos invalidos.";
            return r;
        }
    }
    r.lines.push_back("MÉTODO DOS COMPONENTES — EN 1993-1-8:2005, 6.3 (rigidez inicial Sj,ini)");
    if (in.beamSplice) {
        r.lines.push_back(f("E = %.0f MPa;  EMENDA DE VIGA com duas chapas de extremidade (sem pilar): k1, k2, k3 e k4 nao existem; k5 de cada chapa e k10 (Tabela 6.10; 6.3.3.1(5))", E));
    } else {
        r.lines.push_back(f("E = %.0f MPa;  altura livre da alma do pilar dc = hc - 2 tfc - 2 rc = %.1f mm;  Avc = %.0f mm2", E, g.dc, g.avc));
    }

    double z = in.beamDepth - in.beamTf;
    Component k1;
    k1.name = "Painel de alma do pilar ao cisalhamento";
    k1.symbol = "k1";
    k1.clause = "6.3.2, Tabela 6.11";
    if (in.beamSplice) {
        k1.infinite = true;   // sem pilar: componente inexistente (nao listado)
    } else if (in.beta <= 1e-9) {
        k1.infinite = true;
        k1.lines.push_back("beta = 0 (momentos iguais nos dois lados do pilar): painel sem cisalhamento liquido, k1 = infinito");
    }
    Component k2;
    k2.name = "Alma do pilar comprimida";
    k2.symbol = "k2";
    k2.clause = "6.3.2, Tabela 6.11; beff,c,wc de 6.2.6.2";
    if (in.beamSplice) {
        k2.infinite = true;   // sem pilar: componente inexistente (nao listado)
    } else if (in.webStiffened) {
        k2.infinite = true;
        k2.lines.push_back("Alma enrijecida por enrijecedores horizontais alinhados com as mesas da viga: k2 = infinito");
    } else {
        k2.k = 0.7 * g.beffC * in.colTw / g.dc;
        k2.lines.push_back(f("beff,c,wc = tfb + 2 raiz(2) ap + 5 (tfc + s) + sp = %.2f mm (sp = %.2f mm)", g.beffC, g.sp));
        k2.lines.push_back(f("k2 = 0,7 beff,c,wc twc / dc = 0,7 x %.2f x %.2f / %.2f = %.3f mm", g.beffC, in.colTw, g.dc, k2.k));
    }

    double keq = 0.0;
    std::vector<Component> tension;
    if (!endPlate) {
        Component k3;
        k3.name = "Alma do pilar tracionada";
        k3.symbol = "k3";
        k3.clause = "6.3.2, Tabela 6.11; beff,t,wc de 6.2.6.3(2)";
        if (in.webStiffened) {
            k3.infinite = true;
            k3.lines.push_back("Alma enrijecida: k3 = infinito");
        } else {
            k3.k = 0.7 * g.beffT * in.colTw / g.dc;
            k3.lines.push_back(f("beff,t,wc = tfb + 2 raiz(2) ap + 5 (tfc + s) = %.2f mm", g.beffT));
            k3.lines.push_back(f("k3 = 0,7 beff,t,wc twc / dc = %.3f mm", k3.k));
        }
        tension.push_back(k3);
        keq = k3.infinite ? 0.0 : k3.k;
        r.kEquivalent = k3.k;
    } else {
        const double lb = in.plateThickness + (in.beamSplice ? in.plateThickness : in.colTf) + 2.0 * in.washerThickness
                          + ((in.boltHeadHeight > 0.0 ? in.boltHeadHeight : 0.7 * in.boltDiameter) + (in.boltNutHeight > 0.0 ? in.boltNutHeight : 0.8 * in.boltDiameter)) / 2.0;
        double sumKh = 0.0, sumKh2 = 0.0;
        for (const BoltRow &row : rows) {
            if ((!in.beamSplice && row.mColumn <= 0.0) || row.mPlate <= 0.0) {
                r.error = "Distancias m da fileira de parafusos invalidas (m <= 0): verifique gabarito, perna da solda e posicao da fileira.";
                return r;
            }
            const double leffC = row.leffColumn(), leffP = row.leffPlate();
            Component k3, k4, k5, k10;
            k3.name = "Alma do pilar tracionada";
            k3.symbol = "k3";
            k3.clause = "6.3.2, Tabela 6.11";
            k3.row = row.row;
            if (in.webStiffened) {
                k3.infinite = true;
                k3.lines.push_back("Alma enrijecida: k3 = infinito");
            } else {
                k3.k = 0.7 * leffC * in.colTw / g.dc;
                k3.lines.push_back(f("beff,t,wc = leff da mesa do pilar (fileira isolada) = %.2f mm;  k3 = 0,7 beff,t,wc twc / dc = %.3f mm", leffC, k3.k));
            }
            k4.name = "Mesa do pilar em flexão";
            k4.symbol = "k4";
            k4.clause = "6.3.2, Tabela 6.11";
            k4.row = row.row;
            k4.k = 0.9 * leffC * std::pow(in.colTf, 3.0) / std::pow(row.mColumn, 3.0);
            k4.lines.push_back(f("m = %.2f mm;  leff = min(circular = %.2f; não circular = %.2f) = %.2f mm", row.mColumn, row.leffColumnCircular, row.leffColumnNonCircular, leffC));
            k4.lines.push_back(f("k4 = 0,9 leff tfc^3 / m^3 = %.3f mm", k4.k));
            k5.name = "Chapa de extremidade em flexão";
            k5.symbol = "k5";
            k5.clause = "6.3.2, Tabela 6.11";
            k5.row = row.row;
            k5.k = 0.9 * leffP * std::pow(in.plateThickness, 3.0) / std::pow(row.mPlate, 3.0);
            k5.lines.push_back(f("m = %.2f mm;  leff = min(circular = %.2f; não circular = %.2f) = %.2f mm", row.mPlate, row.leffPlateCircular, row.leffPlateNonCircular, leffP));
            k5.lines.push_back(f("k5 = 0,9 leff tp^3 / m^3 = %.3f mm", k5.k));
            k10.name = "Parafusos tracionados";
            k10.symbol = "k10";
            k10.clause = "6.3.2, Tabela 6.11";
            k10.row = row.row;
            k10.k = 1.6 * in.boltStressArea / lb;   // "for a single bolt-row": sem fator 2 (conferido com SCI P398 C2 e SCIA)
            k10.lines.push_back(f("Lb = tp + tfc + arruelas + (cabeça + porca)/2 = %.2f mm", lb));
            k10.lines.push_back(f("k10 = 1,6 As / Lb = 1,6 x %.0f / %.2f = %.3f mm (uma fileira, As de um parafuso)", in.boltStressArea, lb, k10.k));
            double invSum = inverse(k3) + inverse(k4) + inverse(k5) + inverse(k10);
            if (in.beamSplice) {
                // emenda: a forca atravessa DUAS chapas (k5 de cada uma, em serie) e os parafusos (6.3.3.1(5)); nao ha componentes do pilar
                k5.name = "Chapa de extremidade em flexão (chapa 1)";
                Component k5b = k5;
                k5b.name = "Chapa de extremidade em flexão (chapa 2, igual à 1)";
                k10.lines.back() += "; Lb = 2 tp + arruelas + (cabeça + porca)/2 (emenda: material agarrado = as duas chapas)";
                invSum = inverse(k5) + inverse(k5b) + inverse(k10);
                tension.push_back(k5);
                tension.push_back(k5b);
                tension.push_back(k10);
            } else {
                tension.push_back(k3);
                tension.push_back(k4);
                tension.push_back(k5);
                tension.push_back(k10);
            }
            const double keff = invSum > 0.0 ? 1.0 / invSum : 0.0;
            RowResult rr;
            rr.row = row.row;
            rr.h = row.h;
            rr.keff = keff;
            rr.leffColumn = leffC;
            rr.leffPlate = leffP;
            r.rows.push_back(rr);
            r.lines.push_back(in.beamSplice ? f("Fileira %.0f: h = %.2f mm;  keff,r = 1 / (1/k5 + 1/k5 + 1/k10) = %.4f mm (emenda: duas chapas)", row.row, row.h, keff)
                                            : f("Fileira %.0f: h = %.2f mm;  keff,r = 1 / (1/k3 + 1/k4 + 1/k5 + 1/k10) = %.4f mm", row.row, row.h, keff));
            sumKh += keff * row.h;
            sumKh2 += keff * row.h * row.h;
        }
        if (sumKh <= 0.0) {
            r.error = "Rigidez das fileiras tracionadas nula.";
            return r;
        }
        z = sumKh2 / sumKh;
        keq = sumKh / z;
        r.kEquivalent = keq;
        r.lines.push_back(f("zeq = soma(keff,r h_r^2) / soma(keff,r h_r) = %.2f mm;  keq = soma(keff,r h_r) / zeq = %.4f mm", z, keq));
    }
    r.leverArm = z;
    if (!k1.infinite) {
        k1.k = 0.38 * g.avc / (in.beta * z);
        k1.lines.push_back(f("k1 = 0,38 Avc / (beta z) = 0,38 x %.0f / (%.2f x %.2f) = %.3f mm (z = %s)", g.avc, in.beta, z, k1.k, endPlate ? "zeq" : "d - tfb"));
    }
    if (!in.beamSplice) {
        r.components.push_back(k1);
        r.components.push_back(k2);
    }
    for (const Component &c : tension) {
        r.components.push_back(c);
    }
    const double invTotal = inverse(k1) + inverse(k2) + (keq > 0.0 ? 1.0 / keq : 0.0);
    if (invTotal <= 0.0) {
        r.error = "Ligação sem flexibilidade (todos os componentes rígidos): Sj,ini não limitada pelo modelo.";
        return r;
    }
    r.lines.push_back("Componentes:");
    for (const Component &c : r.components) {
        std::string head = "  " + c.symbol + (c.row > 0 ? " (fileira " + std::to_string(c.row) + ")" : std::string()) + " - " + c.name + ": ";
        head += c.infinite ? std::string("infinito (rígido)") : f("%.3f mm", c.k);
        r.lines.push_back(head);
        for (const std::string &l : c.lines) {
            r.lines.push_back("      " + l);
        }
    }
    r.initialStiffness = E * z * z / invTotal;
    r.lines.push_back(f("Soma das flexibilidades: 1/k1 + 1/k2 + 1/keq = %.5g 1/mm", invTotal));
    r.lines.push_back(f("Sj,ini = E z^2 / soma(1/ki) = %.0f x %.2f^2 / %.5g = %.4g N.mm/rad = %.1f kN.m/rad", E, z, invTotal, r.initialStiffness, r.initialStiffnessKnm()));
    r.ok = true;
    return r;
}

Resistance computeResistance(const ModelInput &in)
{
    Resistance r;
    const Geometry g = geometryOf(in);
    if ((!in.beamSplice && (g.dc <= 0.0 || in.colTw <= 0.0)) || in.beamDepth <= in.beamTf || in.gammaYield <= 0.0) {
        r.error = "Geometria inválida para a resistência.";
        return r;
    }
    const double gamma = in.gammaYield;
    const bool endPlate = in.kind == ModelInput::ExtendedEndPlate;
    const double bt = boltResistance(in);
    r.lines.push_back("RESISTÊNCIA DOS COMPONENTES — EN 1993-1-8:2005, 6.2 (gamma de escoamento = " + f("%.2f", gamma) + ")");

    const double beta = in.beta;
    if (in.beamSplice && in.kind != ModelInput::ExtendedEndPlate) {
        r.error = "Emenda de viga: exige o modelo de chapa de extremidade (kind = ExtendedEndPlate).";
        return r;
    }
    const double vwp = 0.9 * in.colFy * g.avc / (std::sqrt(3.0) * gamma);
    r.panelShear = (in.beamSplice || beta <= 1e-9) ? kInf : vwp / beta;
    if (in.beamSplice) {
        r.lines.push_back("EMENDA DE VIGA (EN 1993-1-8, 6.2.7.2(10)): resistencia pelo mesmo metodo, omitindo os itens do pilar (painel, alma do pilar, mesa do pilar).");
    } else {
        r.lines.push_back(f("Vwp,Rd = 0,9 fy,wc Avc / (raiz(3) g) = %.0f N (6.2.6.1);  Vwp,Rd / beta = %s", vwp, beta <= 1e-9 ? "infinito (beta = 0)" : f("%.0f N", vwp / beta).c_str()));
    }
    if (in.beamSplice) {
        r.columnWebCompression = kInf;
    } else if (in.webStiffened) {
        r.columnWebCompression = kInf;
        r.lines.push_back("Fc,wc,Rd: alma enrijecida, não limita.");
    } else {
        const double omega = omegaOf(beta, g.beffC, in.colTw, g.avc);
        const double lambdaP = 0.932 * std::sqrt(g.beffC * g.dc * in.colFy / (in.youngModulus * in.colTw * in.colTw));
        const double rho = lambdaP <= 0.72 ? 1.0 : (lambdaP - 0.2) / (lambdaP * lambdaP);
        const double kwc = in.columnAxialStressRatio > 0.7 ? std::max(0.0, 1.7 - in.columnAxialStressRatio) : 1.0;
        const double a = omega * kwc * g.beffC * in.colTw * in.colFy / gamma;
        const double b = omega * kwc * rho * g.beffC * in.colTw * in.colFy / gamma;
        r.columnWebCompression = std::min(a, b);
        r.lines.push_back(f("Fc,wc,Rd (6.2.6.2): omega = %.3f; kwc = %.3f; beff,c,wc = %.2f mm; lambda_p = %.3f; rho = %.3f  ->  %.0f N", omega, kwc, g.beffC, lambdaP, rho, r.columnWebCompression));
    }
    const double mcRd = plasticModulusOf(in) * in.beamFy / gamma;
    r.beamFlangeCompression = mcRd / (in.beamDepth - in.beamTf);
    r.lines.push_back(f("Fc,fb,Rd = Mc,Rd / (h - tfb) = %.0f / %.2f = %.0f N (6.2.6.7)", mcRd, in.beamDepth - in.beamTf, r.beamFlangeCompression));
    r.cap = std::min({r.panelShear, r.columnWebCompression, r.beamFlangeCompression});
    r.capComponent = r.cap == r.panelShear ? "painel de alma ao cisalhamento (Vwp,Rd/beta)" : r.cap == r.columnWebCompression ? "alma do pilar comprimida (Fc,wc,Rd)" : "mesa e alma da viga comprimidas (Fc,fb,Rd)";

    if (!endPlate) {
        const double z = in.beamDepth - in.beamTf;
        double ftwc = kInf;
        if (!in.webStiffened) {
            const double omega = omegaOf(beta, g.beffT, in.colTw, g.avc);
            ftwc = omega * g.beffT * in.colTw * in.colFy / gamma;
            r.lines.push_back(f("Ft,wc,Rd (6.2.6.3): omega = %.3f; beff,t,wc = %.2f mm  ->  %.0f N", omega, g.beffT, ftwc));
        } else {
            r.lines.push_back("Ft,wc,Rd: alma enrijecida, não limita.");
        }
        const double force = std::min(ftwc, r.cap);
        r.momentResistance = force * z;
        r.lines.push_back(f("Mj,Rd = F x z = %.0f N x %.2f mm = %.2f kN.m (limitada por: %s)", force, z, r.momentResistance * 1e-6, force == ftwc ? "alma do pilar tracionada" : r.capComponent.c_str()));
        r.ok = r.momentResistance > 0.0;
        return r;
    }

    const std::vector<BoltRow> rows = in.rows.empty() ? deriveExtensionRows(in) : in.rows;
    if (rows.empty() || bt <= 0.0) {
        r.error = "Fileiras ou resistência de parafuso inválidas.";
        return r;
    }
    const double sumFtRow = 2.0 * bt;
    std::vector<RowResult> results;
    for (const BoltRow &row : rows) {
        RowResult rr;
        rr.row = row.row;
        rr.h = row.h;
        rr.leffColumn = row.leffColumn();
        rr.leffPlate = row.leffPlate();
        TStub fc;
        if (in.beamSplice) {
            fc.resistance = kInf;   // sem pilar
        } else {
            fc = tStub(std::min(row.leffColumnCircular, row.leffColumnNonCircular), row.leffColumnNonCircular, in.colTf, in.colFy, gamma, row.mColumn, row.eColumn, sumFtRow,
                       "Fileira " + std::to_string(row.row) + ", mesa do pilar");
        }
        const TStub ep = tStub(std::min(row.leffPlateCircular, row.leffPlateNonCircular), row.leffPlateNonCircular, in.plateThickness, in.plateFy, gamma, row.mPlate, row.ePlate, sumFtRow,
                               "Fileira " + std::to_string(row.row) + ", chapa de extremidade");
        const double omegaT = in.beamSplice ? 1.0 : omegaOf(beta, row.leffColumn(), in.colTw, g.avc);
        const double ftwc = (in.beamSplice || in.webStiffened) ? kInf : omegaT * row.leffColumn() * in.colTw * in.colFy / gamma;
        const double ftwb = row.leffPlate() * in.beamTw * in.beamFy / gamma;
        rr.individual = std::min({fc.resistance, ep.resistance, ftwc, ftwb});
        rr.governingComponent = rr.individual == ep.resistance ? "chapa de extremidade (" + ep.mode + ")"
                              : rr.individual == fc.resistance ? "mesa do pilar (" + fc.mode + ")"
                              : rr.individual == ftwc          ? "alma do pilar tracionada (Ft,wc,Rd)"
                                                               : "alma da viga tracionada (Ft,wb,Rd)";
        if (!in.beamSplice) {
            r.lines.push_back(fc.lines[0]);
            r.lines.push_back(fc.lines[1]);
        }
        r.lines.push_back(ep.lines[0]);
        r.lines.push_back(ep.lines[1]);
        r.lines.push_back(f("Fileira %.0f: Ft,fc,Rd = %.0f N;  Ft,ep,Rd = %.0f N;  Ft,wc,Rd = %s;  Ft,wb,Rd = %.0f N  ->  isolada = %.0f N (%s)", row.row, fc.resistance, ep.resistance,
                            ftwc >= kInf ? "infinito" : f("%.0f N", ftwc).c_str(), ftwb, rr.individual, rr.governingComponent.c_str()));
        results.push_back(rr);
    }
    // distribuicao (6.2.7.2(3) a (9)): da fileira mais afastada para a mais proxima
    double total = 0.0;
    int xRow = -1;   // 1a fileira com resistencia individual > 1,9 Ft,Rd de um parafuso (6.2.7.2(9))
    for (std::size_t i = 0; i < results.size(); ++i) {
        RowResult &rr = results[i];
        double value = rr.individual;
        std::string limited;
        if (r.cap - total < value) {
            value = std::max(0.0, r.cap - total);
            limited = r.capComponent;
        }
        if (xRow >= 0) {
            const RowResult &x = results[static_cast<std::size_t>(xRow)];
            const double bound = x.effective * rr.h / x.h;
            if (bound < value) {
                value = bound;
                limited = "regra de 1,9 Ft,Rd (6.2.7.2(9)): distribuição triangular a partir da fileira " + std::to_string(x.row);
            }
        }
        rr.effective = value;
        rr.limitedBy = limited;
        if (xRow < 0 && rr.individual > 1.9 * bt) {
            xRow = static_cast<int>(i);
        }
        total += value;
    }
    double mj = 0.0;
    for (const RowResult &rr : results) {
        mj += rr.effective * rr.h;
        r.lines.push_back(f("Fileira %.0f: Ftr,Rd efetiva = %.0f N (h = %.2f mm)", rr.row, rr.effective, rr.h) + (rr.limitedBy.empty() ? std::string() : "  [limitada por: " + rr.limitedBy + "]"));
    }
    r.rows = results;
    r.momentResistance = mj;
    r.lines.push_back(f("Mj,Rd = soma(Ftr,Rd hr) = %.2f kN.m", mj * 1e-6));
    r.lines.push_back("Não considerado: efeito de grupo de fileiras (6.2.7.2(8)); o resultado pode ser não conservador com fileiras muito próximas.");
    r.ok = mj > 0.0;
    return r;
}

std::vector<Structura::Design::JointClass::CurvePoint> momentRotationCurve(double sIniKnm, double mjRdKnm, double psi, int points)
{
    std::vector<Structura::Design::JointClass::CurvePoint> curve;
    if (sIniKnm <= 0.0 || mjRdKnm <= 0.0) {
        return curve;
    }
    const int n = 3 * std::max(2, (points + 2) / 3);   // multiplo de 3: inclui exatamente 2/3 Mj,Rd
    for (int i = 1; i <= n; ++i) {
        const double m = mjRdKnm * i / n;
        const double mu = std::max(1.0, std::pow(1.5 * m / mjRdKnm, psi));
        curve.push_back({m / sIniKnm * mu, m});
    }
    return curve;
}

double secantStiffnessAt(double m, double sIniKnm, double mjRdKnm, double psi)
{
    const double mu = std::max(1.0, std::pow(1.5 * std::min(m, mjRdKnm) / mjRdKnm, psi));
    return sIniKnm / mu;
}

double tangentStiffnessAt(double m, double sIniKnm, double mjRdKnm, double psi)
{
    const double x = 1.5 * std::min(m, mjRdKnm) / mjRdKnm;
    if (x <= 1.0) {
        return sIniKnm;   // trecho linear: M <= 2/3 Mj,Rd
    }
    // theta = M/S x^psi com x = 1,5 M/Mj,Rd  ->  dtheta/dM = (1/S) x^psi (1 + psi)
    return sIniKnm / (std::pow(x, psi) * (1.0 + psi));
}

CurveMetrics curveMetrics(double sIniKnm, double mjRdKnm, double psi)
{
    CurveMetrics c;
    if (sIniKnm <= 0.0 || mjRdKnm <= 0.0) {
        return c;
    }
    c.initialStiffness = sIniKnm;
    c.momentResistance = mjRdKnm;
    c.onsetOfNonlinearity = 2.0 / 3.0 * mjRdKnm;
    c.rotationAtOnset = c.onsetOfNonlinearity / sIniKnm;
    c.rotationAtResistance = mjRdKnm / sIniKnm * std::pow(1.5, psi);
    c.secantAtResistance = secantStiffnessAt(mjRdKnm, sIniKnm, mjRdKnm, psi);
    c.tangentAtResistance = tangentStiffnessAt(mjRdKnm, sIniKnm, mjRdKnm, psi);
    // secante = Sj,ini / mu = Sj,ini / 2  ->  (1,5 M/Mj,Rd)^psi = 2
    c.momentAtHalfStiffness = mjRdKnm / 1.5 * std::pow(2.0, 1.0 / psi);
    c.rotationAtHalfStiffness = c.momentAtHalfStiffness / (sIniKnm / 2.0);
    return c;
}

EquivalentModel deriveEquivalent(int kind, double sIniKnm, double mjRdKnm, double psi)
{
    EquivalentModel e;
    e.kind = kind;
    switch (kind) {
    case EquivalentModel::Pinned:
        e.valid = true;
        e.note = "articulação (k = 0)";
        return e;
    case EquivalentModel::Rigid:
        e.valid = true;
        e.note = "continuidade (sem mola)";
        return e;
    case EquivalentModel::SemiRigidLinear:
        e.valid = sIniKnm > 0.0;
        e.stiffness = sIniKnm;
        e.note = "mola rotacional linear Sj,ini (NBR 8800:2024, 6.1.2.1: Si constante)";
        return e;
    case EquivalentModel::SemiRigidBilinear: {
        if (sIniKnm <= 0.0 || mjRdKnm <= 0.0) {
            e.note = "rótula bilinear exige Sj,ini e Mj,Rd";
            return e;
        }
        const CurveMetrics c = curveMetrics(sIniKnm, mjRdKnm, psi);
        const double post = (mjRdKnm - c.onsetOfNonlinearity) / (c.rotationAtResistance - c.rotationAtOnset);
        e.valid = true;
        e.stiffness = sIniKnm;
        e.yieldMoment = c.onsetOfNonlinearity;
        e.hardeningRatio = post / sIniKnm;
        e.note = "rótula bilinear: elástica até 2/3 Mj,Rd; depois a secante entre 2/3 Mj,Rd e Mj,Rd";
        return e;
    }
    }
    return e;
}

} // namespace Structura::Design::JointComponents
