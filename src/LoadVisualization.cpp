#include "LoadVisualization.h"

#include <vtkRenderer.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkCellArray.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkArrowSource.h>
#include <vtkGlyph3D.h>
#include <vtkDoubleArray.h>
#include <vtkBillboardTextActor3D.h>
#include <vtkTextProperty.h>
#include <vtkProperty.h>
#include <vtkConeSource.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkAppendPolyData.h>
#include <vtkDataObject.h>
#include <vtkPointData.h>

#include <cmath>
#include <algorithm>

namespace Structura::Visualization {

namespace {
    // Color definitions (RGB 0-1 range) - matching existing SceneController colors
    constexpr double FORCE_COLOR[3] = {0.90, 0.15, 0.20};           // Red (same as nodal loads)
    constexpr double DISTRIBUTED_LOAD_COLOR[3] = {0.15, 0.58, 0.32}; // Green (same as member loads)
    constexpr double MOMENT_COLOR[3] = {0.65, 0.20, 0.82};          // Purple (same as moments)

    constexpr int LABEL_FONT_SIZE = 14;
    constexpr double EPSILON = 1e-6;
    constexpr double LABEL_OFFSET_SCALE = 0.15;  // Reduced from 0.35 to bring labels closer
}

LoadVisualization::LoadVisualization()
    : m_nodalForcePoints(vtkSmartPointer<vtkPoints>::New())
    , m_nodalForceVectors(vtkSmartPointer<vtkDoubleArray>::New())
    , m_nodalForceMagnitudes(vtkSmartPointer<vtkDoubleArray>::New())
    , m_nodalForcePolyData(vtkSmartPointer<vtkPolyData>::New())
    , m_arrowSource(vtkSmartPointer<vtkArrowSource>::New())
    , m_nodalForceGlyph(vtkSmartPointer<vtkGlyph3D>::New())
    , m_nodalForceMapper(vtkSmartPointer<vtkPolyDataMapper>::New())
    , m_nodalForceActor(vtkSmartPointer<vtkActor>::New())
    , m_distributedLoadPoints(vtkSmartPointer<vtkPoints>::New())
    , m_distributedLoadVectors(vtkSmartPointer<vtkDoubleArray>::New())
    , m_distributedLoadMagnitudes(vtkSmartPointer<vtkDoubleArray>::New())
    , m_distributedLoadPolyData(vtkSmartPointer<vtkPolyData>::New())
    , m_distributedLoadGlyph(vtkSmartPointer<vtkGlyph3D>::New())
    , m_distributedLoadMapper(vtkSmartPointer<vtkPolyDataMapper>::New())
    , m_distributedLoadActor(vtkSmartPointer<vtkActor>::New())
    , m_momentPoints(vtkSmartPointer<vtkPoints>::New())
    , m_momentLines(vtkSmartPointer<vtkCellArray>::New())
    , m_momentPolyData(vtkSmartPointer<vtkPolyData>::New())
    , m_momentMapper(vtkSmartPointer<vtkPolyDataMapper>::New())
    , m_momentActor(vtkSmartPointer<vtkActor>::New())
    , m_renderer(nullptr)
{
    // Configure arrow source (shared for all force arrows)
    m_arrowSource->SetTipLength(DEFAULT_ARROW_TIP_LENGTH);
    m_arrowSource->SetTipRadius(DEFAULT_ARROW_TIP_RADIUS);
    m_arrowSource->SetShaftRadius(DEFAULT_ARROW_SHAFT_RADIUS);

    // Setup nodal forces
    m_nodalForcePolyData->SetPoints(m_nodalForcePoints);
    m_nodalForceVectors->SetNumberOfComponents(3);
    m_nodalForceVectors->SetName("ForceDirection");
    m_nodalForcePolyData->GetPointData()->SetVectors(m_nodalForceVectors);
    m_nodalForceMagnitudes->SetNumberOfComponents(1);
    m_nodalForceMagnitudes->SetName("ForceMagnitude");
    m_nodalForcePolyData->GetPointData()->SetScalars(m_nodalForceMagnitudes);

    m_nodalForceGlyph->SetSourceConnection(m_arrowSource->GetOutputPort());
    m_nodalForceGlyph->SetInputData(m_nodalForcePolyData);
    m_nodalForceGlyph->OrientOn();
    m_nodalForceGlyph->SetVectorModeToUseVector();
    m_nodalForceGlyph->SetScaleModeToScaleByScalar();
    m_nodalForceGlyph->SetScaleFactor(DEFAULT_ARROW_SCALE_FACTOR);
    m_nodalForceGlyph->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, "ForceMagnitude");

