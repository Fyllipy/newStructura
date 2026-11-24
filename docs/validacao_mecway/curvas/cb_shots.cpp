// Prints do cilindro (flambagem e vibracao): forma do modo 1 do Structura (rds.dll) e do Mecway 28, renderizadas com a MESMA malha, o mesmo renderizador e a
// mesma escala de cores em faixas do Structura (as duas formas sao normalizadas a deslocamento maximo 1; o sinal e a orientacao dos modos duplos sao arbitrarios).
// uso: cb_shots <pasta_saida> <pasta_case_cb_dump> <pasta_mecway> <h>
#include "shell/ShellMesh.h"
#include "shell/ShellResults.h"
#include "shell/ShellVisual.h"
#include "ui/ColorLegendOverlay.h"
#define CB_NO_MAIN
#include "cb_dump.cpp"
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
#include <sstream>

using namespace Structura::Shell;

namespace {

struct Row {
    double x, y, z, u, v, w;
};

std::vector<Row> readShape(const std::string &path)
{
    std::vector<Row> rows;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        std::stringstream ss(line);
        std::string cell;
        Row r {};
        double *p = &r.x;
        int k = 0;
        while (std::getline(ss, cell, ',') && k < 6) {
            p[k++] = cell.empty() ? 0.0 : std::stod(cell);
        }
        if (k == 6) {
            rows.push_back(r);
        }
    }
    return rows;
}

std::string readValue(const std::string &path, const std::string &key)
{
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind(key + "=", 0) == 0) {
            return line.substr(key.size() + 1);
        }
    }
    return {};
}

long long keyOf(double v)
{
    return std::llround(v * 1e7);
}

