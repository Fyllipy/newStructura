#pragma once

#include "shell/ShellVisual.h"

#include <array>
#include <set>
#include <vector>

namespace Structura::Shell {

/// Nos unicos da malha desenhada (o RenderMesh duplica os vertices por elemento; aqui cada no aparece uma vez).
struct NodeTable {
    std::vector<int> ids;                                  ///< indice do no em Mesh::nodes
    std::vector<std::array<double, 3>> viewPos;            ///< posicao na vista (cena; deformada se ativa)
    std::vector<std::array<double, 3>> restPos;            ///< coordenadas globais SEM deformada, em metros
    std::vector<int> objectId;                             ///< id do objeto do primeiro elemento que usa o no (-1 = desconhecido)

    [[nodiscard]] std::size_t size() const noexcept { return ids.size(); }
    /// Posicao na tabela do no `id` (-1 se nao existe).
    [[nodiscard]] int indexOf(int id) const;
};

/// Constroi a tabela a partir de `RenderMesh::vertexNode`. Vazia se a malha nao traz a associacao vertice-no.
NodeTable buildNodeTable(const RenderMesh &mesh);

/// Projecao cena -> pixels do widget (origem no canto superior esquerdo). `matrix` e a matriz composta projecao*visao em
/// ordem de coluna (como QMatrix4x4::constData()), aplicada a (x, y, z, 1).
struct ScreenProjector {
    std::array<double, 16> matrix {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    double width {1.0}, height {1.0};
    /// false se o ponto esta atras da camera ou fora do volume de recorte em profundidade.
    bool project(const std::array<double, 3> &p, double &sx, double &sy, double &depth) const;
};

/// Mapa de profundidade em software (z-buffer) da malha vista pela camera: serve para saber se um no esta ESCONDIDO atras de outra
/// parte das cascas (o lado de tras de um cilindro, uma casca na frente de outra). Nao usa OpenGL.
/// A resolucao acompanha o widget (reduzida se passar de ~1,5 M de pixels).
struct DepthMap {
    int width {0}, height {0};
    int scale {1};                  ///< pixels do widget por pixel do mapa
    std::vector<float> depth;       ///< menor profundidade (NDC, menor = mais perto) por pixel; +infinito = vazio
    std::vector<float> slope;       ///< |dz/dx| + |dz/dy| do triangulo mais proximo, por pixel do mapa (base da tolerancia)
    [[nodiscard]] bool empty() const noexcept { return depth.empty(); }
};

/// Rasteriza os triangulos do RenderMesh (posicoes da vista, deformadas se for o caso). Triangulos com algum vertice atras da camera
/// sao ignorados.
DepthMap buildDepthMap(const RenderMesh &mesh, const ScreenProjector &projector);

/// true se o ponto (cena) NAO esta atras da superficie das cascas no seu pixel. A tolerancia acompanha a inclinacao da superficie
/// (nos de uma casca em angulo rasante nao se escondem de si mesmos); o minimo da vizinhanca 3x3 cobre o contorno (silhueta).
bool isNodeVisible(const DepthMap &map, const ScreenProjector &projector, const std::array<double, 3> &p);

/// No mais proximo do pixel (sx, sy) dentro de `tolerancePx`; empate (< 0,5 px) vai para o mais perto da camera. -1 se nenhum.
/// Devolve o INDICE NA TABELA, nao o id do no. `distancePx` recebe a distancia na tela. Com `occlusion`, nos escondidos nao contam.
int pickNearestNode(const NodeTable &table, const ScreenProjector &projector, double sx, double sy, double tolerancePx, double *distancePx = nullptr,
                    const DepthMap *occlusion = nullptr);

/// Nos cuja projecao cai dentro do retangulo [x0,x1] x [y0,y1] (qualquer ordem dos limites). Com `occlusion`, so os visiveis
/// (os do lado de tras de um cilindro, por exemplo, ficam de fora); sem ele, entram todos os projetados.
std::vector<int> nodesInRect(const NodeTable &table, const ScreenProjector &projector, double x0, double y0, double x1, double y1, const DepthMap *occlusion = nullptr);

/// Conjunto transitorio de nos selecionados (ids de Mesh::nodes). Nao e gravado no projeto.
class NodeSelection
{
public:
    enum class Mode { Replace, Add, Toggle };

    void apply(const std::vector<int> &ids, Mode mode);
    void clear() { m_nodes.clear(); }
    [[nodiscard]] bool contains(int id) const { return m_nodes.count(id) != 0; }
    [[nodiscard]] bool empty() const noexcept { return m_nodes.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return m_nodes.size(); }
    [[nodiscard]] std::vector<int> ids() const { return {m_nodes.begin(), m_nodes.end()}; }
    void set(const std::vector<int> &ids) { m_nodes = {ids.begin(), ids.end()}; }

private:
    std::set<int> m_nodes;
};

/// Referencia PERSISTENTE a um no: coordenadas globais (m) + objeto de origem. Sobrevive a remalhar e a mudancas de numeracao,
/// ao contrario do indice do no, que muda sempre que a malha e regenerada.
struct NodeRef {
    int objectId {-1};                        ///< dica (um no pode ser compartilhado entre objetos); nao participa da busca
    std::array<double, 3> position {{0.0, 0.0, 0.0}};   ///< m
};

/// Resultado da resolucao de referencias contra uma malha.
struct RefResolution {
    std::vector<int> tableIndex;    ///< por referencia: indice na tabela (-1 = nao resolvida)
    std::vector<int> nodeIds;       ///< ids dos nos resolvidos, sem repeticao
    int unresolved {0};             ///< referencias sem no dentro da tolerancia (a malha nao tem mais esse no)
    int snapped {0};                ///< resolvidas por proximidade (distancia > 1e-9 m), nao por coincidencia exata
    int merged {0};                 ///< referencias que caem no mesmo no que outra (malha mais grossa)
};

std::vector<NodeRef> makeNodeRefs(const NodeTable &table, const std::vector<int> &nodeIds);

/// Resolve as referencias: o no mais proximo a menos de `tolerance` metros (padrao: tolerancia de fusao de nos da malha).
/// Referencias sem no proximo ficam nao resolvidas (nunca se escolhe um no distante em silencio).
RefResolution resolveNodeRefs(const NodeTable &table, const std::vector<NodeRef> &refs, double tolerance = 1e-5);

} // namespace Structura::Shell
