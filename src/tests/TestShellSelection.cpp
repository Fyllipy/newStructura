// Fase 2: infraestrutura de selecao de nos de cascas (tabela de nos, pick, caixa, conjunto transitorio, referencias por coordenadas).
#include <QtTest/QtTest>

#include "shell/ShellMesh.h"
#include "shell/ShellModel.h"
#include "shell/ShellSelection.h"
#include "shell/ShellVisual.h"
#include "viz/structure/ShellRenderer.h"

#include <vtkCamera.h>
#include <vtkNew.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>

#include <algorithm>
#include <cmath>
#include <set>

using namespace Structura::Shell;

namespace {

/// Placa 2 x 1 m, malha `size`, plano XY, na cena em `upm` unidades por metro.
struct Plate {
    Model model;
    Mesh mesh;
    RenderMesh render;
    explicit Plate(double size = 0.5, double upm = 1.0)
    {
        Object o;
        o.id = 7;
        o.name = "Laje";
        o.kind = Kind::Plate;
        o.polygon = {{0, 0, 0}, {2, 0, 0}, {2, 1, 0}, {0, 1, 0}};
        o.p = {{"t", 0.2}};
        o.meshSize = size;
        model.objects.push_back(o);
        mesh = generateMesh(model);
        RenderOptions opt;
        opt.unitsPerMeter = upm;
        render = buildRenderMesh(mesh, model, nullptr, opt);
    }
};

/// Projecao ortografica de topo: cena x em [0,2] -> NDC [-1,1], y em [0,1] -> NDC [-1,1]; profundidade = 0,1 z; widget 400 x 200 px.
ScreenProjector topView()
{
    ScreenProjector p;
    p.matrix = {1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0.1, 0, -1, -1, 0, 1};
    p.width = 400;
    p.height = 200;
    return p;
}

std::array<double, 2> pixelOf(double x, double y)
{
    return {(x - 1.0 + 1.0) * 200.0, (1.0 - (2.0 * y - 1.0)) * 100.0};
}

} // namespace

class TestShellSelection : public QObject
{
    Q_OBJECT

private slots:
    void nodeTableHasEachNodeOnceWithRestCoordinatesInMeters()
    {
        const Plate plate(0.5, 1000.0);   // cena em mm
        QVERIFY(plate.render.vertexNode.size() == plate.render.points.size());
        const NodeTable table = buildNodeTable(plate.render);
        QCOMPARE(static_cast<int>(table.size()), static_cast<int>(plate.mesh.nodes.size()));
        QCOMPARE(table.ids.size(), table.viewPos.size());
        std::set<int> unique(table.ids.begin(), table.ids.end());
        QCOMPARE(unique.size(), table.size());
        for (std::size_t i = 0; i < table.size(); ++i) {
            const auto &n = plate.mesh.nodes[static_cast<std::size_t>(table.ids[i])];
            for (std::size_t k = 0; k < 3; ++k) {
                QVERIFY(std::abs(table.restPos[i][k] - n[k]) < 1e-5);          // metros
                QVERIFY(std::abs(table.viewPos[i][k] - n[k] * 1000.0) < 1e-2);  // cena
            }
            QCOMPARE(table.objectId[i], 7);
        }
        QVERIFY(table.indexOf(table.ids[3]) == 3);
        QCOMPARE(table.indexOf(123456), -1);
    }

    void nodeTableUsesRestPositionsWhenTheViewIsDeformed()
    {
        Plate plate;
        CaseResult r;
        r.displacement.assign(plate.mesh.nodes.size(), std::array<double, 6> {});
        for (auto &d : r.displacement) {
            d[2] = 0.01;
        }
        r.elements.assign(plate.mesh.quads.size(), ElementState {});
        RenderOptions opt;
        opt.quantity = Quantity::Uz;
        opt.deformScale = 10.0;
        const RenderMesh deformed = buildRenderMesh(plate.mesh, plate.model, &r, opt);
        const NodeTable t = buildNodeTable(deformed);
        QVERIFY(t.size() > 0);
        for (std::size_t i = 0; i < t.size(); ++i) {
            QVERIFY(std::abs(t.viewPos[i][2] - 0.1) < 1e-6);   // deformada
            QVERIFY(std::abs(t.restPos[i][2]) < 1e-6);         // referencia persistente: sem deformada
        }
    }

