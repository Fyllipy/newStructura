#pragma once

// Capacidade MECANICA declarada de uma tipologia (J3): separada da representabilidade da topologia. Uma tipologia so tem capacidade mecanica
// quando existe MODELO FUNDAMENTADO implementado; caso contrario o estado e explicito e justificado (nunca "suportada" por ter geometria).

#include "design/jointengine/Types.h"

#include <string>
#include <vector>

namespace Structura::Design::JointEngine {

struct MechanicalCapability {
    /// `ValidityStatus::ValidWithCaveats` = ha modelo implementado (ver `modelId`); `NotYetAvailable` = sem modelo fundamentado;
    /// `Unsupported` = tipologia desconhecida.
    ValidityStatus status {ValidityStatus::NotYetAvailable};
    std::string modelId;                          ///< modelo disponivel (vazio se nao ha)
    std::string justification;                    ///< por que ha / nao ha
    std::vector<std::string> missingFoundations;  ///< o que falta (fontes/modelos), quando indisponivel
    bool normativeResistanceChecks {false};       ///< existe verificacao normativa de RESISTENCIA no programa (nao e modelo constitutivo)
};

} // namespace Structura::Design::JointEngine
