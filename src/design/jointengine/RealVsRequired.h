#pragma once

// INCREMENTO 4 — Classificação normativa da ligação EFETIVAMENTE CONFIGURADA e comparação com o comportamento REQUERIDO do nó.
//
// Não há classificador novo: a classe de rigidez vem de `JointClass::analyzeJoint` / `classify` (NBR 8800:2024, 6.1.2) pelo `NbrClassifier` (J2), alimentado com a
// resposta M–θ do Incremento 3 (Sj,ini = rigidez a 2/3 de Mj,Rd da curva) e com os MESMOS limites e Kv/Kp que o Incremento 1 usou para o requerido (guardados no item).
// A comparação só interpreta duas classes já estabelecidas pelos módulos existentes:
//   requerido ∈ {rotulado, semirrígido, rígido, indeterminado}  ×  classe real ∈ {rotulada, semirrígida, rígida, pendente}.
// Resultado em quatro estados DISTINTOS: Atende, Não atende, Indeterminado (dado ausente/pendente), Não aplicável (critério fora do domínio, ex. par que não é viga–pilar).
// Nada é presumido: sem resposta M–θ com Mj,Rd, sem limites (E, Iv, Lv) ou sem requerido determinado o resultado é Indeterminado, com a lacuna declarada.
// Sem dependência de Qt ou de interface.

#include "design/JointClassification.h"
#include "design/jointengine/RequiredBehavior.h"
#include "design/jointengine/Response.h"

#include <string>
#include <vector>

namespace Structura::Design::JointEngine {

enum class ComparisonOutcome { Meets, DoesNotMeet, Indeterminate, NotApplicable };
[[nodiscard]] std::string comparisonOutcomeName(ComparisonOutcome outcome);   ///< "Atende" | "Não atende" | "Indeterminado" | "Não aplicável"

/// Classificação normativa da ligação real (a de `JointClassification`, sem reinterpretação).
struct RealClassification {
    bool available {false};            ///< há Si (a 2/3 de Mj,Rd) calculado a partir da resposta M–θ
    bool applicable {false};           ///< 6.1.2 vale para o par (viga–pilar)
    double initialStiffnessKnm {kNaN}; ///< Sj,ini informado pela resposta mecânica (Incremento 3)
    double stiffnessKnm {kNaN};        ///< Si usado pelo classificador (rigidez a 2/3 de Mj,Rd lida da curva)
    double eta {kNaN};                 ///< Si / (E Iv/Lv)
    Structura::Design::JointClass::StiffnessClass byStiffness {Structura::Design::JointClass::StiffnessClass::NotClassified};   ///< só pela rigidez (6.1.2.1)
    Structura::Design::JointClass::StiffnessClass finalClass {Structura::Design::JointClass::StiffnessClass::NotClassified};    ///< com Kv/Kp (6.1.2.2)
    bool storeyNeeded {false};
    bool storeyMet {false};
    bool storeyPending {false};        ///< Si ≥ 25 EIv/Lv mas Kv/Kp não pôde ser verificado: a classe rígida fica PENDENTE
    double kvKp {kNaN};
    Structura::Design::JointClass::Limits limits;
    std::string classLabel;            ///< "rotulada" | "semirrígida" | "rígida" | "não classificada"
    std::string justification;         ///< a do módulo de classificação
    std::string standard;
    std::string edition;
    std::vector<std::string> criteria;
};

struct ConnectionRequirementComparison {
    ComparisonOutcome outcome {ComparisonOutcome::Indeterminate};
    ValidityStatus status {ValidityStatus::InsufficientData};   ///< Valid | ValidWithCaveats | InsufficientData | OutOfDomain
    int beamId {0};
    int otherId {0};
    std::string directionLabel;        ///< sentido do momento desta comparação ("" = resposta única, Incremento 4)
    RequiredKind required {RequiredKind::Undetermined};
    RequirementBasis basis {RequirementBasis::InsufficientData};
    RealClassification real;
    double springKnm {kNaN};           ///< k do modelo (requerido semirrígido), para a razão informativa
    double stiffnessRatioToSpring {kNaN};
    std::string summary;               ///< frase única para a interface
    std::vector<std::string> reasons;      ///< justificativas (por que atende / não atende / indeterminado)
    std::vector<std::string> hypotheses;   ///< hipóteses adotadas
    std::vector<std::string> gaps;         ///< lacunas (o que falta para concluir)
    std::vector<std::string> caveats;      ///< ressalvas que acompanham o resultado
    /// Memória em texto (UTF-8): requerido, rigidez, critério, classe, resultado, justificativas, hipóteses e lacunas.
    [[nodiscard]] std::string memory() const;
};

/// `response` = resposta mecânica da ligação definida (ou nulo se não há); `unavailableReason` = por que não há (usado quando indisponível).
/// `directionLabel` (opcional) identifica o sentido do momento da resposta (INCREMENTO 5): a classificação usa a rigidez e Mj,Rd DESTE sentido.
/// Determinístico e sem efeitos colaterais.
[[nodiscard]] ConnectionRequirementComparison compareRealWithRequired(const RequiredBehaviorItem &item, const MechanicalResponse *response, const std::string &unavailableReason, const std::string &directionLabel = {});

/// INCREMENTO 5: comparação nos DOIS sentidos do momento. Cada sentido é classificado separadamente (não há média entre rigidezes ou resistências). A NBR 8800:2024, 6.1.2
/// não define regra para reunir as classes direcionais em uma só; o resultado GLOBAL abaixo é uma decisão CONSERVADORA do programa, não uma prescrição normativa:
/// Atende só se os dois sentidos atendem; Não atende se algum não atende; Indeterminado se nenhum não atende mas algum é indeterminado/indisponível; Não aplicável se ambos o são.
struct DirectionalComparison {
    ConnectionRequirementComparison positive;
    ConnectionRequirementComparison negative;
    ComparisonOutcome overall {ComparisonOutcome::Indeterminate};
    ValidityStatus status {ValidityStatus::InsufficientData};
    std::string summary;       ///< frase do resultado global
    std::string rule;          ///< a regra de combinação adotada e sua natureza (decisão do programa)
    [[nodiscard]] std::string memory() const;
};
[[nodiscard]] DirectionalComparison combineDirections(const ConnectionRequirementComparison &positive, const ConnectionRequirementComparison &negative);

} // namespace Structura::Design::JointEngine
