#pragma once

#include "design/DesignSection.h"
#include "design/DesignTypes.h"
#include "design/Nbr8800.h"

#include <string>
#include <vector>

/// Resistencias de calculo de barras de aco formadas a frio conforme a ABNT NBR 14762:2010
/// (metodo da largura efetiva, MLE; flambagem distorcional por analise de estabilidade elastica via
/// faixas finitas). Unidades: mm, N, N*mm, MPa.
namespace Structura::Design::Nbr14762 {

using Nbr8800::Buckling;
using Nbr8800::CompressionData;
using Nbr8800::FlexureData;
using Nbr8800::ShearData;
using Nbr8800::TensionData;

struct Options {
    double gammaTension {1.10};          ///< escoamento da secao bruta (9.6.2 a)
    double gammaRuptureFar {1.35};       ///< ruptura fora da ligacao (9.6.2 b)
    double gammaRuptureConn {1.65};      ///< ruptura na ligacao (9.6.2 c)
    double gammaCompression {1.20};      ///< 9.7.2 e 9.7.3
    double gammaFlexure {1.10};          ///< 9.8.2 e 9.8.3
    double netAreaRatio {1.0};           ///< An/A na regiao da ligacao
    double ct {1.0};
    double maxSlendernessCompression {200.0};   ///< 9.7.4
    double maxSlendernessTension {300.0};       ///< 9.6.3 (recomendacao)
    bool checkDistortional {true};
    bool shearLagReduction {false};
};

/// Elemento plano (linha media) da secao de parede fina.
struct Element {
    std::array<double, 2> p1 {};      ///< extremidade 1 (a, b) da parte plana, mm, origem no centroide
    std::array<double, 2> p2 {};
    double t {0.0};
    bool freeEnd1 {false};            ///< AL com borda livre na extremidade 1
    bool freeEnd2 {false};            ///< AL com borda livre na extremidade 2
    bool lipStiffened {false};        ///< elemento AA com enrijecedor de borda simples na extremidade 2
    double lipLength {0.0};           ///< d (largura do enrijecedor)
    double lipAngleDeg {90.0};
    double nominalWidth {0.0};        ///< b do elemento (largura plana)
    std::string name;
};

/// Modelo de parede fina de uma secao formada a frio (linha media). Todas as grandezas em mm.
struct ThinWalled {
    std::vector<Element> elements;
    std::vector<std::array<double, 2>> pathNodes;   ///< polilinha da linha media (secoes abertas); vazio em tubulares
    double t {0.0};
    bool closed {false};
    double Cw {0.0};
    double x0 {0.0};                  ///< centro de cisalhamento (a, b) em relacao ao centroide
    double y0 {0.0};
    double J {0.0};
};

/// Verificacao dos valores maximos da relacao largura-espessura (Tabela 4, 9.1.2): devolve o maior (b/t)/(b/t)max.
struct GeometryLimits {
    double ratio {0.0};
    std::vector<std::string> lines;
};
GeometryLimits geometryLimits(const DesignSection &s, const SteelGrade &steel);

/// Constroi o modelo de parede fina a partir da secao (apenas formados a frio).
ThinWalled buildThinWalled(const DesignSection &s);

TensionData tension(const DesignSection &s, const SteelGrade &steel, const Options &o);

CompressionData compression(const DesignSection &s, const SteelGrade &steel, const Buckling &len, const Options &o);

/// Momento fletor resistente de calculo (9.8.2): escoamento da secao efetiva (MLE), FLT e flambagem distorcional.
FlexureData flexure(const DesignSection &s, const SteelGrade &steel, Axis axis, bool positiveSideCompressed, double Lb,
                    double Cb, const Options &o);

/// Forca cortante resistente de calculo (9.8.3). `Axis::Y`: paralela a b (alma).
ShearData shear(const DesignSection &s, const SteelGrade &steel, Axis direction, const Options &o);

} // namespace Structura::Design::Nbr14762