    m_nodalForceMapper->SetInputConnection(m_nodalForceGlyph->GetOutputPort());
    m_nodalForceMapper->ScalarVisibilityOff();  // Don't use scalar colors, use actor color
    m_nodalForceActor->SetMapper(m_nodalForceMapper);
    m_nodalForceActor->GetProperty()->SetColor(FORCE_COLOR[0], FORCE_COLOR[1], FORCE_COLOR[2]);
    m_nodalForceActor->GetProperty()->SetOpacity(0.95);
    m_nodalForceActor->GetProperty()->LightingOff();  // Flat 2D appearance
    m_nodalForceActor->PickableOff();
    m_nodalForceActor->SetVisibility(false);

    // Setup distributed loads
    m_distributedLoadPolyData->SetPoints(m_distributedLoadPoints);
    m_distributedLoadVectors->SetNumberOfComponents(3);
    m_distributedLoadVectors->SetName("DistributedDirection");
    m_distributedLoadPolyData->GetPointData()->SetVectors(m_distributedLoadVectors);
    m_distributedLoadMagnitudes->SetNumberOfComponents(1);
    m_distributedLoadMagnitudes->SetName("DistributedMagnitude");
    m_distributedLoadPolyData->GetPointData()->SetScalars(m_distributedLoadMagnitudes);

    m_distributedLoadGlyph->SetSourceConnection(m_arrowSource->GetOutputPort());
    m_distributedLoadGlyph->SetInputData(m_distributedLoadPolyData);
    m_distributedLoadGlyph->OrientOn();
    m_distributedLoadGlyph->SetVectorModeToUseVector();
    m_distributedLoadGlyph->SetScaleModeToScaleByScalar();
    m_distributedLoadGlyph->SetScaleFactor(DISTRIBUTED_ARROW_SCALE_FACTOR);
    m_distributedLoadGlyph->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, "DistributedMagnitude");

    m_distributedLoadMapper->SetInputConnection(m_distributedLoadGlyph->GetOutputPort());
    m_distributedLoadMapper->ScalarVisibilityOff();  // Don't use scalar colors, use actor color
    m_distributedLoadActor->SetMapper(m_distributedLoadMapper);
    m_distributedLoadActor->GetProperty()->SetColor(DISTRIBUTED_LOAD_COLOR[0], 
                                                     DISTRIBUTED_LOAD_COLOR[1], 
                                                     DISTRIBUTED_LOAD_COLOR[2]);
    m_distributedLoadActor->GetProperty()->SetOpacity(0.90);
    m_distributedLoadActor->GetProperty()->LightingOff();  // Flat 2D appearance
    m_distributedLoadActor->PickableOff();
    m_distributedLoadActor->SetVisibility(false);

    // Setup moments
    m_momentPolyData->SetPoints(m_momentPoints);
    m_momentPolyData->SetLines(m_momentLines);
    m_momentMapper->SetInputData(m_momentPolyData);
    m_momentActor->SetMapper(m_momentMapper);
    m_momentActor->GetProperty()->SetColor(MOMENT_COLOR[0], MOMENT_COLOR[1], MOMENT_COLOR[2]);
    m_momentActor->GetProperty()->SetLineWidth(2.5);
    m_momentActor->GetProperty()->LightingOff();  // Flat 2D appearance
    m_momentActor->PickableOff();
    m_momentActor->SetVisibility(false);
}

LoadVisualization::~LoadVisualization()
{
    removeAllLabels();
}

void LoadVisualization::initialize(vtkRenderer* renderer)
{
    if (!renderer) {
        return;
    }

    m_renderer = renderer;

    // Add actors to renderer
    m_renderer->AddActor(m_nodalForceActor);
    m_renderer->AddActor(m_distributedLoadActor);
    m_renderer->AddActor(m_momentActor);
}

