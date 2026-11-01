#include "constraints.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include "node.h"

namespace {

constexpr double kPenaltyFactor = 1.0e7;

int dofIndex(const std::unordered_map<int, int>& nodeIndexById, int nodeId, int dof, const char* what)
{
    const auto it = nodeIndexById.find(nodeId);
    if (it == nodeIndexById.end()) {
        throw std::runtime_error(std::string(what) + " references unknown node: " + std::to_string(nodeId));
    }
    if (dof < 0 || dof > 5) {
        throw std::runtime_error(std::string(what) + ": invalid degree of freedom");
    }
    return it->second * 6 + dof;
}

} // namespace

std::vector<ConstraintRow> buildConstraintRows(const SolverInput& input,
                                               const std::unordered_map<int, int>& nodeIndexById,
                                               const std::vector<std::shared_ptr<Node>>& nodes,
                                               const Eigen::SparseMatrix<double>& base,
                                               int totalDof)
{
    std::vector<ConstraintRow> rows;
    if (input.diaphragms.empty() && input.rigidLinks.empty() && input.inclinedSupports.empty() && input.constraints.empty()) {
        return rows;
    }

    // maiores rigidezes diagonais de translacao e de rotacao do modelo (escala de reserva dos nos sem rigidez propria, como os escravos puros)
    double maxTranslation = 0.0, maxRotation = 0.0;
    for (int i = 0; i < totalDof; ++i) {
        (i % 6 < 3 ? maxTranslation : maxRotation) = std::max(i % 6 < 3 ? maxTranslation : maxRotation, std::abs(base.coeff(i, i)));
    }
    if (maxTranslation <= 0.0) {
        maxTranslation = 1.0;
    }
    if (maxRotation <= 0.0) {
        maxRotation = maxTranslation;
    }
    // escala da penalidade: maior rigidez diagonal entre os graus de liberdade do mesmo tipo (translacao ou rotacao) do no do grau dependente;
    // sem rigidez propria, 1e-3 da maior do modelo (a penalidade ainda resulta muito maior que a rigidez da estrutura)
    auto scaleOf = [&](int dofIdx) {
        const int node = dofIdx / 6;
        const bool rotation = dofIdx % 6 >= 3;
        double scale = 0.0;
        for (int c = 0; c < 3; ++c) {
            scale = std::max(scale, std::abs(base.coeff(node * 6 + (rotation ? 3 : 0) + c, node * 6 + (rotation ? 3 : 0) + c)));
        }
        const double reference = rotation ? maxRotation : maxTranslation;
        return scale > 1e-9 * reference ? scale : reference * 1e-3;
    };
    auto add = [&](std::vector<std::pair<int, double>> entries, bool report, int group) {
        // o primeiro termo e o de maior coeficiente, normalizado a 1
        std::stable_sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return std::abs(a.second) > std::abs(b.second); });
        if (entries.empty() || std::abs(entries.front().second) < 1e-14) {
            return;
        }
        const double lead = entries.front().second;
        for (auto& e : entries) {
            e.second /= lead;
        }
        ConstraintRow row;
        row.penalty = kPenaltyFactor * scaleOf(entries.front().first);
        row.entries = std::move(entries);
        row.reportReaction = report;
        row.group = group;
        rows.push_back(std::move(row));
    };
    auto position = [&](int nodeId, const char* what) {
        const auto it = nodeIndexById.find(nodeId);
        if (it == nodeIndexById.end()) {
            throw std::runtime_error(std::string(what) + " references unknown node: " + std::to_string(nodeId));
        }
        const auto& n = nodes[static_cast<size_t>(it->second)];
        return Eigen::Vector3d(n->getX(), n->getY(), n->getZ());
    };

    // Diafragma rigido: os nos escravos acompanham o mestre como corpo rigido no plano normal ao eixo `normalAxis`
    // (duas translacoes no plano e a rotacao em torno da normal); os demais graus de liberdade ficam livres.
    for (const DiaphragmData& d : input.diaphragms) {
        const int n = d.normalAxis;
        if (n < 0 || n > 2) {
            throw std::runtime_error("Diaphragm: normal axis must be 0, 1 or 2");
        }
        const int p = (n + 1) % 3, q = (n + 2) % 3;
        const Eigen::Vector3d master = position(d.masterNodeId, "Diaphragm");
        for (const int slaveId : d.slaveNodeIds) {
            if (slaveId == d.masterNodeId) {
                continue;
            }
            const Eigen::Vector3d r = position(slaveId, "Diaphragm") - master;
            // u_p,s = u_p,m - theta_n r_q ; u_q,s = u_q,m + theta_n r_p ; theta_s = theta_m
            add({{dofIndex(nodeIndexById, slaveId, p, "Diaphragm"), 1.0}, {dofIndex(nodeIndexById, d.masterNodeId, p, "Diaphragm"), -1.0},
                 {dofIndex(nodeIndexById, d.masterNodeId, 3 + n, "Diaphragm"), r(q)}}, false, 1);
            add({{dofIndex(nodeIndexById, slaveId, q, "Diaphragm"), 1.0}, {dofIndex(nodeIndexById, d.masterNodeId, q, "Diaphragm"), -1.0},
                 {dofIndex(nodeIndexById, d.masterNodeId, 3 + n, "Diaphragm"), -r(p)}}, false, 1);
            add({{dofIndex(nodeIndexById, slaveId, 3 + n, "Diaphragm"), 1.0}, {dofIndex(nodeIndexById, d.masterNodeId, 3 + n, "Diaphragm"), -1.0}}, false, 1);
        }
    }

    // Ligacoes rigidas entre dois nos
    for (const RigidLinkData& link : input.rigidLinks) {
        const int m = link.masterNodeId, s = link.slaveNodeId;
        switch (link.kind) {
        case 0: {   // corpo rigido de 6 graus de liberdade: u_s = u_m + theta_m x r ; theta_s = theta_m
            const Eigen::Vector3d r = position(s, "Rigid link") - position(m, "Rigid link");
            for (int c = 0; c < 3; ++c) {
                const int a = (c + 1) % 3, b = (c + 2) % 3;
                // (theta x r)_c = theta_a r_b - theta_b r_a
                add({{dofIndex(nodeIndexById, s, c, "Rigid link"), 1.0}, {dofIndex(nodeIndexById, m, c, "Rigid link"), -1.0},
                     {dofIndex(nodeIndexById, m, 3 + a, "Rigid link"), -r(b)}, {dofIndex(nodeIndexById, m, 3 + b, "Rigid link"), r(a)}}, false, 2);
                add({{dofIndex(nodeIndexById, s, 3 + c, "Rigid link"), 1.0}, {dofIndex(nodeIndexById, m, 3 + c, "Rigid link"), -1.0}}, false, 2);
            }
            break;
        }
        case 1:     // tres translacoes iguais
        case 2: {   // tres rotacoes iguais
            const int offset = link.kind == 1 ? 0 : 3;
            for (int c = 0; c < 3; ++c) {
                add({{dofIndex(nodeIndexById, s, offset + c, "Rigid link"), 1.0}, {dofIndex(nodeIndexById, m, offset + c, "Rigid link"), -1.0}}, false, 2);
            }
            break;
        }
        case 3:     // um unico grau de liberdade igual
            add({{dofIndex(nodeIndexById, s, link.dof, "Rigid link"), 1.0}, {dofIndex(nodeIndexById, m, link.dof, "Rigid link"), -1.0}}, false, 2);
            break;
        default:
            throw std::runtime_error("Rigid link: unknown kind " + std::to_string(link.kind));
        }
    }

    // Apoios inclinados: restringe o deslocamento (ou a rotacao) segundo uma direcao qualquer
    for (const InclinedSupportData& sup : input.inclinedSupports) {
        for (int k = 0; k < 6; ++k) {
            if (((sup.mask >> k) & 1) == 0) {
                continue;
            }
            const int axis = k % 3, offset = k < 3 ? 0 : 3;
            Eigen::Vector3d e(sup.axes[static_cast<size_t>(axis * 3)], sup.axes[static_cast<size_t>(axis * 3 + 1)], sup.axes[static_cast<size_t>(axis * 3 + 2)]);
            if (e.norm() < 1e-12) {
                throw std::runtime_error("Inclined support at node " + std::to_string(sup.nodeId) + ": null direction");
            }
            e.normalize();
            std::vector<std::pair<int, double>> entries;
            for (int c = 0; c < 3; ++c) {
                if (std::abs(e(c)) > 1e-12) {
                    entries.push_back({dofIndex(nodeIndexById, sup.nodeId, offset + c, "Inclined support"), e(c)});
                }
            }
            add(entries, true, 3);
        }
    }

    // Vinculos lineares gerais: sum c_j u_j = 0
    for (const LinearConstraintData& c : input.constraints) {
        if (c.nodeIds.size() != c.dofs.size() || c.nodeIds.size() != c.coefs.size()) {
            throw std::runtime_error("Linear constraint with inconsistent term arrays");
        }
        std::vector<std::pair<int, double>> entries;
        for (size_t i = 0; i < c.nodeIds.size(); ++i) {
            entries.push_back({dofIndex(nodeIndexById, c.nodeIds[i], c.dofs[i], "Linear constraint"), c.coefs[i]});
        }
        add(entries, c.reportReaction, 0);
    }
    return rows;
}

