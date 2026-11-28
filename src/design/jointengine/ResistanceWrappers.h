#pragma once

// P1 (F-SOL / F-GUS): WRAPPERS de resistencia do motor sobre as verificacoes legadas `SupportWeld`, `TrussConnections` (gusset) e `TubularJoints`.
// Cada wrapper DELEGA ao legado (nenhuma formula e duplicada) e apenas reorganiza o resultado com PROVENIENCIA explicita por verificacao.
// Os resultados sao identicos aos do legado (TestJointEngineResistanceWrappers: comparacao exata). NAO e modelo constitutivo: sem rigidez,
// sem lei F-delta/M-theta, sem ICR, sem distribuicao entre barras. A condicao de fundamentacao de cada verificacao e preservada: um wrapper que
// funciona NAO torna a verificacao "normativamente validada" e, sem conferencia com a norma, ela NAO e elegivel a aprovacao pelo motor.

#include "design/SupportWeld.h"
#include "design/TrussConnections.h"
#include "design/TubularJoints.h"
#include "design/jointengine/Descriptor.h"

#include <map>
#include <string>
#include <vector>

namespace Structura::Design::JointEngine {

/// Nivel de fundamentacao de uma formulacao (ordem crescente de confianca; `Unavailable` = sem fundamentacao).
enum class FormulationProvenance {
    ReproducedFromCode,      ///< reproduzida do codigo legado; texto da norma NAO conferido nesta fase
    PartiallyDocumented,     ///< parte confrontada com fonte lida (ver `provenanceNote`), restante nao conferido
    ConfrontedWithStandard,  ///< confrontada com o texto da norma/edicao identificada
    DomainVerified,          ///< alem de confrontada, com dominio de validade verificado
    Unavailable
};
[[nodiscard]] std::string provenanceName(FormulationProvenance p);
/// Somente formulacoes confrontadas com a norma podem ter RESISTENCIA confrontada (condicao necessaria, nao suficiente, para elegibilidade).
[[nodiscard]] inline bool approvalEligible(FormulationProvenance p)
{
    return p == FormulationProvenance::ConfrontedWithStandard || p == FormulationProvenance::DomainVerified;
}

/// Base normativa do metodo. A norma brasileira de referencia e a NBR 8800:2024; metodos complementares NAO sao conformidade NBR.
enum class StandardBasis { Nbr8800_2024, ComplementaryAisc, ComplementaryCidect, EnIndependent, Unclassified };
[[nodiscard]] std::string basisName(StandardBasis b);

/// Origem da DEMANDA usada na verificacao (separada da validade da resistencia).
enum class DemandOrigin {
    ProvidedInput,        ///< a demanda e um dado de entrada (nenhuma idealizacao dentro da verificacao)
    IdealizedByLegacy,    ///< distribuicao/idealizacao de esforcos feita pelo legado, NAO confrontada com a norma
    DerivedAndConfronted  ///< derivada por procedimento confrontado com a norma
};
[[nodiscard]] std::string demandOriginName(DemandOrigin d);

struct WrappedCheck {
    std::string name;
    std::string clause;
    std::string unit;
    double demand {0.0};
    double resistance {0.0};
    double ratio {0.0};                                   ///< `Check::ratio()` do legado, sem reinterpretacao
    Structura::Design::Connections::CheckStatus status {Structura::Design::Connections::CheckStatus::Evaluated};
    std::string reason;
    // --- fundamentacao em quatro eixos independentes ---
    FormulationProvenance provenance {FormulationProvenance::ReproducedFromCode};   ///< da formulacao de RESISTENCIA
    std::string provenanceNote;
    StandardBasis basis {StandardBasis::Unclassified};
    DemandOrigin demandOrigin {DemandOrigin::IdealizedByLegacy};
    std::string demandNote;
    bool conditionsVerified {false};                      ///< condicoes de aplicacao/entradas obrigatorias/validade verificadas
    std::string conditionsNote;
    /// Resistencia confrontada E demanda nao idealizada E condicoes verificadas, em relacao a PROPRIA fonte (inclui metodos complementares).
    bool confrontedWithOwnSource {false};
    /// Elegivel a aprovacao pelo motor: SO se a base e a NBR 8800:2024 e a verificacao inteira esta confrontada.
    bool approvalEligible {false};
    std::string eligibilityReason;
    std::vector<std::string> lines;                       ///< memoria do legado, sem alteracao
};

struct ResistanceReport {
    std::string modelId;
    std::string modelVersion;
    ResultKind kind {ResultKind::NormativeResistance};   ///< natureza da grandeza; a elegibilidade a aprovacao e por verificacao (`approvalEligible`)
    std::vector<WrappedCheck> checks;
    std::vector<std::string> excluded;                    ///< verificacoes legadas NAO incluidas (sem correspondencia inequivoca), com motivo
    std::map<std::string, Quantity> parameters;           ///< parametros relevantes com origem (ex.: K de Whitmore)
    std::vector<std::string> hypotheses;
    std::vector<std::string> limitations;
    std::vector<std::string> sourcesRead;                 ///< fontes efetivamente lidas que sustentam notas de proveniencia
};

class WeldSupportResistance
{
public:
    static constexpr const char *kId = "WeldSupportResistance";
    static constexpr const char *kVersion = "1";
    [[nodiscard]] static const ModelDescriptor &descriptor();
    [[nodiscard]] static ResistanceReport evaluate(const Structura::Design::Connections::SupportWeldInput &input);
};

class GussetResistance
{
public:
    static constexpr const char *kId = "GussetResistance";
    static constexpr const char *kVersion = "1";
    [[nodiscard]] static const ModelDescriptor &descriptor();
    /// Parametros da coluna de Whitmore (K, comprimento, limite de largura) sao REGISTRADOS (origem/hipotese), nao alterados.
    [[nodiscard]] static ResistanceReport evaluate(const Structura::Design::Connections::GussetInput &input);
};

class TubularResistance
{
public:
    static constexpr const char *kId = "TubularResistance";
    static constexpr const char *kVersion = "1";
    [[nodiscard]] static const ModelDescriptor &descriptor();
    /// Inclui so verificacoes com equacao/limite CIDECT numerado e inequivoco; as demais ficam em `excluded`.
    [[nodiscard]] static ResistanceReport evaluate(const Structura::Design::Connections::TubularJointInput &input);
};

} // namespace Structura::Design::JointEngine
