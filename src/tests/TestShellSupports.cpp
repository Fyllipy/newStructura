// Fase 3: resolucao compartilhada dos apoios de cascas, apoios por nos (coordenadas), tipos para exibicao, glifos, legenda e dialogo.
#include <QtTest/QtTest>

#include "shell/ShellJob.h"
#include "shell/ShellMesh.h"
#include "shell/ShellModel.h"
#include "shell/ShellSupports.h"
#include "ui/ShellSupportDialog.h"
#include "ui/SupportLegendOverlay.h"
#include "viz/structure/ShellRenderer.h"

#include <QComboBox>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>

using namespace Structura::Shell;

namespace {

Model plateModel(double size = 0.5)
{
    Model model;
    Object o;
    o.id = 7;
    o.name = "Laje";
    o.kind = Kind::Plate;
    o.polygon = {{0, 0, 0}, {2, 0, 0}, {2, 1, 0}, {0, 1, 0}};
    o.p = {{"t", 0.2}};
    o.meshSize = size;
    model.objects.push_back(o);
    return model;
}

Support nodesSupport(const std::vector<std::array<double, 3>> &points, std::array<int, 6> fixed)
{
    Support s;
    s.selector = Support::Nodes;
    s.tol = 1e-5;
    s.fixed = fixed;
    s.points = points;
    return s;
}

std::set<int> nodeSet(const SupportResolution &r, std::size_t index = 0)
{
    return r.supports.size() > index ? std::set<int>(r.supports[index].nodes.begin(), r.supports[index].nodes.end()) : std::set<int> {};
}

} // namespace

class TestShellSupports : public QObject
{
    Q_OBJECT

private slots:
    // ---------------------------------------------------------------- resolucao (comportamento anterior preservado)
    void planeFreeEdgeAndPointSelectorsKeepTheirMeaning()
    {
        Model model = plateModel();
        const Mesh mesh = generateMesh(model);
        Support plane;
        plane.selector = Support::Plane;
        plane.axis = 0;
        plane.value = 0.0;   // lado x = 0
        Support edge;
        edge.selector = Support::FreeEdge;
        Support point;
        point.selector = Support::Point;
        point.value = 1.97;
        point.value2 = 0.02;
        model.supports = {plane, edge, point};
        const SupportResolution r = resolveSupports(model, mesh);
        QCOMPARE(r.supports.size(), std::size_t {3});
        QVERIFY(r.warnings.empty());
        // plano x = 0: os nos do lado esquerdo (malha 0,5 m em 1 m de altura: 3 nos)
        QCOMPARE(nodeSet(r, 0).size(), std::size_t {3});
        for (const int n : r.supports[0].nodes) {
            QVERIFY(std::abs(mesh.nodes[static_cast<std::size_t>(n)][0]) < 1e-9);
        }
        // borda livre: o contorno inteiro (5 x 3 nos -> 12 nos de borda)
        QCOMPARE(nodeSet(r, 1).size(), std::size_t {12});
        // ponto: o no mais proximo de (1,97; 0,02) e o canto (2, 0)
        QCOMPARE(r.supports[2].nodes.size(), std::size_t {1});
        const auto &p = mesh.nodes[static_cast<std::size_t>(r.supports[2].nodes[0])];
        QVERIFY(std::abs(p[0] - 2.0) < 1e-9 && std::abs(p[1]) < 1e-9);
        // plano que nao encontra nos: aviso (como antes)
        plane.value = 123.0;
        model.supports = {plane};
        const SupportResolution none = resolveSupports(model, mesh);
        QVERIFY(none.supports.empty());
        QCOMPARE(none.warnings.size(), std::size_t {1});
    }

