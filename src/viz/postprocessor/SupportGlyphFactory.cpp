#include "SupportGlyphFactory.h"

#include <vtkCellArray.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Structura::Visualization {

// ── Helpers de geometria ──────────────────────────────────────────────────────

static std::array<double, 3> add3(const std::array<double, 3>& a,
                                   const std::array<double, 3>& b) {
    return {a[0]+b[0], a[1]+b[1], a[2]+b[2]};
}

static std::array<double, 3> scale3(const std::array<double, 3>& a, double s) {
    return {a[0]*s, a[1]*s, a[2]*s};
}

void SupportGlyphFactory::appendLine(vtkPolyData* pd,
                                      const std::array<double, 3>& a,
                                      const std::array<double, 3>& b)
{
    vtkPoints* pts = pd->GetPoints();
    vtkCellArray* lines = pd->GetLines();
    if (!pts || !lines) return;

    vtkIdType id0 = pts->InsertNextPoint(a.data());
    vtkIdType id1 = pts->InsertNextPoint(b.data());
    vtkIdType ids[2] = {id0, id1};
    lines->InsertNextCell(2, ids);
}

void SupportGlyphFactory::appendPolygon(vtkPolyData* pd,
                                         const std::vector<std::array<double, 3>>& verts)
{
    if (verts.size() < 3) return;
    vtkPoints* pts = pd->GetPoints();
    vtkCellArray* lines = pd->GetLines();
    if (!pts || !lines) return;

    std::vector<vtkIdType> ids;
    ids.reserve(verts.size());
    for (const auto& v : verts) {
        ids.push_back(pts->InsertNextPoint(v.data()));
    }
    // Contorno fechado como polyline
    ids.push_back(ids[0]);
    lines->InsertNextCell(static_cast<vtkIdType>(ids.size()), ids.data());
}

// ── Nível de restrição ────────────────────────────────────────────────────────

int SupportGlyphFactory::restraintLevel(const std::array<bool, 6>& r)
{
    // Conta translações e rotações bloqueadas
    int trans = (r[0] ? 1 : 0) + (r[1] ? 1 : 0) + (r[2] ? 1 : 0);
    int rots  = (r[3] ? 1 : 0) + (r[4] ? 1 : 0) + (r[5] ? 1 : 0);

    if (rots >= 3 && trans >= 3) return 4; // fixo total
    if (trans >= 3)               return 3; // engaste (todas translações)
    if (trans == 2)               return 2; // pino (2 translações)
    if (trans == 1)               return 1; // rolete (1 translação)
    return 0;
}

// ── Pino (articulado) ─────────────────────────────────────────────────────────

