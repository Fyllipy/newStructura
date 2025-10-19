#pragma once

#include <QVector3D>
#include <QVector>
#include <vtkSmartPointer.h>
#include <array>

class vtkRenderer;
class vtkPoints;
class vtkPolyData;
class vtkCellArray;
class vtkPolyDataMapper;
class vtkActor;
class vtkArrowSource;
class vtkGlyph3D;
class vtkDoubleArray;
class vtkBillboardTextActor3D;

namespace Structura::Visualization {

/**
 * @brief Manages the visual representation of structural loads (forces, distributed loads, moments)
 * 
 * This class follows clean code principles and OOP design to handle all aspects
 * of load visualization in a structural analysis application, including:
 * - Nodal forces as 2D arrows
 * - Distributed loads as multiple arrows along bars
 * - Moments as semicircular arcs with arrow heads
 * 
 * Visual consistency and proper scaling are maintained across all load types.
 */
class LoadVisualization
{
public:
    /// Constants for visual configuration
    static constexpr double DEFAULT_ARROW_TIP_LENGTH = 0.35;
    static constexpr double DEFAULT_ARROW_TIP_RADIUS = 0.08;
    static constexpr double DEFAULT_ARROW_SHAFT_RADIUS = 0.03;
    static constexpr double DEFAULT_ARROW_SCALE_FACTOR = 0.18;
    static constexpr double DISTRIBUTED_ARROW_SCALE_FACTOR = 0.14;
    static constexpr double DISTRIBUTED_LOAD_SPACING_RATIO = 0.06; // 6% of bar length
    static constexpr int MINIMUM_ARROWS_PER_BAR = 3;
    static constexpr int MAXIMUM_ARROWS_PER_BAR = 20;
    static constexpr double MOMENT_ARC_SEGMENTS = 16; // Half-circle segments
    static constexpr double MOMENT_BASE_RADIUS = 0.18;
    static constexpr double MOMENT_CONE_HEIGHT = 0.08;
    static constexpr double MOMENT_CONE_RADIUS = 0.04;
    static constexpr double LABEL_OFFSET_DISTANCE = 0.15;  // Reduced to bring labels closer

    /**
     * @brief Represents a nodal load (force and/or moment at a point)
     */
    struct NodalLoad {
        QVector3D position;     ///< Node position in world coordinates
        QVector3D force;        ///< Force vector [Fx, Fy, Fz]
        QVector3D moment;       ///< Moment vector [Mx, My, Mz]
    };

    /**
     * @brief Represents a distributed load along a bar element
     */
    struct DistributedLoad {
        QVector3D startPoint;   ///< Start point of the bar
        QVector3D endPoint;     ///< End point of the bar
        QVector3D loadVector;   ///< Load vector per unit length
        bool isLocalSystem;     ///< Whether load is in local coordinate system
    };

    explicit LoadVisualization();
    ~LoadVisualization();

    // Initialization
    void initialize(vtkRenderer* renderer);
    
    // Main update methods
    void setNodalLoads(const QVector<NodalLoad>& loads);
    void setDistributedLoads(const QVector<DistributedLoad>& loads);
    void clearAll();
    
    // Visibility control
    void setVisible(bool visible);
    void setNodalLoadsVisible(bool visible);
    void setDistributedLoadsVisible(bool visible);
    void setMomentsVisible(bool visible);

    // Utility methods
    static double computeScaledMagnitude(double magnitude);
    static QVector3D computePerpendicularVector(const QVector3D& vector);

private:
    // Core building methods
    void rebuildNodalForces();
    void rebuildDistributedLoads();
    void rebuildMoments();
    
    // Arrow creation helpers
    void createForceArrow(const QVector3D& position, const QVector3D& force);
    void createDistributedArrowsAlongBar(const DistributedLoad& load);
    int calculateNumberOfArrows(double barLength) const;
    
    // Moment visualization helpers
    void createMomentArc(const QVector3D& position, const QVector3D& moment);
    std::array<QVector3D, 2> computeArcBasis(const QVector3D& axis) const;
    void appendArcWithArrowHead(const QVector3D& center, 
                                const QVector3D& tangent, 
                                const QVector3D& bitangent,
                                double radius,
                                bool clockwise);
    
    // Label creation
    void createForceLabel(const QVector3D& position, 
                         const QVector3D& direction, 
                         double magnitude,
                         const char* unit);
    void createMomentLabel(const QVector3D& position, 
                          const QVector3D& direction, 
                          double magnitude,
                          double radius);

    // Cleanup helpers
    void removeAllLabels();
    void removeNodalLabels();
    void removeDistributedLabels();
    void removeMomentLabels();

    // VTK objects for nodal forces
    vtkSmartPointer<vtkPoints> m_nodalForcePoints;
    vtkSmartPointer<vtkDoubleArray> m_nodalForceVectors;
    vtkSmartPointer<vtkDoubleArray> m_nodalForceMagnitudes;
    vtkSmartPointer<vtkPolyData> m_nodalForcePolyData;
    vtkSmartPointer<vtkArrowSource> m_arrowSource;
    vtkSmartPointer<vtkGlyph3D> m_nodalForceGlyph;
    vtkSmartPointer<vtkPolyDataMapper> m_nodalForceMapper;
    vtkSmartPointer<vtkActor> m_nodalForceActor;

    // VTK objects for distributed loads
    vtkSmartPointer<vtkPoints> m_distributedLoadPoints;
    vtkSmartPointer<vtkDoubleArray> m_distributedLoadVectors;
    vtkSmartPointer<vtkDoubleArray> m_distributedLoadMagnitudes;
    vtkSmartPointer<vtkPolyData> m_distributedLoadPolyData;
    vtkSmartPointer<vtkGlyph3D> m_distributedLoadGlyph;
    vtkSmartPointer<vtkPolyDataMapper> m_distributedLoadMapper;
    vtkSmartPointer<vtkActor> m_distributedLoadActor;

    // VTK objects for moments
    vtkSmartPointer<vtkPoints> m_momentPoints;
    vtkSmartPointer<vtkCellArray> m_momentLines;
    vtkSmartPointer<vtkPolyData> m_momentPolyData;
    vtkSmartPointer<vtkPolyDataMapper> m_momentMapper;
    vtkSmartPointer<vtkActor> m_momentActor;

    // Label storage
    QVector<vtkSmartPointer<vtkBillboardTextActor3D>> m_nodalForceLabels;
    QVector<vtkSmartPointer<vtkBillboardTextActor3D>> m_distributedLoadLabels;
    QVector<vtkSmartPointer<vtkBillboardTextActor3D>> m_momentLabels;

    // Data storage
    QVector<NodalLoad> m_nodalLoads;
    QVector<DistributedLoad> m_distributedLoads;

    // Renderer reference (not owned)
    vtkRenderer* m_renderer;
};

} // namespace Structura::Visualization
