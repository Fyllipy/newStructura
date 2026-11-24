#pragma once

#include "shell/ShellResults.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Structura::Shell {

/// Malha pronta para desenho: triangulos coloridos por vertice (duplicados por elemento) e segmentos das arestas dos elementos.
struct RenderMesh {
    std::vector<std::array<float, 3>> points;
    std::vector<std::array<float, 3>> normals;
    std::vector<std::array<unsigned char, 3>> colors;
    std::vector<std::array<int, 3>> triangles;
    std::vector<std::array<float, 3>> edgePoints;   ///< pares de pontos (segmentos)
    double minValue {0.0};
    double maxValue {0.0};   ///< na unidade de exibicao
    bool hasValues {false};

    // ---- Resultados para a legenda e o probe da vista principal (um unico fluxo: o mesmo RenderMesh que colore a malha) ----
    /// Valor EXIBIDO em cada vertice (mesma indexacao de `points`: 4 por quadrilatero, na ordem dos nos), na unidade de exibicao.
    /// E o valor que gerou a cor: com `smoothed` e a media nos nos da mesma zona (recurso GRAFICO, nao recuperacao fisica de tensoes).
    /// NaN = valor invalido (sem resultado); nunca e infinito. Vazio quando `hasValues` e falso.
    std::vector<double> scalars;
    /// Valor do PROPRIO elemento em cada vertice (sem media entre elementos), o devolvido pelo solver x fator de exibicao.
    /// Preserva a descontinuidade fisica entre elementos. Igual a `scalars` quando nao ha alisamento.
    std::vector<double> solverScalars;
    std::string title;   ///< nome da grandeza
    std::string unit;    ///< unidade de exibicao ("" = adimensional)
    bool smoothed {false};     ///< `scalars` foi alisado (media por no e zona)
    bool fixedRange {false};   ///< faixa de cores fixada pelo usuario
    double unitsPerMeter {1.0};
    std::vector<int> quadObject;                 ///< por quadrilatero: indice do objeto (-1 se desconhecido)
    std::vector<int> objectIds;                  ///< por indice de objeto: id
    std::vector<std::string> objectNames;        ///< por indice de objeto: nome
    std::vector<int> quadElement;                ///< por quadrilatero: numero do elemento (indice em Mesh::quads)
    std::vector<int> vertexNode;                 ///< por vertice (mesma indexacao de `points`): indice do no em Mesh::nodes
    std::vector<std::array<float, 3>> restPoints;   ///< posicoes sem deformada (vazio se a vista nao esta deformada)
    /// Resumo (hash) da geometria: posicoes, triangulos e arestas. Igual entre duas malhas = nada a reconstruir, so recolorir.
    std::uint64_t geometryRevision {0};

    [[nodiscard]] std::size_t quadCount() const noexcept { return triangles.size() / 2; }
    /// true se o numero de valores coincide com o de vertices (ou se nao ha valores) e os demais vetores sao compativeis.
    [[nodiscard]] bool consistent() const noexcept
    {
        const bool valuesOk = hasValues ? (scalars.size() == points.size() && solverScalars.size() == points.size()) : (scalars.empty() && solverScalars.empty());
        return valuesOk && colors.size() == points.size() && normals.size() == points.size();
    }
};

struct RenderOptions {
    Quantity quantity {Quantity::StressVonMisesMax};
    double deformScale {0.0};      ///< 0 = sem deformada
    bool smooth {true};            ///< media nos nos dentro da mesma zona
    bool edges {true};
    double unitsPerMeter {1.0};    ///< unidades do cenario por metro
    bool fixedRange {false};
    double rangeMin {0.0}, rangeMax {0.0};   ///< na unidade de exibicao
    std::array<unsigned char, 3> plainColor {{150, 160, 175}};   ///< cor sem resultado
    int highlightObject {-1};                                     ///< indice do objeto realcado (so sem resultados)
    std::array<unsigned char, 3> highlightColor {{255, 168, 40}};
    const std::vector<double> *elementScalars {nullptr};          ///< valores por elemento (utilizacao, armadura): substituem a grandeza dos resultados
    std::string title;             ///< titulo e unidade da legenda quando `overrideLabels`
    std::string unit;
    bool overrideLabels {false};   ///< true: usa `title`/`unit` mesmo vazios (valores por elemento adimensionais)
};

RenderMesh buildRenderMesh(const Mesh &mesh, const Model &model, const CaseResult *result, const RenderOptions &options);

/// Cor da escala "arco-iris" para t em [0, 1].
std::array<unsigned char, 3> rainbow(double t);

/// Escala em FAIXAS (estilo Mecway): a faixa de valores e dividida em `kColorBands` degraus de cor constante e a transicao entre eles e
/// brusca, seguindo as curvas de nivel dentro de cada elemento. A vista 3D e a legenda usam a mesma funcao.
inline constexpr int kColorBands = 10;
/// Indice da faixa (0 = minimo ... kColorBands - 1 = maximo) de t em [0, 1]; t = 1 cai na ultima faixa.
int colorBand(double t);
/// Cor da faixa de t: degraus igualmente espacados sobre a escala arco-iris, do azul (faixa 0) ao vermelho (ultima faixa).
std::array<unsigned char, 3> bandColor(double t);

/// Cor dos vertices sem valor valido e posicao t de um valor na faixa [lo, hi] (0,5 se a faixa e constante ou so ruido numerico).
inline constexpr std::array<unsigned char, 3> kInvalidValueColor {{110, 110, 110}};
double colorPosition(double value, double lo, double hi);

} // namespace Structura::Shell
