#include "drawing/DrawingSymbols.h"

#include <algorithm>
#include <cmath>

namespace Structura::Drawing {

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string number(double v)
{
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, std::abs(v - std::round(v)) < 1e-6 ? "%.0f" : "%.1f", v);
    std::string s = buffer;
    for (char &c : s) {
        if (c == '.') {
            c = ',';
        }
    }
    return s;
}

} // namespace

void arrowHead(Sheet &sheet, Point from, Point to, int layer, double size)
{
    const double dx = to.x - from.x, dy = to.y - from.y;
    const double len = std::hypot(dx, dy);
    if (len < 1e-9) {
        return;
    }
    const double ux = dx / len, uy = dy / len;
    const double nx = -uy, ny = ux;
    const double w = size * 0.28;
    sheet.polyline({to, {to.x - ux * size + nx * w, to.y - uy * size + ny * w}, {to.x - ux * size - nx * w, to.y - uy * size - ny * w}}, true, layer, true);
}

void drawWeldSymbol(Sheet &sheet, const WeldSymbol &weld, int layer, int textLayer)
{
    // linha de referencia horizontal saindo da seta em direcao a cauda
    const double dir = weld.tail.x >= weld.arrowTip.x ? 1.0 : -1.0;
    const double refLen = 22.0 + (weld.length > 0.0 ? 8.0 : 0.0);
    const Point bend {weld.tail.x, weld.tail.y};
    const Point refEnd {bend.x + dir * refLen, bend.y};
    sheet.line(weld.arrowTip, bend, layer);
    arrowHead(sheet, bend, weld.arrowTip, layer);
    sheet.line(bend, refEnd, layer);

    const double h = 3.2;   // altura do simbolo
    auto drawSymbol = [&](bool below) {
        const double sy = below ? -1.0 : 1.0;
        const double x0 = bend.x + dir * 8.0;   // o simbolo fica a 8 mm da dobra, depois da dimensao
        switch (weld.type) {
        case WeldSymbol::Type::Fillet:
            // triangulo retangulo com o cateto vertical do lado da cauda... convencao: cateto vertical a esquerda do simbolo para quem le da esquerda para a direita
            sheet.polyline({{x0, bend.y}, {x0, bend.y + sy * h}, {x0 + dir * h, bend.y}}, true, layer, false);
            break;
        case WeldSymbol::Type::GrooveFull:
            sheet.polyline({{x0, bend.y + sy * h}, {x0 + dir * h * 0.5, bend.y}, {x0 + dir * h, bend.y + sy * h}}, false, layer);
            break;
        case WeldSymbol::Type::GroovePartial:
            sheet.polyline({{x0, bend.y + sy * h}, {x0, bend.y}, {x0 + dir * h, bend.y}}, false, layer);
            break;
        }
        if (weld.size > 0.0) {
            sheet.text({x0 - dir * 1.2, bend.y + sy * (below ? h * 0.55 + 0.3 : h * 0.15)}, number(weld.size), 2.0, textLayer, dir > 0 ? TextAlign::Right : TextAlign::Left);
        }
        if (weld.length > 0.0) {
            sheet.text({x0 + dir * (h + 1.6), bend.y + sy * (below ? h * 0.55 + 0.3 : h * 0.15)}, "- " + number(weld.length), 2.0, textLayer, dir > 0 ? TextAlign::Left : TextAlign::Right);
        }
    };
    if (weld.bothSides) {
        drawSymbol(true);
        drawSymbol(false);
    } else {
        drawSymbol(weld.arrowSide);
    }
    if (weld.allAround) {
        sheet.circle(bend, 1.6, layer);
    }
    if (weld.field) {
        const Point pole {bend.x + dir * 0.0, bend.y};
        sheet.line(pole, {pole.x, pole.y + 5.5}, layer);
        sheet.polyline({{pole.x, pole.y + 5.5}, {pole.x + dir * 3.2, pole.y + 4.4}, {pole.x, pole.y + 3.3}}, true, layer, true);
    }
    if (!weld.note.empty()) {
        sheet.text({refEnd.x + dir * 1.5, refEnd.y - 0.8}, weld.note, 2.0, textLayer, dir > 0 ? TextAlign::Left : TextAlign::Right);
    }
}

void drawBoltHole(Sheet &sheet, Point center, double holeDiameterPaper, int layer, int centreLayer)
{
    const double r = std::max(0.5, holeDiameterPaper / 2.0);
    sheet.circle(center, r, layer);
    const double ext = r + 1.8;
    sheet.line({center.x - ext, center.y}, {center.x + ext, center.y}, centreLayer);
    sheet.line({center.x, center.y - ext}, {center.x, center.y + ext}, centreLayer);
}

void drawBoltEndSymbol(Sheet &sheet, Point center, double d, int layer, int centreLayer)
{
    const double radius = std::max(0.4, 1.5 * d / std::sqrt(3.0));   // raio circunscrito do hexagono com 1,5 d entre faces
    std::vector<Point> hex;
    for (int i = 0; i < 6; ++i) {
        const double angle = kPi / 6.0 + kPi / 3.0 * i;
        hex.push_back({center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)});
    }
    sheet.polyline(hex, true, layer);
    sheet.circle(center, std::max(0.25, d / 2.0), layer);
    const double ext = radius + 1.6;
    sheet.line({center.x - ext, center.y}, {center.x + ext, center.y}, centreLayer);
    sheet.line({center.x, center.y - ext}, {center.x, center.y + ext}, centreLayer);
}

void drawBoltAxisLine(Sheet &sheet, Point a, Point b, int centreLayer)
{
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double len = std::hypot(dx, dy);
    if (len < 1e-9) {
        return;
    }
    const double ux = dx / len, uy = dy / len;
    sheet.line({a.x - ux * 2.0, a.y - uy * 2.0}, {b.x + ux * 2.0, b.y + uy * 2.0}, centreLayer);
}

void drawMarkCallout(Sheet &sheet, Point target, Point label, const std::string &mark, int layer, int textLayer, double radius)
{
    const double dx = target.x - label.x, dy = target.y - label.y;
    const double len = std::hypot(dx, dy);
    if (len > radius) {
        const double ux = dx / len, uy = dy / len;
        sheet.line({label.x + ux * radius, label.y + uy * radius}, target, layer);
        arrowHead(sheet, label, target, layer, 2.0);
    }
    sheet.circle(label, radius, layer);
    sheet.text({label.x, label.y - 1.0}, mark, 2.4, textLayer, TextAlign::Center);
}

void drawSectionCut(Sheet &sheet, Point a, Point b, const std::string &letter, double side, int layer, int textLayer)
{
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double len = std::hypot(dx, dy);
    if (len < 1e-9) {
        return;
    }
    const double ux = dx / len, uy = dy / len;
    const double nx = -uy * side, ny = ux * side;
    // linha de corte em traco e ponto, com extremidades reforcadas e setas apontando para o lado visto
    const double tick = std::min(6.0, len / 4.0);
    sheet.line(a, b, layer);
    for (const Point &p : {a, b}) {
        const Point end {p.x + nx * 7.0, p.y + ny * 7.0};
        sheet.line(p, end, layer);
        arrowHead(sheet, p, end, layer, 3.0);
        sheet.text({end.x + nx * 3.0, end.y + ny * 3.0 - 1.0}, letter, 3.2, textLayer, TextAlign::Center, 0.0, true);
    }
    (void)tick;
    (void)kPi;
}

} // namespace Structura::Drawing
