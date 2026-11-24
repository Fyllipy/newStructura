// Fase 4: cargas de casca dadas por clique em nos (forca nodal, carga linear, carga de superficie), equilibrio e setas.
#include <QtTest/QtTest>

#include "app/ShellSolverBridge.h"
#include "shell/ShellJob.h"
#include "shell/ShellLoads.h"
#include "shell/ShellMesh.h"
#include "shell/ShellModel.h"
#include "ui/ShellLoadDialog.h"
#include "viz/structure/ShellRenderer.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QStandardItemModel>

#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <set>

using namespace Structura::Shell;

namespace {

Model plateModel(std::vector<Vec3> polygon = {{0, 0, 0}, {2, 0, 0}, {2, 1, 0}, {0, 1, 0}}, double size = 0.5)
{
    Model model;
    Object o;
    o.id = 7;
    o.name = "Laje";
    o.kind = Kind::Plate;
    o.polygon = std::move(polygon);
    o.p = {{"t", 0.2}};
    o.meshSize = size;
    model.objects.push_back(o);
    return model;
}

std::vector<std::array<double, 3>> nodesWhere(const Mesh &mesh, const std::function<bool(const Vec3 &)> &pred)
{
    std::vector<std::array<double, 3>> pts;
    for (const Vec3 &p : mesh.nodes) {
        if (pred(p)) {
            pts.push_back(p);
        }
    }
    return pts;
}

Load pointLoad(Load::Type type, std::array<double, 4> v, std::vector<std::array<double, 3>> points, const std::string &caseName = "Q")
{
    Load l;
    l.caseName = caseName;
    l.type = type;
    l.v = v;
    l.points = std::move(points);
    return l;
}

std::array<double, 3> totalForce(const Job &job, std::size_t caseIndex = 0)
{
    std::array<double, 3> f {{0.0, 0.0, 0.0}};
    for (const auto &n : job.cases[caseIndex].nodal) {
        for (std::size_t k = 0; k < 3; ++k) {
            f[k] += n.f[k];
        }
    }
    return f;
}

double areaOfAll(const Mesh &mesh)
{
    double a = 0.0;
    for (const MeshQuad &q : mesh.quads) {
        a += quadArea(mesh, q);
    }
    return a;
}

} // namespace

class TestShellLoads : public QObject
{
    Q_OBJECT

private slots:
    // ---------------------------------------------------------------- resolucao dos alvos
    void nodalLoadResolvesEachPointAndReportsMissingOnes()
    {
        Model model = plateModel();
        const Mesh mesh = generateMesh(model);
        auto shifted = mesh.nodes[6];
        shifted[0] += 1.0e-3;   // 1 mm: nao e o mesmo no
        model.loads = {pointLoad(Load::NodalPoints, {0, 0, -5000.0, 0}, {mesh.nodes[2], mesh.nodes[4], shifted})};
        const ResolvedPointLoad r = resolvePointLoad(model, mesh, 0);
        QCOMPARE(r.nodes, (std::vector<int> {2, 4}));
        QCOMPARE(r.missing, 1);
        const Job job = buildJob(model, mesh, {"Q"});
        QCOMPARE(job.cases[0].nodal.size(), std::size_t {2});
        QVERIFY(job.warnings.size() == 1 && QString::fromStdString(job.warnings[0]).contains(QStringLiteral("1 ponto")));
        QCOMPARE(totalForce(job)[2], -10000.0);   // 2 nos x -5 kN
    }

    void lineLoadUsesOnlyEdgesWithBothNodesAndCountsSharedEdgesOnce()
    {
        Model model = plateModel();
        const Mesh mesh = generateMesh(model);
        // linha interna y = 0,5 (arestas compartilhadas por dois elementos): 4 arestas de 0,5 m
        const auto row = nodesWhere(mesh, [](const Vec3 &p) { return std::abs(p[1] - 0.5) < 1e-9; });
        QCOMPARE(row.size(), std::size_t {5});
        model.loads = {pointLoad(Load::LinePoints, {0, 0, -10000.0, 0}, row)};
        const ResolvedPointLoad r = resolvePointLoad(model, mesh, 0);
        QCOMPARE(r.edges.size(), std::size_t {4});
        const Job job = buildJob(model, mesh, {"Q"});
        QVERIFY(std::abs(totalForce(job)[2] - (-10000.0 * 2.0)) < 1e-9);   // q x L = 10 kN/m x 2 m
        // dois nos nao vizinhos nao formam aresta: nada de carga, com aviso
        model.loads = {pointLoad(Load::LinePoints, {0, 0, -10000.0, 0}, {mesh.nodes[0], mesh.nodes[2]})};
        const Job none = buildJob(model, mesh, {"Q"});
        QVERIFY(none.cases[0].nodal.empty());
        QCOMPARE(none.warnings.size(), std::size_t {1});
    }

