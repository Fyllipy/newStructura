// Verificacao do MEF de cascas contra solucoes analiticas.
#include "fem_model.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string &what, double value = 0.0, double expected = 0.0)
{
    std::printf("%s  %s  (obtido %.6g, esperado %.6g)\n", ok ? "[ok]  " : "[FALHA]", what.c_str(), value, expected);
    if (!ok) {
        ++failures;
    }
}

void near(double value, double expected, double relTol, const std::string &what)
{
    const double err = std::abs(value - expected) / std::max(std::abs(expected), 1e-30);
    check(err <= relTol, what + " (erro " + std::to_string(100.0 * err) + " %)", value, expected);
}

/// Malha retangular nx x ny em [0,Lx] x [0,Ly] no plano z = 0; `warp` permite deslocar os nos internos.
struct Grid {
    int nx, ny;
    int node(int i, int j) const { return j * (nx + 1) + i; }
};

fem::Model rectangle(double lx, double ly, int nx, int ny, double t, double E, double nu, const std::function<void(double &, double &, int, int)> &distort = {})
{
    fem::Model m;
    fem::Material mat;
    mat.id = 1;
    mat.E = E;
    mat.nu = nu;
    m.materials.push_back(mat);
    const Grid g {nx, ny};
    for (int j = 0; j <= ny; ++j) {
        for (int i = 0; i <= nx; ++i) {
            fem::Node n;
            n.id = g.node(i, j);
            double x = lx * i / nx, y = ly * j / ny;
            if (distort) {
                distort(x, y, i, j);
            }
            n.x = fem::Vec3(x, y, 0.0);
            m.nodes.push_back(n);
        }
    }
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            fem::Shell s;
            s.id = j * nx + i;
            s.nodes = {g.node(i, j), g.node(i + 1, j), g.node(i + 1, j + 1), g.node(i, j + 1)};
            s.material = 0;
            s.thickness = t;
            m.shells.push_back(s);
        }
    }
    return m;
}

void membranePatchTest()
{
    // tracao uniforme em chapa com malha distorcida: tensao constante e igual a P/(H t)
    const double L = 2.0, H = 1.0, t = 0.1, E = 2.0e11, nu = 0.3;
    fem::Model m = rectangle(L, H, 4, 4, t, E, nu, [&](double &x, double &y, int i, int j) {
        if (i > 0 && i < 4 && j > 0 && j < 4) {
            x += 0.07 * std::sin(3.0 * i + j);
            y += 0.05 * std::cos(2.0 * j + i);
        }
    });
    const Grid g {4, 4};
    for (int j = 0; j <= 4; ++j) {
        fem::Node &left = m.nodes[g.node(0, j)];
        left.fixed[0] = 1;   // ux = 0
    }
    m.nodes[g.node(0, 0)].fixed[1] = 1;
    // fixa w e rotacoes de flexao em todos os nos (so membrana)
    for (auto &n : m.nodes) {
        n.fixed[2] = n.fixed[3] = n.fixed[4] = 1;
    }
    fem::LoadCase lc;
    const double sigma = 1.0e6;   // N/m2
    for (int j = 0; j <= 4; ++j) {
        const double w = (j == 0 || j == 4) ? 0.5 : 1.0;
        fem::NodalLoad l;
        l.node = g.node(4, j);
        l.f[0] = sigma * t * (H / 4.0) * w;
        lc.nodal.push_back(l);
    }
    m.cases.push_back(lc);
    std::string error;
    { const bool solved = m.solve({}, error); check(solved, "patch de membrana resolve: " + error); }
    double worstDev = 0.0;
    for (const auto &s : m.results[0].shells) {
        // resultantes locais -> eixos globais (x, y)
        const double e1x = s.axes[0], e1y = s.axes[1], e2x = s.axes[3], e2y = s.axes[4];
        for (int p = 0; p < 5; ++p) {
            const double n11 = s.values[8 * p], n22 = s.values[8 * p + 1], n12 = s.values[8 * p + 2];
            const double nxx = n11 * e1x * e1x + n22 * e2x * e2x + 2.0 * n12 * e1x * e2x;
            const double nyy = n11 * e1y * e1y + n22 * e2y * e2y + 2.0 * n12 * e1y * e2y;
            const double nxy = n11 * e1x * e1y + n22 * e2x * e2y + n12 * (e1x * e2y + e1y * e2x);
            worstDev = std::max(worstDev, std::abs(nxx - sigma * t) / (sigma * t));
            worstDev = std::max(worstDev, std::abs(nyy) / (sigma * t));
            worstDev = std::max(worstDev, std::abs(nxy) / (sigma * t));
        }
    }
    check(worstDev < 1e-6, "patch de membrana: tensao constante em malha distorcida", worstDev, 0.0);
}

