// Fase 1: valores escalares do RenderMesh, interpolacao Q4 do probe, legenda de cores e picking de cascas na vista principal.
#include <QtTest/QtTest>

#include "shell/ShellMesh.h"
#include "shell/ShellModel.h"
#include "shell/ShellProbe.h"
#include "shell/ShellResults.h"
#include "shell/ShellVisual.h"
#include "ui/ColorLegendOverlay.h"
#include "viz/structure/ShellRenderer.h"

#include <vtkCamera.h>
#include <vtkNew.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>

#include <cmath>
#include <limits>

using namespace Structura::Shell;

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// Placa retangular 2 x 1 m, malha 0,5 m (plano XY), com modelo de um objeto "Laje".
struct Fixture {
    Model model;
    Mesh mesh;
    CaseResult result;
    Fixture()
    {
        Object o;
        o.id = 7;
        o.name = "Laje";
        o.kind = Kind::Plate;
        o.polygon = {{0, 0, 0}, {2, 0, 0}, {2, 1, 0}, {0, 1, 0}};
        o.p = {{"t", 0.2}};
        o.meshSize = 0.5;
        model.objects.push_back(o);
        mesh = generateMesh(model);
        Q_ASSERT(!mesh.empty());
        result.displacement.assign(mesh.nodes.size(), std::array<double, 6> {});
        result.reaction.assign(mesh.nodes.size(), std::array<double, 6> {});
        result.elements.assign(mesh.quads.size(), ElementState {});
    }
    /// uz(x, y) = f nos nos (campo nodal); ux = 0
    template <class F> void setUz(F f)
    {
        for (std::size_t n = 0; n < mesh.nodes.size(); ++n) {
            result.displacement[n][2] = f(mesh.nodes[n][0], mesh.nodes[n][1], mesh.nodes[n][2]);
        }
    }
    RenderMesh build(Quantity q = Quantity::Uz, bool smooth = true, double upm = 1.0, double deform = 0.0) const
    {
        RenderOptions opt;
        opt.quantity = q;
        opt.smooth = smooth;
        opt.unitsPerMeter = upm;
        opt.deformScale = deform;
        return buildRenderMesh(mesh, model, &result, opt);
    }
};

std::array<std::array<double, 3>, 4> unitSquare()
{
    return {{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}};
}

std::array<double, 3> mapPoint(const std::array<std::array<double, 3>, 4> &c, double xi, double eta)
{
    const auto n = q4Shape(xi, eta);
    std::array<double, 3> p {};
    for (std::size_t i = 0; i < 4; ++i) {
        for (std::size_t k = 0; k < 3; ++k) {
            p[k] += n[i] * c[i][k];
        }
    }
    return p;
}

} // namespace

class TestShellProbe : public QObject
{
    Q_OBJECT

private slots:
    // ---------------------------------------------------------------- funcoes de forma e interpolacao Q4
    void shapeFunctionsFormPartitionOfUnityAndAreNodal()
    {
        for (double xi : {-1.0, -0.3, 0.0, 0.77, 1.0}) {
            for (double eta : {-1.0, 0.2, 1.0}) {
                const auto n = q4Shape(xi, eta);
                QVERIFY(std::abs(n[0] + n[1] + n[2] + n[3] - 1.0) < 1e-14);
            }
        }
        const double cx[4] = {-1, 1, 1, -1}, cy[4] = {-1, -1, 1, 1};
        for (int i = 0; i < 4; ++i) {
            const auto n = q4Shape(cx[i], cy[i]);
            for (int j = 0; j < 4; ++j) {
                QVERIFY(std::abs(n[static_cast<std::size_t>(j)] - (i == j ? 1.0 : 0.0)) < 1e-15);
            }
        }
    }

    void interpolationAtCenterCornersEdgesAndInside()
    {
        const std::array<double, 4> v {1.0, 3.0, 7.0, 5.0};
        QCOMPARE(q4Interpolate(v, 0.0, 0.0), 4.0);                      // centro = media dos 4 cantos
        QCOMPARE(q4Interpolate(v, -1.0, -1.0), 1.0);                    // cantos
        QCOMPARE(q4Interpolate(v, 1.0, -1.0), 3.0);
        QCOMPARE(q4Interpolate(v, 1.0, 1.0), 7.0);
        QCOMPARE(q4Interpolate(v, -1.0, 1.0), 5.0);
        QVERIFY(std::abs(q4Interpolate(v, 0.0, -1.0) - 2.0) < 1e-14);   // meio das arestas
        QVERIFY(std::abs(q4Interpolate(v, 1.0, 0.0) - 5.0) < 1e-14);
        // solucao analitica: N1 v1 + ... em (0,5; -0,25)
        const auto n = q4Shape(0.5, -0.25);
        QVERIFY(std::abs(q4Interpolate(v, 0.5, -0.25) - (n[0] * 1 + n[1] * 3 + n[2] * 7 + n[3] * 5)) < 1e-14);
    }