    void surfaceLoadUsesOnlyElementsWithAllFourNodes()
    {
        Model model = plateModel();
        const Mesh mesh = generateMesh(model);
        const auto left = nodesWhere(mesh, [](const Vec3 &p) { return p[0] <= 1.0 + 1e-9; });   // metade esquerda: 2 x 2 elementos
        model.loads = {pointLoad(Load::RegionPoints, {0, 0, -3000.0, 0}, left)};
        const ResolvedPointLoad r = resolvePointLoad(model, mesh, 0);
        QCOMPARE(r.elements.size(), std::size_t {4});
        const Job job = buildJob(model, mesh, {"Q"});
        QVERIFY(std::abs(totalForce(job)[2] - (-3000.0 * 1.0)) < 1e-9);   // q x A = 3 kN/m2 x 1 m2
        // um canto isolado de elemento nao e elemento selecionado
        const auto corner = nodesWhere(mesh, [](const Vec3 &p) { return p[0] < 0.6 && p[1] < 0.6 && !(p[0] > 0.4 && p[1] > 0.4); });
        model.loads = {pointLoad(Load::RegionPoints, {0, 0, -3000.0, 0}, corner)};
        QCOMPARE(resolvePointLoad(model, mesh, 0).elements.size(), std::size_t {0});
    }

    void loadFiltersByObject()
    {
        Model model = plateModel();
        Object other = model.objects[0];
        other.id = 8;
        other.polygon = {{5, 0, 0}, {7, 0, 0}, {7, 1, 0}, {5, 1, 0}};
        model.objects.push_back(other);
        const Mesh mesh = generateMesh(model);
        const auto far = nodesWhere(mesh, [](const Vec3 &p) { return p[0] > 4.0 && p[1] < 0.01; });
        Load l = pointLoad(Load::NodalPoints, {0, 0, -1.0, 0}, far);
        l.object = 7;
        model.loads = {l};
        QVERIFY(resolvePointLoad(model, mesh, 0).nodes.empty());
        model.loads[0].object = 8;
        QCOMPARE(resolvePointLoad(model, mesh, 0).nodes.size(), far.size());
    }

    // ---------------------------------------------------------------- equilibrio (soma das forcas)
    void surfaceLoadModesGlobalProjectedAndNormal()
    {
        // placa inclinada 45 graus: normal (0,-1,1)/sqrt2
        Model model = plateModel({{0, 0, 0}, {2, 0, 0}, {2, 1, 1}, {0, 1, 1}});
        const Mesh mesh = generateMesh(model);
        const double area = areaOfAll(mesh);
        QVERIFY(std::abs(area - 2.0 * std::sqrt(2.0)) < 1e-9);
        const auto all = nodesWhere(mesh, [](const Vec3 &) { return true; });
        // modo 0: forca por area global -> SF = q A
        model.loads = {pointLoad(Load::RegionPoints, {0, 0, -1000.0, 0}, all)};
        QVERIFY(std::abs(totalForce(buildJob(model, mesh, {"Q"}))[2] + 1000.0 * area) < 1e-6);
        // modo 1: por area projetada em planta -> SF = q A |nz| = q x (area em planta = 2)
        model.loads = {pointLoad(Load::RegionPoints, {0, 0, -1000.0, 1.0}, all)};
        QVERIFY(std::abs(totalForce(buildJob(model, mesh, {"Q"}))[2] + 1000.0 * 2.0) < 1e-6);
        // modo 2: pressao normal -> vira pressao por elemento (sem forcas nodais); soma p A no sentido da normal
        model.loads = {pointLoad(Load::RegionPoints, {500.0, 0, 0, 2.0}, all)};
        const Job job = buildJob(model, mesh, {"Q"});
        QVERIFY(job.cases[0].nodal.empty());
        double pa = 0.0;
        for (const auto &p : job.cases[0].pressure) {
            QCOMPARE(p.p[0], 500.0);
            pa += p.p[0] * quadArea(mesh, mesh.quads[static_cast<std::size_t>(p.shell)]);
        }
        QVERIFY(std::abs(pa - 500.0 * area) < 1e-6);
    }

