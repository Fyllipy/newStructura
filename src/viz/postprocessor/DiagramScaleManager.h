#pragma once

#include "PostProcessorDataTypes.h"
#include "DiagramGeometryCache.h"

#include <vector>

namespace Structura::Visualization {

/**
 * @brief Gerencia a escala visual dos diagramas de esforços.
 *
 * A geometria é construída com offsets normalizados:
 *     scaledOffset = (value / maxAbs) * physicalAmplitude
 *
 * onde physicalAmplitude = userScale * referenceLength * defaultAmplitudeFraction.
 *
 * Quando userScale muda (slider), physicalAmplitude é atualizado sem
 * re-avaliar a física — apenas os valores brutos do cache são re-escalados.
 *
 * Modos de normalização:
 *   Global          → maxAbs de todos os elementos
 *   CurrentLoadCase → maxAbs dos elementos passados em activeIds
 *   SelectedElements→ maxAbs dos elementos em m_activeIds
 */
class DiagramScaleManager {
public:
    DiagramScaleManager() = default;

    // ── Configuração ────────────────────────────────────────────────────────

    void setMode(DiagramScaleMode mode);
    void setUserScale(double scale);

    /// IDs dos elementos ativos para os modos CurrentLoadCase e SelectedElements.
    void setActiveElementIds(const std::vector<int>& ids);

    // ── Cálculo de maxAbs ───────────────────────────────────────────────────

    /**
     * @brief Recalcula maxAbs percorrendo o cache de diagramas amostrados.
     * @param cache    Cache com os valores brutos amostrados.
     * @param results  Resultados para filtrar por ID ativo quando necessário.
     * @param type     Tipo de diagrama para filtrar entradas do cache.
     * @param revision Revisão de análise atual.
     */
    void compute(DiagramGeometryCache& cache,
                 const std::vector<BeamElementResults>& results,
                 DiagramType type,
                 int revision);

    // ── Aplicação da escala ─────────────────────────────────────────────────

    /**
     * @brief Converte valor bruto em deslocamento físico no mundo.
     * @param rawValue Valor em unidades de esforço.
     * @return Amplitude em metros (unidade do modelo).
     */
    double physicalOffset(double rawValue) const noexcept;

    /**
     * @brief Comprimento de referência física para calcular a amplitude.
     * @param averageBarLength Comprimento médio das barras do modelo.
     */
    void setReferenceLength(double averageBarLength);

    // ── Leitura ─────────────────────────────────────────────────────────────
    DiagramScaleMode mode()       const noexcept { return m_mode; }
    double           maxAbs()     const noexcept { return m_maxAbs; }
    double           userScale()  const noexcept { return m_userScale; }
    double           refLength()  const noexcept { return m_refLength; }

private:
    bool isElementActive(int elementId) const;

    DiagramScaleMode  m_mode      { DiagramScaleMode::Global };
    double            m_maxAbs    { 1.0 };
    double            m_userScale { 1.0 };
    double            m_refLength { 1.0 }; ///< Comprimento de referência em metros
    std::vector<int>  m_activeIds;
};

} // namespace Structura::Visualization
