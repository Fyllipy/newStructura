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

---

# Bar Local Coordinate System (LCS) - Developer Guide

## Overview

The Local Coordinate System (LCS) feature provides visualization of the local axes (x', y', z') for structural bars. This is essential for understanding how loads and internal forces are oriented relative to each bar's local reference frame.

## Quick Reference

### Basic Usage

```cpp
// Enable/disable LCS visualization
m_sceneController->setShowBarLCS(true);

// Update LCS after bar modifications
m_sceneController->updateBarLCSVisuals();

// Check if LCS is currently displayed
bool showing = m_sceneController->isShowingBarLCS();

// Set custom K point for a bar
Bar* bar = m_sceneController->findBar(barId);
if (bar) {
    std::array<double, 3> kPoint = {1.0, 0.0, 0.0};
    bar->setKPoint(kPoint);
    m_sceneController->updateBarLCSVisuals();
}
```

### User Interface

In the footer bar (bottom of window):
- **Checkbox**: "Mostrar eixos locais (LCS)"
- Toggle on/off to show/hide all bar local axes

## Class Structure

```
LocalCoordinateSystem (Geometry Module)
├── struct LCS
│   ├── xPrime: std::array<double, 3>  // Local x-axis
│   ├── yPrime: std::array<double, 3>  // Local y-axis  
│   ├── zPrime: std::array<double, 3>  // Local z-axis
│   └── origin: std::array<double, 3>  // Origin point
│
├── interface ILocalAxisProvider
│   └── computeLCS(pointA, pointB, kPoint) -> LCS
│
└── class DefaultLocalAxisProvider : ILocalAxisProvider
    ├── computeLCS() [implements algorithm]
    ├── setParallelEpsilon()
    └── Private geometry utilities

Bar (Model Entity)
├── kPoint: std::optional<std::array<double, 3>>
├── lcsDirty: bool
├── setKPoint()
├── clearKPoint()
└── hasKPoint()

SceneController (Visualization)
├── setShowBarLCS(bool)
├── isShowingBarLCS() -> bool
├── updateBarLCSVisuals()
└── rebuildBarLCSVisuals() [private]
```

## LCS Computation Algorithm

### Mathematical Formulation

The algorithm follows structural analysis conventions:

1. **X' Axis (Longitudinal)**: Along bar direction
   ```
   x' = (B - A) / ||B - A||
   ```

2. **Auxiliary Vector Selection**: 
   - If K point provided: `v = K - A`
   - If parallel or invalid: Use fallback (global X, Y, or Z)

3. **Z' Axis (Perpendicular)**: Cross product
   ```
   z' = (x' × v) / ||x' × v||
   ```

4. **Y' Axis (Completes right-hand system)**:
   ```
   y' = z' × x'
   ```

5. **Origin**: Bar midpoint
   ```
   origin = (A + B) / 2
   ```

### Code Implementation

```cpp
Structura::Geometry::DefaultLocalAxisProvider lcsProvider;

std::array<double, 3> pointA = {nodeA->x(), nodeA->y(), nodeA->z()};
std::array<double, 3> pointB = {nodeB->x(), nodeB->y(), nodeB->z()};
std::optional<std::array<double, 3>> kPoint = bar.kPoint();

try {
    auto lcs = lcsProvider.computeLCS(pointA, pointB, kPoint);
    // lcs.xPrime, lcs.yPrime, lcs.zPrime, lcs.origin
} catch (const std::exception& e) {
    // Handle degenerate bars or computation errors
}
```

## K Point (Reference Point)

### Purpose
The K point defines the orientation of the local z-axis. It's crucial for:
- Beam local axes orientation
- Principal axes alignment with structure geometry
- Consistent load and stress interpretation

### Storage
- **Type**: `std::optional<std::array<double, 3>>`
- **Persistence**: Saved with bar data
- **Default**: None (automatic selection)

### Automatic Selection
When no K point is specified, the algorithm tries:
1. Global X direction: `(1, 0, 0)`
2. Global Y direction: `(0, 1, 0)`  
3. Global Z direction: `(0, 0, 1)`

The first non-parallel direction is used.

### Validation
- **Parallel check**: K point cannot be parallel to bar axis
- **Tolerance**: Default `1e-5` (configurable)
- **Fallback**: Automatic if K point invalid

## Visualization Details

### Visual Properties

```cpp
// Colors (RGB 0-255)
X' axis: Red   (255, 0, 0)
Y' axis: Green (0, 255, 0)
Z' axis: Blue  (0, 0, 255)

// Arrow geometry
Arrow length: 25% of bar length
Line width: 2.5 pixels
Lighting: Off (flat colors)
Pickable: No (doesn't interfere with selection)
```

### Rendering Strategy

1. **Full rebuild on demand**: Not real-time cached
2. **Per-bar iteration**: Computes LCS for each bar
3. **Error handling**: Skips degenerate bars
4. **Efficient**: Uses VTK line primitives

### Performance

- **Typical overhead**: Negligible for <10,000 bars
- **Update frequency**: Only when bars change or toggle state
- **Memory**: 3 lines × 2 points × N bars

## Configuration Constants

In `SceneController.cpp`:

```cpp
constexpr double arrowScale = 0.25;  // Arrow length = 25% of bar length
```

In `LocalCoordinateSystem.cpp`:

```cpp
constexpr double kMinBarLength = 1e-9;     // Minimum valid bar length
DefaultLocalAxisProvider::m_parallelEps = 1e-5;  // Parallel tolerance
```

### Adjusting Arrow Length

```cpp
// Current: 25% of bar length
constexpr double arrowScale = 0.25;

// Shorter (15%):
constexpr double arrowScale = 0.15;

// Longer (40%):
constexpr double arrowScale = 0.40;
```

## Common Use Cases

### 1. Verify Beam Orientation

```cpp
// Enable LCS display
m_sceneController->setShowBarLCS(true);

// Visual inspection in viewport:
// - X' (red) should align with beam span
// - Y' (green) typically vertical for floor beams
// - Z' (blue) perpendicular to web for I-beams
```

### 2. Define Custom K Point

```cpp
Bar* bar = m_sceneController->findBar(barId);
if (bar) {
    // Example: Force Z' to point upward (for horizontal beam)
    std::array<double, 3> kUp = {0.0, 0.0, 1.0};
    bar->setKPoint(kUp);
    bar->setLCSDirty(true);
    m_sceneController->updateBarLCSVisuals();
}
```

### 3. Export LCS Data

```cpp
Structura::Geometry::DefaultLocalAxisProvider provider;

for (const auto& bar : bars) {
    auto lcs = provider.computeLCS(pointA, pointB, bar.kPoint());
    
    // Export to analysis software
    outputFile << "BAR " << bar.externalId() << "\n";
    outputFile << "  X': " << lcs.xPrime[0] << ", " 
               << lcs.xPrime[1] << ", " << lcs.xPrime[2] << "\n";
    outputFile << "  Y': " << lcs.yPrime[0] << ", " 
               << lcs.yPrime[1] << ", " << lcs.yPrime[2] << "\n";
    outputFile << "  Z': " << lcs.zPrime[0] << ", " 
               << lcs.zPrime[1] << ", " << lcs.zPrime[2] << "\n";
}
```

## Edge Cases and Error Handling

### Degenerate Bars
- **Definition**: Bar length < 1e-9
- **Behavior**: Skipped in visualization
- **Exception**: `std::runtime_error` in `computeLCS()`

### Parallel K Point
- **Detection**: `|dot(x', normalize(K-A))| ≈ 1`
- **Behavior**: Falls back to automatic selection
- **User feedback**: Silent (uses valid fallback)

### Zero-length Auxiliary Vector
- **Cause**: K point = point A
- **Behavior**: Falls back to automatic selection
- **Prevention**: Validate K point before setting

## Integration with Analysis

### Local Load Application

```cpp
// Convert global load to local coordinates
QVector3D globalLoad(Fx, Fy, Fz);

// Get bar LCS
auto lcs = provider.computeLCS(pointA, pointB, bar.kPoint());

// Transform to local (matrix multiplication)
double Fx_local = globalLoad.x() * lcs.xPrime[0] + 
                  globalLoad.y() * lcs.xPrime[1] + 
                  globalLoad.z() * lcs.xPrime[2];
// ... (similarly for Fy_local, Fz_local)
```

### Stress Results Visualization

```cpp
// Bar internal forces are in local system
double N = axialForce;      // Along x'
double Vy = shearForceY;    // Along y'
double Vz = shearForceZ;    // Along z'
double Mx = torque;         // About x'
double My = momentY;        // About y'
double Mz = momentZ;        // About z'

// Visualize using LCS orientation for proper alignment
```

## Debugging Tips

### Verify Orthogonality

```cpp
auto lcs = provider.computeLCS(pointA, pointB, kPoint);

// Check unit vectors
auto lenX = sqrt(lcs.xPrime[0]*lcs.xPrime[0] + ...);
assert(fabs(lenX - 1.0) < 1e-6);

// Check orthogonality
auto dotXY = lcs.xPrime[0]*lcs.yPrime[0] + ...;
assert(fabs(dotXY) < 1e-6);
```

### Visualize K Points

```cpp
// Add temporary visualization of K points
if (bar.hasKPoint()) {
    auto k = bar.kPoint().value();
    // Draw sphere at (k[0], k[1], k[2])
    // Draw line from A to K
}
```

### Check Handedness

```cpp
// Verify right-hand rule: x' × y' = z'
auto computed_z = cross(lcs.xPrime, lcs.yPrime);
assert(fabs(computed_z[0] - lcs.zPrime[0]) < 1e-6);
// ... (similarly for y and z components)
```

## API Stability

### Stable (safe to use):
- `LCS` struct
- `ILocalAxisProvider` interface
- `DefaultLocalAxisProvider` public methods
- `Bar::kPoint()`, `Bar::setKPoint()`, `Bar::clearKPoint()`
- `SceneController::setShowBarLCS()`, `isShowingBarLCS()`

### Internal (may change):
- Private geometry utilities
- Exact color values
- Arrow scale factor
- Rendering implementation details

## Future Enhancements

### Planned Features
1. **Interactive K Point Selection**: Click in viewport to define K point
2. **LCS Labels**: Show "X'", "Y'", "Z'" text labels
3. **Per-bar Toggle**: Show/hide LCS for selected bars only
4. **Analysis Integration**: Use LCS for local load input dialogs

### Extension Points

```cpp
// Custom LCS provider (e.g., for shells, plates)
class ShellLocalAxisProvider : public ILocalAxisProvider {
    LCS computeLCS(pointA, pointB, kPoint) const override {
        // Custom shell element orientation logic
    }
};

// Use dependency injection
SceneController::setLocalAxisProvider(
    std::make_unique<ShellLocalAxisProvider>()
);
```

## References

- **Theory**: Bathe, K.J. "Finite Element Procedures", Chapter 5
- **Implementation**: `src/LocalCoordinateSystem.h`, `.cpp`
- **Visualization**: `src/SceneController.cpp` (rebuildBarLCSVisuals)
- **Planning**: `plano2.md` (original specification)