void LoadVisualization::setNodalLoads(const QVector<NodalLoad>& loads)
{
    m_nodalLoads = loads;
    rebuildNodalForces();
    rebuildMoments();
}

void LoadVisualization::setDistributedLoads(const QVector<DistributedLoad>& loads)
{
    m_distributedLoads = loads;
    rebuildDistributedLoads();
}

void LoadVisualization::clearAll()
{
    m_nodalLoads.clear();
    m_distributedLoads.clear();
    rebuildNodalForces();
    rebuildDistributedLoads();
    rebuildMoments();
}

void LoadVisualization::setVisible(bool visible)
{
    setNodalLoadsVisible(visible);
    setDistributedLoadsVisible(visible);
    setMomentsVisible(visible);
}

void LoadVisualization::setNodalLoadsVisible(bool visible)
{
    if (m_nodalForceActor) {
        m_nodalForceActor->SetVisibility(visible && m_nodalForcePoints->GetNumberOfPoints() > 0);
    }
    for (auto& label : m_nodalForceLabels) {
        if (label) {
            label->SetVisibility(visible);
        }
    }
}

void LoadVisualization::setDistributedLoadsVisible(bool visible)
{
    if (m_distributedLoadActor) {
        m_distributedLoadActor->SetVisibility(visible && m_distributedLoadPoints->GetNumberOfPoints() > 0);
    }
    for (auto& label : m_distributedLoadLabels) {
        if (label) {
            label->SetVisibility(visible);
        }
    }
}

void LoadVisualization::setMomentsVisible(bool visible)
{
    if (m_momentActor) {
        m_momentActor->SetVisibility(visible && m_momentLines->GetNumberOfCells() > 0);
    }
    for (auto& label : m_momentLabels) {
        if (label) {
            label->SetVisibility(visible);
        }
    }
}

double LoadVisualization::computeScaledMagnitude(double magnitude)
{
    if (magnitude <= EPSILON) {
        return 0.0;
    }
    // Logarithmic scaling for better visual representation across different orders of magnitude
    return std::max(0.12, std::log10(1.0 + magnitude) * 0.6);
}

QVector3D LoadVisualization::computePerpendicularVector(const QVector3D& vector)
{
    // Find a vector perpendicular to the input
    QVector3D reference(0.0f, 0.0f, 1.0f);
    if (std::abs(QVector3D::dotProduct(vector, reference)) > 0.95f) {
        reference = QVector3D(0.0f, 1.0f, 0.0f);
    }
    QVector3D perpendicular = QVector3D::crossProduct(vector, reference);
    if (perpendicular.lengthSquared() < EPSILON) {
        reference = QVector3D(1.0f, 0.0f, 0.0f);
        perpendicular = QVector3D::crossProduct(vector, reference);
    }
    return perpendicular.normalized();
}

void LoadVisualization::rebuildNodalForces()
{
    // Clear existing data
    removeNodalLabels();
    m_nodalForcePoints->Reset();
    m_nodalForceVectors->Reset();
    m_nodalForceMagnitudes->Reset();

    // Build new force arrows
    for (const auto& load : m_nodalLoads) {
        const double magnitude = load.force.length();
        if (magnitude > EPSILON) {
            createForceArrow(load.position, load.force);
        }
    }

    // Update VTK pipeline
    m_nodalForcePoints->Modified();
    m_nodalForceVectors->Modified();
    m_nodalForceMagnitudes->Modified();
    m_nodalForcePolyData->Modified();
    m_nodalForceGlyph->Modified();

    // Update visibility
    const bool hasForces = m_nodalForcePoints->GetNumberOfPoints() > 0;
    m_nodalForceActor->SetVisibility(hasForces);
}

void LoadVisualization::rebuildDistributedLoads()
{
    // Clear existing data
    removeDistributedLabels();
    m_distributedLoadPoints->Reset();
    m_distributedLoadVectors->Reset();
    m_distributedLoadMagnitudes->Reset();

    // Build new distributed load arrows
    for (const auto& load : m_distributedLoads) {
        createDistributedArrowsAlongBar(load);
    }

    // Update VTK pipeline
    m_distributedLoadPoints->Modified();
    m_distributedLoadVectors->Modified();
    m_distributedLoadMagnitudes->Modified();
    m_distributedLoadPolyData->Modified();
    m_distributedLoadGlyph->Modified();

    // Update visibility
    const bool hasLoads = m_distributedLoadPoints->GetNumberOfPoints() > 0;
    m_distributedLoadActor->SetVisibility(hasLoads);
}

