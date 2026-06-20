#include "PostProcessorRenderer.h"
#include "VisualizationTheme.h"
#include "UniformLoadForceEvaluator.h"

#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkPolyData.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace Structura::Visualization {

// ── Destrutor ────────────────────────────────────────────────────────────────

PostProcessorRenderer::~PostProcessorRenderer()
{
    clearAll();
}

// ── Inicialização ─────────────────────────────────────────────────────────────

void PostProcessorRenderer::initialize(vtkRenderer* renderer)
{
    m_renderer = renderer;
    m_labelManager.initialize(renderer);
    m_deformedShape.initialize(renderer);
}

// ── setResults ────────────────────────────────────────────────────────────────

void PostProcessorRenderer::setResults(const std::vector<BeamElementResults>& results,
                                        int analysisRevision)
{
    // Limpa cena e cache ao receber nova análise
    clearAll();
    m_cache.invalidateAll();

    m_results = results;
    m_analysisRevision = analysisRevision;

    // Comprimento de referência = média dos comprimentos das barras
    if (!results.empty()) {
        double totalLen = 0.0;
        for (const auto& el : results) {
            const double dx = el.node2[0] - el.node1[0];
            const double dy = el.node2[1] - el.node1[1];
            const double dz = el.node2[2] - el.node1[2];
            totalLen += std::sqrt(dx*dx + dy*dy + dz*dz);
        }
        m_scaleManager.setReferenceLength(totalLen / static_cast<double>(results.size()));
    }
}

// ── Controle de modo (Ribbon) ─────────────────────────────────────────────────

void PostProcessorRenderer::activate()
{
    m_active = true;
    if (m_currentDiagram != DiagramType::None) {
        rebuildDiagram(m_currentDiagram);
    }
}

void PostProcessorRenderer::deactivate()
{
    m_active = false;
    removeActorsFromRenderer();
    m_labelManager.clearAll();
}

// ── showDiagram ───────────────────────────────────────────────────────────────

void PostProcessorRenderer::showDiagram(DiagramType type)
{
    if (type == DiagramType::None) {
        hideDiagram();
        return;
    }

    m_currentDiagram = type;
    if (m_active) {
        rebuildDiagram(type);
    }
}

void PostProcessorRenderer::hideDiagram()
{
    removeActorsFromRenderer();
    m_labelManager.clearAll();
    m_currentDiagram = DiagramType::None;
}

// ── Escala ────────────────────────────────────────────────────────────────────

void PostProcessorRenderer::setUserScale(double scale)
{
    m_scaleManager.setUserScale(scale);
    if (m_active && m_currentDiagram != DiagramType::None) {
        rebuildDiagram(m_currentDiagram);
    }
}

void PostProcessorRenderer::setScaleMode(DiagramScaleMode mode)
{
    m_scaleManager.setMode(mode);
    if (m_active && m_currentDiagram != DiagramType::None) {
        rebuildDiagram(m_currentDiagram);
    }
}

void PostProcessorRenderer::setActiveElementIds(const std::vector<int>& ids)
{
    m_scaleManager.setActiveElementIds(ids);
}

// ── clearAll ──────────────────────────────────────────────────────────────────

void PostProcessorRenderer::clearAll()
{
    removeActorsFromRenderer();
    m_posActors.clear();
    m_negActors.clear();
    m_outlineActors.clear();
    m_labelManager.clearAll();
}

// ── rebuildDiagram ────────────────────────────────────────────────────────────

void PostProcessorRenderer::rebuildDiagram(DiagramType type)
{
    // Remove atores antigos
    removeActorsFromRenderer();
    m_posActors.clear();
    m_negActors.clear();
    m_outlineActors.clear();

    if (m_results.empty() || !m_renderer) {
        return;
    }

    // Fase 1: Amostrar todos os elementos (usa cache — pula se já amostrado)
    for (const auto& el : m_results) {
        m_meshBuilder.sampleElement(el, type, m_analysisRevision, m_cache);
    }

    // Fase 2: Calcular maxAbs para a escala
    m_scaleManager.compute(m_cache, m_results, type, m_analysisRevision);

    // Fase 3: Construir geometria e criar atores
    for (const auto& el : m_results) {
        const DiagramCacheKey key{el.externalId, type, m_analysisRevision};
        if (!m_cache.has(key)) continue;

        const auto& sampled = m_cache.get(key);
        buildActorsForElement(el, type, sampled);
    }

    // Fase 4: Adicionar atores ao renderer
    addActorsToRenderer();

    // Fase 5: Labels MAX/MIN
    rebuildLabels();

    if (m_renderer) {
        m_renderer->GetRenderWindow()->Render();
    }
}

