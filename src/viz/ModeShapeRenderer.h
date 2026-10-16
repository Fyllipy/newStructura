#pragma once

#include <vtkSmartPointer.h>

#include <array>
#include <vector>

class vtkActor;
class vtkPolyData;
class vtkPolyDataMapper;
class vtkRenderer;

namespace Structura::Visualization {

/// Desenha a forma modal (ou qualquer deformada) como polilinhas sobre a estrutura.
class ModeShapeRenderer
{
public:
    using Vec3 = std::array<double, 3>;

    ModeShapeRenderer();
    ~ModeShapeRenderer();

    void attach(vtkRenderer *renderer);
    void render(const std::vector<std::vector<Vec3>> &lines, bool visible);

private:
    vtkSmartPointer<vtkPolyData> m_data;
    vtkSmartPointer<vtkPolyDataMapper> m_mapper;
    vtkSmartPointer<vtkActor> m_actor;
};

} // namespace Structura::Visualization
