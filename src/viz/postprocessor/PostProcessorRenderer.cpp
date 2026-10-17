#include "PostProcessorRenderer.h"
#include "VisualizationTheme.h"
#include "UniformLoadForceEvaluator.h"

#include <QVector4D>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkPolyData.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <iomanip>

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
    m_hatchActors.clear();
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
    m_hatchActors.clear();

    if (m_results.empty() || !m_renderer) {
        return;
    }

    // Fase 1: Amostrar todos os elementos (usa cache — pula se já amostrado)
    for (const auto& el : m_results) {
        m_meshBuilder.sampleElement(el, type, m_analysisRevision, m_cache);
    }

    // Fase 2: Calcular maxAbs para a escala
    m_scaleManager.compute(m_cache, m_results, type, m_analysisRevision);

    // Fase 3: Construir a geometria (uma malha por categoria) e criar os atores
    buildActors(type);

    // Fase 4: Adicionar atores ao renderer
    addActorsToRenderer();

    // Fase 5: Labels MAX/MIN
    rebuildLabels();

    if (m_renderer) {
        m_renderer->GetRenderWindow()->Render();
    }
}

// ── buildActorsForElement ─────────────────────────────────────────────────────

void PostProcessorRenderer::buildActors(DiagramType type)
{
    DiagramMeshBuilder::MeshAccumulator mesh;
    for (const auto& el : m_results) {
        const DiagramCacheKey key{el.externalId, type, m_analysisRevision};
        if (m_cache.has(key)) {
            m_meshBuilder.append(m_cache.get(key), m_scaleManager, type, m_hatchVisible, mesh);
        }
    }
    const double fade = m_stale ? 0.30 : 1.0;
    const double alpha = VisualizationTheme::diagramOpacity * fade;
    if (const auto positive = mesh.positive(); positive && positive->GetNumberOfCells() > 0) {
        m_posActors.push_back(makeActor(positive, VisualizationTheme::posColorR, VisualizationTheme::posColorG, VisualizationTheme::posColorB, alpha));
    }
    if (const auto negative = mesh.negative(); negative && negative->GetNumberOfCells() > 0) {
        m_negActors.push_back(makeActor(negative, VisualizationTheme::negColorR, VisualizationTheme::negColorG, VisualizationTheme::negColorB, alpha));
    }
    if (const auto hatch = mesh.hatch(); m_hatchVisible && hatch && hatch->GetNumberOfCells() > 0) {
        m_hatchActors.push_back(makeActor(hatch, 0.25, 0.32, 0.45, VisualizationTheme::hatchOpacity * fade, true));
        m_hatchActors.back()->GetProperty()->SetLineWidth(1.0f);
    }
    if (const auto outline = mesh.outline(); outline && outline->GetNumberOfCells() > 0) {
        m_outlineActors.push_back(makeActor(outline, 0.10, 0.14, 0.24, VisualizationTheme::outlineOpacity * fade, true));
    }
}

// ── rebuildLabels ─────────────────────────────────────────────────────────────

namespace {

/// O diagrama de My e Mz e desenhado com o sinal trocado (lado tracionado); o valor fisico e o oposto.
double physicalValue(double raw, DiagramType type)
{
    return (type == DiagramType::My || type == DiagramType::Mz) ? -raw : raw;
}

std::string formatValue(double v, const QString &unit)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(std::abs(v) >= 100.0 ? 1 : 2);
    if (v > 0.0) {
        ss << "+";
    }
    ss << v << " " << unit.toStdString();
    return ss.str();
}

} // namespace

QString PostProcessorRenderer::unitOf(DiagramType type)
{
    switch (type) {
    case DiagramType::N:
    case DiagramType::Vy:
    case DiagramType::Vz: return QStringLiteral("kN");
    case DiagramType::Mt:
    case DiagramType::My:
    case DiagramType::Mz: return QStringLiteral("kN·m");
    default: return QString();
    }
}

QString PostProcessorRenderer::nameOf(DiagramType type)
{
    switch (type) {
    case DiagramType::N: return QStringLiteral("N (esforço normal)");
    case DiagramType::Vy: return QStringLiteral("Vy (cortante em y)");
    case DiagramType::Vz: return QStringLiteral("Vz (cortante em z)");
    case DiagramType::Mt: return QStringLiteral("Mt (torção)");
    case DiagramType::My: return QStringLiteral("My (fletor em torno de y)");
    case DiagramType::Mz: return QStringLiteral("Mz (fletor em torno de z)");
    default: return QString();
    }
}

void PostProcessorRenderer::setLabelMode(LabelMode mode)
{
    m_labelMode = mode;
    if (m_active && m_currentDiagram != DiagramType::None) {
        rebuildLabels();
        if (m_renderer) {
            m_renderer->GetRenderWindow()->Render();
        }
    }
}

void PostProcessorRenderer::setStale(bool stale)
{
    if (m_stale == stale) {
        return;
    }
    m_stale = stale;
    if (m_active && m_currentDiagram != DiagramType::None) {
        rebuildDiagram(m_currentDiagram);
    }
}

