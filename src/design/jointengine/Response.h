#pragma once

// Resposta mecanica tipada, proveniencia, classificacao e estrategia de montagem (J0). Contratos conceituais do D0 (API_CONTRACT) sem
// implementacao de solver: `IAssemblyStrategy` e so a interface. A curva M-theta e UM dos tipos de resposta, nao a unica.

#include "design/jointengine/Component.h"
#include "design/jointengine/Descriptor.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Structura::Design::JointEngine {

enum class ResponseKind { MomentRotation, ForceDisplacement, GeneralizedVector, ResistanceOnly, StiffnessOnly };
[[nodiscard]] std::string responseKindName(ResponseKind kind);

struct Curve {
    std::string actionDof;   ///< id do GDL da acao (ex.: "M")
    std::string actionUnit;
    std::string dispUnit;
    std::vector<std::pair<double, double>> points;   ///< (deslocamento, acao), crescente no deslocamento
};

struct Stiffness {
    double value {std::numeric_limits<double>::quiet_NaN()};
    std::string unit;
};

struct ResistanceItem {
    std::string name;
    double value {std::numeric_limits<double>::quiet_NaN()};
    std::string unit;
    ResultKind kind {ResultKind::NormativeResistance};
    bool governing {false};
};

struct ConvergenceInfo {
    bool applicable {false};   ///< falso para respostas fechadas (sem iteracao)
    bool converged {false};
    int iterations {0};
    double residual {std::numeric_limits<double>::quiet_NaN()};
};

struct Provenance {
    std::string modelId;
    std::string modelVersion;
    std::string standard;
    std::string edition;
    std::map<std::string, Quantity> parameters;
    std::vector<std::string> assumptions;
    std::vector<std::string> limitations;
    std::string configurationFingerprint;
    std::string analysisFingerprint;
    std::string solverStrategy;
};

struct MechanicalResponse {
    ResponseKind kind {ResponseKind::ResistanceOnly};
    DofSpace dofs;
    ValidityStatus status {ValidityStatus::InsufficientData};
    std::vector<Curve> curves;
    std::optional<Stiffness> initialStiffness;
    std::optional<Stiffness> tangentStiffness;
    std::optional<Stiffness> secantStiffness;
    std::vector<ResistanceItem> resistances;
    std::optional<Quantity> deformationCapacity;   ///< so quando o modelo a fornece (a curva EN atual NAO tem)
    std::string governingMechanism;
    Provenance provenance;
    ConvergenceInfo convergence;
    std::vector<std::string> hypotheses;
    std::vector<std::string> limitations;
    std::vector<Message> messages;

    /// Invariante: sem estado que autorize numeros, nao ha curvas, rigidezes nem resistencias.
    [[nodiscard]] bool consistent() const
    {
        if (allowsNumbers(status)) {
            return true;
        }
        return curves.empty() && !initialStiffness && !tangentStiffness && !secantStiffness && resistances.empty() && !deformationCapacity;
    }
};

// ---------------------------------------------------------------------------------------------------------------- classificacao

enum class Verdict { Atende, NaoAtende, Indeterminado, NaoVerificavel };
[[nodiscard]] std::string verdictName(Verdict verdict);

/// Comportamento PRETENDIDO pelo projetista (objeto proprio; nao compartilha variavel com o calculado nem com a classificacao).
struct DesiredBehavior {
    enum class Kind { Rigid, SemiRigid, Pinned, Custom };
    Kind kind {Kind::Custom};
    std::vector<std::string> requirements;
};

struct ClassificationContext {
    std::string standard;
    std::string edition;
    std::string jointKind;
    std::map<std::string, Quantity> structural;   ///< parametros estruturais usados (Kb, Kc, contraventamento...)
};

struct ClassificationResult {
    Verdict verdict {Verdict::NaoVerificavel};
    std::string classLabel;
    std::string justification;
    std::vector<std::string> criteria;
    std::string standard;
    std::string edition;
};

struct BehaviorComparison {
    Verdict verdict {Verdict::NaoVerificavel};
    std::vector<std::string> deviations;
};

class IClassifier
{
public:
    virtual ~IClassifier() = default;
    [[nodiscard]] virtual ClassificationResult classify(const MechanicalResponse &response, const ClassificationContext &context) const = 0;
};

// ------------------------------------------------------------------------------------------------------------------- montagem

struct AssemblyNode {
    int id {0};
    DofSpace dofs;
};

struct AssemblyEdge {
    enum class Kind { Component, Interface, Mechanism };
    int id {0};
    Kind kind {Kind::Component};
    int nodeA {0};
    int nodeB {0};
    std::string modelId;   ///< descritor do modelo que governa a aresta
};

/// Montagem mecanica. A topologia de transmissao e DECLARADA (nunca inferida como serie/paralelo).
struct Assembly {
    std::string configurationFingerprint;
    std::vector<AssemblyNode> nodes;
    std::vector<AssemblyEdge> edges;
};

struct ActionSet {
    std::map<std::string, Quantity> actions;
};

/// Controles de solucao: nenhum default. Estrategias fechadas (S1) os ignoram; as nao lineares os exigirao.
struct SolveControls {
    std::optional<int> maxIterations;
    std::optional<double> tolerance;
    std::optional<int> incrementSteps;
};

class IAssemblyStrategy
{
public:
    virtual ~IAssemblyStrategy() = default;
    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual Applicability applicable(const Assembly &assembly) const = 0;
    [[nodiscard]] virtual MechanicalResponse solve(const Assembly &assembly, const ActionSet &actions, const SolveControls &controls) const = 0;
};

} // namespace Structura::Design::JointEngine
