#pragma once

// INCREMENTO 1 — Comportamento estrutural REQUERIDO de um nó viga-pilar, ANTES de existir qualquer ligação física.
//
// DISTINÇÃO TÉCNICA (obrigatória): este módulo NÃO classifica a rigidez de uma ligação real (isso é a classificação normativa da etapa 5, que exige a
// configuração física e a resposta M–θ). Aqui só se determina, a partir do MODELO ESTRUTURAL:
//   (1) a idealização que a análise global ADOTA no nó (rotulado / semirrígido / rígido) e de onde ela vem (declarada pelo projetista por liberação
//       ou mola de extremidade, ou implícita: continuidade padrão do modelo — que NÃO é uma decisão declarada);
//   (2) as METAS de rigidez que uma ligação física teria de atingir para SUSTENTAR essa idealização segundo a NBR 8800:2024, 6.1.2.1 e 6.1.2.2
//       (Si <= 0,5 EIv/Lv para rotulada; Si >= 25 EIv/Lv para rígida, esta só admitida se Kv/Kp >= 0,1 no andar), calculadas com a viga do modelo;
//   (3) o que falta no modelo para concluir (dados insuficientes) — sem inventar valores.
// Os critérios numéricos de 6.1.2 são reutilizados de `JointClassification` (limitsFor, storeyStiffness); nada é reimplementado.
// Esforços solicitantes NÃO entram: nenhum critério da NBR 8800:2024, 6.1.2 depende deles; sistema estrutural e contraventamento não constam do modelo (lacuna).
// Sem dependência de Qt ou de interface.

#include "design/JointClassification.h"
#include "design/jointengine/Types.h"

#include <array>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace Structura::Design::JointEngine {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

enum class NodeMemberRole { Beam, Column, Diagonal, Unclassified };
[[nodiscard]] std::string nodeMemberRoleName(NodeMemberRole role);

/// Condição de rotação da extremidade da barra neste nó, em torno do eixo normal ao plano de análise.
enum class RotationalCondition {
    ContinuousImplicit,   ///< sem liberação nem mola: continuidade PADRÃO do modelo (não é uma declaração do projetista)
    Released,             ///< liberação de rotação declarada (articulação)
    Spring,               ///< mola rotacional de extremidade declarada
    Unknown
};
[[nodiscard]] std::string rotationalConditionName(RotationalCondition c);

struct NodeMemberFacts {
    int memberId {0};
    NodeMemberRole role {NodeMemberRole::Unclassified};
    std::string label;                  ///< rótulo informado pelo chamador (ex.: nome do perfil); pode ser vazio
    double lengthM {kNaN};              ///< comprimento eixo a eixo dos nós (m)
    double inertiaM4 {kNaN};            ///< I no plano de análise (m4)
    double youngKnM2 {kNaN};            ///< E (kN/m2)
    RotationalCondition condition {RotationalCondition::Unknown};
    double springKnm {kNaN};            ///< kN.m/rad, se `Spring`
    bool hasEccentricity {false};       ///< offset de eixo ou zona rígida neste nó
    std::array<double, 3> direction {{0, 0, 0}};   ///< unitário, saindo do nó
};

/// Par de barras do nó (cada par viga–pilar é uma ligação distinta). Os limites e o andar vêm do contexto já calculado pelo serviço do modelo.
struct PairFacts {
    int beamId {0};
    int otherId {0};
    Structura::Design::JointClass::JointKind kind {Structura::Design::JointClass::JointKind::Other};
    RotationalCondition beamCondition {RotationalCondition::Unknown};   ///< rotação da extremidade da viga neste nó, no plano DESTE par
    double beamSpringKnm {kNaN};                     ///< k (kN.m/rad) se `beamCondition == Spring`
    bool limitsAvailable {false};                    ///< E, Iv e Lv da viga de referência disponíveis e positivos
    Structura::Design::JointClass::Limits limits;    ///< EIv/Lv, 0,5 EIv/Lv, 25 EIv/Lv (kN.m/rad)
    Structura::Design::JointClass::StoreyStiffness storey;   ///< Kv/Kp do andar (6.1.2.2)
    std::vector<std::string> notes;                  ///< hipóteses/limitações da extração (plano, eixos, painel de alma...)
};

