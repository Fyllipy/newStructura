#pragma once

#include "IModelRepository.h"
#include <QHash>
#include <algorithm>

namespace Structura::App {

/**
 * @brief In-memory implementation of IModelRepository.
 * 
 * This implementation stores all entities in memory using QHash for fast lookup
 * and std::vector for ordered iteration. This is suitable for desktop applications
 * with models that fit in memory.
 * 
 * Thread safety: This implementation is NOT thread-safe. External synchronization
 * is required if accessed from multiple threads.
 */
class InMemoryModelRepository : public IModelRepository
{
public:
    InMemoryModelRepository() = default;
    ~InMemoryModelRepository() override = default;

    // Prevent copying
    InMemoryModelRepository(const InMemoryModelRepository&) = delete;
    InMemoryModelRepository& operator=(const InMemoryModelRepository&) = delete;

    // Allow moving
    InMemoryModelRepository(InMemoryModelRepository&&) = default;
    InMemoryModelRepository& operator=(InMemoryModelRepository&&) = default;

    // ===== Node Operations =====
    
    bool addNode(const Node &node) override
    {
        if (m_nodeById.contains(node.id())) {
            return false;
        }
        m_nodes.push_back(node);
        m_nodeById[node.id()] = static_cast<int>(m_nodes.size() - 1);
        return true;
    }
    
    bool removeNode(const QUuid &id) override
    {
        if (!m_nodeById.contains(id)) {
            return false;
        }
        
        int index = m_nodeById[id];
        m_nodes.erase(m_nodes.begin() + index);
        
        // Rebuild index map
        rebuildNodeIndex();
        return true;
    }
    
    bool updateNode(const Node &node) override
    {
        if (!m_nodeById.contains(node.id())) {
            return false;
        }
        
        int index = m_nodeById[node.id()];
        m_nodes[index] = node;
        return true;
    }
    
    std::optional<Node> findNode(const QUuid &id) const override
    {
        if (!m_nodeById.contains(id)) {
            return std::nullopt;
        }
        
        int index = m_nodeById[id];
        return m_nodes[index];
    }
    
    std::optional<Node> findNodeByExternalId(int externalId) const override
    {
        auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
            [externalId](const Node &n) { return n.externalId() == externalId; });
        
        if (it != m_nodes.end()) {
            return *it;
        }
        return std::nullopt;
    }
    
    std::vector<Node> allNodes() const override
    {
        return m_nodes;
    }
    
    size_t nodeCount() const override
    {
        return m_nodes.size();
    }
    
    void clearNodes() override
    {
        m_nodes.clear();
        m_nodeById.clear();
    }

    // ===== Bar Operations =====
    
    bool addBar(const Bar &bar) override
    {
        if (m_barById.contains(bar.id())) {
            return false;
        }
        m_bars.push_back(bar);
        m_barById[bar.id()] = static_cast<int>(m_bars.size() - 1);
        return true;
    }
    
    bool removeBar(const QUuid &id) override
    {
        if (!m_barById.contains(id)) {
            return false;
        }
        
        int index = m_barById[id];
        m_bars.erase(m_bars.begin() + index);
        
        // Rebuild index map
        rebuildBarIndex();
        return true;
    }
    
    bool updateBar(const Bar &bar) override
    {
        if (!m_barById.contains(bar.id())) {
            return false;
        }
        
        int index = m_barById[bar.id()];
        m_bars[index] = bar;
        return true;
    }
    
    std::optional<Bar> findBar(const QUuid &id) const override
    {
        if (!m_barById.contains(id)) {
            return std::nullopt;
        }
        
        int index = m_barById[id];
        return m_bars[index];
    }
    
    std::optional<Bar> findBarByExternalId(int externalId) const override
    {
        auto it = std::find_if(m_bars.begin(), m_bars.end(),
            [externalId](const Bar &b) { return b.externalId() == externalId; });
        
        if (it != m_bars.end()) {
            return *it;
        }
        return std::nullopt;
    }
    
    std::vector<Bar> allBars() const override
    {
        return m_bars;
    }
    
    size_t barCount() const override
    {
        return m_bars.size();
    }
    
    void clearBars() override
    {
        m_bars.clear();
        m_barById.clear();
    }
    
    std::vector<Bar> findBarsConnectedToNode(const QUuid &nodeId) const override
    {
        std::vector<Bar> result;
        std::copy_if(m_bars.begin(), m_bars.end(), std::back_inserter(result),
            [&nodeId](const Bar &bar) {
                return bar.startNodeId() == nodeId || bar.endNodeId() == nodeId;
            });
        return result;
    }

    // ===== Material Operations =====
    
    bool addMaterial(const Material &material) override
    {
        if (m_materialById.contains(material.id())) {
            return false;
        }
        m_materials.push_back(material);
        m_materialById[material.id()] = static_cast<int>(m_materials.size() - 1);
        return true;
    }
    
    bool removeMaterial(const QUuid &id) override
    {
        if (!m_materialById.contains(id)) {
            return false;
        }
        
        int index = m_materialById[id];
        m_materials.erase(m_materials.begin() + index);
        
        // Rebuild index map
        rebuildMaterialIndex();
        return true;
    }
    
    bool updateMaterial(const Material &material) override
    {
        if (!m_materialById.contains(material.id())) {
            return false;
        }
        
        int index = m_materialById[material.id()];
        m_materials[index] = material;
        return true;
    }
    
