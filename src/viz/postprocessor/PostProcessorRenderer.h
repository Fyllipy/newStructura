#pragma once

#include "PostProcessorDataTypes.h"
#include "DiagramScaleManager.h"
#include "DiagramMeshBuilder.h"
#include "DiagramExtremaFinder.h"
#include "DiagramLabelManager.h"
#include "DiagramGeometryCache.h"
#include "DeformedShapeRenderer.h"
#include "SupportGlyphFactory.h"

#include <vtkSmartPointer.h>
#include <QMatrix4x4>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <optional>

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

    // ── Valores e hachura ────────────────────────────────────────────────────
    /// Rotulos de valor: nenhum, extremos globais ou maximo de cada barra.
    enum class LabelMode { None, Global, PerMember };
    void setLabelMode(LabelMode mode);
    [[nodiscard]] LabelMode labelMode() const { return m_labelMode; }
    void setHatchVisible(bool visible);
    /// Resultados desatualizados (modelo mudou depois da analise): o diagrama fica esmaecido.
    void setStale(bool stale);
    [[nodiscard]] bool isStale() const { return m_stale; }
    [[nodiscard]] bool hatchVisible() const { return m_hatchVisible; }

    /// Resumo do diagrama atual (valores fisicos, com o sinal do esforco).
    struct Summary {
        bool valid {false};
        DiagramType type {DiagramType::None};
        double maxValue {0.0}, minValue {0.0};
        int maxElement {0}, minElement {0};
        double metersPerUnit {0.0};   ///< comprimento desenhado (m) por unidade do esforco
        QString unit;
        QString name;
    };
    [[nodiscard]] Summary summary() const;
    [[nodiscard]] static QString unitOf(DiagramType type);
    [[nodiscard]] static QString nameOf(DiagramType type);

    /// Leitura do diagrama sob o cursor (coordenadas de widget; `composite` = projecao x visao da camera).
    struct ProbeHit {
        int elementId {0};
        double x {0.0};          ///< m, medido a partir do no inicial
        double length {0.0};
        std::array<double, 3> curvePoint {};   ///< ponto da curva do diagrama (mundo)
        double value {0.0};      ///< valor fisico do diagrama atual
        InternalForces forces;   ///< todos os esforcos nesta secao
        std::array<double, 3> end1 {}, end2 {};
    };
    [[nodiscard]] std::optional<ProbeHit> probe(const QMatrix4x4 &composite, const QSizeF &viewport, const QPointF &cursor, double tolerancePx) const;

    // ── Limpeza ──────────────────────────────────────────────────────────────
    void clearAll();

private:
    // ── Reconstrução de diagrama ─────────────────────────────────────────────
    void rebuildDiagram(DiagramType type);
    void buildActors(DiagramType type);
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
    LabelMode                          m_labelMode{LabelMode::Global};
    bool                               m_hatchVisible{true};
    bool                               m_stale{false};
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
    std::vector<vtkSmartPointer<vtkActor>> m_hatchActors;   ///< Hachura (ordenadas)
};

} // namespace Structura::Visualization
