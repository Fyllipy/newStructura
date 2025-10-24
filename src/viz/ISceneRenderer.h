#pragma once

#include <QUuid>
#include <QSet>
#include <QVector>
#include <array>
#include <vector>
#include <optional>

// Forward declarations to avoid VTK dependency in header
class QVTKOpenGLNativeWidget;

namespace Structura::Viz {

/**
 * @brief Snapshot of model data for rendering.
 * 
 * This is an immutable view of the model state that can be passed
 * to the renderer without exposing mutable model access.
 */
struct ModelSnapshot {
    struct NodeData {
        QUuid id;
        int externalId;
        double x, y, z;
        bool isSelected;
        bool isHighlighted;
        std::array<bool, 6> restraints;
    };
    
    struct BarData {
        QUuid id;
        int externalId;
        QUuid startNodeId;
        QUuid endNodeId;
        bool isSelected;
        std::optional<std::array<double, 3>> kPoint;
    };
    
    struct GridLineData {
        QUuid id;
        int axis; // 0=X, 1=Y, 2=Z
        double offset;
        std::array<double, 3> startPoint;
        std::array<double, 3> endPoint;
        bool isHighlighted;
        bool isGhost;
    };
    
    struct NodalLoadData {
        std::array<double, 3> position;
        std::array<double, 3> force;
        std::array<double, 3> moment;
    };
    
    struct MemberLoadData {
        std::array<double, 3> position;
        std::array<double, 3> force;
        std::array<double, 3> barVector;
        bool localSystem;
    };
    
    struct SupportData {
        std::array<double, 3> position;
        std::array<bool, 6> restraints;
    };
    
    struct BarLCSData {
        QUuid barId;
        std::array<double, 3> origin;
        std::array<double, 3> xAxis;
        std::array<double, 3> yAxis;
        std::array<double, 3> zAxis;
    };
    
    std::vector<NodeData> nodes;
    std::vector<BarData> bars;
    std::vector<GridLineData> gridLines;
    std::vector<NodalLoadData> nodalLoads;
    std::vector<MemberLoadData> memberLoads;
    std::vector<SupportData> supports;
    std::vector<BarLCSData> barLCS;
    
    bool showBarLCS = false;
};

/**
 * @brief Pure interface for scene rendering operations.
 * 
 * This interface abstracts all VTK rendering logic, allowing for:
 * - Testing with mock implementations
 * - Future renderer replacements
 * - Clear separation between domain and visualization
 * 
 * The renderer is responsible ONLY for drawing; it does not maintain
 * or modify the model state.
 */
class ISceneRenderer
{
public:
    virtual ~ISceneRenderer() = default;
    
    /**
     * @brief Initialize the renderer with a VTK widget.
     * @param widget The Qt widget for VTK rendering
     */
    virtual void initialize(QVTKOpenGLNativeWidget *widget) = 0;
    
    /**
     * @brief Render a complete model snapshot.
     * @param snapshot Immutable model data to render
     * 
     * This replaces all rendered entities with the snapshot data.
     */
    virtual void renderSnapshot(const ModelSnapshot &snapshot) = 0;
    
    /**
     * @brief Update only node visuals (color, position).
     * @param nodes Nodes to update
     */
    virtual void updateNodes(const std::vector<ModelSnapshot::NodeData> &nodes) = 0;
    
    /**
     * @brief Update only bar visuals (color).
     * @param bars Bars to update
     */
    virtual void updateBars(const std::vector<ModelSnapshot::BarData> &bars) = 0;
    
    /**
     * @brief Update grid lines visualization.
     * @param gridLines Grid lines to render
     */
    virtual void updateGridLines(const std::vector<ModelSnapshot::GridLineData> &gridLines) = 0;
    
    /**
     * @brief Highlight a specific node.
     * @param nodeId UUID of node to highlight (null to clear)
     */
    virtual void highlightNode(const QUuid &nodeId) = 0;
    
    /**
     * @brief Set selected nodes.
     * @param nodeIds Set of selected node UUIDs
     */
    virtual void setSelectedNodes(const QSet<QUuid> &nodeIds) = 0;
    
    /**
     * @brief Set selected bars.
     * @param barIds Set of selected bar UUIDs
     */
    virtual void setSelectedBars(const QSet<QUuid> &barIds) = 0;
    
    /**
     * @brief Highlight a grid line.
     * @param lineId UUID of grid line to highlight (null to clear)
     */
    virtual void highlightGridLine(const QUuid &lineId) = 0;
    
    /**
     * @brief Show a ghost grid line during interactive placement.
     * @param axis Axis (0=X, 1=Y, 2=Z)
     * @param startPoint Line start position
     * @param endPoint Line end position
     */
    virtual void showGridGhostLine(int axis,
                                   const std::array<double, 3> &startPoint,
                                   const std::array<double, 3> &endPoint) = 0;
    
    /**
     * @brief Hide the ghost grid line.
     */
    virtual void hideGridGhostLine() = 0;
    
    /**
     * @brief Update load visualizations.
     * @param nodalLoads Nodal load data
     * @param memberLoads Member load data
     */
    virtual void updateLoads(const std::vector<ModelSnapshot::NodalLoadData> &nodalLoads,
                            const std::vector<ModelSnapshot::MemberLoadData> &memberLoads) = 0;
    
    /**
     * @brief Update support (restraint) visualizations.
     * @param supports Support data
     */
    virtual void updateSupports(const std::vector<ModelSnapshot::SupportData> &supports) = 0;
    
    /**
     * @brief Update bar local coordinate system visualizations.
     * @param barLCS Bar LCS data
     * @param visible Whether to show bar LCS
     */
    virtual void updateBarLCS(const std::vector<ModelSnapshot::BarLCSData> &barLCS, bool visible) = 0;
    
    /**
     * @brief Clear all rendered entities.
     */
    virtual void clearAll() = 0;
    
    /**
     * @brief Reset camera to default view.
     */
    virtual void resetCamera() = 0;
    
    /**
     * @brief Zoom camera to fit all entities.
     */
    virtual void zoomExtents() = 0;
    
    /**
     * @brief Refresh/redraw the scene.
     */
    virtual void refresh() = 0;
    
    // Picking operations
    
    /**
     * @brief Pick a node at screen coordinates.
     * @param displayX Screen X coordinate
     * @param displayY Screen Y coordinate
     * @return UUID of picked node, or null UUID if none
     */
    virtual QUuid pickNode(int displayX, int displayY) const = 0;
    
    /**
     * @brief Pick a bar at screen coordinates.
     * @param displayX Screen X coordinate
     * @param displayY Screen Y coordinate
     * @return UUID of picked bar, or null UUID if none
     */
    virtual QUuid pickBar(int displayX, int displayY) const = 0;
    
    /**
     * @brief Pick a grid line at screen coordinates.
     * @param displayX Screen X coordinate
     * @param displayY Screen Y coordinate
     * @return UUID of picked grid line, or null UUID if none
     */
    virtual QUuid pickGridLine(int displayX, int displayY) const = 0;
    
    /**
     * @brief Convert screen coordinates to world point on Z=0 plane.
     * @param displayX Screen X coordinate
     * @param displayY Screen Y coordinate
     * @param[out] x World X coordinate
     * @param[out] y World Y coordinate
     * @param[out] z World Z coordinate
     * @return true if successful
     */
    virtual bool pickWorldPoint(int displayX, int displayY,
                                double &x, double &y, double &z) const = 0;
    
    /**
     * @brief Get viewport height in pixels.
     * @return Height in pixels
     */
    virtual int viewportHeight() const = 0;
};

} // namespace Structura::Viz
