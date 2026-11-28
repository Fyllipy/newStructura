#pragma once

// INCREMENTO 4: liga uma ligação DEFINIDA (NodeConnection) ao par viga–pilar do nó avaliado no Incremento 1 e compara a classificação normativa da ligação real
// (resposta M–θ do Incremento 3 + `JointClassification`) com o comportamento requerido. Só orquestra: não calcula critério algum (ver `design/jointengine/RealVsRequired`).

#include "app/ProjectDataTypes.h"
#include "app/services/ConnectionMechanicalResponseService.h"
#include "design/jointengine/RealVsRequired.h"

namespace Structura::App::Services {

class ConnectionRequirementService
{
public:
    /// O par é identificado pela viga da ligação (`memberId`) e, quando há, pela barra de apoio vinculada (`supMember`). Ambiguidade ou ausência do par
    /// resulta em "Indeterminado" com a lacuna declarada (nenhum par é escolhido por palpite).
    [[nodiscard]] static Structura::Design::JointEngine::ConnectionRequirementComparison evaluate(const Structura::App::ProjectData::NodeConnection &connection,
                                                                                                  const Structura::Design::JointEngine::RequiredBehaviorAssessment &assessment,
                                                                                                  const ConnectionMechanicalResult &mechanical);

    /// INCREMENTO 5: os dois sentidos (cada um com a rigidez e Mj,Rd do seu sentido) e o resultado global conservador (`combineDirections`).
    [[nodiscard]] static Structura::Design::JointEngine::DirectionalComparison evaluateBoth(const Structura::App::ProjectData::NodeConnection &connection,
                                                                                             const Structura::Design::JointEngine::RequiredBehaviorAssessment &assessment,
                                                                                             const ConnectionMechanicalBoth &mechanical);
};

} // namespace Structura::App::Services
