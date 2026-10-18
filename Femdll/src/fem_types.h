#pragma once

#include <Eigen/Dense>
#include <Eigen/Sparse>

#include <array>
#include <map>
#include <string>
#include <vector>

/// Nucleo do MEF de cascas/chapas e barras (unidades consistentes do chamador, p.ex. N, m, Pa).
namespace fem {

using Vec3 = Eigen::Vector3d;
using Mat3 = Eigen::Matrix3d;

struct Material {
    int id {0};
    double E {2.0e11};
    double nu {0.3};
    double density {0.0};   ///< massa especifica (modal)
    double alpha {1.2e-5};  ///< dilatacao termica
    double G() const { return E / (2.0 * (1.0 + nu)); }
};

struct Node {
    int id {0};
    Vec3 x {Vec3::Zero()};
    std::array<int, 6> fixed {{0, 0, 0, 0, 0, 0}};
};

/// Casca quadrilatera de 4 nos (Q4): membrana com modos incompativeis + penalidade de rotacao no plano, flexao de Mindlin com MITC4.
struct Shell {
    int id {0};
    std::array<int, 4> nodes {{0, 0, 0, 0}};   ///< indices (nao ids) dos nos, em ordem anti-horaria vista do lado da normal
    int material {0};                           ///< indice
    double thickness {0.0};
    double shearCorrection {5.0 / 6.0};
};

/// Barra espacial de Euler-Bernoulli (reforcos, aneis, vigas).
struct Beam {
    int id {0};
    std::array<int, 2> nodes {{0, 0}};
    int material {0};
    double A {0.0}, Iy {0.0}, Iz {0.0}, J {0.0};
    Vec3 refY {0.0, 0.0, 1.0};   ///< vetor de referencia: o eixo local y fica no plano (x, refY)
};

/// Mola nodal (apoio elastico). Com `compressionOnly`, atua so quando o deslocamento ao longo de `normal` e contra o apoio.
struct Spring {
    int node {0};
    std::array<double, 6> k {{0, 0, 0, 0, 0, 0}};   ///< rigidezes globais (ux uy uz rx ry rz)
    bool compressionOnly {false};
    Vec3 normal {0.0, 0.0, 1.0};                    ///< do apoio para a estrutura
};

/// Ligacao nó-escravo a nos mestres (nos pendentes em arestas de malhas nao conformes, nos de barras sobre cascas):
/// u_escravo = soma(w_i u_mestre_i) nos 6 graus de liberdade, imposta por penalidade proporcional a rigidez do proprio no.
struct Tie {
    int slave {0};
    std::vector<int> masters;
    std::vector<double> weights;
};

struct ShellPressure {
    int shell {0};
    std::array<double, 4> p {{0, 0, 0, 0}};   ///< pressao nodal, positiva no sentido da normal do elemento
};

struct ShellBody {
    int shell {0};
    Vec3 b {Vec3::Zero()};   ///< forca por unidade de volume (global)
};

struct ShellThermal {
    int shell {0};
    double uniform {0.0};    ///< variacao uniforme de temperatura
    double gradient {0.0};   ///< T(topo, +normal) - T(base): curvatura livre alpha*gradient/t
};

struct NodalLoad {
    int node {0};
    std::array<double, 6> f {{0, 0, 0, 0, 0, 0}};
};

struct BeamUniform {
    int beam {0};
    Vec3 q {Vec3::Zero()};   ///< carga distribuida global por unidade de comprimento
};

struct LoadCase {
    std::vector<NodalLoad> nodal;
    std::vector<ShellPressure> pressure;
    std::vector<ShellBody> body;
    std::vector<ShellThermal> thermal;
    std::vector<BeamUniform> beamUniform;
};

/// Resultantes por elemento: 5 pontos (4 cantos e o centro) x (nxx nyy nxy mxx myy mxy qx qy) + eixos locais e1 e2 e3.
struct ShellResult {
    static constexpr int kPoints = 5;
    static constexpr int kValues = 8;
    std::array<double, kPoints * kValues> values {};
    std::array<double, 9> axes {};
};

struct CaseResult {
    std::vector<double> displacements;   ///< 6 por no
    std::vector<double> reactions;       ///< 6 por no (apoios e molas)
    std::vector<ShellResult> shells;
    std::vector<std::array<double, 12>> beams;   ///< esforcos nas extremidades, eixos locais
};

} // namespace fem
