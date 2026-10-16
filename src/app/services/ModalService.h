#pragma once

#include "app/SolverBridge.h"

namespace Structura::App::Services {

/**
 * @brief Analise modal (vibracao livre): frequencias, periodos, participacao de massa e formas modais.
 *
 * A massa vem das cargas gravitacionais do modelo combinado (carga vertical / g) e, quando o modelo nao tem caso
 * de peso proprio, da massa especifica dos materiais. Unidades coerentes com o projeto (kN-m ou N-m).
 */
class ModalService
{
public:
    static constexpr double kGravity = 9.80665;

    /// Massas nodais concentradas (id do no, massa) a partir das cargas verticais (-Z) do modelo.
    static std::vector<std::pair<int, double>> massesFromLoads(const Structura::App::SolverBridge::ModelData &model);

    /// Resolve o modelo pedindo `modes` modos. `includeMaterialMass`: usa a massa especifica dos materiais (peso proprio).
    static Structura::App::SolverBridge::Result run(Structura::App::SolverBridge::ModelData model, int modes, bool includeMaterialMass);
};

} // namespace Structura::App::Services
