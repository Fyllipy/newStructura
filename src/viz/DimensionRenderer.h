#pragma once

#include <vtkSmartPointer.h>

#include <array>
#include <vector>

class vtkActor;
class vtkPolyData;
class vtkPolyDataMapper;
class vtkRenderer;

namespace Structura::Visualization {

/// Desenha as linhas das cotas (chamadas, linha de cota e marcas) como polilinhas sobre o modelo.
class DimensionRenderer
{
public:
    using Vec3 = std::array<double, 3>;

    DimensionRenderer();
    ~DimensionRenderer();

    void attach(vtkRenderer *renderer);
    void render(const std::vector<std::vector<Vec3>> &lines);

private:
    vtkSmartPointer<vtkPolyData> m_data;
    vtkSmartPointer<vtkPolyDataMapper> m_mapper;
    vtkSmartPointer<vtkActor> m_actor;
};

} // namespace Structura::Visualization
