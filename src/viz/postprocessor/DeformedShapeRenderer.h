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
 * @brief Diagrama de deslocamentos: deformada de cada barra (polilinha amostrada ao longo do eixo), com escala automatica.
 *
 * Cada barra traz os pontos do eixo indeformado e o vetor de deslocamento global em cada ponto (translacoes da barra: deslocamentos nodais mais a
 * flexao entre os nos). A escala automatica faz o maior deslocamento valer uma fracao do tamanho do modelo; o controle de escala dos diagramas
 * multiplica essa escala.
 */
class DeformedShapeRenderer {
public:
    using Vec3 = std::array<double, 3>;

    struct Member {
        int id {0};
        std::vector<Vec3> base;   ///< pontos do eixo indeformado
        std::vector<Vec3> disp;   ///< deslocamento global em cada ponto (mesma unidade de comprimento do modelo)
    };

    DeformedShapeRenderer();
    ~DeformedShapeRenderer();

    void initialize(vtkRenderer *renderer);
    void clearAll();

    void setData(std::vector<Member> members, double modelExtent, int maxNode);
    void setUserScale(double scale);   ///< 1 = escala automatica
    void setVisible(bool visible);
    [[nodiscard]] bool visible() const noexcept { return m_visible; }

    [[nodiscard]] bool hasData() const noexcept { return !m_members.empty(); }
    [[nodiscard]] double maxDisplacement() const noexcept { return m_maxDisplacement; }   ///< modulo, na unidade do modelo
    [[nodiscard]] int maxNode() const noexcept { return m_maxNode; }
    [[nodiscard]] double scaleFactor() const noexcept { return m_autoScale * m_userScale; }

private:
    void rebuild();

    vtkRenderer *m_renderer {nullptr};
    vtkSmartPointer<vtkPolyData> m_data;
    vtkSmartPointer<vtkPolyDataMapper> m_mapper;
    vtkSmartPointer<vtkActor> m_actor;
    std::vector<Member> m_members;
    double m_extent {1.0};
    double m_maxDisplacement {0.0};
    double m_autoScale {1.0};
    double m_userScale {1.0};
    int m_maxNode {0};
    bool m_visible {false};
};

} // namespace Structura::Visualization
