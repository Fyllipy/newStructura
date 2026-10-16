#pragma once

#include "app/ProjectDataTypes.h"
#include "viz/construction/ConstructionPlane.h"

#include <QString>

#include <vector>

#include <vtkActor.h>
#include <vtkSmartPointer.h>

class vtkPolyData;
class vtkPolyDataMapper;

namespace Structura::Construction {

/// Posicoes e textos dos rotulos dos eixos de referencia (nas duas pontas).
struct AxisLabels {
    std::vector<Vec3> positions;
    std::vector<QString> texts;
};

/**
 * @brief Eixos de referencia nomeados desenhados em tracejado traco-ponto.
 *
 * Todos os eixos do plano ativo sao agrupados em um unico ator.
 */
class ReferenceAxesRenderer
{
public:
    using Axes = Structura::App::ProjectData::ReferenceAxes;

    static constexpr double kDashLength = 0.55;
    static constexpr double kDotLength = 0.08;
    static constexpr double kGapLength = 0.20;
    static constexpr double kExtension = 5.0;

    ReferenceAxesRenderer();
    ~ReferenceAxesRenderer();

    [[nodiscard]] vtkActor *actor() const { return m_actor; }

    void update(const Axes &axes, const Window &gridBounds, PlaneMode mode, double offset);
    [[nodiscard]] const AxisLabels &labels() const noexcept { return m_labels; }

    void setVisible(bool visible);

private:
    struct LineSpec {
        double u0, v0, u1, v1;
        QString label;
        double labelOffsetU, labelOffsetV;
    };

    std::vector<LineSpec> lineSpecs(const Axes &axes, const Window &bounds, PlaneMode mode, double offset) const;

    vtkSmartPointer<vtkPolyData> m_data;
    vtkSmartPointer<vtkPolyDataMapper> m_mapper;
    vtkSmartPointer<vtkActor> m_actor;
    AxisLabels m_labels;
};

} // namespace Structura::Construction
