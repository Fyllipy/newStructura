#pragma once

#include "PostProcessorDataTypes.h"
#include "DiagramScaleManager.h"
#include "DiagramGeometryCache.h"
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <array>
#include <vector>
class vtkPolyData;
class vtkPoints;
class vtkCellArray;

namespace Structura::Visualization {

/**
 * @brief Constrói a geometria VTK dos diagramas de esforços internos.
 *
 * Todas as barras acumulam nas mesmas quatro malhas (superfície positiva, negativa, contorno e hachura), de modo que
 * o diagrama inteiro vira 4 atores — o custo de renderização não cresce com o número de barras.
 * Sem filtro de normais (iluminação plana calculada pelo shader).
 *
 * Amostragem: o esforço é polinomial de grau <= 2 ao longo da barra (cargas uniformes), então poucas amostras bastam;
 * ver computeSampleCount().
 */
class DiagramMeshBuilder {
public:
    /// Acumulador compartilhado por todos os elementos.
    class MeshAccumulator {
    public:
        MeshAccumulator();
        [[nodiscard]] vtkSmartPointer<vtkPolyData> positive() const;
        [[nodiscard]] vtkSmartPointer<vtkPolyData> negative() const;
        [[nodiscard]] vtkSmartPointer<vtkPolyData> outline() const;
        [[nodiscard]] vtkSmartPointer<vtkPolyData> hatch() const;

    private:
        friend class DiagramMeshBuilder;
        vtkSmartPointer<vtkPoints> m_posPoints, m_negPoints, m_outPoints, m_hatchPoints;
        vtkSmartPointer<vtkCellArray> m_posCells, m_negCells, m_outCells, m_hatchCells;
    };

    DiagramMeshBuilder() = default;

    /// Amostra o elemento e armazena no cache (nada é refeito se já estiver amostrado nesta revisão).
    void sampleElement(const BeamElementResults& element,
                       DiagramType type,
                       int analysisRevision,
                       DiagramGeometryCache& cache);

    /// Acrescenta a geometria do elemento ao acumulador (pode ser repetido com outra escala sem reamostrar).
    void append(const SampledDiagram& sampled,
                const DiagramScaleManager& scale,
                DiagramType type,
                bool withHatch,
                MeshAccumulator& out) const;

private:
    void appendStandard(const SampledDiagram& sampled, const DiagramScaleManager& scale, bool withHatch, MeshAccumulator& out) const;
    void appendTorsion(const SampledDiagram& sampled, const DiagramScaleManager& scale, MeshAccumulator& out) const;

    static std::array<double, 3> addScaled(const std::array<double, 3>& base,
                                           const std::array<double, 3>& axis,
                                           double scale);
    static int computeSampleCount(double length);
    static double extractValue(const InternalForces& f, DiagramType type);
    static const std::array<double, 3>& selectPerpAxis(const BeamElementResults& el, DiagramType type);
};

} // namespace Structura::Visualization
