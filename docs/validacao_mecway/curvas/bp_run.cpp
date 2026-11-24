// Comparacao com o exemplo do Mecway "BucklingPlate.liml" (placa 1 x 2 x 0,0125 m modelada com solidos hex20, E = 1e6 Pa, nu = 0,3, apoiada nas quatro bordas no plano medio,
// compressao no sentido de y por pressao de 80 Pa nas faces da borda y = 2, analise B30): o equivalente em CASCA no Structura (placa 1 x 2, malha quadrada de 1/14 m), a mesma
// compressao (1 N/m: 80 Pa x 0,0125 m) e a flambagem pelo rds.dll. O desenho do modo usa a MESMA funcao da janela (BucklingService::shellShapeMesh).
// uso: bp_run solve  <rds.dll> <pasta_saida>
//      bp_run render <rds.dll> <pasta_saida> <forma.csv> <nome_png> <titulo> <subtitulo>      (forma.csv: x,y,z,u,v,w por no; coordenadas do no de casca)
#include "app/SolverBridge.h"
#include "app/services/BucklingService.h"
#include "app/services/ShellCouplingService.h"
#include "shell/ShellMesh.h"
#include "shell/ShellModel.h"
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

#include <vtkCamera.h>
#include <vtkNew.h>
#include <vtkPNGWriter.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkWindowToImageFilter.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <tuple>

using Structura::App::SolverBridge;
using Structura::App::Services::BucklingService;
using Structura::App::Services::ShellCouplingService;
namespace Sh = Structura::Shell;

