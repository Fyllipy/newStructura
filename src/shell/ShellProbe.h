#pragma once

#include "shell/ShellVisual.h"

#include <array>
#include <string>

namespace Structura::Shell {

/// Funcoes de forma do Q4 isoparametrico bilinear em (xi, eta) em [-1, 1]; nos na ordem do elemento
/// (-1,-1), (+1,-1), (+1,+1), (-1,+1), a mesma de MeshQuad::n e dos 4 vertices de cada quadrilatero do RenderMesh.
std::array<double, 4> q4Shape(double xi, double eta);

/// Interpola valores nodais com as funcoes de forma do Q4. NaN se algum no com peso nao nulo e invalido (NaN/infinito):
/// um valor invalido nunca e "completado" por vizinhos.
double q4Interpolate(const std::array<double, 4> &nodalValues, double xi, double eta);

/// Coordenadas naturais (xi, eta) do ponto da superficie bilinear do elemento mais proximo de `p` (Gauss-Newton em 3D, vale para
/// elementos distorcidos e empenados). Devolve false se a iteracao nao convergir ou o elemento for degenerado. `distance` recebe a
/// distancia entre `p` e a superficie; `outside` e verdadeiro se (xi, eta) caiu fora de [-1,1] alem da tolerancia (as coordenadas
/// devolvidas ja sao limitadas a [-1,1]).
bool q4NaturalCoordinates(const std::array<std::array<double, 3>, 4> &corners, const std::array<double, 3> &p, double &xi, double &eta, double *distance = nullptr,
                          bool *outside = nullptr);

/// Resultado de uma consulta (probe) em um ponto de um quadrilatero da malha colorida.
struct ProbeHit {
    bool valid {false};          ///< a geometria foi resolvida (quadrilatero existe e o ponto foi localizado)
    int quad {-1};               ///< indice do quadrilatero no RenderMesh
    int element {-1};            ///< numero do elemento (indice em Mesh::quads)
    int objectIndex {-1};
    int objectId {-1};
    std::string objectName;
    double xi {0.0}, eta {0.0};  ///< coordenadas naturais do ponto consultado
    bool outside {false};        ///< o ponto tocado ficou fora do elemento (borda/aresta): usado o ponto mais proximo
    double distance {0.0};       ///< distancia (unidades da cena) entre o ponto tocado e a superficie do elemento
    bool hasValue {false};       ///< ha valor valido na grandeza exibida
    double value {0.0};          ///< valor EXIBIDO (o que colore a malha: alisado se `smoothed`) no ponto, unidade de exibicao
    bool hasSolverValue {false};
    double solverValue {0.0};    ///< valor do PROPRIO elemento no ponto (sem media com vizinhos): a distincao entre exibicao e solver
    bool smoothed {false};
    std::string title, unit;
    std::array<double, 3> position {{0.0, 0.0, 0.0}};      ///< coordenadas globais do ponto do elemento SEM deformada (m)
    std::array<double, 3> viewPosition {{0.0, 0.0, 0.0}};  ///< posicao na vista (deformada, se ativa), em m
    bool deformed {false};
};

/// Consulta o quadrilatero `quad` do `mesh` no ponto `viewPoint` (coordenadas da cena, ex.: ponto do picker). Nao altera o RenderMesh.
ProbeHit probeQuad(const RenderMesh &mesh, int quad, const std::array<double, 3> &viewPoint);

/// Texto de uma linha para a barra de status/tooltip, ja formatado ("Elemento 12 (Laje) | Mxx = 3,2 kN.m/m | ...").
std::string describeProbe(const ProbeHit &hit);

} // namespace Structura::Shell
