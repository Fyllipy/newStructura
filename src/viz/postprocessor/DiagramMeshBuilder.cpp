#include "DiagramMeshBuilder.h"
#include "UniformLoadForceEvaluator.h"
#include "VisualizationTheme.h"

#include <vtkCellArray.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>

#include <algorithm>
#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Structura::Visualization {

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
    return std::max(VisualizationTheme::minSamples,
                    static_cast<int>(length * VisualizationTheme::samplesPerMeter));
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
        const double v = extractValue(f, type);

        // Posição mundo ao longo do eixo da barra
        std::array<double, 3> bp{
            element.node1[0] + element.xLocal[0] * x,
            element.node1[1] + element.xLocal[1] * x,
            element.node1[2] + element.xLocal[2] * x
        };
        sampled.basePoints.push_back(bp);
        sampled.rawValues.push_back(v);
    }
    
    // LOG DE COMPROVAÇÃO SOLICITADO
    #include <fstream>
    std::ofstream logFile("C:\\Users\\João\\Documents\\Repos\\newStructura\\diagram_log.txt", std::ios_base::app);
    if (logFile.is_open()) {
        logFile << "\n============================================\n"
                << "PROVA NUMERICA - GERACAO DE DIAGRAMA\n"
                << "Elemento ID: " << element.externalId << "\n"
                << "Tipo Solicitado (Enum): " << static_cast<int>(type) << "\n"
                << "Valor Extraido no meio da barra (x=L/2): " 
                << extractValue(evaluator.evaluate(L/2.0), type) << "\n"
                << "Eixo de Extrusao Escolhido (perpAxis): "
                << "[" << sampled.perpAxis[0] << ", " 
                << sampled.perpAxis[1] << ", " 
                << sampled.perpAxis[2] << "]\n"
                << "============================================\n";
    }

    cache.store(key, std::move(sampled));
}

// ── buildMesh (dispatcher) ────────────────────────────────────────────────────

DiagramMeshBuilder::ElementMesh DiagramMeshBuilder::buildMesh(
    const SampledDiagram& sampled,
    const DiagramScaleManager& scale) const
{
    // Tipo Mt → faixa helicoidal; demais → superfície lateral padrão
    // Detectamos Mt indiretamente: se perpAxis == secAxis, é Mt.
    // Mais robusto: checar externamente. Aqui usamos heurística de perp vs sec.
    // Para simplificar: chamamos sempre buildStandardMesh, com hélice
    // implementada no buildTorsionMesh quando secAxis difere de perpAxis.
    // O PostProcessorRenderer passa o tipo → DiagramMeshBuilder deveria recebê-lo.
    // Solução: adicionamos overload com tipo explícito (ver buildMesh abaixo).
    return buildStandardMesh(sampled, scale);
}

DiagramMeshBuilder::ElementMesh DiagramMeshBuilder::buildMesh(
    const SampledDiagram& sampled,
    const DiagramScaleManager& scale,
    DiagramType type) const
{
    if (type == DiagramType::Mt) {
        return buildTorsionMesh(sampled, scale);
    }
    return buildStandardMesh(sampled, scale);
}

// ── buildStandardMesh ────────────────────────────────────────────────────────

