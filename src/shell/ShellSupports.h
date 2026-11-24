#pragma once

#include "shell/ShellMesh.h"
#include "shell/ShellModel.h"

#include <array>
#include <string>
#include <vector>

namespace Structura::Shell {

/// Nos da malha atingidos por cada apoio do modelo (a mesma resolucao usada pelo solver e pela vista: um unico lugar).
struct ResolvedSupport {
    int support {-1};          ///< indice em Model::supports
    std::vector<int> nodes;    ///< nos de Mesh::nodes
};

struct SupportResolution {
    std::vector<ResolvedSupport> supports;
    /// Avisos de diagnostico: apoio que nao achou nos, e pontos do seletor `Nodes` que nao existem mais na malha.
    std::vector<std::string> warnings;
};

/// Resolve todos os apoios sobre a malha. `Plane`, `FreeEdge` e `Point` mantem exatamente o comportamento de antes
/// (a regra de `Point` continua "no mais proximo"); `Nodes` procura cada ponto guardado dentro de `tol` e relata os que sumiram,
/// sem nunca escolher um no distante.
SupportResolution resolveSupports(const Model &model, const Mesh &mesh);

/// Tipo de apoio para exibicao.
enum class SupportKind {
    Fixed,            ///< engaste: 3 translacoes e 3 rotacoes
    Pinned,           ///< articulado: 3 translacoes
    Roller,           ///< movel: 1 ou 2 translacoes
    Partial,          ///< demais combinacoes (rotacao sem translacao, translacoes com rotacao parcial...)
    Spring,           ///< apoio elastico
    CompressionOnly   ///< apoio elastico que so comprime (fundacao)
};

/// Tipo de um apoio isolado. Devolve false se o apoio nao restringe nada (nao ha o que desenhar).
bool classifySupport(const Support &support, SupportKind &kind);

const char *supportKindName(SupportKind kind);

/// Um no apoiado, para desenhar. Varios apoios no mesmo no se fundem (uniao das restricoes; mola prevalece).
struct SupportMark {
    int node {-1};
    std::array<double, 3> position {{0.0, 0.0, 0.0}};   ///< m
    SupportKind kind {SupportKind::Fixed};
    std::array<int, 6> fixed {{0, 0, 0, 0, 0, 0}};      ///< uniao das restricoes (0/1)
    std::array<double, 3> normal {{0.0, 0.0, 1.0}};     ///< so apoios elasticos so-compressao
};

std::vector<SupportMark> supportMarks(const Model &model, const Mesh &mesh, const SupportResolution &resolution);

} // namespace Structura::Shell
