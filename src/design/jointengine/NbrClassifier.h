#pragma once

// J2: WRAPPER da classificacao de rigidez da NBR 8800:2024, 6.1.2 (`JointClassification`) como `IClassifier`. Nao reimplementa criterio algum:
// converte a `MechanicalResponse` (M-theta + Mj,Rd) em `JointClass::JointBehavior` e chama `JointClass::analyzeJoint`.
// Semantica do veredito em `ClassificationResult`: Atende = classificacao ESTABELECIDA pelo criterio; Indeterminado = pendente de dado (ex.: Kv/Kp);
// NaoVerificavel = criterio nao aplicavel ou sem dados. Atende/NaoAtende em relacao a um comportamento DESEJADO e papel de `BehaviorComparison`.
// Os parametros estruturais (limites, Kv/Kp, tipo da ligacao) sao fornecidos pelo chamador (JointAnalysisService); nada e presumido.

#include "design/JointClassification.h"
#include "design/jointengine/Response.h"

namespace Structura::Design::JointEngine {

class NbrClassifier final : public IClassifier
{
public:
    NbrClassifier(const Structura::Design::JointClass::Limits &limits, const Structura::Design::JointClass::StoreyStiffness &storey,
                  Structura::Design::JointClass::JointKind kind)
        : m_limits(limits), m_storey(storey), m_kind(kind)
    {
    }

    /// Analise nativa (mesmo objeto que `JointClass::analyzeJoint` devolve) a partir da resposta; sentido positivo. Resposta sem curva M-theta
    /// ou sem Mj,Rd gera analise sem dados (nao inventa).
    [[nodiscard]] Structura::Design::JointClass::JointAnalysis analyze(const MechanicalResponse &response) const;

    [[nodiscard]] ClassificationResult classify(const MechanicalResponse &response, const ClassificationContext &context) const override;

private:
    Structura::Design::JointClass::Limits m_limits;
    Structura::Design::JointClass::StoreyStiffness m_storey;
    Structura::Design::JointClass::JointKind m_kind;
};

} // namespace Structura::Design::JointEngine
