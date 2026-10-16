#pragma once

#include <vtkSmartPointer.h>

#include <array>
#include <vector>

class vtkActor;
class vtkPolyData;
class vtkPolyDataMapper;
class vtkRenderer;

namespace Structura::Visualization {

/**
 * @brief Desenha os panos das cargas de area: superficie translucida, contorno e setas da pressao sobre o pano
 *        e ao longo das barras que a recebem (a acao do pano nas barras).
 */
class AreaPanelRenderer
{
public:
    using Vec3 = std::array<double, 3>;

    struct Panel {
        std::vector<Vec3> polygon;      ///< contorno (3 ou mais pontos, m)
        Vec3 direction {0.0, 0.0, -1.0};   ///< sentido da pressao positiva (qualquer modulo)
        double pressure {0.0};          ///< kN/m2 com sinal; negativo inverte as setas
        bool wind {false};              ///< cor diferente para o vento
        std::vector<std::pair<Vec3, Vec3>> bars;   ///< barras que recebem a carga (inicio, fim)
    };

    AreaPanelRenderer();
    ~AreaPanelRenderer();

    void attach(vtkRenderer *renderer);
    void render(const std::vector<Panel> &panels, bool visible);

private:
    vtkSmartPointer<vtkPolyData> m_surfaceData;
    vtkSmartPointer<vtkPolyDataMapper> m_surfaceMapper;
    vtkSmartPointer<vtkActor> m_surfaceActor;
    vtkSmartPointer<vtkPolyData> m_lineData;
    vtkSmartPointer<vtkPolyDataMapper> m_lineMapper;
    vtkSmartPointer<vtkActor> m_lineActor;
};

} // namespace Structura::Visualization