    void nodeTableIsEmptyWithoutTheVertexNodeAssociation()
    {
        RenderMesh m;
        m.points = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
        QCOMPARE(buildNodeTable(m).size(), std::size_t {0});
        QCOMPARE(buildNodeTable(RenderMesh {}).size(), std::size_t {0});
    }

    void projectorMapsAndRejectsPointsBehindTheCamera()
    {
        const ScreenProjector p = topView();
        double sx, sy, d;
        QVERIFY(p.project({1.0, 0.5, 0.0}, sx, sy, d));
        QVERIFY(std::abs(sx - 200.0) < 1e-9 && std::abs(sy - 100.0) < 1e-9);
        QVERIFY(p.project({2.0, 1.0, 0.0}, sx, sy, d));
        QVERIFY(std::abs(sx - 400.0) < 1e-9 && std::abs(sy - 0.0) < 1e-9);   // origem da tela em cima
        ScreenProjector behind = p;
        behind.matrix[11] = 1.0;   // w = 1 + z
        QVERIFY(!behind.project({1.0, 0.5, -2.0}, sx, sy, d));
        ScreenProjector clipped = p;
        QVERIFY(!clipped.project({1.0, 0.5, 100.0}, sx, sy, d));   // profundidade fora do volume de recorte
    }

    void pickReturnsTheNearestNodeWithinTolerance()
    {
        const Plate plate;
        const NodeTable table = buildNodeTable(plate.render);
        const ScreenProjector proj = topView();
        // sobre cada no: ele proprio, distancia ~0
        for (std::size_t i = 0; i < table.size(); i += 3) {
            const auto px = pixelOf(table.viewPos[i][0], table.viewPos[i][1]);
            double dist = 99;
            const int hit = pickNearestNode(table, proj, px[0], px[1], 12.0, &dist);
            QCOMPARE(hit, static_cast<int>(i));
            QVERIFY(dist < 1e-6);
        }
        // deslocado alguns pixels: ainda o mesmo no
        const auto p0 = pixelOf(table.viewPos[5][0], table.viewPos[5][1]);
        QCOMPARE(pickNearestNode(table, proj, p0[0] + 5.0, p0[1] - 4.0, 12.0), 5);
        // longe de qualquer no (no meio de um elemento de 100 x 100 px, a mais de 12 px dos cantos)
        QCOMPARE(pickNearestNode(table, proj, 100.0 + 50.0, 100.0, 12.0), -1);
        QCOMPARE(pickNearestNode(NodeTable {}, proj, 10, 10, 12.0), -1);
    }

    void pickTiePrefersTheNodeNearestTheCamera()
    {
        NodeTable t;
        t.ids = {10, 11};
        t.viewPos = {{1.0, 0.5, 5.0}, {1.0, 0.5, -5.0}};   // mesmo pixel, profundidades diferentes (profundidade = 0,1 z)
        t.restPos = t.viewPos;
        t.objectId = {1, 1};
        QCOMPARE(pickNearestNode(t, topView(), 200.0, 100.0, 12.0), 1);   // menor profundidade = mais proximo
    }

    void boxSelectsNodesInsideEitherCornerOrder()
    {
        const Plate plate;
        const NodeTable table = buildNodeTable(plate.render);
        const ScreenProjector proj = topView();
        // metade esquerda da placa: x em [0, 1] -> pixels 0..200 (com folga de 1 px)
        const auto left = nodesInRect(table, proj, -1, -1, 201, 201);
        const auto leftReversed = nodesInRect(table, proj, 201, 201, -1, -1);
        QCOMPARE(left, leftReversed);
        int expected = 0;
        for (std::size_t i = 0; i < table.size(); ++i) {
            expected += table.viewPos[i][0] <= 1.0 + 1e-9 ? 1 : 0;
        }
        QCOMPARE(static_cast<int>(left.size()), expected);
        QVERIFY(left.size() > 0 && left.size() < table.size());
        QCOMPARE(nodesInRect(table, proj, 1000, 1000, 1100, 1100).size(), std::size_t {0});
        QCOMPARE(nodesInRect(table, proj, -10, -10, 500, 300).size(), table.size());
    }


