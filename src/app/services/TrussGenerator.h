#pragma once

#include <array>
#include <vector>

namespace Structura::App::Services {

/**
 * Gerador de treliças planas: combina o perfil dos banzos (paralelas, duas águas, uma água, arco, tesoura)
 * com o arranjo da alma (Pratt, Howe, Warren, Warren com montantes, Fink e K).
 * A geometria é gerada no plano (x ao longo do vão, z para cima) e depois posicionada pelo chamador.
 */
class TrussGenerator
{
public:
    enum class Profile { Parallel, Gable, Shed, Bowstring, Scissors };
    enum class Web { Pratt, Howe, Warren, WarrenVerticals, Fink, K };
    enum class Role { BottomChord, TopChord, Vertical, Diagonal };

    struct Params {
        Profile profile {Profile::Parallel};
        Web web {Web::Pratt};
        double span {12.0};
        double height {2.0};        ///< altura máxima (no meio, ou na ponta alta na uma água)
        double endHeight {0.0};     ///< altura nos apoios (duas águas, uma água, arco e tesoura); ignorada nas paralelas
        int panels {6};             ///< painéis; arranjos simétricos exigem número par (é arredondado para cima)
    };

    struct Bar {
        int a {0}, b {0};
        Role role {Role::Diagonal};
    };

    struct Result {
        std::vector<std::array<double, 2>> nodes;   ///< (x, z)
        std::vector<Bar> bars;
        int panels {0};   ///< painéis efetivamente usados
    };

    [[nodiscard]] static Result generate(const Params &params);
    [[nodiscard]] static const char *profileName(Profile profile);
    [[nodiscard]] static const char *webName(Web web);
};

} // namespace Structura::App::Services