    void interpolationIsExactForLinearFieldOnDistortedWarpedQuad()
    {
        // quadrilatero distorcido e empenado (isoparametrico reproduz campos lineares nas coordenadas fisicas)
        const std::array<std::array<double, 3>, 4> c {{{0.0, 0.0, 0.0}, {2.0, 0.3, 0.1}, {2.4, 1.6, -0.1}, {-0.2, 1.1, 0.05}}};
        auto field = [](const std::array<double, 3> &p) { return 1.5 + 2.0 * p[0] - 3.0 * p[1] + 0.5 * p[2]; };
        std::array<double, 4> nodal;
        for (std::size_t i = 0; i < 4; ++i) {
            nodal[i] = field(c[i]);
        }
        for (double xi : {-0.9, -0.2, 0.4, 0.95}) {
            for (double eta : {-0.8, 0.0, 0.7}) {
                QVERIFY(std::abs(q4Interpolate(nodal, xi, eta) - field(mapPoint(c, xi, eta))) < 1e-12);
            }
        }
    }

    void invalidNodalValueNeverBecomesAValidInterpolation()
    {
        const std::array<double, 4> v {1.0, kNaN, 7.0, 5.0};
        QVERIFY(std::isnan(q4Interpolate(v, 0.0, 0.0)));
        QVERIFY(std::isnan(q4Interpolate(v, 0.9, -0.9)));
        QCOMPARE(q4Interpolate(v, -1.0, -1.0), 1.0);   // exatamente sobre outro canto: o no invalido nao influi
        QVERIFY(std::isnan(q4Interpolate({1.0, 2.0, std::numeric_limits<double>::infinity(), 4.0}, 0.2, 0.2)));
    }

    // ---------------------------------------------------------------- coordenadas naturais
    void naturalCoordinatesRoundTripOnRectangleDistortedAndWarpedQuads()
    {
        const std::array<std::array<std::array<double, 3>, 4>, 3> quads {{
            unitSquare(),
            {{{0, 0, 0}, {3, 0.2, 0}, {2.5, 1.5, 0}, {-0.4, 1.0, 0}}},
            {{{0, 0, 0}, {1, 0, 0.08}, {1, 1, 0}, {0, 1, 0.08}}},   // empenado
        }};
        for (const auto &q : quads) {
            for (double xi : {-1.0, -0.6, 0.0, 0.31, 0.999999, 1.0}) {
                for (double eta : {-1.0, -0.25, 0.5, 1.0}) {
                    const auto p = mapPoint(q, xi, eta);
                    double x = 9, y = 9, d = 9;
                    bool outside = true;
                    QVERIFY(q4NaturalCoordinates(q, p, x, y, &d, &outside));
                    QVERIFY2(std::abs(x - xi) < 1e-9 && std::abs(y - eta) < 1e-9, "coordenadas naturais nao reproduzidas");
                    QVERIFY(d < 1e-10);
                    QVERIFY(!outside);
                }
            }
        }
    }

    void naturalCoordinatesNearAndBeyondTheEdges()
    {
        const auto q = unitSquare();
        double x = 0, y = 0, d = 0;
        bool outside = false;
        // um pouco alem da aresta: limitado ao elemento e marcado
        QVERIFY(q4NaturalCoordinates(q, {1.2, 0.5, 0.0}, x, y, &d, &outside));
        QVERIFY(outside);
        QCOMPARE(x, 1.0);
        QVERIFY(std::abs(y) < 1e-12);
        QVERIFY(std::abs(d - 0.2) < 1e-12);
        // exatamente na aresta nao e "fora"
        QVERIFY(q4NaturalCoordinates(q, {1.0, 0.25, 0.0}, x, y, &d, &outside));
        QVERIFY(!outside);
        // ponto acima da superficie: distancia ortogonal
        QVERIFY(q4NaturalCoordinates(q, {0.5, 0.5, 0.3}, x, y, &d, &outside));
        QVERIFY(std::abs(d - 0.3) < 1e-12);
        QVERIFY(std::abs(x) < 1e-12 && std::abs(y) < 1e-12);
    }

