#pragma once

#include "../core/model/ModelEntities.h"
#include <QUuid>
#include <QString>
#include <vector>
#include <optional>
#include <functional>

namespace Structura::App {

using Node = Structura::Model::Node;
using Bar = Structura::Model::Bar;
using Material = Structura::Model::Material;
using Section = Structura::Model::Section;
using GridLine = Structura::Model::GridLine;

/**
 * @brief Interface for managing structural model entities.
 * 
 * This interface provides CRUD operations for all model entities:
 * Nodes, Bars, Materials, Sections, and GridLines.
 * 
 * The repository pattern abstracts data storage and allows for
 * easy testing and future implementation changes (e.g., database backend).
 */
class IModelRepository
{
public:
    virtual ~IModelRepository() = default;

    // ===== Node Operations =====
    
    /**
     * @brief Add a new node to the repository.
     * @param node The node to add
     * @return true if successful, false if node with same ID already exists
     */
    virtual bool addNode(const Node &node) = 0;
    
    /**
     * @brief Remove a node by ID.
     * @param id The UUID of the node to remove
     * @return true if node was found and removed
     */
    virtual bool removeNode(const QUuid &id) = 0;
    
    /**
     * @brief Update an existing node.
     * @param node The node with updated data (ID must match existing node)
     * @return true if node was found and updated
     */
    virtual bool updateNode(const Node &node) = 0;
    
    /**
     * @brief Find a node by its UUID.
     * @param id The UUID to search for
     * @return Optional containing the node if found
     */
    virtual std::optional<Node> findNode(const QUuid &id) const = 0;
    
    /**
     * @brief Find a node by its external ID.
     * @param externalId The external ID to search for
     * @return Optional containing the node if found
     */
    virtual std::optional<Node> findNodeByExternalId(int externalId) const = 0;
    
    /**
     * @brief Get all nodes in the repository.
     * @return Vector of all nodes
     */
    virtual std::vector<Node> allNodes() const = 0;
    
    /**
     * @brief Get the count of nodes.
     * @return Number of nodes
     */
    virtual size_t nodeCount() const = 0;
    
    /**
     * @brief Clear all nodes from the repository.
     */
    virtual void clearNodes() = 0;

    // ===== Bar Operations =====
    
    /**
     * @brief Add a new bar to the repository.
     * @param bar The bar to add
     * @return true if successful, false if bar with same ID already exists
     */
    virtual bool addBar(const Bar &bar) = 0;
    
    /**
     * @brief Remove a bar by ID.
     * @param id The UUID of the bar to remove
     * @return true if bar was found and removed
     */
    virtual bool removeBar(const QUuid &id) = 0;
    
    /**
     * @brief Update an existing bar.
     * @param bar The bar with updated data (ID must match existing bar)
     * @return true if bar was found and updated
     */
    virtual bool updateBar(const Bar &bar) = 0;
    
    /**
     * @brief Find a bar by its UUID.
     * @param id The UUID to search for
     * @return Optional containing the bar if found
     */
    virtual std::optional<Bar> findBar(const QUuid &id) const = 0;
    
    /**
     * @brief Find a bar by its external ID.
     * @param externalId The external ID to search for
     * @return Optional containing the bar if found
     */
    virtual std::optional<Bar> findBarByExternalId(int externalId) const = 0;
    
    /**
     * @brief Get all bars in the repository.
     * @return Vector of all bars
     */
    virtual std::vector<Bar> allBars() const = 0;
    
    /**
     * @brief Get the count of bars.
     * @return Number of bars
     */
    virtual size_t barCount() const = 0;
    
    /**
     * @brief Clear all bars from the repository.
     */
    virtual void clearBars() = 0;
    
    /**
     * @brief Find all bars connected to a specific node.
     * @param nodeId The UUID of the node
     * @return Vector of bars connected to the node
     */
    virtual std::vector<Bar> findBarsConnectedToNode(const QUuid &nodeId) const = 0;

    // ===== Material Operations =====
    
