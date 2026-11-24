#include "viz/structure/ShellRenderer.h"

#include <algorithm>
#include <cmath>

#include <vtkCellArray.h>
#include <vtkFloatArray.h>
#include <vtkLookupTable.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkUnsignedCharArray.h>

namespace Structura::Visualization {

namespace {

/// Tabela de cores em faixas: 250 entradas (25 por faixa) para que a textura 1D de cores tenha degraus nitidos nas fronteiras das faixas.
vtkSmartPointer<vtkLookupTable> makeBandLookupTable()
{
    constexpr int kPerBand = 25;
    auto lut = vtkSmartPointer<vtkLookupTable>::New();
    lut->SetNumberOfTableValues(Structura::Shell::kColorBands * kPerBand);
    lut->SetRange(0.0, 1.0);
    for (int i = 0; i < Structura::Shell::kColorBands * kPerBand; ++i) {
        const double t = (static_cast<double>(i) + 0.5) / (Structura::Shell::kColorBands * kPerBand);
        const auto c = Structura::Shell::bandColor(t);
        lut->SetTableValue(i, c[0] / 255.0, c[1] / 255.0, c[2] / 255.0, 1.0);
    }
    const auto &bad = Structura::Shell::kInvalidValueColor;
    lut->SetNanColor(bad[0] / 255.0, bad[1] / 255.0, bad[2] / 255.0, 1.0);
    return lut;
}

} // namespace

ShellRenderer::ShellRenderer()
    : m_faces(vtkSmartPointer<vtkActor>::New())
    , m_edges(vtkSmartPointer<vtkActor>::New())
{
    auto faceMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    faceMapper->SetInputData(vtkSmartPointer<vtkPolyData>::New());
    faceMapper->SetColorModeToDirectScalars();
    // as arestas ficam ligeiramente a frente para nao brigar com as faces
    faceMapper->SetResolveCoincidentTopologyToPolygonOffset();
    m_faces->SetMapper(faceMapper);
    m_faces->PickableOn();
    m_faces->SetVisibility(0);
    m_picker = vtkSmartPointer<vtkCellPicker>::New();
    m_picker->SetTolerance(0.0005);
    m_picker->SetPickFromList(true);
    m_picker->AddPickList(m_faces);
    m_faces->GetProperty()->SetAmbient(0.45);
    m_faces->GetProperty()->SetDiffuse(0.65);
    m_faces->GetProperty()->SetSpecular(0.05);
    m_faces->GetProperty()->BackfaceCullingOff();

    auto edgeMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    edgeMapper->SetInputData(vtkSmartPointer<vtkPolyData>::New());
    edgeMapper->SetResolveCoincidentTopologyToPolygonOffset();
    m_edges->SetMapper(edgeMapper);
    m_edges->PickableOff();
    m_edges->SetVisibility(0);
    m_edges->GetProperty()->SetColor(0.15, 0.17, 0.2);
    m_edges->GetProperty()->SetOpacity(0.55);
    m_edges->GetProperty()->SetLineWidth(1.0);
    m_edges->GetProperty()->LightingOff();

    // nos selecionados: pontos grandes por cima da malha
    auto selMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    selMapper->SetInputData(vtkSmartPointer<vtkPolyData>::New());
    selMapper->SetResolveCoincidentTopologyToPolygonOffset();
    selMapper->SetRelativeCoincidentTopologyPointOffsetParameter(-66000.0);
    m_selection = vtkSmartPointer<vtkActor>::New();
    m_selection->SetMapper(selMapper);
    m_selection->PickableOff();
    m_selection->SetVisibility(0);
    m_selection->GetProperty()->SetColor(1.0, 0.55, 0.0);
    m_selection->GetProperty()->SetPointSize(11.0);
    m_selection->GetProperty()->SetRenderPointsAsSpheres(true);
    m_selection->GetProperty()->LightingOff();

    // apoios: faces coloridas por vertice e linhas (molas)
    m_supportFaces = vtkSmartPointer<vtkActor>::New();
    {
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputData(vtkSmartPointer<vtkPolyData>::New());
        mapper->SetColorModeToDirectScalars();
        m_supportFaces->SetMapper(mapper);
        m_supportFaces->PickableOff();
        m_supportFaces->SetVisibility(0);
        m_supportFaces->GetProperty()->SetAmbient(0.55);
        m_supportFaces->GetProperty()->SetDiffuse(0.5);
    }
    m_supportLines = vtkSmartPointer<vtkActor>::New();
    {
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputData(vtkSmartPointer<vtkPolyData>::New());
        mapper->SetColorModeToDirectScalars();
        m_supportLines->SetMapper(mapper);
        m_supportLines->PickableOff();
        m_supportLines->SetVisibility(0);
        m_supportLines->GetProperty()->SetLineWidth(2.5f);
        m_supportLines->GetProperty()->LightingOff();
    }

    // setas das cargas por clique
    m_arrows = vtkSmartPointer<vtkActor>::New();
    {
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputData(vtkSmartPointer<vtkPolyData>::New());
        mapper->SetColorModeToDirectScalars();
        mapper->SetResolveCoincidentTopologyToPolygonOffset();
        mapper->SetRelativeCoincidentTopologyLineOffsetParameters(-1.0, -66000.0);
        m_arrows->SetMapper(mapper);
        m_arrows->PickableOff();
        m_arrows->SetVisibility(0);
        m_arrows->GetProperty()->SetLineWidth(3.0f);
        m_arrows->GetProperty()->LightingOff();
    }
}

void ShellRenderer::attach(vtkRenderer *renderer)
{
    m_renderer = renderer;
    if (m_renderer) {
        m_renderer->AddActor(m_faces);
        m_renderer->AddActor(m_edges);
        m_renderer->AddActor(m_selection);
        m_renderer->AddActor(m_supportFaces);
        m_renderer->AddActor(m_supportLines);
        m_renderer->AddActor(m_arrows);
    }
}

ShellRenderer::PickResult ShellRenderer::pickPoint(int displayX, int displayY) const
{
    PickResult result;
    if (!m_renderer || !m_visible || !m_picker) {
        return result;
    }
    if (m_picker->Pick(displayX, displayY, 0.0, m_renderer)) {
        const vtkIdType cell = m_picker->GetCellId();
        if (cell >= 0) {
            result.quad = static_cast<int>(cell / 2);   // dois triangulos por quadrilatero
            double position[3] = {0.0, 0.0, 0.0};
            m_picker->GetPickPosition(position);
            result.point = {position[0], position[1], position[2]};
        }
    }
    return result;
}

int ShellRenderer::pickQuad(int displayX, int displayY) const
{
    return pickPoint(displayX, displayY).quad;
}

void ShellRenderer::setVisible(bool visible)
{
    m_visible = visible;
    m_faces->SetVisibility(visible ? 1 : 0);
    m_edges->SetVisibility(visible ? 1 : 0);
    m_selection->SetVisibility(visible && m_selectedCount > 0 ? 1 : 0);
    setSupportsVisible(m_supportsVisible);
    setLoadArrowsVisible(m_arrowsVisible);
}

namespace {

// cores por tipo (ordem de Structura::Shell::SupportKind: engaste, articulado, movel, parcial, mola, so compressao)
constexpr unsigned char kSupportColors[6][3] = {{192, 57, 43}, {46, 134, 193}, {40, 180, 99}, {175, 122, 197}, {230, 126, 34}, {23, 165, 137}};

struct Shape {
    std::vector<std::array<float, 3>> v;
    std::vector<std::array<int, 3>> t;
};

// formas unitarias com o ponto de apoio na origem e o corpo ABAIXO (z negativo)
Shape cubeShape()
{
    Shape s;
    for (int i = 0; i < 8; ++i) {
        s.v.push_back({((i & 1) ? 0.5f : -0.5f), ((i & 2) ? 0.5f : -0.5f), ((i & 4) ? -0.1f : -1.1f)});
    }
    const int f[12][3] = {{0, 2, 1}, {1, 2, 3}, {4, 5, 6}, {5, 7, 6}, {0, 1, 4}, {1, 5, 4}, {2, 6, 3}, {3, 6, 7}, {0, 4, 2}, {2, 4, 6}, {1, 3, 5}, {3, 7, 5}};
    for (const auto &q : f) {
        s.t.push_back({q[0], q[1], q[2]});
    }
    return s;
}

Shape pyramidShape()
{
    Shape s;
    s.v = {{0, 0, 0}, {-0.6f, -0.6f, -1.0f}, {0.6f, -0.6f, -1.0f}, {0.6f, 0.6f, -1.0f}, {-0.6f, 0.6f, -1.0f}};
    s.t = {{0, 1, 2}, {0, 2, 3}, {0, 3, 4}, {0, 4, 1}, {1, 3, 2}, {1, 4, 3}};
    return s;
}

Shape octahedronShape()
{
    Shape s;
    s.v = {{0.5f, 0, -0.5f}, {-0.5f, 0, -0.5f}, {0, 0.5f, -0.5f}, {0, -0.5f, -0.5f}, {0, 0, 0}, {0, 0, -1.0f}};
    s.t = {{4, 0, 2}, {4, 2, 1}, {4, 1, 3}, {4, 3, 0}, {5, 2, 0}, {5, 1, 2}, {5, 3, 1}, {5, 0, 3}};
    return s;
}

Shape tetraShape()
{
    Shape s;
    s.v = {{0, 0, 0}, {-0.6f, -0.5f, -1.0f}, {0.6f, -0.5f, -1.0f}, {0, 0.7f, -1.0f}};
    s.t = {{0, 1, 2}, {0, 2, 3}, {0, 3, 1}, {1, 3, 2}};
    return s;
}

Shape slabShape()
{
    Shape s;
    for (int i = 0; i < 8; ++i) {
        s.v.push_back({((i & 1) ? 0.7f : -0.7f), ((i & 2) ? 0.7f : -0.7f), ((i & 4) ? -0.75f : -1.0f)});
    }
    const int f[12][3] = {{0, 2, 1}, {1, 2, 3}, {4, 5, 6}, {5, 7, 6}, {0, 1, 4}, {1, 5, 4}, {2, 6, 3}, {3, 6, 7}, {0, 4, 2}, {2, 4, 6}, {1, 3, 5}, {3, 7, 5}};
    for (const auto &q : f) {
        s.t.push_back({q[0], q[1], q[2]});
    }
    return s;
}

} // namespace

void ShellRenderer::setLoadArrowsVisible(bool visible)
{
    m_arrowsVisible = visible;
    m_arrows->SetVisibility(visible && m_visible && m_arrowCount > 0 ? 1 : 0);
}

void ShellRenderer::setLoadArrows(const std::vector<LoadArrowGlyph> &arrows)
{
    m_arrowCount = arrows.size();
    // cores por tipo: forca nodal, carga linear, carga de superficie
    static constexpr unsigned char kColors[3][3] = {{170, 0, 170}, {210, 90, 0}, {0, 125, 75}};
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto lines = vtkSmartPointer<vtkCellArray>::New();
    auto colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    colors->SetNumberOfComponents(3);
    for (const LoadArrowGlyph &a : arrows) {
        const unsigned char *c = kColors[std::clamp(a.kind, 0, 2)];
        float d[3] = {a.direction[0], a.direction[1], a.direction[2]};
        const float dl = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (!(dl > 0.f) || !(a.length > 0.f)) {
            continue;
        }
        for (float &x : d) {
            x /= dl;
        }
        // dois vetores perpendiculares para a cabeca
        float u[3] = {1.f, 0.f, 0.f};
        if (std::abs(d[0]) > 0.9f) {
            u[0] = 0.f;
            u[1] = 1.f;
            u[2] = 0.f;
        }
        const float dot = u[0] * d[0] + u[1] * d[1] + u[2] * d[2];
        for (int k = 0; k < 3; ++k) {
            u[k] -= dot * d[k];
        }
        const float ul = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
        for (float &x : u) {
            x /= ul;
        }
        const float w[3] = {d[1] * u[2] - d[2] * u[1], d[2] * u[0] - d[0] * u[2], d[0] * u[1] - d[1] * u[0]};
        const float tip[3] = {a.tip[0], a.tip[1], a.tip[2]};
        const float tail[3] = {tip[0] - d[0] * a.length, tip[1] - d[1] * a.length, tip[2] - d[2] * a.length};
        const float head = 0.28f * a.length, half = 0.13f * a.length;
        auto addLine = [&](const float *p0, const float *p1) {
            const vtkIdType i0 = points->InsertNextPoint(p0[0], p0[1], p0[2]);
            const vtkIdType i1 = points->InsertNextPoint(p1[0], p1[1], p1[2]);
            colors->InsertNextTuple3(c[0], c[1], c[2]);
            colors->InsertNextTuple3(c[0], c[1], c[2]);
            const vtkIdType ids[2] = {i0, i1};
            lines->InsertNextCell(2, ids);
        };
        addLine(tail, tip);
        const float base[3] = {tip[0] - d[0] * head, tip[1] - d[1] * head, tip[2] - d[2] * head};
        for (const float sign : {1.f, -1.f}) {
            const float pu[3] = {base[0] + sign * u[0] * half, base[1] + sign * u[1] * half, base[2] + sign * u[2] * half};
            const float pw[3] = {base[0] + sign * w[0] * half, base[1] + sign * w[1] * half, base[2] + sign * w[2] * half};
            addLine(tip, pu);
            addLine(tip, pw);
        }
    }
    auto data = vtkSmartPointer<vtkPolyData>::New();
    data->SetPoints(points);
    data->SetLines(lines);
    data->GetPointData()->SetScalars(colors);
    if (auto *mapper = vtkPolyDataMapper::SafeDownCast(m_arrows->GetMapper())) {
        mapper->SetInputData(data);
        mapper->Modified();
    }
    setLoadArrowsVisible(m_arrowsVisible);
}

void ShellRenderer::setSupportsVisible(bool visible)
{
    m_supportsVisible = visible;
    const int on = visible && m_visible && m_supportCount > 0 ? 1 : 0;
    m_supportFaces->SetVisibility(on);
    m_supportLines->SetVisibility(on);
}

void ShellRenderer::setSupports(const std::vector<SupportGlyph> &glyphs, float size)
{
    m_supportCount = glyphs.size();
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto cells = vtkSmartPointer<vtkCellArray>::New();
    auto colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    colors->SetNumberOfComponents(3);
    auto linePoints = vtkSmartPointer<vtkPoints>::New();
    auto lines = vtkSmartPointer<vtkCellArray>::New();
    auto lineColors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    lineColors->SetNumberOfComponents(3);

    const Shape cube = cubeShape(), pyramid = pyramidShape(), octa = octahedronShape(), tetra = tetraShape(), slab = slabShape();
    for (const SupportGlyph &g : glyphs) {
        const int kind = std::clamp(g.kind, 0, 5);
        const unsigned char *c = kSupportColors[kind];
        auto emit = [&](const Shape &shape) {
            const vtkIdType base = points->GetNumberOfPoints();
            for (const auto &v : shape.v) {
                points->InsertNextPoint(g.position[0] + v[0] * size, g.position[1] + v[1] * size, g.position[2] + v[2] * size);
                colors->InsertNextTuple3(c[0], c[1], c[2]);
            }
            for (const auto &t : shape.t) {
                const vtkIdType ids[3] = {base + t[0], base + t[1], base + t[2]};
                cells->InsertNextCell(3, ids);
            }
        };
        switch (kind) {
        case 0: emit(cube); break;
        case 1: emit(pyramid); break;
        case 2: emit(octa); break;
        case 3: emit(tetra); break;
        case 4:
        case 5: {
            // mola: zigue-zague do no ate a base (so-compressao: com placa de apoio)
            float dir[3] = {0.f, 0.f, -1.f};
            if (kind == 5) {
                dir[0] = -g.normal[0];
                dir[1] = -g.normal[1];
                dir[2] = -g.normal[2];
            }
            // vetor perpendicular para a oscilacao
            float perp[3] = {1.f, 0.f, 0.f};
            if (std::abs(dir[0]) > 0.9f) {
                perp[0] = 0.f;
                perp[1] = 1.f;
            }
            const float dot = perp[0] * dir[0] + perp[1] * dir[1] + perp[2] * dir[2];
            for (int k = 0; k < 3; ++k) {
                perp[k] -= dot * dir[k];
            }
            const float len = std::sqrt(perp[0] * perp[0] + perp[1] * perp[1] + perp[2] * perp[2]);
            for (float &q : perp) {
                q /= len > 0.f ? len : 1.f;
            }
            const int segments = 8;
            const float total = 1.2f * size;
            vtkIdType previous = -1;
            for (int i = 0; i <= segments; ++i) {
                const float t = total * static_cast<float>(i) / segments;
                const float amp = (i == 0 || i == segments) ? 0.f : ((i % 2) ? 0.3f : -0.3f) * size;
                const vtkIdType id = linePoints->InsertNextPoint(g.position[0] + dir[0] * t + perp[0] * amp, g.position[1] + dir[1] * t + perp[1] * amp,
                                                                  g.position[2] + dir[2] * t + perp[2] * amp);
                lineColors->InsertNextTuple3(c[0], c[1], c[2]);
                if (previous >= 0) {
                    const vtkIdType seg[2] = {previous, id};
                    lines->InsertNextCell(2, seg);
                }
                previous = id;
            }
            if (kind == 5) {
                // placa de apoio ao fim da mola (em planta, perpendicular a normal; para normal vertical, o laje)
                const float cx = g.position[0] + dir[0] * total, cy = g.position[1] + dir[1] * total, cz = g.position[2] + dir[2] * total;
                const float h = 0.45f * size;
                float u[3] = {perp[0], perp[1], perp[2]};
                float w[3] = {dir[1] * u[2] - dir[2] * u[1], dir[2] * u[0] - dir[0] * u[2], dir[0] * u[1] - dir[1] * u[0]};
                const vtkIdType base = points->GetNumberOfPoints();
                const float cs[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
                for (const auto &q : cs) {
                    points->InsertNextPoint(cx + h * (q[0] * u[0] + q[1] * w[0]), cy + h * (q[0] * u[1] + q[1] * w[1]), cz + h * (q[0] * u[2] + q[1] * w[2]));
                    colors->InsertNextTuple3(c[0], c[1], c[2]);
                }
                const vtkIdType t1[3] = {base, base + 1, base + 2}, t2[3] = {base, base + 2, base + 3};
                cells->InsertNextCell(3, t1);
                cells->InsertNextCell(3, t2);
            }
            break;
        }
        default: break;
        }
    }
    (void)slab;
    auto faces = vtkSmartPointer<vtkPolyData>::New();
    faces->SetPoints(points);
    faces->SetPolys(cells);
    faces->GetPointData()->SetScalars(colors);
    if (auto *mapper = vtkPolyDataMapper::SafeDownCast(m_supportFaces->GetMapper())) {
        mapper->SetInputData(faces);
        mapper->Modified();
    }
    auto lineData = vtkSmartPointer<vtkPolyData>::New();
    lineData->SetPoints(linePoints);
    lineData->SetLines(lines);
    lineData->GetPointData()->SetScalars(lineColors);
    if (auto *mapper = vtkPolyDataMapper::SafeDownCast(m_supportLines->GetMapper())) {
        mapper->SetInputData(lineData);
        mapper->Modified();
    }
    setSupportsVisible(m_supportsVisible);
}

void ShellRenderer::setSelectedNodes(const std::vector<std::array<float, 3>> &points)
{
    m_selectedCount = points.size();
    auto pts = vtkSmartPointer<vtkPoints>::New();
    pts->SetNumberOfPoints(static_cast<vtkIdType>(points.size()));
    auto verts = vtkSmartPointer<vtkCellArray>::New();
    for (std::size_t i = 0; i < points.size(); ++i) {
        pts->SetPoint(static_cast<vtkIdType>(i), points[i][0], points[i][1], points[i][2]);
        const vtkIdType id = static_cast<vtkIdType>(i);
        verts->InsertNextCell(1, &id);
    }
    auto data = vtkSmartPointer<vtkPolyData>::New();
    data->SetPoints(pts);
    data->SetVerts(verts);
    if (auto *mapper = vtkPolyDataMapper::SafeDownCast(m_selection->GetMapper())) {
        mapper->SetInputData(data);
        mapper->Modified();
    }
    m_selection->SetVisibility(m_visible && m_selectedCount > 0 ? 1 : 0);
}

void ShellRenderer::setMesh(const Structura::Shell::RenderMesh &mesh)
{
    // faces coloridas por VALOR: faixas de cor constante com transicao brusca (estilo Mecway), desenhadas por textura 1D sobre o valor
    // interpolado no elemento (a fronteira acompanha a curva de nivel); sem valores, as cores RGB por vertice
    const bool banded = mesh.hasValues && mesh.scalars.size() == mesh.points.size();
    auto position = [&](std::size_t i) { return Structura::Shell::colorPosition(mesh.scalars[i], mesh.minValue, mesh.maxValue); };
    // so as cores mudaram (outra grandeza, outra faixa, realce): a geometria nao e reconstruida
    if (m_faceData && mesh.geometryRevision != 0 && mesh.geometryRevision == m_revision && mesh.points.size() == m_vertexCount && mesh.colors.size() == m_vertexCount
        && banded == m_bandedColors) {
        if (banded) {
            if (auto *t = vtkFloatArray::SafeDownCast(m_faceData->GetPointData()->GetScalars())) {
                for (std::size_t i = 0; i < mesh.scalars.size(); ++i) {
                    t->SetValue(static_cast<vtkIdType>(i), static_cast<float>(position(i)));
                }
                t->Modified();
                m_faceData->Modified();
                ++m_colorUpdates;
                return;
            }
        } else if (auto *colors = vtkUnsignedCharArray::SafeDownCast(m_faceData->GetPointData()->GetScalars())) {
            for (std::size_t i = 0; i < mesh.colors.size(); ++i) {
                colors->SetTuple3(static_cast<vtkIdType>(i), mesh.colors[i][0], mesh.colors[i][1], mesh.colors[i][2]);
            }
            colors->Modified();
            m_faceData->Modified();
            ++m_colorUpdates;
            return;
        }
    }
    ++m_geometryRebuilds;
    m_revision = mesh.geometryRevision;
    m_vertexCount = mesh.points.size();
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetNumberOfPoints(static_cast<vtkIdType>(mesh.points.size()));
    auto normals = vtkSmartPointer<vtkFloatArray>::New();
    normals->SetNumberOfComponents(3);
    normals->SetNumberOfTuples(static_cast<vtkIdType>(mesh.points.size()));
    auto colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    colors->SetNumberOfComponents(3);
    colors->SetNumberOfTuples(static_cast<vtkIdType>(mesh.points.size()));
    for (std::size_t i = 0; i < mesh.points.size(); ++i) {
        const auto id = static_cast<vtkIdType>(i);
        points->SetPoint(id, mesh.points[i][0], mesh.points[i][1], mesh.points[i][2]);
        normals->SetTuple3(id, mesh.normals[i][0], mesh.normals[i][1], mesh.normals[i][2]);
        colors->SetTuple3(id, mesh.colors[i][0], mesh.colors[i][1], mesh.colors[i][2]);
    }
    auto cells = vtkSmartPointer<vtkCellArray>::New();
    for (const auto &t : mesh.triangles) {
        const vtkIdType ids[3] = {t[0], t[1], t[2]};
        cells->InsertNextCell(3, ids);
    }
    auto data = vtkSmartPointer<vtkPolyData>::New();
    data->SetPoints(points);
    data->SetPolys(cells);
    data->GetPointData()->SetNormals(normals);
    m_bandedColors = banded;
    if (banded) {
        auto positions = vtkSmartPointer<vtkFloatArray>::New();
        positions->SetNumberOfComponents(1);
        positions->SetNumberOfTuples(static_cast<vtkIdType>(mesh.points.size()));
        for (std::size_t i = 0; i < mesh.scalars.size(); ++i) {
            positions->SetValue(static_cast<vtkIdType>(i), static_cast<float>(position(i)));
        }
        data->GetPointData()->SetScalars(positions);
    } else {
        data->GetPointData()->SetScalars(colors);
    }
    m_faceData = data;
    if (auto *mapper = vtkPolyDataMapper::SafeDownCast(m_faces->GetMapper())) {
        mapper->SetInputData(data);
        if (banded) {
            if (!m_bandTable) {
                m_bandTable = makeBandLookupTable();
            }
            mapper->SetColorModeToMapScalars();
            mapper->SetLookupTable(m_bandTable);
            mapper->SetScalarRange(0.0, 1.0);
            mapper->UseLookupTableScalarRangeOff();
            mapper->SetScalarModeToUsePointData();
            mapper->InterpolateScalarsBeforeMappingOn();
        } else {
            mapper->SetColorModeToDirectScalars();
            mapper->InterpolateScalarsBeforeMappingOff();
        }
        mapper->Modified();
    }

    auto edgePoints = vtkSmartPointer<vtkPoints>::New();
    edgePoints->SetNumberOfPoints(static_cast<vtkIdType>(mesh.edgePoints.size()));
    auto lines = vtkSmartPointer<vtkCellArray>::New();
    for (std::size_t i = 0; i < mesh.edgePoints.size(); ++i) {
        edgePoints->SetPoint(static_cast<vtkIdType>(i), mesh.edgePoints[i][0], mesh.edgePoints[i][1], mesh.edgePoints[i][2]);
        if (i % 2 == 1) {
            const vtkIdType ids[2] = {static_cast<vtkIdType>(i - 1), static_cast<vtkIdType>(i)};
            lines->InsertNextCell(2, ids);
        }
    }
    auto edgeData = vtkSmartPointer<vtkPolyData>::New();
    edgeData->SetPoints(edgePoints);
    edgeData->SetLines(lines);
    if (auto *mapper = vtkPolyDataMapper::SafeDownCast(m_edges->GetMapper())) {
        mapper->SetInputData(edgeData);
        mapper->Modified();
    }
}

} // namespace Structura::Visualization