    void naturalCoordinatesRejectDegenerateAndNonFiniteInput()
    {
        double x, y;
        QVERIFY(!q4NaturalCoordinates({{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}}, {0, 0, 0}, x, y));
        QVERIFY(!q4NaturalCoordinates(unitSquare(), {kNaN, 0, 0}, x, y));
        auto bad = unitSquare();
        bad[2][1] = kNaN;
        QVERIFY(!q4NaturalCoordinates(bad, {0.5, 0.5, 0}, x, y));
    }

    // ---------------------------------------------------------------- valores escalares no RenderMesh
    void scalarsMatchVerticesAndColors()
    {
        Fixture f;
        f.setUz([](double x, double y, double) { return 1e-3 * (x - 2.0 * y); });
        const RenderMesh m = f.build();
        QVERIFY(m.hasValues);
        QVERIFY(m.consistent());
        QCOMPARE(m.scalars.size(), m.points.size());
        QCOMPARE(m.solverScalars.size(), m.points.size());
        QCOMPARE(m.colors.size(), m.points.size());
        QCOMPARE(m.quadElement.size(), f.mesh.quads.size());
        QCOMPARE(m.quadObject.size(), f.mesh.quads.size());
        QCOMPARE(m.triangles.size(), f.mesh.quads.size() * 2);
        QCOMPARE(m.title, std::string("Deslocamento Z"));
        QVERIFY(!m.unit.empty());
        QVERIFY(m.minValue < 0.0 && m.maxValue > 0.0);   // positivos e negativos
        for (double v : m.scalars) {
            QVERIFY(std::isfinite(v));
            QVERIFY(v >= m.minValue - 1e-12 && v <= m.maxValue + 1e-12);
        }
        QCOMPARE(m.objectIds.size(), std::size_t {1});
        QCOMPARE(m.objectIds[0], 7);
        QCOMPARE(m.objectNames[0], std::string("Laje"));
    }

    void withoutResultsThereAreNoScalars()
    {
        Fixture f;
        RenderOptions opt;
        const RenderMesh m = buildRenderMesh(f.mesh, f.model, nullptr, opt);
        QVERIFY(!m.hasValues);
        QVERIFY(m.scalars.empty() && m.solverScalars.empty());
        QVERIFY(m.consistent());
        QVERIFY(!m.points.empty());
        // resultado de outro tamanho (modelo editado depois da analise): tratado como ausente
        CaseResult stale;
        stale.displacement.assign(3, {});
        const RenderMesh s = buildRenderMesh(f.mesh, f.model, &stale, opt);
        QVERIFY(!s.hasValues);
    }

    void constantAndNearlyConstantFieldsGiveAMidScaleColor()
    {
        Fixture f;
        f.setUz([](double, double, double) { return 2.5e-3; });
        RenderMesh m = f.build();
        QVERIFY(m.hasValues);
        QCOMPARE(m.minValue, m.maxValue);
        QCOMPARE(m.colors.front(), rainbow(0.5));
        // ruido de arredondamento nao e ampliado ate o fundo de escala
        int k = 0;
        f.setUz([&k](double, double, double) { return 2.5e-3 * (1.0 + 1e-13 * (k++ % 3)); });
        m = f.build();
        QVERIFY(m.maxValue > m.minValue);
        for (const auto &c : m.colors) {
            QCOMPARE(c, rainbow(0.5));
        }
        // diferenca real, mesmo pequena em valor absoluto, e colorida
        f.setUz([](double x, double, double) { return 1e-9 * x; });
        m = f.build();
        QVERIFY(m.colors.front() != m.colors.back());
        QCOMPARE(colorPosition(5.0, 5.0, 5.0), 0.5);
        QCOMPARE(colorPosition(0.0, -1.0, 1.0), 0.5);
        QCOMPARE(colorPosition(1.0, 0.0, 1.0), 1.0);
    }

