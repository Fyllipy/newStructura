#pragma once

#include <vtkSmartPointer.h>

#include <array>
#include <vector>

class vtkActor;
class vtkPolyData;
class vtkRenderer;

namespace Structura::Visualization {

/**
 * @brief Indicacao visual do tipo de barra: rotulas (esfera branca com contorno nas extremidades articuladas)
 *        e destaque colorido ao longo de trelicas (violeta) e tirantes (laranja, com marcas ao longo da barra).
 */
class BarTypeRenderer
{
public:
    using Vec3 = std::array<double, 3>;

    struct Marker {
        Vec3 start {0, 0, 0};
        Vec3 end {0, 0, 0};
        bool hingeStart {false};   ///< rotacoes de flexao liberadas em I
        bool hingeEnd {false};
        int kind {0};              ///< 0 = normal, 1 = trelica, 2 = tirante
    };

    BarTypeRenderer();
    ~BarTypeRenderer();

    void attach(vtkRenderer *renderer);
    void render(const std::vector<Marker> &markers, bool visible);

private:
    vtkSmartPointer<vtkActor> makeGlyphActor(vtkPolyData *points, double radius, bool outlineOnly);

    vtkRenderer *m_renderer {nullptr};
    std::vector<vtkSmartPointer<vtkActor>> m_actors;
    vtkSmartPointer<vtkActor> m_lineActor;
};

} // namespace Structura::Visualization
