#include "nonlinear_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

#include <Eigen/SparseCholesky>

namespace {

struct MemberInfo {
    std::shared_ptr<Member> member;
    std::array<int, 12> dofs{};
    bool nonlinear = false;   // possui rotulas plasticas
};

// Anula linhas e colunas dos graus de liberdade restritos (diagonal 1) e zera o resíduo neles
void restrain(const std::vector<bool>& fixed, Eigen::SparseMatrix<double>& k, Eigen::VectorXd& rhs)
{
    std::vector<Eigen::Triplet<double>> triplets;
    triplets.reserve(static_cast<size_t>(k.nonZeros()) + fixed.size());
    for (int col = 0; col < k.outerSize(); ++col) {
        for (Eigen::SparseMatrix<double>::InnerIterator it(k, col); it; ++it) {
            if (!fixed[static_cast<size_t>(it.row())] && !fixed[static_cast<size_t>(col)]) {
                triplets.emplace_back(it.row(), col, it.value());
            }
        }
    }
    for (size_t i = 0; i < fixed.size(); ++i) {
        if (fixed[i]) {
            triplets.emplace_back(static_cast<int>(i), static_cast<int>(i), 1.0);
            rhs(static_cast<Eigen::Index>(i)) = 0.0;
        }
    }
    Eigen::SparseMatrix<double> modified(k.rows(), k.cols());
    modified.setFromTriplets(triplets.begin(), triplets.end());
    modified.makeCompressed();
    k = std::move(modified);
}

class Engine {
public:
    Engine(NonlinearContext& context, bool secondOrder)
        : c_(context), secondOrder_(secondOrder), n_(context.totalDof), fixed_(static_cast<size_t>(context.totalDof), false)
    {
        for (const int dof : c_.restrainedDof) {
            fixed_[static_cast<size_t>(dof)] = true;
        }
        for (const auto& member : c_.members) {
            MemberInfo info;
            info.member = member;
            const int nodeI = c_.nodeIndexById.at(member->getNodeI()->getId());
            const int nodeJ = c_.nodeIndexById.at(member->getNodeJ()->getId());
            for (int i = 0; i < 6; ++i) {
                info.dofs[static_cast<size_t>(i)] = nodeI * 6 + i;
                info.dofs[static_cast<size_t>(i + 6)] = nodeJ * 6 + i;
            }
            if (member->hasWarping()) {
                throw std::runtime_error("Nonlinear analysis does not support members with warping torsion (member " + std::to_string(member->getId()) + ")");
            }
            info.nonlinear = member->hasPlasticHinges();
            members_.push_back(std::move(info));
        }
        const NonlinearOptions& o = c_.input.nonlinear;
        tolerance_ = o.tolerance > 0.0 ? o.tolerance : 1e-6;
        maxIterations_ = std::max(2, o.maxIterations);
        criteria_ = o.criteria != 0 ? o.criteria : 5;
        referenceNorm_ = freeNorm(c_.referenceLoad.size() == c_.externalLoad.size() ? c_.referenceLoad : c_.externalLoad);
    }

    NonlinearOutcome run();

private:
    double freeNorm(const Eigen::VectorXd& v) const
    {
        double s = 0.0;
        for (int i = 0; i < n_; ++i) {
            if (!fixed_[static_cast<size_t>(i)]) {
                s += v(i) * v(i);
            }
        }
        return std::sqrt(s);
    }

    void publish(const Eigen::VectorXd& u)
    {
        for (const auto& node : c_.nodes) {
            const int index = c_.nodeIndexById.at(node->getId());
            for (int d = 0; d < 6; ++d) {
                node->setDisplacement(d, u(index * 6 + d));
            }
        }
    }