    void loadsOfDifferentCasesStaySeparate()
    {
        Model model = plateModel();
        const Mesh mesh = generateMesh(model);
        const auto top = nodesWhere(mesh, [](const Vec3 &p) { return std::abs(p[1] - 1.0) < 1e-9; });
        model.loads = {pointLoad(Load::LinePoints, {0, 0, -1000.0, 0}, top, "A"), pointLoad(Load::NodalPoints, {200.0, 0, 0, 0}, {top[0]}, "B")};
        const Job job = buildJob(model, mesh, {"A", "B"});
        QVERIFY(std::abs(totalForce(job, 0)[2] + 2000.0) < 1e-9);
        QCOMPARE(totalForce(job, 0)[0], 0.0);
        QCOMPARE(totalForce(job, 1)[0], 200.0);
        QCOMPARE(totalForce(job, 1)[2], 0.0);
    }

    void fingerprintChangesWithLoadPoints()
    {
        Model model = plateModel();
        model.loads = {pointLoad(Load::NodalPoints, {0, 0, -1.0, 0}, {{1, 0, 0}})};
        const std::string a = fingerprint(model);
        model.loads[0].points.push_back({1, 0.5, 0});
        QVERIFY(fingerprint(model) != a);
    }

    // ---------------------------------------------------------------- setas
    void arrowsFollowTheLoads()
    {
        Model model = plateModel();
        const Mesh mesh = generateMesh(model);
        const auto row = nodesWhere(mesh, [](const Vec3 &p) { return std::abs(p[1]) < 1e-9; });
        const auto all = nodesWhere(mesh, [](const Vec3 &) { return true; });
        model.loads = {pointLoad(Load::NodalPoints, {0, 0, -3000.0, 0}, {mesh.nodes[4]}), pointLoad(Load::LinePoints, {0, 4000.0, 0, 0}, row),
                       pointLoad(Load::RegionPoints, {-800.0, 0, 0, 2.0}, all), Load {}};   // a ultima (peso proprio) nao desenha seta
        const auto arrows = loadArrows(model, mesh);
        int nodal = 0, line = 0, region = 0;
        for (const LoadArrow &a : arrows) {
            if (a.kind == 0) {
                ++nodal;
                QCOMPARE(a.direction[2], -1.0);
                QCOMPARE(a.magnitude, 3000.0);
            } else if (a.kind == 1) {
                ++line;
                QCOMPARE(a.direction[1], 1.0);
                QCOMPARE(a.magnitude, 4000.0);
            } else {
                ++region;
                QVERIFY(std::abs(a.direction[2] + 1.0) < 1e-12);   // pressao negativa na normal +z: seta para -z
                QCOMPARE(a.magnitude, 800.0);
            }
        }
        QCOMPARE(nodal, 1);
        QCOMPARE(line, 4);
        QCOMPARE(region, 8);
        // renderizador
        Structura::Visualization::ShellRenderer renderer;
        std::vector<Structura::Visualization::ShellRenderer::LoadArrowGlyph> glyphs;
        for (const LoadArrow &a : arrows) {
            Structura::Visualization::ShellRenderer::LoadArrowGlyph g;
            g.tip = {static_cast<float>(a.position[0]), static_cast<float>(a.position[1]), static_cast<float>(a.position[2])};
            g.direction = {static_cast<float>(a.direction[0]), static_cast<float>(a.direction[1]), static_cast<float>(a.direction[2])};
            g.length = 0.3f;
            g.kind = a.kind;
            glyphs.push_back(g);
        }
        renderer.setLoadArrows(glyphs);
        QCOMPARE(renderer.loadArrowCount(), glyphs.size());
        renderer.setLoadArrowsVisible(false);
        renderer.setLoadArrows({});
        QCOMPARE(renderer.loadArrowCount(), std::size_t {0});
    }

