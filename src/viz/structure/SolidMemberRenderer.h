#pragma once

#include "viz/structure/SectionShapes.h"

#include <QMap>
#include <QString>

#include <array>
#include <map>
#include <vector>

#include <vtkActor.h>
#include <vtkUnsignedCharArray.h>
#include <vtkSmartPointer.h>

class vtkRenderer;

namespace Structura::Visualization {

/**
 * @brief Exibicao extrudada das barras (secao transversal ao longo do eixo).
 *
 * Todas as barras sao fundidas em dois atores (faces e arestas). Cada secao
 * distinta e triangulada uma unica vez e reaproveitada.
 */
class SolidMemberRenderer
{
public:
    using Vec3 = std::array<double, 3>;

    struct Member {
        Vec3 start {};
        Vec3 end {};
        Vec3 xAxis {1, 0, 0}; ///< Eixos locais da barra (mundo)
        Vec3 yAxis {0, 1, 0};
        Vec3 zAxis {0, 0, 1};
        QString family;
        SectionShapes::Parameters parameters;
        bool mirrored {false};
        std::array<unsigned char, 3> color {{180, 189, 199}};
        bool dimmed {false};
    };

    SolidMemberRenderer();
    ~SolidMemberRenderer();

    void attach(vtkRenderer *renderer);
    void setMembers(const std::vector<Member> &members);
    /// Destaca (cor de selecao) os solidos inteiros das barras marcadas; indices na mesma ordem de `setMembers`. So atualiza as cores.
    void setSelected(const std::vector<bool> &selected);
    void setVisible(bool visible);
    [[nodiscard]] bool visible() const noexcept { return m_visible; }

private:
    struct Geometry {
        std::vector<std::vector<std::array<double, 2>>> loops;   ///< Em metros, orientados (material a esquerda)
        std::vector<std::vector<std::array<double, 2>>> normals; ///< Normal 2D suavizada por vertice
        std::vector<std::vector<bool>> sharp;                    ///< Vertices de quina (arestas longitudinais)
        std::vector<std::array<std::array<double, 2>, 3>> capTriangles;
    };

    const Geometry *geometryFor(const Member &member);
    void rebuildActors(const std::vector<Member> &members);

    vtkRenderer *m_renderer {nullptr};
    vtkSmartPointer<vtkActor> m_faceActor;
    vtkSmartPointer<vtkActor> m_edgeActor;
    std::map<QString, Geometry> m_cache;
    bool m_visible {false};
    vtkSmartPointer<vtkUnsignedCharArray> m_colorArray;
    std::vector<std::pair<vtkIdType, vtkIdType>> m_ranges;      ///< intervalo de vertices de cada barra
    std::vector<std::array<unsigned char, 3>> m_baseColors;     ///< cor sem destaque
};

} // namespace Structura::Visualization
