#pragma once

#include "shell/ShellModel.h"

#include <optional>
#include <string>
#include <vector>

namespace Structura::Shell {

struct MeshQuad {
    std::array<int, 4> n {{0, 0, 0, 0}};   ///< nos (indices), anti-horarios vistos do lado da normal positiva
    int object {0};                        ///< indice do objeto em Model::objects
    int zone {0};                          ///< indice em Mesh::zones
    double thickness {0.0};                ///< m
    int material {0};                      ///< indice em Model::materials
};

/// Ligacao de um no "pendente" (no de borda de um objeto que cai sobre aresta ou face de elemento de outro objeto, em malhas nao
/// conformes) aos nos do elemento hospedeiro: u_escravo = soma(w_i u_mestre_i).
struct Tie {
    int slave {0};
    std::vector<int> masters;
    std::vector<double> weights;
};

struct Mesh {
    std::vector<Vec3> nodes;
    std::vector<MeshQuad> quads;
    std::vector<std::string> zones;   ///< "objeto: zona" (fundo, virola 1, tampo, ...)
    std::vector<Tie> ties;            ///< nos pendentes ligados ao elemento vizinho (malhas nao conformes)
    std::vector<std::string> warnings;
    [[nodiscard]] bool empty() const { return quads.empty(); }
};

/// Gera a malha de quadrilateros de todos os objetos visiveis. Nos coincidentes (tolerancia de 1 micrometro) sao unidos,
/// de modo que partes que se tocam ficam conectadas. Placas de 4 vertices usam malha estruturada; poligonos gerais sao
/// triangulados e cada triangulo vira 3 quadrilateros, refinados ate a dimensao pedida.
Mesh generateMesh(const Model &model);

/// Nos de borda de um objeto que ficam sobre aresta ou face de elemento de outro objeto sem coincidir com seus nos: ficam presos por
/// interpolacao aos nos do hospedeiro (o lado mais fino e sempre o escravo). Chamada por generateMesh (resultado em Mesh::ties).
std::vector<Tie> findEdgeTies(const Model &model, const Mesh &mesh);

/// Elemento que contem o ponto `x` (a ate `tol` de distancia da superficie): ligacao bilinear (escravo = -1) aos nos dele.
/// Vazio se nenhum elemento contem o ponto. Pontos que coincidem com um no devolvem uma ligacao de um unico mestre.
std::optional<Tie> hostTie(const Mesh &mesh, const Vec3 &x, double tol);

/// Normal do quadrilatero (unitaria, pela media das diagonais) e area.
Vec3 quadNormal(const Mesh &mesh, const MeshQuad &quad);
double quadArea(const Mesh &mesh, const MeshQuad &quad);

} // namespace Structura::Shell
