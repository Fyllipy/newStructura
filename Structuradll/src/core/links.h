#pragma once

#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

#include <Eigen/Dense>
#include <Eigen/Sparse>

#include "solver_types.h"

class Node;

// Ligacoes entre dois nos (molas/elementos de interface) com lei de forca-deformacao independente em cada um dos 6 graus de liberdade relativos.
// A deformacao do grau de liberdade k e a projecao do deslocamento (ou da rotacao) relativo do no J em relacao ao I no eixo local k.
// Leis elastoplasticas guardam o estado plastico confirmado; as demais (tracao/compressao, folga, multilinear) sao elasticas nao lineares.
class LinkSet {
public:
    LinkSet(const SolverInput& input, const std::unordered_map<int, int>& nodeIndexById, const std::vector<std::shared_ptr<Node>>& nodes);

    [[nodiscard]] bool empty() const { return links_.empty(); }
    // Alguma lei nao linear (exige o motor incremental)
    [[nodiscard]] bool nonlinear() const { return nonlinear_; }

    // Forcas internas (vetor global) e rigidez tangente para os deslocamentos `u`; guarda o estado de tentativa
    void evaluate(const Eigen::VectorXd& u, Eigen::VectorXd& internalForce, std::vector<Eigen::Triplet<double>>& tangent);
    void commit();
    void reset();
    [[nodiscard]] std::vector<LinkResult> results() const;

private:
    struct Link {
        int id = 0;
        int dofI = 0, dofJ = 0;                 // indice global do primeiro grau de liberdade de cada no
        std::array<Eigen::Vector3d, 3> axis;    // eixos locais (globais, unitarios)
        std::array<LinkLawData, 6> laws;
        std::array<double, 6> plastic{};        // estado plastico confirmado
        std::array<double, 6> trialPlastic{};
        std::array<double, 6> deformation{};    // ultima avaliacao
        std::array<double, 6> force{};
    };
    std::vector<Link> links_;
    bool nonlinear_ = false;
};
