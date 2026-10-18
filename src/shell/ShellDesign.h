#pragma once

#include "shell/ShellResults.h"

#include <string>
#include <vector>

namespace Structura::Shell {

/// Uma verificacao agrupada por zona (parte do objeto com a mesma espessura): demanda, limite e razao.
struct ZoneCheck {
    std::string zone;
    std::string criterion;
    double demand {0.0};
    double limit {0.0};
    double ratio {0.0};
    std::string unit;
    std::string reference;   ///< norma / item
    std::string note;
};

struct DesignOutput {
    std::vector<ZoneCheck> checks;
    std::vector<double> elementRatio;                 ///< razao governante por elemento
    std::vector<std::array<double, 4>> reinforcement; ///< concreto: As (cm2/m) face+ dir.a, face+ dir.b, face- dir.a, face- dir.b
    std::vector<std::string> notes;
    double maxRatio {0.0};
    bool hasConcrete {false};
    [[nodiscard]] bool ok() const { return maxRatio <= 1.0 + 1e-9; }
};

/// Verifica as cascas com os resultados das combinacoes ultimas (`ultimate`, cada uma ja combinada com seus coeficientes):
///  - aco: tensao equivalente de von Mises <= fy/1,10 (NBR 8800); vasos de pressao: intensidade de tensao de membrana <= S e
///    de membrana + flexao + secundarias <= 3 S, com S = min(fu/3,5 ; fy/1,5) (criterio de projeto de vasos tipo ASME VIII Div. 1, nao consta em normas/);
///  - concreto (NBR 6118:2023): momentos de projeto de Wood-Armer, armadura de flexao (retangular, x/d <= 0,45), armadura minima (Tabelas 17.3 e 19.1)
///    e cisalhamento sem armadura transversal (19.4.1).
/// `service` (opcional) traz a combinacao de servico usada para a flecha maxima.
DesignOutput designShells(const Mesh &mesh, const Model &model, const std::vector<CaseResult> &ultimate, const CaseResult *service);

/// Taxa minima de armadura de flexao rho_min (Tabela 17.3 da NBR 6118:2023, CA-50, retangular), em fracao (0,00150...).
double minimumFlexureRatio(double fckMPa);

/// Armadura de flexao (m2 por metro) para o momento de projeto `md` (N.m/m): retorna false se x/d > limite (secao insuficiente).
bool flexureReinforcement(double md, double h, double d, double fck, double fyk, double gammaC, double gammaS, double &as, double &xd);

/// Cisalhamento resistente sem armadura (NBR 6118:2023, 19.4.1), N por metro de largura.
double slabShearResistance(double fck, double d, double rho1, double gammaC);

} // namespace Structura::Shell
