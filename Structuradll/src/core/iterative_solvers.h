#pragma once

#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>
#include <Eigen/IterativeLinearSolvers>

#include <string>

namespace Structura::Solver {

/// Resultado do problema B x = mu A x (A simetrica positiva definida, B simetrica) para os maiores mu.
struct GeneralizedEigenResult {
    Eigen::VectorXd mu;        ///< em ordem decrescente
    Eigen::MatrixXd vectors;   ///< colunas A-ortonormais, na mesma ordem
    bool converged = false;
    int iterations = 0;
    std::string error;
};

/// Iteracao em subespaco com fatoracao esparsa de A (ordenacao AMD) e Rayleigh-Ritz: obtem os `count` maiores mu de B x = mu A x sem formar matrizes densas.
/// Flambagem: A = K, B = -Kg (mu = 1/lambda). Modal: A = K, B = M (mu = 1/omega^2). Bloco de max(2 count, count + 8) vetores; a convergencia exige
/// variacao relativa dos `count` maiores mu positivos menor que `tolerance`.
GeneralizedEigenResult largestGeneralizedEigenpairs(const Eigen::SparseMatrix<double>& A, const Eigen::SparseMatrix<double>& B, int count, double tolerance = 1e-10,
                                                    int maxIterations = 300);

/// Gradiente conjugado precondicionado (Cholesky incompleta) para A x = b com A simetrica positiva definida. Retorna false se nao convergir.
bool solvePreconditionedCG(const Eigen::SparseMatrix<double>& A, const Eigen::VectorXd& b, Eigen::VectorXd& x, double tolerance, int maxIterations, int* iterationsOut = nullptr,
                           double* errorOut = nullptr);

}   // namespace Structura::Solver
