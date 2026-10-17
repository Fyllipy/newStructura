#include "DiagramMeshBuilder.h"
#include "UniformLoadForceEvaluator.h"
#include "VisualizationTheme.h"

#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>

#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Structura::Visualization {

// ── Acumulador ────────────────────────────────────────────────────────────────

DiagramMeshBuilder::MeshAccumulator::MeshAccumulator()
    : m_posPoints(vtkSmartPointer<vtkPoints>::New())
    , m_negPoints(vtkSmartPointer<vtkPoints>::New())
    , m_outPoints(vtkSmartPointer<vtkPoints>::New())
    , m_hatchPoints(vtkSmartPointer<vtkPoints>::New())
    , m_posCells(vtkSmartPointer<vtkCellArray>::New())
    , m_negCells(vtkSmartPointer<vtkCellArray>::New())
    , m_outCells(vtkSmartPointer<vtkCellArray>::New())
    , m_hatchCells(vtkSmartPointer<vtkCellArray>::New())
{
}

namespace {

vtkSmartPointer<vtkPolyData> surface(vtkPoints* points, vtkCellArray* cells)
{
    if (points->GetNumberOfPoints() == 0) {
        return nullptr;
    }
    auto pd = vtkSmartPointer<vtkPolyData>::New();
    pd->SetPoints(points);
    pd->SetPolys(cells);
    return pd;
}

vtkSmartPointer<vtkPolyData> lines(vtkPoints* points, vtkCellArray* cells)
{
    if (points->GetNumberOfPoints() == 0) {
        return nullptr;
    }
    auto pd = vtkSmartPointer<vtkPolyData>::New();
    pd->SetPoints(points);
    pd->SetLines(cells);
    return pd;
}

} // namespace

vtkSmartPointer<vtkPolyData> DiagramMeshBuilder::MeshAccumulator::positive() const { return surface(m_posPoints, m_posCells); }
vtkSmartPointer<vtkPolyData> DiagramMeshBuilder::MeshAccumulator::negative() const { return surface(m_negPoints, m_negCells); }
vtkSmartPointer<vtkPolyData> DiagramMeshBuilder::MeshAccumulator::outline() const { return lines(m_outPoints, m_outCells); }
vtkSmartPointer<vtkPolyData> DiagramMeshBuilder::MeshAccumulator::hatch() const { return lines(m_hatchPoints, m_hatchCells); }

// ── Helpers estáticos ─────────────────────────────────────────────────────────

std::array<double, 3> DiagramMeshBuilder::addScaled(const std::array<double, 3>& base,
                                                      const std::array<double, 3>& axis,
                                                      double s)
{
    return {base[0] + axis[0]*s,
            base[1] + axis[1]*s,
            base[2] + axis[2]*s};
}

int DiagramMeshBuilder::computeSampleCount(double length)
{
    // Esforcos de cargas uniformes sao polinomios de grau <= 2: 4 amostras por metro (entre 24 e 120) reproduzem a curva.
    return std::clamp(static_cast<int>(std::ceil(length * VisualizationTheme::samplesPerMeter)), VisualizationTheme::minSamples, VisualizationTheme::maxSamples);
}

double DiagramMeshBuilder::extractValue(const InternalForces& f, DiagramType type)
{
    switch (type) {
    case DiagramType::N:  return f.N;
    case DiagramType::Vy: return f.Vy;
    case DiagramType::Vz: return f.Vz;
    case DiagramType::Mt: return f.Mt;
    // Inverte o momento para desenhar no lado da fibra tracionada
    case DiagramType::My: return -f.My;
    case DiagramType::Mz: return -f.Mz;
    default:              return 0.0;
    }
}

const std::array<double, 3>& DiagramMeshBuilder::selectPerpAxis(
    const BeamElementResults& el, DiagramType type)
{
    // N, Vy, Mz deslocam em yLocal; Vz, My deslocam em zLocal; Mt usa ambos
    switch (type) {
    case DiagramType::Vz:
    case DiagramType::My:
        return el.zLocal;
    default:
        return el.yLocal;
    }
}

// ── sampleElement ─────────────────────────────────────────────────────────────

