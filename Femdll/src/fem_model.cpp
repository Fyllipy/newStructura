#include "fem_model.h"
#include "../../Structuradll/src/core/solver_security.h"

#include "shell_q4.h"

#include <Eigen/SparseCholesky>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

namespace fem {

namespace {

using Triplet = Eigen::Triplet<double>;
using Mat12 = Eigen::Matrix<double, 12, 12>;
using Vec12 = Eigen::Matrix<double, 12, 1>;

struct BeamFrame {
    Vec3 ex, ey, ez;
    double length {0.0};
};

BeamFrame beamFrame(const Vec3 &xi, const Vec3 &xj, const Vec3 &refY)
{
    BeamFrame f;
    const Vec3 d = xj - xi;
    f.length = d.norm();
    f.ex = d / f.length;
    Vec3 ref = refY;
    if (ref.cross(f.ex).norm() < 1e-9) {
        ref = std::abs(f.ex.z()) < 0.9 ? Vec3(0, 0, 1) : Vec3(1, 0, 0);
    }
    f.ez = f.ex.cross(ref).normalized();
    f.ey = f.ez.cross(f.ex);
    return f;
}

} // namespace

Mat12 beamLocalStiffness(const Beam &b, const Material &m, double L)
{
    Mat12 k = Mat12::Zero();
    const double E = m.E, G = m.G();
    const double ea = E * b.A / L, gj = G * b.J / L;
    const double l2 = L * L, l3 = L * L * L;
    // axial
    k(0, 0) = ea;
    k(0, 6) = -ea;
    k(6, 6) = ea;
    // torcao
    k(3, 3) = gj;
    k(3, 9) = -gj;
    k(9, 9) = gj;
    // flexao no plano x-y (v, theta_z) com Iz
    const double a = 12.0 * E * b.Iz / l3, bb = 6.0 * E * b.Iz / l2, c = 4.0 * E * b.Iz / L, d = 2.0 * E * b.Iz / L;
    k(1, 1) = a;
    k(1, 5) = bb;
    k(1, 7) = -a;
    k(1, 11) = bb;
    k(5, 5) = c;
    k(5, 7) = -bb;
    k(5, 11) = d;
    k(7, 7) = a;
    k(7, 11) = -bb;
    k(11, 11) = c;
    // flexao no plano x-z (w, theta_y) com Iy
    const double a2 = 12.0 * E * b.Iy / l3, b2 = 6.0 * E * b.Iy / l2, c2 = 4.0 * E * b.Iy / L, d2 = 2.0 * E * b.Iy / L;
    k(2, 2) = a2;
    k(2, 4) = -b2;
    k(2, 8) = -a2;
    k(2, 10) = -b2;
    k(4, 4) = c2;
    k(4, 8) = b2;
    k(4, 10) = d2;
    k(8, 8) = a2;
    k(8, 10) = b2;
    k(10, 10) = c2;
    for (int i = 0; i < 12; ++i) {
        for (int j = i + 1; j < 12; ++j) {
            k(j, i) = k(i, j);
        }
    }
    return k;
}

Mat12 beamTransform(const Vec3 &xi, const Vec3 &xj, const Vec3 &refY)
{
    const BeamFrame f = beamFrame(xi, xj, refY);
    Mat3 r;
    r.row(0) = f.ex.transpose();
    r.row(1) = f.ey.transpose();
    r.row(2) = f.ez.transpose();
    Mat12 t = Mat12::Zero();
    for (int i = 0; i < 4; ++i) {
        t.block<3, 3>(3 * i, 3 * i) = r;
    }
    return t;
}

bool Model::solve(const SolveOptions &options, std::string &error)
{
    results.clear();
    springIterations = 0;
    maxWarp = 0.0;
    const int nn = static_cast<int>(nodes.size());
    const int nd = 6 * nn;
    if (nn == 0 || (shells.empty() && beams.empty())) {
        error = OBF_STR("Modelo sem nos ou sem elementos.");
        return false;
    }
    auto checkIndex = [&](int i, int n) { return i >= 0 && i < n; };

    // ---- rigidez dos elementos em coordenadas globais -------------------------------------------------------------
    struct ShellData {
        ShellFrame frame;
        ShellStiffness stiffness;
        Mat24 transform;
        std::array<int, 24> dofs {};
    };
    std::vector<ShellData> shellData(shells.size());
    std::vector<Triplet> trips;
    trips.reserve(shells.size() * 576 + beams.size() * 144 + nd);
    for (std::size_t e = 0; e < shells.size(); ++e) {
        const Shell &sh = shells[e];
        if (!checkIndex(sh.material, static_cast<int>(materials.size())) || sh.thickness <= 0.0) {
            error = OBF_STR("Casca ") + std::to_string(sh.id) + OBF_STR(": material ou espessura invalidos.");
            return false;
        }
        std::array<Vec3, 4> p;
        for (int i = 0; i < 4; ++i) {
            if (!checkIndex(sh.nodes[i], nn)) {
                error = OBF_STR("Casca ") + std::to_string(sh.id) + OBF_STR(": no inexistente.");
                return false;
            }
            p[i] = nodes[sh.nodes[i]].x;
        }
        ShellData &d = shellData[e];
        d.frame = shellFrame(p);
        if (d.frame.area < 1e-14) {
            error = OBF_STR("Casca ") + std::to_string(sh.id) + OBF_STR(": area nula.");
            return false;
        }
        maxWarp = std::max(maxWarp, d.frame.warp);
        d.stiffness = shellStiffness(d.frame, materials[sh.material], sh.thickness, sh.shearCorrection);
        d.transform = shellTransform(d.frame);
        for (int i = 0; i < 4; ++i) {
            for (int c = 0; c < 6; ++c) {
                d.dofs[6 * i + c] = 6 * sh.nodes[i] + c;
            }
        }
        const Mat24 kg = d.transform.transpose() * d.stiffness.K * d.transform;
        for (int i = 0; i < 24; ++i) {
            for (int j = 0; j < 24; ++j) {
                trips.emplace_back(d.dofs[i], d.dofs[j], kg(i, j));
            }
        }
    }
    struct BeamData {
        Mat12 local, transform;
        std::array<int, 12> dofs {};
        double length {0.0};
    };
    std::vector<BeamData> beamData(beams.size());
    for (std::size_t e = 0; e < beams.size(); ++e) {
        const Beam &b = beams[e];
        if (!checkIndex(b.material, static_cast<int>(materials.size())) || !checkIndex(b.nodes[0], nn) || !checkIndex(b.nodes[1], nn)) {
            error = OBF_STR("Barra ") + std::to_string(b.id) + OBF_STR(": dados invalidos.");
            return false;
        }
        BeamData &d = beamData[e];
        const Vec3 xi = nodes[b.nodes[0]].x, xj = nodes[b.nodes[1]].x;
        d.length = (xj - xi).norm();
        if (d.length < 1e-12) {
            error = OBF_STR("Barra ") + std::to_string(b.id) + OBF_STR(": comprimento nulo.");
            return false;
        }
        d.local = beamLocalStiffness(b, materials[b.material], d.length);
        d.transform = beamTransform(xi, xj, b.refY);
        for (int i = 0; i < 2; ++i) {
            for (int c = 0; c < 6; ++c) {
                d.dofs[6 * i + c] = 6 * b.nodes[i] + c;
            }
        }
        const Mat12 kg = d.transform.transpose() * d.local * d.transform;
        for (int i = 0; i < 12; ++i) {
            for (int j = 0; j < 12; ++j) {
                trips.emplace_back(d.dofs[i], d.dofs[j], kg(i, j));
            }
        }
    }
    Eigen::SparseMatrix<double> kStruct(nd, nd);
    kStruct.setFromTriplets(trips.begin(), trips.end());

    // ---- ligacoes nó-escravo (penalidade escalada pela rigidez diagonal do proprio no) ----------------------------------
    if (!ties.empty()) {
        double maxDiagonal = 0.0;
        for (int i = 0; i < nd; ++i) {
            maxDiagonal = std::max(maxDiagonal, std::abs(kStruct.coeff(i, i)));
        }
        std::vector<Triplet> tieTriplets;
        for (const Tie &t : ties) {
            if (!checkIndex(t.slave, nn) || t.masters.size() != t.weights.size() || t.masters.empty()) {
                error = OBF_STR("Ligacao no-aresta invalida.");
                return false;
            }
            for (const int m : t.masters) {
                if (!checkIndex(m, nn)) {
                    error = OBF_STR("Ligacao no-aresta: no mestre inexistente.");
                    return false;
                }
            }
            for (int c = 0; c < 6; ++c) {
                if (((t.dofMask >> c) & 1) == 0) {
                    continue;
                }
                const double diagonal = std::abs(kStruct.coeff(6 * t.slave + c, 6 * t.slave + c));
                const double kp = 1.0e7 * (diagonal > 1e-12 * maxDiagonal ? diagonal : maxDiagonal * 1e-6);
                std::vector<std::pair<int, double>> entries;
                entries.emplace_back(6 * t.slave + c, 1.0);
                for (std::size_t i = 0; i < t.masters.size(); ++i) {
                    entries.emplace_back(6 * t.masters[i] + c, -t.weights[i]);
                }
                for (const auto &a : entries) {
                    for (const auto &b : entries) {
                        tieTriplets.emplace_back(a.first, b.first, kp * a.second * b.second);
                    }
                }
            }
        }
        Eigen::SparseMatrix<double> kTie(nd, nd);
        kTie.setFromTriplets(tieTriplets.begin(), tieTriplets.end());
        kStruct += kTie;
    }

    // ---- restricoes ---------------------------------------------------------------------------------------------------
    std::vector<int> freeIndex(nd, -1);
    int freeCount = 0;
    for (int n = 0; n < nn; ++n) {
        for (int c = 0; c < 6; ++c) {
            if (!nodes[n].fixed[c]) {
                freeIndex[6 * n + c] = freeCount++;
            }
        }
    }
    if (freeCount == 0) {
        error = OBF_STR("Todos os graus de liberdade estao restritos.");
        return false;
    }

    // ---- vetores de carga --------------------------------------------------------------------------------------------
    auto loadVector = [&](const LoadCase &lc) {
        Eigen::VectorXd f = Eigen::VectorXd::Zero(nd);
        for (const NodalLoad &l : lc.nodal) {
            if (!checkIndex(l.node, nn)) {
                continue;
            }
            for (int c = 0; c < 6; ++c) {
                f(6 * l.node + c) += l.f[c];
            }
        }
        for (const ShellPressure &p : lc.pressure) {
            if (!checkIndex(p.shell, static_cast<int>(shells.size()))) {
                continue;
            }
            const ShellData &d = shellData[p.shell];
            const Vec24 g = d.transform.transpose() * shellPressureLoad(d.frame, p.p);
            for (int i = 0; i < 24; ++i) {
                f(d.dofs[i]) += g(i);
            }
        }
        for (const ShellBody &b : lc.body) {
            if (!checkIndex(b.shell, static_cast<int>(shells.size()))) {
                continue;
            }
            const ShellData &d = shellData[b.shell];
            const Vec24 g = d.transform.transpose() * shellBodyLoad(d.frame, d.frame.R * b.b, shells[b.shell].thickness);
            for (int i = 0; i < 24; ++i) {
                f(d.dofs[i]) += g(i);
            }
        }
        for (const ShellThermal &t : lc.thermal) {
            if (!checkIndex(t.shell, static_cast<int>(shells.size()))) {
                continue;
            }
            const ShellData &d = shellData[t.shell];
            const Shell &sh = shells[t.shell];
            const Vec24 g = d.transform.transpose() * shellThermalLoad(d.frame, materials[sh.material], sh.thickness, t.uniform, t.gradient);
            for (int i = 0; i < 24; ++i) {
                f(d.dofs[i]) += g(i);
            }
        }
        for (const BeamUniform &u : lc.beamUniform) {
            if (!checkIndex(u.beam, static_cast<int>(beams.size()))) {
                continue;
            }
            const BeamData &d = beamData[u.beam];
            const Beam &b = beams[u.beam];
            const BeamFrame fr = beamFrame(nodes[b.nodes[0]].x, nodes[b.nodes[1]].x, b.refY);
            const double qx = u.q.dot(fr.ex), qy = u.q.dot(fr.ey), qz = u.q.dot(fr.ez), L = d.length;
            Vec12 fe = Vec12::Zero();
            fe(0) = qx * L / 2.0;
            fe(6) = qx * L / 2.0;
            fe(1) = qy * L / 2.0;
            fe(7) = qy * L / 2.0;
            fe(5) = qy * L * L / 12.0;
            fe(11) = -qy * L * L / 12.0;
            fe(2) = qz * L / 2.0;
            fe(8) = qz * L / 2.0;
            fe(4) = -qz * L * L / 12.0;
            fe(10) = qz * L * L / 12.0;
            const Vec12 g = d.transform.transpose() * fe;
            for (int i = 0; i < 12; ++i) {
                f(d.dofs[i]) += g(i);
            }
        }
        return f;
    };
    std::vector<Eigen::VectorXd> rhs;
    for (const LoadCase &lc : cases) {
        rhs.push_back(loadVector(lc));
    }
    const int caseCount = static_cast<int>(cases.size());
    results.assign(caseCount, CaseResult {});

    // ---- molas -------------------------------------------------------------------------------------------------------------
    // D2: apoios que so comprimem tem conjunto ativo PROPRIO de cada caso de carga (resposta de um caso nao depende dos outros casos nem da ordem),
    // com criterios explicitos de convergencia; resultado nao convergido e erro, nunca sucesso silencioso.
    bool hasCompression = false;
    for (const Spring &sp : springs) {
        hasCompression = hasCompression || (sp.compressionOnly && checkIndex(sp.node, nn));
    }
    auto springMatrix = [&](const std::vector<char> &active) {
        std::vector<Triplet> st;
        for (std::size_t i = 0; i < springs.size(); ++i) {
            const Spring &sp = springs[i];
            if (!checkIndex(sp.node, nn) || !active[i]) {
                continue;
            }
            const int base = 6 * sp.node;
            if (sp.compressionOnly) {
                const Vec3 n = sp.normal.normalized();
                const Mat3 kk = sp.k[0] * n * n.transpose() + sp.k[1] * (Mat3::Identity() - n * n.transpose());
                for (int a = 0; a < 3; ++a) {
                    for (int b = 0; b < 3; ++b) {
                        st.emplace_back(base + a, base + b, kk(a, b));
                    }
                }
            } else {
                for (int c = 0; c < 6; ++c) {
                    if (sp.k[c] != 0.0) {
                        st.emplace_back(base + c, base + c, sp.k[c]);
                    }
                }
            }
        }
        Eigen::SparseMatrix<double> m(nd, nd);
        m.setFromTriplets(st.begin(), st.end());
        return m;
    };

    std::vector<Eigen::VectorXd> displacements(caseCount);
    std::vector<Eigen::SparseMatrix<double>> kSpringCase(caseCount);
    std::vector<int> caseIterations(caseCount, 0);

    // fatora K_struct + K_mola(active) e resolve os casos pedidos; devolve false e preenche `error` (mecanismo, falha numerica, residuo)
    auto solveCases = [&](const std::vector<char> &active, const std::vector<int> &caseList, Eigen::SparseMatrix<double> &kSpringOut) -> bool {
        kSpringOut = springMatrix(active);
        const Eigen::SparseMatrix<double> kTotal = kStruct + kSpringOut;
        std::vector<Triplet> reduced;
        reduced.reserve(kTotal.nonZeros());
        for (int c = 0; c < kTotal.outerSize(); ++c) {
            if (freeIndex[c] < 0) {
                continue;
            }
            for (Eigen::SparseMatrix<double>::InnerIterator it(kTotal, c); it; ++it) {
                if (freeIndex[it.row()] >= 0) {
                    reduced.emplace_back(freeIndex[it.row()], freeIndex[c], it.value());
                }
            }
        }
        Eigen::SparseMatrix<double> kff(freeCount, freeCount);
        kff.setFromTriplets(reduced.begin(), reduced.end());
        Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
        solver.compute(kff);
        if (solver.info() != Eigen::Success) {
            error = OBF_STR("Falha na fatoracao da matriz de rigidez (estrutura instavel ou apoios insuficientes).");
            return false;
        }
        {
            // mecanismo: pivo que perde ~12 digitos em relacao a diagonal do proprio grau de liberdade (dependencia linear).
            const auto pivots = solver.vectorD();
            const auto &permutation = solver.permutationP().indices();
            std::vector<int> nodalDof(freeCount, 0);
            for (int d = 0; d < nd; ++d) {
                if (freeIndex[d] >= 0) {
                    nodalDof[freeIndex[d]] = d;
                }
            }
            for (int i = 0; i < freeCount; ++i) {
                const double diagonal = std::abs(kff.coeff(i, i));
                const double pivot = pivots(permutation(i));
                if (diagonal > 0.0 && (pivot <= 0.0 || pivot < 1.0e-12 * diagonal)) {
                    error = OBF_STR("Estrutura instavel (mecanismo): no ") + std::to_string(nodes[nodalDof[i] / 6].id) + OBF_STR(", grau de liberdade ")
                            + std::to_string(nodalDof[i] % 6 + 1) + OBF_STR(" sem restricao pela rigidez do modelo; verifique apoios e ligacoes.");
                    return false;
                }
            }
        }
        for (const int cidx : caseList) {
            Eigen::VectorXd fr(freeCount);
            for (int d = 0; d < nd; ++d) {
                if (freeIndex[d] >= 0) {
                    fr(freeIndex[d]) = rhs[cidx](d);
                }
            }
            const Eigen::VectorXd uf = solver.solve(fr);
            if (solver.info() != Eigen::Success || !uf.allFinite()) {
                error = OBF_STR("Falha na solucao do sistema (matriz singular).");
                return false;
            }
            // erro inverso normwise do sistema linear: ||K u - f|| / (||K||_F ||u|| + ||f||); um LDL^T estavel fica em ~1e-16..1e-13
            const Eigen::VectorXd ku = kff * uf;
            const double scale = kff.norm() * uf.norm() + fr.norm();
            if (scale > 0.0 && (ku - fr).norm() > 1e-9 * scale) {
                error = OBF_STR("Residuo do sistema linear acima da tolerancia (matriz mal condicionada); resultado rejeitado.");
                return false;
            }
            Eigen::VectorXd u = Eigen::VectorXd::Zero(nd);
            for (int d = 0; d < nd; ++d) {
                if (freeIndex[d] >= 0) {
                    u(d) = uf(freeIndex[d]);
                }
            }
            displacements[cidx] = u;
        }
        return true;
    };

    if (!hasCompression) {
        // sem apoios unilaterais: uma unica fatoracao para todos os casos (igual ao comportamento anterior)
        const std::vector<char> all(springs.size(), 1);
        std::vector<int> every(caseCount);
        for (int c = 0; c < caseCount; ++c) {
            every[c] = c;
        }
        Eigen::SparseMatrix<double> kS;
        if (!solveCases(all, every, kS)) {
            return false;
        }
        for (int c = 0; c < caseCount; ++c) {
            kSpringCase[c] = kS;
            caseIterations[c] = 1;
        }
        springIterations = 1;
    } else {
        const int maxIterations = std::max(1, options.maxSpringIterations);
        for (int cidx = 0; cidx < caseCount; ++cidx) {
            std::vector<char> active(springs.size(), 1);
            std::set<std::string> visited;
            bool converged = false;
            for (int iteration = 0; iteration < maxIterations; ++iteration) {
                std::string key(active.begin(), active.end());
                visited.insert(key);
                if (!solveCases(active, {cidx}, kSpringCase[cidx])) {
                    if (iteration > 0) {
                        error = OBF_STR("Caso ") + std::to_string(cidx + 1) + OBF_STR(": depois de desativar as molas que so comprimem e tracionavam, a estrutura ficou instavel (")
                                + error + OBF_STR("). Nao ha equilibrio possivel com apoios que so comprimem: o carregamento levanta a estrutura.");
                    } else if (caseCount > 1) {
                        error = OBF_STR("Caso ") + std::to_string(cidx + 1) + OBF_STR(": ") + error;
                    }
                    return false;
                }
                const Eigen::VectorXd &u = displacements[cidx];
                double uscale = 0.0;
                for (int n = 0; n < nn; ++n) {
                    uscale = std::max(uscale, u.segment<3>(6 * n).cwiseAbs().maxCoeff());
                }
                const double tol = 1e-12 * uscale;
                // atualizacao com histerese: ativa continua ativa enquanto u.n <= +tol; inativa so volta se u.n < -tol (evita chattering em contato nulo)
                std::vector<char> next = active;
                bool changed = false;
                for (std::size_t i = 0; i < springs.size(); ++i) {
                    const Spring &sp = springs[i];
                    if (!sp.compressionOnly || !checkIndex(sp.node, nn)) {
                        continue;
                    }
                    const double un = u.segment<3>(6 * sp.node).dot(sp.normal.normalized());
                    const bool want = active[i] ? (un <= tol) : (un < -tol);
                    if (static_cast<bool>(active[i]) != want) {
                        next[i] = want ? 1 : 0;
                        changed = true;
                    }
                }
                caseIterations[cidx] = iteration + 1;
                if (!changed) {
                    converged = true;
                    break;
                }
                if (visited.count(std::string(next.begin(), next.end())) != 0) {
                    error = OBF_STR("Caso ") + std::to_string(cidx + 1) + OBF_STR(": as molas que so comprimem entraram em ciclo de ativacao/desativacao (sem convergencia); resultado rejeitado.");
                    return false;
                }
                active = next;
            }
            if (!converged) {
                error = OBF_STR("Caso ") + std::to_string(cidx + 1) + OBF_STR(": as molas que so comprimem nao convergiram em ") + std::to_string(maxIterations)
                        + OBF_STR(" iteracoes; resultado rejeitado (aumente o limite ou revise o modelo).");
                return false;
            }
            // verificacao a posteriori da consistencia de contato: mola ativa nao pode tracionar; mola inativa nao pode penetrar
            {
                const Eigen::VectorXd &u = displacements[cidx];
                double uscale = 0.0;
                for (int n = 0; n < nn; ++n) {
                    uscale = std::max(uscale, u.segment<3>(6 * n).cwiseAbs().maxCoeff());
                }
                const double tol = 1e-9 * std::max(uscale, 1e-300);
                for (std::size_t i = 0; i < springs.size(); ++i) {
                    const Spring &sp = springs[i];
                    if (!sp.compressionOnly || !checkIndex(sp.node, nn)) {
                        continue;
                    }
                    const double un = u.segment<3>(6 * sp.node).dot(sp.normal.normalized());
                    if ((active[i] && un > tol) || (!active[i] && un < -tol)) {
                        error = OBF_STR("Caso ") + std::to_string(cidx + 1) + OBF_STR(": configuracao de contato inconsistente com os deslocamentos (mola ativa tracionada ou inativa penetrando); resultado rejeitado.");
                        return false;
                    }
                }
            }
        }
        springIterations = 0;
        for (int c = 0; c < caseCount; ++c) {
            springIterations = std::max(springIterations, caseIterations[c]);
        }
    }

    // ---- resultados -----------------------------------------------------------------------------------------------------
    for (int cidx = 0; cidx < caseCount; ++cidx) {
        const Eigen::SparseMatrix<double> kTotalFinal = kStruct + kSpringCase[cidx];
        CaseResult &r = results[cidx];
        const Eigen::VectorXd &u = displacements[cidx];
        r.displacements.assign(u.data(), u.data() + nd);
        const Eigen::VectorXd force = kTotalFinal * u - rhs[cidx];
        const Eigen::VectorXd springForce = kSpringCase[cidx] * u;
        r.reactions.assign(nd, 0.0);
        for (int d = 0; d < nd; ++d) {
            r.reactions[d] = (freeIndex[d] < 0) ? force(d) : -springForce(d);
        }
        {
            // equilibrio global de forcas (translacoes): soma das reacoes + soma das cargas deve se anular. So se verifica quando ha forca translacional
            // significativa (cargas so de momento produzem reacoes de ruido). Limite 1e-4: ~3x o pior desbalanco medido com a penalidade padrao 1e7
            // (3,1e-5 em placa fina, Etapa 2/WP3); com penalidade >= 1e8 o desbalanco chega a 4e-3 a 1e-1 e o resultado e rejeitado.
            double sumR[3] = {0, 0, 0}, sumF[3] = {0, 0, 0}, mag = 0.0, totalMag = 0.0;
            for (int n = 0; n < nn; ++n) {
                for (int c = 0; c < 3; ++c) {
                    sumR[c] += r.reactions[6 * n + c];
                    sumF[c] += rhs[cidx](6 * n + c);
                    mag += std::abs(r.reactions[6 * n + c]) + std::abs(rhs[cidx](6 * n + c));
                }
            }
            for (int d = 0; d < nd; ++d) {
                totalMag += std::abs(rhs[cidx](d));
            }
            double worst = 0.0;
            for (int c = 0; c < 3; ++c) {
                worst = std::max(worst, std::abs(sumR[c] + sumF[c]));
            }
            if (mag > 1e-6 * totalMag && worst > 1e-4 * mag) {
                error = OBF_STR("Caso ") + std::to_string(cidx + 1) + OBF_STR(": equilibrio global de forcas violado (soma das reacoes e das cargas); resultado rejeitado.");
                return false;
            }
        }
        // temperatura por casca (para as resultantes)
        std::vector<double> tu(shells.size(), 0.0), tg(shells.size(), 0.0);
        for (const ShellThermal &t : cases[cidx].thermal) {
            if (checkIndex(t.shell, static_cast<int>(shells.size()))) {
                tu[t.shell] += t.uniform;
                tg[t.shell] += t.gradient;
            }
        }
        r.shells.resize(shells.size());
        for (std::size_t e = 0; e < shells.size(); ++e) {
            const ShellData &d = shellData[e];
            Vec24 ug;
            for (int i = 0; i < 24; ++i) {
                ug(i) = u(d.dofs[i]);
            }
            const Vec24 ul = d.transform * ug;
            const Shell &sh = shells[e];
            r.shells[e] = shellResultants(d.frame, materials[sh.material], sh.thickness, sh.shearCorrection, d.stiffness, ul, tu[e], tg[e]);
        }
        r.beams.resize(beams.size());
        for (std::size_t e = 0; e < beams.size(); ++e) {
            const BeamData &d = beamData[e];
            Vec12 ug;
            for (int i = 0; i < 12; ++i) {
                ug(i) = u(d.dofs[i]);
            }
            Vec12 fl = d.local * (d.transform * ug);
            // desconta a carga distribuida (forcas de engastamento perfeito)
            for (const BeamUniform &q : cases[cidx].beamUniform) {
                if (q.beam != static_cast<int>(e)) {
                    continue;
                }
                const Beam &b = beams[e];
                const BeamFrame fr = beamFrame(nodes[b.nodes[0]].x, nodes[b.nodes[1]].x, b.refY);
                const double qx = q.q.dot(fr.ex), qy = q.q.dot(fr.ey), qz = q.q.dot(fr.ez), L = d.length;
                Vec12 fe = Vec12::Zero();
                fe(0) = qx * L / 2.0;
                fe(6) = qx * L / 2.0;
                fe(1) = qy * L / 2.0;
                fe(7) = qy * L / 2.0;
                fe(5) = qy * L * L / 12.0;
                fe(11) = -qy * L * L / 12.0;
                fe(2) = qz * L / 2.0;
                fe(8) = qz * L / 2.0;
                fe(4) = -qz * L * L / 12.0;
                fe(10) = qz * L * L / 12.0;
                fl -= fe;
            }
            std::array<double, 12> out;
            for (int i = 0; i < 12; ++i) {
                out[i] = fl(i);
            }
            r.beams[e] = out;
        }
    }
    return true;
}

} // namespace fem
