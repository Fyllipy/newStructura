#pragma once

// INCREMENTO 1: orientação do comportamento estrutural REQUERIDO de um nó selecionado, antes de existir ligação física. Extrai do modelo (`ModelData`) os
// fatos do nó — barras que chegam, papéis, seções/E/Iv/Lv, condição de rotação das extremidades (liberação, mola ou continuidade padrão), apoio, Kv/Kp do
// andar — reutilizando `JointAnalysisService` (nenhuma geometria é reimplementada) e delega a decisão a `Design::JointEngine::assessRequiredBehavior`.
// Não depende de nenhuma ligação cadastrada; não altera o modelo; não classifica ligação real.

#include "app/SolverBridge.h"
#include "design/jointengine/RequiredBehavior.h"

#include <map>
#include <string>

namespace Structura::App::Services {

class NodeBehaviorService
{
public:
    struct Options {
        std::map<int, std::string> memberLabels;   ///< rótulo por barra (ex.: perfil), opcional
        std::string standard {"NBR 8800"};
        std::string edition {"2024"};
        std::string standardOrigin {"padrão do programa (única norma de classificação de rigidez implementada)"};
        int structuralSystem {0};   ///< declaração do projeto (0 = não declarado): 1 pórtico, 2 galpão, 3 treliça, 4 outro
        int bracing {0};            ///< declaração do projeto (0 = não declarada): 1 contraventada, 2 não contraventada
    };

    /// Fatos do nó. Se o nó não existe ou tem menos de duas barras, `error` recebe o motivo e os fatos ficam parciais.
    [[nodiscard]] static Structura::Design::JointEngine::NodeFacts collectFacts(const Structura::App::SolverBridge::ModelData &model, int nodeId, double forceToKn, const Options &options,
                                                                                std::string *error = nullptr);

    /// Coleta e avalia. Em caso de erro de extração devolve avaliação `InsufficientData` com a lacuna registrada.
    [[nodiscard]] static Structura::Design::JointEngine::RequiredBehaviorAssessment assess(const Structura::App::SolverBridge::ModelData &model, int nodeId, double forceToKn,
                                                                                           const Options &options);
};

} // namespace Structura::App::Services