    // ---------------------------------------------------------------- oclusao (nos atras das cascas)
    void boxIgnoresNodesBehindTheFrontOfACylinder()
    {
        Model model;
        Object tank;
        tank.id = 1;
        tank.name = "Tanque";
        tank.kind = Kind::CylTank;
        tank.meshSize = 0.5;
        tank.p = {{"r", 3.0}, {"h", 4.0}, {"courses", 2.0}, {"t1", 0.01}, {"t2", 0.01}, {"tBottom", 0.01}, {"roof", 0.0}};
        model.objects.push_back(tank);
        const Mesh mesh = generateMesh(model);
        RenderOptions opt;
        const RenderMesh render = buildRenderMesh(mesh, model, nullptr, opt);
        const NodeTable table = buildNodeTable(render);
        QVERIFY(table.size() > 100);
        // camera ortografica olhando para +y (de y = -infinito): x -> horizontal, z -> vertical, profundidade = y (menor = mais perto)
        ScreenProjector proj;
        proj.width = 600;
        proj.height = 400;
        proj.matrix = {1.0 / 3.5, 0, 0, 0,   0, 0, 0.5 / 3.5, 0,   0, 1.0 / 2.2, 0, 0,   0, -1.0, 0, 1};
        // (coluna 0: x -> ndc x; coluna 1: y -> profundidade; coluna 2: z -> ndc y; coluna 3: deslocamento)
        const DepthMap map = buildDepthMap(render, proj);
        QVERIFY(!map.empty());
        const auto all = nodesInRect(table, proj, 0, 0, 600, 400);   // sem oclusao: tudo o que projeta
        const auto visible = nodesInRect(table, proj, 0, 0, 600, 400, &map);
        QCOMPARE(all.size(), table.size());
        QVERIFY(visible.size() < all.size());
        std::set<int> seen(visible.begin(), visible.end());
        int frontTotal = 0, frontSeen = 0, backTotal = 0, backSeen = 0;
        for (std::size_t i = 0; i < table.size(); ++i) {
            const double y = table.viewPos[i][1], z = table.viewPos[i][2];
            if (z < 0.01) {
                continue;   // o fundo e visto de perfil: fora do criterio
            }
            if (y < -0.2) {
                ++frontTotal;
                frontSeen += seen.count(static_cast<int>(i)) ? 1 : 0;
            } else if (y > 0.2) {
                ++backTotal;
                backSeen += seen.count(static_cast<int>(i)) ? 1 : 0;
            }
        }
        QVERIFY(frontTotal > 50 && backTotal > 50);
        QCOMPARE(frontSeen, frontTotal);   // a frente toda e selecionavel
        QCOMPARE(backSeen, 0);             // nenhum no do fundo
        // clique: o no do fundo, que cai no mesmo pixel de um da frente, nao e escolhido
        int frontNode = -1, backNode = -1;
        for (std::size_t i = 0; i < table.size() && (frontNode < 0 || backNode < 0); ++i) {
            if (table.viewPos[i][2] > 1.0 && table.viewPos[i][2] < 3.0 && std::abs(table.viewPos[i][0]) < 0.1) {
                (table.viewPos[i][1] < 0 ? frontNode : backNode) = static_cast<int>(i);
            }
        }
        QVERIFY(frontNode >= 0 && backNode >= 0);
        double px, py, depth;
        QVERIFY(proj.project(table.viewPos[static_cast<std::size_t>(backNode)], px, py, depth));
        const int picked = pickNearestNode(table, proj, px, py, 12.0, nullptr, &map);
        QVERIFY(picked < 0 || table.viewPos[static_cast<std::size_t>(picked)][1] < 0.0);   // so nos da frente
    }

    void steepSurfacesDoNotHideTheirOwnNodes()
    {
        const Plate plate;
        const NodeTable table = buildNodeTable(plate.render);
        // profundidade varia muito ao longo da placa (superficie em angulo rasante): nenhum no pode se esconder de si mesmo
        for (const double slope : {0.0, 0.3, 0.9, 1.8}) {
            ScreenProjector proj = topView();
            proj.matrix[2] = slope;   // z_ndc = slope * x (+ 0,1 z)
            const DepthMap map = buildDepthMap(plate.render, proj);
            int hidden = 0;
            for (std::size_t i = 0; i < table.size(); ++i) {
                double sx, sy, d;
                if (!proj.project(table.viewPos[i], sx, sy, d)) {
                    continue;   // fora do recorte: nao e questao de oclusao
                }
                hidden += isNodeVisible(map, proj, table.viewPos[i]) ? 0 : 1;
            }
            QVERIFY2(hidden == 0, qPrintable(QStringLiteral("inclinacao %1: %2 nos escondidos de si mesmos").arg(slope).arg(hidden)));
        }
    }