    void jobAppliesTheSharedResolution()
    {
        Model model = plateModel();
        const Mesh mesh = generateMesh(model);
        Support plane;
        plane.selector = Support::Plane;
        plane.axis = 0;
        plane.value = 0.0;
        plane.fixed = {1, 1, 1, 1, 1, 1};
        Support spring;
        spring.selector = Support::Plane;
        spring.axis = 0;
        spring.value = 2.0;
        spring.springPerArea = 1.0e7;
        spring.compressionOnly = true;
        model.supports = {plane, spring};
        const Job job = buildJob(model, mesh, {"A"});
        int fixedNodes = 0;
        for (const auto &n : job.nodes) {
            fixedNodes += std::all_of(n.fixed.begin(), n.fixed.end(), [](int f) { return f == 1; }) ? 1 : 0;
        }
        QCOMPARE(fixedNodes, 3);
        QCOMPARE(job.springs.size(), std::size_t {3});
        for (const auto &s : job.springs) {
            QVERIFY(s.compressionOnly && s.k[0] > 0.0);
        }
        QVERIFY(job.warnings.empty());
    }

    // ---------------------------------------------------------------- apoios por nos (coordenadas)
    void nodesSelectorFindsEachPointAndAppliesThroughTheJob()
    {
        Model model = plateModel();
        const Mesh mesh = generateMesh(model);
        const auto &a = mesh.nodes[2], &b = mesh.nodes[7];
        model.supports = {nodesSupport({a, b}, {1, 1, 1, 0, 0, 0})};
        const SupportResolution r = resolveSupports(model, mesh);
        QVERIFY(r.warnings.empty());
        QCOMPARE(nodeSet(r), (std::set<int> {2, 7}));
        const Job job = buildJob(model, mesh, {"A"});
        for (std::size_t n = 0; n < job.nodes.size(); ++n) {
            const bool expected = n == 2 || n == 7;
            QCOMPARE(job.nodes[n].fixed[0], expected ? 1 : 0);
            QCOMPARE(job.nodes[n].fixed[3], 0);
        }
    }

    void missingPointsAreReportedNeverSnappedToADistantNode()
    {
        Model model = plateModel();
        const Mesh mesh = generateMesh(model);
        auto shifted = mesh.nodes[4];
        shifted[0] += 1.0e-3;   // 1 mm: nao e o mesmo no
        model.supports = {nodesSupport({mesh.nodes[1], shifted}, {1, 1, 1, 0, 0, 0})};
        const SupportResolution r = resolveSupports(model, mesh);
        QCOMPARE(nodeSet(r), (std::set<int> {1}));   // so o que existe
        QCOMPARE(r.warnings.size(), std::size_t {1});
        QVERIFY(QString::fromStdString(r.warnings[0]).contains(QStringLiteral("1 ponto")));
        // todos ausentes: nenhum no e so o aviso especifico
        model.supports = {nodesSupport({shifted}, {1, 1, 1, 0, 0, 0})};
        const SupportResolution none = resolveSupports(model, mesh);
        QVERIFY(none.supports.empty());
        QCOMPARE(none.warnings.size(), std::size_t {1});
        QVERIFY(QString::fromStdString(none.warnings[0]).contains(QStringLiteral("ponto")));
        QVERIFY(buildJob(model, mesh, {"A"}).warnings.size() == 1);
    }

    void nodesSupportsSurviveRemeshing()
    {
        Model model = plateModel(0.5);
        const Mesh coarse = generateMesh(model);
        std::vector<std::array<double, 3>> pts;
        for (std::size_t n = 0; n < coarse.nodes.size(); n += 2) {
            pts.push_back(coarse.nodes[n]);
        }
        model.supports = {nodesSupport(pts, {1, 1, 1, 1, 1, 1})};
        model.objects[0].meshSize = 0.25;   // malha refeita: outra numeracao e mais nos
        const Mesh fine = generateMesh(model);
        QVERIFY(fine.nodes.size() > coarse.nodes.size());
        const SupportResolution r = resolveSupports(model, fine);
        QVERIFY(r.warnings.empty());
        QCOMPARE(nodeSet(r).size(), pts.size());
        for (const int n : r.supports[0].nodes) {
            bool match = false;
            for (const auto &p : pts) {
                const auto &q = fine.nodes[static_cast<std::size_t>(n)];
                match = match || (std::abs(p[0] - q[0]) < 1e-9 && std::abs(p[1] - q[1]) < 1e-9 && std::abs(p[2] - q[2]) < 1e-9);
            }
            QVERIFY(match);
        }
        // volta para a malha grossa: pontos que so existiam na fina seriam relatados (aqui todos existem)
        model.objects[0].meshSize = 1.0;
        const Mesh veryCoarse = generateMesh(model);
        const SupportResolution back = resolveSupports(model, veryCoarse);
        QVERIFY(!back.warnings.empty());   // varios pontos nao existem mais nessa malha
    }

