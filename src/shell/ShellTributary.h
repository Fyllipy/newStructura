#pragma once

#include "shell/ShellModel.h"

#include <string>
#include <vector>

namespace Structura::Shell {

/// Barra (segmento reto, metros) candidata a apoiar a casca.
struct TributaryBar {
    int id {0};
    Vec3 a {};
    Vec3 b {};
};

/// Carga distribuida resultante em uma barra (N/m, eixos globais).
struct TributaryLoad {
    int barId {0};
    Vec3 q {};
};

/// Quinhoes de carga de uma chapa plana apoiada em barras (metodo da envoltoria: cada ponto da chapa descarrega na aresta apoiada
/// mais proxima; para retangulos resulta nos triangulos e trapezios a 45 graus). `force` e a forca por area (N/m2, eixos globais) que
/// atua na chapa. As barras apoiam a chapa quando estao sobre uma aresta (a ate `tol` metros). Devolve uma carga uniforme por barra
/// (media do quinhao sobre o trecho da aresta que a barra cobre), de modo que barras divididas descrevem melhor o trapezio.
/// Arestas sem barra nao apoiam (a carga delas vai as arestas apoiadas). `unsupportedArea` recebe a area sem aresta apoiada.
std::vector<TributaryLoad> tributaryLoads(const Object &plate, const Vec3 &force, const std::vector<TributaryBar> &bars, double tol,
                                          std::vector<std::string> *warnings = nullptr);

/// Forca por area (N/m2) de uma carga de casca sobre uma chapa plana; vazio se a carga nao e superficial (hidrostatica, temperatura).
bool surfaceForce(const Model &model, const Object &plate, const Load &load, Vec3 &force);

} // namespace Structura::Shell