    void aShellInFrontHidesTheNodesBehindItOnly()
    {
        // duas placas: a de tras (y de 0 a 1) e a da frente (profundidade menor), cobrindo so a metade esquerda da vista
        Model model;
        for (int k = 0; k < 2; ++k) {
            Object o;
            o.id = k + 1;
            o.name = k == 0 ? "Atras" : "Frente";
            o.kind = Kind::Plate;
            o.polygon = k == 0 ? std::vector<Vec3> {{0, 0, 0}, {2, 0, 0}, {2, 1, 0}, {0, 1, 0}} : std::vector<Vec3> {{0, 0, -1}, {1, 0, -1}, {1, 1, -1}, {0, 1, -1}};
            o.p = {{"t", 0.2}};
            o.meshSize = 0.5;
            model.objects.push_back(o);
        }
        const Mesh mesh = generateMesh(model);
        const RenderMesh render = buildRenderMesh(mesh, model, nullptr, RenderOptions {});
        const NodeTable table = buildNodeTable(render);
        const ScreenProjector proj = topView();   // profundidade = 0,1 z: z = -1 fica mais perto da camera
        const DepthMap map = buildDepthMap(render, proj);
        int behindHidden = 0, behindTotal = 0, elseVisible = 0, elseTotal = 0;
        for (std::size_t i = 0; i < table.size(); ++i) {
            const auto &p = table.viewPos[i];
            const bool inFront = p[2] < -0.5;
            const bool underFront = !inFront && p[0] < 1.0 - 1e-9 && p[0] > 1e-9;   // interior da area coberta pela placa da frente
            const bool visible = isNodeVisible(map, proj, p);
            if (underFront) {
                ++behindTotal;
                behindHidden += visible ? 0 : 1;
            } else if (inFront || p[0] > 1.0 + 1e-9) {
                ++elseTotal;
                elseVisible += visible ? 1 : 0;
            }
        }
        QVERIFY(behindTotal > 0 && elseTotal > 0);
        QCOMPARE(behindHidden, behindTotal);
        QCOMPARE(elseVisible, elseTotal);
        QVERIFY(isNodeVisible(DepthMap {}, proj, {0.5, 0.5, 0.0}));   // sem mapa: sem oclusao
    }

    void transientSelectionReplaceAddToggleAndClear()
    {
        NodeSelection s;
        QVERIFY(s.empty());
        s.apply({3, 4}, NodeSelection::Mode::Replace);
        QCOMPARE(s.size(), std::size_t {2});
        s.apply({4, 5}, NodeSelection::Mode::Add);
        QCOMPARE(s.ids(), (std::vector<int> {3, 4, 5}));
        s.apply({4, 9}, NodeSelection::Mode::Toggle);   // 4 sai, 9 entra
        QCOMPARE(s.ids(), (std::vector<int> {3, 5, 9}));
        QVERIFY(s.contains(9) && !s.contains(4));
        s.apply({7}, NodeSelection::Mode::Replace);
        QCOMPARE(s.ids(), (std::vector<int> {7}));
        s.apply({}, NodeSelection::Mode::Replace);   // substituir por nada = limpar
        QVERIFY(s.empty());
        s.set({1, 1, 2});
        QCOMPARE(s.size(), std::size_t {2});
        s.clear();
        QVERIFY(s.empty());
    }

    void referencesReturnTheSameNodesInTheSameMesh()
    {
        const Plate plate;
        const NodeTable table = buildNodeTable(plate.render);
        const std::vector<int> pick = {table.ids[2], table.ids[7], table.ids[11]};
        const auto refs = makeNodeRefs(table, pick);
        QCOMPARE(refs.size(), pick.size());
        QCOMPARE(refs[0].objectId, 7);
        const RefResolution r = resolveNodeRefs(table, refs);
        QCOMPARE(r.unresolved, 0);
        QCOMPARE(r.snapped, 0);
        QCOMPARE(r.merged, 0);
        QCOMPARE(r.nodeIds, pick);
        // id inexistente e ignorado
        QCOMPARE(makeNodeRefs(table, {424242}).size(), std::size_t {0});
    }

