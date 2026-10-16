#pragma once

#include "app/ProjectDataTypes.h"
#include "viz/construction/ConstructionGridRenderer.h"
#include "viz/construction/ConstructionPlane.h"
#include "viz/construction/ReferenceAxesRenderer.h"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <vtkSmartPointer.h>

class vtkActor;
class vtkPolyData;
class vtkPolyDataMapper;
class vtkRenderer;

namespace Structura::Construction {

/// Quais alvos de snap estao ativos (equivalente a `_snap_types` do opensa).
struct SnapSettings {
    bool enabled {true};
    bool grid {true};
    bool endpoint {true};
    bool center {true};
    bool perpendicular {true};
    bool orthogonal {true};
};

/**
 * @brief Estado e atores do plano de construcao.
 *
 * Reune o plano ativo (XY/XZ/YZ + nivel), a malha, os eixos de referencia,
 * a linha de previa da barra e as guias perpendicular/ortogonal usadas no
 * lancamento de elementos.
 */
class ConstructionScene
{
public:
    using ReferenceAxes = Structura::App::ProjectData::ReferenceAxes;

    static constexpr double kGuideHalfLength = 5.0;

    ConstructionScene();
    ~ConstructionScene();

    /// Anexa os atores ao renderer. `requestRender` e chamado apos mudancas visuais.
    void attach(vtkRenderer *renderer, std::function<void()> requestRender);

    // ── Modelo ───────────────────────────────────────────────────────────
    /// Posicoes dos nos e pontos medios das barras (alvos de snap).
    void setSnapGeometry(std::vector<Vec3> nodes, std::vector<Vec3> barMidpoints);
    void setAxes(const ReferenceAxes &axes);
    [[nodiscard]] const ReferenceAxes &axes() const noexcept { return m_axes; }
    [[nodiscard]] const std::vector<Vec3> &nodePositions() const noexcept { return m_nodes; }
    [[nodiscard]] const std::vector<Vec3> &barMidpoints() const noexcept { return m_barMidpoints; }

    // ── Plano de referencia ──────────────────────────────────────────────
    [[nodiscard]] PlaneMode planeMode() const noexcept { return m_mode; }
    void setPlaneMode(PlaneMode mode);
    [[nodiscard]] double level() const noexcept { return m_level; }
    void stepLevel(int step);
    void setLevel(double level);
    /// Niveis disponiveis: 0 e os valores dos eixos normais ao plano.
    [[nodiscard]] std::vector<double> levels() const;
    [[nodiscard]] std::vector<double> planeLevelsList() const { return levels(); }

    // ── Malha e eixos ────────────────────────────────────────────────────
    [[nodiscard]] double spacing() const noexcept { return m_spacing; }
    void setSpacing(double spacing);
    [[nodiscard]] bool gridVisible() const noexcept { return m_gridVisible; }
    void setGridVisible(bool visible);
    [[nodiscard]] bool referenceAxesVisible() const noexcept { return m_axesVisible; }
    void setReferenceAxesVisible(bool visible);
    [[nodiscard]] const ConstructionGridRenderer &grid() const noexcept { return m_grid; }
    [[nodiscard]] const AxisLabels &axisLabels() const noexcept { return m_labels; }

    /// Exibe a malha e os eixos de todos os niveis ao mesmo tempo (somente no plano XY).
    void setShowAllLevels(bool show);
    [[nodiscard]] bool showAllLevels() const noexcept { return m_showAllLevels && m_mode == PlaneMode::XY; }

    /// Valores da malha (alinhados ao espacamento + eixos nomeados) em u e v.
    void latticeValues(std::vector<double> &u, std::vector<double> &v) const;
    /// Igual a `latticeValues`, para um nivel especifico (usado com todos os niveis visiveis).
    void latticeValues(std::vector<double> &u, std::vector<double> &v, double level) const;
    /// Janela da malha (inclui margem) para limitar o lancamento.
    [[nodiscard]] const Window &gridWindow() const noexcept { return m_grid.bounds(); }

    // ── Snap ─────────────────────────────────────────────────────────────
    [[nodiscard]] SnapSettings &snap() noexcept { return m_snap; }
    [[nodiscard]] const SnapSettings &snap() const noexcept { return m_snap; }

    // ── Lancamento ───────────────────────────────────────────────────────
    void setPreviewLine(const std::optional<std::pair<Vec3, Vec3>> &line);
    void setGuides(const std::optional<Vec3> &start);
    void hidePlacementVisuals();
    /// Segmentos "fantasma" (previa de copiar/mover); vazio oculta.
    void setGhostSegments(const std::vector<std::pair<Vec3, Vec3>> &segments);

    /// Chamado quando os rotulos dos eixos de referencia mudam (plano, nivel, eixos).
    std::function<void()> onLabelsChanged;
    /// Chamado quando o plano/nivel muda (para atualizar controles de UI).
    std::function<void()> onPlaneChanged;

private:
    void rebuild();
    void render() const;
    Window footprint() const;
    void updateGuideActor(vtkPolyData *data, const Vec3 &start, bool perpendicular);

    vtkRenderer *m_renderer {nullptr};
    std::function<void()> m_requestRender;

    ConstructionGridRenderer m_grid;
    ReferenceAxesRenderer m_axes_renderer;

    /// Camadas extras (um nivel cada) usadas quando todos os niveis estao visiveis.
    struct LevelLayer {
        ConstructionGridRenderer grid;
        ReferenceAxesRenderer axes;
    };
    std::vector<std::unique_ptr<LevelLayer>> m_layers;
    AxisLabels m_labels;
    bool m_showAllLevels {false};
    vtkSmartPointer<vtkPolyData> m_ghostData;
    vtkSmartPointer<vtkActor> m_ghostActor;

    vtkSmartPointer<vtkPolyData> m_previewData;
    vtkSmartPointer<vtkActor> m_previewActor;
    vtkSmartPointer<vtkPolyData> m_perpendicularData;
    vtkSmartPointer<vtkActor> m_perpendicularActor;
    vtkSmartPointer<vtkPolyData> m_orthogonalData;
    vtkSmartPointer<vtkActor> m_orthogonalActor;

    std::vector<Vec3> m_nodes;
    std::vector<Vec3> m_barMidpoints;
    ReferenceAxes m_axes;
    PlaneMode m_mode {PlaneMode::XY};
    double m_level {0.0};
    double m_spacing {1.0};
    bool m_gridVisible {true};
    bool m_axesVisible {true};
    SnapSettings m_snap;
};

} // namespace Structura::Construction