    void nanAndInfiniteValuesAreInvalidNotPoisoningTheRange()
    {
        Fixture f;
        f.setUz([](double x, double, double) { return 1e-3 * x; });
        f.result.displacement[0][2] = kNaN;
        f.result.displacement[1][2] = std::numeric_limits<double>::infinity();
        const RenderMesh m = f.build();
        QVERIFY(m.hasValues);
        QVERIFY(std::isfinite(m.minValue) && std::isfinite(m.maxValue));
        int invalid = 0;
        for (std::size_t i = 0; i < m.scalars.size(); ++i) {
            if (!std::isfinite(m.scalars[i])) {
                ++invalid;
                QCOMPARE(m.colors[i], kInvalidValueColor);
            }
        }
        QVERIFY(invalid > 0);
        QVERIFY(m.consistent());
        // todos invalidos: malha sem resultados (nada de legenda)
        for (auto &d : f.result.displacement) {
            d[2] = kNaN;
        }
        const RenderMesh all = f.build();
        QVERIFY(!all.hasValues);
        QVERIFY(all.consistent());
    }

    void smoothingKeepsTheSolverValuesOfEachElement()
    {
        Fixture f;
        // membrana nxx diferente em cada elemento: descontinuidade entre elementos
        for (std::size_t e = 0; e < f.result.elements.size(); ++e) {
            f.result.elements[e].axes = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            for (int p = 0; p < 5; ++p) {
                f.result.elements[e].v[static_cast<std::size_t>(p * 8)] = 1.0e5 * static_cast<double>(e + 1);
            }
        }
        const RenderMesh smooth = f.build(Quantity::MembraneA, true);
        const RenderMesh raw = f.build(Quantity::MembraneA, false);
        QVERIFY(smooth.smoothed && !raw.smoothed);
        QCOMPARE(smooth.geometryRevision, raw.geometryRevision);
        bool differs = false;
        for (std::size_t i = 0; i < smooth.scalars.size(); ++i) {
            QCOMPARE(smooth.solverScalars[i], raw.scalars[i]);   // o valor do solver e o mesmo, alisado ou nao
            QCOMPARE(raw.solverScalars[i], raw.scalars[i]);
            differs = differs || std::abs(smooth.scalars[i] - smooth.solverScalars[i]) > 1.0;
        }
        QVERIFY(differs);   // o alisamento realmente muda o valor exibido nos nos compartilhados
        // e o solver nao foi tocado
        const QuantityInfo &info = quantityInfo(Quantity::MembraneA);
        for (std::size_t e = 0; e < f.mesh.quads.size(); ++e) {
            for (int c = 0; c < 4; ++c) {
                QCOMPARE(smooth.solverScalars[e * 4 + static_cast<std::size_t>(c)], elementQuantity(f.mesh, f.model, f.result, e, c, Quantity::MembraneA) * info.scale);
            }
        }
    }

    void changingOnlyTheQuantityKeepsTheGeometryRevision()
    {
        Fixture f;
        f.setUz([](double x, double, double) { return 1e-3 * x; });
        const RenderMesh a = f.build(Quantity::Uz);
        const RenderMesh b = f.build(Quantity::DisplacementMagnitude);
        QVERIFY(a.title != b.title);
        QCOMPARE(a.geometryRevision, b.geometryRevision);
        QVERIFY(a.geometryRevision != 0);
        const RenderMesh scaled = f.build(Quantity::Uz, true, 1000.0);
        QVERIFY(scaled.geometryRevision != a.geometryRevision);
        const RenderMesh deformed = f.build(Quantity::Uz, true, 1.0, 100.0);
        QVERIFY(deformed.geometryRevision != a.geometryRevision);
        QCOMPARE(deformed.restPoints.size(), deformed.points.size());
        QVERIFY(a.restPoints.empty());
    }

    void customLabelsOverrideTheQuantityNames()
    {
        Fixture f;
        std::vector<double> util(f.mesh.quads.size(), 0.5);
        RenderOptions opt;
        opt.elementScalars = &util;
        opt.overrideLabels = true;
        opt.title = "Razão de utilização";
        const RenderMesh m = buildRenderMesh(f.mesh, f.model, &f.result, opt);
        QVERIFY(m.hasValues);
        QCOMPARE(m.title, std::string("Razão de utilização"));
        QVERIFY(m.unit.empty());
        QVERIFY(!m.smoothed);
    }

