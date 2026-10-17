#pragma once

#include <array>
#include <cmath>

namespace Structura::App::Services {

/**
 * Geometria da barra de apoio de uma ligacao no no: eixo da barra (ponto do eixo, ja com a excentricidade da secao),
 * eixos locais y' (altura) e z' (largura) e dimensoes da secao (mm).
 */
struct SupportSection {
    std::array<double, 3> axisPoint {};
    std::array<double, 3> y {};
    std::array<double, 3> z {};
    double depth {0.0};
    double width {0.0};
    double webThickness {0.0};
    bool isIShape {false};   ///< perfil I/H: a viga que chega pela largura apoia-se na face da alma
};

/// Distancia, medida ao longo de `s` (unitario, do no para dentro da viga), do eixo da viga ate a face do apoio que a viga encontra:
/// face externa da mesa (viga chegando pela altura do perfil) ou face da alma (I/H) / borda (demais) quando chega pela largura.
/// `beamOrigin`: ponto do eixo da viga no no. `modelPerMm`: unidades do modelo por mm. Devolve false se a viga e quase paralela
/// a face (cosseno < 0,2) ou se as dimensoes do apoio sao nulas.
inline bool supportFaceOffset(const std::array<double, 3> &beamOrigin, const std::array<double, 3> &s, const SupportSection &support, double modelPerMm,
                              double &offset)
{
    auto dot = [](const std::array<double, 3> &a, const std::array<double, 3> &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
    const bool onFlange = std::abs(dot(s, support.y)) >= std::abs(dot(s, support.z));
    const auto &axis = onFlange ? support.y : support.z;
    const double half = onFlange ? support.depth / 2.0 : (support.isIShape ? support.webThickness / 2.0 : support.width / 2.0);
    const double cosine = std::abs(dot(s, axis));
    if (cosine < 0.2 || half <= 0.0) {
        return false;
    }
    const double facing = dot(s, axis) >= 0.0 ? 1.0 : -1.0;   // normal da face voltada para a viga
    const std::array<double, 3> rel {beamOrigin[0] - support.axisPoint[0], beamOrigin[1] - support.axisPoint[1], beamOrigin[2] - support.axisPoint[2]};
    // plano da face: n.(X - C) = half; X = beamOrigin + lambda s
    offset = (half * modelPerMm - facing * dot(axis, rel)) / cosine;
    return true;
}

} // namespace Structura::App::Services