void PostProcessorRenderer::setHatchVisible(bool visible)
{
    m_hatchVisible = visible;
    if (m_active && m_currentDiagram != DiagramType::None) {
        rebuildDiagram(m_currentDiagram);
    }
}

PostProcessorRenderer::Summary PostProcessorRenderer::summary() const
{
    Summary s;
    if (m_currentDiagram == DiagramType::None) {
        return s;
    }
    s.type = m_currentDiagram;
    s.unit = unitOf(m_currentDiagram);
    s.name = nameOf(m_currentDiagram);
    s.metersPerUnit = m_scaleManager.physicalOffset(1.0);
    bool first = true;
    for (const auto& el : m_results) {
        const DiagramCacheKey key{el.externalId, m_currentDiagram, m_analysisRevision};
        if (!m_cache.has(key)) {
            continue;
        }
        for (const double raw : m_cache.get(key).rawValues) {
            const double v = physicalValue(raw, m_currentDiagram);
            if (first || v > s.maxValue) {
                s.maxValue = v;
                s.maxElement = el.externalId;
            }
            if (first || v < s.minValue) {
                s.minValue = v;
                s.minElement = el.externalId;
            }
            first = false;
        }
    }
    s.valid = !first;
    return s;
}

void PostProcessorRenderer::rebuildLabels()
{
    m_labelManager.clearAll();
    if (m_labelMode == LabelMode::None || m_currentDiagram == DiagramType::None) {
        return;
    }
    const QString unit = unitOf(m_currentDiagram);
    std::vector<DiagramLabelManager::Label> labels;
    auto tipOf = [&](const SampledDiagram& sampled, std::size_t i) {
        const double offset = m_scaleManager.physicalOffset(sampled.rawValues[i]);
        return std::array<double, 3>{sampled.basePoints[i][0] + sampled.perpAxis[0] * offset,
                                     sampled.basePoints[i][1] + sampled.perpAxis[1] * offset,
                                     sampled.basePoints[i][2] + sampled.perpAxis[2] * offset};
    };
    if (m_labelMode == LabelMode::Global) {
        const auto extrema = m_extremaFinder.find(m_cache, m_results, m_scaleManager, m_currentDiagram, m_analysisRevision, 1);
        // os extremos desenhados correspondem aos fisicos, com a ordem trocada em My/Mz
        const bool flip = m_currentDiagram == DiagramType::My || m_currentDiagram == DiagramType::Mz;
        for (const auto& pt : extrema.maxima) {
            labels.push_back({pt.worldPosition, (flip ? "MIN: " : "MAX: ") + formatValue(physicalValue(pt.value, m_currentDiagram), unit), flip ? 1 : 0});
        }
        for (const auto& pt : extrema.minima) {
            labels.push_back({pt.worldPosition, (flip ? "MAX: " : "MIN: ") + formatValue(physicalValue(pt.value, m_currentDiagram), unit), flip ? 0 : 1});
        }
        m_labelManager.setLabels(labels);
        return;
    }
    // por barra: valor de pico e, se houver, os valores das extremidades
    const double threshold = 0.02 * m_scaleManager.maxAbs();
    constexpr std::size_t kMaxLabels = 400;
    for (const auto& el : m_results) {
        const DiagramCacheKey key{el.externalId, m_currentDiagram, m_analysisRevision};
        if (!m_cache.has(key)) {
            continue;
        }
        const auto& sampled = m_cache.get(key);
        const std::size_t n = sampled.rawValues.size();
        if (n < 2) {
            continue;
        }
        std::size_t peak = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (std::abs(sampled.rawValues[i]) > std::abs(sampled.rawValues[peak])) {
                peak = i;
            }
        }
        std::vector<std::size_t> picks;
        auto add = [&](std::size_t i) {
            if (std::abs(sampled.rawValues[i]) >= threshold && std::find(picks.begin(), picks.end(), i) == picks.end()) {
                picks.push_back(i);
            }
        };
        const bool peakAtEnd = peak < 3 || peak + 3 >= n;
        add(peakAtEnd ? (peak < 3 ? 0 : n - 1) : peak);
        if (!peakAtEnd) {
            if (std::abs(sampled.rawValues[0]) > 0.3 * std::abs(sampled.rawValues[peak])) add(0);
            if (std::abs(sampled.rawValues[n - 1]) > 0.3 * std::abs(sampled.rawValues[peak])) add(n - 1);
        } else {
            add(peak < 3 ? n - 1 : 0);
        }
        for (const std::size_t i : picks) {
            const double v = physicalValue(sampled.rawValues[i], m_currentDiagram);
            labels.push_back({tipOf(sampled, i), formatValue(v, unit), 2});
        }
        if (labels.size() >= kMaxLabels) {
            break;
        }
    }
    m_labelManager.setLabels(labels);
}