void assembleConstraintStiffness(const std::vector<ConstraintRow>& rows, int totalDof, Eigen::SparseMatrix<double>& k)
{
    std::vector<Eigen::Triplet<double>> triplets;
    for (const ConstraintRow& row : rows) {
        for (const auto& a : row.entries) {
            for (const auto& b : row.entries) {
                triplets.emplace_back(a.first, b.first, row.penalty * a.second * b.second);
            }
        }
    }
    k.resize(totalDof, totalDof);
    k.setFromTriplets(triplets.begin(), triplets.end());
}

std::vector<double> constraintForces(const std::vector<ConstraintRow>& rows, const Eigen::VectorXd& displacement)
{
    std::vector<double> forces;
    forces.reserve(rows.size());
    for (const ConstraintRow& row : rows) {
        double violation = 0.0;
        for (const auto& e : row.entries) {
            violation += e.second * displacement(e.first);
        }
        forces.push_back(row.penalty * violation);
    }
    return forces;
}

Eigen::VectorXd constraintReactionVector(const std::vector<ConstraintRow>& rows, const Eigen::VectorXd& displacement, int totalDof)
{
    Eigen::VectorXd reaction = Eigen::VectorXd::Zero(totalDof);
    for (const ConstraintRow& row : rows) {
        if (!row.reportReaction) {
            continue;
        }
        double violation = 0.0;
        for (const auto& e : row.entries) {
            violation += e.second * displacement(e.first);
        }
        for (const auto& e : row.entries) {
            reaction(e.first) -= row.penalty * violation * e.second;
        }
    }
    return reaction;
}
