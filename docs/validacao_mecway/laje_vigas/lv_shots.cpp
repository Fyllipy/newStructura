// Prints do Structura dos casos laje + vigas + pilares (modo M, monolitico): laje colorida pelo deslocamento total, barras (tubos) com a mesma
// escala de cores, deformada e escala de cores (ColorLegendOverlay real). Para comparar com o print do Mecway 28 e para material de divulgacao.
// uso: lv_shots <pasta_saida> <rds.dll> [casos separados por virgula]
#define LV_NO_MAIN
#include "lv_dump.cpp"

#include "shell/ShellVisual.h"
#include "ui/ColorLegendOverlay.h"
#include "viz/structure/ShellRenderer.h"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkNew.h>
#include <vtkPNGWriter.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkTubeFilter.h>
#include <vtkUnsignedCharArray.h>
#include <vtkWindowToImageFilter.h>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 3) {
        std::fprintf(stderr, "uso: lv_shots <pasta_saida> <rds.dll> [casos]\n");
        return 2;
    }
    const QString outDir = args.at(1);
    QDir().mkpath(outDir);
    const SolverBridge solver(args.at(2));
    QStringList only;
    if (args.size() > 3) {
        only = args.at(3).split(',');
    }
    const int W = 1800, H = 1100;
    for (const Case &c : buildCases()) {
        if (!only.isEmpty() && !only.contains(QString::fromStdString(c.name))) {
            continue;
        }
        const Built b = buildBars(c);
        Sh::Model shell = buildShell(c);
        shell.coupled = true;
        shell.monolithic = true;
        SolverBridge::ModelData model = b.model;
        const auto mapping = ShellCouplingService::attach(model, shell, "Q", 1.0);
        std::fprintf(stderr, "[%s] resolvendo\n", c.name.c_str());
        const SolverBridge::Result r = solver.solve(model);
        std::fprintf(stderr, "[%s] resolvido\n", c.name.c_str());
        const Sh::CaseResult cr = ShellCouplingService::caseResult(*mapping, r, "Q");
        std::map<int, std::array<double, 6>> disp;
        for (const auto &n : r.nodeResults) {
            disp[n.nodeId] = n.displacements;
        }
        double umax = 0.0, lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
        for (std::size_t i = 0; i < mapping->mesh.nodes.size(); ++i) {
            const auto &u = cr.displacement[i];
            umax = std::max(umax, std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]));
        }
        for (const auto &n : model.nodes) {
            const std::array<double, 3> p {n.x, n.y, n.z};
            for (int k = 0; k < 3; ++k) {
                lo[k] = std::min(lo[k], p[static_cast<std::size_t>(k)]);
                hi[k] = std::max(hi[k], p[static_cast<std::size_t>(k)]);
            }
        }
        const double size = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]});
        Sh::RenderOptions opt;
        opt.quantity = Sh::Quantity::DisplacementMagnitude;
        opt.smooth = true;
        opt.edges = true;
        opt.deformScale = umax > 0 ? 0.05 * size / umax : 0.0;
        const Sh::RenderMesh render = Sh::buildRenderMesh(mapping->mesh, shell, &cr, opt);

        std::fprintf(stderr, "[%s] malha de render pronta\n", c.name.c_str());
        Structura::Visualization::ShellRenderer renderer;
        vtkNew<vtkRenderer> ren;
        vtkNew<vtkRenderWindow> window;
        window->SetOffScreenRendering(1);
        window->SetMultiSamples(8);
        window->SetSize(W, H);
        window->AddRenderer(ren);
        ren->SetViewport(0.0, 0.0, 0.74, 1.0);
        ren->SetBackground(1.0, 1.0, 1.0);
        ren->SetBackground2(0.86, 0.9, 0.96);
        ren->GradientBackgroundOn();
        renderer.attach(ren);
        renderer.setMesh(render);
        renderer.setVisible(true);

        // barras como tubos deformados (mesma escala da laje), coloridas pelo deslocamento total com a escala da laje
        vtkNew<vtkPoints> pts;
        vtkNew<vtkCellArray> lines;
        vtkNew<vtkUnsignedCharArray> colors;
        colors->SetNumberOfComponents(3);
        std::map<int, vtkIdType> pointOf;
        auto pointFor = [&](int id) {
            const auto it = pointOf.find(id);
            if (it != pointOf.end()) {
                return it->second;
            }
            const auto &pos = b.position.at(id);
            const auto &u = disp[id];
            const double mag = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]) * 1000.0;   // mm
            const vtkIdType pid = pts->InsertNextPoint(pos[0] + opt.deformScale * u[0], pos[1] + opt.deformScale * u[1], pos[2] + opt.deformScale * u[2]);
            const auto col = Sh::rainbow(Sh::colorPosition(mag, render.minValue, render.maxValue));
            colors->InsertNextTuple3(col[0], col[1], col[2]);
            pointOf[id] = pid;
            return pid;
        };
        for (const auto &bar : b.bars) {
            vtkIdType ids[2] = {pointFor(bar[0]), pointFor(bar[1])};
            lines->InsertNextCell(2, ids);
        }
        vtkNew<vtkPolyData> poly;
        poly->SetPoints(pts);
        poly->SetLines(lines);
        poly->GetPointData()->SetScalars(colors);
        vtkNew<vtkTubeFilter> tube;
        tube->SetInputData(poly);
        tube->SetRadius(0.17);
        tube->SetNumberOfSides(4);
        tube->SetCapping(1);
        vtkNew<vtkPolyDataMapper> mapper;
        mapper->SetInputConnection(tube->GetOutputPort());
        mapper->SetScalarModeToUsePointData();
        mapper->SetColorModeToDirectScalars();
        vtkNew<vtkActor> actor;
        actor->SetMapper(mapper);
        ren->AddActor(actor);

        const double cx = 0.5 * (lo[0] + hi[0]), cy = 0.5 * (lo[1] + hi[1]), cz = 0.5 * (lo[2] + hi[2]);
        vtkCamera *cam = ren->GetActiveCamera();
        cam->SetFocalPoint(cx, cy, cz);
        cam->SetPosition(cx + 1.2 * size, cy - 1.5 * size, cz + 1.1 * size);
        cam->SetViewUp(0, 0, 1);
        ren->ResetCamera();
        cam->Zoom(1.1);
        std::fprintf(stderr, "[%s] render\n", c.name.c_str());
        window->Render();

        vtkNew<vtkWindowToImageFilter> grab;
        grab->SetInput(window);
        grab->SetInputBufferTypeToRGB();
        grab->ReadFrontBufferOff();
        grab->Update();
        const QString raw = QDir(outDir).filePath(QString::fromStdString(c.name) + "_raw.png");
        vtkNew<vtkPNGWriter> writer;
        writer->SetFileName(raw.toLocal8Bit().constData());
        writer->SetInputConnection(grab->GetOutputPort());
        writer->Write();

        QImage image(raw);
        if (image.isNull()) {
            std::fprintf(stderr, "[%s] imagem nao gerada\n", c.name.c_str());
            continue;
        }
        image = image.convertToFormat(QImage::Format_ARGB32);
        QWidget host;
        host.resize(image.width(), image.height());
        QFont font = host.font();
        font.setPointSize(12);
        host.setFont(font);
        Structura::UI::ColorLegendOverlay legend(&host);
        legend.setFont(font);
        legend.setLegend(QStringLiteral("Deslocamento total"), QStringLiteral("mm"), render.minValue, render.maxValue, false, true);
        const QPixmap legendPix = legend.grab();
        QPainter painter(&image);
        {
            QLinearGradient grad(0, 0, 0, image.height());
            grad.setColorAt(0.0, QColor(219, 230, 245));
            grad.setColorAt(1.0, QColor(255, 255, 255));
            const int x0 = static_cast<int>(std::floor(0.74 * image.width()));
            painter.fillRect(QRect(x0, 0, image.width() - x0, image.height()), grad);
        }
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.drawPixmap(image.width() - legendPix.width() - 30, (image.height() - legendPix.height()) / 2, legendPix);
        QFont title = font;
        title.setPointSize(22);
        title.setBold(true);
        painter.setFont(title);
        painter.setPen(QColor(30, 35, 43));
        painter.drawText(QRect(40, 28, image.width() - 80, 50), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Structura — ") + QString::fromStdString(c.name));
        QFont sub = font;
        sub.setPointSize(14);
        painter.setFont(sub);
        painter.setPen(QColor(70, 78, 90));
        painter.drawText(QRect(40, 78, image.width() - 80, 90), Qt::AlignLeft | Qt::TextWordWrap, QString::fromStdString(c.description));
        painter.end();
        const QString finalPath = QDir(outDir).filePath(QString::fromStdString(c.name) + "_structura.png");
        image.save(finalPath);
        std::printf("[%s] %s  (min %.4g, max %.4g mm; deformada x%.0f)\n", c.name.c_str(), finalPath.toLocal8Bit().constData(), render.minValue, render.maxValue, opt.deformScale);
    }
    return 0;
}
