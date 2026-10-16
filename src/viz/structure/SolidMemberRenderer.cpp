#include "viz/structure/SolidMemberRenderer.h"

#include <vtkCellArray.h>
#include <vtkContourTriangulator.h>
#include <vtkFloatArray.h>
#include <vtkUnsignedCharArray.h>
#include "viz/structure/ElementAppearance.h"
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

#include <algorithm>
#include <cmath>

namespace Structura::Visualization {

namespace {

using Vec2 = std::array<double, 2>;
using Vec3 = SolidMemberRenderer::Vec3;

constexpr double kSharpAngleCos = 0.8660254037844387; // 30 graus

Vec3 add(const Vec3 &a, const Vec3 &b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 mul(const Vec3 &a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }

double signedArea(const std::vector<Vec2> &loop)
{
    double twice = 0.0;
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const Vec2 &p = loop[i];
        const Vec2 &q = loop[(i + 1) % loop.size()];
        twice += p[0] * q[1] - q[0] * p[1];
    }
    return twice / 2.0;
}

Vec2 normalized(const Vec2 &v)
{
    const double l = std::hypot(v[0], v[1]);
    return l > 0.0 ? Vec2 {v[0] / l, v[1] / l} : Vec2 {0.0, 0.0};
}

QString cacheKey(const QString &family, const SectionShapes::Parameters &parameters, bool mirrored)
{
    QString key = family + QLatin1Char('|') + (mirrored ? QLatin1Char('m') : QLatin1Char('n'));
    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        key += QLatin1Char('|') + it.key() + QLatin1Char('=') + QString::number(it.value(), 'g', 10);
    }
    return key;
}

} // namespace

SolidMemberRenderer::SolidMemberRenderer()
    : m_faceActor(vtkSmartPointer<vtkActor>::New())
    , m_edgeActor(vtkSmartPointer<vtkActor>::New())
{
    auto faceMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    faceMapper->ScalarVisibilityOn();
    faceMapper->SetColorModeToDirectScalars();
    faceMapper->SetScalarModeToUsePointData();
    m_faceActor->SetMapper(faceMapper);
    m_faceActor->GetProperty()->SetColor(0xb4 / 255.0, 0xbd / 255.0, 0xc7 / 255.0);
    m_faceActor->GetProperty()->SetAmbient(0.35);
    m_faceActor->GetProperty()->SetDiffuse(0.7);
    m_faceActor->GetProperty()->SetSpecular(0.15);
    m_faceActor->PickableOff();

    auto edgeMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    edgeMapper->ScalarVisibilityOff();
    m_edgeActor->SetMapper(edgeMapper);
    m_edgeActor->GetProperty()->SetColor(0x4a / 255.0, 0x55 / 255.0, 0x60 / 255.0);
    m_edgeActor->GetProperty()->SetLineWidth(1.2);
    m_edgeActor->GetProperty()->LightingOff();
    m_edgeActor->PickableOff();

    m_faceActor->SetVisibility(0);
    m_edgeActor->SetVisibility(0);
}

SolidMemberRenderer::~SolidMemberRenderer()
{
    if (m_renderer) {
        m_renderer->RemoveActor(m_faceActor);
        m_renderer->RemoveActor(m_edgeActor);
    }
}

void SolidMemberRenderer::attach(vtkRenderer *renderer)
{
    m_renderer = renderer;
    if (m_renderer) {
        m_renderer->AddActor(m_faceActor);
        m_renderer->AddActor(m_edgeActor);
    }
}

void SolidMemberRenderer::setVisible(bool visible)
{
    m_visible = visible;
    m_faceActor->SetVisibility(visible ? 1 : 0);
    m_edgeActor->SetVisibility(visible ? 1 : 0);
}

