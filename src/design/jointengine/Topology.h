#pragma once

// TOPOLOGIA DE REFERENCIA da ligacao (J1, D2): partes, uniões (hiperarestas de N partes) e caminhos de carga DECLARADOS. Estrutura separada de
// `NodeConnection::values`. Nada aqui e inferido em silencio: dado nao derivavel vira `DataGap`; caminho de carga exige `basis` (fundamento).
// Serie/paralelo NUNCA e presumido.

#include "design/jointengine/Materials.h"
#include "design/jointengine/Types.h"

#include <optional>
#include <string>
#include <vector>

namespace Structura::Design::JointEngine {

enum class PartKind { Member, Plate, Angle, TStub, Stiffener, Diaphragm, Gusset, BasePlate, Bracket, Tube, Concrete, Grout, Anchor, Auxiliary };
[[nodiscard]] std::string partKindName(PartKind kind);

struct PartRef {
    int id {0};
    PartKind kind {PartKind::Auxiliary};
    int memberId {0};          ///< id externo da barra (0 = nao e barra ou desconhecido)
    std::string role;          ///< papel declarado ("beam", "column", "support", "diagonal", "chord", ... ou "unclassified")
    std::optional<MaterialSpec> material;   ///< concreto/graute/aco como referencia tipada (propriedades so se informadas)
    std::string note;
};

enum class LinkKind { Bolted, Welded, Contact, Bearing, Pin, Embedded, Rigid };
[[nodiscard]] std::string linkKindName(LinkKind kind);

/// Uniao entre N >= 2 partes. `Aggregate` = as partes participam da uniao mas o PAREAMENTO exato nao foi derivado (dado ausente declarado).
struct Link {
    enum class Resolution { Pairwise, Aggregate };
    int id {0};
    LinkKind kind {LinkKind::Bolted};
    std::vector<int> partIds;
    Resolution resolution {Resolution::Pairwise};
    std::string note;
};

/// Caminho de carga DECLARADO. `basis` (fundamento: norma, esquema da familia, decisao do usuario) e obrigatorio.
struct LoadPath {
    int id {0};
    std::vector<int> linkIds;   ///< sequencia ordenada de uniões atravessadas
    std::string basis;
    std::string description;
};

struct DataGap {
    std::string code;           ///< ex.: "LOAD_PATHS_NOT_DERIVED", "SUPPORT_MEMBER_UNKNOWN"
    std::string description;
};

enum class TopologyStatus { Complete, Partial, Unavailable };
[[nodiscard]] std::string topologyStatusName(TopologyStatus status);

struct JointTopology {
    int configId {0};           ///< id da ligacao legada (NodeConnection::id) ou 0
    std::string familyId;       ///< ex.: "WebShear", "legacy:kGusset"
    TopologyStatus status {TopologyStatus::Unavailable};
    std::vector<PartRef> parts;
    std::vector<Link> links;
    std::vector<LoadPath> paths;
    std::vector<DataGap> gaps;
    std::string unavailableReason;   ///< obrigatorio se `Unavailable`
};

/// Problemas de integridade (vazio = valida): ids unicos; uniões com >= 2 partes existentes e sem repeticao; caminhos so com uniões existentes e
/// com `basis`; `Complete` sem lacunas; `Partial` com lacunas; `Unavailable` com justificativa e sem partes/uniões/caminhos.
[[nodiscard]] std::vector<std::string> validateTopology(const JointTopology &topology);

} // namespace Structura::Design::JointEngine