void cantileverInPlane()
{
    const double L = 10.0, H = 1.0, t = 0.1, E = 2.0e11, nu = 0.3, P = 1.0e5;
    fem::Model m = rectangle(L, H, 40, 4, t, E, nu);
    const Grid g {40, 4};
    for (auto &n : m.nodes) {
        n.fixed[2] = n.fixed[3] = n.fixed[4] = 1;
    }
    for (int j = 0; j <= 4; ++j) {
        m.nodes[g.node(0, j)].fixed[0] = m.nodes[g.node(0, j)].fixed[1] = 1;
    }
    fem::LoadCase lc;
    for (int j = 0; j <= 4; ++j) {
        const double w = (j == 0 || j == 4) ? 0.5 : 1.0;
        fem::NodalLoad l;
        l.node = g.node(40, j);
        l.f[1] = P / 4.0 * w;
        lc.nodal.push_back(l);
    }
    m.cases.push_back(lc);
    std::string error;
    { const bool solved = m.solve({}, error); check(solved, "viga em balanco no plano resolve: " + error); }
    const double I = t * H * H * H / 12.0;
    const double G = E / (2.0 * (1.0 + nu));
    const double expected = P * L * L * L / (3.0 * E * I) + P * L / (5.0 / 6.0 * G * t * H);
    double tip = 0.0;
    for (int j = 0; j <= 4; ++j) {
        tip += m.results[0].displacements[6 * g.node(40, j) + 1] / 5.0;
    }
    near(tip, expected, 0.03, "flecha da viga em balanco (membrana com modos incompativeis)");
}

void simplySupportedPlate()
{
    const double a = 1.0, t = 0.01, E = 2.1e11, nu = 0.3, q = 1000.0;
    const int n = 16;
    fem::Model m = rectangle(a, a, n, n, t, E, nu);
    const Grid g {n, n};
    for (int j = 0; j <= n; ++j) {
        for (int i = 0; i <= n; ++i) {
            fem::Node &node = m.nodes[g.node(i, j)];
            if (i == 0 || i == n || j == 0 || j == n) {
                node.fixed[2] = 1;
            }
            node.fixed[0] = node.fixed[1] = 1;   // sem membrana
        }
    }
    fem::LoadCase lc;
    for (std::size_t e = 0; e < m.shells.size(); ++e) {
        lc.pressure.push_back({static_cast<int>(e), {q, q, q, q}});
    }
    m.cases.push_back(lc);
    std::string error;
    { const bool solved = m.solve({}, error); check(solved, "placa apoiada resolve: " + error); }
    const double D = E * t * t * t / (12.0 * (1.0 - nu * nu));
    const double expected = 0.00406 * q * a * a * a * a / D;
    const double center = m.results[0].displacements[6 * g.node(n / 2, n / 2) + 2];
    near(center, expected, 0.01, "flecha central da placa apoiada (MITC4, t/a = 0,01)");
    // momento central: 0,0479 q a2
    const auto &s = m.results[0].shells[(n / 2) * n + n / 2];
    near(std::abs(s.values[8 * 4 + 3]), 0.0479 * q * a * a, 0.03, "momento central da placa apoiada");
}

void clampedPlate()
{
    const double a = 1.0, t = 0.01, E = 2.1e11, nu = 0.3, q = 1000.0;
    const int n = 16;
    fem::Model m = rectangle(a, a, n, n, t, E, nu);
    const Grid g {n, n};
    for (int j = 0; j <= n; ++j) {
        for (int i = 0; i <= n; ++i) {
            fem::Node &node = m.nodes[g.node(i, j)];
            node.fixed[0] = node.fixed[1] = 1;
            if (i == 0 || i == n || j == 0 || j == n) {
                node.fixed[2] = node.fixed[3] = node.fixed[4] = 1;
            }
        }
    }
    fem::LoadCase lc;
    for (std::size_t e = 0; e < m.shells.size(); ++e) {
        lc.pressure.push_back({static_cast<int>(e), {q, q, q, q}});
    }
    m.cases.push_back(lc);
    std::string error;
    { const bool solved = m.solve({}, error); check(solved, "placa engastada resolve: " + error); }
    const double D = E * t * t * t / (12.0 * (1.0 - nu * nu));
    near(m.results[0].displacements[6 * g.node(n / 2, n / 2) + 2], 0.00126 * q * a * a * a * a / D, 0.02, "flecha central da placa engastada");
}

