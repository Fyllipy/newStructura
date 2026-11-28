#pragma once

// Efeito de alavanca na mesa do pilar em ligacoes por chapa de extremidade (item A2 da auditoria).
// Metodologia aprovada: docs/METODOLOGIA_ALAVANCA_MESA_PILAR.md; arquitetura: docs/NUCLEO_COMPARTILHADO_COMPONENTES_TSTUB.md.
//
//  Camada A - NBR 8800:2024, 6.3.5.2 a 6.3.5.4 (decide a aprovacao; so UMA fileira tracionada; multiplas fileiras = NAO VERIFICAVEL).
//             Coeficientes e criterios exclusivamente da NBR. NAO usa TStub::EnCoefficients.
//  Camada B - EN 1993-1-8, T-stub (metodo 2). Informativa (CheckStatus::Complementary): nunca altera Summary/maxRatio.

#include "design/Connections.h"
#include "design/EndPlateConnection.h"

#include <string>
#include <vector>

namespace Structura::Design::Connections {

inline constexpr const char *kLeverNbrName = "Efeito de alavanca - mesa do pilar (NBR 8800:2024, 6.3.5.4)";
inline constexpr const char *kLeverEnName = "T-stub EN 1993-1-8 - METODO COMPLEMENTAR (nao participa da aprovacao global)";
inline constexpr const char *kLeverWarnName = "Divergência entre metodologias (informativa)";

/// Verificacoes introduzidas pelo A2 (as da camada A, da B e a advertencia). Usado para preservar consumidores que varrem os checks.
bool isColumnFlangeLeverCheck(const Check &check);

/// NBR 8800:2024, 6.3.5.2 e 6.3.5.4 para uma placa/mesa. Entradas em N e mm.
struct NbrPrying {
    bool valid {false};
    std::string error;
    double aEff {0.0};       ///< min(a ; 1,25 b)
    double bEff {0.0};       ///< b - 0,5 db
    double delta {0.0};
    double beta {0.0};
    double alpha {0.0};
    double tRequired {0.0};  ///< 6.3.5.4
    double tRigid {0.0};     ///< 6.3.5.3
    bool boltInadequate {false};   ///< beta < 0
};
NbrPrying nbrPrying(double b, double a, double p, double db, double df, double fu, double gammaA1, double boltRd, double boltTension);
/// Maior tracao total da fileira (2 parafusos) admitida pelo 6.3.5.4 para a espessura dada (N); 0 se nao ha como determinar.
double nbrPryingRowResistance(double thickness, double b, double a, double p, double db, double df, double fu, double gammaA1, double boltRd);

/// Verificacoes A2 para a mesa do pilar com a tracao total `tensionForce` (N) na mesa tracionada da viga.
/// Pre-condicao: as entradas de `input` ja foram validadas (checkEndPlate).
std::vector<Check> columnFlangeLeverChecks(const EndPlateInput &input, double tensionForce);

} // namespace Structura::Design::Connections
