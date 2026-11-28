#pragma once

// JOINT MECHANICAL ENGINE (J0): tipos basicos de proveniencia, estados e mensagens. Sem dependencia de Qt nem de interface.
// Principios (D0): nenhum valor e criado por default; dado ausente e ausente (NaN / std::nullopt), nunca zero; toda grandeza tem unidade e ORIGEM.

#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace Structura::Design::JointEngine {

/// Origem de um dado ou parametro (rastreabilidade).
enum class Origin {
    Normative,             ///< valor de norma (norma e edicao em `source`)
    ManufacturerDeclared,  ///< declarado por fabricante
    Test,                  ///< ensaio
    Correlation,           ///< correlacao publicada
    ModelAssumption,       ///< hipotese do modelo (declarada)
    UserProvided,          ///< informado pelo usuario
    Derived                ///< calculado/derivado de outros dados rastreaveis
};
[[nodiscard]] std::string originName(Origin origin);

/// Grandeza com unidade e origem. Construida vazia = AUSENTE (nao e zero). So e "informada" com valor finito, unidade e origem.
struct Quantity {
    double value {std::numeric_limits<double>::quiet_NaN()};
    std::string unit;
    std::optional<Origin> origin;
    std::string source;   ///< referencia (norma/edicao/artigo/arquivo)

    [[nodiscard]] bool provided() const { return std::isfinite(value) && !unit.empty() && origin.has_value(); }
    static Quantity make(double value, std::string unit, Origin origin, std::string source = {})
    {
        Quantity q;
        q.value = value;
        q.unit = std::move(unit);
        q.origin = origin;
        q.source = std::move(source);
        return q;
    }
};

/// Estado de validade de um resultado ou consulta. So `Valid` e `ValidWithCaveats` autorizam numeros.
enum class ValidityStatus {
    Valid,
    ValidWithCaveats,
    OutOfDomain,
    InsufficientData,
    InvalidGeometry,
    MissingMaterialProperties,
    Unsupported,             ///< tipologia/modelo nao suportado (ha justificativa em mensagens)
    NotYetAvailable          ///< topologia representavel, resposta mecanica indisponivel por falta de modelo fundamentado
};
[[nodiscard]] std::string statusName(ValidityStatus status);
[[nodiscard]] inline bool allowsNumbers(ValidityStatus status)
{
    return status == ValidityStatus::Valid || status == ValidityStatus::ValidWithCaveats;
}

/// Natureza do resultado. So `NormativeResistance` pode participar de aprovacao.
enum class ResultKind {
    NormativeResistance,
    ModelUltimate,   ///< estimativa de modelo calibrado (ex.: Piluso 2001): nunca resistencia de calculo
    ModelEstimate,
    Reference        ///< referencia numerica/experimental (MEF externo, ensaio)
};
[[nodiscard]] std::string resultKindName(ResultKind kind);
[[nodiscard]] inline bool mayEnterApproval(ResultKind kind) { return kind == ResultKind::NormativeResistance; }

enum class Severity { Info, Warning, Error };

struct Message {
    Severity severity {Severity::Info};
    std::string code;   ///< identificador estavel (ex.: "MISSING_PARAMETER")
    std::string text;
};

} // namespace Structura::Design::JointEngine