std::optional<PostProcessorRenderer::ProbeHit> PostProcessorRenderer::probe(const QMatrix4x4 &composite, const QSizeF &viewport,
                                                                           const QPointF &cursor, double tolerancePx) const
{
    if (!m_active || m_currentDiagram == DiagramType::None || viewport.width() <= 0.0 || viewport.height() <= 0.0) {
        return std::nullopt;
    }
    auto project = [&](const std::array<double, 3>& p, QPointF& out) {
        const QVector4D clip = composite * QVector4D(static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2]), 1.0f);
        if (clip.w() <= 1e-9f) {
            return false;
        }
        out = QPointF((clip.x() / clip.w() + 1.0) * viewport.width() / 2.0, (1.0 - clip.y() / clip.w()) * viewport.height() / 2.0);
        return true;
    };
    auto distanceToSegment = [&](const QPointF& a, const QPointF& b) {
        const QPointF ab = b - a;
        const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
        double t = len2 > 1e-12 ? ((cursor.x() - a.x()) * ab.x() + (cursor.y() - a.y()) * ab.y()) / len2 : 0.0;
        t = std::clamp(t, 0.0, 1.0);
        const QPointF c = a + ab * t;
        return std::hypot(cursor.x() - c.x(), cursor.y() - c.y());
    };
    double best = tolerancePx;
    const BeamElementResults* bestElement = nullptr;
    std::size_t bestIndex = 0;
    for (const auto& el : m_results) {
        const DiagramCacheKey key{el.externalId, m_currentDiagram, m_analysisRevision};
        if (!m_cache.has(key)) {
            continue;
        }
        const auto& sampled = m_cache.get(key);
        const std::size_t n = sampled.rawValues.size();
        const std::size_t step = std::max<std::size_t>(1, n / 80);
        for (std::size_t i = 0; i < n; i += step) {
            QPointF base, tip;
            const double offset = m_scaleManager.physicalOffset(sampled.rawValues[i]);
            const std::array<double, 3> curve{sampled.basePoints[i][0] + sampled.perpAxis[0] * offset,
                                              sampled.basePoints[i][1] + sampled.perpAxis[1] * offset,
                                              sampled.basePoints[i][2] + sampled.perpAxis[2] * offset};
            if (!project(sampled.basePoints[i], base) || !project(curve, tip)) {
                continue;
            }
            const double d = distanceToSegment(base, tip);
            if (d < best) {
                best = d;
                bestElement = &el;
                bestIndex = i;
            }
        }
    }
    if (!bestElement) {
        return std::nullopt;
    }
    // refinamento: amostra de maior proximidade na vizinhanca
    const auto& sampled = m_cache.get({bestElement->externalId, m_currentDiagram, m_analysisRevision});
    const std::size_t n = sampled.rawValues.size();
    const std::size_t step = std::max<std::size_t>(1, n / 80);
    const std::size_t lo = bestIndex > step ? bestIndex - step : 0, hi = std::min(n - 1, bestIndex + step);
    double bestFine = 1e300;
    for (std::size_t i = lo; i <= hi; ++i) {
        QPointF base, tip;
        const double offset = m_scaleManager.physicalOffset(sampled.rawValues[i]);
        const std::array<double, 3> curve{sampled.basePoints[i][0] + sampled.perpAxis[0] * offset,
                                          sampled.basePoints[i][1] + sampled.perpAxis[1] * offset,
                                          sampled.basePoints[i][2] + sampled.perpAxis[2] * offset};
        if (project(sampled.basePoints[i], base) && project(curve, tip)) {
            const double d = distanceToSegment(base, tip);
            if (d < bestFine) {
                bestFine = d;
                bestIndex = i;
            }
        }
    }
    ProbeHit hit;
    hit.elementId = bestElement->externalId;
    hit.length = sampled.elementLength;
    hit.x = n > 1 ? static_cast<double>(bestIndex) * sampled.elementLength / static_cast<double>(n - 1) : 0.0;
    const double offset = m_scaleManager.physicalOffset(sampled.rawValues[bestIndex]);
    hit.curvePoint = {sampled.basePoints[bestIndex][0] + sampled.perpAxis[0] * offset,
                      sampled.basePoints[bestIndex][1] + sampled.perpAxis[1] * offset,
                      sampled.basePoints[bestIndex][2] + sampled.perpAxis[2] * offset};
    hit.value = physicalValue(sampled.rawValues[bestIndex], m_currentDiagram);
    hit.forces = UniformLoadForceEvaluator(*bestElement).evaluate(hit.x);
    hit.end1 = bestElement->node1;
    hit.end2 = bestElement->node2;
    return hit;
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
    for (auto& a : m_hatchActors)   m_renderer->AddActor(a);
    for (auto& a : m_outlineActors) m_renderer->AddActor(a);
}

void PostProcessorRenderer::removeActorsFromRenderer()
{
    if (!m_renderer) return;
    for (auto& a : m_posActors)     m_renderer->RemoveActor(a);
    for (auto& a : m_negActors)     m_renderer->RemoveActor(a);
    for (auto& a : m_hatchActors)   m_renderer->RemoveActor(a);
    for (auto& a : m_outlineActors) m_renderer->RemoveActor(a);
}

} // namespace Structura::Visualization