    // ---------------------------------------------------------------- consulta (probe)
    void probeReturnsElementObjectCoordinatesAndInterpolatedValue()
    {
        Fixture f;
        const double upm = 1000.0;   // cena em mm
        f.setUz([](double x, double y, double) { return 1e-3 * (2.0 * x + 3.0 * y); });
        const RenderMesh m = f.build(Quantity::Uz, false, upm);
        QVERIFY(m.hasValues);
        const double scale = quantityInfo(Quantity::Uz).scale;
        // pontos arbitrarios, no interior de varios elementos
        for (const std::array<double, 2> pt : {std::array<double, 2> {0.13, 0.07}, {1.01, 0.62}, {1.77, 0.93}, {0.5, 0.5}}) {
            // quadrilatero que contem o ponto (malha regular: busca)
            int found = -1;
            for (std::size_t e = 0; e < f.mesh.quads.size(); ++e) {
                const auto &q = f.mesh.quads[e];
                const auto &a = f.mesh.nodes[static_cast<std::size_t>(q.n[0])];
                const auto &c = f.mesh.nodes[static_cast<std::size_t>(q.n[2])];
                if (pt[0] >= std::min(a[0], c[0]) - 1e-12 && pt[0] <= std::max(a[0], c[0]) + 1e-12 && pt[1] >= std::min(a[1], c[1]) - 1e-12 && pt[1] <= std::max(a[1], c[1]) + 1e-12) {
                    found = static_cast<int>(e);
                    break;
                }
            }
            QVERIFY(found >= 0);
            const ProbeHit hit = probeQuad(m, found, {pt[0] * upm, pt[1] * upm, 0.0});
            QVERIFY(hit.valid && hit.hasValue);
            QCOMPARE(hit.element, found);
            QCOMPARE(hit.objectId, 7);
            QCOMPARE(hit.objectName, std::string("Laje"));
            QVERIFY(std::abs(hit.position[0] - pt[0]) < 1e-9 && std::abs(hit.position[1] - pt[1]) < 1e-9);   // em metros, apesar da cena em mm
            QVERIFY(std::abs(hit.value - 1e-3 * (2.0 * pt[0] + 3.0 * pt[1]) * scale) < 1e-9 * std::max(1.0, std::abs(hit.value)));
            QVERIFY(!hit.outside);
        }
    }

    void probeAtCornersAndCenterMatchesTheVertexValues()
    {
        Fixture f;
        f.setUz([](double x, double y, double) { return 1e-3 * (x * x + y); });   // nao linear: so os nos sao exatos
        const RenderMesh m = f.build(Quantity::Uz, false);
        const std::size_t e = 3;
        std::array<std::array<double, 3>, 4> c {};
        double mean = 0.0;
        for (std::size_t i = 0; i < 4; ++i) {
            for (std::size_t k = 0; k < 3; ++k) {
                c[i][k] = m.points[e * 4 + i][k];
            }
            mean += m.scalars[e * 4 + i] / 4.0;
        }
        for (std::size_t i = 0; i < 4; ++i) {
            const ProbeHit h = probeQuad(m, static_cast<int>(e), c[i]);
            QVERIFY(h.valid && h.hasValue);
            QVERIFY(std::abs(h.value - m.scalars[e * 4 + i]) < 1e-9 * std::max(1.0, std::abs(m.scalars[e * 4 + i])));
        }
        const ProbeHit center = probeQuad(m, static_cast<int>(e), mapPoint(c, 0.0, 0.0));
        QVERIFY(std::abs(center.value - mean) < 1e-9 * std::max(1.0, std::abs(mean)));
    }

    void probeDistinguishesDisplayedAndSolverValues()
    {
        Fixture f;
        for (std::size_t e = 0; e < f.result.elements.size(); ++e) {
            f.result.elements[e].axes = {1, 0, 0, 0, 1, 0, 0, 0, 1};
            for (int p = 0; p < 5; ++p) {
                f.result.elements[e].v[static_cast<std::size_t>(p * 8)] = 1.0e5 * static_cast<double>(e + 1);
            }
        }
        const RenderMesh m = f.build(Quantity::MembraneA, true);
        QVERIFY(m.smoothed);
        // no no compartilhado entre elementos o valor exibido e a media; o do elemento permanece o do solver
        std::size_t e = 5;
        std::array<double, 3> corner {};
        for (std::size_t k = 0; k < 3; ++k) {
            corner[k] = m.points[e * 4 + 2][k];
        }
        const ProbeHit h = probeQuad(m, static_cast<int>(e), corner);
        QVERIFY(h.hasValue && h.hasSolverValue && h.smoothed);
        QVERIFY(std::abs(h.value - h.solverValue) > 1.0);
        QVERIFY(std::abs(h.solverValue - m.solverScalars[e * 4 + 2]) < 1e-6);
    }

