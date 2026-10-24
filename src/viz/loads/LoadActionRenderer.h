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

    /// Origem de uma carga nos dados do projeto (tipo e indice no vetor de cargas); permite destacar e selecionar a carga na viewport.
    struct Key {
        int kind {-1};   ///< 0 = nodal, 1 = distribuida, 2 = momento distribuido, 3 = peso proprio
        int index {-1};
        [[nodiscard]] bool valid() const { return kind >= 0; }
        bool operator==(const Key &o) const { return kind == o.kind && index == o.index; }
    };

    struct NodeAction {
        Key key;
        Vec3 position {};
        Vec3 force {};  ///< Componentes globais (kN)
        Vec3 moment {}; ///< Componentes globais (kNm)
    };

    enum class MemberKind { Force, Moment };

    /// Uma carga uniforme em uma barra: componentes por eixo, em sistema local ou global.
    struct MemberAction {
        Key key;
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
        std::vector<Vec3> colors;      ///< cor do desenho da carga a que o rotulo pertence
        std::vector<double> weights;   ///< |valor|: prioridade do rotulo ao evitar poluicao
    };

    /// Geometria de selecao de uma carga: segmentos (setas, arcos, bordas) e quadrilateros (faixas de carga distribuida; a aresta 0-1 e a propria barra).
    struct PickItem {
        Key key;
        std::vector<std::array<Vec3, 2>> segments;
        std::vector<std::array<Vec3, 4>> quads;
    };

    LoadActionRenderer() = default;
    ~LoadActionRenderer();

    void attach(vtkRenderer *renderer) { m_renderer = renderer; }
    void render(const std::vector<NodeAction> &nodes,
                const std::vector<MemberAction> &members,
                const Visibility &visibility, const Key &highlighted = {});
    void clear();
    [[nodiscard]] const std::vector<PickItem> &pickItems() const noexcept { return m_picks; }
    [[nodiscard]] const Labels &labels() const noexcept { return m_labels; }

private:
    vtkRenderer *m_renderer {nullptr};
    std::vector<vtkSmartPointer<vtkActor>> m_actors;
    Labels m_labels;
    std::vector<PickItem> m_picks;
};

} // namespace Structura::Visualization