    void nodesSupportRespectsTheObjectFilter()
    {
        Model model = plateModel();
        Object other = model.objects[0];
        other.id = 8;
        other.name = "Outra";
        other.polygon = {{5, 0, 0}, {7, 0, 0}, {7, 1, 0}, {5, 1, 0}};
        model.objects.push_back(other);
        const Mesh mesh = generateMesh(model);
        // no do objeto 8, mas o apoio so vale para o objeto 7
        std::array<double, 3> far {};
        for (std::size_t n = 0; n < mesh.nodes.size(); ++n) {
            if (mesh.nodes[n][0] > 4.0) {
                far = mesh.nodes[n];
            }
        }
        Support s = nodesSupport({far}, {1, 1, 1, 0, 0, 0});
        s.object = 7;
        model.supports = {s};
        QVERIFY(resolveSupports(model, mesh).supports.empty());
        model.supports[0].object = 8;
        QCOMPARE(resolveSupports(model, mesh).supports.size(), std::size_t {1});
    }

    void fingerprintChangesWithTheSupportPoints()
    {
        Model model = plateModel();
        model.supports = {nodesSupport({{1, 0, 0}}, {1, 1, 1, 0, 0, 0})};
        const std::string a = fingerprint(model);
        model.supports[0].points.push_back({1, 0.5, 0});
        QVERIFY(fingerprint(model) != a);
        model.supports[0].points[1][1] = 0.75;
        QVERIFY(fingerprint(model) != a);
    }

    // ---------------------------------------------------------------- tipos
    void classifiesSupportTypes()
    {
        auto kindOf = [](std::array<int, 6> fixed, double spring = 0.0, bool comp = false, bool *ok = nullptr) {
            Support s;
            s.fixed = fixed;
            s.springPerArea = spring;
            s.compressionOnly = comp;
            SupportKind k = SupportKind::Fixed;
            const bool classified = classifySupport(s, k);
            if (ok) {
                *ok = classified;
            }
            return k;
        };
        bool ok = false;
        QCOMPARE(kindOf({1, 1, 1, 1, 1, 1}), SupportKind::Fixed);
        QCOMPARE(kindOf({1, 1, 1, 0, 0, 0}), SupportKind::Pinned);
        QCOMPARE(kindOf({0, 0, 1, 0, 0, 0}), SupportKind::Roller);
        QCOMPARE(kindOf({1, 0, 1, 0, 0, 0}), SupportKind::Roller);
        QCOMPARE(kindOf({1, 1, 1, 1, 0, 0}), SupportKind::Partial);
        QCOMPARE(kindOf({0, 0, 0, 1, 0, 0}), SupportKind::Partial);
        QCOMPARE(kindOf({0, 0, 1, 1, 0, 0}), SupportKind::Partial);
        QCOMPARE(kindOf({0, 0, 0, 0, 0, 0}, 0.0, false, &ok), SupportKind::Fixed);
        QVERIFY(!ok);   // sem restricao: nada a desenhar
        QCOMPARE(kindOf({0, 0, 0, 0, 0, 0}, 1e7), SupportKind::Spring);
        QCOMPARE(kindOf({0, 0, 0, 0, 0, 0}, 1e7, true), SupportKind::CompressionOnly);
        QVERIFY(std::string(supportKindName(SupportKind::Fixed)).size() > 0);
    }