void shot(const std::string &outDir, const std::string &file, const Mesh &mesh, const Model &model, const std::vector<Row> &shape, const std::string &title, const std::string &subtitle)
{
    // deslocamento por no (coordenadas) normalizado a maximo 1
    std::map<std::tuple<long long, long long, long long>, std::array<double, 3>> byPoint;
    double umax = 0.0;
    for (const Row &r : shape) {
        byPoint[{keyOf(r.x), keyOf(r.y), keyOf(r.z)}] = {r.u, r.v, r.w};
        umax = std::max(umax, std::sqrt(r.u * r.u + r.v * r.v + r.w * r.w));
    }
    CaseResult result;
    result.name = "Q";
    result.displacement.assign(mesh.nodes.size(), {0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    result.elements.assign(mesh.quads.size(), ElementState {});   // sem esforcos: so o deslocamento e colorido
    int missing = 0;
    for (std::size_t i = 0; i < mesh.nodes.size(); ++i) {
        const auto it = byPoint.find({keyOf(mesh.nodes[i][0]), keyOf(mesh.nodes[i][1]), keyOf(mesh.nodes[i][2])});
        if (it == byPoint.end()) {
            ++missing;
            continue;
        }
        for (int k = 0; k < 3; ++k) {
            result.displacement[i][static_cast<std::size_t>(k)] = umax > 0 ? it->second[static_cast<std::size_t>(k)] / umax : 0.0;
        }
    }
    if (missing > 0) {
        std::fprintf(stderr, "[%s] %d nos sem valor no arquivo da forma\n", file.c_str(), missing);
    }
    const int W = 1800, H = 1100;
    RenderOptions opt;
    opt.quantity = Quantity::DisplacementMagnitude;
    opt.smooth = true;
    opt.edges = false;
    opt.deformScale = 0.12;   // forma normalizada: amplitude maxima de 0,12 m num cilindro de 2 m
    const RenderMesh render = buildRenderMesh(mesh, model, &result, opt);
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
    vtkCamera *cam = ren->GetActiveCamera();
    cam->SetFocalPoint(0.0, 0.0, 1.0);
    cam->SetPosition(4.2, -4.6, 3.4);
    cam->SetViewUp(0, 0, 1);
    ren->ResetCamera();
    cam->Zoom(1.1);
    window->Render();
    vtkNew<vtkWindowToImageFilter> grab;
    grab->SetInput(window);
    grab->SetInputBufferTypeToRGB();
    grab->ReadFrontBufferOff();
    grab->Update();
    const QString raw = QDir(QString::fromStdString(outDir)).filePath(QString::fromStdString(file) + "_raw.png");
    vtkNew<vtkPNGWriter> writer;
    writer->SetFileName(raw.toLocal8Bit().constData());
    writer->SetInputConnection(grab->GetOutputPort());
    writer->Write();
    QImage image(raw);
    if (image.isNull()) {
        std::fprintf(stderr, "[%s] imagem nao gerada\n", file.c_str());
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
    legend.setLegend(QStringLiteral("Forma normalizada (x1000)"), QStringLiteral(""), render.minValue, render.maxValue, false, true);
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
    const QString finalPath = QDir(QString::fromStdString(outDir)).filePath(QString::fromStdString(file) + ".png");
    image.save(finalPath);
    std::printf("%s\n", finalPath.toLocal8Bit().constData());
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 5) {
        std::fprintf(stderr, "uso: cb_shots <saida> <case> <mecway> <h>\n");
        return 2;
    }
    const std::string out = args.at(1).toStdString(), cdir = args.at(2).toStdString(), mdir = args.at(3).toStdString();
    const double h = args.at(4).toDouble();
    QDir().mkpath(args.at(1));
    const Model model = cb::cylinder(h);
    const Mesh mesh = generateMesh(model);
    auto num = [](const std::string &s) { return s.empty() ? std::string("?") : s; };
    auto fmt = [](const std::string &s, double scale, const char *f) {
        char buf[64];
        std::snprintf(buf, sizeof buf, f, std::stod(s) * scale);
        return std::string(buf);
    };
    const std::string classico = readValue(cdir + "/s_buckling.txt", "classico");
    const std::string sb = readValue(cdir + "/s_buckling.txt", "fator1"), mb = readValue(mdir + "/cil_buckling_mw_result.txt", "fator1");
    const std::string sm = readValue(cdir + "/s_modal.txt", "f1"), mm = readValue(mdir + "/cil_modal_mw_result.txt", "f1");
    const std::string geo = "Cilindro R = 1 m, L = 2 m, t = 10 mm (aco), malha " + std::to_string(mesh.quads.size()) + " elementos. ";
    if (!sb.empty()) {
        shot(out, "cilindro_flambagem_structura", mesh, model, readShape(cdir + "/s_buckling_mode1.csv"), "Structura — flambagem axial de casca cilindrica",
             geo + "Modo 1: carga critica " + fmt(sb, 1e-6, "%.3f") + " MN/m (classica: " + fmt(classico, 1e-6, "%.3f") + " MN/m)");
    }
    if (!mb.empty()) {
        shot(out, "cilindro_flambagem_mecway", mesh, model, readShape(mdir + "/cil_buckling_mw_mode1.csv"), "Mecway 28 — flambagem axial de casca cilindrica",
             geo + "Modo 1: carga critica " + fmt(mb, 1e-6, "%.3f") + " MN/m (mesma malha; resultado renderizado com a escala do Structura)");
    }
    if (!sm.empty()) {
        shot(out, "cilindro_modal_structura", mesh, model, readShape(cdir + "/s_modal_mode1.csv"), "Structura — vibracao livre de casca cilindrica",
             geo + "Modo 1: frequencia " + fmt(sm, 1.0, "%.2f") + " Hz");
    }
    if (!mm.empty()) {
        shot(out, "cilindro_modal_mecway", mesh, model, readShape(mdir + "/cil_modal_mw_mode1.csv"), "Mecway 28 — vibracao livre de casca cilindrica",
             geo + "Modo 1: frequencia " + fmt(mm, 1.0, "%.2f") + " Hz (mesma malha; resultado renderizado com a escala do Structura)");
    }
    (void)num;
    return 0;
}
