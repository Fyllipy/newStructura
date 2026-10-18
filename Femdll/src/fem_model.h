#pragma once

#include "fem_types.h"

#include <string>

namespace fem {

struct SolveOptions {
    int maxSpringIterations {30};
};

class Model {
public:
    std::vector<Material> materials;
    std::vector<Node> nodes;
    std::vector<Shell> shells;
    std::vector<Beam> beams;
    std::vector<Spring> springs;
    std::vector<Tie> ties;
    std::vector<LoadCase> cases;

    /// Resolve todos os casos (uma fatoracao por configuracao de molas). Devolve false e preenche `error` em caso de falha.
    bool solve(const SolveOptions &options, std::string &error);

    std::vector<CaseResult> results;
    int springIterations {0};
    double maxWarp {0.0};   ///< maior empenamento de elemento (distancia dos nos ao plano medio)
};

/// Rigidez local 12x12 da barra (Euler-Bernoulli) e matriz de transformacao global -> local dos 12 graus de liberdade.
Eigen::Matrix<double, 12, 12> beamLocalStiffness(const Beam &b, const Material &m, double length);
Eigen::Matrix<double, 12, 12> beamTransform(const Vec3 &xi, const Vec3 &xj, const Vec3 &refY);

} // namespace fem
