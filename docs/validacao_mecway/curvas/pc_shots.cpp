// Prints do Structura, pinched cylinder e hemisferio com furo (malha colorida pelo deslocamento total + escala de cores) dos casos de validacao, em alta resolucao, para material de divulgacao.
// uso: pc_shots <pasta_saida> <mefs.dll> [malhas separadas por virgula, ex.: 16,32]
#include "app/ShellSolverBridge.h"
#include "shell/ShellJob.h"
#include "shell/ShellResults.h"
#include "shell/ShellVisual.h"
#include "ui/ColorLegendOverlay.h"
#define PC_NO_MAIN
#include "pc_dump.cpp"
#include "viz/structure/ShellRenderer.h"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>

#include <vtkCamera.h>
#include <vtkNew.h>
#include <vtkPNGWriter.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkWindowToImageFilter.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace Structura::Shell;

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 3) {
        std::fprintf(stderr, "uso: sl_shots <pasta_saida> <mefs.dll> [casos]\n");
        return 2;
    }
    const QString outDir = args.at(1);
    QDir().mkpath(outDir);
    const Structura::App::ShellSolverBridge bridge(args.at(2));
    QStringList only;
    if (args.size() > 3) {
        only = args.at(3).split(',');
    }
    const int W = 1800, H = 1100;
    struct SlCase {
        std::string name, description;
        Model model;
        int axial;
    };
    std::vector<SlCase> slCases;
    for (const int n : {8, 16, 32}) {
        slCases.push_back({"pinched_cylinder_" + std::to_string(n), "Pinched cylinder (modelo de 1/8): duas cargas radiais opostas, diafragmas rigidos nas pontas; elementos de " + std::to_string(300.0 / n) + " de lado", pc::pinchedCylinder(300.0 / n).model, n});
        slCases.push_back({"hemisferio_furo18_" + std::to_string(n), "Hemisferio com furo de 18 graus (modelo de 1/4): quatro cargas radiais alternadas; " + std::to_string(n) + " elementos no meridiano", pc::hemisphere(n).model, n});
    }
    for (const SlCase &c : slCases) {
        if (!only.isEmpty() && !only.contains(QString::number(c.axial))) {
            continue;
        }
        const Mesh mesh = generateMesh(c.model);
        const Job job = buildJob(c.model, mesh, {"Q"});
        const Results results = bridge.solve(c.model, mesh, job);
        if (!results.ok()) {
            std::fprintf(stderr, "[%s] falha: %s\n", c.name.c_str(), results.error.c_str());
            continue;
        }
        const CaseResult &r = results.cases.front();
        // deformada com amplitude de ~6% da maior dimensao do modelo
        double umax = 0.0, lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
        for (std::size_t i = 0; i < mesh.nodes.size(); ++i) {
            umax = std::max(umax, std::sqrt(r.displacement[i][0] * r.displacement[i][0] + r.displacement[i][1] * r.displacement[i][1] + r.displacement[i][2] * r.displacement[i][2]));
            for (int k = 0; k < 3; ++k) {
                lo[k] = std::min(lo[k], mesh.nodes[i][static_cast<std::size_t>(k)]);
                hi[k] = std::max(hi[k], mesh.nodes[i][static_cast<std::size_t>(k)]);
            }
        }
        const double size = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]});
        RenderOptions opt;
        opt.quantity = Quantity::DisplacementMagnitude;
        opt.smooth = true;
        opt.edges = c.name.find("fine") == std::string::npos;
        opt.deformScale = umax > 0 ? 0.06 * size / umax : 0.0;
        const RenderMesh render = buildRenderMesh(mesh, c.model, &r, opt);

        Structura::Visualization::ShellRenderer renderer;
        vtkNew<vtkRenderer> ren;
        vtkNew<vtkRenderWindow> window;
        window->SetOffScreenRendering(1);
        window->SetMultiSamples(8);
        window->SetSize(W, H);
        window->AddRenderer(ren);
        ren->SetViewport(0.0, 0.0, 0.74, 1.0);   // a faixa da direita fica para a escala de cores
        ren->SetBackground(1.0, 1.0, 1.0);
        ren->SetBackground2(0.86, 0.9, 0.96);
        ren->GradientBackgroundOn();
        renderer.attach(ren);
        renderer.setMesh(render);
        renderer.setVisible(true);
        const double cx = 0.5 * (lo[0] + hi[0]), cy = 0.5 * (lo[1] + hi[1]), cz = 0.5 * (lo[2] + hi[2]);
        vtkCamera *cam = ren->GetActiveCamera();
        cam->SetFocalPoint(cx, cy, cz);
        const bool tall = (hi[2] - lo[2]) > 0.5 * std::max(hi[0] - lo[0], hi[1] - lo[1]);
        cam->SetPosition(cx + 1.0 * size * (tall ? 1.6 : 1.2), cy - 1.3 * size * (tall ? 1.6 : 1.2), cz + (tall ? 0.7 : 1.0) * size * (tall ? 1.6 : 1.2));
        cam->SetViewUp(0, 0, 1);
        if (c.name.find("hemisferio") != std::string::npos) {
            cam->SetPosition(cx + 2.0 * size, cy + 2.0 * size, cz + 1.4 * size);   // vista pela face externa (convexa)
        }
        ren->ResetCamera();
        cam->Zoom(tall ? 1.15 : 1.12);
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

        // composicao: imagem 3D + escala de cores (ColorLegendOverlay real) + titulo
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
            // faixa da direita (fora do viewport do VTK): mesmo degrade de fundo
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
