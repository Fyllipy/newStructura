#pragma once

#include "shell/ShellModel.h"

#include <optional>
#include <vector>

namespace Structura::Shell {

/// Plano de construcao: eixos u, v e normal (indices 0..2 de x, y, z).
struct PlaneSpec {
    int u {0};
    int v {1};
    int normal {2};
};

/// Construtores dos objetos desenhados no modelo 3D (coordenadas em metros). Os pontos ja estao no plano de construcao ativo.
/// Chapas, retangulos e discos saem com a normal positiva no sentido do eixo normal do plano.

/// Chapa por contorno (>= 3 vertices), com vertices em ordem anti-horaria vistos do lado de +normal.
std::optional<Object> makePlate(const std::vector<Vec3> &points, const PlaneSpec &plane);
/// Chapa retangular por dois cantos opostos.
std::optional<Object> makeRectangle(const Vec3 &a, const Vec3 &b, const PlaneSpec &plane);
/// Disco com centro e um ponto do contorno.
std::optional<Object> makeDisk(const Vec3 &center, const Vec3 &edge, const PlaneSpec &plane);
/// Extrusao de uma linha ou polilinha (fechada ou nao) segundo o vetor `v`.
std::optional<Object> makeExtrusion(const std::vector<Vec3> &path, bool closed, const Vec3 &v);
/// Cilindro, cone ou tronco (escala da extremidade em `endScale`): circulo extrudado segundo o vetor `v`.
std::optional<Object> makeCylinder(const Vec3 &center, double radius, const Vec3 &v, double endScale = 1.0);
/// Revolucao de um perfil em torno do eixo (ponto e direcao).
std::optional<Object> makeRevolution(const std::vector<Vec3> &profile, const Vec3 &axisPoint, const Vec3 &axisDirection);

/// Desloca a geometria do objeto de id `objectId` e os apoios e cargas que dependem de coordenadas dele (apoios por plano/ponto, cota do
/// liquido). `copyId` > 0 cria uma copia (novo objeto com esse id, nome com sufixo) e duplica seus apoios e cargas. false = id inexistente.
bool translateObject(Model &model, int objectId, const Vec3 &delta, int copyId = 0);

/// Dimensao de malha sugerida (~1/16 da diagonal, 1-2-5): `extra` e uma dimensao adicional a considerar.
double suggestedMeshSize(const std::vector<Vec3> &points, double extra = 0.0);

} // namespace Structura::Shell
