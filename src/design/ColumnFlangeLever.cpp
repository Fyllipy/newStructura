#include "design/ColumnFlangeLever.h"

#include "design/ConnectionHelpers.h"
#include "design/TStub.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace Structura::Design::Connections {

using detail::fmtn;
namespace TS = Structura::Design::TStub;

bool isColumnFlangeLeverCheck(const Check &check)
{
    return check.origin == CheckOrigin::ColumnFlangeLeverNbr || check.origin == CheckOrigin::ColumnFlangeLeverEn || check.origin == CheckOrigin::DivergenceWarning;
}

NbrPrying nbrPrying(double b, double a, double p, double db, double df, double fu, double gammaA1, double boltRd, double boltTension)
{
    NbrPrying r;
    const auto ok = [](double v) { return std::isfinite(v) && v > 0.0; };
    if (!ok(b) || !ok(a) || !ok(p) || !ok(db) || !ok(df) || !ok(fu) || !ok(gammaA1) || !ok(boltRd) || !ok(boltTension)) {
        r.error = "dados invalidos (b, a, p, db, df, fu, gamma_a1, Ft,Rd ou Ft,0,Sd nao positivos)";
        return r;
    }
    r.bEff = b - 0.5 * db;
    if (r.bEff <= 0.0) {
        r.error = "geometria invalida: b <= 0,5 db (parafuso encostado na alma)";
        return r;
    }
    r.delta = 1.0 - df / p;
    if (!(r.delta > 0.0)) {
        r.error = "delta = 1 - df/p <= 0 (largura tributaria p menor que o furo): geometria invalida";
        return r;
    }
    r.aEff = std::min(a, 1.25 * b);
    r.beta = ((r.aEff + 0.5 * db) / r.bEff) * (boltRd / boltTension - 1.0);
    r.tRigid = std::sqrt(4.0 * r.bEff * boltTension * gammaA1 / (p * fu));
    if (r.beta < 0.0) {
        r.boltInadequate = true;
        r.valid = true;
        return r;
    }
    r.alpha = r.beta >= 1.0 ? 1.0 : std::min(1.0, (1.0 / r.delta) * (r.beta / (1.0 - r.beta)));
    r.tRequired = std::sqrt(4.0 * r.bEff * boltTension * gammaA1 / (p * fu * (1.0 + r.delta * r.alpha)));
    r.valid = true;
    return r;
}

