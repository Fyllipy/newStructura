#pragma once

#include "PostProcessorDataTypes.h"
#include "DiagramScaleManager.h"
#include "DiagramGeometryCache.h"

#include <vtkSmartPointer.h>

#include <array>
#include <vector>

class vtkPolyData;
class vtkActor;
class vtkRenderer;

namespace Structura::Visualization {

class InternalForceEvaluator;

/**
 * @brief Constrói malhas VTK para diagramas de esforços internos.
 *
 * Para cada elemento:
 * 1. Amostra o avaliador adaptivamente (min 80 amostras ou 1/0.10 m).
 * 2. Armazena os valores brutos no cache.
 * 3. Constrói geometria VTK com offsets físicos (physicalOffset).
 * 4. Detecta cruzamentos de zero e insere vértice exato.
 * 5. Gera malha positiva e negativa separadas (superfícies trianguladas).
 * 6. Gera polyline de contorno independente.
 * 7. Aplica vtkPolyDataNormals para iluminação correta.
 *
 * NÃO usa vtkSmoothPolyDataFilter — a fidelidade matemática dos diagramas
 * é preservada. Qualidade visual é controlada apenas pela densidade de amostras.
 *
 * Para torção (Mt): gera faixa helicoidal com ângulo limitado a
 * VisualizationTheme::maxTwistAngleDeg por unidade normalizada.
 */
class DiagramMeshBuilder {
public:
    struct ElementMesh {
        vtkSmartPointer<vtkPolyData> positiveSurface; ///< Região positiva (azul)
        vtkSmartPointer<vtkPolyData> negativeSurface; ///< Região negativa (vermelho)
        vtkSmartPointer<vtkPolyData> outline;          ///< Contorno do diagrama
    };

    DiagramMeshBuilder() = default;

    /**
     * @brief Amostra o elemento e armazena no cache.
     * Deve ser chamado antes de buildMesh.
     */
    void sampleElement(const BeamElementResults& element,
                       DiagramType type,
                       int analysisRevision,
                       DiagramGeometryCache& cache);

    /**
     * @brief Constrói a malha VTK a partir dos dados amostrados no cache.
     * Pode ser chamado novamente com nova escala sem re-amostrar.
     */
    ElementMesh buildMesh(const SampledDiagram& sampled,
                          const DiagramScaleManager& scale) const;

    ElementMesh buildMesh(const SampledDiagram& sampled,
                          const DiagramScaleManager& scale,
                          DiagramType type) const;

private:
    // ── Geometria de diagramas comuns (N, Vy, Vz, My, Mz) ──────────────────
    ElementMesh buildStandardMesh(const SampledDiagram& sampled,
                                  const DiagramScaleManager& scale) const;

    // ── Geometria de torção (Mt) ────────────────────────────────────────────
    ElementMesh buildTorsionMesh(const SampledDiagram& sampled,
                                 const DiagramScaleManager& scale) const;

    // ── Helpers ─────────────────────────────────────────────────────────────
    static std::array<double, 3> addScaled(const std::array<double, 3>& base,
                                           const std::array<double, 3>& axis,
                                           double scale);

    static int computeSampleCount(double length);

    /// Extrai o valor relevante de InternalForces conforme o tipo de diagrama.
    static double extractValue(const InternalForces& f, DiagramType type);

    /// Determina o eixo perpendicular de deslocamento para cada tipo.
    static const std::array<double, 3>& selectPerpAxis(
        const BeamElementResults& el, DiagramType type);
};

} // namespace Structura::Visualization