    // Monta a rigidez tangente e as forcas internas para o estado u no fator de carga lambda. false: falha em alguma barra com rotulas.
    bool assemble(const Eigen::VectorXd& u, double lambda, Eigen::SparseMatrix<double>& k, Eigen::VectorXd& fint)
    {
        std::vector<Eigen::Triplet<double>> triplets;
        triplets.reserve(members_.size() * 144U + static_cast<size_t>(c_.linearStiffness.nonZeros()));
        fint = c_.linearStiffness * u;
        for (int col = 0; col < c_.linearStiffness.outerSize(); ++col) {
            for (Eigen::SparseMatrix<double>::InnerIterator it(c_.linearStiffness, col); it; ++it) {
                triplets.emplace_back(it.row(), col, it.value());
            }
        }

        if (secondOrder_) {
            // forca axial das barras elasticas no estado atual (rigidez geometrica); as com rotulas usam a ultima avaliada
            publish(u);
            for (auto& info : members_) {
                info.member->setLoadFactor(lambda);
                if (!info.nonlinear) {
                    info.member->calculateInternalForces();
                    const auto& f = info.member->getInternalForces();
                    info.member->setGeometricAxialForce(0.5 * (f[6] - f[0]));
                } else {
                    info.member->setGeometricAxialForce(info.member->axialForceEstimate());
                }
            }
        }

        for (auto& info : members_) {
            Eigen::VectorXd ug(12);
            for (int i = 0; i < 12; ++i) {
                ug(i) = u(info.dofs[static_cast<size_t>(i)]);
            }
            Eigen::MatrixXd km;
            Eigen::VectorXd fm;
            if (info.nonlinear) {
                const Eigen::MatrixXd T = info.member->calculateNodeToAxisMatrix();
                Eigen::VectorXd f;
                Eigen::MatrixXd kt;
                if (!info.member->evaluateNonlinear(T * ug, lambda, f, kt)) {
                    return false;
                }
                km = T.transpose() * kt * T;
                fm = T.transpose() * f;
            } else {
                km = info.member->getGlobalStiffnessMatrix();
                fm = km * ug;
            }
            for (int i = 0; i < 12; ++i) {
                fint(info.dofs[static_cast<size_t>(i)]) += fm(i);
                for (int j = 0; j < 12; ++j) {
                    if (std::abs(km(i, j)) > 1e-12) {
                        triplets.emplace_back(info.dofs[static_cast<size_t>(i)], info.dofs[static_cast<size_t>(j)], km(i, j));
                    }
                }
            }
        }
        c_.links.evaluate(u, fint, triplets);
        k.resize(n_, n_);
        k.setFromTriplets(triplets.begin(), triplets.end());
        return true;
    }

    // Newton-Raphson no fator de carga `lambda` (controle de carga) ou com lambda incognita ate o deslocamento `target` do grau de controle.
    bool newton(Eigen::VectorXd& u, double& lambda, bool displacementControl, double target, int& iterationsOut);

    void commitAll()
    {
        for (auto& info : members_) {
            if (info.nonlinear) {
                info.member->commitHinges();
            }
        }
        c_.links.commit();
    }

    // deslocamento acompanhado na curva de capacidade: o do grau de controle ou, sem ele, o maior deslocamento de translacao
    double controlValue(const Eigen::VectorXd& u) const
    {
        if (controlDof_ >= 0) {
            return u(controlDof_);
        }
        double best = 0.0;
        for (int i = 0; i < n_; ++i) {
            if (i % 6 < 3 && std::abs(u(i)) > std::abs(best)) {
                best = u(i);
            }
        }
        return best;
    }