namespace {

constexpr double kE = 1.0e6, kNu = 0.3, kT = 0.0125, kA = 1.0, kB = 2.0;

Sh::Model plate()
{
    Sh::Model model;
    model.materials[0].E = kE;
    model.materials[0].nu = kNu;
    model.materials[0].unitWeight = 0.0;
    Sh::Object slab;
    slab.id = 1;
    slab.name = "Placa";
    slab.kind = Sh::Kind::Plate;
    slab.polygon = {{0, 0, 0}, {kA, 0, 0}, {kA, kB, 0}, {0, kB, 0}};
    slab.p = {{"t", kT}};
    slab.meshSize = kA / 14.0;
    model.objects = {slab};
    auto edge = [](int axis, double value, std::array<int, 6> fixed) {
        Sh::Support s;
        s.selector = Sh::Support::Plane;
        s.axis = axis;
        s.value = value;
        s.tol = 1.0e-6;
        s.fixed = fixed;
        return s;
    };
    // apoio simples nas quatro bordas (uz = 0); uy = 0 na borda y = 0; ux = 0 num ponto do canto (movimento de corpo rigido)
    Sh::Support corner;
    corner.selector = Sh::Support::Point;
    corner.value = 0.0;
    corner.value2 = 0.0;
    corner.value3 = 0.0;
    corner.fixed = {1, 0, 0, 0, 0, 0};
    model.supports = {edge(0, 0.0, {0, 0, 1, 0, 0, 0}), edge(0, kA, {0, 0, 1, 0, 0, 0}), edge(1, 0.0, {0, 1, 1, 0, 0, 0}), edge(1, kB, {0, 0, 1, 0, 0, 0}), corner};
    const Sh::Mesh mesh = Sh::generateMesh(model);
    Sh::Load line;
    line.caseName = "Q";
    line.type = Sh::Load::LinePoints;
    line.v = {0.0, -80.0 * kT, 0.0, 0.0};   // 1 N/m de compressao na borda y = 2 (80 Pa x 12,5 mm)
    for (const auto &p : mesh.nodes) {
        if (std::abs(p[1] - kB) < 1e-9) {
            line.points.push_back(p);
        }
    }
    model.loads = {line};
    return model;
}

struct Solved {
    SolverBridge::ModelData data;
    SolverBridge::Result result;
};

Solved solve(const SolverBridge &solver, int modes)
{
    Solved s;
    Sh::Model coupled = plate();
    coupled.coupled = true;
    coupled.monolithic = true;
    ShellCouplingService::attach(s.data, coupled, "Q", 1.0);
    s.data.materials.clear();
    s.data.sections.clear();
    s.data.bucklingModes = modes;
    s.result = solver.solve(s.data);
    return s;
}

void render(const std::filesystem::path &dir, const SolverBridge::ModelData &data, const std::vector<double> &shape, const std::string &name, const std::string &title, const std::string &subtitle)
{
    const Structura::Shell::RenderMesh mesh = BucklingService::shellShapeMesh(data, {}, shape, 0.3);
    const int W = 1800, H = 1100;
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
    renderer.setMesh(mesh);
    renderer.setVisible(true);
    vtkCamera *cam = ren->GetActiveCamera();
    cam->SetFocalPoint(0.5, 1.0, 0.0);
    cam->SetPosition(3.2, -2.4, 2.6);
    cam->SetViewUp(0, 0, 1);
    ren->ResetCamera();
    cam->Zoom(1.15);
    window->Render();
    vtkNew<vtkWindowToImageFilter> grab;
    grab->SetInput(window);
    grab->SetInputBufferTypeToRGB();
    grab->ReadFrontBufferOff();
    grab->Update();
    const QString raw = QString::fromStdString((dir / (name + "_raw.png")).string());
    vtkNew<vtkPNGWriter> writer;
    writer->SetFileName(raw.toLocal8Bit().constData());
    writer->SetInputConnection(grab->GetOutputPort());
    writer->Write();
    QImage image(raw);
    if (image.isNull()) {
        std::fprintf(stderr, "imagem nao gerada\n");
        return;
    }
    image = image.convertToFormat(QImage::Format_ARGB32);
    QWidget host;
    host.resize(image.width(), image.height());
    QFont font = host.font();
    font.setPointSize(12);
    host.setFont(font);
    Structura::UI::ColorLegendOverlay legend(&host);
    legend.setFont(font);
    legend.setLegend(QStringLiteral("Forma normalizada (x1000)"), QStringLiteral(""), mesh.minValue, mesh.maxValue, false, true);
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
    QFont big = font;
    big.setPointSize(22);
    big.setBold(true);
    painter.setFont(big);
    painter.setPen(QColor(30, 35, 43));
    painter.drawText(QRect(40, 28, image.width() - 80, 50), Qt::AlignLeft | Qt::AlignVCenter, QString::fromStdString(title));
    QFont sub = font;
    sub.setPointSize(14);
    painter.setFont(sub);
    painter.setPen(QColor(70, 78, 90));
    painter.drawText(QRect(40, 78, image.width() - 80, 90), Qt::AlignLeft | Qt::TextWordWrap, QString::fromStdString(subtitle));
    painter.end();
    image.save(QString::fromStdString((dir / (name + ".png")).string()));
    std::printf("%s\n", (dir / (name + ".png")).string().c_str());
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 4) {
        std::fprintf(stderr, "uso: bp_run solve|render <rds.dll> <pasta> ...\n");
        return 2;
    }
    const SolverBridge solver(args.at(2));
    const std::filesystem::path dir = std::filesystem::u8path(args.at(3).toUtf8().toStdString());
    std::filesystem::create_directories(dir);
    const Solved s = solve(solver, 6);
    if (s.result.bucklingFactors.empty()) {
        std::fprintf(stderr, "sem fatores de flambagem\n");
        return 1;
    }
    const double D = kE * kT * kT * kT / (12.0 * (1.0 - kNu * kNu));
    const double ncr = 4.0 * 3.14159265358979323846 * 3.14159265358979323846 * D / (kA * kA);   // N/m, placa apoiada nas quatro bordas, a/b = 2 (k = 4)
    if (args.at(1) == QLatin1String("solve")) {
        std::ofstream o(dir / "s_buckling.txt");
        o.precision(10);
        o << "analitico=" << ncr << "\n";
        for (std::size_t k = 0; k < s.result.bucklingFactors.size(); ++k) {
            o << "fator" << k + 1 << '=' << s.result.bucklingFactors[k] << "\n";
            std::printf("modo %zu: %.5f (analitico do modo 1: %.5f)\n", k + 1, s.result.bucklingFactors[k], ncr);
            std::ofstream shape(dir / ("s_mode" + std::to_string(k + 1) + ".csv"));
            shape.precision(17);
            const auto &v = s.result.bucklingShellShapes[k];
            for (std::size_t i = 0; i < s.data.shell->nodes.size(); ++i) {
                const auto &n = s.data.shell->nodes[i];
                shape << n.x << ',' << n.y << ',' << n.z << ',' << v[6 * i] << ',' << v[6 * i + 1] << ',' << v[6 * i + 2] << '\n';
            }
        }
        // malha dos nos de casca (para reamostrar a forma do Mecway)
        std::ofstream nodes(dir / "nodes.csv");
        nodes.precision(17);
        for (const auto &n : s.data.shell->nodes) {
            nodes << n.x << ',' << n.y << ',' << n.z << '\n';
        }
        char buf[200];
        std::snprintf(buf, sizeof buf, "Structura (casca): modo 1, fator %.4f (analitico %.4f)", s.result.bucklingFactors[0], ncr);
        render(dir, s.data, s.result.bucklingShellShapes[0], "placa_flambagem_structura", "Structura — flambagem de placa (BucklingPlate do Mecway)",
               std::string("Placa 1 x 2 x 0,0125 m, apoiada nas quatro bordas, compressao na direcao de y. ") + buf);
        return 0;
    }
    // render: forma lida de um CSV (coordenadas x,y,z do no de casca)
    if (args.size() < 8) {
        return 2;
    }
    std::map<std::tuple<long long, long long>, std::array<double, 3>> byPoint;
    {
        std::ifstream f(std::filesystem::u8path(args.at(4).toUtf8().toStdString()));
        std::string line;
        while (std::getline(f, line)) {
            std::stringstream ss(line);
            std::string cell;
            double v[6] = {};
            int k = 0;
            while (std::getline(ss, cell, ',') && k < 6) {
                v[k++] = cell.empty() ? 0.0 : std::stod(cell);
            }
            if (k == 6) {
                byPoint[{std::llround(v[0] * 1e7), std::llround(v[1] * 1e7)}] = {v[3], v[4], v[5]};
            }
        }
    }
    std::vector<double> shape(s.data.shell->nodes.size() * 6, 0.0);
    int missing = 0;
    for (std::size_t i = 0; i < s.data.shell->nodes.size(); ++i) {
        const auto &n = s.data.shell->nodes[i];
        const auto it = byPoint.find({std::llround(n.x * 1e7), std::llround(n.y * 1e7)});
        if (it == byPoint.end()) {
            ++missing;
            continue;
        }
        for (int k = 0; k < 3; ++k) {
            shape[6 * i + static_cast<std::size_t>(k)] = it->second[static_cast<std::size_t>(k)];
        }
    }
    if (missing > 0) {
        std::fprintf(stderr, "%d nos sem valor\n", missing);
    }
    render(dir, s.data, shape, args.at(5).toStdString(), args.at(6).toStdString(), args.at(7).toStdString());
    return 0;
}
