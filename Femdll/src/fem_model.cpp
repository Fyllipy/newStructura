#include "fem_model.h"

#include "shell_q4.h"

#include <Eigen/SparseCholesky>

#include <algorithm>
#include <cmath>

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
        error = "Modelo sem nos ou sem elementos.";
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
            error = "Casca " + std::to_string(sh.id) + ": material ou espessura invalidos.";
            return false;
        }
        std::array<Vec3, 4> p;
        for (int i = 0; i < 4; ++i) {
            if (!checkIndex(sh.nodes[i], nn)) {
                error = "Casca " + std::to_string(sh.id) + ": no inexistente.";
                return false;
            }
            p[i] = nodes[sh.nodes[i]].x;
        }
        ShellData &d = shellData[e];
        d.frame = shellFrame(p);
        if (d.frame.area < 1e-14) {
            error = "Casca " + std::to_string(sh.id) + ": area nula.";
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
            error = "Barra " + std::to_string(b.id) + ": dados invalidos.";
            return false;
        }
        BeamData &d = beamData[e];
        const Vec3 xi = nodes[b.nodes[0]].x, xj = nodes[b.nodes[1]].x;
        d.length = (xj - xi).norm();
        if (d.length < 1e-12) {
            error = "Barra " + std::to_string(b.id) + ": comprimento nulo.";
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
                error = "Ligacao no-aresta invalida.";
                return false;
            }
            for (const int m : t.masters) {
                if (!checkIndex(m, nn)) {
                    error = "Ligacao no-aresta: no mestre inexistente.";
                    return false;
                }
            }
            for (int c = 0; c < 6; ++c) {
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
        error = "Todos os graus de liberdade estao restritos.";
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

    // ---- molas (com iteracao para apoios que so comprimem) -------------------------------------------------------------
    std::vector<char> active(springs.size(), 1);
    auto springMatrix = [&]() {
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
    Eigen::SparseMatrix<double> kSpring;
    for (int iteration = 0; iteration < std::max(1, options.maxSpringIterations); ++iteration) {
        springIterations = iteration + 1;
        kSpring = springMatrix();
        const Eigen::SparseMatrix<double> kTotal = kStruct + kSpring;
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
            error = "Falha na fatoracao da matriz de rigidez (estrutura instavel ou apoios insuficientes).";
            return false;
        }
        for (int cidx = 0; cidx < caseCount; ++cidx) {
            Eigen::VectorXd fr(freeCount);
            for (int d = 0; d < nd; ++d) {
                if (freeIndex[d] >= 0) {
                    fr(freeIndex[d]) = rhs[cidx](d);
                }
            }
            const Eigen::VectorXd uf = solver.solve(fr);
            if (solver.info() != Eigen::Success || !uf.allFinite()) {
                error = "Falha na solucao do sistema (matriz singular).";
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
        // molas que so comprimem: desativa as tracionadas e reativa as que voltam a comprimir (pelo pior caso de cada uma)
        bool changed = false;
        for (std::size_t i = 0; i < springs.size(); ++i) {
            const Spring &sp = springs[i];
            if (!sp.compressionOnly || !checkIndex(sp.node, nn)) {
                continue;
            }
            const Vec3 n = sp.normal.normalized();
            bool wantActive = false;
            for (int cidx = 0; cidx < caseCount; ++cidx) {
                const Vec3 u = displacements[cidx].segment<3>(6 * sp.node);
                if (u.dot(n) <= 0.0) {
                    wantActive = true;
                }
            }
            // com varios casos a mola e a mesma para todos (aproximacao): ativa se algum caso comprime
            if (static_cast<bool>(active[i]) != wantActive) {
                active[i] = wantActive ? 1 : 0;
                changed = true;
            }
        }
        if (!changed) {
            break;
        }
    }

    // ---- resultados -----------------------------------------------------------------------------------------------------
    const Eigen::SparseMatrix<double> kTotalFinal = kStruct + kSpring;
    for (int cidx = 0; cidx < caseCount; ++cidx) {
        CaseResult &r = results[cidx];
        const Eigen::VectorXd &u = displacements[cidx];
        r.displacements.assign(u.data(), u.data() + nd);
        const Eigen::VectorXd force = kTotalFinal * u - rhs[cidx];
        const Eigen::VectorXd springForce = kSpring * u;
        r.reactions.assign(nd, 0.0);
        for (int d = 0; d < nd; ++d) {
            r.reactions[d] = (freeIndex[d] < 0) ? force(d) : -springForce(d);
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