    /**
     * @brief Add a new material to the repository.
     * @param material The material to add
     * @return true if successful, false if material with same ID already exists
     */
    virtual bool addMaterial(const Material &material) = 0;
    
    /**
     * @brief Remove a material by ID.
     * @param id The UUID of the material to remove
     * @return true if material was found and removed
     */
    virtual bool removeMaterial(const QUuid &id) = 0;
    
    /**
     * @brief Update an existing material.
     * @param material The material with updated data
     * @return true if material was found and updated
     */
    virtual bool updateMaterial(const Material &material) = 0;
    
    /**
     * @brief Find a material by its UUID.
     * @param id The UUID to search for
     * @return Optional containing the material if found
     */
    virtual std::optional<Material> findMaterial(const QUuid &id) const = 0;
    
    /**
     * @brief Get all materials in the repository.
     * @return Vector of all materials
     */
    virtual std::vector<Material> allMaterials() const = 0;
    
    /**
     * @brief Get the count of materials.
     * @return Number of materials
     */
    virtual size_t materialCount() const = 0;
    
    /**
     * @brief Clear all materials from the repository.
     */
    virtual void clearMaterials() = 0;

    // ===== Section Operations =====
    
    /**
     * @brief Add a new section to the repository.
     * @param section The section to add
     * @return true if successful, false if section with same ID already exists
     */
    virtual bool addSection(const Section &section) = 0;
    
    /**
     * @brief Remove a section by ID.
     * @param id The UUID of the section to remove
     * @return true if section was found and removed
     */
    virtual bool removeSection(const QUuid &id) = 0;
    
    /**
     * @brief Update an existing section.
     * @param section The section with updated data
     * @return true if section was found and updated
     */
    virtual bool updateSection(const Section &section) = 0;
    
    /**
     * @brief Find a section by its UUID.
     * @param id The UUID to search for
     * @return Optional containing the section if found
     */
    virtual std::optional<Section> findSection(const QUuid &id) const = 0;
    
    /**
     * @brief Get all sections in the repository.
     * @return Vector of all sections
     */
    virtual std::vector<Section> allSections() const = 0;
    
    /**
     * @brief Get the count of sections.
     * @return Number of sections
     */
    virtual size_t sectionCount() const = 0;
    
    /**
     * @brief Clear all sections from the repository.
     */
    virtual void clearSections() = 0;

    // ===== GridLine Operations =====
    
    /**
     * @brief Add a new grid line to the repository.
     * @param gridLine The grid line to add
     * @return true if successful, false if grid line with same ID already exists
     */
    virtual bool addGridLine(const GridLine &gridLine) = 0;
    
    /**
     * @brief Remove a grid line by ID.
     * @param id The UUID of the grid line to remove
     * @return true if grid line was found and removed
     */
    virtual bool removeGridLine(const QUuid &id) = 0;
    
    /**
     * @brief Update an existing grid line.
     * @param gridLine The grid line with updated data
     * @return true if grid line was found and updated
     */
    virtual bool updateGridLine(const GridLine &gridLine) = 0;
    
    /**
     * @brief Find a grid line by its UUID.
     * @param id The UUID to search for
     * @return Optional containing the grid line if found
     */
    virtual std::optional<GridLine> findGridLine(const QUuid &id) const = 0;
    
    /**
     * @brief Get all grid lines in the repository.
     * @return Vector of all grid lines
     */
    virtual std::vector<GridLine> allGridLines() const = 0;
    
    /**
     * @brief Get the count of grid lines.
     * @return Number of grid lines
     */
    virtual size_t gridLineCount() const = 0;
    
    /**
     * @brief Clear all grid lines from the repository.
     */
    virtual void clearGridLines() = 0;

    // ===== Bulk Operations =====
    
    /**
     * @brief Clear all entities from the repository.
     */
    virtual void clearAll() = 0;
    
    /**
     * @brief Check if the repository is empty (no entities).
     * @return true if no entities exist
     */
    virtual bool isEmpty() const = 0;
};

} // namespace Structura::App
