#include "iterative_solvers.h"

#include <algorithm>
#include <cmath>

namespace Structura::Solver {

GeneralizedEigenResult largestGeneralizedEigenpairs(const Eigen::SparseMatrix<double>& A, const Eigen::SparseMatrix<double>& B, int count, double tolerance, int maxIterations) {
    GeneralizedEigenResult result;
    const int n = static_cast<int>(A.rows());
    if (n == 0 || count <= 0) {
        result.error = "empty eigenproblem";
        return result;
    }
    const int block = std::min(n, std::max(2 * count, count + 8));
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>, Eigen::Lower, Eigen::AMDOrdering<int>> factor;
    factor.compute(A);
    if (factor.info() != Eigen::Success) {
        result.error = "stiffness factorization failed";
        return result;
    }
    // vetores iniciais deterministicos (sequencia congruencial) para nao depender de semente global
    Eigen::MatrixXd x(n, block);
    unsigned state = 12345u;
    for (int j = 0; j < block; ++j) {
        for (int i = 0; i < n; ++i) {
            state = state * 1664525u + 1013904223u;
            x(i, j) = (static_cast<double>(state >> 8) / 16777216.0) - 0.5;
        }
    }
    Eigen::VectorXd previous = Eigen::VectorXd::Zero(count);
    Eigen::VectorXd mu;
    Eigen::MatrixXd ritz;
    for (int iteration = 1; iteration <= std::max(1, maxIterations); ++iteration) {
        Eigen::MatrixXd y = factor.solve(Eigen::MatrixXd(B * x));
        if (factor.info() != Eigen::Success || !y.allFinite()) {
            result.error = "subspace iteration diverged";
            return result;
        }
        // base ortonormal do subespaco gerado por y e Rayleigh-Ritz no problema reduzido
        Eigen::HouseholderQR<Eigen::MatrixXd> qr(y);
        Eigen::MatrixXd q = qr.householderQ() * Eigen::MatrixXd::Identity(n, block);
        const Eigen::MatrixXd ar = q.transpose() * (A * q);
        const Eigen::MatrixXd br = q.transpose() * (B * q);
        Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::MatrixXd> small(br, ar);
        if (small.info() != Eigen::Success) {
            result.error = "reduced eigenproblem failed";
            return result;
        }
        // ordem decrescente
        mu = small.eigenvalues().reverse();
        ritz = small.eigenvectors().rowwise().reverse();
        x = q * ritz;
        result.iterations = iteration;
        // convergencia: maiores mu positivos
        double change = 0.0;
        bool enough = true;
        for (int k = 0; k < count; ++k) {
            if (mu(k) <= 0.0) {
                enough = false;
                break;
            }
            change = std::max(change, std::abs(mu(k) - previous(k)) / mu(k));
        }
        for (int k = 0; k < count; ++k) {
            previous(k) = k < mu.size() ? mu(k) : 0.0;
        }
        if (enough && iteration > 1 && change < tolerance) {
            result.converged = true;
            break;
        }
        if (!enough && iteration > 3 && mu(0) <= 0.0) {
            // sem modos positivos
            result.converged = true;
            break;
        }
    }
    result.mu = mu;
    result.vectors = x;
    if (!result.converged) {
        // aceita o que houver; o chamador decide pelo indicador
        result.error = "subspace iteration reached the iteration limit";
    }
    return result;
}

bool solvePreconditionedCG(const Eigen::SparseMatrix<double>& A, const Eigen::VectorXd& b, Eigen::VectorXd& x, double tolerance, int maxIterations, int* iterationsOut, double* errorOut) {
    Eigen::ConjugateGradient<Eigen::SparseMatrix<double>, Eigen::Lower | Eigen::Upper, Eigen::IncompleteCholesky<double>> cg;
    cg.setTolerance(tolerance);
    cg.setMaxIterations(maxIterations);
    cg.compute(A);
    if (cg.info() != Eigen::Success) {
        return false;
    }
    x = cg.solve(b);
    if (iterationsOut) {
        *iterationsOut = static_cast<int>(cg.iterations());
    }
    if (errorOut) {
        *errorOut = cg.error();
    }
    return cg.info() == Eigen::Success && x.allFinite();
}

}   // namespace Structura::Solver