void cylinderUnderPressure()
{
    // cilindro aberto, pressao interna: n_theta = p R, deslocamento radial p R2/(E t)
    const double R = 1.0, Lc = 2.0, t = 0.01, E = 2.0e11, nu = 0.3, p = 1.0e5;
    const int nt = 32, nz = 16;
    fem::Model m;
    fem::Material mat;
    mat.id = 1;
    mat.E = E;
    mat.nu = nu;
    m.materials.push_back(mat);
    const double pi = 3.14159265358979323846;
    auto id = [&](int i, int j) { return j * nt + (i % nt); };
    for (int j = 0; j <= nz; ++j) {
        for (int i = 0; i < nt; ++i) {
            fem::Node n;
            n.id = id(i, j);
            n.x = fem::Vec3(R * std::cos(2.0 * pi * i / nt), R * std::sin(2.0 * pi * i / nt), Lc * j / nz);
            m.nodes.push_back(n);
        }
    }
    for (int j = 0; j < nz; ++j) {
        for (int i = 0; i < nt; ++i) {
            fem::Shell s;
            s.id = j * nt + i;
            s.nodes = {id(i, j), id(i + 1, j), id(i + 1, j + 1), id(i, j + 1)};
            s.material = 0;
            s.thickness = t;
            m.shells.push_back(s);
        }
    }
    // apoios: impede movimentos de corpo rigido (z em um no, x/y em dois nos sobre o eixo x e y no meio)
    m.nodes[id(0, nz / 2)].fixed[2] = 1;
    m.nodes[id(0, nz / 2)].fixed[1] = 1;
    m.nodes[id(nt / 2, nz / 2)].fixed[1] = 1;
    m.nodes[id(nt / 2, nz / 2)].fixed[2] = 1;
    m.nodes[id(nt / 4, nz / 2)].fixed[0] = 1;
    m.nodes[id(nt / 4, nz / 2)].fixed[2] = 1;   // 6 vinculos no total (A: y z; B: y z; C: x z) eliminam os 6 modos de corpo rigido
    fem::LoadCase lc;
    for (std::size_t e = 0; e < m.shells.size(); ++e) {
        lc.pressure.push_back({static_cast<int>(e), {p, p, p, p}});
    }
    m.cases.push_back(lc);
    std::string error;
    { const bool solved = m.solve({}, error); check(solved, "cilindro resolve: " + error); }
    const auto &mid = m.results[0].shells[(nz / 2) * nt + 3];
    // eixos locais: e1 tangencial (theta), e2 axial; n_theta = nxx
    near(mid.values[8 * 4 + 0], p * R, 0.02, "esforco circunferencial n_theta = p R");
    check(std::abs(mid.values[8 * 4 + 1]) < 0.01 * p * R, "esforco axial nulo no meio do cilindro livre", mid.values[8 * 4 + 1], 0.0);
    const double ur = m.results[0].displacements[6 * id(0, nz / 2)];
    near(ur, p * R * R / (E * t), 0.02, "deslocamento radial p R2/(E t)");
}

void beamTest()
{
    fem::Model m;
    fem::Material mat;
    mat.id = 1;
    mat.E = 2.0e11;
    mat.nu = 0.3;
    m.materials.push_back(mat);
    const int n = 4;
    const double L = 5.0, I = 8.0e-6, A = 5.0e-3, P = 1.0e4;
    for (int i = 0; i <= n; ++i) {
        fem::Node node;
        node.id = i;
        node.x = fem::Vec3(L * i / n, 0.0, 0.0);
        if (i == 0) {
            node.fixed = {1, 1, 1, 1, 1, 1};
        }
        m.nodes.push_back(node);
    }
    for (int i = 0; i < n; ++i) {
        fem::Beam b;
        b.id = i;
        b.nodes = {i, i + 1};
        b.material = 0;
        b.A = A;
        b.Iy = I;
        b.Iz = I;
        b.J = 1.0e-5;
        b.refY = fem::Vec3(0.0, 0.0, 1.0);
        m.beams.push_back(b);
    }
    fem::LoadCase lc;
    fem::NodalLoad l;
    l.node = n;
    l.f[2] = -P;
    lc.nodal.push_back(l);
    m.cases.push_back(lc);
    std::string error;
    { const bool solved = m.solve({}, error); check(solved, "barra em balanco resolve: " + error); }
    near(m.results[0].displacements[6 * n + 2], -P * L * L * L / (3.0 * mat.E * I), 1e-9, "flecha da barra em balanco");
    near(std::abs(m.results[0].reactions[2]), P, 1e-9, "reacao vertical da barra");
    near(std::abs(m.results[0].reactions[4]), P * L, 1e-9, "momento de engastamento da barra");
}

