#pragma once

// Descritor que TODO modelo do motor publica: identificacao, formulacao, GDL suportados, parametros obrigatorios com origens admitidas,
// dominio de validade, hipoteses e limitacoes. Um modelo sem descritor completo nao e registravel (`validateDescriptor`).

#include "design/jointengine/Types.h"

#include <map>
#include <string>
#include <vector>

namespace Structura::Design::JointEngine {

enum class DofKind { Force, Moment };

/// Grandeza generalizada (acao) e seu deslocamento conjugado. Ex.: {"N", Force, "x", "kN"} <-> deslocamento axial.
struct Dof {
    std::string id;
    DofKind kind {DofKind::Force};
    std::string axis;
    std::string unit;       ///< unidade da ACAO
    std::string dispUnit;   ///< unidade do deslocamento conjugado
};

struct DofSpace {
    std::vector<Dof> dofs;
    [[nodiscard]] std::size_t size() const { return dofs.size(); }
};

struct ParameterSpec {
    std::string name;
    std::string unit;
    std::vector<Origin> allowedOrigins;   ///< vazio = qualquer origem rastreada
    bool required {true};
    std::string description;
};

/// Regra do dominio de validade (texto + referencia); a verificacao numerica fica no proprio modelo.
struct DomainRule {
    std::string name;
    std::string description;
};

struct ModelDescriptor {
    std::string id;                       ///< ex.: "EnComponentMethod", "Piluso2001"
    std::string version;
    std::string formulation;
    std::vector<std::string> references;  ///< referencia completa (autor, ano, norma/edicao)
    DofSpace supportedDofs;
    std::vector<ParameterSpec> parameters;
    std::vector<DomainRule> domain;
    std::vector<std::string> hypotheses;
    std::vector<std::string> limitations;
    ResultKind resultKind {ResultKind::ModelEstimate};
    bool nonlinear {false};               ///< a resposta depende de estado/historia (arquitetura preparada desde J0)
    bool hasInternalState {false};
};

/// Problemas que impedem registrar o modelo (vazio = descritor completo).
[[nodiscard]] std::vector<std::string> validateDescriptor(const ModelDescriptor &descriptor);

/// Conjunto de parametros efetivos de uma consulta (nome -> grandeza com origem).
using ParameterSet = std::map<std::string, Quantity>;

/// Parametros obrigatorios ausentes ou sem unidade/origem, ou com origem nao admitida (mensagens prontas).
[[nodiscard]] std::vector<Message> checkParameters(const ModelDescriptor &descriptor, const ParameterSet &parameters);

} // namespace Structura::Design::JointEngine