void DiagramMeshBuilder::sampleElement(const BeamElementResults& element,
                                        DiagramType type,
                                        int analysisRevision,
                                        DiagramGeometryCache& cache)
{
    const DiagramCacheKey key{element.externalId, type, analysisRevision};
    if (cache.has(key)) {
        return; // já amostrado nesta revisão
    }

    UniformLoadForceEvaluator evaluator(element);
    const double L = evaluator.length();
    const int n = computeSampleCount(L);

    SampledDiagram sampled;
    sampled.elementLength = L;
    sampled.perpAxis = selectPerpAxis(element, type);
    sampled.secAxis  = element.zLocal; // usado para torção
    sampled.basePoints.reserve(static_cast<std::size_t>(n));
    sampled.rawValues.reserve(static_cast<std::size_t>(n));

    for (int i = 0; i < n; ++i) {
        const double x  = static_cast<double>(i) * L / static_cast<double>(n - 1);
        const InternalForces f = evaluator.evaluate(x);
        sampled.basePoints.push_back({
            element.node1[0] + element.xLocal[0] * x,
            element.node1[1] + element.xLocal[1] * x,
            element.node1[2] + element.xLocal[2] * x});
        sampled.rawValues.push_back(extractValue(f, type));
    }
    cache.store(key, std::move(sampled));
}

// ── append (despachante) ──────────────────────────────────────────────────────

void DiagramMeshBuilder::append(const SampledDiagram& sampled,
                                const DiagramScaleManager& scale,
                                DiagramType type,
                                bool withHatch,
                                MeshAccumulator& out) const
{
    if (type == DiagramType::Mt) {
        appendTorsion(sampled, scale, out);
    } else {
        appendStandard(sampled, scale, withHatch, out);
    }
}

namespace {

/// Quadrilatero (dois triangulos) entre as amostras p0-p1 e as pontas d0-d1.
void addQuad(vtkPoints* pts, vtkCellArray* cells,
             const std::array<double,3>& p0, const std::array<double,3>& p1,
             const std::array<double,3>& d0, const std::array<double,3>& d1)
{
    const vtkIdType i0 = pts->InsertNextPoint(p0.data());
    const vtkIdType i1 = pts->InsertNextPoint(p1.data());
    const vtkIdType i2 = pts->InsertNextPoint(d1.data());
    const vtkIdType i3 = pts->InsertNextPoint(d0.data());
    const vtkIdType triA[3] = {i0, i1, i2};
    const vtkIdType triB[3] = {i0, i2, i3};
    cells->InsertNextCell(3, triA);
    cells->InsertNextCell(3, triB);
}

} // namespace

// ── Diagramas comuns (N, Vy, Vz, My, Mz) ──────────────────────────────────────