    void marksMergeSupportsOnTheSameNode()
    {
        Model model = plateModel();
        const Mesh mesh = generateMesh(model);
        const auto &p = mesh.nodes[3];
        Support roller = nodesSupport({p}, {0, 0, 1, 0, 0, 0});
        Support xRoller = nodesSupport({p}, {1, 0, 0, 0, 0, 0});
        Support lone = nodesSupport({mesh.nodes[5]}, {1, 1, 1, 1, 1, 1});
        Support spring = nodesSupport({mesh.nodes[8]}, {0, 0, 0, 0, 0, 0});
        spring.springPerArea = 1e7;
        Support pinnedSameNode = nodesSupport({mesh.nodes[8]}, {1, 1, 1, 0, 0, 0});
        model.supports = {roller, xRoller, lone, spring, pinnedSameNode};
        const auto marks = supportMarks(model, mesh, resolveSupports(model, mesh));
        QCOMPARE(marks.size(), std::size_t {3});
        for (const SupportMark &m : marks) {
            if (m.node == 3) {
                QCOMPARE(m.kind, SupportKind::Roller);   // UZ + UX: 2 translacoes sem rotacao
                QCOMPARE(m.fixed[0] + m.fixed[2], 2);
            } else if (m.node == 5) {
                QCOMPARE(m.kind, SupportKind::Fixed);
            } else {
                QCOMPARE(m.node, 8);
                QCOMPARE(m.kind, SupportKind::Spring);   // a mola prevalece na exibicao
            }
        }
    }

    // ---------------------------------------------------------------- glifos
    void rendererDrawsSupportGlyphsAndCanHideThem()
    {
        Structura::Visualization::ShellRenderer renderer;
        std::vector<Structura::Visualization::ShellRenderer::SupportGlyph> glyphs;
        for (int k = 0; k < 6; ++k) {
            Structura::Visualization::ShellRenderer::SupportGlyph g;
            g.position = {static_cast<float>(k), 0.f, 0.f};
            g.kind = k;
            g.normal = {0.f, 0.f, 1.f};
            glyphs.push_back(g);
        }
        renderer.setSupports(glyphs, 0.2f);
        QCOMPARE(renderer.supportGlyphCount(), std::size_t {6});
        renderer.setSupportsVisible(false);
        renderer.setSupportsVisible(true);
        renderer.setSupports({}, 0.2f);
        QCOMPARE(renderer.supportGlyphCount(), std::size_t {0});
    }

    // ---------------------------------------------------------------- interface
    void legendListsTheKindsPresent()
    {
        QWidget host;
        host.resize(800, 500);
        host.show();
        Structura::UI::SupportLegendOverlay legend(&host);
        QVERIFY(legend.isHidden());
        legend.setEntries({{4, 5}, {0, 12}, {1, 3}});
        QVERIFY(legend.isVisible());
        QCOMPARE(legend.entries().size(), std::size_t {3});
        QCOMPARE(legend.entries().front().kind, 0);   // ordenado por tipo
        QVERIFY(legend.geometry().right() <= host.width() && legend.geometry().bottom() <= host.height());
        QVERIFY(legend.testAttribute(Qt::WA_TransparentForMouseEvents));
        QVERIFY(!legend.grab().isNull());
        legend.setEntries({});
        QVERIFY(legend.isHidden());
    }

    void supportDialogProducesTheChosenSupport()
    {
        Structura::UI::ShellSupportDialog dialog(5);
        auto *preset = dialog.findChild<QComboBox *>();
        QVERIFY(preset);
        auto supportAt = [&](int index) {
            preset->setCurrentIndex(index);
            return dialog.support();
        };
        auto s = supportAt(0);   // engaste
        QCOMPARE(std::accumulate(s.fixed.begin(), s.fixed.end(), 0), 6);
        s = supportAt(1);   // articulado
        QCOMPARE(s.fixed, (std::array<int, 6> {1, 1, 1, 0, 0, 0}));
        s = supportAt(2);   // movel em Z
        QCOMPARE(s.fixed, (std::array<int, 6> {0, 0, 1, 0, 0, 0}));
        s = supportAt(3);
        QCOMPARE(s.fixed, (std::array<int, 6> {1, 0, 0, 0, 0, 0}));
        s = supportAt(5);   // mola
        QVERIFY(s.springPerArea > 0.0);
        QCOMPARE(std::accumulate(s.fixed.begin(), s.fixed.end(), 0), 0);
        QVERIFY(!s.compressionOnly);
    }
};

QTEST_MAIN(TestShellSupports)
#include "TestShellSupports.moc"