DiagramMeshBuilder::ElementMesh DiagramMeshBuilder::buildStandardMesh(
    const SampledDiagram& sampled,
    const DiagramScaleManager& scale) const
{
    const std::size_t n = sampled.rawValues.size();
    if (n < 2) {
        return {};
    }

    // Pré-computa pontos do diagrama (com offset físico)
    std::vector<std::array<double, 3>> diagPts(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double offset = scale.physicalOffset(sampled.rawValues[i]);
        diagPts[i] = addScaled(sampled.basePoints[i], sampled.perpAxis, offset);
    }

    // ── Construção das malhas positiva e negativa ──────────────────────────
    // Usamos VTK para construir dois vtkPolyData separados
    vtkNew<vtkPoints>    posPoints, negPoints, outPoints;
    vtkNew<vtkCellArray> posCells,  negCells,  outCells;

    auto addQuad = [](vtkPoints* pts, vtkCellArray* cells,
                      const std::array<double,3>& p0,
                      const std::array<double,3>& p1,
                      const std::array<double,3>& d0,
                      const std::array<double,3>& d1)
    {
        const vtkIdType i0 = pts->InsertNextPoint(p0.data());
        const vtkIdType i1 = pts->InsertNextPoint(p1.data());
        const vtkIdType i2 = pts->InsertNextPoint(d1.data());
        const vtkIdType i3 = pts->InsertNextPoint(d0.data());
        // Triângulo A
        vtkIdType triA[3] = {i0, i1, i2};
        cells->InsertNextCell(3, triA);
        // Triângulo B
        vtkIdType triB[3] = {i0, i2, i3};
        cells->InsertNextCell(3, triB);
    };

    // Contorno: polyline ao longo da curva do diagrama
    vtkNew<vtkCellArray> outlineCells;
    vtkNew<vtkPoints>    outlinePoints;

    // Processar segmentos entre amostras consecutivas
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const double v0 = sampled.rawValues[i];
        const double v1 = sampled.rawValues[i + 1];

        const auto& p0 = sampled.basePoints[i];
        const auto& p1 = sampled.basePoints[i + 1];
        auto d0 = diagPts[i];
        auto d1 = diagPts[i + 1];

        if (v0 * v1 >= 0.0) {
            // Mesmo sinal — um único quad
            if (v0 >= 0.0) {
                addQuad(posPoints.GetPointer(), posCells.GetPointer(), p0, p1, d0, d1);
            } else {
                addQuad(negPoints.GetPointer(), negCells.GetPointer(), p0, p1, d0, d1);
            }
        } else {
            // Cruzamento de zero — inserir ponto exato
            const double t   = std::abs(v0) / (std::abs(v0) + std::abs(v1));
            std::array<double,3> pMid{
                p0[0] + (p1[0] - p0[0]) * t,
                p0[1] + (p1[1] - p0[1]) * t,
                p0[2] + (p1[2] - p0[2]) * t
            };
            // dMid = pMid (offset zero = no eixo)

            if (v0 >= 0.0) {
                addQuad(posPoints.GetPointer(), posCells.GetPointer(), p0, pMid, d0, pMid);
                addQuad(negPoints.GetPointer(), negCells.GetPointer(), pMid, p1, pMid, d1);
            } else {
                addQuad(negPoints.GetPointer(), negCells.GetPointer(), p0, pMid, d0, pMid);
                addQuad(posPoints.GetPointer(), posCells.GetPointer(), pMid, p1, pMid, d1);
            }
        }
    }

    // ── Contorno: polyline da curva do diagrama ───────────────────────────
    {
        std::vector<vtkIdType> ids;
        ids.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            ids.push_back(outlinePoints->InsertNextPoint(diagPts[i].data()));
        }
        outlineCells->InsertNextCell(static_cast<vtkIdType>(ids.size()), ids.data());
        // Linha de base (eixo da barra)
        std::vector<vtkIdType> baseIds;
        baseIds.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            baseIds.push_back(outlinePoints->InsertNextPoint(sampled.basePoints[i].data()));
        }
        outlineCells->InsertNextCell(static_cast<vtkIdType>(baseIds.size()), baseIds.data());
    }

    // ── Monta vtkPolyData ─────────────────────────────────────────────────
    auto makePolyData = [](vtkPoints* pts, vtkCellArray* cells) {
        vtkNew<vtkPolyData> pd;
        pd->SetPoints(pts);
        pd->SetPolys(cells);
        // Normais para iluminação correta (sem suavização geométrica)
        vtkNew<vtkPolyDataNormals> normals;
        normals->SetInputData(pd);
        normals->ComputePointNormalsOn();
        normals->ComputeCellNormalsOn();
        normals->SplittingOff();  // preserva topologia original
        normals->Update();
        return vtkSmartPointer<vtkPolyData>(normals->GetOutput());
    };

    ElementMesh mesh;
    if (posPoints->GetNumberOfPoints() > 0) {
        mesh.positiveSurface = makePolyData(posPoints.GetPointer(), posCells.GetPointer());
    }
    if (negPoints->GetNumberOfPoints() > 0) {
        mesh.negativeSurface = makePolyData(negPoints.GetPointer(), negCells.GetPointer());
    }

    // Contorno (polylines — não precisa de normais)
    {
        vtkNew<vtkPolyData> outlinePD;
        outlinePD->SetPoints(outlinePoints);
        outlinePD->SetLines(outlineCells);
        mesh.outline = outlinePD;
    }

    return mesh;
}

// ── buildTorsionMesh ──────────────────────────────────────────────────────────