const SolidMemberRenderer::Geometry *SolidMemberRenderer::geometryFor(const Member &member)
{
    const QString key = cacheKey(member.family, member.parameters, member.mirrored);
    const auto found = m_cache.find(key);
    if (found != m_cache.end()) {
        return found->second.loops.empty() ? nullptr : &found->second;
    }

    Geometry geometry;
    const auto shape = SectionShapes::build(member.family, member.parameters, member.mirrored);
    if (shape) {
        for (std::size_t i = 0; i < shape->loops.size(); ++i) {
            std::vector<Vec2> loop;
            loop.reserve(shape->loops[i].size());
            for (const auto &point : shape->loops[i]) {
                loop.push_back({point[0] * 1e-3, point[1] * 1e-3});
            }
            // Contorno externo anti-horario; furos horarios (material sempre a esquerda).
            const bool counterClockwise = signedArea(loop) > 0.0;
            if ((i == 0) != counterClockwise) {
                std::reverse(loop.begin(), loop.end());
            }
            geometry.loops.push_back(std::move(loop));
        }

        for (const auto &loop : geometry.loops) {
            const std::size_t count = loop.size();
            std::vector<Vec2> segmentNormals(count);
            for (std::size_t i = 0; i < count; ++i) {
                const Vec2 &p = loop[i];
                const Vec2 &q = loop[(i + 1) % count];
                segmentNormals[i] = normalized({q[1] - p[1], -(q[0] - p[0])});
            }
            std::vector<Vec2> vertexNormals(count);
            std::vector<bool> sharp(count);
            for (std::size_t i = 0; i < count; ++i) {
                const Vec2 &previous = segmentNormals[(i + count - 1) % count];
                const Vec2 &next = segmentNormals[i];
                const double cosine = previous[0] * next[0] + previous[1] * next[1];
                sharp[i] = cosine < kSharpAngleCos;
                vertexNormals[i] = sharp[i] ? next : normalized({previous[0] + next[0], previous[1] + next[1]});
            }
            geometry.normals.push_back(std::move(vertexNormals));
            // Arestas longitudinais so em quinas mais agudas que 30 graus.
            std::vector<bool> edge(count);
            for (std::size_t i = 0; i < count; ++i) {
                const Vec2 &previous = segmentNormals[(i + count - 1) % count];
                const Vec2 &next = segmentNormals[i];
                edge[i] = previous[0] * next[0] + previous[1] * next[1] < kSharpAngleCos;
            }
            geometry.sharp.push_back(std::move(edge));
        }

        // Tampas: triangulacao dos contornos (com furos) uma unica vez por secao.
        auto points = vtkSmartPointer<vtkPoints>::New();
        points->SetDataTypeToDouble();
        auto lines = vtkSmartPointer<vtkCellArray>::New();
        for (const auto &loop : geometry.loops) {
            const vtkIdType first = points->GetNumberOfPoints();
            for (const Vec2 &p : loop) {
                points->InsertNextPoint(p[0], p[1], 0.0);
            }
            lines->InsertNextCell(static_cast<vtkIdType>(loop.size() + 1));
            for (std::size_t i = 0; i < loop.size(); ++i) {
                lines->InsertCellPoint(first + static_cast<vtkIdType>(i));
            }
            lines->InsertCellPoint(first);
        }
        auto outline = vtkSmartPointer<vtkPolyData>::New();
        outline->SetPoints(points);
        outline->SetLines(lines);
        auto triangulator = vtkSmartPointer<vtkContourTriangulator>::New();
        triangulator->SetInputData(outline);
        triangulator->Update();
        vtkPolyData *triangles = triangulator->GetOutput();
        vtkCellArray *polys = triangles->GetPolys();
        vtkIdType npts = 0;
        const vtkIdType *ids = nullptr;
        polys->InitTraversal();
        while (polys->GetNextCell(npts, ids)) {
            if (npts != 3) {
                continue;
            }
            std::array<Vec2, 3> triangle;
            for (int k = 0; k < 3; ++k) {
                double p[3];
                triangles->GetPoint(ids[k], p);
                triangle[static_cast<std::size_t>(k)] = {p[0], p[1]};
            }
            geometry.capTriangles.push_back(triangle);
        }
    }

    const auto inserted = m_cache.emplace(key, std::move(geometry));
    return inserted.first->second.loops.empty() ? nullptr : &inserted.first->second;
}

void SolidMemberRenderer::setMembers(const std::vector<Member> &members)
{
    rebuildActors(members);
}