struct NodeFacts {
    int nodeId {0};
    std::array<double, 3> position {{0, 0, 0}};
    std::array<int, 6> restraints {{0, 0, 0, 0, 0, 0}};   ///< apoio no nó (não zero = restrito)
    std::vector<NodeMemberFacts> members;
    std::vector<PairFacts> pairs;
    std::string standard {"NBR 8800"};
    std::string edition {"2024"};
    std::string standardOrigin;                      ///< de onde vem a norma (ex.: "padrão do programa")
    /// DECLARAÇÃO do projetista nas propriedades do projeto (0 = não declarado; nenhum valor padrão é assumido).
    int structuralSystem {0};                        ///< 1 pórtico de vigas e pilares; 2 galpão/um pavimento; 3 treliça; 4 outro
    int bracing {0};                                 ///< 1 contraventada; 2 não contraventada (estabilidade lateral pelos pórticos)
    std::vector<std::string> modelNotes;
};

enum class RequiredKind { Rigid, SemiRigid, Pinned, Undetermined };
[[nodiscard]] std::string requiredKindName(RequiredKind k);

enum class RequirementBasis {
    ModelImplicitContinuity,   ///< continuidade padrão do modelo: NÃO é decisão declarada do projetista
    ModelRelease,              ///< liberação de rotação declarada
    ModelSpring,               ///< mola rotacional declarada
    NotApplicable,             ///< par que não é viga–pilar: o critério de 6.1.2 não se aplica
    InsufficientData
};
[[nodiscard]] std::string requirementBasisName(RequirementBasis b);

/// 6.1.2.2: a meta Si >= 25 EIv/Lv só é admitida se Kv/Kp >= 0,1 no andar.
enum class RigidAdmissibility { Admissible, NotAdmissible, Unknown };
[[nodiscard]] std::string rigidAdmissibilityName(RigidAdmissibility a);

/// Rótulo do estado para o usuário (a interface não mostra nomes internos).
[[nodiscard]] std::string statusLabelPt(ValidityStatus status);

/// Compatibilidade qualitativa do comportamento requerido com a estabilidade lateral declarada (sem limiares numéricos inventados).
enum class SystemConsistency { Undetermined, Consistent, NeedsAttention };
[[nodiscard]] std::string systemConsistencyName(SystemConsistency c);

struct RequiredBehaviorItem {
    int beamId {0};
    int otherId {0};
    Structura::Design::JointClass::JointKind kind {Structura::Design::JointClass::JointKind::Other};
    RequiredKind required {RequiredKind::Undetermined};
    RequirementBasis basis {RequirementBasis::InsufficientData};
    ValidityStatus status {ValidityStatus::InsufficientData};   ///< Valid | ValidWithCaveats | InsufficientData | Unsupported
    std::optional<double> springKnm;
    double referenceKnm {kNaN};      ///< EIv/Lv (kN.m/rad)
    double pinnedMaxKnm {kNaN};      ///< 0,5 EIv/Lv: meta máxima de Si para a ligação ser considerada rotulada
    double rigidMinKnm {kNaN};       ///< 25 EIv/Lv: meta mínima de Si para a ligação ser considerada rígida
    double kvKp {kNaN};
    bool limitsAvailable {false};    ///< E, Iv e Lv da viga disponíveis (limites calculados)
    Structura::Design::JointClass::Limits limits;            ///< os mesmos limites usados nas metas (reaproveitados pela classificação da ligação real, Incremento 4)
    Structura::Design::JointClass::StoreyStiffness storey;   ///< Kv/Kp do andar usado
    RigidAdmissibility rigidAdmissibility {RigidAdmissibility::Unknown};
    SystemConsistency systemConsistency {SystemConsistency::Undetermined};
    std::string systemNote;          ///< justificativa da compatibilidade com o sistema/contraventamento declarados
    std::string statement;           ///< frase de orientação
    std::vector<std::string> criteria;      ///< critérios aplicados (norma e item)
    std::vector<std::string> assumptions;   ///< hipóteses adotadas
    std::vector<std::string> gaps;          ///< o que falta no modelo / decisão necessária
    std::vector<std::string> notes;
};

struct RequiredBehaviorAssessment {
    int nodeId {0};
    ValidityStatus status {ValidityStatus::InsufficientData};   ///< pior estado entre os itens (ou InsufficientData sem itens)
    std::string standard;
    std::string edition;
    int structuralSystem {0};
    int bracing {0};
    std::vector<NodeMemberFacts> members;
    std::vector<RequiredBehaviorItem> items;
    std::vector<std::string> nodeGaps;
    std::vector<std::string> nodeNotes;
    /// Memória de classificação em texto (UTF-8), com dados, critérios, hipóteses e lacunas.
    [[nodiscard]] std::string memory() const;
};

/// Determina o comportamento requerido do nó a partir dos fatos do modelo. Determinístico; sem efeitos colaterais; não usa nenhuma ligação física.
[[nodiscard]] RequiredBehaviorAssessment assessRequiredBehavior(const NodeFacts &facts);

} // namespace Structura::Design::JointEngine
