#pragma once

#include "shell/ShellMesh.h"
#include "shell/ShellModel.h"

#include <array>
#include <string>
#include <vector>

namespace Structura::Shell {

/// Tolerancia (m) para encontrar o no da malha de cada ponto de uma carga dada por clique (mesma dos apoios por nos).
inline constexpr double kPointLoadTolerance = 1.0e-5;

/// Cargas definidas por pontos (selecao de nos na vista): forca em nos, carga linear sobre arestas, carga de superficie sobre elementos.
inline bool isPointLoad(Load::Type t)
{
    return t == Load::NodalPoints || t == Load::LinePoints || t == Load::RegionPoints;
}

/// Alvos de uma carga por pontos na malha atual.
struct ResolvedPointLoad {
    int load {-1};                                   ///< indice em Model::loads
    std::vector<int> nodes;                          ///< nos encontrados (todos os tipos)
    std::vector<std::array<int, 2>> edges;           ///< LinePoints: arestas de elemento com os DOIS nos selecionados (sem repeticao)
    std::vector<int> elements;                       ///< RegionPoints: elementos com os QUATRO nos selecionados
    int missing {0};                                 ///< pontos sem no na malha atual (a malha foi refeita?)
};

ResolvedPointLoad resolvePointLoad(const Model &model, const Mesh &mesh, std::size_t loadIndex);

/// Comprimento (m) de uma aresta.
double edgeLength(const Mesh &mesh, int a, int b);

/// Forcas nodais equivalentes de uma carga por pontos (N, globais): NodalPoints = F em cada no; LinePoints = q L / 2 nos dois nos de cada aresta;
/// RegionPoints (modos 0/1) = q A / 4 em cada no do elemento. Regiao em modo normal (v[3] = 2) NAO gera forcas nodais: vira pressao por elemento
/// (`pressure`: elemento, valor em Pa positivo na normal).
struct PointLoadEffect {
    std::vector<std::pair<int, std::array<double, 3>>> nodalForces;
    std::vector<std::pair<int, double>> pressures;
    std::vector<std::string> warnings;
};
PointLoadEffect pointLoadEffect(const Model &model, const Mesh &mesh, std::size_t loadIndex);

/// Seta para desenhar uma carga por pontos: o corpo termina na posicao (m), na direcao `direction` (unitaria). `magnitude` (N, N/m ou N/m2)
/// e a base do comprimento relativo dentro do mesmo tipo.
struct LoadArrow {
    int load {-1};
    int kind {0};                                    ///< 0 forca nodal, 1 carga linear, 2 carga de superficie
    std::array<double, 3> position {{0.0, 0.0, 0.0}};
    std::array<double, 3> direction {{0.0, 0.0, -1.0}};
    double magnitude {0.0};
    std::string caseName;
};
std::vector<LoadArrow> loadArrows(const Model &model, const Mesh &mesh);

} // namespace Structura::Shell