void DiagramMeshBuilder::appendStandard(const SampledDiagram& sampled,
                                        const DiagramScaleManager& scale,
                                        bool withHatch,
                                        MeshAccumulator& out) const
{
    const std::size_t n = sampled.rawValues.size();
    if (n < 2) {
        return;
    }
    std::vector<std::array<double, 3>> diagPts(n);
    for (std::size_t i = 0; i < n; ++i) {
        diagPts[i] = addScaled(sampled.basePoints[i], sampled.perpAxis, scale.physicalOffset(sampled.rawValues[i]));
    }

    for (std::size_t i = 0; i + 1 < n; ++i) {
        const double v0 = sampled.rawValues[i];
        const double v1 = sampled.rawValues[i + 1];
        const auto& p0 = sampled.basePoints[i];
        const auto& p1 = sampled.basePoints[i + 1];
        const auto& d0 = diagPts[i];
        const auto& d1 = diagPts[i + 1];
        if (v0 * v1 >= 0.0) {
            if (v0 >= 0.0 && (v0 > 0.0 || v1 > 0.0)) {
                addQuad(out.m_posPoints, out.m_posCells, p0, p1, d0, d1);
            } else if (v0 < 0.0 || v1 < 0.0) {
                addQuad(out.m_negPoints, out.m_negCells, p0, p1, d0, d1);
            }
        } else {
            // cruzamento de zero: insere o ponto exato (offset nulo = sobre o eixo)
            const double t = std::abs(v0) / (std::abs(v0) + std::abs(v1));
            const std::array<double, 3> pMid{p0[0] + (p1[0] - p0[0]) * t, p0[1] + (p1[1] - p0[1]) * t, p0[2] + (p1[2] - p0[2]) * t};
            if (v0 >= 0.0) {
                addQuad(out.m_posPoints, out.m_posCells, p0, pMid, d0, pMid);
                addQuad(out.m_negPoints, out.m_negCells, pMid, p1, pMid, d1);
            } else {
                addQuad(out.m_negPoints, out.m_negCells, p0, pMid, d0, pMid);
                addQuad(out.m_posPoints, out.m_posCells, pMid, p1, pMid, d1);
            }
        }
    }

    // contorno: curva do diagrama e linha de base (eixo da barra)
    {
        std::vector<vtkIdType> ids(n), baseIds(n);
        for (std::size_t i = 0; i < n; ++i) {
            ids[i] = out.m_outPoints->InsertNextPoint(diagPts[i].data());
        }
        out.m_outCells->InsertNextCell(static_cast<vtkIdType>(n), ids.data());
        for (std::size_t i = 0; i < n; ++i) {
            baseIds[i] = out.m_outPoints->InsertNextPoint(sampled.basePoints[i].data());
        }
        out.m_outCells->InsertNextCell(static_cast<vtkIdType>(n), baseIds.data());
    }

    // hachura: ordenadas do eixo ate a curva
    if (withHatch) {
        const int count = std::clamp(static_cast<int>(std::ceil(sampled.elementLength / 0.25)), 6, 40);
        for (int k = 0; k <= count; ++k) {
            const std::size_t i = std::min(n - 1, static_cast<std::size_t>(std::llround(static_cast<double>(k) * static_cast<double>(n - 1) / count)));
            if (std::abs(sampled.rawValues[i]) < 1e-12) {
                continue;
            }
            const vtkIdType ids[2] = {out.m_hatchPoints->InsertNextPoint(sampled.basePoints[i].data()), out.m_hatchPoints->InsertNextPoint(diagPts[i].data())};
            out.m_hatchCells->InsertNextCell(2, ids);
        }
    }
}

// ── Torção (Mt): faixa helicoidal ─────────────────────────────────────────────

void DiagramMeshBuilder::appendTorsion(const SampledDiagram& sampled,
                                       const DiagramScaleManager& scale,
                                       MeshAccumulator& out) const
{
    const std::size_t n = sampled.rawValues.size();
    if (n < 2) {
        return;
    }
    const double maxTwist = VisualizationTheme::maxTwistAngleDeg * M_PI / 180.0;
    const double refAmplitude = scale.refLength()
                              * VisualizationTheme::defaultAmplitudeFraction
                              * VisualizationTheme::torsionBandWidthFraction
                              * scale.userScale();
    const double maxAbs = scale.maxAbs() < 1e-30 ? 1.0 : scale.maxAbs();
    const auto& yA = sampled.perpAxis;
    const auto& zA = sampled.secAxis;

    auto wing = [&](const std::array<double,3>& center, double theta) {
        const double cy = std::cos(theta) * refAmplitude;
        const double sz = std::sin(theta) * refAmplitude;
        std::array<std::array<double, 3>, 2> w;
        for (std::size_t k = 0; k < 3; ++k) {
            w[0][k] = center[k] + yA[k] * cy + zA[k] * sz;
            w[1][k] = center[k] - yA[k] * cy - zA[k] * sz;
        }
        return w;
    };

    for (std::size_t i = 0; i + 1 < n; ++i) {
        const double v0 = sampled.rawValues[i];
        const double v1 = sampled.rawValues[i + 1];
        const auto a = wing(sampled.basePoints[i], (v0 / maxAbs) * maxTwist);
        const auto b = wing(sampled.basePoints[i + 1], (v1 / maxAbs) * maxTwist);
        if (v0 >= 0.0) {
            addQuad(out.m_posPoints, out.m_posCells, a[0], b[0], a[1], b[1]);
        } else {
            addQuad(out.m_negPoints, out.m_negCells, a[0], b[0], a[1], b[1]);
        }
    }
    std::vector<vtkIdType> ids(n);
    for (std::size_t i = 0; i < n; ++i) {
        ids[i] = out.m_outPoints->InsertNextPoint(sampled.basePoints[i].data());
    }
    out.m_outCells->InsertNextCell(static_cast<vtkIdType>(n), ids.data());
}

} // namespace Structura::Visualization
