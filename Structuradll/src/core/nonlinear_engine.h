#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <Eigen/Dense>
#include <Eigen/Sparse>

#include "links.h"
#include "member.h"
#include "node.h"
#include "solver_types.h"

// Motor de analise estatica nao linear incremental (Newton-Raphson com passos de carga ou de deslocamento).
//
// Fontes de nao linearidade: rotulas plasticas bilineares nas extremidades das barras (com interacao opcional N-M), ligacoes entre nos com leis
// nao lineares (so tracao/compressao, folga, elastoplastica, multilinear) e, se pedido, os efeitos de segunda ordem (rigidez geometrica das barras
// atualizada a cada iteracao). Os demais elementos (barras elasticas, cascas, molas, vinculos por penalidade) entram como parte linear.
struct NonlinearContext {
    const SolverInput& input;
    std::vector<std::shared_ptr<Node>>& nodes;
    std::vector<std::shared_ptr<Member>>& members;
    const std::unordered_map<int, int>& nodeIndexById;
    const std::vector<int>& restrainedDof;
    int totalDof = 0;
    const Eigen::SparseMatrix<double>& linearStiffness;   // cascas, ligacoes por penalidade, molas, vinculos (sem barras e sem ligacoes nao lineares)
    Eigen::VectorXd externalLoad;                          // cargas de referencia (fator de carga 1); sem a parte flexivel das barras com rotulas plasticas
    LinkSet& links;
    std::vector<std::string>& warnings;
    // Carga de referencia completa (fator 1), incluindo a parte flexivel das barras com rotulas, que o motor aplica por dentro do equilibrio local da barra:
    // define a escala dos criterios de convergencia e a direcao da carga no controle de deslocamento.
    Eigen::VectorXd referenceLoad;
};

struct NonlinearOutcome {
    Eigen::VectorXd displacement;
    Eigen::VectorXd internalForce;   // forcas internas totais (barras, ligacoes, parte linear) no estado final, no fator de carga alcancado
    double loadFactor = 0.0;
    bool completed = true;           // false: o ultimo fator pedido nao foi alcancado (resultados do ultimo passo confirmado)
    int steps = 0;
    int iterations = 0;
    std::vector<std::array<double, 2>> curve;
};

// Executa a analise; lanca std::runtime_error se nem o menor passo converge. Deixa em cada barra com rotulas os esforcos finais
// (Member::overrideInternalForces) e atualiza o estado plastico confirmado das barras e ligacoes.
NonlinearOutcome runNonlinearAnalysis(NonlinearContext& context, bool secondOrder);