    void probeOnDeformedViewReportsUndeformedCoordinates()
    {
        Fixture f;
        f.setUz([](double, double, double) { return 0.01; });
        const RenderMesh m = f.build(Quantity::Uz, false, 1.0, 10.0);   // deformada: z + 0,1
        QVERIFY(m.restPoints.size() == m.points.size());
        const ProbeHit h = probeQuad(m, 0, {m.points[0][0] + 0.1, m.points[0][1] + 0.1, m.points[0][2] + 1e-3});
        QVERIFY(h.valid && h.deformed);
        QVERIFY(std::abs(h.position[2]) < 1e-9);            // sem deformada
        QVERIFY(std::abs(h.viewPosition[2] - 0.1) < 1e-6);  // na vista
    }

    void probeHandlesInvalidAndMissingData()
    {
        Fixture f;
        f.setUz([](double x, double, double) { return 1e-3 * x; });
        f.result.displacement[static_cast<std::size_t>(f.mesh.quads[0].n[1])][2] = kNaN;
        const RenderMesh m = f.build();
        const std::size_t e = 0;
        std::array<std::array<double, 3>, 4> c {};
        for (std::size_t i = 0; i < 4; ++i) {
            for (std::size_t k = 0; k < 3; ++k) {
                c[i][k] = m.points[e * 4 + i][k];
            }
        }
        const ProbeHit bad = probeQuad(m, 0, mapPoint(c, 0.3, 0.3));
        QVERIFY(bad.valid);
        QVERIFY(!bad.hasValue);   // um canto invalido: sem valor, sem inventar
        const ProbeHit atGood = probeQuad(m, 0, c[0]);
        QVERIFY(atGood.hasValue);
        // quadrilatero inexistente
        QVERIFY(!probeQuad(m, -1, {0, 0, 0}).valid);
        QVERIFY(!probeQuad(m, 100000, {0, 0, 0}).valid);
        // malha sem resultados: geometria resolvida, sem valor
        RenderOptions opt;
        const RenderMesh plain = buildRenderMesh(f.mesh, f.model, nullptr, opt);
        const ProbeHit nores = probeQuad(plain, 0, mapPoint(c, 0, 0));
        QVERIFY(nores.valid && !nores.hasValue);
        QVERIFY(!describeProbe(nores).empty());
        QVERIFY(!describeProbe(ProbeHit {}).empty());
    }

    void probeOnADistortedMeshElement()
    {
        // um unico elemento distorcido (angulos de 60 a 120 graus): campo linear reproduzido
        Model model;
        Object o;
        o.id = 1;
        o.name = "Chapa";
        model.objects.push_back(o);
        Mesh mesh;
        mesh.nodes = {{0, 0, 0}, {2, 0, 0}, {2.7, 1.2, 0}, {0.5, 1.0, 0}};
        MeshQuad q;
        q.n = {{0, 1, 2, 3}};
        q.thickness = 0.01;
        mesh.quads.push_back(q);
        mesh.zones = {"z"};
        CaseResult r;
        r.displacement.assign(4, {});
        for (std::size_t n = 0; n < 4; ++n) {
            r.displacement[n][2] = 1e-3 * (1.0 + 2.0 * mesh.nodes[n][0] - mesh.nodes[n][1]);
        }
        r.elements.assign(1, ElementState {});
        RenderOptions opt;
        opt.quantity = Quantity::Uz;
        opt.smooth = false;
        const RenderMesh m = buildRenderMesh(mesh, model, &r, opt);
        const std::array<double, 3> p {1.4, 0.55, 0.0};
        const ProbeHit h = probeQuad(m, 0, p);
        QVERIFY(h.valid && h.hasValue);
        const double expected = 1e-3 * (1.0 + 2.0 * p[0] - p[1]) * quantityInfo(Quantity::Uz).scale;
        // vertices do RenderMesh sao float (~1e-7 relativo): a tolerancia acompanha essa precisao de armazenamento
        QVERIFY(std::abs(h.value - expected) < 1e-6 * std::abs(expected));
    }