void LoadVisualization::rebuildMoments()
{
    // Clear existing data
    removeMomentLabels();
    m_momentPoints->Reset();
    m_momentLines->Reset();

    // Build new moment arcs
    for (const auto& load : m_nodalLoads) {
        const double magnitude = load.moment.length();
        if (magnitude > EPSILON) {
            createMomentArc(load.position, load.moment);
        }
    }

    // Update VTK pipeline
    m_momentPoints->Modified();
    m_momentLines->Modified();
    m_momentPolyData->Modified();

    // Update visibility
    const bool hasMoments = m_momentLines->GetNumberOfCells() > 0;
    m_momentActor->SetVisibility(hasMoments);
}

void LoadVisualization::createForceArrow(const QVector3D& position, const QVector3D& force)
{
    const double magnitude = force.length();
    if (magnitude <= EPSILON) {
        return;
    }

    // Add point and direction for glyph
    m_nodalForcePoints->InsertNextPoint(position.x(), position.y(), position.z());
    
    const QVector3D direction = force.normalized();
    const double vec[3] = {direction.x(), direction.y(), direction.z()};
    m_nodalForceVectors->InsertNextTuple(vec);
    
    const double scaledMag = computeScaledMagnitude(magnitude);
    m_nodalForceMagnitudes->InsertNextValue(scaledMag);

    // Create label
    createForceLabel(position, direction, magnitude, "kN");
}

void LoadVisualization::createDistributedArrowsAlongBar(const DistributedLoad& load)
{
    const QVector3D barVector = load.endPoint - load.startPoint;
    const double barLength = barVector.length();
    
    if (barLength <= EPSILON) {
        return;
    }

    const double magnitude = load.loadVector.length();
    if (magnitude <= EPSILON) {
        return;
    }

    // Calculate number of arrows based on bar length and spacing ratio
    const int numArrows = calculateNumberOfArrows(barLength);
    
    // Direction of the load (not the bar!)
    const QVector3D loadDirection = load.loadVector.normalized();
    const double vec[3] = {loadDirection.x(), loadDirection.y(), loadDirection.z()};
    const double scaledMag = computeScaledMagnitude(magnitude);

    // Sample points along the bar at regular intervals
    // Using spacing of 6% of bar length, avoiding endpoints
    for (int i = 0; i < numArrows; ++i) {
        // Distribute arrows evenly: t = (i+1) / (numArrows+1) to avoid endpoints
        const double t = static_cast<double>(i + 1) / static_cast<double>(numArrows + 1);
        const QVector3D position = load.startPoint + barVector * static_cast<float>(t);
        
        m_distributedLoadPoints->InsertNextPoint(position.x(), position.y(), position.z());
        m_distributedLoadVectors->InsertNextTuple(vec);
        m_distributedLoadMagnitudes->InsertNextValue(scaledMag);
    }

    // Create single label at midpoint of bar
    const QVector3D midPoint = (load.startPoint + load.endPoint) * 0.5f;
    createForceLabel(midPoint, loadDirection, magnitude, "kN/m");
}

int LoadVisualization::calculateNumberOfArrows(double barLength) const
{
    // Calculate based on 6% spacing
    const double spacing = barLength * DISTRIBUTED_LOAD_SPACING_RATIO;
    int numArrows = static_cast<int>(barLength / spacing);
    
    // Clamp to reasonable bounds
    numArrows = std::max(MINIMUM_ARROWS_PER_BAR, std::min(MAXIMUM_ARROWS_PER_BAR, numArrows));
    
    return numArrows;
}

