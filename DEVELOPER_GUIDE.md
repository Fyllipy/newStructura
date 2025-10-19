# LoadVisualization - Developer Guide

## Quick Reference

### Basic Usage

```cpp
// In SceneController initialization
m_loadVisualization = std::make_unique<Structura::Visualization::LoadVisualization>();
m_loadVisualization->initialize(m_renderer);

// Update nodal loads
QVector<Structura::Visualization::LoadVisualization::NodalLoad> nodalLoads;
Structura::Visualization::LoadVisualization::NodalLoad load;
load.position = QVector3D(x, y, z);
load.force = QVector3D(fx, fy, fz);
load.moment = QVector3D(mx, my, mz);
nodalLoads.append(load);
m_loadVisualization->setNodalLoads(nodalLoads);

// Update distributed loads
QVector<Structura::Visualization::LoadVisualization::DistributedLoad> distLoads;
Structura::Visualization::LoadVisualization::DistributedLoad dist;
dist.startPoint = QVector3D(x1, y1, z1);
dist.endPoint = QVector3D(x2, y2, z2);
dist.loadVector = QVector3D(qx, qy, qz);
dist.isLocalSystem = false; // or true for local coordinates
distLoads.append(dist);
m_loadVisualization->setDistributedLoads(distLoads);
```

### Class Structure

```
LoadVisualization
├── Public Methods
│   ├── initialize(vtkRenderer*)
│   ├── setNodalLoads(QVector<NodalLoad>)
│   ├── setDistributedLoads(QVector<DistributedLoad>)
│   ├── clearAll()
│   ├── setVisible(bool)
│   ├── setNodalLoadsVisible(bool)
│   ├── setDistributedLoadsVisible(bool)
│   └── setMomentsVisible(bool)
│
├── Public Static Methods
│   ├── computeScaledMagnitude(double) -> double
│   └── computePerpendicularVector(QVector3D) -> QVector3D
│
└── Data Structures
    ├── NodalLoad { position, force, moment }
    └── DistributedLoad { startPoint, endPoint, loadVector, isLocalSystem }
```

## Key Algorithms

### 1. Distributed Load Arrow Placement

**Spacing Rule**: 6% of bar length

```cpp
barLength = (endPoint - startPoint).length();
spacing = barLength * 0.06;
numArrows = clamp(barLength / spacing, 3, 20);

// Distribute evenly, excluding endpoints
for (i = 0; i < numArrows; i++) {
    t = (i + 1) / (numArrows + 1);  // Range: (0, 1) exclusive
    position = startPoint + t * (endPoint - startPoint);
    createArrow(position, loadDirection);
}
```

**Why exclude endpoints?**
- Visual clarity: arrows don't overlap with node symbols
- Structural convention: distributed loads typically shown between nodes

### 2. Magnitude Scaling

**Logarithmic scaling** for visual consistency across orders of magnitude:

```cpp
scaledMagnitude(m) = max(0.12, log10(1 + m) * 0.6)
```

**Examples:**
- 1 kN → 0.18 scale
- 10 kN → 0.66 scale
- 100 kN → 1.26 scale
- 1000 kN → 1.86 scale

### 3. Moment Arc Construction

**Create semicircle perpendicular to moment axis:**

```cpp
// 1. Normalize moment axis
axis = moment.normalized();

// 2. Find two orthogonal vectors in perpendicular plane
tangent = findPerpendicular(axis);
bitangent = cross(axis, tangent);

// 3. Create arc points
for (angle = 0 to π, step = π/16) {
    offset = radius * (cos(angle) * tangent + sin(angle) * bitangent);
    point = center + offset;
    arcPoints.append(point);
}

// 4. Add arrow head at end
arrowDirection = derivative at end angle
arrowHead = cone(endPoint, arrowDirection, height=0.08)
```

## Visual Configuration Constants

Located in `LoadVisualization.h`:

```cpp
// Arrow geometry
DEFAULT_ARROW_TIP_LENGTH = 0.35
DEFAULT_ARROW_TIP_RADIUS = 0.08
DEFAULT_ARROW_SHAFT_RADIUS = 0.03
DEFAULT_ARROW_SCALE_FACTOR = 0.18
DISTRIBUTED_ARROW_SCALE_FACTOR = 0.14

// Spacing and bounds
DISTRIBUTED_LOAD_SPACING_RATIO = 0.06  // 6%
MINIMUM_ARROWS_PER_BAR = 3
MAXIMUM_ARROWS_PER_BAR = 20

// Moment arcs
MOMENT_ARC_SEGMENTS = 16  // Half circle
MOMENT_BASE_RADIUS = 0.18
MOMENT_CONE_HEIGHT = 0.08
MOMENT_CONE_RADIUS = 0.04

// Labels
LABEL_OFFSET_DISTANCE = 0.35
LABEL_FONT_SIZE = 14
```

**To adjust visualization:**
1. Modify constants in header
2. Rebuild project
3. No need to change algorithms

## Color Scheme

Defined in `LoadVisualization.cpp`:

```cpp
namespace {
    constexpr double FORCE_COLOR[3] = {0.90, 0.15, 0.20};           // Red
    constexpr double DISTRIBUTED_LOAD_COLOR[3] = {0.15, 0.58, 0.32}; // Green
    constexpr double MOMENT_COLOR[3] = {0.65, 0.20, 0.82};          // Purple
}
```

**Why these colors?**
- **Red** (forces): High visibility, industry standard for forces
- **Green** (distributed): Distinct from forces, lower intensity
- **Purple** (moments): Unique color, clearly different from forces

## Label Formatting

**3 significant figures** with adaptive precision:

```cpp
if (magnitude >= 100.0)
    text = QString("%1 %2").arg(magnitude, 0, 'f', 1).arg(unit);
else if (magnitude >= 10.0)
    text = QString("%1 %2").arg(magnitude, 0, 'f', 2).arg(unit);
else
    text = QString("%1 %2").arg(magnitude, 0, 'f', 3).arg(unit);
```

**Examples:**
- 1234.5 kN → "1234.5 kN" (1 decimal)
- 123.45 kN → "123.45 kN" (2 decimals)
- 12.345 kN → "12.345 kN" (3 decimals)
- 1.2345 kN → "1.235 kN" (3 decimals)

## Performance Considerations

### Memory Management
- Uses VTK smart pointers (`vtkSmartPointer`)
- Automatic cleanup when objects destroyed
- Labels explicitly removed from renderer on update

### Update Strategy
- **Idempotent**: Can call multiple times safely
- **Full rebuild**: Clears all data and recreates
- **Efficient**: Reuses same actors and mappers

### Optimization for Large Models
Current implementation handles up to:
- **1000s of nodal loads**: One arrow each
- **1000s of distributed loads**: Multiple arrows each
- **1000s of moments**: Arc geometry each

For larger models, consider:
1. Level-of-detail (LOD) based on camera distance
2. Culling of off-screen loads
3. Instancing for repeated geometry

## Common Pitfalls

### ❌ Don't: Manually manage VTK objects
```cpp
// BAD
vtkArrowSource* arrow = vtkArrowSource::New();
// ... forget to call arrow->Delete()
```

### ✅ Do: Use smart pointers
```cpp
// GOOD
vtkSmartPointer<vtkArrowSource> arrow = vtkSmartPointer<vtkArrowSource>::New();
// Automatic cleanup
```

### ❌ Don't: Forget to call Modified()
```cpp
// BAD
m_nodalForcePoints->InsertNextPoint(x, y, z);
// VTK pipeline won't update
```

### ✅ Do: Mark data as modified
```cpp
// GOOD
m_nodalForcePoints->InsertNextPoint(x, y, z);
m_nodalForcePoints->Modified();
```

### ❌ Don't: Assume bar orientation
```cpp
// BAD - arrows follow bar direction
direction = (endPoint - startPoint).normalized();
```

### ✅ Do: Use load direction
```cpp
// GOOD - arrows follow load direction
direction = loadVector.normalized();
```

## Debugging Tips

### Visualize individual components
```cpp
m_loadVisualization->setNodalLoadsVisible(true);
m_loadVisualization->setDistributedLoadsVisible(false);
m_loadVisualization->setMomentsVisible(false);
```

### Check arrow counts
```cpp
qDebug() << "Nodal force points:" << m_nodalForcePoints->GetNumberOfPoints();
qDebug() << "Distributed load points:" << m_distributedLoadPoints->GetNumberOfPoints();
qDebug() << "Moment arcs:" << m_momentLines->GetNumberOfCells();
```

### Verify transformations
```cpp
// Print first few positions
for (int i = 0; i < 3 && i < points->GetNumberOfPoints(); i++) {
    double p[3];
    points->GetPoint(i, p);
    qDebug() << "Point" << i << ":" << p[0] << p[1] << p[2];
}
```

## Extension Points

### Adding new load types

1. **Define structure in header:**
```cpp
struct SurfaceLoad {
    QVector3D center;
    QVector3D normal;
    double pressure;
};
```

2. **Add private member:**
```cpp
QVector<SurfaceLoad> m_surfaceLoads;
vtkSmartPointer<vtkActor> m_surfaceLoadActor;
```

3. **Implement methods:**
```cpp
void setSurfaceLoads(const QVector<SurfaceLoad>& loads);
void rebuildSurfaceLoads();
void createSurfacePressure(const SurfaceLoad& load);
```

4. **Update rebuild chain:**
```cpp
void updateLoadVisuals() {
    rebuildNodalForces();
    rebuildDistributedLoads();
    rebuildMoments();
    rebuildSurfaceLoads();  // Add this
}
```

## API Stability

### Stable (safe to use):
- All public methods
- NodalLoad and DistributedLoad structures
- Static utility methods

### Internal (may change):
- Private methods
- VTK object details
- Specific rendering parameters

## See Also
- `IMPLEMENTATION_SUMMARY.md` - High-level overview
- `plannertemp.md` - Original requirements
- VTK Documentation: https://vtk.org/doc/
- Qt Documentation: https://doc.qt.io/
