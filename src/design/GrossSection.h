#pragma once

#include "design/DesignTypes.h"

#include <vector>

namespace Structura::Design {

/// Propriedades geometricas da secao bruta, obtidas por integracao do contorno (mm, mm2, mm4, mm3).
/// Eixos centroidais: `a` horizontal (largura) e `b` vertical (altura). X e o eixo horizontal
/// (flexao no plano da altura), Y o vertical.
struct GrossSection {
    bool valid {false};
    double area {0.0};
    double Ix {0.0};        ///< integral de b^2 dA
    double Iy {0.0};        ///< integral de a^2 dA
    double Ixy {0.0};       ///< integral de a*b dA
    double aMin {0.0}, aMax {0.0}, bMin {0.0}, bMax {0.0};
    double WxTop {0.0};     ///< Ix / bMax (fibra superior)
    double WxBottom {0.0};  ///< Ix / |bMin|
    double WyRight {0.0};   ///< Iy / aMax
    double WyLeft {0.0};    ///< Iy / |aMin|
    double Zx {0.0};        ///< modulo plastico em relacao ao eixo X
    double Zy {0.0};
    double rx {0.0};
    double ry {0.0};
    double pnaX {0.0};      ///< posicao b da linha neutra plastica (eixo X)
    double pnaY {0.0};      ///< posicao a da linha neutra plastica (eixo Y)
    double centroidA {0.0}; ///< centroide dos contornos de entrada (deslocamento aplicado)
    double centroidB {0.0};

    [[nodiscard]] double Wx() const { return WxTop < WxBottom ? WxTop : WxBottom; }
    [[nodiscard]] double Wy() const { return WyRight < WyLeft ? WyRight : WyLeft; }
};

/// Calcula as propriedades brutas. `loops[0]` e o contorno externo; os demais sao furos.
/// A origem e deslocada para o centroide.
GrossSection computeGrossSection(const std::vector<Loop> &loops);

} // namespace Structura::Design