    std::optional<Material> findMaterial(const QUuid &id) const override
    {
        if (!m_materialById.contains(id)) {
            return std::nullopt;
        }
        
        int index = m_materialById[id];
        return m_materials[index];
    }
    
    std::vector<Material> allMaterials() const override
    {
        return m_materials;
    }
    
    size_t materialCount() const override
    {
        return m_materials.size();
    }
    
    void clearMaterials() override
    {
        m_materials.clear();
        m_materialById.clear();
    }

    // ===== Section Operations =====
    
    bool addSection(const Section &section) override
    {
        if (m_sectionById.contains(section.id())) {
            return false;
        }
        m_sections.push_back(section);
        m_sectionById[section.id()] = static_cast<int>(m_sections.size() - 1);
        return true;
    }
    
    bool removeSection(const QUuid &id) override
    {
        if (!m_sectionById.contains(id)) {
            return false;
        }
        
        int index = m_sectionById[id];
        m_sections.erase(m_sections.begin() + index);
        
        // Rebuild index map
        rebuildSectionIndex();
        return true;
    }
    
    bool updateSection(const Section &section) override
    {
        if (!m_sectionById.contains(section.id())) {
            return false;
        }
        
        int index = m_sectionById[section.id()];
        m_sections[index] = section;
        return true;
    }
    
    std::optional<Section> findSection(const QUuid &id) const override
    {
        if (!m_sectionById.contains(id)) {
            return std::nullopt;
        }
        
        int index = m_sectionById[id];
        return m_sections[index];
    }
    
    std::vector<Section> allSections() const override
    {
        return m_sections;
    }
    
    size_t sectionCount() const override
    {
        return m_sections.size();
    }
    
    void clearSections() override
    {
        m_sections.clear();
        m_sectionById.clear();
    }

    // ===== GridLine Operations =====
    
    bool addGridLine(const GridLine &gridLine) override
    {
        if (m_gridLineById.contains(gridLine.id())) {
            return false;
        }
        m_gridLines.push_back(gridLine);
        m_gridLineById[gridLine.id()] = static_cast<int>(m_gridLines.size() - 1);
        return true;
    }
    
    bool removeGridLine(const QUuid &id) override
    {
        if (!m_gridLineById.contains(id)) {
            return false;
        }
        
        int index = m_gridLineById[id];
        m_gridLines.erase(m_gridLines.begin() + index);
        
        // Rebuild index map
        rebuildGridLineIndex();
        return true;
    }
    
    bool updateGridLine(const GridLine &gridLine) override
    {
        if (!m_gridLineById.contains(gridLine.id())) {
            return false;
        }
        
        int index = m_gridLineById[gridLine.id()];
        m_gridLines[index] = gridLine;
        return true;
    }
    
    std::optional<GridLine> findGridLine(const QUuid &id) const override
    {
        if (!m_gridLineById.contains(id)) {
            return std::nullopt;
        }
        
        int index = m_gridLineById[id];
        return m_gridLines[index];
    }
    
    std::vector<GridLine> allGridLines() const override
    {
        return m_gridLines;
    }
    
    size_t gridLineCount() const override
    {
        return m_gridLines.size();
    }
    
    void clearGridLines() override
    {
        m_gridLines.clear();
        m_gridLineById.clear();
    }

    // ===== Bulk Operations =====
    
    void clearAll() override
    {
        clearNodes();
        clearBars();
        clearMaterials();
        clearSections();
        clearGridLines();
    }
    
    bool isEmpty() const override
    {
        return m_nodes.empty() && m_bars.empty() && m_materials.empty() &&
               m_sections.empty() && m_gridLines.empty();
    }

private:
    // Helper methods to rebuild index maps after removal
    void rebuildNodeIndex()
    {
        m_nodeById.clear();
        for (size_t i = 0; i < m_nodes.size(); ++i) {
            m_nodeById[m_nodes[i].id()] = static_cast<int>(i);
        }
    }
    
    void rebuildBarIndex()
    {
        m_barById.clear();
        for (size_t i = 0; i < m_bars.size(); ++i) {
            m_barById[m_bars[i].id()] = static_cast<int>(i);
        }
    }
    
    void rebuildMaterialIndex()
    {
        m_materialById.clear();
        for (size_t i = 0; i < m_materials.size(); ++i) {
            m_materialById[m_materials[i].id()] = static_cast<int>(i);
        }
    }
    
    void rebuildSectionIndex()
    {
        m_sectionById.clear();
        for (size_t i = 0; i < m_sections.size(); ++i) {
            m_sectionById[m_sections[i].id()] = static_cast<int>(i);
        }
    }
    
    void rebuildGridLineIndex()
    {
        m_gridLineById.clear();
        for (size_t i = 0; i < m_gridLines.size(); ++i) {
            m_gridLineById[m_gridLines[i].id()] = static_cast<int>(i);
        }
    }

    // Storage
    std::vector<Node> m_nodes;
    std::vector<Bar> m_bars;
    std::vector<Material> m_materials;
    std::vector<Section> m_sections;
    std::vector<GridLine> m_gridLines;
    
    // Index maps for fast lookup by ID
    QHash<QUuid, int> m_nodeById;
    QHash<QUuid, int> m_barById;
    QHash<QUuid, int> m_materialById;
    QHash<QUuid, int> m_sectionById;
    QHash<QUuid, int> m_gridLineById;
};

} // namespace Structura::App
