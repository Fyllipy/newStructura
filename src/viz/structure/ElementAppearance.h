#pragma once

#include <QHash>
#include <QUuid>

#include <array>

namespace Structura::Visualization {

/// Aparencia de um no/barra na viewport: estado (normal, esmaecido, oculto) e cor base opcional.
struct ElementAppearance {
    enum class State { Normal, Dimmed, Hidden };

    State state {State::Normal};
    bool hasColor {false};
    std::array<unsigned char, 3> color {{0, 0, 0}};
};

using AppearanceMap = QHash<QUuid, ElementAppearance>;

/// Mistura `color` com o fundo da viewport (usado para esmaecer).
inline std::array<unsigned char, 3> dimColor(const std::array<unsigned char, 3> &color)
{
    constexpr double keep = 0.28;
    constexpr std::array<double, 3> background {{208.0, 220.0, 237.0}};
    std::array<unsigned char, 3> result {};
    for (std::size_t i = 0; i < 3; ++i) {
        result[i] = static_cast<unsigned char>(color[i] * keep + background[i] * (1.0 - keep));
    }
    return result;
}

} // namespace Structura::Visualization