    NonlinearContext& c_;
    bool secondOrder_;
    int n_;
    std::vector<bool> fixed_;
    std::vector<MemberInfo> members_;
    double tolerance_ = 1e-6;
    int maxIterations_ = 40;
    int criteria_ = 5;
    double referenceNorm_ = 0.0;
    int controlDof_ = -1;
};

bool Engine::newton(Eigen::VectorXd& u, double& lambda, bool displacementControl, double target, int& iterationsOut)
{
    Eigen::SparseMatrix<double> k;
    Eigen::VectorXd fint, previousDu = Eigen::VectorXd::Zero(n_);
    double firstEnergy = 0.0, firstResidual = 0.0, previousEnergy = 0.0;
    bool haveStep = false;
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
    const double forceScale = std::max(referenceNorm_, 1e-12);

    for (int iteration = 0; iteration <= maxIterations_; ++iteration) {
        iterationsOut = iteration;
        if (!assemble(u, lambda, k, fint)) {
            return false;
        }
        Eigen::VectorXd residual = lambda * c_.externalLoad - fint;
        for (int i = 0; i < n_; ++i) {
            if (fixed_[static_cast<size_t>(i)]) {
                residual(i) = 0.0;
            }
        }
        const double residualNorm = residual.norm();
        if (!std::isfinite(residualNorm)) {
            return false;
        }
        if (iteration == 0) {
            firstResidual = residualNorm;
        }
        // criterios de convergencia (todos os escolhidos) avaliados no estado atual com o ultimo incremento
        bool converged = false;
        if (haveStep || (!displacementControl && residualNorm <= 1e-14 * forceScale)) {
            const bool force = residualNorm <= tolerance_ * std::max(std::abs(lambda) * referenceNorm_, 1e-9 * forceScale);
            const bool displacement = previousDu.norm() <= tolerance_ * std::max(u.norm(), 1e-12);
            const bool energy = previousEnergy <= tolerance_ * tolerance_ * std::max(firstEnergy, 1e-30) || previousEnergy < 1e-30;
            converged = ((criteria_ & 1) == 0 || force) && ((criteria_ & 2) == 0 || displacement) && ((criteria_ & 4) == 0 || energy);
            if (residualNorm <= 1e-14 * forceScale) {
                converged = true;
            }
        }
        if (displacementControl && converged) {
            // o deslocamento de controle tem de ter chegado ao valor do passo
            converged = std::abs(u(controlDof_) - target) <= 1e-9 * std::max(std::abs(target), 1e-12) + 1e-14;
        }
        if (converged) {
            return true;
        }
        if (iteration == maxIterations_) {
            return false;
        }
        if (haveStep && residualNorm > 1e6 * std::max(firstResidual, forceScale)) {
            return false;   // divergencia
        }

        restrain(fixed_, k, residual);
        solver.compute(k);
        if (solver.info() != Eigen::Success) {
            return false;
        }
        const Eigen::VectorXd& pivots = solver.vectorD();
        if (pivots.size() > 0 && pivots.minCoeff() <= 0.0) {
            return false;   // matriz tangente singular ou indefinida: ponto limite ou mecanismo
        }
        Eigen::VectorXd du = solver.solve(residual);
        if (!du.allFinite()) {
            return false;
        }
        if (displacementControl) {
            // lambda incognita: du = du_R + dlambda du_P com a restricao u_c + du_c = target
            Eigen::VectorXd load = c_.referenceLoad.size() == c_.externalLoad.size() ? c_.referenceLoad : c_.externalLoad;
            for (int i = 0; i < n_; ++i) {
                if (fixed_[static_cast<size_t>(i)]) {
                    load(i) = 0.0;
                }
            }
            const Eigen::VectorXd duP = solver.solve(load);
            if (!duP.allFinite() || std::abs(duP(controlDof_)) < 1e-300) {
                return false;
            }
            const double dLambda = (target - u(controlDof_) - du(controlDof_)) / duP(controlDof_);
            du += dLambda * duP;
            lambda += dLambda;
        }
        previousEnergy = std::abs(du.dot(residual));
        if (!haveStep) {
            firstEnergy = previousEnergy;
        }
        previousDu = du;
        haveStep = true;
        u += du;
    }
    return false;
}

NonlinearOutcome Engine::run()
{
    const NonlinearOptions& o = c_.input.nonlinear;
    NonlinearOutcome out;
    Eigen::VectorXd u = Eigen::VectorXd::Zero(n_);
    double lambda = 0.0;

    const bool displacementControl = o.controlMode == 1;
    if (const auto it = c_.nodeIndexById.find(o.controlNodeId); it != c_.nodeIndexById.end() && o.controlDof >= 0 && o.controlDof <= 5) {
        controlDof_ = it->second * 6 + o.controlDof;
    } else if (displacementControl) {
        throw std::runtime_error("Nonlinear analysis: invalid displacement-control node/dof");
    }
    if (displacementControl) {
        if (fixed_[static_cast<size_t>(controlDof_)]) {
            throw std::runtime_error("Nonlinear analysis: the control degree of freedom is restrained");
        }
        if (o.controlTarget == 0.0) {
            throw std::runtime_error("Nonlinear analysis: displacement-control target is zero");
        }
    }
    for (auto& info : members_) {
        if (info.nonlinear) {
            info.member->resetHinges();
        }
    }
    c_.links.reset();

    const int steps = std::max(1, o.steps);
    const double lambdaMax = o.loadFactorMax > 0.0 ? o.loadFactorMax : 1.0;
    double progress = 0.0;   // fator de carga (controle de carga) ou deslocamento de controle (controle de deslocamento) ja confirmado
    const double progressMax = displacementControl ? o.controlTarget : lambdaMax;
    double increment = progressMax / steps;
    int cutbacks = 0;
    bool failedEarly = false;
    out.curve.push_back({0.0, 0.0});

    while (std::abs(progress) < std::abs(progressMax) * (1.0 - 1e-12)) {
        double next = progress + increment;
        if (std::abs(next) > std::abs(progressMax)) {
            next = progressMax;
        }
        Eigen::VectorXd trial = u;
        double trialLambda = lambda;
        int iterations = 0;
        const bool ok = displacementControl ? newton(trial, trialLambda, true, next, iterations)
                                            : (trialLambda = next, newton(trial, trialLambda, false, 0.0, iterations));
        out.iterations += iterations;
        if (ok) {
            u = trial;
            lambda = trialLambda;
            progress = next;
            commitAll();
            ++out.steps;
            out.curve.push_back({lambda, controlValue(u)});
            cutbacks = 0;
        } else {
            ++cutbacks;
            increment *= 0.5;
            if (cutbacks > o.maxCutbacks || std::abs(increment) < 1e-9 * std::abs(progressMax)) {
                failedEarly = true;
                break;
            }
        }
    }

    if (failedEarly) {
        if (out.steps == 0) {
            throw std::runtime_error("Nonlinear analysis did not converge even with the smallest load step (tangent stiffness singular or divergent iteration): "
                                     "check supports, hinge capacities and nonlinear links");
        }
        out.completed = false;
        char text[320];
        if (displacementControl) {
            std::snprintf(text, sizeof(text),
                          "Nonlinear analysis stopped at a displacement of %.6g (target %.6g) and load factor %.4f: the tangent stiffness became singular or the iteration "
                          "did not converge; the results of the last converged step were kept",
                          u(controlDof_), o.controlTarget, lambda);
        } else {
            std::snprintf(text, sizeof(text),
                          "Nonlinear analysis stopped at load factor %.4f of %.4f: the structure reached its capacity (mechanism or loss of convergence); the results of the "
                          "last converged step were kept",
                          lambda, lambdaMax);
        }
        c_.warnings.push_back(text);
    }

    // estado final: reavalia as forcas internas e deixa os esforcos das barras com rotulas
    Eigen::SparseMatrix<double> k;
    Eigen::VectorXd fint;
    if (!assemble(u, lambda, k, fint)) {
        throw std::runtime_error("Nonlinear analysis: failed to evaluate the final state");
    }
    for (auto& info : members_) {
        info.member->setLoadFactor(lambda);
        if (!info.nonlinear) {
            continue;
        }
        Eigen::VectorXd ug(12);
        for (int i = 0; i < 12; ++i) {
            ug(i) = u(info.dofs[static_cast<size_t>(i)]);
        }
        Eigen::VectorXd f;
        Eigen::MatrixXd kt;
        info.member->evaluateNonlinear(info.member->calculateNodeToAxisMatrix() * ug, lambda, f, kt);
        // forcas nos nos (eixos da barra): reacao da zona rigida mais a da parte flexivel levada ao no
        Eigen::VectorXd atNodes = info.member->getRigidZoneMatrix().transpose() * f;
        const auto& rigid = info.member->rigidFixed();
        for (size_t i = 0; i < rigid.size() && i < 12; ++i) {
            atNodes(static_cast<Eigen::Index>(i)) += lambda * rigid[i];
        }
        std::vector<double> forces(12);
        for (int i = 0; i < 12; ++i) {
            forces[static_cast<size_t>(i)] = atNodes(i);
        }
        info.member->overrideInternalForces(forces);
    }
    out.displacement = u;
    out.internalForce = fint;
    out.loadFactor = lambda;
    return out;
}

} // namespace

NonlinearOutcome runNonlinearAnalysis(NonlinearContext& context, bool secondOrder)
{
    Engine engine(context, secondOrder);
    return engine.run();
}