void compressionOnlySprings()
{
    // viga sobre molas que so comprimem: carga uniforme para baixo e forca para cima na ponta (gangorra): a ponta levanta
    fem::Model m;
    fem::Material mat;
    mat.id = 1;
    m.materials.push_back(mat);
    const int n = 8;
    const double L = 4.0, q = 2.0e4, up = 2.0e4;
    for (int i = 0; i <= n; ++i) {
        fem::Node node;
        node.id = i;
        node.x = fem::Vec3(L * i / n, 0.0, 0.0);
        node.fixed = {1, 1, 0, 1, 0, 1};
        m.nodes.push_back(node);
    }
    for (int i = 0; i < n; ++i) {
        fem::Beam b;
        b.id = i;
        b.nodes = {i, i + 1};
        b.material = 0;
        b.A = 1.0e-2;
        b.Iy = 5.0e-5;
        b.Iz = 5.0e-5;
        b.J = 1.0e-5;
        m.beams.push_back(b);
    }
    for (int i = 0; i <= n; ++i) {
        fem::Spring s;
        s.node = i;
        s.k = {1.0e7, 0.0, 0.0, 0.0, 0.0, 0.0};   // k[0] = normal, k[1] = tangencial
        s.compressionOnly = true;
        s.normal = fem::Vec3(0.0, 0.0, 1.0);
        m.springs.push_back(s);
    }
    fem::LoadCase lc;
    for (int i = 0; i < n; ++i) {
        lc.beamUniform.push_back({i, fem::Vec3(0.0, 0.0, -q)});
    }
    fem::NodalLoad lift;
    lift.node = n;
    lift.f[2] = up;
    lc.nodal.push_back(lift);
    m.cases.push_back(lc);
    std::string error;
    const bool solved = m.solve({}, error);
    check(solved, "viga sobre molas so-compressao resolve: " + error);
    double sum = 0.0, minReaction = 0.0;
    for (int i = 0; i <= n; ++i) {
        const double r = m.results[0].reactions[6 * i + 2];
        sum += r;
        minReaction = std::min(minReaction, r);
    }
    near(sum, q * L - up, 1.0e-6, "equilibrio vertical da viga sobre molas");
    check(minReaction > -1.0e-6, "nenhuma mola traciona", minReaction, 0.0);
    check(m.springIterations > 1, "molas desativadas por iteracao", m.springIterations, 2.0);
    check(m.results[0].displacements[6 * n + 2] > 0.0, "a ponta levantada sobe", m.results[0].displacements[6 * n + 2], 0.0);
}