void SupportGlyphFactory::addPinGlyph(vtkPolyData* pd,
                                        const std::array<double, 3>& pos,
                                        double size,
                                        const std::array<double, 3>& up)
{
    // up: direção "para cima" da estrutura (normalmente Z global)
    // Triangulo apontando para cima com vértice no ponto de apoio
    const double h = size;
    const double w = size * 0.6;

    // Vetor perpendicular ao up no plano XY
    std::array<double, 3> right{-up[1], up[0], 0.0};
    double rlen = std::sqrt(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
    if (rlen < 1e-10) right = {1.0, 0.0, 0.0};
    else { right[0]/=rlen; right[1]/=rlen; right[2]/=rlen; }

    const auto tip    = pos; // vértice no ponto de apoio
    const auto base_l = add3(add3(pos, scale3(up, -h)), scale3(right, -w));
    const auto base_r = add3(add3(pos, scale3(up, -h)), scale3(right,  w));

    appendPolygon(pd, {tip, base_l, base_r});

    // Linha de base (solo)
    const auto soil_l = add3(base_l, scale3(right, -w * 0.3));
    const auto soil_r = add3(base_r, scale3(right,  w * 0.3));
    appendLine(pd, soil_l, soil_r);
}

// ── Rolete (móvel) ───────────────────────────────────────────────────────────

void SupportGlyphFactory::addRollerGlyph(vtkPolyData* pd,
                                          const std::array<double, 3>& pos,
                                          double size,
                                          const std::array<double, 3>& up,
                                          const std::array<double, 3>& right)
{
    addPinGlyph(pd, pos, size, up);

    // 3 círculos planos (polígono de 8 vértices) representando roletes
    const double r = size * 0.08;
    const double baseY = -size;
    const int segs = 8;

    for (int c = -1; c <= 1; ++c) {
        const auto center = add3(add3(pos, scale3(up, baseY)), scale3(right, c * size * 0.25));
        std::vector<std::array<double, 3>> circVerts;
        circVerts.reserve(segs);
        for (int k = 0; k < segs; ++k) {
            const double angle = 2.0 * M_PI * k / segs;
            const auto v = add3(add3(center, scale3(right, r * std::cos(angle))),
                                scale3(up, r * std::sin(angle)));
            circVerts.push_back(v);
        }
        appendPolygon(pd, circVerts);
    }

    // Linha de base
    const auto bl = add3(add3(pos, scale3(up, -size * 1.18)), scale3(right, -size * 0.5));
    const auto br = add3(add3(pos, scale3(up, -size * 1.18)), scale3(right,  size * 0.5));
    appendLine(pd, bl, br);
}

// ── Engaste ──────────────────────────────────────────────────────────────────

void SupportGlyphFactory::addFixedGlyph(vtkPolyData* pd,
                                         const std::array<double, 3>& pos,
                                         double size,
                                         const std::array<double, 3>& up,
                                         const std::array<double, 3>& right,
                                         bool bilateral)
{
    // Linha de ancoragem vertical
    const auto anchorTop = add3(pos, scale3(up,  size * 0.5));
    const auto anchorBot = add3(pos, scale3(up, -size * 0.5));
    appendLine(pd, anchorTop, anchorBot);

    // 5 hachuras diagonais
    const int numHatch = 5;
    const double hatchLen = size * 0.25;
    for (int i = 0; i < numHatch; ++i) {
        const double t = -0.4 + 0.2 * i; // distribuição uniforme
        const auto origin = add3(pos, scale3(up, t * size));
        // Hachura para a direita
        const auto hEnd = add3(add3(origin, scale3(right, hatchLen * 0.7)),
                               scale3(up, -hatchLen * 0.7));
        appendLine(pd, origin, hEnd);
        if (bilateral) {
            // Hachura para a esquerda também
            const auto hEndL = add3(add3(origin, scale3(right, -hatchLen * 0.7)),
                                    scale3(up, -hatchLen * 0.7));
            appendLine(pd, origin, hEndL);
        }
    }
}

// ── build (ponto de entrada) ─────────────────────────────────────────────────

vtkSmartPointer<vtkPolyData> SupportGlyphFactory::build(
    const std::vector<SupportData>& supports)
{
    vtkNew<vtkPolyData> pd;
    vtkNew<vtkPoints>   pts;
    vtkNew<vtkCellArray> lines;
    pd->SetPoints(pts);
    pd->SetLines(lines);

    // Vetores padrão de referência
    const std::array<double, 3> up    = {0.0, 0.0, 1.0};
    const std::array<double, 3> right = {1.0, 0.0, 0.0};

    for (const auto& s : supports) {
        const int level = restraintLevel(s.restraints);
        switch (level) {
        case 1:
            addRollerGlyph(pd, s.position, s.glyphSize, up, right);
            break;
        case 2:
            addPinGlyph(pd, s.position, s.glyphSize, up);
            break;
        case 3:
            addFixedGlyph(pd, s.position, s.glyphSize, up, right, false);
            break;
        case 4:
            addFixedGlyph(pd, s.position, s.glyphSize, up, right, true);
            break;
        default:
            break; // apoio livre — nenhum glifo
        }
    }

    return pd;
}

} // namespace Structura::Visualization