    // ---------------------------------------------------------------- escala em faixas (estilo Mecway)
    void colorBandsAreHardSteps()
    {
        using namespace Structura::Shell;
        QCOMPARE(colorBand(0.0), 0);
        QCOMPARE(colorBand(1.0), kColorBands - 1);          // o maximo cai na ultima faixa
        QCOMPARE(colorBand(0.0999999), 0);
        QCOMPARE(colorBand(0.1000001), 1);
        QCOMPARE(colorBand(-3.0), 0);
        QCOMPARE(colorBand(7.0), kColorBands - 1);
        QCOMPARE(colorBand(std::numeric_limits<double>::quiet_NaN()), 0);
        // dentro de uma faixa a cor e constante; faixas vizinhas tem cores diferentes; extremos azul e vermelho
        for (int b = 0; b < kColorBands; ++b) {
            const double lo = static_cast<double>(b) / kColorBands, hi = static_cast<double>(b + 1) / kColorBands;
            QCOMPARE(bandColor(lo + 1e-9), bandColor(hi - 1e-9));
            if (b + 1 < kColorBands) {
                QVERIFY(bandColor(hi - 1e-9) != bandColor(hi + 1e-9));
            }
        }
        QCOMPARE(bandColor(0.0), rainbow(0.0));
        QCOMPARE(bandColor(1.0), rainbow(1.0));
        // a legenda desenha os mesmos degraus: a barra tem exatamente kColorBands cores distintas
        QWidget host;
        host.resize(900, 600);
        Structura::UI::ColorLegendOverlay legend(&host);
        host.show();
        legend.setLegend(QStringLiteral("g"), QStringLiteral("u"), 0.0, 1.0, false, false);
        const QImage image = legend.grab().toImage();
        const int x = 9 + 8;   // meio da barra de cores (kPad 9, largura 16)
        QMap<QRgb, int> rows;   // linhas da barra por cor de faixa
        QSet<QRgb> expected;
        for (int b = 0; b < kColorBands; ++b) {
            const auto c = bandColor((b + 0.5) / kColorBands);
            expected.insert(qRgb(c[0], c[1], c[2]));
        }
        for (int y = 0; y < image.height(); ++y) {
            const QRgb c = image.pixel(x, y) | 0xff000000u;
            if (expected.contains(c)) {
                ++rows[c];
            }
        }
        QCOMPARE(rows.size(), kColorBands);   // as dez faixas aparecem, cada uma como um degrau (nenhuma cor intermediaria entre elas)
        for (auto it = rows.constBegin(); it != rows.constEnd(); ++it) {
            QVERIFY2(it.value() >= 5, "faixa muito estreita na legenda");
        }
    }

    // ---------------------------------------------------------------- legenda de cores
    void legendShowsTitleUnitAndRange()
    {
        QWidget host;
        host.resize(900, 600);
        Structura::UI::ColorLegendOverlay legend(&host);
        QVERIFY(!legend.hasLegend());
        QVERIFY(legend.isHidden());
        host.show();
        legend.setLegend(QStringLiteral("Momento Mxx"), QStringLiteral("kN·m/m"), -12.5, 40.0, false, true);
        QVERIFY(legend.hasLegend());
        QVERIFY(legend.isVisible());
        QCOMPARE(legend.title(), QStringLiteral("Momento Mxx"));
        QCOMPARE(legend.unit(), QStringLiteral("kN·m/m"));
        const QStringList labels = legend.tickLabels();
        QCOMPARE(labels.size(), Structura::Shell::kColorBands + 1);   // um rotulo por fronteira de faixa (estilo Mecway)
        QCOMPARE(labels.first(), QStringLiteral("40"));      // maximo no topo
        QCOMPARE(labels.last(), QStringLiteral("-12.5"));    // minimo na base
        QVERIFY(legend.geometry().right() <= host.width());
        QVERIFY(legend.geometry().bottom() <= host.height());
        QVERIFY(!legend.grab().isNull());
        // transparente ao mouse
        QVERIFY(legend.testAttribute(Qt::WA_TransparentForMouseEvents));
    }

    void legendUpdatesFixedRangeAndHidesWithoutResults()
    {
        QWidget host;
        host.resize(700, 420);
        host.show();
        Structura::UI::ColorLegendOverlay legend(&host);
        legend.setLegend(QStringLiteral("A"), QString(), 0.0, 1.0, false, false);
        QVERIFY(!legend.fixedRange());
        legend.setLegend(QStringLiteral("Deslocamento Uz"), QStringLiteral("mm"), -3.0, 3.0, true, false);   // outra grandeza, faixa fixa
        QVERIFY(legend.fixedRange());
        QCOMPARE(legend.title(), QStringLiteral("Deslocamento Uz"));
        QCOMPARE(legend.minValue(), -3.0);
        QCOMPARE(legend.maxValue(), 3.0);
        legend.setLegend(QStringLiteral("Const"), QStringLiteral("MPa"), 2.0, 2.0, false, false);   // valor constante
        QCOMPARE(legend.tickLabels().first(), legend.tickLabels().last());
        legend.clearLegend();
        QVERIFY(!legend.hasLegend());
        QVERIFY(legend.isHidden());
        QVERIFY(legend.tickLabels().isEmpty());
    }

