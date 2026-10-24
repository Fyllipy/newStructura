#pragma once

#include "design/BarDesign.h"

#include <string>
#include <vector>

namespace Structura::Design {

/// Contexto numerico (ja calculado em `designBar`) das verificacoes adicionais de vigas da NBR 8800:2024.
struct BeamExtraContext {
    double length {0.0};          ///< L da barra (mm)
    int segments {1};             ///< trechos destravados da mesa comprimida
    double lb {0.0};              ///< comprimento destravado (mm)
    double mxRdBare {0.0};        ///< Mx,Rd do perfil sem lamela (N.mm), no comprimento destravado e Cb adotados
    double vyRd {0.0};            ///< V_Rd na direcao da altura (N)
    double gammaA1 {1.10};
    double gammaA2 {1.35};
    double gammaW2 {1.35};
    std::string clauseTag;        ///< "NBR 8800:2024"
};

/// 5.4.5 (solda mesa-alma de perfil soldado), 5.4.4 (lamelas sobrepostas a mesas) e 4.11.5 (comprimento destravado entre rotulas plasticas):
/// verificacoes por envoltoria das combinacoes de `input`. Vazio quando o item nao se aplica ou nao foi informado.
std::vector<CheckResult> beamExtraChecks(const BarDesignInput &input, const BeamExtraContext &context);

/// Comprimento destravado maximo entre rotulas plasticas (4.11.5): secoes I (alinea a) ou caixao/tubular retangular/macica (alinea b); m1OverM2 positivo em curvatura reversa.
double plasticHingeUnbracedLength(bool boxSection, double e, double fy, double ry, double m1OverM2);

} // namespace Structura::Design
