#include "LoadGlyphFactory.h"

#include <vtkCellArray.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Structura::Visualization {

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::array<double,3> add3L(const std::array<double,3>& a,
                                   const std::array<double,3>& b) {
    return {a[0]+b[0], a[1]+b[1], a[2]+b[2]};
}
static std::array<double,3> scale3L(const std::array<double,3>& a, double s) {
    return {a[0]*s, a[1]*s, a[2]*s};
}
static double len3(const std::array<double,3>& a) {
    return std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);
}
static std::array<double,3> norm3(const std::array<double,3>& a) {
    const double l = len3(a);
    if (l < 1e-10) return {1,0,0};
    return {a[0]/l, a[1]/l, a[2]/l};
}
static std::array<double,3> cross3(const std::array<double,3>& a,
                                    const std::array<double,3>& b) {
    return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
}

void LoadGlyphFactory::appendLine(vtkPolyData* pd,
                                   const std::array<double,3>& a,
                                   const std::array<double,3>& b)
{
    vtkIdType id0 = pd->GetPoints()->InsertNextPoint(a.data());
    vtkIdType id1 = pd->GetPoints()->InsertNextPoint(b.data());
    vtkIdType ids[2] = {id0, id1};
    pd->GetLines()->InsertNextCell(2, ids);
}

std::array<double,3> LoadGlyphFactory::perpToDir(const std::array<double,3>& dir)
{
    // Encontra vetor perpendicular ao dir
    std::array<double,3> ref = {0,0,1};
    if (std::abs(dir[2]) > 0.9) ref = {1,0,0};
    return norm3(cross3(dir, ref));
}

// ── Seta (haste + ponta triangular plana) ─────────────────────────────────────

void LoadGlyphFactory::addArrow(vtkPolyData* pd,
                                 const std::array<double,3>& tip,
                                 const std::array<double,3>& dir,
                                 double shaftLen,
                                 double headSize,
                                 const std::array<double,3>& perpRef)
{
    const auto nd = norm3(dir);
    // Pé da haste
    const auto base = add3L(tip, scale3L(nd, -shaftLen));
    appendLine(pd, base, tip);

    // Ponta triangular plana (3 vértices)
    const auto side = scale3L(perpRef, headSize * 0.35);
    const auto headBase = add3L(tip, scale3L(nd, -headSize));

    vtkIdType i0 = pd->GetPoints()->InsertNextPoint(tip.data());
    vtkIdType i1 = pd->GetPoints()->InsertNextPoint(add3L(headBase, side).data());
    vtkIdType i2 = pd->GetPoints()->InsertNextPoint(add3L(headBase, scale3L(side, -1.0)).data());
    vtkIdType tri[3] = {i0, i1, i2};
    pd->GetPolys()->InsertNextCell(3, tri);
}

// ── Cargas nodais ─────────────────────────────────────────────────────────────

vtkSmartPointer<vtkPolyData> LoadGlyphFactory::buildNodalLoads(
    const std::vector<NodalLoadData>& loads)
{
    vtkNew<vtkPolyData>  pd;
    vtkNew<vtkPoints>    pts;
    vtkNew<vtkCellArray> lines;
    vtkNew<vtkCellArray> polys;
    pd->SetPoints(pts);
    pd->SetLines(lines);
    pd->SetPolys(polys);

    for (const auto& load : loads) {
        if (load.magnitude == 0.0) continue;
        const auto dir  = norm3(load.direction);
        const auto perp = perpToDir(dir);
        addArrow(pd, load.position, dir, load.arrowSize, load.arrowSize * 0.25, perp);
    }
    return pd;
}

// ── Carga distribuída ────────────────────────────────────────────────────────

