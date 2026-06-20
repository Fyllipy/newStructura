#include "DiagramScaleManager.h"
#include "VisualizationTheme.h"

#include <algorithm>
#include <cmath>

namespace Structura::Visualization {

// ── Configuração ────────────────────────────────────────────────────────────

void DiagramScaleManager::setMode(DiagramScaleMode mode)
{
    m_mode = mode;
}

void DiagramScaleManager::setUserScale(double scale)
{
    m_userScale = std::max(0.05, scale);
}

void DiagramScaleManager::setActiveElementIds(const std::vector<int>& ids)
{
    m_activeIds = ids;
}

void DiagramScaleManager::setReferenceLength(double averageBarLength)
{
    m_refLength = std::max(1e-6, averageBarLength);
}

// ── Verificação de elemento ativo ────────────────────────────────────────────

bool DiagramScaleManager::isElementActive(int elementId) const
{
    if (m_activeIds.empty()) {
        return true; // sem filtro = todos ativos
    }
    return std::find(m_activeIds.begin(), m_activeIds.end(), elementId)
           != m_activeIds.end();
}

// ── Cálculo de maxAbs ────────────────────────────────────────────────────────

void DiagramScaleManager::compute(DiagramGeometryCache& cache,
                                   const std::vector<BeamElementResults>& results,
                                   DiagramType type,
                                   int revision)
{
    double maxAbs = 1e-12; // evita divisão por zero

    for (const auto& element : results) {
        if (m_mode != DiagramScaleMode::Global && !isElementActive(element.externalId)) {
            continue;
        }

        const DiagramCacheKey key{element.externalId, type, revision};
        if (!cache.has(key)) {
            continue;
        }

        const auto& sampled = cache.get(key);
        for (double v : sampled.rawValues) {
            maxAbs = std::max(maxAbs, std::abs(v));
        }
    }

    m_maxAbs = maxAbs;
}

// ── Aplicação da escala ──────────────────────────────────────────────────────

double DiagramScaleManager::physicalOffset(double rawValue) const noexcept
{
    if (m_maxAbs < 1e-30) {
        return 0.0;
    }
    // offset = (value / maxAbs) * userScale * refLength * amplitudeFraction
    return (rawValue / m_maxAbs)
         * m_userScale
         * m_refLength
         * VisualizationTheme::defaultAmplitudeFraction;
}

} // namespace Structura::Visualization