    void referencesSurviveRemeshingWithDifferentNumbering()
    {
        const Plate coarse(0.5), fine(0.25);
        const NodeTable tc = buildNodeTable(coarse.render), tf = buildNodeTable(fine.render);
        QVERIFY(tf.size() > tc.size());
        // todos os nos da malha grossa existem na fina (mesma geometria, malha estruturada): selecao completa sobrevive
        std::vector<int> all(tc.ids.begin(), tc.ids.end());
        const RefResolution up = resolveNodeRefs(tf, makeNodeRefs(tc, all));
        QCOMPARE(up.unresolved, 0);
        QCOMPARE(up.nodeIds.size(), all.size());
        for (std::size_t i = 0; i < all.size(); ++i) {
            const int fi = tf.indexOf(up.nodeIds[i]);
            QVERIFY(fi >= 0);
            const int ci = tc.indexOf(all[i]);
            for (std::size_t k = 0; k < 3; ++k) {
                QVERIFY(std::abs(tf.restPos[static_cast<std::size_t>(fi)][k] - tc.restPos[static_cast<std::size_t>(ci)][k]) < 1e-9);
            }
        }
        // os indices mudam entre as malhas (por isso a selecao NAO pode guardar indices)
        bool renumbered = false;
        for (std::size_t i = 0; i < all.size(); ++i) {
            renumbered = renumbered || up.nodeIds[i] != all[i];
        }
        QVERIFY(renumbered);
        // da fina para a grossa: os nos que so existem na fina nao tem correspondente e sao relatados, nunca trocados por outro
        std::vector<int> allFine(tf.ids.begin(), tf.ids.end());
        const RefResolution down = resolveNodeRefs(tc, makeNodeRefs(tf, allFine));
        QCOMPARE(down.unresolved, static_cast<int>(tf.size() - tc.size()));
        QCOMPARE(down.nodeIds.size(), tc.size());
    }

    void referencesNeverSnapToADistantNode()
    {
        const Plate plate;
        const NodeTable table = buildNodeTable(plate.render);
        NodeRef near;
        near.position = table.restPos[4];
        near.position[0] += 2e-7;    // dentro da tolerancia: resolvida por proximidade
        NodeRef far;
        far.position = table.restPos[4];
        far.position[0] += 1e-3;     // a 1 mm: nao e o mesmo no
        NodeRef twin;
        twin.position = table.restPos[4];
        twin.position[1] += 3e-7;    // cai no mesmo no que `near`
        const RefResolution r = resolveNodeRefs(table, {near, far, twin});
        QCOMPARE(r.tableIndex[0], 4);
        QCOMPARE(r.tableIndex[1], -1);
        QCOMPARE(r.tableIndex[2], 4);
        QCOMPARE(r.unresolved, 1);
        QCOMPARE(r.snapped, 2);
        QCOMPARE(r.merged, 1);
        QCOMPARE(r.nodeIds.size(), std::size_t {1});
        // tolerancia maior deixa a de 1 mm resolver (e e contada como proximidade)
        QCOMPARE(resolveNodeRefs(table, {far}, 5e-3).tableIndex[0], 4);
        QCOMPARE(resolveNodeRefs(NodeTable {}, {near}).unresolved, 1);
        QCOMPARE(resolveNodeRefs(table, {}).nodeIds.size(), std::size_t {0});
    }

    void rendererHighlightsSelectedNodesWithoutAffectingPicking()
    {
        Plate plate;
        Structura::Visualization::ShellRenderer renderer;
        vtkNew<vtkRenderer> ren;
        vtkNew<vtkRenderWindow> window;
        window->SetOffScreenRendering(1);
        window->SetSize(400, 300);
        window->AddRenderer(ren);
        renderer.attach(ren);
        renderer.setMesh(plate.render);
        renderer.setVisible(true);
        vtkCamera *cam = ren->GetActiveCamera();
        cam->ParallelProjectionOn();
        cam->SetFocalPoint(1.0, 0.5, 0.0);
        cam->SetPosition(1.0, 0.5, 5.0);
        cam->SetViewUp(0, 1, 0);
        cam->SetParallelScale(0.8);
        ren->ResetCameraClippingRange();
        QCOMPARE(renderer.selectedNodeCount(), std::size_t {0});
        renderer.setSelectedNodes({{1.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}});
        QCOMPARE(renderer.selectedNodeCount(), std::size_t {2});
        QVERIFY(renderer.pickPoint(200, 150).quad >= 0);   // o destaque nao e "pickable"
        const int rebuilds = renderer.geometryRebuilds();
        renderer.setMesh(plate.render);                     // mesma geometria: o destaque nao forca reconstrucao
        QCOMPARE(renderer.geometryRebuilds(), rebuilds);
        QCOMPARE(renderer.selectedNodeCount(), std::size_t {2});
        renderer.setSelectedNodes({});
        QCOMPARE(renderer.selectedNodeCount(), std::size_t {0});
    }
};

QTEST_MAIN(TestShellSelection)
#include "TestShellSelection.moc"