void LoadVisualization::createMomentArc(const QVector3D& position, const QVector3D& moment)
{
    const double magnitude = moment.length();
    if (magnitude <= EPSILON) {
        return;
    }

    QVector3D axis = moment.normalized();
    
    // Determine the arc radius based on magnitude
    const double radius = std::max(MOMENT_BASE_RADIUS, 
                                  0.35 + 0.08 * std::log10(1.0 + magnitude));

    // Compute basis vectors for the arc plane (perpendicular to moment axis)
    auto basis = computeArcBasis(axis);
    const QVector3D tangent = basis[0];
    const QVector3D bitangent = basis[1];

    // Determine clockwise direction (right-hand rule: positive moment is counter-clockwise
    // when looking along the positive axis direction)
    const bool clockwise = (moment.lengthSquared() > 0);
    
    appendArcWithArrowHead(position, tangent, bitangent, radius, clockwise);
    
    // Create label
    createMomentLabel(position, axis, magnitude, static_cast<float>(radius));
}

std::array<QVector3D, 2> LoadVisualization::computeArcBasis(const QVector3D& axis) const
{
    QVector3D reference(0.0f, 0.0f, 1.0f);
    if (std::abs(QVector3D::dotProduct(axis, reference)) > 0.95f) {
        reference = QVector3D(0.0f, 1.0f, 0.0f);
    }
    
    QVector3D tangent = QVector3D::crossProduct(axis, reference);
    if (tangent.lengthSquared() < EPSILON) {
        reference = QVector3D(1.0f, 0.0f, 0.0f);
        tangent = QVector3D::crossProduct(axis, reference);
    }
    tangent.normalize();
    
    QVector3D bitangent = QVector3D::crossProduct(axis, tangent);
    bitangent.normalize();
    
    return {tangent, bitangent};
}

void LoadVisualization::appendArcWithArrowHead(const QVector3D& center,
                                               const QVector3D& tangent,
                                               const QVector3D& bitangent,
                                               double radius,
                                               bool clockwise)
{
    const int segments = static_cast<int>(MOMENT_ARC_SEGMENTS);
    const double pi = 3.14159265358979323846;
    
    // Create semicircle (180 degrees)
    std::vector<vtkIdType> arcIds;
    arcIds.reserve(segments + 1);
    
    for (int i = 0; i <= segments; ++i) {
        // Angle from 0 to π (180°)
        double angle = pi * static_cast<double>(i) / static_cast<double>(segments);
        if (clockwise) {
            angle = -angle;
        }
        
        const double c = std::cos(angle);
        const double s = std::sin(angle);
        const QVector3D offset = static_cast<float>(radius) * (tangent * static_cast<float>(c) + bitangent * static_cast<float>(s));
        const QVector3D point = center + offset;
        
        arcIds.push_back(m_momentPoints->InsertNextPoint(point.x(), point.y(), point.z()));
    }
    
    // Insert the arc line
    m_momentLines->InsertNextCell(static_cast<vtkIdType>(arcIds.size()), arcIds.data());
    
    // Add arrow head at the end of the arc
    // Calculate the tangent direction at the end point for the arrow
    const double endAngle = clockwise ? -pi : pi;
    const QVector3D arrowTangent = (clockwise ? 1.0f : -1.0f) * 
        (tangent * static_cast<float>(-std::sin(endAngle)) + 
         bitangent * static_cast<float>(std::cos(endAngle)));
    
    const QVector3D endPoint = center + static_cast<float>(radius) * 
        (tangent * static_cast<float>(std::cos(endAngle)) + 
         bitangent * static_cast<float>(std::sin(endAngle)));
    
    // Create small cone for arrow head
    const double coneHeight = MOMENT_CONE_HEIGHT;
    const QVector3D coneBase = endPoint;
    const QVector3D coneTip = endPoint + arrowTangent.normalized() * static_cast<float>(coneHeight);
    
    // Add cone as a small line segment for simplicity (could be enhanced with actual cone)
    const vtkIdType baseId = m_momentPoints->InsertNextPoint(coneBase.x(), coneBase.y(), coneBase.z());
    const vtkIdType tipId = m_momentPoints->InsertNextPoint(coneTip.x(), coneTip.y(), coneTip.z());
    m_momentLines->InsertNextCell(2);
    m_momentLines->InsertCellPoint(baseId);
    m_momentLines->InsertCellPoint(tipId);
}