// ── buildActorsForElement ─────────────────────────────────────────────────────

void PostProcessorRenderer::buildActorsForElement(const BeamElementResults& /*el*/,
                                                   DiagramType type,
                                                   const SampledDiagram& sampled)
{
    const auto mesh = m_meshBuilder.buildMesh(sampled, m_scaleManager, type);

    const double posR = VisualizationTheme::posColorR;
    const double posG = VisualizationTheme::posColorG;
    const double posB = VisualizationTheme::posColorB;
    const double negR = VisualizationTheme::negColorR;
    const double negG = VisualizationTheme::negColorG;
    const double negB = VisualizationTheme::negColorB;
    const double alpha = VisualizationTheme::diagramOpacity;

    if (mesh.positiveSurface && mesh.positiveSurface->GetNumberOfCells() > 0) {
        m_posActors.push_back(makeActor(mesh.positiveSurface, posR, posG, posB, alpha));
    }
    if (mesh.negativeSurface && mesh.negativeSurface->GetNumberOfCells() > 0) {
        m_negActors.push_back(makeActor(mesh.negativeSurface, negR, negG, negB, alpha));
    }
    if (mesh.outline && mesh.outline->GetNumberOfCells() > 0) {
        m_outlineActors.push_back(makeActor(mesh.outline,
                                            0.9, 0.9, 0.95,
                                            VisualizationTheme::outlineOpacity,
                                            true));
    }
}

// ── rebuildLabels ─────────────────────────────────────────────────────────────

void PostProcessorRenderer::rebuildLabels()
{
    const auto extrema = m_extremaFinder.find(
        m_cache, m_results, m_scaleManager,
        m_currentDiagram, m_analysisRevision,
        1 /* maxCount: apenas global MAX e MIN */);

    m_labelManager.update(extrema);
}

// ── makeActor ────────────────────────────────────────────────────────────────

vtkSmartPointer<vtkActor> PostProcessorRenderer::makeActor(
    vtkSmartPointer<vtkPolyData> data,
    double r, double g, double b,
    double opacity,
    bool isWireframe) const
{
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(data);
    mapper->ScalarVisibilityOff();

    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);

    auto* prop = actor->GetProperty();
    prop->SetColor(r, g, b);
    prop->SetOpacity(opacity);
    prop->SetAmbient(VisualizationTheme::ambient);
    prop->SetDiffuse(VisualizationTheme::diffuse);
    prop->SetSpecular(VisualizationTheme::specular);

    if (isWireframe) {
        prop->SetRepresentationToWireframe();
        prop->SetLineWidth(static_cast<float>(VisualizationTheme::outlineWidth));
    } else {
        prop->SetRepresentationToSurface();
        prop->BackfaceCullingOff();
    }

    return vtkSmartPointer<vtkActor>(actor);
}

// ── Gestão de atores ──────────────────────────────────────────────────────────

void PostProcessorRenderer::addActorsToRenderer()
{
    if (!m_renderer) return;
    for (auto& a : m_posActors)     m_renderer->AddActor(a);
    for (auto& a : m_negActors)     m_renderer->AddActor(a);
    for (auto& a : m_outlineActors) m_renderer->AddActor(a);
}

void PostProcessorRenderer::removeActorsFromRenderer()
{
    if (!m_renderer) return;
    for (auto& a : m_posActors)     m_renderer->RemoveActor(a);
    for (auto& a : m_negActors)     m_renderer->RemoveActor(a);
    for (auto& a : m_outlineActors) m_renderer->RemoveActor(a);
}

} // namespace Structura::Visualization
