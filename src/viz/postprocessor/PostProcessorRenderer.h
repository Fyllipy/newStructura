#pragma once

#include "PostProcessorDataTypes.h"
#include "DiagramScaleManager.h"
#include "DiagramMeshBuilder.h"
#include "DiagramExtremaFinder.h"
#include "DiagramLabelManager.h"
#include "DiagramGeometryCache.h"
#include "DeformedShapeRenderer.h"
#include "SupportGlyphFactory.h"
#include "LoadGlyphFactory.h"

#include <vtkSmartPointer.h>

#include <vector>
#include <memory>

class vtkRenderer;
class vtkActor;
class vtkPolyDataMapper;

namespace Structura::Visualization {

/**
 * @brief Orquestrador do pós-processador estrutural.
 *
 * Responsabilidades:
 * - Receber resultados do solver (setResults)
 * - Construir e gerenciar atores VTK dos diagramas
 * - Gerenciar labels de extremos
 * - Delegar ao DiagramMeshBuilder, DiagramScaleManager, etc.
 *
 * Separação de responsabilidades da UI:
 * - Ribbon:     ativa/desativa o modo de pós-processamento (activate/deactivate)
 * - Footer bar: seleciona diagrama, escala, modo de escala (showDiagram, setUserScale, setScaleMode)
 */
class PostProcessorRenderer {
public:
    PostProcessorRenderer() = default;
    ~PostProcessorRenderer();

    // ── Inicialização ────────────────────────────────────────────────────────
    void initialize(vtkRenderer* renderer);

    // ── Alimentação de resultados ────────────────────────────────────────────
    /**
     * @brief Define os resultados da análise.
     * Invalida o cache e recalcula o comprimento de referência.
     * @param results         Vetor de resultados por elemento.
     * @param analysisRevision Incrementado a cada nova análise.
     */
    void setResults(const std::vector<BeamElementResults>& results,
                    int analysisRevision);

    // ── Controle de modo (Ribbon) ────────────────────────────────────────────
    void activate();
    void deactivate();
    bool isActive() const { return m_active; }

    // ── Controle de diagrama (Footer bar) ────────────────────────────────────
    void showDiagram(DiagramType type);
    void hideDiagram();
    DiagramType currentDiagram() const { return m_currentDiagram; }

    // ── Escala (Footer bar) ──────────────────────────────────────────────────
    /**
     * @brief Define o fator de escala do usuário (slider).
     * Reconstrói a geometria a partir do cache — sem re-avaliar a física.
     * @param scale Fator multiplicativo (0.1 … 5.0).
     */
    void setUserScale(double scale);
    void setScaleMode(DiagramScaleMode mode);
    void setActiveElementIds(const std::vector<int>& ids);

    // ── Limpeza ──────────────────────────────────────────────────────────────
    void clearAll();

private:
    // ── Reconstrução de diagrama ─────────────────────────────────────────────
    void rebuildDiagram(DiagramType type);
    void buildActorsForElement(const BeamElementResults& el,
                               DiagramType type,
                               const SampledDiagram& sampled);
    void rebuildLabels();

    // ── Gestão de atores VTK ─────────────────────────────────────────────────
    vtkSmartPointer<vtkActor> makeActor(vtkSmartPointer<vtkPolyData> data,
                                         double r, double g, double b,
                                         double opacity,
                                         bool isWireframe = false) const;

    void addActorsToRenderer();
    void removeActorsFromRenderer();

    // ── Estado ──────────────────────────────────────────────────────────────
    vtkRenderer*                       m_renderer{nullptr};
    std::vector<BeamElementResults>    m_results;
    int                                m_analysisRevision{0};
    DiagramType                        m_currentDiagram{DiagramType::None};
    bool                               m_active{false};

    // ── Componentes ──────────────────────────────────────────────────────────
    DiagramScaleManager                m_scaleManager;
    DiagramMeshBuilder                 m_meshBuilder;
    DiagramExtremaFinder               m_extremaFinder;
    DiagramLabelManager                m_labelManager;
    DiagramGeometryCache               m_cache;
    DeformedShapeRenderer              m_deformedShape;

    // ── Atores VTK ───────────────────────────────────────────────────────────
    std::vector<vtkSmartPointer<vtkActor>> m_posActors;     ///< Superfícies positivas (azul)
    std::vector<vtkSmartPointer<vtkActor>> m_negActors;     ///< Superfícies negativas (vermelho)
    std::vector<vtkSmartPointer<vtkActor>> m_outlineActors; ///< Contornos
};

} // namespace Structura::Visualization
