#pragma once

#include "PostProcessorDataTypes.h"
#include "DiagramScaleManager.h"
#include "DiagramGeometryCache.h"

#include <array>
#include <vector>

namespace Structura::Visualization {

/// Posição e valor de um extremo local do diagrama.
struct ExtremaPoint {
    double                value{0.0};
    std::array<double, 3> worldPosition{}; ///< Ponta do diagrama (onde o label é colocado)
    int                   elementId{0};
};

/// Resultado da busca: listas de máximos e mínimos ordenados por |value|.
struct ExtremaResult {
    std::vector<ExtremaPoint> maxima; ///< Ordenados por valor decrescente
    std::vector<ExtremaPoint> minima; ///< Ordenados por valor crescente (mais negativo primeiro)
};

/**
 * @brief Localiza extremos globais dos diagramas de esforços.
 *
 * Por padrão retorna apenas o MAX global e o MIN global (maxCount=1),
 * evitando poluição visual. Suporte interno a TopN disponível para
 * uso futuro sem alteração de interface.
 */
class DiagramExtremaFinder {
public:
    /**
     * @brief Encontra os extremos do diagrama em todos os elementos.
     * @param cache         Cache com dados amostrados.
     * @param results       Resultados para iterar elementos.
     * @param scale         Escala atual (para calcular posição da ponta).
     * @param type          Tipo de diagrama.
     * @param revision      Revisão de análise atual.
     * @param maxCount      Número máximo de extremos a retornar (padrão=1).
     */
    ExtremaResult find(const DiagramGeometryCache& cache,
                       const std::vector<BeamElementResults>& results,
                       const DiagramScaleManager& scale,
                       DiagramType type,
                       int revision,
                       int maxCount = 1) const;
};

} // namespace Structura::Visualization
