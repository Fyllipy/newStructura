#include "DiagramGeometryCache.h"

#include <stdexcept>

namespace Structura::Visualization {

void DiagramGeometryCache::invalidateAll()
{
    m_cache.clear();
}

bool DiagramGeometryCache::has(const DiagramCacheKey& key) const
{
    return m_cache.find(key) != m_cache.end();
}

const SampledDiagram& DiagramGeometryCache::get(const DiagramCacheKey& key) const
{
    auto it = m_cache.find(key);
    if (it == m_cache.end()) {
        throw std::out_of_range("DiagramGeometryCache::get — chave nao encontrada");
    }
    return it->second;
}

void DiagramGeometryCache::store(const DiagramCacheKey& key, SampledDiagram data)
{
    m_cache[key] = std::move(data);
}

} // namespace Structura::Visualization