vtkSmartPointer<vtkPolyData> LoadGlyphFactory::buildDistributedLoads(
    const std::vector<DistributedLoadData>& loads)
{
    vtkNew<vtkPolyData>  pd;
    vtkNew<vtkPoints>    pts;
    vtkNew<vtkCellArray> lines;
    vtkNew<vtkCellArray> polys;
    pd->SetPoints(pts);
    pd->SetLines(lines);
    pd->SetPolys(polys);

    for (const auto& load : loads) {
        if (load.magnitude == 0.0) continue;

        const std::array<double,3> span = {
            load.node2[0] - load.node1[0],
            load.node2[1] - load.node1[1],
            load.node2[2] - load.node1[2]
        };
        const double length = len3(span);
        if (length < 1e-6) continue;

        const auto barDir = norm3(span);
        const auto loadDir = norm3(load.loadDir);
        const auto perp = perpToDir(loadDir);

        // Linha de referência paralela à barra (onde as setas partem)
        const auto offset = scale3L(loadDir, load.arrowSize);
        const auto refStart = add3L(load.node1, offset);
        const auto refEnd   = add3L(load.node2, offset);
        appendLine(pd, refStart, refEnd);

        // N setas espaçadas
        const int nArrows = std::max(3, static_cast<int>(length / 0.5));
        for (int i = 0; i <= nArrows; ++i) {
            const double t = static_cast<double>(i) / nArrows;
            const auto origin = add3L(load.node1, scale3L(span, t));
            const auto arrowTip = origin; // seta aponta para a barra
            const auto arrowFrom = add3L(origin, offset);
            appendLine(pd, arrowFrom, arrowTip);
            // Ponta triangular
            const auto headBase = add3L(arrowTip, scale3L(loadDir, load.arrowSize * 0.2));
            const auto side = scale3L(barDir, load.arrowSize * 0.08);
            vtkIdType i0 = pd->GetPoints()->InsertNextPoint(arrowTip.data());
            vtkIdType i1 = pd->GetPoints()->InsertNextPoint(add3L(headBase, side).data());
            vtkIdType i2 = pd->GetPoints()->InsertNextPoint(add3L(headBase, scale3L(side,-1)).data());
            vtkIdType tri[3] = {i0, i1, i2};
            pd->GetPolys()->InsertNextCell(3, tri);
        }
    }
    return pd;
}

// ── Momentos nodais ───────────────────────────────────────────────────────────

vtkSmartPointer<vtkPolyData> LoadGlyphFactory::buildNodalMoments(
    const std::vector<NodalMomentData>& moments)
{
    vtkNew<vtkPolyData>  pd;
    vtkNew<vtkPoints>    pts;
    vtkNew<vtkCellArray> lines;
    vtkNew<vtkCellArray> polys;
    pd->SetPoints(pts);
    pd->SetLines(lines);
    pd->SetPolys(polys);

    for (const auto& mom : moments) {
        if (mom.magnitude == 0.0) continue;
        const auto axisN = norm3(mom.axis);
        const auto ref   = perpToDir(axisN);
        const auto tang  = cross3(axisN, ref);

        // Arco de 180° com 16 segmentos
        const int segs = 16;
        const double angleStep = M_PI / segs;
        std::vector<vtkIdType> arcIds;
        arcIds.reserve(segs + 1);

        for (int k = 0; k <= segs; ++k) {
            const double a = k * angleStep;
            std::array<double,3> p = {
                mom.position[0] + ref[0]*mom.radius*std::cos(a) + tang[0]*mom.radius*std::sin(a),
                mom.position[1] + ref[1]*mom.radius*std::cos(a) + tang[1]*mom.radius*std::sin(a),
                mom.position[2] + ref[2]*mom.radius*std::cos(a) + tang[2]*mom.radius*std::sin(a)
            };
            arcIds.push_back(pd->GetPoints()->InsertNextPoint(p.data()));
        }
        pd->GetLines()->InsertNextCell(static_cast<vtkIdType>(arcIds.size()), arcIds.data());

        // Ponta tangencial plana no final do arco (k = segs)
        const double aEnd = M_PI;
        std::array<double,3> tipPt = {
            mom.position[0] + ref[0]*mom.radius*std::cos(aEnd) + tang[0]*mom.radius*std::sin(aEnd),
            mom.position[1] + ref[1]*mom.radius*std::cos(aEnd) + tang[1]*mom.radius*std::sin(aEnd),
            mom.position[2] + ref[2]*mom.radius*std::cos(aEnd) + tang[2]*mom.radius*std::sin(aEnd)
        };
        // Tangente no ponto final: d/da(arc) = -ref*sin(a) + tang*cos(a)
        std::array<double,3> tangDir = {
            -ref[0]*std::sin(aEnd) + tang[0]*std::cos(aEnd),
            -ref[1]*std::sin(aEnd) + tang[1]*std::cos(aEnd),
            -ref[2]*std::sin(aEnd) + tang[2]*std::cos(aEnd)
        };
        tangDir = norm3(tangDir);
        const double hs = mom.radius * 0.20;
        const auto perpT = cross3(axisN, tangDir);
        vtkIdType tip  = pd->GetPoints()->InsertNextPoint(tipPt.data());
        vtkIdType hb1  = pd->GetPoints()->InsertNextPoint(
            add3L(add3L(tipPt, scale3L(tangDir, -hs)), scale3L(perpT,  hs*0.5)).data());
        vtkIdType hb2  = pd->GetPoints()->InsertNextPoint(
            add3L(add3L(tipPt, scale3L(tangDir, -hs)), scale3L(perpT, -hs*0.5)).data());
        vtkIdType tri[3] = {tip, hb1, hb2};
        pd->GetPolys()->InsertNextCell(3, tri);
    }
    return pd;
}

} // namespace Structura::Visualization