    void legendFitsSmallAndLargeViewports()
    {
        for (const QSize size : {QSize(260, 180), QSize(640, 360), QSize(1920, 1080)}) {
            QWidget host;
            host.resize(size);
            host.show();
            Structura::UI::ColorLegendOverlay legend(&host);
            legend.setLegend(QStringLiteral("Tensão de von Mises (máx. entre as faces)"), QStringLiteral("MPa"), -1.234e3, 5.6789e5, false, true);
            QVERIFY2(legend.width() <= size.width(), qPrintable(QStringLiteral("largura %1 > %2").arg(legend.width()).arg(size.width())));
            QVERIFY2(legend.height() <= size.height(), qPrintable(QStringLiteral("altura %1 > %2").arg(legend.height()).arg(size.height())));
            QVERIFY(legend.x() >= 0 && legend.y() >= 0);
            host.resize(size.width() + 50, size.height() + 40);
            legend.placeInParent();
            QVERIFY(legend.geometry().right() <= host.width());
        }
    }

    // ---------------------------------------------------------------- renderizador e picking
    void rendererUpdatesOnlyColorsWhenTheGeometryIsTheSame()
    {
        Fixture f;
        f.setUz([](double x, double y, double) { return 1e-3 * (x + y); });
        Structura::Visualization::ShellRenderer renderer;
        const RenderMesh a = f.build(Quantity::Uz);
        renderer.setMesh(a);
        QCOMPARE(renderer.geometryRebuilds(), 1);
        QCOMPARE(renderer.colorUpdates(), 0);
        renderer.setMesh(f.build(Quantity::DisplacementMagnitude));   // outra grandeza: so cores
        renderer.setMesh(f.build(Quantity::Uz, false));
        QCOMPARE(renderer.geometryRebuilds(), 1);
        QCOMPARE(renderer.colorUpdates(), 2);
        renderer.setMesh(f.build(Quantity::Uz, true, 1000.0));        // outra escala da cena: geometria nova
        QCOMPARE(renderer.geometryRebuilds(), 2);
        RenderOptions plain;
        plain.highlightObject = 0;
        renderer.setMesh(buildRenderMesh(f.mesh, f.model, nullptr, plain));   // sem resultados: mesma geometria de 1 (upm = 1)? nao: upm=1000 acima
        QVERIFY(renderer.geometryRebuilds() >= 2);
    }

    void pickPointReturnsTheQuadAndTheScenePoint()
    {
        Fixture f;
        f.setUz([](double x, double, double) { return 1e-3 * x; });
        const RenderMesh m = f.build();
        Structura::Visualization::ShellRenderer renderer;
        vtkNew<vtkRenderer> ren;
        vtkNew<vtkRenderWindow> window;
        window->SetOffScreenRendering(1);
        window->SetSize(400, 300);
        window->AddRenderer(ren);
        renderer.attach(ren);
        renderer.setMesh(m);
        renderer.setVisible(true);
        // camera de topo, olhando a placa 2 x 1 m
        vtkCamera *cam = ren->GetActiveCamera();
        cam->ParallelProjectionOn();
        cam->SetFocalPoint(1.0, 0.5, 0.0);
        cam->SetPosition(1.0, 0.5, 5.0);
        cam->SetViewUp(0, 1, 0);
        cam->SetParallelScale(0.8);
        ren->ResetCameraClippingRange();
        const auto center = renderer.pickPoint(200, 150);
        QVERIFY(center.quad >= 0);
        QVERIFY(std::abs(center.point[0] - 1.0) < 0.05 && std::abs(center.point[1] - 0.5) < 0.05);
        const ProbeHit h = probeQuad(m, center.quad, center.point);
        QVERIFY(h.valid && h.hasValue);
        QVERIFY(std::abs(h.position[0] - center.point[0]) < 1e-5);
        QVERIFY(std::abs(h.value - 1e-3 * center.point[0] * quantityInfo(Quantity::Uz).scale) < 1e-4 * std::max(1.0, std::abs(h.value)));
        QCOMPARE(renderer.pickQuad(200, 150), center.quad);
        QCOMPARE(renderer.pickPoint(2, 2).quad, -1);   // fora da placa
        renderer.setVisible(false);
        QCOMPARE(renderer.pickPoint(200, 150).quad, -1);   // oculta: nao consulta
    }
};

QTEST_MAIN(TestShellProbe)
#include "TestShellProbe.moc"
