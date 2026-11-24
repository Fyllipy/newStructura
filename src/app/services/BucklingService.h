#pragma once

#include "app/SolverBridge.h"
#include "shell/ShellVisual.h"

#include <array>
#include <vector>

namespace Structura::App::Services {

/**
 * @brief Flambagem elastica linear (autovalores de K x = -lambda Kg x) e geometria das formas modais.
 *
 * O fator critico multiplica as cargas do modelo: Pcr = lambda * P. A analise e elastica e ideal (sem
 * imperfeicoes); serve para estimar comprimentos efetivos e identificar modos de instabilidade global.
 */
class BucklingService
{
public:
    using Vec3 = std::array<double, 3>;

    /// Resolve o modelo pedindo `modes` modos. Lanca SolverBridgeError em caso de falha.
    static Structura::App::SolverBridge::Result run(Structura::App::SolverBridge::ModelData model, int modes);

    /// Polilinhas (uma por barra) da forma modal; a maior translacao nodal vale `maxDisplacement`.
    static std::vector<std::vector<Vec3>> shapeLines(const Structura::App::SolverBridge::ModelData &model,
                                                     const std::vector<double> &shape,
                                                     double maxDisplacement,
                                                     int samplesPerBar = 12);

    /// Malha das cascas (acopladas ao modelo) deformada pela forma modal/de flambagem, colorida pelo deslocamento. `barShape` e `shellShape` sao as formas
    /// devolvidas pelo solver (6 valores por no de barra em `model.nodes` e por no de casca em `model.shell->nodes`); nos de casca fundidos com nos de barra
    /// usam o deslocamento do no de barra. A maior translacao entre os dois conjuntos vale `maxDisplacement`. Vazia se o modelo nao tem cascas.
    static Structura::Shell::RenderMesh shellShapeMesh(const Structura::App::SolverBridge::ModelData &model, const std::vector<double> &barShape,
                                                       const std::vector<double> &shellShape, double maxDisplacement);

    /// Maior translacao nodal da forma (valores normalizados).
    static double maxTranslation(const std::vector<double> &shape);
};

} // namespace Structura::App::Services
