#include "design/TubularJoints.h"

#include "design/ConnectionHelpers.h"

#include <algorithm>
#include <cmath>

namespace Structura::Design::Connections {

using namespace detail;

namespace {

double rad(double deg) { return deg * kPi / 180.0; }

/// Limite d/t de classe 2 (Tabela 4.2); degraus conservadores entre as classes de aco da tabela.
double class2Limit(double fy)
{
    if (fy <= 235.0) return 70.0;
    if (fy <= 275.0) return 59.0;
    if (fy <= 355.0) return 46.0;
    return 35.0;
}

double plasticModulusCircular(double d, double t)
{
    const double di = d - 2.0 * t;
    return (d * d * d - di * di * di) / 6.0;
}

double areaCircular(double d, double t)
{
    return kPi / 4.0 * (d * d - (d - 2.0 * t) * (d - 2.0 * t));
}

} // namespace

std::vector<Check> checkTubularJoint(const TubularJointInput &in)
{
    std::vector<Check> checks;
    const bool plate = in.braceKind == TubularJointInput::TransversePlate;
    const int kind = plate && in.kind != TubularJointInput::X ? TubularJointInput::TY : in.kind;
    const bool isK = kind == TubularJointInput::KGap || kind == TubularJointInput::KOverlap;
    const int braces = isK ? 2 : 1;

    // propriedades do banzo e fatores gerais
    const double fy0 = std::min(in.fy0, 0.8 * in.fu0);
    const double reduction = in.fy0 > 355.0 ? 0.9 : 1.0;   // 1.2.1: fy0 > 355 MPa
    const double a0 = areaCircular(in.d0, in.t0);
    const double npl0 = a0 * fy0;
    const double mpl0 = plasticModulusCircular(in.d0, in.t0) * fy0;
    const double gamma = in.d0 / (2.0 * in.t0);

    struct Brace {
        double d, t, fy, theta, n;
        int index;
    };
    std::vector<Brace> list;
    list.push_back({in.d1, in.t1, in.fy1, in.theta1, in.n1, 1});
    if (isK) {
        list.push_back({in.d2, in.t2, in.fy2, in.theta2, in.n2, 2});
    }

    // --- faixa de validade (Tabela 4.1 / 7.1) ---------------------------------------------------------------
    {
        const double classLimit = class2Limit(in.fy0);
        const double chordLimit = kind == TubularJointInput::X ? 40.0 : 50.0;
        const bool compression = in.chordAxial < 0.0;
        const double limit = compression ? std::min(chordLimit, classLimit) : chordLimit;
        Check c = rangeMax("Banzo: esbeltez d0/t0", "CIDECT 4.1 (validade)", in.d0 / in.t0, limit);
        c.lines.push_back(fmtn("2 gamma = d0/t0 = %.1f <= %.0f%s", in.d0 / in.t0, limit, compression ? " (compressao: classe 1 ou 2 e limite de 2 gamma)" : ""));
        checks.push_back(c);
        Check s = rangeMax("Aco: fy <= 460 MPa e fy <= 0,8 fu", "CIDECT 1.2.1", in.fy0, std::min(460.0, 0.8 * in.fu0), "MPa");
        s.lines.push_back(fmtn("fy0 = %.0f MPa; fu0 = %.0f MPa%s", in.fy0, in.fu0, in.fy0 > 355.0 ? "; fy0 > 355 MPa: resistencias multiplicadas por 0,9" : ""));
        checks.push_back(s);
    }
    for (const Brace &b : list) {
        const std::string tag = braces > 1 ? " (diagonal " + std::to_string(b.index) + ")" : "";
        const double beta = b.d / in.d0;
        Check c = beta < 0.2 ? rangeMin("Diagonal: 0,2 <= d/d0 <= 1,0" + tag, "CIDECT 4.1 (validade)", beta, 0.2) : rangeMax("Diagonal: 0,2 <= d/d0 <= 1,0" + tag, "CIDECT 4.1 (validade)", beta, 1.0);
        c.lines.push_back(fmtn("beta = d%d/d0 = %.3f", b.index, beta));
        checks.push_back(c);
        Check th = rangeMin("Diagonal: angulo >= 30 graus" + tag, "CIDECT 4.1 (validade)", b.theta, 30.0, "graus");
        th.lines.push_back(fmtn("theta = %.1f graus", b.theta));
        checks.push_back(th);
        if (!plate) {
            const double limit = std::min(50.0, class2Limit(b.fy));
            Check s = rangeMax("Diagonal: esbeltez d/t" + tag, "CIDECT 4.1 / 4.2", b.d / b.t, limit);
            s.lines.push_back(fmtn("d/t = %.1f <= %.0f (compressao: classe 1 ou 2, Tabela 4.2)", b.d / b.t, limit));
            checks.push_back(s);
            Check tt = rangeMax("Diagonal: t <= t0 e fy <= fy0" + tag, "CIDECT 4.1 (validade)", std::max(b.t / in.t0, b.fy / in.fy0), 1.0);
            tt.lines.push_back(fmtn("t = %.1f mm (t0 = %.1f mm); fy = %.0f MPa (fy0 = %.0f MPa)", b.t, in.t0, b.fy, in.fy0));
            checks.push_back(tt);
        }
    }
    if (in.kind == TubularJointInput::KGap && !plate) {
        const double minGap = in.t1 + in.t2;
        Check c = rangeMin("Afastamento g >= t1 + t2", "CIDECT 4.1 (validade)", in.gap, minGap, "mm");
        c.lines.push_back(fmtn("g = %.1f mm >= t1 + t2 = %.1f mm", in.gap, minGap));
        checks.push_back(c);
    }
    {
        Check c = rangeMax("Excentricidade e/d0 <= 0,25", "CIDECT 4.1 (validade)", std::abs(in.eccentricity), 0.25 * in.d0, "mm");
        c.lines.push_back(fmtn("|e| = %.1f mm <= 0,25 d0 = %.1f mm", std::abs(in.eccentricity), 0.25 * in.d0));
        checks.push_back(c);
    }

    // --- tensao no banzo: Qf -------------------------------------------------------------------------------------
    const double n = in.chordAxial / npl0 + std::abs(in.chordMoment) / mpl0;   // + tracao
    auto qfFor = [&](double beta, bool kGap) {
        const double nn = in.chordAxial / npl0 - std::abs(in.chordMoment) / mpl0;   // lado mais desfavoravel (compressao)
        const double nCompression = std::min(nn, 0.0);
        const double nTension = std::max(n, 0.0);
        const double c1c = kGap ? 0.25 : 0.45 - 0.25 * beta;
        const double qfC = nCompression < 0.0 ? std::pow(std::max(0.0, 1.0 - std::abs(nCompression)), c1c) : 1.0;
        const double qfT = nTension > 0.0 ? std::pow(std::max(0.0, 1.0 - nTension), 0.20) : 1.0;
        return std::min(qfC, qfT);
    };

    for (const Brace &b : list) {
        const std::string tag = braces > 1 ? " (diagonal " + std::to_string(b.index) + ")" : "";
        const double beta = b.d / in.d0;
        const double theta = rad(b.theta);
        const double sinT = std::sin(theta);
        if (sinT <= 1e-6 || b.n < 0.0) {
            continue;
        }
        double qu = 0.0;
        std::string quText;
        const double g = in.gap / in.t0;
        if (plate) {
            if (kind == TubularJointInput::X) {
                qu = 2.2 * std::pow((1.0 + beta) / (1.0 - 0.7 * beta), 1.0) * std::pow(gamma, 0.15);
                quText = "Qu = 2,2 [(1 + beta)/(1 - 0.7 beta)] gamma^0,15 (eq. 7.3, chapa transversal em X)";
            } else {
                qu = 2.2 * (1.0 + 6.8 * beta * beta) * std::pow(gamma, 0.2);
                quText = "Qu = 2,2 (1 + 6,8 beta^2) gamma^0,2 (eq. 7.4, chapa transversal em T)";
            }
        } else if (kind == TubularJointInput::X) {
            qu = 2.6 * ((1.0 + beta) / (1.0 - 0.7 * beta)) * std::pow(gamma, 0.15);
            quText = "Qu = 2,6 [(1 + beta)/(1 - 0.7 beta)] gamma^0,15 (eq. 4.4)";
        } else if (kind == TubularJointInput::TY) {
            qu = 2.6 * (1.0 + 6.8 * beta * beta) * std::pow(gamma, 0.2);
            quText = "Qu = 2,6 (1 + 6,8 beta^2) gamma^0,2 (eq. 4.5)";
        } else if (kind == TubularJointInput::KGap) {
            qu = 1.65 * (1.0 + 8.0 * std::pow(beta, 1.6)) * std::pow(gamma, 0.3) * (1.0 + 1.0 / (1.2 + std::pow(g, 0.8)));
            quText = "Qu = 1,65 (1 + 8 beta^1,6) gamma^0,3 [1 + 1/(1,2 + (g/t0)^0,8)] (eq. 4.6)";
        }
        if (kind != TubularJointInput::KOverlap) {
            const double qf = qfFor(beta, kind == TubularJointInput::KGap);
            const double nStar = qu * qf * fy0 * in.t0 * in.t0 / sinT * reduction;
            Check c = ratioN("Plastificacao da face do banzo" + tag, "CIDECT eq. 4.1", b.n, nStar);
            c.lines.push_back(fmtn("beta = %.3f; gamma = d0/(2 t0) = %.2f; theta = %.1f graus", beta, gamma, b.theta));
            c.lines.push_back(quText + fmtn(" = %.2f", qu));
            c.lines.push_back(fmtn("n = N0/Npl,0 + M0/Mpl,0 = %.3f; Qf = (1 - |n|)^C1 = %.3f", n, qf));
            c.lines.push_back(fmtn("N* = Qu Qf fy0 t0^2 / sin(theta) = %.2f kN%s", nStar / 1000.0, reduction < 1.0 ? " (x 0,9, fy0 > 355 MPa)" : ""));
            checks.push_back(c);
            // puncao: so para d_i <= d0 - 2 t0
            const double punchingWidth = plate ? b.d : b.d;
            if (punchingWidth <= in.d0 - 2.0 * in.t0) {
                if (plate) {
                    const double rd = 1.16 * fy0 * in.t0 * b.d * reduction;
                    Check p = ratioN("Puncao da parede do banzo (chapa)" + tag, "CIDECT eq. 7.2", b.n, rd);
                    p.lines.push_back(fmtn("N1 <= 1,16 fy0 t0 b1 = 1,16 x %.0f x %.1f x %.0f = %.2f kN", fy0, in.t0, b.d, rd / 1000.0));
                    checks.push_back(p);
                } else {
                    const double ka = (1.0 + sinT) / (2.0 * sinT);
                    const double nPunch = 0.58 * fy0 * kPi * b.d * in.t0 * ka / sinT * reduction;
                    Check p = ratioN("Puncao da parede do banzo" + tag, "CIDECT eq. 4.2", b.n, nPunch);
                    p.lines.push_back(fmtn("ka = (1 + sin theta)/(2 sin theta) = %.3f; N* = 0,58 fy0 pi d t0 ka / sin theta = %.2f kN", ka, nPunch / 1000.0));
                    checks.push_back(p);
                }
            }
        }
    }

    // --- cisalhamento do banzo (X com cos theta > beta; K com afastamento) -----------------------------------------
    {
        const double vpl = 0.58 * fy0 * 2.0 * a0 / kPi;
        if (kind == TubularJointInput::X && std::cos(rad(in.theta1)) > in.d1 / in.d0 && in.n1 > 0.0) {
            const double v = in.n1 * std::sin(rad(in.theta1));
            const double value = std::pow(in.chordAxial / npl0, 2) + std::pow(v / vpl, 2);
            Check c = dimensionless("Cisalhamento do banzo (X, cos theta > beta)", "CIDECT eq. 6.2", value);
            c.lines.push_back(fmtn("(N0/Npl,0)^2 + (V/Vpl,0)^2 = %.3f^2 + %.3f^2 = %.3f; Vpl,0 = 0,58 fy0 (2 A0/pi) = %.1f kN", in.chordAxial / npl0, v / vpl, value, vpl / 1000.0));
            checks.push_back(c);
        }
        if (kind == TubularJointInput::KGap) {
            const double v = std::abs(in.n1 * std::sin(rad(in.theta1)));
            const double value = std::pow(in.chordAxial / npl0, 2) + std::pow(v / vpl, 2);
            Check c = dimensionless("Cisalhamento do banzo no afastamento", "CIDECT eq. 6.2", value);
            c.lines.push_back(fmtn("(Ngap,0/Npl,0)^2 + (Vgap,0/Vpl,0)^2 = %.3f^2 + %.3f^2 = %.3f", in.chordAxial / npl0, v / vpl, value));
            c.lines.push_back("Ngap,0 tomado igual ao esforco axial informado do banzo (conservador).");
            checks.push_back(c);
        }
    }

    // --- K com sobreposicao ------------------------------------------------------------------------------------------
    if (kind == TubularJointInput::KOverlap && !plate) {
        const double ov = in.overlap;
        // diagonal 1: sobreposta (i); diagonal 2: da qual a 1 se sobrepoe (j)
        const double dei = std::min(in.d1, 12.0 / (in.d0 / in.t0) * (in.fy0 * in.t0) / (in.fy1 * in.t1) * in.d1);
        const double dej = std::min(in.d2, 12.0 / (in.d0 / in.t0) * (in.fy0 * in.t0) / (in.fy2 * in.t2) * in.d2);
        const double deov = std::min(in.d1, 12.0 / (in.d2 / in.t2) * (in.fy2 * in.t2) / (in.fy1 * in.t1) * in.d1);
        double nStarI = 0.0;
        if (ov >= 100.0 - 1e-9) {
            nStarI = in.fy1 * in.t1 * kPi / 4.0 * (2.0 * in.d1 + 2.0 * deov - 4.0 * in.t1);
        } else {
            nStarI = in.fy1 * in.t1 * kPi / 4.0 * (2.0 * in.d1 + dei + deov - 4.0 * in.t1);
        }
        nStarI *= reduction;
        Check ci = ratioN("Escoamento local da diagonal sobreposta (1)", ov >= 100.0 ? "CIDECT eq. 4.9" : "CIDECT eq. 4.8", in.n1, nStarI);
        ci.lines.push_back(fmtn("Larguras efetivas: dei = %.1f mm; de,ov = %.1f mm; N1* = fy1 t1 (pi/4) [2 d1 + dei + de,ov - 4 t1] = %.2f kN", dei, deov, nStarI / 1000.0));
        checks.push_back(ci);
        // eficiencia da diagonal sobreposta j limitada a da i
        const double efficiencyI = nStarI / (areaCircular(in.d1, in.t1) * in.fy1);
        const double nStarJ = efficiencyI * areaCircular(in.d2, in.t2) * in.fy2;
        Check cj = ratioN("Diagonal coberta (2): eficiencia limitada a da sobreposta", "CIDECT Tab. 4.3 (nota geral)", in.n2, nStarJ);
        cj.lines.push_back(fmtn("Eficiencia da diagonal 1 = N1*/(A1 fy1) = %.3f; N2* = eficiencia x A2 fy2 = %.2f kN", efficiencyI, nStarJ / 1000.0));
        cj.lines.push_back("Por convencao, a diagonal 1 e a que se sobrepoe (a de menor espessura/resistencia, ou a de menor diametro se iguais).");
        checks.push_back(cj);
        // plastificacao local do banzo
        {
            const double value = std::pow(std::abs(in.chordAxial) / npl0, 1.7) + std::abs(in.chordMoment) / mpl0;
            Check c = dimensionless("Escoamento local do banzo (sobreposicao)", "CIDECT eq. 4.10", value);
            c.lines.push_back(fmtn("(N0/Npl,0)^1,7 + M0/Mpl,0 = %.3f", value));
            checks.push_back(c);
        }
        const double limit = in.hiddenToeWelded ? 80.0 : 60.0;
        if (ov > limit) {
            const double cs = in.hiddenToeWelded ? 2.0 : 1.0;
            const double lhs = in.n1 * std::cos(rad(in.theta1)) + in.n2 * std::cos(rad(in.theta2));
            double rhs;
            if (ov >= 100.0 - 1e-9) {
                rhs = 0.58 * in.fy2 * kPi / 4.0 * (3.0 * in.d2 + dej) * in.t2 / std::sin(rad(in.theta2));
            } else {
                rhs = kPi / 4.0 * (0.58 * in.fy1 * ((100.0 - ov) / 100.0 * (2.0 * in.d1 + dei)) * in.t1 / std::sin(rad(in.theta1)) +
                                   0.58 * in.fy2 * (2.0 * in.d2 + cs * dej) * in.t2 / std::sin(rad(in.theta2)));
            }
            rhs *= reduction;
            Check c = ratioN("Cisalhamento das diagonais (Ov > Ov,limite)", ov >= 100.0 ? "CIDECT eq. 4.12" : "CIDECT eq. 4.11", lhs, rhs);
            c.lines.push_back(fmtn("N1 cos(theta1) + N2 cos(theta2) = %.2f kN <= %.2f kN (Ov = %.0f%% > %.0f%%)", lhs / 1000.0, rhs / 1000.0, ov, limit));
            checks.push_back(c);
        }
        Check c = rangeMin("Sobreposicao minima Ov >= 25%", "CIDECT 4.3 (validade)", ov, 25.0, "%");
        c.lines.push_back(fmtn("Ov = %.0f%%", ov));
        checks.push_back(c);
    }
    return checks;
}

std::vector<Part> partsTubularJoint(const TubularJointInput &in)
{
    std::vector<Part> parts;
    const bool plate = in.braceKind == TubularJointInput::TransversePlate;
    const bool isK = in.kind == TubularJointInput::KGap || in.kind == TubularJointInput::KOverlap;
    auto weldPart = [&](double d, double t, double theta, const char *name) {
        Part weld;
        // contorno de intersecao aproximado: pi d / sen(theta) (diagonal inclinada alonga a elipse)
        const double length = plate ? 2.0 * (d + t) : kPi * d * (1.0 + 0.5 * (1.0 / std::max(0.2, std::sin(rad(theta))) - 1.0));
        weld.description = std::string("Solda de composicao ao redor da ") + name + fmtn(": garganta >= t = %.1f mm", t);
        weld.quantity = 1;
        weld.length = length;
        weld.unit = "mm";
        weld.massEach = 0.5 * t * t * length / 1e9 * kSteelDensity;
        parts.push_back(weld);
    };
    weldPart(in.d1, in.t1, in.theta1, plate ? "chapa" : "diagonal 1");
    if (isK) {
        weldPart(in.d2, in.t2, in.theta2, "diagonal 2");
    }
    return parts;
}

} // namespace Structura::Design::Connections