double nbrPryingRowResistance(double thickness, double b, double a, double p, double db, double df, double fu, double gammaA1, double boltRd)
{
    double lo = 0.0, hi = boltRd;
    for (int i = 0; i < 100; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (mid <= 0.0) {
            break;
        }
        const NbrPrying r = nbrPrying(b, a, p, db, df, fu, gammaA1, boltRd, mid);
        if (r.valid && !r.boltInadequate && r.tRequired <= thickness) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return 2.0 * lo;
}

namespace {

Check complementary(const std::string &name, const std::string &clause, double demandKN, double resistanceKN, CheckOrigin origin)
{
    Check c;
    c.origin = origin;
    c.name = name;
    c.clause = clause;
    c.status = CheckStatus::Complementary;
    c.demand = demandKN;
    c.resistance = resistanceKN;
    c.unit = "kN";
    return c;
}

struct Layer {
    bool valid {false};
    double resistance {0.0};   ///< N, mesa do pilar
};

} // namespace

std::vector<Check> columnFlangeLeverChecks(const EndPlateInput &in, double ft)
{
    std::vector<Check> out;
    const int rows = std::max(1, in.tensionRows);
    if (!(ft > 0.0)) {
        out.push_back(notApplicableCheck(kLeverNbrName, "NBR 8800:2024, 6.3.5.4", "nao ha tracao nos parafusos (M/z + N/2 <= 0): o efeito de alavanca da mesa do pilar nao tem demanda"));
        out.back().origin = CheckOrigin::ColumnFlangeLeverNbr;
        return out;
    }
    const double dh = standardHoleDiameter(in.db);
    const double ab = 0.25 * 3.14159265358979 * in.db * in.db;
    const double ftRdNbr = 0.75 * ab * in.grade.fub / in.gammaA2;   // NBR 8800:2024, 6.3.3.1
    const double bLiteral = (in.gauge - in.colWebThickness) / 2.0;   // H1 aprovada: b literal, sem desconto de 0,8 r
    const double aEdge = (in.colFlangeWidth - in.gauge) / 2.0;

    // ---------------------------------------------------------------- Camada A (NBR): tudo o que for inserido ate o fim deste bloco e Camada A
    std::optional<double> resistanceA;
    if (rows != 1) {
        out.push_back(unverifiableCheck(kLeverNbrName, "NBR 8800:2024, 6.3.5.4",
                                        "ligacao com " + std::to_string(rows) + " fileiras tracionadas: a interacao entre fileiras ainda nao foi validada por MEF independente; a soma de larguras tributarias nao e usada para aprovar (metodologia aprovada: apenas 1 fileira)",
                                        {"6.3.5.4 aplicado a mesa do pilar esta liberado somente para 1 fileira tracionada",
                                         "Use a Camada B (T-stub EN, complementar e informativa) apenas como indicacao; ela nao substitui esta verificacao"}));
    } else {
        const double p = 3.5 * bLiteral;   // 6.3.5.2: p = 2 min(e_ext ; 1,75 b), borda do pilar suficientemente afastada (H2)
        const double ft0 = ft / 2.0;
        const NbrPrying r = nbrPrying(bLiteral, aEdge, p, in.db, dh, in.column.fu, in.gammaA1, ftRdNbr, ft0);
        if (!r.valid) {
            out.push_back(unverifiableCheck(kLeverNbrName, "NBR 8800:2024, 6.3.5.4", r.error));
        } else if (r.boltInadequate) {
            Check c = detail::ratioN(kLeverNbrName, "NBR 8800:2024, 6.3.5.2", ft0, ftRdNbr);
            c.lines.push_back(fmtn("beta = %.3f < 0 (Ft,Rd = %.2f kN < Ft,0,Sd = %.2f kN): o parafuso deve ser alterado (6.3.5.2)", r.beta, ftRdNbr / 1e3, ft0 / 1e3));
            out.push_back(c);
        } else {
            Check c = detail::minimumRule(kLeverNbrName, "NBR 8800:2024, 6.3.5.4", in.colFlangeThickness, r.tRequired);
            c.lines.push_back(fmtn("Mesa do pilar tratada como placa flexivel da Figura 15 (1 fileira tracionada, 2 parafusos, um de cada lado da alma)"));
            c.lines.push_back(fmtn("b = (g - tw)/2 = (%.1f - %.1f)/2 = %.2f mm [b LITERAL, sem desconto de 0,8 r]; a = min((bf - g)/2 ; 1,25 b) = %.2f mm; p = 2 x min(e_ext ; 1,75 b) = 3,5 b = %.1f mm", in.gauge, in.colWebThickness, bLiteral, r.aEff, p));
            c.lines.push_back(fmtn("Ft,Rd = 0,75 Ab fub / ga2 = %.2f kN; Ft,0,Sd = (M/z + N/2)/2 = %.2f kN; df = %.0f mm; delta = 1 - df/p = %.3f", ftRdNbr / 1e3, ft0 / 1e3, dh, r.delta));
            c.lines.push_back(fmtn("beta = [(a + 0,5 db)/(b - 0,5 db)] (Ft,Rd/Ft,0,Sd - 1) = %.3f; alfa = %.3f", r.beta, r.alpha));
            c.lines.push_back(fmtn("t >= sqrt[4 (b - 0,5 db) Ft,0,Sd ga1 / (p fu (1 + delta alfa))] = %.2f mm (mesa tf = %.2f mm; fu = %.0f MPa; ga1 = %.2f)", r.tRequired, in.colFlangeThickness, in.column.fu, in.gammaA1));
            c.lines.push_back(fmtn("Se a mesa fosse rigida (6.3.5.3), exigiria t >= %.2f mm", r.tRigid));
            c.lines.push_back("Hipoteses complementares (nao textuais da NBR): H1 b literal (sem filete); H2 bordas e fileiras vizinhas do pilar suficientemente afastadas (p = 3,5 b); aplicacao do 6.3.5.4 a mesa de perfil laminado com alma e filete.");
            c.lines.push_back("Limitacao: validado so para 1 fileira; contra o T-stub EN e MEF publicado a razao varia (ver docs/METODOLOGIA_ALAVANCA_MESA_PILAR.md, 4.3).");
            out.push_back(c);
            const double ra = nbrPryingRowResistance(in.colFlangeThickness, bLiteral, aEdge, p, in.db, dh, in.column.fu, in.gammaA1, ftRdNbr);
            if (ra > 0.0) {
                resistanceA = ra;
            }
        }
    }

    // ---------------------------------------------------------------- Camada B (EN, informativa)
    const TS::EnCoefficients coef;
    std::string reason;
    const double as = TS::boltStressArea(in.db);
    const double fub = in.grade.fub;
    if (as <= 0.0) {
        reason = "diametro do parafuso nao tabelado (M12 a M36) para a area resistente As";
    }
    TS::ColumnFlangeInput cf;
    cf.m = bLiteral - 0.8 * in.colRadius;   // EN Figura 6.8: m = (g - tw)/2 - 0,8 r
    cf.e = aEdge;
    cf.emin = std::min(aEdge, (in.plateWidth - in.gauge) / 2.0);
    cf.tf = in.colFlangeThickness;
    cf.fy = in.column.fy;
    const TS::HeadWidth head = TS::boltHeadWidth(in.db);
    cf.ew = head.acrossPoints / 4.0;
    cf.boltFtRd = TS::boltTensionResistance(fub, as, coef);
    cf.rows = rows;
    cf.rowSpacings.assign(static_cast<std::size_t>(rows - 1), in.pitch);
    TS::ColumnFlangeResult col;
    if (reason.empty()) {
        col = TS::columnFlange(cf, coef, TS::Method::Two);
        if (!col.valid) {
            reason = col.error;
        }
    }
    for (Check &layerA : out) {
        layerA.origin = CheckOrigin::ColumnFlangeLeverNbr;
    }
    Check b = complementary(kLeverEnName, "EN 1993-1-8, 6.2.4 e 6.2.6.4 (metodo complementar)", ft / 1e3, 0.0, CheckOrigin::ColumnFlangeLeverEn);
    b.lines.push_back("METODO COMPLEMENTAR (EN 1993-1-8): nao e exigencia da NBR 8800 e NAO participa da aprovacao global.");
    if (!reason.empty()) {
        b.reason = "nao calculado: " + reason;
        b.lines.push_back("T-stub EN nao calculado: " + reason);
        out.push_back(b);
        return out;
    }
    double total = col.total;
    std::string governingComponent = "mesa do pilar";
    double plateResistance = -1.0;
    if (rows == 1) {
        TS::Geometry g;
        g.m = in.firstRow - 0.8 * in.weldFlangeLeg;
        g.e = in.edgeVertical;
        g.emin = std::min(in.edgeVertical, (in.plateWidth - in.gauge) / 2.0);
        g.ew = cf.ew;
        const TS::Modes plate = TS::resistance(g, TS::endPlateExtensionRow(g.m, in.edgeVertical, (in.plateWidth - in.gauge) / 2.0, in.gauge, in.plateWidth), in.plateThickness, {in.plate.fy},
                                               {cf.boltFtRd, 2}, coef, TS::Method::Two, "Chapa de extremidade, fileira na extensao");
        if (plate.valid) {
            plateResistance = plate.resistance;
            if (plate.resistance < total) {
                total = plate.resistance;
                governingComponent = "chapa de extremidade";
            }
            for (const std::string &line : plate.lines) {
                col.lines.push_back(line);
            }
        } else {
            col.lines.push_back("Chapa de extremidade (T-stub): nao calculada: " + plate.error);
        }
    }
    b.resistance = total / 1e3;
    b.lines.push_back(fmtn("Coeficientes EN usados: gammaM0 = %.2f; gammaM2 = %.2f; k2 = %.2f; metodo 2 da Tabela 6.2 (nao ha coeficientes da NBR neste metodo).", coef.gammaM0, coef.gammaM2, coef.k2));
    b.lines.push_back(fmtn("Parafuso: Ft,Rd = k2 fub As / gM2 = %.1f kN (fub = %.0f MPa; As = %.0f mm2)", cf.boltFtRd / 1e3, fub, as));
    if (in.grade.name.find("ASTM") != std::string::npos) {
        b.lines.push_back("Parafuso ASTM: a EN nao tem essa classe; fub informado pelo usuario e usado no metodo EN sob responsabilidade do projetista.");
    }
    b.lines.push_back(fmtn("m = (g - tw)/2 - 0,8 r = %.2f mm; e = (bf - g)/2 = %.2f mm; emin = %.2f mm; ew = dw/4 = %.2f mm", cf.m, cf.e, cf.emin, cf.ew));
    if (!head.tabulated) {
        b.lines.push_back(fmtn("ATENÇÃO — VALOR ESTIMADO (hipótese empírica, não é dado normativo nem de catálogo): dw (largura entre vértices da cabeça) do parafuso de %.0f mm não consta da tabela de larguras entre faces; adotado dw = 1,5 db x 1,1547 = %.1f mm. Confirme com o catálogo do parafuso; ew = dw/4 depende deste valor (método 2).", in.db, head.acrossPoints));
    }
    for (const std::string &line : col.lines) {
        b.lines.push_back(line);
    }
    b.lines.push_back("Resistencia a tracao do conjunto (componente governante: " + governingComponent + ") = " +
                      fmtn("%.1f kN; tracao solicitante M/z + N/2 = %.1f kN; razao informativa = %.2f", total / 1e3, ft / 1e3, ft / total));
    b.lines.push_back("Nao incluidos nesta versao: alma do pilar tracionada (EN 6.2.6.3), alma da viga tracionada (6.2.6.8), mesa enrijecida (Tabela 6.5)" + std::string(rows > 1 ? "; chapa de extremidade em multiplas fileiras (Tabela 6.6, fileiras alem da primeira)." : "."));
    out.push_back(b);

    // ---------------------------------------------------------------- advertencia de divergencia (R_A >= 1,10 R_B)
    if (resistanceA && rows == 1 && col.valid && !col.rows.empty()) {
        const double rb = col.total;   // mesa do pilar (mesmo componente da camada A)
        if (rb > 0.0 && *resistanceA >= 1.10 * rb) {   // condicao inalterada: R_A >= 1,10 R_B
            // INFORMATIVA: sem demanda, sem resistencia de ligacao e sem indice de utilizacao (resistance = 0 => nenhum numero e exibido como verificacao)
            Check w = complementary(kLeverWarnName, "informativa", 0.0, 0.0, CheckOrigin::DivergenceWarning);
            const auto comma = [](std::string s) {
                for (char &ch : s) {
                    if (ch == '.') {
                        ch = ',';
                    }
                }
                return s;
            };
            const std::string message = "Divergência entre metodologias: Camada A (NBR) = " + comma(fmtn("%.2f", *resistanceA / 1e3)) + " kN; Camada B (EN) = " + comma(fmtn("%.2f", rb / 1e3)) +
                                        " kN. A estimativa da Camada A é " + comma(fmtn("%.1f", (*resistanceA / rb - 1.0) * 100.0)) +
                                        "% superior à da Camada B. Resultado informativo, sem influência na aprovação global.";
            w.reason = message;
            w.lines.push_back(message);
            w.lines.push_back("Camada A: NBR 8800:2024, 6.3.5.4 (tração total da fileira admitida pela espessura da mesa). Camada B: EN 1993-1-8, T-stub, método 2 (mesa do pilar). Os valores são estimativas de resistência da mesa do pilar, não demanda e resistência da ligação.");
            w.lines.push_back("As metodologias seguem normas diferentes (formulações, coeficientes e calibrações distintos): não é uma comparação de fatores de segurança. Revise a mesa do pilar com critério de engenharia.");
            out.push_back(w);
        }
    }
    return out;
}

} // namespace Structura::Design::Connections
