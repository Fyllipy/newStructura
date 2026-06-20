#include "DiagramExtremaFinder.h"

#include <algorithm>
#include <limits>

namespace Structura::Visualization {

ExtremaResult DiagramExtremaFinder::find(
    const DiagramGeometryCache& cache,
    const std::vector<BeamElementResults>& results,
    const DiagramScaleManager& scale,
    DiagramType type,
    int revision,
    int maxCount) const
{
    // Coletamos todos os candidatos
    std::vector<ExtremaPoint> allPoints;
    allPoints.reserve(results.size() * 4);

    for (const auto& el : results) {
        const DiagramCacheKey key{el.externalId, type, revision};
        if (!cache.has(key)) {
            continue;
        }

        const auto& sampled = cache.get(key);
        const std::size_t n = sampled.rawValues.size();

        for (std::size_t i = 0; i < n; ++i) {
            const double v = sampled.rawValues[i];

            // Posição da ponta do diagrama (onde o label é ancorado)
            const double offset = scale.physicalOffset(v);
            std::array<double, 3> tip{
                sampled.basePoints[i][0] + sampled.perpAxis[0] * offset,
                sampled.basePoints[i][1] + sampled.perpAxis[1] * offset,
                sampled.basePoints[i][2] + sampled.perpAxis[2] * offset
            };

            allPoints.push_back({v, tip, el.externalId});
        }
    }

    if (allPoints.empty()) {
        return {};
    }

    // Ordenar por valor
    auto byValueDesc = [](const ExtremaPoint& a, const ExtremaPoint& b) {
        return a.value > b.value;
    };
    auto byValueAsc  = [](const ExtremaPoint& a, const ExtremaPoint& b) {
        return a.value < b.value;
    };

    ExtremaResult result;

    // Top maxCount máximos
    {
        auto sorted = allPoints;
        std::sort(sorted.begin(), sorted.end(), byValueDesc);
        const int count = std::min(maxCount, static_cast<int>(sorted.size()));
        result.maxima.assign(sorted.begin(), sorted.begin() + count);
    }

    // Top maxCount mínimos
    {
        auto sorted = allPoints;
        std::sort(sorted.begin(), sorted.end(), byValueAsc);
        const int count = std::min(maxCount, static_cast<int>(sorted.size()));
        result.minima.assign(sorted.begin(), sorted.begin() + count);
    }

    return result;
}

} // namespace Structura::Visualization
