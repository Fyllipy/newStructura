#pragma once

#include "shell/ShellJob.h"

#include <array>
#include <string>
#include <vector>

namespace Structura::Shell {

/// Resultantes de um elemento nos 4 cantos e no centro (pontos 0..3 cantos, 4 centro), no sistema local:
/// nxx nyy nxy (N/m), mxx myy mxy (N.m/m), qx qy (N/m). m > 0 traciona a face de normal positiva.
struct ElementState {
    std::array<double, 40> v {};
    std::array<double, 9> axes {};   ///< e1, e2, e3 (linhas)
};

struct CaseResult {
    std::string name;
    std::vector<std::array<double, 6>> displacement;   ///< por no
    std::vector<std::array<double, 6>> reaction;       ///< por no (apoios e molas)
    std::vector<ElementState> elements;
};

struct Results {
    std::vector<CaseResult> cases;
    std::string error;
    int springIterations {0};
    double maxWarp {0.0};
    [[nodiscard]] bool ok() const { return error.empty() && !cases.empty(); }
};

/// Combinacao linear de casos (fatores por nome do caso).
CaseResult combine(const Results &results, const std::vector<std::pair<std::string, double>> &factors, const std::string &name);

enum class Quantity {
    DisplacementMagnitude,
    Ux, Uy, Uz,
    MembraneA,        ///< n_aa (direcao de referencia a: circunferencial nas cascas de revolucao, X nas chapas)
    MembraneB,        ///< n_bb
    MembraneAB,
    MembranePrincipalMax,
    MembranePrincipalMin,
    MomentA,
    MomentB,
    MomentAB,
    MomentPrincipalMax,
    MomentPrincipalMin,
    ShearMax,         ///< sqrt(qx2 + qy2)
    StressVonMisesPlus,    ///< tensao equivalente na face + (normal positiva)
    StressVonMisesMinus,
    StressVonMisesMax,     ///< maior das duas faces
    StressMaxPrincipal,    ///< maior tensao principal entre as faces
    StressMinPrincipal
};

struct QuantityInfo {
    Quantity id;
    const char *name;
    const char *unit;
    double scale;   ///< fator de exibicao (ex.: Pa -> MPa)
    bool nodal;     ///< true: valor do deslocamento (por no); false: por elemento
};
const std::vector<QuantityInfo> &quantityList();
const QuantityInfo &quantityInfo(Quantity q);

/// Valor de uma grandeza no ponto `point` (0..3 cantos, 4 centro) do elemento `element`.
double elementQuantity(const Mesh &mesh, const Model &model, const CaseResult &result, std::size_t element, int point, Quantity q);

/// Tensoes (Pa) nas faces do elemento: sigma_a, sigma_b, tau_ab para a face `side` (+1 normal positiva, -1 negativa), nas direcoes de referencia.
struct FaceStress {
    double sa {0.0}, sb {0.0}, tab {0.0};
    double vonMises() const;
    double principalMax() const;
    double principalMin() const;
};
FaceStress faceStress(const MeshQuad &quad, const ElementState &state, int point, int side);

} // namespace Structura::Shell
