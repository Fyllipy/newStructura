#pragma once

// J2: WRAPPER do metodo dos componentes da EN 1993-1-8 (`JointComponents`) como modelo do motor. NAO reescreve nem altera `JointComponents`:
// chama computeStiffness / computeResistance / curveMetrics / momentRotationCurve e apenas organiza o resultado em `MechanicalResponse`.
// Os numeros sao IDENTICOS aos das funcoes originais (garantido por TestJointEngineRegression, comparacao exata).
// Escopo = o do modelo existente: mesas soldadas e chapa de extremidade estendida com fileiras tracionadas; capacidade de rotacao NAO calculada.
// Mj,Rd aqui e sempre o de EN 6.2.7 (nivel A); a regra de fallback para a NBR do `JointComponentService` NAO faz parte deste wrapper.

#include "design/JointComponents.h"
#include "design/jointengine/Response.h"

namespace Structura::Design::JointEngine {

class EnComponentMethodModel
{
public:
    static constexpr const char *kId = "EnComponentMethod";
    static constexpr const char *kVersion = "1";

    [[nodiscard]] static const ModelDescriptor &descriptor();

    /// Resposta M-theta (EN 6.3.1, psi = Tabela 6.8) e resistencias. `curvePoints` e OBRIGATORIO (sem default). Entradas que o modelo EN
    /// rejeita (ok=false) devolvem `InsufficientData` com a mensagem original e NENHUM numero.
    [[nodiscard]] static MechanicalResponse respond(const Structura::Design::JointComponents::ModelInput &input, int curvePoints);
};

} // namespace Structura::Design::JointEngine