void nonConformingTies()
{
    // viga-placa em balanço (flexao fora do plano) com malhas de refinamento diferente dos dois lados da junta, ligadas por ties
    const double L = 4.0, W = 1.0, t = 0.1, E = 2.0e11, nu = 0.3, P = 1.0e4;
    auto tipDeflection = [&](fem::Model &m, const std::vector<int> &tipNodes) {
        fem::LoadCase lc;
        for (const int n : tipNodes) {
            fem::NodalLoad l;
            l.node = n;
            l.f[2] = -P / static_cast<double>(tipNodes.size());
            lc.nodal.push_back(l);
        }
        m.cases.push_back(lc);
        std::string error;
        const bool solved = m.solve({}, error);
        check(solved, "viga-placa resolve: " + error);
        double w = 0.0;
        for (const int n : tipNodes) {
            w += m.results[0].displacements[6 * n + 2] / static_cast<double>(tipNodes.size());
        }
        return w;
    };
    // referencia: malha uniforme 8 x 4
    fem::Model uniform = rectangle(L, W, 8, 4, t, E, nu);
    const Grid gu {8, 4};
    std::vector<int> tipU;
    for (int j = 0; j <= 4; ++j) {
        for (int c = 0; c < 6; ++c) {
            uniform.nodes[gu.node(0, j)].fixed[c] = 1;
        }
        tipU.push_back(gu.node(8, j));
    }
    const double reference = tipDeflection(uniform, tipU);
    // malha mista: bloco esquerdo [0,2] com 2 divisoes em y, bloco direito [2,4] com 4 divisoes em y
    fem::Model m;
    fem::Material mat;
    mat.id = 1;
    mat.E = E;
    mat.nu = nu;
    m.materials.push_back(mat);
    auto addBlock = [&](double x0, double x1, int nx, int ny, int &firstNode) {
        firstNode = static_cast<int>(m.nodes.size());
        for (int j = 0; j <= ny; ++j) {
            for (int i = 0; i <= nx; ++i) {
                fem::Node n;
                n.id = static_cast<int>(m.nodes.size());
                n.x = fem::Vec3(x0 + (x1 - x0) * i / nx, W * j / ny, 0.0);
                m.nodes.push_back(n);
            }
        }
        const Grid g {nx, ny};
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                fem::Shell sh;
                sh.id = static_cast<int>(m.shells.size());
                sh.nodes = {firstNode + g.node(i, j), firstNode + g.node(i + 1, j), firstNode + g.node(i + 1, j + 1), firstNode + g.node(i, j + 1)};
                sh.thickness = t;
                m.shells.push_back(sh);
            }
        }
    };
    int left = 0, right = 0;
    addBlock(0.0, 2.0, 4, 2, left);
    addBlock(2.0, 4.0, 4, 4, right);
    const Grid gl {4, 2}, gr {4, 4};
    std::vector<int> tip;
    for (int j = 0; j <= 2; ++j) {
        for (int c = 0; c < 6; ++c) {
            m.nodes[left + gl.node(0, j)].fixed[c] = 1;
        }
    }
    for (int j = 0; j <= 4; ++j) {
        tip.push_back(right + gr.node(4, j));
        // no da coluna esquerda do bloco direito: escravo da aresta do bloco esquerdo (entre y = 0..0.5..1)
        const double y = W * j / 4.0;
        const int jl = std::min(static_cast<int>(y / 0.5), 1);
        const double w = (y - 0.5 * jl) / 0.5;
        fem::Tie tie;
        tie.slave = right + gr.node(0, j);
        tie.masters = {left + gl.node(4, jl), left + gl.node(4, jl + 1)};
        tie.weights = {1.0 - w, w};
        m.ties.push_back(tie);
    }
    const double mixed = tipDeflection(m, tip);
    near(mixed, reference, 0.03, "malha nao conforme ligada por ties reproduz a malha uniforme (flecha na ponta)");
    // trecho tracionado: deslocamentos do no escravo seguem a interpolacao do mestre
    const int slave = right + gr.node(0, 1);
    const double wSlave = m.results[0].displacements[6 * slave + 2];
    const double wInterp = 0.5 * (m.results[0].displacements[6 * (left + gl.node(4, 0)) + 2] + m.results[0].displacements[6 * (left + gl.node(4, 1)) + 2]);
    near(wSlave, wInterp, 1e-5, "no pendente segue a interpolacao da aresta mestra");
}

void mechanismIsRejected()
{
    // chapa com rotacao de corpo rigido no plano livre (apoios insuficientes): deve ser erro, nao "sucesso" com deslocamentos absurdos
    fem::Model m = rectangle(1.0, 1.0, 1, 1, 0.1, 2.0e11, 0.3);
    m.nodes[0].fixed = {1, 1, 1, 0, 0, 0};
    m.nodes[1].fixed = {0, 0, 1, 0, 0, 0};
    m.nodes[2].fixed = {0, 0, 1, 0, 0, 0};
    fem::LoadCase lc;
    fem::NodalLoad l;
    l.node = 3;
    l.f[0] = 1000.0;
    lc.nodal.push_back(l);
    m.cases.push_back(lc);
    std::string error;
    check(!m.solve({}, error), "mecanismo (rotacao no plano livre) e rejeitado: " + error);
}

} // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    membranePatchTest();
    cantileverInPlane();
    simplySupportedPlate();
    clampedPlate();
    cylinderUnderPressure();
    beamTest();
    compressionOnlySprings();
    nonConformingTies();
    mechanismIsRejected();
    std::printf("\n%s (%d falhas)\n", failures == 0 ? "TODOS OS TESTES PASSARAM" : "HA FALHAS", failures);
    return failures == 0 ? 0 : 1;
}
