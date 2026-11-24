// Prints da torre: barras deformadas (tubos) coloridas pelo deslocamento nodal com a escala em faixas do Structura (a mesma da vista de cascas), sobre a estrutura indeformada
// em cinza. Entrada: geometria (torre_geom.csv, linhas N e M) e deslocamentos por no (id,x,y,z,ux,uy,uz,...).
// uso: tower_shots <geom.csv> <desloc.csv> <saida.png> <titulo> <subtitulo> <titulo_legenda> <unidade> <modo:0|1> <amplitude_visual_m> <fator_cor>
//   modo = 1: forma normalizada a maior translacao 1 (modos de vibracao/flambagem); modo = 0: deslocamento real (a amplitude visual amplifica so a figura)
#include "shell/ShellVisual.h"
#include "ui/ColorLegendOverlay.h"

#include <QApplication>
#include <QFont>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkDoubleArray.h>
#include <vtkLookupTable.h>
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
#include <vtkWindowToImageFilter.h>
#include <vtkLine.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

namespace {

std::vector<std::string> split(const std::string &line)
{
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string cell;
    while (std::getline(ss, cell, ',')) {
        out.push_back(cell);
    }
    return out;
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 11) {
        std::fprintf(stderr, "uso: tower_shots geom desloc saida.png titulo subtitulo titulo_legenda unidade modo amplitude fator\n");
        return 2;
    }
    std::map<int, std::array<double, 3>> pos;
    std::vector<std::array<int, 2>> members;
    {
        std::ifstream f(args.at(1).toStdString());
        std::string line;
        while (std::getline(f, line)) {
            const auto c = split(line);
            if (c.size() >= 5 && c[0] == "N") {
                pos[std::stoi(c[1])] = {std::stod(c[2]), std::stod(c[3]), std::stod(c[4])};
            } else if (c.size() >= 4 && c[0] == "M") {
                members.push_back({std::stoi(c[2]), std::stoi(c[3])});
            }
        }
    }
    std::map<int, std::array<double, 3>> disp;
    {
        std::ifstream f(args.at(2).toStdString());
        std::string line;
        while (std::getline(f, line)) {
            const auto c = split(line);
            // Structura: id,x,y,z,ux,uy,uz,...; Mecway: id,ux,uy,uz,...  (o chamador converte o do Mecway para o formato do Structura)
            if (c.size() >= 7) {
                disp[std::stoi(c[0])] = {std::stod(c[4]), std::stod(c[5]), std::stod(c[6])};
            }
        }
    }
    const bool mode = args.at(8).toInt() != 0;
    const double visual = args.at(9).toDouble();
    const double colorFactor = args.at(10).toDouble();
    double peak = 0.0;
    for (const auto &[id, u] : disp) {
        peak = std::max(peak, std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]));
    }
    const double normal = mode && peak > 0 ? 1.0 / peak : 1.0;   // forma normalizada a translacao maxima 1
    const double amplify = peak > 0 ? visual / (peak * normal) : 0.0;
    vtkNew<vtkPoints> points, ghostPoints;
    vtkNew<vtkDoubleArray> scalars;
    scalars->SetName("u");
    std::map<int, vtkIdType> index;
    double vmin = 1e300, vmax = -1e300;
    for (const auto &[id, p] : pos) {
        const auto it = disp.find(id);
        const std::array<double, 3> u = it != disp.end() ? it->second : std::array<double, 3> {0.0, 0.0, 0.0};
        const double mag = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]) * normal * colorFactor;
        vmin = std::min(vmin, mag);
        vmax = std::max(vmax, mag);
        index[id] = points->InsertNextPoint(p[0] + u[0] * normal * amplify, p[1] + u[1] * normal * amplify, p[2] + u[2] * normal * amplify);
        ghostPoints->InsertNextPoint(p[0], p[1], p[2]);
        scalars->InsertNextValue(mag);
    }
    vtkNew<vtkCellArray> lines;
    for (const auto &m : members) {
        vtkNew<vtkLine> line;
        line->GetPointIds()->SetId(0, index[m[0]]);
        line->GetPointIds()->SetId(1, index[m[1]]);
        lines->InsertNextCell(line);
    }
    vtkNew<vtkPolyData> deformed, ghost;
    deformed->SetPoints(points);
    deformed->SetLines(lines);
    deformed->GetPointData()->SetScalars(scalars);
    ghost->SetPoints(ghostPoints);
    ghost->SetLines(lines);
    vtkNew<vtkTubeFilter> tube, ghostTube;
    tube->SetInputData(deformed);
    tube->SetRadius(0.045);
    tube->SetNumberOfSides(8);
    ghostTube->SetInputData(ghost);
    ghostTube->SetRadius(0.012);
    ghostTube->SetNumberOfSides(6);
    // escala em faixas do Structura (10 degraus de cor)
    vtkNew<vtkLookupTable> table;
    const int perBand = 25;
    table->SetNumberOfTableValues(Structura::Shell::kColorBands * perBand);
    for (int i = 0; i < Structura::Shell::kColorBands * perBand; ++i) {
        const double t = (i + 0.5) / (Structura::Shell::kColorBands * perBand);
        const auto c = Structura::Shell::bandColor(t);
        table->SetTableValue(i, c[0] / 255.0, c[1] / 255.0, c[2] / 255.0, 1.0);
    }
    table->SetRange(vmin, vmax > vmin ? vmax : vmin + 1.0);
    table->Build();
    vtkNew<vtkPolyDataMapper> mapper, ghostMapper;
    mapper->SetInputConnection(tube->GetOutputPort());
    mapper->SetLookupTable(table);
    mapper->SetScalarRange(vmin, vmax > vmin ? vmax : vmin + 1.0);
    mapper->SetColorModeToMapScalars();
    mapper->ScalarVisibilityOn();
    ghostMapper->SetInputConnection(ghostTube->GetOutputPort());
    ghostMapper->ScalarVisibilityOff();
    vtkNew<vtkActor> actor, ghostActor;
    actor->SetMapper(mapper);
    ghostActor->SetMapper(ghostMapper);
    ghostActor->GetProperty()->SetColor(0.62, 0.65, 0.70);
    const int W = 1800, H = 1100;
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
    ren->AddActor(ghostActor);
    ren->AddActor(actor);
    vtkCamera *cam = ren->GetActiveCamera();
    cam->SetFocalPoint(0.0, 0.0, 15.0);
    cam->SetPosition(34.0, -40.0, 20.0);
    cam->SetViewUp(0, 0, 1);
    ren->ResetCamera();
    cam->Zoom(0.9);
    window->Render();
    vtkNew<vtkWindowToImageFilter> grab;
    grab->SetInput(window);
    grab->SetInputBufferTypeToRGB();
    grab->ReadFrontBufferOff();
    grab->Update();
    const QString raw = args.at(3) + QStringLiteral(".raw.png");
    vtkNew<vtkPNGWriter> writer;
    writer->SetFileName(raw.toLocal8Bit().constData());
    writer->SetInputConnection(grab->GetOutputPort());
    writer->Write();
    QImage image(raw);
    if (image.isNull()) {
        std::fprintf(stderr, "imagem nao gerada\n");
        return 1;
    }
    image = image.convertToFormat(QImage::Format_ARGB32);
    QWidget host;
    host.resize(image.width(), image.height());
    QFont font = host.font();
    font.setPointSize(12);
    host.setFont(font);
    Structura::UI::ColorLegendOverlay legend(&host);
    legend.setFont(font);
    legend.setLegend(args.at(6), args.at(7), vmin, vmax, false, true);
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
    painter.drawText(QRect(40, 28, image.width() - 80, 50), Qt::AlignLeft | Qt::AlignVCenter, args.at(4));
    QFont sub = font;
    sub.setPointSize(14);
    painter.setFont(sub);
    painter.setPen(QColor(70, 78, 90));
    painter.drawText(QRect(40, 78, image.width() - 80, 90), Qt::AlignLeft | Qt::TextWordWrap, args.at(5));
    painter.end();
    image.save(args.at(3));
    std::printf("%s (min %.4g, max %.4g)\n", args.at(3).toLocal8Bit().constData(), vmin, vmax);
    return 0;
}
