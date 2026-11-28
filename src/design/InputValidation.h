#pragma once

// Camada COMPARTILHADA de validacao das entradas do modulo de dimensionamento de ligacoes.
//
// Principio: dado obrigatorio ausente ou invalido NUNCA e substituido por valor padrao e NUNCA faz uma verificacao desaparecer. A validacao
// devolve a lista de problemas; o chamador converte em verificacoes NAO VERIFICAVEL (Check::status = Unverifiable), que impedem a aprovacao
// global da ligacao (ver `summarize` em Connections.h). As regras ficam aqui, uma vez, e nao em cada tipologia.

#include "design/Connections.h"
#include "design/EndPlateConnection.h"
#include "design/SupportChecks.h"

#include <string>
#include <vector>

namespace Structura::Design::Validation {

using Structura::Design::Connections::BoltedInput;
using Structura::Design::Connections::Check;
using Structura::Design::Connections::ColumnPanelInput;
using Structura::Design::Connections::EndPlateInput;
using Structura::Design::Connections::SupportInput;
using Structura::Design::Connections::WeldInput;

struct Issue {
    std::string group;     ///< "pilar", "viga", "chapa", "parafusos", "soldas", "esforcos", ...
    std::string field;     ///< nome do dado
    std::string message;   ///< o que esta errado
};

/// Acumula problemas de validacao.
class Validator
{
public:
    explicit Validator(std::string group) : m_group(std::move(group)) {}
    Validator &group(const std::string &group);
    /// finito (nem NaN nem infinito)
    Validator &finite(const char *field, double value);
    /// finito e > 0
    Validator &positive(const char *field, double value, const char *unit = "mm");
    /// finito e >= 0
    Validator &nonNegative(const char *field, double value, const char *unit = "mm");
    /// a < b (por exemplo, altura do perfil > 2 x espessura da mesa)
    Validator &less(const char *fieldA, double a, const char *fieldB, double b, const char *why);
    /// aco com fy > 0, fu > 0 e fu >= fy
    Validator &steel(const char *prefix, double fy, double fu);
    /// condicao arbitraria
    Validator &require(bool condition, const char *field, const std::string &message);

    [[nodiscard]] bool ok() const { return m_issues.empty(); }
    [[nodiscard]] const std::vector<Issue> &issues() const { return m_issues; }
    void append(const std::vector<Issue> &issues);

private:
    std::string m_group;
    std::vector<Issue> m_issues;
};

/// Converte os problemas em verificacoes NAO VERIFICAVEL, uma por grupo (a razao lista cada dado), no padrao dos demais resultados.
std::vector<Check> toUnverifiableChecks(const std::vector<Issue> &issues, const std::string &clause);

std::vector<Issue> validate(const BoltedInput &input);
std::vector<Issue> validate(const WeldInput &input);
/// Perfil de apoio (so quando `face != None`).
std::vector<Issue> validate(const SupportInput &input);
std::vector<Issue> validate(const ColumnPanelInput &input);
/// Ligacao rigida por chapa de extremidade: viga, chapa, parafusos, soldas, PILAR (geometria e aco), esforcos e coeficientes.
std::vector<Issue> validate(const EndPlateInput &input);

} // namespace Structura::Design::Validation
