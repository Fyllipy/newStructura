#pragma once

#include "viz/construction/ConstructionPlane.h"

#include <vtkActor.h>
#include <vtkSmartPointer.h>

class vtkPolyData;
class vtkPolyDataMapper;

namespace Structura::Construction {

/**
 * @brief Malha do plano de construcao (equivalente ao grid do opensa).
 *
 * A janela visivel acompanha a "pegada" do modelo no plano ativo, mas seus
 * limites sao alinhados a malha global (multiplos do espacamento). Cada
 * vertice recebe um alfa que decai da pegada estrutural ate a margem externa.
 */
class ConstructionGridRenderer
{
public:
    static constexpr int kMarginCells = 5;
    static constexpr int kMaxCellsPerSide = 400;

    ConstructionGridRenderer();
    ~ConstructionGridRenderer();

    [[nodiscard]] vtkActor *actor() const { return m_actor; }

    /// Reconstroi a malha apenas quando pegada, plano, nivel ou espacamento mudam.
    void update(const Window &footprint, PlaneMode mode, double offset, double spacing);

    /// Janela alinhada a malha (inclui a margem).
    [[nodiscard]] const Window &bounds() const noexcept { return m_bounds; }
    /// Espacamento efetivo (multiplo do solicitado se a malha ficaria grande demais).
    [[nodiscard]] double effectiveSpacing() const noexcept { return m_effectiveSpacing; }

    void setVisible(bool visible);

private:
    void build(const Window &footprint, PlaneMode mode, double offset, double spacing);

    vtkSmartPointer<vtkPolyData> m_data;
    vtkSmartPointer<vtkPolyDataMapper> m_mapper;
    vtkSmartPointer<vtkActor> m_actor;

    Window m_bounds;
    Window m_footprint;
    PlaneMode m_mode {PlaneMode::XY};
    double m_offset {0.0};
    double m_spacing {1.0};
    double m_effectiveSpacing {1.0};
    bool m_built {false};
};

} // namespace Structura::Construction