DiagramMeshBuilder::ElementMesh DiagramMeshBuilder::buildTorsionMesh(
    const SampledDiagram& sampled,
    const DiagramScaleManager& scale) const
{
    const std::size_t n = sampled.rawValues.size();
    if (n < 2) {
        return {};
    }

    const double maxTwist = VisualizationTheme::maxTwistAngleDeg * M_PI / 180.0;
    const double refAmplitude = scale.refLength()
                              * VisualizationTheme::defaultAmplitudeFraction
                              * VisualizationTheme::torsionBandWidthFraction
                              * scale.userScale();

    vtkNew<vtkPoints>    posPoints, negPoints;
    vtkNew<vtkCellArray> posCells,  negCells;
    vtkNew<vtkPoints>    outlinePoints;
    vtkNew<vtkCellArray> outlineCells;

    for (std::size_t i = 0; i + 1 < n; ++i) {
        const double v0 = sampled.rawValues[i];
        const double v1 = sampled.rawValues[i + 1];

        // Ângulo de torção proporcional ao valor normalizado do elemento
        const double maxAbs = scale.maxAbs() < 1e-30 ? 1.0 : scale.maxAbs();
        const double theta0 = (v0 / maxAbs) * maxTwist;
        const double theta1 = (v1 / maxAbs) * maxTwist;

        const auto& c0 = sampled.basePoints[i];
        const auto& c1 = sampled.basePoints[i + 1];
        const auto& yA = sampled.perpAxis; // yLocal
        const auto& zA = sampled.secAxis;  // zLocal

        // P+/P- para amostra i
        auto wing = [&](const std::array<double,3>& center, double theta, double hw)
            -> std::pair<std::array<double,3>, std::array<double,3>>
        {
            const double cy = std::cos(theta) * hw;
            const double sz = std::sin(theta) * hw;
            std::array<double,3> plus {
                center[0] + yA[0]*cy + zA[0]*sz,
                center[1] + yA[1]*cy + zA[1]*sz,
                center[2] + yA[2]*cy + zA[2]*sz
            };
            std::array<double,3> minus {
                center[0] - yA[0]*cy - zA[0]*sz,
                center[1] - yA[1]*cy - zA[1]*sz,
                center[2] - yA[2]*cy - zA[2]*sz
            };
            return {plus, minus};
        };

        auto [p0plus, p0minus] = wing(c0, theta0, refAmplitude);
        auto [p1plus, p1minus] = wing(c1, theta1, refAmplitude);

        // Quad entre amostras consecutivas
        auto addStripQuad = [](vtkPoints* pts, vtkCellArray* cells,
                                const std::array<double,3>& a0,
                                const std::array<double,3>& a1,
                                const std::array<double,3>& b0,
                                const std::array<double,3>& b1) {
            vtkIdType i0 = pts->InsertNextPoint(a0.data());
            vtkIdType i1 = pts->InsertNextPoint(a1.data());
            vtkIdType i2 = pts->InsertNextPoint(b1.data());
            vtkIdType i3 = pts->InsertNextPoint(b0.data());
            vtkIdType triA[3] = {i0, i1, i2};
            cells->InsertNextCell(3, triA);
            vtkIdType triB[3] = {i0, i2, i3};
            cells->InsertNextCell(3, triB);
        };

        // Cor: positivo (torção positiva) → posPoints, negativo → negPoints
        bool isPos0 = (v0 >= 0.0);
        if (isPos0) {
            addStripQuad(posPoints.GetPointer(), posCells.GetPointer(),
                         p0plus, p1plus, p0minus, p1minus);
        } else {
            addStripQuad(negPoints.GetPointer(), negCells.GetPointer(),
                         p0plus, p1plus, p0minus, p1minus);
        }
    }

    // Contorno: eixo da barra
    {
        std::vector<vtkIdType> ids;
        ids.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            ids.push_back(outlinePoints->InsertNextPoint(sampled.basePoints[i].data()));
        }
        outlineCells->InsertNextCell(static_cast<vtkIdType>(ids.size()), ids.data());
    }

    auto makePolyData = [](vtkPoints* pts, vtkCellArray* cells) {
        vtkNew<vtkPolyData> pd;
        pd->SetPoints(pts);
        pd->SetPolys(cells);
        vtkNew<vtkPolyDataNormals> normals;
        normals->SetInputData(pd);
        normals->ComputePointNormalsOn();
        normals->SplittingOff();
        normals->Update();
        return vtkSmartPointer<vtkPolyData>(normals->GetOutput());
    };

    ElementMesh mesh;
    if (posPoints->GetNumberOfPoints() > 0) {
        mesh.positiveSurface = makePolyData(posPoints.GetPointer(), posCells.GetPointer());
    }
    if (negPoints->GetNumberOfPoints() > 0) {
        mesh.negativeSurface = makePolyData(negPoints.GetPointer(), negCells.GetPointer());
    }
    {
        vtkNew<vtkPolyData> outlinePD;
        outlinePD->SetPoints(outlinePoints);
        outlinePD->SetLines(outlineCells);
        mesh.outline = outlinePD;
    }
    return mesh;
}

} // namespace Structura::Visualization
