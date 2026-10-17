#include "viz/structure/MemberRenderer.h"
#include "viz/structure/SupportMeshBuilder.h"
#include <vtkPolyData.h>
#include "viz/structure/StructureRenderer.h"
#include "viz/postprocessor/PostProcessorRenderer.h"

#include <QTest>

#include <vtkActor.h>
#include <vtkCellPicker.h>
#include <vtkPointPicker.h>
#include <vtkCamera.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkUnsignedCharArray.h>

#include <vector>

using namespace Structura::Visualization;

namespace {

struct Counts {
    int top {0};
    int bottom {0};
    int topLight {0};
};

// Renderiza duas barras horizontais (superior = B, inferior = A) e conta pixels diferentes do fundo.
Counts render(const AppearanceMap &appearance, const QUuid &a, const QUuid &b, MemberRenderer &renderer,
              vtkRenderWindow *window, vtkRenderer *vtkRen)
{
    renderer.setAppearance(appearance);
    window->Render();
    vtkNew<vtkUnsignedCharArray> pixels;
    window->GetPixelData(0, 0, 199, 199, 1, pixels);
    Counts counts;
    for (int y = 0; y < 200; ++y) {
        for (int x = 0; x < 200; ++x) {
            unsigned char rgb[3];
            pixels->GetTypedTuple((y * 200 + x), rgb);
            const bool nonWhite = rgb[0] < 250 || rgb[1] < 250 || rgb[2] < 250;
            if (!nonWhite) {
                continue;
            }
            // y cresce para cima no GetPixelData
            if (y >= 100) {
                ++counts.top;
                if (rgb[0] > 190) {
                    ++counts.topLight;
                }
            } else {
                ++counts.bottom;
            }
        }
    }
    Q_UNUSED(a) Q_UNUSED(b) Q_UNUSED(vtkRen)
    return counts;
}

} // namespace

class TestAppearanceRender : public QObject
{
    Q_OBJECT

private slots:
    void supportsAndNodalConstraintsAreDrawnSeparately()
    {
        using Structura::Visualization::SupportMeshBuilder;
        using R = SupportMeshBuilder::Restraints;
        const R fixed {true, true, true, true, true, true}, pinned {true, true, true, false, false, false}, roller {false, false, true, false, false, false};
        const R lockY {false, true, false, false, false, false}, rotation {false, false, false, true, false, false};
        for (const R &r : {fixed, pinned, roller}) {
            QVERIFY(SupportMeshBuilder::isSupport(r));
        }
        for (const R &r : {lockY, rotation}) {
            QVERIFY(!SupportMeshBuilder::isSupport(r));
        }
        auto cells = [](const R &r, bool supports) {
            const std::vector<SupportMeshBuilder::Support> list {{{0.0, 0.0, 0.0}, r}};
            auto data = supports ? SupportMeshBuilder::buildSupports(list, 0.3) : SupportMeshBuilder::buildConstraints(list, 0.3);
            return data->GetNumberOfCells();
        };
        for (const R &r : {fixed, pinned, roller}) {
            QVERIFY(cells(r, true) > 0);
            QCOMPARE(cells(r, false), 0);   // apoio nao gera simbolo de restricao
        }
        for (const R &r : {lockY, rotation}) {
            QVERIFY(cells(r, false) > 0);
            QCOMPARE(cells(r, true), 0);
        }
        QVERIFY(cells(roller, true) > cells(pinned, true));   // o apoio movel acrescenta os rolos
    }

    void hiddenAndDimmedBarsRenderAsExpected()
    {
        vtkNew<vtkPoints> points;
        points->InsertNextPoint(-1.0, -0.5, 0.0);
        points->InsertNextPoint(1.0, -0.5, 0.0);
        points->InsertNextPoint(-1.0, 0.5, 0.0);
        points->InsertNextPoint(1.0, 0.5, 0.0);

        const QUuid a = QUuid::createUuid(); // inferior
        const QUuid b = QUuid::createUuid(); // superior
        MemberRenderer renderer;
        renderer.setPoints(points);
        renderer.addBar(a, 0, 1);
        renderer.addBar(b, 2, 3);

        vtkNew<vtkRenderer> vtkRen;
        vtkRen->SetBackground(1, 1, 1);
        renderer.addToRenderer(vtkRen);
        vtkRen->GetActiveCamera()->ParallelProjectionOn();
        vtkRen->ResetCamera();

        vtkNew<vtkRenderWindow> window;
        window->SetOffScreenRendering(1);
        window->SetMultiSamples(0);
        window->SetSize(200, 200);
        window->AddRenderer(vtkRen);

        const Counts normal = render({}, a, b, renderer, window, vtkRen);
        QVERIFY(normal.top > 50);
        QVERIFY(normal.bottom > 50);

        AppearanceMap hidden;
        ElementAppearance hide;
        hide.state = ElementAppearance::State::Hidden;
        hidden.insert(b, hide);
        const Counts hiddenCounts = render(hidden, a, b, renderer, window, vtkRen);
        QCOMPARE(hiddenCounts.top, 0);
        QVERIFY(hiddenCounts.bottom > 50);

        AppearanceMap dimmed;
        ElementAppearance dim;
        dim.state = ElementAppearance::State::Dimmed;
        dimmed.insert(b, dim);
        const Counts dimCounts = render(dimmed, a, b, renderer, window, vtkRen);
        QVERIFY(dimCounts.top > 0);
        QVERIFY(dimCounts.topLight > normal.topLight);
    }

