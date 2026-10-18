#pragma once

#include "shell/ShellResults.h"

#include <array>
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
};

RenderMesh buildRenderMesh(const Mesh &mesh, const Model &model, const CaseResult *result, const RenderOptions &options);

/// Cor da escala "arco-iris" para t em [0, 1].
std::array<unsigned char, 3> rainbow(double t);

} // namespace Structura::Shell
