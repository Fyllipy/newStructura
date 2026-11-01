#pragma once

#include <vtkSmartPointer.h>

#include <array>
#include <vector>

class vtkActor;
class vtkPolyData;
class vtkRenderer;

namespace Structura::Visualization {

/**
 * @brief Indicacao visual do tipo de barra: rotulas (esfera branca com contorno nas extremidades articuladas),
 *        ligacoes semirrigidas (esfera verde-agua), rotulas plasticas (cubo vermelho) e destaque colorido ao longo de trelicas (violeta) e tirantes (laranja, com marcas ao longo da barra).
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
        bool semiStart {false};    ///< ligacao semirrigida (mola rotacional) em I
        bool semiEnd {false};
        bool plasticStart {false}; ///< rotula plastica em I
        bool plasticEnd {false};
        int kind {0};              ///< 0 = normal, 1 = trelica, 2 = tirante
    };

    BarTypeRenderer();
    ~BarTypeRenderer();

    void attach(vtkRenderer *renderer);
    void render(const std::vector<Marker> &markers, bool visible);
    /// Mostra/oculta todos os glifos sem recriar a geometria.
    void setVisible(bool visible);

private:
    enum class Shape { Sphere, Cube };
    vtkSmartPointer<vtkActor> makeGlyphActor(vtkPolyData *points, double radius, bool outlineOnly, Shape shape = Shape::Sphere);

    vtkRenderer *m_renderer {nullptr};
    std::vector<vtkSmartPointer<vtkActor>> m_actors;
    vtkSmartPointer<vtkActor> m_lineActor;
    bool m_visible {true};
    bool m_hasLines {false};
};

} // namespace Structura::Visualization