    // ---------------------------------------------------------------- interface
    void loadDialogOffersOnlyTypesWithTargets()
    {
        Structura::UI::ShellLoadDialog dialog(QStringList {QStringLiteral("PP"), QStringLiteral("SC")}, 5, 4, 0);
        auto combos = dialog.findChildren<QComboBox *>();
        QVERIFY(combos.size() >= 3);
        QComboBox *type = nullptr;
        for (auto *c : combos) {
            if (c->count() == 3 && c->itemText(0).startsWith(QStringLiteral("Força em"))) {
                type = c;
            }
        }
        QVERIFY(type);
        QCOMPARE(type->currentIndex(), 1);   // selecao em linha (4 arestas entre 5 nos): carga linear
        auto *model = qobject_cast<QStandardItemModel *>(type->model());
        QVERIFY(model);
        QVERIFY(model->item(0)->isEnabled());
        QVERIFY(model->item(1)->isEnabled());
        QVERIFY(!model->item(2)->isEnabled());   // sem elementos completos: carga de superficie indisponivel
        const auto spins = dialog.findChildren<QDoubleSpinBox *>();
        QVERIFY(spins.size() >= 3);
        spins[0]->setValue(2.0);
        spins[1]->setValue(0.0);
        spins[2]->setValue(-3.5);
        const Load l = dialog.load();
        QCOMPARE(static_cast<int>(l.type), static_cast<int>(Load::LinePoints));
        QCOMPARE(l.v[0], 2000.0);   // kN/m -> N/m
        QCOMPARE(l.v[2], -3500.0);
        QCOMPARE(l.caseName, std::string("PP"));
    }

    // ---------------------------------------------------------------- solver: reacoes opostas as cargas
    void reactionsBalanceThePointLoadsInTheSolver()
    {
        const QString dll = QStringLiteral(STRUCTURA_SOURCE_DIR "/mefs.dll");
        if (!QFile::exists(dll)) {
            QSKIP("mefs.dll nao encontrada na raiz do repositorio.");
        }
        Model model = plateModel();
        const Mesh mesh = generateMesh(model);
        Support wall;
        wall.selector = Support::Plane;
        wall.axis = 0;
        wall.value = 0.0;
        wall.fixed = {1, 1, 1, 1, 1, 1};
        model.supports = {wall};
        const auto tipEdge = nodesWhere(mesh, [](const Vec3 &p) { return std::abs(p[0] - 2.0) < 1e-9; });
        const auto left = nodesWhere(mesh, [](const Vec3 &p) { return p[0] >= 0.99 && p[0] <= 1.51; });
        model.loads = {pointLoad(Load::LinePoints, {0, 0, -8000.0, 0}, tipEdge, "L"), pointLoad(Load::NodalPoints, {1500.0, 0, -2500.0, 0}, {mesh.nodes[mesh.nodes.size() - 1]}, "N"),
                       pointLoad(Load::RegionPoints, {0, 0, -1200.0, 0}, left, "S"), pointLoad(Load::RegionPoints, {700.0, 0, 0, 2.0}, left, "P")};
        const Job job = buildJob(model, mesh, {"L", "N", "S", "P"});
        const Structura::App::ShellSolverBridge bridge(dll);
        const Results results = bridge.solve(model, mesh, job);
        QVERIFY2(results.ok(), results.error.c_str());
        // area dos elementos da regiao e comprimento da aresta
        double regionArea = 0.0;
        for (const MeshQuad &q : mesh.quads) {
            bool allIn = true;
            for (const int n : q.n) {
                const double x = mesh.nodes[static_cast<std::size_t>(n)][0];
                allIn = allIn && x >= 0.99 && x <= 1.51;
            }
            regionArea += allIn ? quadArea(mesh, q) : 0.0;
        }
        const double expectedFz[4] = {-8000.0 * 1.0, -2500.0, -1200.0 * regionArea, 700.0 * regionArea};   // aresta de 1 m; pressao + na normal (+z)
        for (std::size_t c = 0; c < 4; ++c) {
            double rz = 0.0, rx = 0.0;
            for (const auto &r : results.cases[c].reaction) {
                rz += r[2];
                rx += r[0];
            }
            QVERIFY2(std::abs(rz + expectedFz[c]) < 1e-6 * std::max(1.0, std::abs(expectedFz[c])), qPrintable(QStringLiteral("caso %1: soma de Rz %2, esperado %3").arg(c).arg(rz).arg(-expectedFz[c])));
            QVERIFY(std::abs(rx + (c == 1 ? 1500.0 : 0.0)) < 1e-6 * 1500.0 + 1e-9);
        }
    }
};

QTEST_MAIN(TestShellLoads)
#include "TestShellLoads.moc"
