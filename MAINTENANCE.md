# Structura Ribbon 3D - Maintenance Guide

## Architecture Overview

This project follows a layered architecture to improve maintainability and testability.

### Directory Structure (`src/`)

- **`core/model/`**: Contains pure domain entities (`Node`, `Bar`, `Material`, etc.). These classes should **not** depend on Qt (except simple types like `QUuid`) or VTK. They represent the "Truth" of the engineering model.
- **`app/`**: Application services and repositories. Contains business logic, undo/redo mechanisms, and data management (`NodeService`, `IModelRepository`).
- **`viz/`**: Visualization layer. Contains VTK logic, `SceneController`, and rendering adapters. This layer listens to model changes and updates the view.
- **`ui/`**: User Interface. Qt Widgets, Dialogs, and Panels. These should only handle user interaction and delegate logic to `app/` services.
- **`utils/`**: Shared utilities.

## adding New Features

1. **Model**: If you need new data, add it to `src/core/model/`. Keep it POD-like (Plain Old Data) with invariants.
2. **Service**: Add methods to `src/app/` services to manipulate the model. Use the Repository pattern.
3. **UI**: Create the widget in `src/ui/` and call the service.
4. **Viz**: If it needs 3D representation, update `src/viz/SceneController` or `VtkSceneRenderer`.

## Building and Testing

- **Build**: Use CMake.
  ```bash
  cmake -S . -B build
  cmake --build build --config Debug
  ```
- **Tests**: Run unit tests via CTest.
  ```bash
  cd build
  ctest -C Debug
  ```

## Code Standards
- **Includes**: Use full paths from `src/` root (e.g., `#include "core/model/Node.h"`).
- **Naming**: Classes are PascalCase, methods camelCase.
