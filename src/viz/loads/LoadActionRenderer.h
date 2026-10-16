#pragma once

#include <QString>

#include <array>
#include <vector>

#include <vtkActor.h>
#include <vtkSmartPointer.h>

class vtkRenderer;

namespace Structura::Visualization {

/**
 * @brief Desenho das acoes (cargas) aplicadas a nos e barras.
 *
 * Forcas distribuidas viram faixas translucidas ao lado da barra, forcas
 * nodais viram setas, momentos viram arcos com ponta de seta. As geometrias
 * sao acumuladas em poucos atores (um por cor/espessura) e os valores
 * formatados sao entregues como rotulos para a camada de texto da viewport.
 */
class LoadActionRenderer
{
public:
    using Vec3 = std::array<double, 3>;

    struct NodeAction {
        Vec3 position {};
        Vec3 force {};  ///< Componentes globais (kN)
        Vec3 moment {}; ///< Componentes globais (kNm)
    };

    enum class MemberKind { Force, Moment };

    /// Uma carga uniforme em uma barra: componentes por eixo, em sistema local ou global.
    struct MemberAction {
        MemberKind kind {MemberKind::Force};
        Vec3 start {};
        Vec3 end {};
        std::array<Vec3, 3> basis {}; ///< Eixos locais x, y, z da barra (mundo)
        Vec3 values {};               ///< kN/m ou kNm/m por componente
        bool local {false};
        bool selfWeight {false};
    };

    struct Visibility {
        bool nodeForces {true};
        bool nodeMoments {true};
        bool memberForces {true};
        bool memberMoments {true};
    };

    struct Labels {
        std::vector<Vec3> positions;
        std::vector<QString> texts;
    };

    LoadActionRenderer() = default;
    ~LoadActionRenderer();

    void attach(vtkRenderer *renderer) { m_renderer = renderer; }
    void render(const std::vector<NodeAction> &nodes,
                const std::vector<MemberAction> &members,
                const Visibility &visibility);
    void clear();
    [[nodiscard]] const Labels &labels() const noexcept { return m_labels; }

private:
    vtkRenderer *m_renderer {nullptr};
    std::vector<vtkSmartPointer<vtkActor>> m_actors;
    Labels m_labels;
};

} // namespace Structura::Visualization
