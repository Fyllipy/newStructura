#pragma once

// Interfaces de modelos mecanicos (J0): tres categorias distintas (D0 §3.3), NAO uma "mola universal".
//  - IDeformableComponent: F = f(u) com estado interno opcional (preparado para comportamento nao linear desde ja);
//  - IResistanceMechanism: capacidade e modo governante (verificacao/ruptura/instabilidade);
//  - IInterfaceModel: transferencia dependente de estado (contato, separacao, atrito).
// Consulta fora do dominio devolve ESTADO, nunca numero extrapolado (`ComponentResponse::unavailable`).

#include "design/jointengine/Descriptor.h"

#include <optional>
#include <string>
#include <vector>

namespace Structura::Design::JointEngine {

using Vector = std::vector<double>;

/// Estado interno opaco de um componente (historia, plastificacao, abertura...). O significado e do modelo.
struct ComponentState {
    Vector internal;
    std::string tag;
};

struct Applicability {
    ValidityStatus status {ValidityStatus::InsufficientData};
    std::vector<Message> messages;
};

/// Resposta de um componente em um ponto de deslocamento. Invariante: `force` so tem valores se `status` os autoriza.
struct ComponentResponse {
    ValidityStatus status {ValidityStatus::InsufficientData};
    Vector force;                           ///< acoes generalizadas (mesma ordem dos GDL do descritor)
    std::optional<Vector> tangent;          ///< matriz tangente linha a linha (n x n), se o modelo a fornece
    std::vector<Message> messages;

    [[nodiscard]] static ComponentResponse unavailable(ValidityStatus status, std::vector<Message> messages)
    {
        ComponentResponse r;
        r.status = status;
        r.messages = std::move(messages);
        return r;
    }
    [[nodiscard]] bool consistent() const { return allowsNumbers(status) || (force.empty() && !tangent.has_value()); }
};

class IDeformableComponent
{
public:
    virtual ~IDeformableComponent() = default;
    [[nodiscard]] virtual const ModelDescriptor &descriptor() const = 0;
    [[nodiscard]] virtual Applicability applicable(const ParameterSet &parameters) const = 0;
    /// F = f(u). `state` e lido e atualizado (modelos sem estado o ignoram).
    [[nodiscard]] virtual ComponentResponse evaluate(const Vector &displacement, ComponentState &state) const = 0;
};

struct MechanismResult {
    ValidityStatus status {ValidityStatus::InsufficientData};
    double resistance {std::numeric_limits<double>::quiet_NaN()};
    std::string unit;
    std::string governingMode;
    ResultKind kind {ResultKind::NormativeResistance};
    std::vector<Message> messages;
};

class IResistanceMechanism
{
public:
    virtual ~IResistanceMechanism() = default;
    [[nodiscard]] virtual const ModelDescriptor &descriptor() const = 0;
    [[nodiscard]] virtual MechanismResult capacity(const ParameterSet &parameters, const Vector &actions) const = 0;
};

enum class InterfaceCondition { Unknown, Open, Closed, Slipping };

struct InterfaceState {
    InterfaceCondition condition {InterfaceCondition::Unknown};
    Vector internal;
};

class IInterfaceModel
{
public:
    virtual ~IInterfaceModel() = default;
    [[nodiscard]] virtual const ModelDescriptor &descriptor() const = 0;
    [[nodiscard]] virtual InterfaceState update(const Vector &relativeDisplacement, const InterfaceState &previous) const = 0;
};

} // namespace Structura::Design::JointEngine