    void diagramsRebuildWithStaleScaleLabelsAndHatch()
    {
        vtkNew<vtkRenderer> ren;
        vtkNew<vtkRenderWindow> window;
        window->SetOffScreenRendering(1);
        window->SetMultiSamples(0);
        window->SetSize(300, 300);
        window->AddRenderer(ren);
        PostProcessorRenderer post;
        post.initialize(ren);
        std::vector<BeamElementResults> results;
        for (int i = 0; i < 6; ++i) {
            BeamElementResults el;
            el.externalId = i + 1;
            el.node1 = {double(i), 0.0, 0.0};
            el.node2 = {double(i) + 1.0, 0.0, 0.0};
            el.xLocal = {1, 0, 0};
            el.yLocal = {0, 1, 0};
            el.zLocal = {0, 0, 1};
            el.endForces.Mz1 = 10.0 * (i + 1);
            el.endForces.Mz2 = -5.0 * (i + 1);
            el.endForces.Vy1 = 3.0;
            el.endForces.Vy2 = -3.0;
            UniformDistributedLoad load;
            load.qy = -4.0;
            el.distributedLoads.push_back(load);
            results.push_back(el);
        }
        post.setResults(results, 1);
        post.activate();
        for (const auto type : {DiagramType::N, DiagramType::Vy, DiagramType::Vz, DiagramType::Mt, DiagramType::My, DiagramType::Mz}) {
            post.showDiagram(type);
            window->Render();
            post.setLabelMode(PostProcessorRenderer::LabelMode::PerMember);
            post.setHatchVisible(false);
            post.setStale(true);
            post.setUserScale(2.0);
            post.setStale(false);
            post.setHatchVisible(true);
            post.setLabelMode(PostProcessorRenderer::LabelMode::Global);
            window->Render();
            const auto summary = post.summary();
            QVERIFY(summary.valid);
        }
        post.hideDiagram();
        post.deactivate();
        post.setStale(true);   // sem diagrama ativo
        post.setResults({}, 2);
        post.activate();
        post.showDiagram(DiagramType::Mz);
        post.deactivate();
    }

    void pickingSurvivesAddingBarsAndRebuilding()
    {
        StructureRenderer structure;
        vtkNew<vtkRenderer> ren;
        structure.addToRenderer(ren);
        vtkNew<vtkRenderWindow> window;
        window->SetOffScreenRendering(1);
        window->SetMultiSamples(0);
        window->SetSize(300, 300);
        window->AddRenderer(ren);
        vtkNew<vtkCellPicker> barPicker;
        barPicker->SetPickFromList(true);
        barPicker->SetTolerance(0.005);
        barPicker->AddPickList(structure.getBarActor());
        vtkNew<vtkPointPicker> nodePicker;
        nodePicker->SetPickFromList(true);
        nodePicker->SetTolerance(0.01);
        nodePicker->AddPickList(structure.getNodeActor());
        auto pickAll = [&]() {
            for (int x = 0; x < 300; x += 25) {
                for (int y = 0; y < 300; y += 25) {
                    barPicker->Pick(x, y, 0.0, ren);
                    nodePicker->Pick(x, y, 0.0, ren);
                }
            }
        };
        std::vector<QUuid> nodes;
        auto addNode = [&](double x, double y) {
            nodes.push_back(QUuid::createUuid());
            structure.addNode(nodes.back(), x, y, 0.0);
        };
        addNode(0, 0);
        addNode(4, 0);
        addNode(4, 3);
        structure.addBar(QUuid::createUuid(), nodes[0], nodes[1]);
        structure.addBar(QUuid::createUuid(), nodes[1], nodes[2]);
        ren->ResetCamera();
        window->Render();
        pickAll();
        // acrescenta barras SEM renderizar antes de pegar (como o espelhamento com copia faz) e pega de novo
        addNode(0, 3);
        structure.addBar(QUuid::createUuid(), nodes[2], nodes[3]);
        structure.addBar(QUuid::createUuid(), nodes[3], nodes[0]);
        pickAll();
        window->Render();
        pickAll();
        // reconstrucao completa (limpa e recria, como restoreGeometry)
        structure.clear();
        nodes.clear();
        addNode(0, 0);
        addNode(2, 0);
        structure.addBar(QUuid::createUuid(), nodes[0], nodes[1]);
        pickAll();
        window->Render();
        pickAll();
        QVERIFY(true);
    }
};

QTEST_GUILESS_MAIN(TestAppearanceRender)
#include "TestAppearanceRender.moc"
