#pragma once

#include "PostProcessorDataTypes.h"
#include "InternalForceEvaluator.h"

#include <array>
#include <unordered_map>
#include <vector>

namespace Structura::Visualization {

// ── Dados de diagrama amostrado ────────────────────────────────────────────

/**
 * @brief Diagrama amostrado de um elemento, armazenado sem escala aplicada.
 *
 * Separa a fase de avaliação física (cara) da geração de geometria VTK (rápida).
 * Armazenar valores brutos permite reconstruir a geometria a qualquer escala
 * sem re-avaliar os esforços internos.
 */
struct SampledDiagram {
    /// Posições no mundo ao longo do eixo da barra (um por amostra).
    std::vector<std::array<double, 3>> basePoints;

    /// Valores de esforço brutos (sem normalização) em cada amostra.
    std::vector<double> rawValues;

    /// Eixo local de deslocamento perpendicular para este tipo de diagrama.
    /// Para Mz, Vy, N → yLocal; para My, Vz → zLocal.
    /// Para Mt → usado como eixo base da faixa helicoidal.
    std::array<double, 3> perpAxis{};

    /// Eixo secundário (para torção: zLocal).
    std::array<double, 3> secAxis{};

    double elementLength{0.0};
};

// ── Chave de cache ─────────────────────────────────────────────────────────

struct DiagramCacheKey {
    int         elementId{0};
    DiagramType type{DiagramType::None};
    int         analysisRevision{0};

    bool operator==(const DiagramCacheKey& o) const noexcept
    {
        return elementId        == o.elementId
            && type             == o.type
            && analysisRevision == o.analysisRevision;
    }
};

struct DiagramCacheKeyHash {
    std::size_t operator()(const DiagramCacheKey& k) const noexcept
    {
        std::size_t h = std::hash<int>{}(k.elementId);
        h ^= std::hash<int>{}(static_cast<int>(k.type))
           + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(k.analysisRevision)
           + 0x9e3779b9u + (h << 6) + (h >> 2);
        return h;
    }
};

// ── Cache ──────────────────────────────────────────────────────────────────

/**
 * @brief Cache de diagramas amostrados, indexado por (elementId, tipo, revisão).
 *
 * Design:
 * - scaleFactor NÃO faz parte da chave. Mover o slider de escala não gera
 *   novas entradas — a geometria é reconstruída a partir dos valores brutos.
 * - analysisRevision garante invalidação automática quando uma nova análise
 *   for executada sobre o mesmo modelo.
 */
class DiagramGeometryCache {
public:
    /// Invalida todo o cache (chamar após nova análise).
    void invalidateAll();

    /// Invalida entradas de um elemento específico (ex.: barra modificada).
    void invalidateElement(int elementId);

    bool has(const DiagramCacheKey& key) const;
    const SampledDiagram& get(const DiagramCacheKey& key) const;
    void store(const DiagramCacheKey& key, SampledDiagram data);

    std::size_t size() const { return m_cache.size(); }

private:
    std::unordered_map<DiagramCacheKey, SampledDiagram, DiagramCacheKeyHash> m_cache;
};

} // namespace Structura::Visualization
