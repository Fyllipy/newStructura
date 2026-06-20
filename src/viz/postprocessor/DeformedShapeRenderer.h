#pragma once

class vtkRenderer;

namespace Structura::Visualization {

/**
 * @brief Stub para futura renderização da deformada estrutural.
 *
 * A arquitetura está preparada para expansão. A adição da deformada
 * não exigirá refatoração do PostProcessorRenderer.
 *
 * Modos futuros:
 *   Original  — apenas geometria não deformada
 *   Deformed  — apenas geometria deformada
 *   Overlay   — original + deformada sobrepostos
 */
class DeformedShapeRenderer {
public:
    enum class DisplayMode {
        Original,   ///< Apenas geometria original
        Deformed,   ///< Apenas geometria deformada
        Overlay     ///< Original + deformada sobrepostos
    };

    void initialize(vtkRenderer* renderer)
    {
        m_renderer = renderer;
    }

    void clearAll()
    {
        // Noop até implementação
    }

    // ── Reservado para implementação futura ────────────────────────────────
    // struct NodeDisplacement {
    //     int nodeId;
    //     std::array<double, 3> translation;
    //     std::array<double, 3> rotation;
    // };
    //
    // void setDisplacements(const std::vector<NodeDisplacement>& displacements);
    // void setScaleFactor(double scale);
    // void setDisplayMode(DisplayMode mode);
    // void show();
    // void hide();

private:
    vtkRenderer* m_renderer{nullptr};
};

} // namespace Structura::Visualization