void SolidMemberRenderer::rebuildActors(const std::vector<Member> &members)
{
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetDataTypeToDouble();
    auto normals = vtkSmartPointer<vtkFloatArray>::New();
    normals->SetNumberOfComponents(3);
    normals->SetName("Normals");
    auto polys = vtkSmartPointer<vtkCellArray>::New();
    auto colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
    colors->SetNumberOfComponents(3);
    colors->SetName("SolidColors");

    auto edgePoints = vtkSmartPointer<vtkPoints>::New();
    edgePoints->SetDataTypeToDouble();
    auto edgeLines = vtkSmartPointer<vtkCellArray>::New();

    for (const Member &member : members) {
        const Geometry *geometry = geometryFor(member);
        if (!geometry) {
            continue;
        }
        std::array<unsigned char, 3> memberColor = member.color;
        if (member.dimmed) {
            memberColor = Structura::Visualization::dimColor(memberColor);
        }
        const std::size_t pointsBefore = static_cast<std::size_t>(points->GetNumberOfPoints());
        auto position = [&](const Vec3 &origin, const Vec2 &p) {
            return add(origin, add(mul(member.zAxis, p[0]), mul(member.yAxis, p[1])));
        };
        auto direction = [&](const Vec2 &n) { return add(mul(member.zAxis, n[0]), mul(member.yAxis, n[1])); };

        for (std::size_t li = 0; li < geometry->loops.size(); ++li) {
            const auto &loop = geometry->loops[li];
            const auto &vertexNormals = geometry->normals[li];
            const std::size_t count = loop.size();

            // Faces laterais (quadrilateros) com normais suavizadas nas curvas.
            for (std::size_t i = 0; i < count; ++i) {
                const std::size_t next = (i + 1) % count;
                const Vec2 &p0 = loop[i];
                const Vec2 &p1 = loop[next];
                const Vec2 segmentNormal = normalized({p1[1] - p0[1], -(p1[0] - p0[0])});
                const Vec2 n0 = geometry->sharp[li][i] || vertexNormals[i] == Vec2 {0, 0} ? segmentNormal : vertexNormals[i];
                // A normal do vertice final usa o suavizado do proximo vertice se ele nao for quina.
                const bool nextSharp = std::hypot(vertexNormals[next][0] - segmentNormal[0], vertexNormals[next][1] - segmentNormal[1]) > 0.5176;
                const Vec2 n1 = nextSharp ? segmentNormal : vertexNormals[next];

                const vtkIdType base = points->GetNumberOfPoints();
                const Vec3 a0 = position(member.start, p0);
                const Vec3 a1 = position(member.start, p1);
                const Vec3 b1 = position(member.end, p1);
                const Vec3 b0 = position(member.end, p0);
                // Ordem a0, b0, b1, a1: o enrolamento fica coerente com a normal externa.
                for (const Vec3 &v : {a0, b0, b1, a1}) {
                    points->InsertNextPoint(v[0], v[1], v[2]);
                }
                const Vec3 d0 = direction(n0);
                const Vec3 d1 = direction(n1);
                for (const Vec3 &d : {d0, d0, d1, d1}) {
                    normals->InsertNextTuple3(d[0], d[1], d[2]);
                }
                polys->InsertNextCell(4);
                for (int k = 0; k < 4; ++k) {
                    polys->InsertCellPoint(base + k);
                }
            }

            // Arestas: contornos nas duas extremidades e linhas nas quinas.
            for (const Vec3 &origin : {member.start, member.end}) {
                const vtkIdType first = edgePoints->GetNumberOfPoints();
                for (const Vec2 &p : loop) {
                    const Vec3 v = position(origin, p);
                    edgePoints->InsertNextPoint(v[0], v[1], v[2]);
                }
                edgeLines->InsertNextCell(static_cast<vtkIdType>(count + 1));
                for (std::size_t i = 0; i < count; ++i) {
                    edgeLines->InsertCellPoint(first + static_cast<vtkIdType>(i));
                }
                edgeLines->InsertCellPoint(first);
            }
            for (std::size_t i = 0; i < count; ++i) {
                if (!geometry->sharp[li][i]) {
                    continue;
                }
                const Vec3 a = position(member.start, loop[i]);
                const Vec3 b = position(member.end, loop[i]);
                const vtkIdType base = edgePoints->GetNumberOfPoints();
                edgePoints->InsertNextPoint(a[0], a[1], a[2]);
                edgePoints->InsertNextPoint(b[0], b[1], b[2]);
                edgeLines->InsertNextCell(2);
                edgeLines->InsertCellPoint(base);
                edgeLines->InsertCellPoint(base + 1);
            }
        }

        // Tampas nas duas extremidades.
        for (int end = 0; end < 2; ++end) {
            const Vec3 &origin = end == 0 ? member.start : member.end;
            const Vec3 capNormal = end == 0 ? mul(member.xAxis, -1.0) : member.xAxis;
            for (auto triangle : geometry->capTriangles) {
                // Orientacao 2D (a,b): positiva => normal geometrica -x; a extremidade final precisa de +x.
                const double orientation = (triangle[1][0] - triangle[0][0]) * (triangle[2][1] - triangle[0][1])
                                         - (triangle[1][1] - triangle[0][1]) * (triangle[2][0] - triangle[0][0]);
                if ((end == 0 && orientation < 0.0) || (end == 1 && orientation > 0.0)) {
                    std::swap(triangle[1], triangle[2]);
                }
                const vtkIdType base = points->GetNumberOfPoints();
                for (const Vec2 &p : triangle) {
                    const Vec3 v = position(origin, p);
                    points->InsertNextPoint(v[0], v[1], v[2]);
                    normals->InsertNextTuple3(capNormal[0], capNormal[1], capNormal[2]);
                }
                polys->InsertNextCell(3);
                polys->InsertCellPoint(base);
                polys->InsertCellPoint(base + 1);
                polys->InsertCellPoint(base + 2);
            }
        }

        for (vtkIdType i = static_cast<vtkIdType>(pointsBefore); i < points->GetNumberOfPoints(); ++i) {
            colors->InsertNextTuple3(memberColor[0], memberColor[1], memberColor[2]);
        }
    }

    auto faces = vtkSmartPointer<vtkPolyData>::New();
    faces->SetPoints(points);
    faces->GetPointData()->SetScalars(colors);
    faces->SetPolys(polys);
    faces->GetPointData()->SetNormals(normals);
    auto edges = vtkSmartPointer<vtkPolyData>::New();
    edges->SetPoints(edgePoints);
    edges->SetLines(edgeLines);

    static_cast<vtkPolyDataMapper *>(m_faceActor->GetMapper())->SetInputData(faces);
    static_cast<vtkPolyDataMapper *>(m_edgeActor->GetMapper())->SetInputData(edges);
    m_faceActor->GetMapper()->Modified();
    m_edgeActor->GetMapper()->Modified();
}

} // namespace Structura::Visualization
