#pragma once

#include "app/ProjectDataTypes.h"

#include <array>
#include <vector>

namespace Structura::App::Services {

/// Cotas (dimensoes) desenhadas direto no modelo: linha de cota paralela ao segmento medido, linhas de chamada e marcas de extremidade.
class DimensionService
{
public:
    using Vec3 = std::array<double, 3>;

    struct Geometry {
        std::vector<std::vector<Vec3>> lines;   ///< polilinhas (linhas de chamada, linha de cota e marcas)
        Vec3 labelPosition {0.0, 0.0, 0.0};     ///< meio da linha de cota
        double length {0.0};                    ///< comprimento medido (m)
    };

    /// Afastamento padrao da linha de cota: perpendicular ao segmento (em planta; nos segmentos verticais, na direcao X), proporcional ao comprimento.
    [[nodiscard]] static Vec3 defaultOffset(const Vec3 &a, const Vec3 &b);

    [[nodiscard]] static Geometry build(const Structura::App::ProjectData::DimensionLine &dimension);
};

} // namespace Structura::App::Services
