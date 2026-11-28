#pragma once

// Materiais de primeira classe do motor (D3): aco, parafuso, solda, CONCRETO e GRAUTE como referencias tipadas com propriedades
// (valor, unidade, origem). NAO altera `MaterialData` global nem a persistencia. Nenhuma propriedade e inventada: a especificacao
// nasce vazia e o que falta e reportado por `missing`. Os modelos de concreto/ancoragem (contato, arrancamento) sao da faixa F-BAS.

#include "design/jointengine/Types.h"

#include <map>
#include <string>
#include <vector>

namespace Structura::Design::JointEngine {

enum class MaterialKind { Steel, Bolt, Weld, Concrete, Grout, Other };
[[nodiscard]] std::string materialKindName(MaterialKind kind);

struct MaterialSpec {
    MaterialKind kind {MaterialKind::Other};
    std::string name;                               ///< designacao (ex.: "ASTM A572 Gr.50", "C30") - informada, sem default
    std::map<std::string, Quantity> properties;     ///< ex.: "fy", "fu", "E", "fck", "fcd"; chaves livres, sem lista presumida

    /// Propriedade informada (valor finito, unidade e origem) ou nullptr.
    [[nodiscard]] const Quantity *find(const std::string &key) const;
    /// Chaves de `required` que NAO estao informadas.
    [[nodiscard]] std::vector<std::string> missing(const std::vector<std::string> &required) const;
};

} // namespace Structura::Design::JointEngine
