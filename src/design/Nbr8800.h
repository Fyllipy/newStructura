#pragma once

#include "design/DesignSection.h"
#include "design/DesignTypes.h"

#include <string>
#include <vector>

/// Resistencias de calculo de barras de aco laminadas, soldadas e tubulares conforme a ABNT NBR 8800:2024.
/// Unidades: mm, N, N*mm, MPa. As referencias de item sao as da edicao 2024.
namespace Structura::Design::Nbr8800 {

struct Options {
    double gammaA1 {1.10};          ///< escoamento e instabilidade (Tabela 3, combinacoes normais)
    double gammaA2 {1.35};          ///< ruptura
    double netAreaRatio {1.0};      ///< An/Ag da regiao da ligacao (1,0 = sem furos)
    double ct {1.0};                ///< coeficiente de reducao da area liquida (5.2.5)
    double maxSlendernessCompression {200.0};   ///< 5.3.7.1 (recomendacao)
    double maxSlendernessTension {300.0};       ///< 5.2.8.1 (recomendacao)
};

/// Comprimentos efetivos de flambagem (mm): flexao em relacao ao eixo X, ao eixo Y e torcao.
struct Buckling {
    double Lx {0.0};
    double Ly {0.0};
    double Lz {0.0};
};

struct TensionData {
    double NtRd {0.0};
    double yieldRd {0.0};
    double ruptureRd {0.0};
    std::vector<std::string> lines;
};

struct CompressionData {
    bool covered {true};
    double NcRd {0.0};
    double Ne {0.0};
    double lambda0 {0.0};
    double chi {1.0};
    double Aef {0.0};
    double slenderness {0.0};   ///< maior KL/r
    std::string mode;           ///< modo de flambagem que governa Ne
    std::vector<std::string> lines;
};

struct FlexureData {
    bool covered {true};
    double MRd {0.0};
    double Mpl {0.0};
    std::string governing;      ///< estado-limite que governa (FLT, FLM, FLA, ...)
    std::vector<std::string> lines;
};

struct ShearData {
    bool covered {true};
    double VRd {0.0};
    std::vector<std::string> lines;
};

struct TorsionData {
    bool covered {false};
    double TRd {0.0};
    std::vector<std::string> lines;
};

TensionData tension(const DesignSection &s, const SteelGrade &steel, const Options &o);

CompressionData compression(const DesignSection &s, const SteelGrade &steel, const Buckling &len, const Options &o);

/// Momento fletor resistente de calculo. `positiveSideCompressed`: a fibra extrema em +b (eixo X) ou +a (eixo Y)
/// esta comprimida. `Lb` = comprimento destravado (0 = totalmente travada) e `Cb` o fator de modificacao.
FlexureData flexure(const DesignSection &s, const SteelGrade &steel, Axis axis, bool positiveSideCompressed, double Lb,
                    double Cb, const Options &o);

/// Forca cortante resistente de calculo na direcao `Axis::Y` (paralela a b: resistida pela alma) ou `Axis::X`.
/// `lv`: distancia entre as secoes de cortante maximo e nulo (tubular circular; 0 = comprimento nao informado).
ShearData shear(const DesignSection &s, const SteelGrade &steel, Axis direction, const Options &o, double lv = 0.0);

/// Momento de torcao resistente (apenas tubulares, 5.5.2.1); `covered == false` nos demais casos.
TorsionData torsion(const DesignSection &s, const SteelGrade &steel, double barLength, const Options &o);

/// Fator de modificacao para momento nao uniforme (5.4.2.3 a): Cb = 12,5 Mmax / (2,5 Mmax + 3 MA + 4 MB + 3 MC) * Rm.
double cbFromQuarterMoments(double mMax, double mA, double mB, double mC, double rm = 1.0);

/// Texto formatado (printf) para memoria de calculo.
std::string fmt(const char *format, ...);

} // namespace Structura::Design::Nbr8800
