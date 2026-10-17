#pragma once

#include "design/Connections.h"

#include <vector>

namespace Structura::Design::Connections {

/// Elemento de apoio de uma ligação de viga ou diagonal (pilar ou viga principal de perfil I/H) e a força que a ligação lhe transmite.
struct SupportInput {
    enum Face { None = 0, Flange = 1, Web = 2 };
    int face {None};          ///< onde a chapa está soldada: face externa da mesa ou face da alma
    double d {0.0};           ///< altura do perfil de apoio (mm)
    double bf {0.0};
    double tf {0.0};
    double tw {0.0};
    double k {0.0};           ///< mesa + raio de concordância com a alma (mm); 0 = 1,5 tf
    double fy {250.0};
    double fu {400.0};
    bool atMemberEnd {true};  ///< a ligação está na extremidade do perfil de apoio (sem continuidade para o outro lado)
    double tension {0.0};     ///< força normal à face que tende a arrancá-la (N)
    double compression {0.0}; ///< força normal à face que a comprime (N)
    double lengthAlong {0.0};      ///< comprimento de atuação ao longo do perfil de apoio, n (mm)
    double lengthTransverse {0.0}; ///< dimensão da área carregada transversal ao perfil (mm), p.ex. espessura da chapa mais soldas
    double unbracedLength {0.0};   ///< ℓ: maior comprimento destravado lateralmente na seção da força (mm); 0 = mesas contidas (5.7.5 dispensada)
    double gammaA1 {1.10};
};

/// Estados-limite locais do perfil de apoio: flexão local da mesa, escoamento local, enrugamento e flambagem lateral da alma
/// (NBR 8800:2024, 5.7.2 a 5.7.5) para a ligação na mesa; flexão transversal da alma (linhas de plastificação) para a ligação na alma.
std::vector<Check> checkSupportMember(const SupportInput &input);

} // namespace Structura::Design::Connections
