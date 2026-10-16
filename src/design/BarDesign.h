#pragma once

#include "design/DesignSection.h"
#include "design/DesignTypes.h"
#include "design/Nbr14762.h"
#include "design/Nbr8800.h"

#include <string>
#include <vector>

namespace Structura::Design {

/// Esforcos solicitantes de calculo em uma secao. Convencao: N > 0 e tracao; Mx > 0 comprime a fibra +b
/// (topo); My > 0 comprime a fibra +a; Vy e a cortante paralela a b (alma), Vz a paralela a a.
struct Effects {
    double N {0.0};
    double Mx {0.0};
    double My {0.0};
    double Vy {0.0};
    double Vz {0.0};
    double T {0.0};
};

struct StationEffects {
    double x {0.0};      ///< posicao ao longo da barra (mm)
    Effects e;
};

/// Combinacao ultima: esforcos nas secoes de verificacao e informacao sobre cargas transversais (para Cm).
struct ComboEffects {
    std::string name;
    std::vector<StationEffects> stations;   ///< ordenadas por x, de 0 a L
    bool transverseLoadX {false};           ///< ha cargas transversais no plano de flexao em torno de X (plano x-y local)
    bool transverseLoadY {false};
    double b2 {1.0};                        ///< amplificacao global B2 desta combinacao (Anexo C)
};

struct BarDesignInput {
    const DesignSection *section {nullptr};
    SteelGrade steel;
    double length {0.0};                    ///< mm
    double Kx {1.0}, Ky {1.0}, Kz {1.0};    ///< coeficientes de flambagem (comprimento efetivo = K L)
    double lxOverride {0.0}, lyOverride {0.0}, lzOverride {0.0};   ///< >0: comprimento de flambagem informado (mm), substitui K L
    int lateralSegments {1};                ///< numero de trechos destravados da mesa comprimida (>= 1)
    bool compressionFlangeBraced {false};   ///< mesa comprimida contida continuamente (sem FLT)
    double lbOverride {0.0};                ///< >0: comprimento destravado informado (mm)
    bool cbEqualsOne {false};               ///< Cb = 1,0 (balancos, etc.)
    bool amplifyB1 {true};                  ///< amplificacao local B1 (Anexo C)
    double b2 {1.0};                        ///< amplificacao global B2 aplicada aos momentos
    bool slendernessGoverns {true};         ///< limite de esbeltez entra no aproveitamento
    double netAreaRatio {1.0};              ///< An/Ag
    double ct {1.0};
    Standard rectangularTubeStandard {Standard::NBR14762};
    Nbr8800::Options opt8800;
    Nbr14762::Options opt14762;
    std::vector<ComboEffects> combos;
};

struct BarDesignOutput {
    bool supported {false};
    std::string standard;                   ///< "NBR 8800:2024" ou "NBR 14762:2010"
    double maxRatio {0.0};
    std::string governingCheck;             ///< id do estado-limite que governa
    std::string governingCombo;
    double governingX {0.0};                ///< mm
    std::vector<CheckResult> checks;        ///< pior ocorrencia de cada verificacao
    std::vector<std::string> warnings;
    std::vector<std::string> summaryLines;  ///< propriedades e parametros usados
};

BarDesignOutput designBar(const BarDesignInput &input);

} // namespace Structura::Design
