#include "viz/structure/MemberRenderer.h"

#include <QTest>

#include <vtkActor.h>
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
};

QTEST_GUILESS_MAIN(TestAppearanceRender)
#include "TestAppearanceRender.moc"