void LoadVisualization::createForceLabel(const QVector3D& position,
                                        const QVector3D& direction,
                                        double magnitude,
                                        const char* unit)
{
    if (!m_renderer) {
        return;
    }

    // Offset label perpendicular to force direction
    QVector3D labelDir = direction;
    if (labelDir.lengthSquared() < EPSILON) {
        labelDir = QVector3D(0.0f, 0.0f, 1.0f);
    }
    labelDir.normalize();
    
    const QVector3D labelPos = position + labelDir * static_cast<float>(LABEL_OFFSET_DISTANCE);

    auto label = vtkSmartPointer<vtkBillboardTextActor3D>::New();
    
    // Format with 3 significant figures
    QString text;
    if (magnitude >= 100.0) {
        text = QString("%1 %2").arg(magnitude, 0, 'f', 1).arg(unit);
    } else if (magnitude >= 10.0) {
        text = QString("%1 %2").arg(magnitude, 0, 'f', 2).arg(unit);
    } else {
        text = QString("%1 %2").arg(magnitude, 0, 'f', 3).arg(unit);
    }
    
    label->SetInput(text.toUtf8().constData());
    label->SetPosition(labelPos.x(), labelPos.y(), labelPos.z());
    label->GetTextProperty()->SetFontSize(LABEL_FONT_SIZE);
    
    // Color based on unit type
    if (QString(unit).contains("kN/m")) {
        label->GetTextProperty()->SetColor(DISTRIBUTED_LOAD_COLOR[0], 
                                          DISTRIBUTED_LOAD_COLOR[1], 
                                          DISTRIBUTED_LOAD_COLOR[2]);
        m_distributedLoadLabels.append(label);
    } else {
        label->GetTextProperty()->SetColor(FORCE_COLOR[0], FORCE_COLOR[1], FORCE_COLOR[2]);
        m_nodalForceLabels.append(label);
    }
    
    label->GetTextProperty()->SetBold(true);
    label->GetTextProperty()->ShadowOff();
    
    m_renderer->AddActor(label);
}

void LoadVisualization::createMomentLabel(const QVector3D& position,
                                         const QVector3D& direction,
                                         double magnitude,
                                         double radius)
{
    if (!m_renderer) {
        return;
    }

    QVector3D labelDir = direction;
    if (labelDir.lengthSquared() < EPSILON) {
        labelDir = QVector3D(0.0f, 0.0f, 1.0f);
    }
    labelDir.normalize();
    
    const QVector3D labelPos = position + labelDir * (static_cast<float>(radius) + 0.15f);

    auto label = vtkSmartPointer<vtkBillboardTextActor3D>::New();
    
    // Format with 3 significant figures
    QString text;
    if (magnitude >= 100.0) {
        text = QString("%1 kN·m").arg(magnitude, 0, 'f', 1);
    } else if (magnitude >= 10.0) {
        text = QString("%1 kN·m").arg(magnitude, 0, 'f', 2);
    } else {
        text = QString("%1 kN·m").arg(magnitude, 0, 'f', 3);
    }
    
    label->SetInput(text.toUtf8().constData());
    label->SetPosition(labelPos.x(), labelPos.y(), labelPos.z());
    label->GetTextProperty()->SetFontSize(LABEL_FONT_SIZE);
    label->GetTextProperty()->SetColor(MOMENT_COLOR[0], MOMENT_COLOR[1], MOMENT_COLOR[2]);
    label->GetTextProperty()->SetBold(true);
    label->GetTextProperty()->ShadowOff();
    
    m_renderer->AddActor(label);
    m_momentLabels.append(label);
}

void LoadVisualization::removeAllLabels()
{
    removeNodalLabels();
    removeDistributedLabels();
    removeMomentLabels();
}

void LoadVisualization::removeNodalLabels()
{
    if (m_renderer) {
        for (auto& label : m_nodalForceLabels) {
            if (label) {
                m_renderer->RemoveActor(label);
            }
        }
    }
    m_nodalForceLabels.clear();
}

void LoadVisualization::removeDistributedLabels()
{
    if (m_renderer) {
        for (auto& label : m_distributedLoadLabels) {
            if (label) {
                m_renderer->RemoveActor(label);
            }
        }
    }
    m_distributedLoadLabels.clear();
}

void LoadVisualization::removeMomentLabels()
{
    if (m_renderer) {
        for (auto& label : m_momentLabels) {
            if (label) {
                m_renderer->RemoveActor(label);
            }
        }
    }
    m_momentLabels.clear();
}

} // namespace Structura::Visualization
