#pragma once

#include "shell/ShellVisual.h"

#include <vtkActor.h>
#include <vtkCellPicker.h>
#include <vtkLookupTable.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <array>
#include <cstdint>
#include <vector>

class vtkRenderer;

namespace Structura::Visualization {

/// Malha de cascas colorida por vertice (resultados) com as arestas dos elementos em um segundo ator.
class ShellRenderer
{
public:
    ShellRenderer();
    void attach(vtkRenderer *renderer);
    void setMesh(const Structura::Shell::RenderMesh &mesh);
    void setVisible(bool visible);
    [[nodiscard]] bool visible() const noexcept { return m_visible; }
    /// Indice do quadrilatero sob o pixel (coordenadas de exibicao, origem embaixo); -1 se nenhum.
    [[nodiscard]] int pickQuad(int displayX, int displayY) const;

    /// Quadrilatero e ponto (coordenadas da cena) sob o pixel; quad = -1 se nenhum.
    struct PickResult {
        int quad {-1};
        std::array<double, 3> point {{0.0, 0.0, 0.0}};
    };
    [[nodiscard]] PickResult pickPoint(int displayX, int displayY) const;

    /// Destaque dos nos selecionados (pontos na cena); vazio remove. Nao e pickable e nao altera a malha.
    void setSelectedNodes(const std::vector<std::array<float, 3>> &points);
    [[nodiscard]] std::size_t selectedNodeCount() const noexcept { return m_selectedCount; }

    /// Apoio a desenhar: posicao na cena, tipo (valor de Structura::Shell::SupportKind) e direcao da mola (so-compressao).
    struct SupportGlyph {
        std::array<float, 3> position {{0.f, 0.f, 0.f}};
        int kind {0};
        std::array<float, 3> normal {{0.f, 0.f, 1.f}};
    };
    /// Desenha os apoios (glifos de `size` unidades da cena); lista vazia remove. Nao sao pickable.
    void setSupports(const std::vector<SupportGlyph> &glyphs, float size);
    void setSupportsVisible(bool visible);
    [[nodiscard]] std::size_t supportGlyphCount() const noexcept { return m_supportCount; }

    /// Seta de carga: a ponta fica em `tip` (cena), o corpo vem do lado oposto a `direction` (unitaria), com `length` unidades da cena.
    struct LoadArrowGlyph {
        std::array<float, 3> tip {{0.f, 0.f, 0.f}};
        std::array<float, 3> direction {{0.f, 0.f, -1.f}};
        float length {1.f};
        int kind {0};   ///< 0 forca nodal, 1 carga linear, 2 carga de superficie
    };
    void setLoadArrows(const std::vector<LoadArrowGlyph> &arrows);
    void setLoadArrowsVisible(bool visible);
    [[nodiscard]] std::size_t loadArrowCount() const noexcept { return m_arrowCount; }

    /// Quantas vezes a geometria foi reconstruida e quantas so as cores foram atualizadas (mesma `geometryRevision`).
    [[nodiscard]] int geometryRebuilds() const noexcept { return m_geometryRebuilds; }
    [[nodiscard]] int colorUpdates() const noexcept { return m_colorUpdates; }

private:
    vtkRenderer *m_renderer {nullptr};
    vtkSmartPointer<vtkActor> m_faces;
    vtkSmartPointer<vtkActor> m_edges;
    vtkSmartPointer<vtkCellPicker> m_picker;
    vtkSmartPointer<vtkPolyData> m_faceData;
    vtkSmartPointer<vtkLookupTable> m_bandTable;
    bool m_bandedColors {false};   ///< faces coloridas por valor com tabela de faixas (textura); senao cores RGB diretas
    vtkSmartPointer<vtkActor> m_selection;
    std::size_t m_selectedCount {0};
    vtkSmartPointer<vtkActor> m_supportFaces;
    vtkSmartPointer<vtkActor> m_supportLines;
    std::size_t m_supportCount {0};
    vtkSmartPointer<vtkActor> m_arrows;
    std::size_t m_arrowCount {0};
    bool m_arrowsVisible {true};
    bool m_supportsVisible {true};
    std::uint64_t m_revision {0};
    std::size_t m_vertexCount {0};
    int m_geometryRebuilds {0};
    int m_colorUpdates {0};
    bool m_visible {false};
};

} // namespace Structura::Visualization
