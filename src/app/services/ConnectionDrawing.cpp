#include "app/services/SheetBuilderInternal.h"

#include "app/services/ConnectionCatalog.h"
#include "app/services/ConnectionService.h"
#include "drawing/HiddenLine.h"
#include "design/BasePlate.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Structura::App::Services::internal {

using namespace Structura::Drawing;
using Structura::App::ProjectData::NodeConnection;

namespace {

struct L {
    int outline {0}, hidden {0}, centre {0}, dim {0}, text {0}, bolt {0}, weld {0}, hatch {0}, plate {0}, frame {0};
};

L layers(Sheet &s)
{
    L l;
    l.frame = s.addLayer("MOLDURA", {0, 0, 0}, 0.5);
    l.outline = s.addLayer("PERFIS", {0, 0, 0}, 0.25);
    l.plate = s.addLayer("CHAPAS", {20, 60, 140}, 0.3);
    l.hidden = s.addLayer("OCULTO", {120, 120, 120}, 0.13, LineStyle::Dashed);
    l.centre = s.addLayer("EIXOS", {200, 40, 40}, 0.13, LineStyle::DashDot);
    l.dim = s.addLayer("COTAS", {0, 120, 0}, 0.13);
    l.text = s.addLayer("TEXTO", {0, 0, 0}, 0.18);
    l.bolt = s.addLayer("PARAFUSOS", {150, 60, 0}, 0.25);
    l.weld = s.addLayer("SOLDA", {200, 0, 0}, 0.3);
    l.hatch = s.addLayer("HACHURA", {150, 150, 150}, 0.09);
    return l;
}

std::string fmtNum(double v, int decimals = 0)
{
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*f", decimals, v);
    std::string s = buffer;
    for (char &c : s) {
        if (c == '.') {
            c = ',';
        }
    }
    return s;
}

double val(const NodeConnection &c, const char *key, double fallback = 0.0)
{
    return c.values.value(QString::fromLatin1(key), fallback);
}

/// Cota alinhada entre p1 e p2, deslocada `offset` mm (lado esquerdo do sentido p1->p2 se positivo).
void dimLinear(Sheet &s, const L &l, Point p1, Point p2, double offset, const std::string &text, double textHeight = 2.2)
{
    const double dx = p2.x - p1.x, dy = p2.y - p1.y;
    const double len = std::hypot(dx, dy);
    if (len < 1e-6) {
        return;
    }
    const double ux = dx / len, uy = dy / len;
    const double nx = -uy, ny = ux;
    const Point a {p1.x + nx * offset, p1.y + ny * offset};
    const Point b {p2.x + nx * offset, p2.y + ny * offset};
    const double sign = offset >= 0 ? 1.0 : -1.0;
    s.line({p1.x + nx * sign * 1.0, p1.y + ny * sign * 1.0}, {a.x + nx * sign * 1.2, a.y + ny * sign * 1.2}, l.dim);
    s.line({p2.x + nx * sign * 1.0, p2.y + ny * sign * 1.0}, {b.x + nx * sign * 1.2, b.y + ny * sign * 1.2}, l.dim);
    s.line(a, b, l.dim);
    for (const Point &p : {a, b}) {
        s.line({p.x - ux * 1.0 - nx * 1.0, p.y - uy * 1.0 - ny * 1.0}, {p.x + ux * 1.0 + nx * 1.0, p.y + uy * 1.0 + ny * 1.0}, l.dim);
    }
    double angle = std::atan2(dy, dx) * 180.0 / 3.14159265358979;
    if (angle > 90.0) angle -= 180.0;
    if (angle < -90.0) angle += 180.0;
    const double mx = (a.x + b.x) / 2.0, my = (a.y + b.y) / 2.0;
    // texto do lado de fora da linha de cota
    s.text({mx + nx * sign * 0.8, my + ny * sign * 0.8 - (std::abs(uy) < 0.5 ? 0.0 : 0.0)}, text, textHeight, l.dim, TextAlign::Center, angle);
}

void centreCross(Sheet &s, const L &l, Point c, double half)
{
    s.line({c.x - half, c.y}, {c.x + half, c.y}, l.centre);
    s.line({c.x, c.y - half}, {c.x, c.y + half}, l.centre);
}

void hatchRect(Sheet &s, const L &l, double x0, double y0, double x1, double y1, double step)
{
    // linhas a 45 graus recortadas no retangulo
    for (double c = (x0 - y1); c <= (x1 - y0); c += step) {
        // reta y = x - c
        double ax = std::max(x0, c + y0), bx = std::min(x1, c + y1);
        if (bx > ax) {
            s.line({ax, ax - c}, {bx, bx - c}, l.hatch);
        }
    }
}

struct Frame {
    double x0, y0, x1, y1;
};

struct Pick {
    double scale;
};

double pickScale(double extentX, double extentY, double w, double h, double fixed)
{
    if (fixed > 0.0) {
        return fixed;
    }
    static const double scales[] = {1, 2, 2.5, 4, 5, 8, 10, 15, 20, 25, 30, 40, 50, 75, 100};
    for (const double s : scales) {
        if (extentX / s <= w && extentY / s <= h) {
            return s;
        }
    }
    return 200.0;
}

void notesBlock(Sheet &s, const L &l, double x, double y, const std::vector<std::string> &lines)
{
    s.text({x, y}, "NOTAS", 3.0, l.text, TextAlign::Left, 0.0, true);
    double yy = y - 5.0;
    for (const auto &line : lines) {
        s.text({x, yy}, line, 2.3, l.text);
        yy -= 4.2;
    }
}

// =====================================================================================================================
// Novas tipologias: chapa de no, emenda de viga, base de pilar e ligacao tubular
// =====================================================================================================================

double pickScaleMargin(double extentX, double extentY, double w, double h, double margin, double fixed)
{
    if (fixed > 0.0) {
        return fixed;
    }
    static const double scales[] = {1, 2, 2.5, 4, 5, 8, 10, 15, 20, 25, 30, 40, 50, 75, 100, 150, 200, 250, 500};
    for (const double s : scales) {
        if (extentX / s + 2.0 * margin <= w && extentY / s + 2.0 * margin <= h) {
            return s;
        }
    }
    return 1000.0;
}

void weldTriangle(Sheet &s, const L &l, Point corner, double dx, double dy, double size)
{
    s.polyline({corner, {corner.x + dx * size, corner.y}, {corner.x, corner.y + dy * size}}, true, l.weld, true);
}

/// Parafuso visto de lado (haste, cabeca e porca) entre y0 e y1 na posicao x (papel).
void boltSide(Sheet &s, const L &l, double x, double y0, double y1, double shank, double head, double headT)
{
    s.rect({x - shank / 2.0, std::min(y0, y1)}, {x + shank / 2.0, std::max(y0, y1)}, l.bolt);
    s.rect({x - head / 2.0, std::max(y0, y1)}, {x + head / 2.0, std::max(y0, y1) + headT}, l.bolt);
    s.rect({x - head / 2.0, std::min(y0, y1) - headT}, {x + head / 2.0, std::min(y0, y1)}, l.bolt);
}

void boltSideH(Sheet &s, const L &l, double y, double x0, double x1, double shank, double head, double headT)
{
    s.rect({std::min(x0, x1), y - shank / 2.0}, {std::max(x0, x1), y + shank / 2.0}, l.bolt);
    s.rect({std::max(x0, x1), y - head / 2.0}, {std::max(x0, x1) + headT, y + head / 2.0}, l.bolt);
    s.rect({std::min(x0, x1) - headT, y - head / 2.0}, {std::min(x0, x1), y + head / 2.0}, l.bolt);
}

struct Panels {
    double lx0, lx1, rx0, rx1, y0, y1;
};

Panels panelsFor(double rx0, double ry0, double rx1, double ry1)
{
    Panels p;
    const double w = rx1 - rx0;
    p.lx0 = rx0;
    p.lx1 = rx0 + 0.58 * w;
    p.rx0 = rx0 + 0.60 * w;
    p.rx1 = rx1;
    p.y0 = ry0 + 44.0;
    p.y1 = ry1 - 10.0;
    return p;
}

std::vector<std::string> connectionNotes(const SheetBuilder::ConnectionDetail &d, const std::string &what)
{
    const NodeConnection &c = d.connection;
    std::vector<std::string> lines;
    lines.push_back(std::string("LIGAÇÃO: ") + ConnectionService::typeName(c.type).toStdString() + " - NÓ " + std::to_string(c.nodeId) + ", BARRA " + std::to_string(c.memberId));
    if (!d.beamSection.isEmpty()) {
        lines.push_back("PERFIL: " + d.beamSection.toStdString());
    }
    if (!what.empty()) {
        lines.push_back(what);
    }
    return lines;
}

void notesFinish(Sheet &s, const L &l, double rx0, double ry0, std::vector<std::string> lines, const SheetBuilder::ConnectionDetail &d, const char *norm)
{
    if (d.hasForces) {
        lines.push_back("ESFORÇOS DE CÁLCULO: V = " + fmtNum(d.shearKN, 1) + " kN; N = " + fmtNum(d.axialKN, 1) + " kN; APROVEIT. η = " + fmtNum(d.eta, 2));
    } else {
        lines.push_back("APROVEITAMENTO η = " + fmtNum(d.eta, 2));
    }
    lines.push_back(std::string(norm) + " COTAS EM mm.");
    notesBlock(s, l, rx0 + 6.0, ry0 + 33.0, lines);
}

// ---- chapa de no ---------------------------------------------------------------------------------------------------

double drawGussetDetail(Sheet &s, const SheetBuilder::ConnectionDetail &d, double rx0, double ry0, double rx1, double ry1, bool notes, double fixedScale)
{
    const L l = layers(s);
    const NodeConnection &c = d.connection;
    const double db = val(c, "db", 20.0);
    const double dh = Structura::Design::Connections::standardHoleDiameter(db);
    const int rows = std::max(1, static_cast<int>(val(c, "rows", 3.0) + 0.5));
    const int lines = std::max(1, std::min(2, static_cast<int>(val(c, "lines", 1.0) + 0.5)));
    const double pitch = val(c, "pitch", 70.0), gauge = val(c, "gauge", 60.0);
    const double endLeg = val(c, "endLeg", 35.0), edgeLeg = val(c, "edgeLeg", 30.0), endG = val(c, "endG", 35.0);
    const double gT = val(c, "gT", 9.5), W = std::max(val(c, "gWidth", 200.0), 30.0), freeL = val(c, "freeL", 80.0);
    const double legT = val(c, "legT", 6.35), weldLeg = val(c, "weldLeg", 6.0);
    const int bars = std::max(1, static_cast<int>(val(c, "bars", 2.0) + 0.5));
    const double Lg = freeL + (rows - 1) * pitch + endG;
    const double barW = (lines - 1) * gauge + 2.0 * edgeLeg;
    const double supportT = 30.0;
    const Panels p = panelsFor(rx0, ry0, rx1, ry1);

    const double extX = W + 40.0, extY = Lg + 60.0 + supportT;
    const double scale = pickScaleMargin(extX, extY, p.lx1 - p.lx0, p.y1 - p.y0 - 8.0, 20.0, fixedScale);
    const double k = 1.0 / scale;
    const double cx = (p.lx0 + p.lx1) / 2.0, base = p.y0 + 20.0 + supportT * k;
    auto P = [&](double x, double y) { return Point {cx + x * k, base + y * k}; };
    s.text({p.lx0 + 6.0, p.y1}, "VISTA NO PLANO DA CHAPA", 3.2, l.text, TextAlign::Left, 0.0, true);

    // apoio (banzo ou pilar) abaixo da chapa
    s.rect(P(-W / 2.0 - 20.0, -supportT), P(W / 2.0 + 20.0, 0.0), l.outline);
    hatchRect(s, l, cx - (W / 2.0 + 20.0) * k, base - supportT * k, cx + (W / 2.0 + 20.0) * k, base, 2.0);
    s.text(P(W / 2.0 + 24.0, -supportT / 2.0), "APOIO", 2.2, l.text);
    // chapa
    s.rect(P(-W / 2.0, 0.0), P(W / 2.0, Lg), l.plate);
    // diagonal (contorno); a extremidade fica a endLeg do primeiro furo (lado do apoio)
    const double yEnd = std::max(2.0, freeL - endLeg);
    const double yTop = Lg + 45.0;
    s.rect(P(-barW / 2.0, yEnd), P(barW / 2.0, yTop), l.outline);
    s.line(P(-barW / 2.0 - 4.0, yTop - 6.0), P(barW / 2.0 + 4.0, yTop + 6.0), l.outline);
    s.text(P(barW / 2.0 + 6.0, yTop - 8.0), "DIAGONAL", 2.2, l.text);
    // linhas de Whitmore (30 graus)
    const double yFar = freeL + (rows - 1) * pitch;
    const double half = (lines - 1) * gauge / 2.0;
    const double spread = (rows - 1) * pitch * std::tan(30.0 * 3.14159265358979 / 180.0);
    for (const double sign : {-1.0, 1.0}) {
        s.line(P(sign * half, yFar), P(sign * (half + spread), freeL), l.hidden);
    }
    s.line(P(-(half + spread), freeL), P(half + spread, freeL), l.hidden);
    s.text(P(half + spread + 4.0, freeL - 2.0), "WHITMORE lw = " + fmtNum(2.0 * (half + spread), 0), 2.0, l.text);
    // parafusos
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < lines; ++j) {
            const double x = (j - (lines - 1) / 2.0) * gauge, y = freeL + i * pitch;
            s.circle(P(x, y), std::max(0.6, dh / 2.0 * k), l.bolt);
            centreCross(s, l, P(x, y), std::max(1.5, dh * k * 0.9));
        }
    }
    // soldas na base da chapa
    const double t = std::max(1.6, weldLeg * k);
    weldTriangle(s, l, P(-W / 2.0, 0.0), 1.0, -1.0, t);
    weldTriangle(s, l, P(W / 2.0, 0.0), -1.0, -1.0, t);
    s.text(P(-W / 2.0 - 6.0, -weldLeg - 4.0), "FILETE " + fmtNum(weldLeg, 0) + " mm", 2.0, l.text, TextAlign::Right);
    // cotas
    const double off = 9.0;
    for (int i = 0; i + 1 < rows; ++i) {
        dimLinear(s, l, P(half + 0.0, freeL + i * pitch), P(half + 0.0, freeL + (i + 1) * pitch), -(off + (half + spread) * k * 0.0), fmtNum(pitch, 0));
    }
    if (lines == 2) {
        dimLinear(s, l, P(-half, yFar + endG * 0.0 + 2.0), P(half, yFar + 2.0), 0.0, fmtNum(gauge, 0));
    }
    dimLinear(s, l, P(-W / 2.0, Lg), P(W / 2.0, Lg), -(off), fmtNum(W, 0));
    dimLinear(s, l, P(W / 2.0, 0.0), P(W / 2.0, freeL), -(off + 6.0), fmtNum(freeL, 0));
    dimLinear(s, l, P(W / 2.0, yFar), P(W / 2.0, Lg), -(off + 6.0), fmtNum(endG, 0));
    dimLinear(s, l, P(W / 2.0, 0.0), P(W / 2.0, Lg), -(off + 16.0), fmtNum(Lg, 0));
    dimLinear(s, l, P(-barW / 2.0, freeL), P(-barW / 2.0, yEnd), off + 4.0, fmtNum(endLeg, 0));

    // corte transversal pelo eixo de uma linha de parafusos
    const double sx = (p.rx0 + p.rx1) / 2.0;
    const double secScale = scale;
    const double ks = 1.0 / secScale;
    auto Q = [&](double x, double y) { return Point {sx + x * ks, base + y * ks}; };
    s.text({p.rx0 + 4.0, p.y1}, "CORTE (PELA LINHA DE PARAFUSOS)", 3.2, l.text, TextAlign::Left, 0.0, true);
    s.rect(Q(-45.0, -supportT), Q(45.0, 0.0), l.outline);
    hatchRect(s, l, sx - 45.0 * ks, base - supportT * ks, sx + 45.0 * ks, base, 2.0);
    s.rect(Q(-gT / 2.0, 0.0), Q(gT / 2.0, Lg), l.plate);
    // pecas da diagonal junto a chapa
    for (int b = 0; b < bars; ++b) {
        const double sign = bars == 2 ? (b == 0 ? 1.0 : -1.0) : 1.0;
        const double xi = sign * (gT / 2.0), xo = sign * (gT / 2.0 + legT);
        s.rect(Q(std::min(xi, xo), yEnd), Q(std::max(xi, xo), yTop), l.outline);
    }
    // parafusos (haste horizontal, cabeca e porca)
    for (int i = 0; i < rows; ++i) {
        const double y = freeL + i * pitch;
        const double reach = gT / 2.0 + legT * (bars == 2 ? 1.0 : 1.0) + 6.0;
        s.rect(Q(-reach, y - db / 2.0), Q(reach, y + db / 2.0), l.bolt);
        s.rect(Q(reach, y - 0.8 * db), Q(reach + 0.7 * db, y + 0.8 * db), l.bolt);
        s.rect(Q(-reach - 0.7 * db, y - 0.8 * db), Q(-reach, y + 0.8 * db), l.bolt);
    }
    weldTriangle(s, l, Q(-gT / 2.0, 0.0), -1.0, 1.0, std::max(1.6, weldLeg * ks));
    weldTriangle(s, l, Q(gT / 2.0, 0.0), 1.0, 1.0, std::max(1.6, weldLeg * ks));
    s.text(Q(gT / 2.0 + legT + 10.0, Lg * 0.5), "CH. " + fmtNum(gT, 1) + " mm", 2.2, l.text);
    s.text(Q(gT / 2.0 + legT + 10.0, yTop - 10.0), std::to_string(bars) + " x t = " + fmtNum(legT, 2) + " mm", 2.2, l.text);

    if (notes) {
        auto text = connectionNotes(d, "");
        text.push_back("PARAFUSOS: " + std::to_string(rows * lines) + " x d" + fmtNum(db, 0) + " " + c.grade.toStdString() + " (furo " + fmtNum(dh, 1) + " mm)");
        text.push_back("CHAPA " + fmtNum(gT, 1) + " x " + fmtNum(W, 0) + " x " + fmtNum(Lg, 0) + " mm; SOLDA DE FILETE " + fmtNum(weldLeg, 0) + " mm EM AMBOS OS LADOS");
        notesFinish(s, l, rx0, ry0, text, d, "NBR 8800:2024 - 5.2, 5.3, 6.2, 6.3 E 6.5 (SEÇÃO DE WHITMORE).");
    }
    return scale;
}

// ---- emenda de viga ------------------------------------------------------------------------------------------------

double drawSpliceDetail(Sheet &s, const SheetBuilder::ConnectionDetail &d, double rx0, double ry0, double rx1, double ry1, bool notes, double fixedScale)
{
    const L l = layers(s);
    const NodeConnection &c = d.connection;
    const double db = val(c, "db", 20.0), dh = Structura::Design::Connections::standardHoleDiameter(db);
    const double depth = val(c, "beamD", 300.0), bf = val(c, "bfl", 150.0), tf = val(c, "tfl", 10.0), tw = val(c, "tw", 6.0), gap = val(c, "gap", 10.0);
    const int fRows = std::max(1, static_cast<int>(val(c, "fRows", 3.0) + 0.5)), fLines = std::max(1, static_cast<int>(val(c, "fLines", 2.0) + 0.5));
    const double fPitch = val(c, "fPitch", 70.0), fGauge = val(c, "fGauge", 80.0), fEnd = val(c, "fEnd", 40.0);
    const double opW = val(c, "opW", 150.0), opT = val(c, "opT", 12.5), ipT = val(c, "ipT", 0.0), ipW = val(c, "ipW", 55.0);
    const int wRows = std::max(1, static_cast<int>(val(c, "wRows", 3.0) + 0.5)), wCols = std::max(1, static_cast<int>(val(c, "wCols", 1.0) + 0.5));
    const double wPitch = val(c, "wPitch", 70.0), wGauge = val(c, "wGauge", 60.0), wEdgeH = val(c, "wEdgeH", 40.0), wEdgeV = val(c, "wEdgeV", 35.0), wpT = val(c, "wpT", 6.3);
    const double fl = 2.0 * ((fRows - 1) * fPitch + fEnd) + gap;
    const double wl = 2.0 * ((wCols - 1) * wGauge + wEdgeH) + gap;
    const double wh = (wRows - 1) * wPitch + 2.0 * wEdgeV;
    const double halfSpan = fl / 2.0 + 60.0;
    const Panels p = panelsFor(rx0, ry0, rx1, ry1);

    const double extX = 2.0 * halfSpan, extY = depth + 2.0 * (opT + 20.0);
    const double scale = pickScaleMargin(extX, extY, p.lx1 - p.lx0, p.y1 - p.y0 - 8.0, 18.0, fixedScale);
    const double k = 1.0 / scale;
    const double cx = (p.lx0 + p.lx1) / 2.0, cy = (p.y0 + p.y1) / 2.0 - 2.0;
    auto P = [&](double x, double y) { return Point {cx + x * k, cy + y * k}; };
    s.text({p.lx0 + 6.0, p.y1}, "ELEVAÇÃO (VISTA LATERAL)", 3.2, l.text, TextAlign::Left, 0.0, true);

    // vigas: mesas, alma e folga
    for (const double side : {-1.0, 1.0}) {
        const double x0 = side * gap / 2.0, x1 = side * halfSpan;
        s.rect(P(std::min(x0, x1), depth / 2.0 - tf), P(std::max(x0, x1), depth / 2.0), l.outline);
        s.rect(P(std::min(x0, x1), -depth / 2.0), P(std::max(x0, x1), -depth / 2.0 + tf), l.outline);
        s.line(P(x1, -depth / 2.0 + tf), P(x1, depth / 2.0 - tf), l.hidden);
    }
    s.line(P(-halfSpan, 0.0), P(halfSpan, 0.0), l.centre);
    s.line(P(0.0, -depth / 2.0 - opT - 12.0), P(0.0, depth / 2.0 + opT + 12.0), l.centre);
    // talas de mesa externas e internas
    s.rect(P(-fl / 2.0, depth / 2.0), P(fl / 2.0, depth / 2.0 + opT), l.plate);
    s.rect(P(-fl / 2.0, -depth / 2.0 - opT), P(fl / 2.0, -depth / 2.0), l.plate);
    if (ipT > 0.0) {
        s.rect(P(-fl / 2.0, depth / 2.0 - tf - ipT), P(fl / 2.0, depth / 2.0 - tf), l.plate);
        s.rect(P(-fl / 2.0, -depth / 2.0 + tf), P(fl / 2.0, -depth / 2.0 + tf + ipT), l.plate);
    }
    // tala de alma (a frente)
    s.rect(P(-wl / 2.0, -wh / 2.0), P(wl / 2.0, wh / 2.0), l.plate);
    // parafusos de mesa (vistos de lado)
    const double headT = std::max(0.8, 0.6 * db * k);
    for (const double side : {-1.0, 1.0}) {
        for (int i = 0; i < fRows; ++i) {
            const double x = side * (gap / 2.0 + fEnd + i * fPitch);
            const Point top = P(x, depth / 2.0 + opT), bottom = P(x, depth / 2.0 - tf - ipT);
            boltSide(s, l, top.x, bottom.y, top.y, db * k, 1.6 * db * k, headT);
            const Point t2 = P(x, -depth / 2.0 + tf + ipT), b2 = P(x, -depth / 2.0 - opT);
            boltSide(s, l, t2.x, b2.y, t2.y, db * k, 1.6 * db * k, headT);
        }
    }
    // parafusos de alma
    for (const double side : {-1.0, 1.0}) {
        for (int j = 0; j < wCols; ++j) {
            const double x = side * (gap / 2.0 + wEdgeH + j * wGauge);
            for (int i = 0; i < wRows; ++i) {
                const double y = (i - (wRows - 1) / 2.0) * wPitch;
                s.circle(P(x, y), std::max(0.6, dh / 2.0 * k), l.bolt);
                centreCross(s, l, P(x, y), std::max(1.4, dh * k * 0.9));
            }
        }
    }
    // cotas
    const double off = 8.0;
    const double yTopDim = depth / 2.0 + opT;
    dimLinear(s, l, P(-gap / 2.0, yTopDim), P(gap / 2.0, yTopDim), off, fmtNum(gap, 0));
    for (int i = 0; i + 1 < fRows; ++i) {
        dimLinear(s, l, P(gap / 2.0 + fEnd + i * fPitch, yTopDim), P(gap / 2.0 + fEnd + (i + 1) * fPitch, yTopDim), off, fmtNum(fPitch, 0));
    }
    dimLinear(s, l, P(gap / 2.0, yTopDim), P(gap / 2.0 + fEnd, yTopDim), off, fmtNum(fEnd, 0));
    dimLinear(s, l, P(-fl / 2.0, yTopDim), P(fl / 2.0, yTopDim), off + 9.0, fmtNum(fl, 0));
    for (int i = 0; i + 1 < wRows; ++i) {
        const double x = -(gap / 2.0 + wEdgeH);
        dimLinear(s, l, P(x, (i - (wRows - 1) / 2.0) * wPitch), P(x, (i + 1 - (wRows - 1) / 2.0) * wPitch), off, fmtNum(wPitch, 0));
    }
    dimLinear(s, l, P(-wl / 2.0, -wh / 2.0), P(-wl / 2.0, wh / 2.0), off + 8.0, fmtNum(wh, 0));
    dimLinear(s, l, P(-wl / 2.0, -wh / 2.0), P(wl / 2.0, -wh / 2.0), -off, fmtNum(wl, 0));

    // corte transversal na emenda
    const double sx = (p.rx0 + p.rx1) / 2.0;
    const double ks = 1.0 / std::max(scale, std::max(1.0, std::ceil(std::max(bf + 2.0 * (wpT + 20.0), depth) / std::min(p.rx1 - p.rx0 - 36.0, p.y1 - p.y0 - 30.0))));
    auto Q = [&](double x, double y) { return Point {sx + x * ks, cy + y * ks}; };
    s.text({p.rx0 + 4.0, p.y1}, "CORTE NA EMENDA", 3.2, l.text, TextAlign::Left, 0.0, true);
    s.rect(Q(-bf / 2.0, depth / 2.0 - tf), Q(bf / 2.0, depth / 2.0), l.outline);
    s.rect(Q(-bf / 2.0, -depth / 2.0), Q(bf / 2.0, -depth / 2.0 + tf), l.outline);
    s.rect(Q(-tw / 2.0, -depth / 2.0 + tf), Q(tw / 2.0, depth / 2.0 - tf), l.outline);
    s.rect(Q(-opW / 2.0, depth / 2.0), Q(opW / 2.0, depth / 2.0 + opT), l.plate);
    s.rect(Q(-opW / 2.0, -depth / 2.0 - opT), Q(opW / 2.0, -depth / 2.0), l.plate);
    if (ipT > 0.0) {
        for (const double sign : {-1.0, 1.0}) {
            const double xa = sign * (tw / 2.0 + 6.0), xb = sign * (tw / 2.0 + 6.0 + ipW);
            s.rect(Q(std::min(xa, xb), depth / 2.0 - tf - ipT), Q(std::max(xa, xb), depth / 2.0 - tf), l.plate);
            s.rect(Q(std::min(xa, xb), -depth / 2.0 + tf), Q(std::max(xa, xb), -depth / 2.0 + tf + ipT), l.plate);
        }
    }
    for (const double sign : {-1.0, 1.0}) {
        const double xa = sign * tw / 2.0, xb = sign * (tw / 2.0 + wpT);
        s.rect(Q(std::min(xa, xb), -wh / 2.0), Q(std::max(xa, xb), wh / 2.0), l.plate);
    }
    for (int j = 0; j < fLines; ++j) {
        const double x = (j - (fLines - 1) / 2.0) * fGauge;
        s.circle(Q(x, depth / 2.0 + opT / 2.0), std::max(0.6, dh / 2.0 * ks), l.bolt);
        s.circle(Q(x, -depth / 2.0 - opT / 2.0), std::max(0.6, dh / 2.0 * ks), l.bolt);
    }
    for (int i = 0; i < wRows; ++i) {
        const double y = (i - (wRows - 1) / 2.0) * wPitch, reach = tw / 2.0 + wpT + 5.0;
        boltSideH(s, l, Q(0, y).y, Q(-reach, 0).x, Q(reach, 0).x, db * ks, 1.6 * db * ks, std::max(0.8, 0.6 * db * ks));
    }
    dimLinear(s, l, Q(-fGauge * (fLines - 1) / 2.0, depth / 2.0 + opT), Q(fGauge * (fLines - 1) / 2.0, depth / 2.0 + opT), 7.0, fmtNum(fGauge * (fLines - 1), 0));
    dimLinear(s, l, Q(-opW / 2.0, depth / 2.0 + opT + 2.0), Q(opW / 2.0, depth / 2.0 + opT + 2.0), 14.0, fmtNum(opW, 0));
    s.text(Q(opW / 2.0 + 4.0, depth / 2.0 + opT / 2.0), "TALA " + fmtNum(opT, 1), 2.0, l.text);
    s.text(Q(tw / 2.0 + wpT + 6.0, 0.0), "2 x TALA ALMA " + fmtNum(wpT, 1), 2.0, l.text);

    if (notes) {
        auto text = connectionNotes(d, "");
        text.push_back("PARAFUSOS DAS MESAS: " + std::to_string(4 * fRows * fLines) + " x d" + fmtNum(db, 0) + " " + c.grade.toStdString() + "; DA ALMA: " + std::to_string(2 * wRows * wCols) + " x d" + fmtNum(db, 0));
        text.push_back("TALAS DE MESA " + fmtNum(opT, 1) + " x " + fmtNum(opW, 0) + " x " + fmtNum(fl, 0) + (ipT > 0.0 ? " + INTERNAS " + fmtNum(ipT, 1) : "") + "; TALAS DE ALMA 2 x " + fmtNum(wpT, 1) + " x " + fmtNum(wl, 0) + " x " + fmtNum(wh, 0) + " mm");
        notesFinish(s, l, rx0, ry0, text, d, "NBR 8800:2024 - 5.2, 5.4, 6.3 E 6.5.");
    }
    return scale;
}

// ---- base de pilar ---------------------------------------------------------------------------------------------------

double drawBasePlateDetail(Sheet &s, const SheetBuilder::ConnectionDetail &d, double rx0, double ry0, double rx1, double ry1, bool notes, double fixedScale)
{
    const L l = layers(s);
    const NodeConnection &c = d.connection;
    Structura::Design::Connections::BasePlateInput in;
    in.d = val(c, "colD", 300.0);
    in.bf = val(c, "colBf", 150.0);
    in.tw = val(c, "colTw", 6.0);
    in.tf = val(c, "colTf", 10.0);
    in.anchorDiameter = val(c, "anchorD", 25.0);
    in.anchors = std::max(4, std::min(8, static_cast<int>(val(c, "anchors", 4.0) + 0.5)));
    in.plateThickness = val(c, "plateT", 25.0);
    in.fck = val(c, "fck", 25.0);
    in.blockN = val(c, "blockN", 0.0);
    in.blockB = val(c, "blockB", 0.0);
    const auto g = Structura::Design::Connections::basePlateGeometry(in);
    const auto &t = Structura::Design::Connections::anchorFor(in.anchorDiameter);
    const double weldLeg = val(c, "weldLeg", 6.0);
    const double grout = 30.0;
    const Panels p = panelsFor(rx0, ry0, rx1, ry1);

    // planta
    const double extX = g.blockN, extY = g.blockB;
    const double scale = pickScaleMargin(extX, extY, p.lx1 - p.lx0, p.y1 - p.y0 - 8.0, 20.0, fixedScale);
    const double k = 1.0 / scale;
    const double cx = (p.lx0 + p.lx1) / 2.0, cy = (p.y0 + p.y1) / 2.0 - 2.0;
    auto P = [&](double x, double y) { return Point {cx + x * k, cy + y * k}; };
    s.text({p.lx0 + 6.0, p.y1}, "PLANTA", 3.2, l.text, TextAlign::Left, 0.0, true);
    s.rect(P(-g.blockN / 2.0, -g.blockB / 2.0), P(g.blockN / 2.0, g.blockB / 2.0), l.hidden);
    s.rect(P(-g.lx / 2.0, -g.ly / 2.0), P(g.lx / 2.0, g.ly / 2.0), l.plate);
    // perfil I: mesas ao longo de y nas extremidades de x; alma ao longo de x
    s.rect(P(-in.d / 2.0, -in.bf / 2.0), P(-in.d / 2.0 + in.tf, in.bf / 2.0), l.outline);
    s.rect(P(in.d / 2.0 - in.tf, -in.bf / 2.0), P(in.d / 2.0, in.bf / 2.0), l.outline);
    s.rect(P(-in.d / 2.0 + in.tf, -in.tw / 2.0), P(in.d / 2.0 - in.tf, in.tw / 2.0), l.outline);
    s.line(P(-g.blockN / 2.0 - 15.0, 0.0), P(g.blockN / 2.0 + 15.0, 0.0), l.centre);
    s.line(P(0.0, -g.blockB / 2.0 - 15.0), P(0.0, g.blockB / 2.0 + 15.0), l.centre);
    // chumbadores: duas linhas em x = +-a, nb/2 por linha
    const int perSide = in.anchors / 2;
    for (const double sx : {-1.0, 1.0}) {
        for (int j = 0; j < perSide; ++j) {
            const double y = (j - (perSide - 1) / 2.0) * t.a2;
            const Point ctr = P(sx * g.a, y);
            s.circle(ctr, std::max(0.8, t.holeDiameter / 2.0 * k), l.bolt);
            s.circle(ctr, std::max(0.5, in.anchorDiameter / 2.0 * k), l.bolt);
            s.rect({ctr.x - t.washerSize / 2.0 * k, ctr.y - t.washerSize / 2.0 * k}, {ctr.x + t.washerSize / 2.0 * k, ctr.y + t.washerSize / 2.0 * k}, l.bolt);
            centreCross(s, l, ctr, std::max(1.5, in.anchorDiameter * k));
        }
    }
    const double off = 9.0;
    dimLinear(s, l, P(-g.lx / 2.0, g.ly / 2.0), P(g.lx / 2.0, g.ly / 2.0), off, fmtNum(g.lx, 0));
    dimLinear(s, l, P(g.lx / 2.0, -g.ly / 2.0), P(g.lx / 2.0, g.ly / 2.0), off, fmtNum(g.ly, 0));
    dimLinear(s, l, P(-g.lx / 2.0, -g.ly / 2.0), P(-g.a, -g.ly / 2.0), -off, fmtNum(t.a1, 0));
    dimLinear(s, l, P(-g.blockN / 2.0, -g.blockB / 2.0), P(g.blockN / 2.0, -g.blockB / 2.0), -off - 8.0, fmtNum(g.blockN, 0));
    dimLinear(s, l, P(-g.blockN / 2.0, -g.blockB / 2.0), P(-g.blockN / 2.0, g.blockB / 2.0), -off - 4.0, fmtNum(g.blockB, 0));
    if (perSide > 1) {
        dimLinear(s, l, P(g.a, -(perSide - 1) * t.a2 / 2.0), P(g.a, -(perSide - 1) * t.a2 / 2.0 + t.a2), -off, fmtNum(t.a2, 0));
    }
    s.text(P(-g.blockN / 2.0, g.blockB / 2.0 + 6.0 * scale), "BLOCO DE CONCRETO", 2.2, l.text);

    // elevacao (corte pelo plano da alma)
    const double sx0 = (p.rx0 + p.rx1) / 2.0;
    const double colH = 160.0;
    const double depthShown = std::min(t.h1, 320.0);
    const double extY2 = colH + in.plateThickness + grout + depthShown;
    const double ks = 1.0 / pickScaleMargin(g.lx + 40.0, extY2, p.rx1 - p.rx0, p.y1 - p.y0 - 8.0, 14.0, fixedScale);
    auto Q = [&](double x, double y) { return Point {sx0 + x * ks, cy + (y - (colH + in.plateThickness + grout - depthShown) / 2.0) * ks}; };
    s.text({p.rx0 + 4.0, p.y1}, "CORTE (PELA ALMA)", 3.2, l.text, TextAlign::Left, 0.0, true);
    const double yTopPlate = in.plateThickness + grout;   // face superior da placa (topo do bloco em y = 0)
    s.rect(Q(-g.lx / 2.0 - 40.0, -depthShown), Q(g.lx / 2.0 + 40.0, 0.0), l.outline);
    hatchRect(s, l, sx0 - (g.lx / 2.0 + 40.0) * ks, cy + (-depthShown - (colH + yTopPlate - depthShown) / 2.0) * ks, sx0 + (g.lx / 2.0 + 40.0) * ks,
              cy + (0.0 - (colH + yTopPlate - depthShown) / 2.0) * ks, 2.4);
    s.rect(Q(-g.lx / 2.0, 0.0), Q(g.lx / 2.0, grout), l.hatch);
    s.rect(Q(-g.lx / 2.0, grout), Q(g.lx / 2.0, yTopPlate), l.plate);
    s.rect(Q(-in.d / 2.0, yTopPlate), Q(-in.d / 2.0 + in.tf, yTopPlate + colH), l.outline);
    s.rect(Q(in.d / 2.0 - in.tf, yTopPlate), Q(in.d / 2.0, yTopPlate + colH), l.outline);
    s.line(Q(-in.d / 2.0 + in.tf, yTopPlate + colH), Q(in.d / 2.0 - in.tf, yTopPlate + colH), l.hidden);
    for (const double sg : {-1.0, 1.0}) {
        const double x = sg * g.a;
        s.rect(Q(x - in.anchorDiameter / 2.0, -depthShown), Q(x + in.anchorDiameter / 2.0, yTopPlate + 25.0), l.bolt);
        s.rect(Q(x - t.washerSize / 2.0, yTopPlate), Q(x + t.washerSize / 2.0, yTopPlate + t.washerThickness), l.bolt);
        s.rect(Q(x - 0.8 * in.anchorDiameter, yTopPlate + t.washerThickness), Q(x + 0.8 * in.anchorDiameter, yTopPlate + t.washerThickness + 0.8 * in.anchorDiameter), l.bolt);
        s.rect(Q(x - 0.8 * in.anchorDiameter, grout - 0.8 * in.anchorDiameter), Q(x + 0.8 * in.anchorDiameter, grout), l.bolt);
    }
    weldTriangle(s, l, Q(-in.d / 2.0, yTopPlate), -1.0, 1.0, std::max(1.6, weldLeg * ks));
    weldTriangle(s, l, Q(in.d / 2.0, yTopPlate), 1.0, 1.0, std::max(1.6, weldLeg * ks));
    dimLinear(s, l, Q(g.lx / 2.0, grout), Q(g.lx / 2.0, yTopPlate), -12.0, fmtNum(in.plateThickness, 1));
    dimLinear(s, l, Q(g.lx / 2.0, 0.0), Q(g.lx / 2.0, grout), -12.0, fmtNum(grout, 0));
    dimLinear(s, l, Q(-g.lx / 2.0, yTopPlate + 1.0), Q(g.lx / 2.0, yTopPlate + 1.0), 22.0 + colH * ks, fmtNum(g.lx, 0));
    s.text(Q(g.lx / 2.0 + 44.0, -depthShown / 2.0), "ANCORAGEM h1 = " + fmtNum(t.h1, 0) + " mm", 2.0, l.text);
    s.text(Q(g.lx / 2.0 + 44.0, grout / 2.0), "ARGAMASSA EXPANSIVA", 2.0, l.text);

    if (notes) {
        auto text = connectionNotes(d, "");
        text.push_back("PLACA " + fmtNum(in.plateThickness, 1) + " x " + fmtNum(g.lx, 0) + " x " + fmtNum(g.ly, 0) + " mm; " + std::to_string(in.anchors) + " CHUMBADORES d" + fmtNum(in.anchorDiameter, 0) +
                       " ASTM A36 (h1 = " + fmtNum(t.h1, 0) + " mm)");
        text.push_back("ARRUELAS ESPECIAIS " + fmtNum(t.washerThickness, 1) + " x " + fmtNum(t.washerSize, 0) + " x " + fmtNum(t.washerSize, 0) + "; CONCRETO fck = " + fmtNum(in.fck, 0) + " MPa; BLOCO " +
                       fmtNum(g.blockN, 0) + " x " + fmtNum(g.blockB, 0) + " mm");
        text.push_back("SOLDA PILAR-PLACA: FILETE " + fmtNum(weldLeg, 0) + " mm CONTÍNUO; ARMADURA DO BLOCO CONFORME NBR 6118 E TABELA 18");
        notesFinish(s, l, rx0, ry0, text, d, "NBR 8800:2024 - 6.7; NBR 6118:2023.");
    }
    return scale;
}

// ---- ligacao tubular -------------------------------------------------------------------------------------------------

double drawColdFormedDetail(Sheet &s, const SheetBuilder::ConnectionDetail &d, double rx0, double ry0, double rx1, double ry1, bool notes, double fixedScale)
{
    const L l = layers(s);
    const NodeConnection &c = d.connection;
    const int mode = static_cast<int>(val(c, "mode", 0.0) + 0.5);
    const int n = std::max(1, static_cast<int>(val(c, "n", 2.0) + 0.5)), lines = std::max(1, static_cast<int>(val(c, "lines", 1.0) + 0.5));
    const double pitch = val(c, "pitch", 40.0), gauge = val(c, "gauge", 40.0), end1 = val(c, "end1", 30.0), end2 = val(c, "end2", 30.0), edge = val(c, "edge", 25.0);
    const double dd = val(c, "d", 12.0);
    const double dh = dd < 12.5 ? dd + 0.8 : dd + 1.5;
    const double lLong = val(c, "lLong", 80.0), lTrans = val(c, "lTrans", 0.0);
    double width = (lines - 1) * gauge + 2.0 * edge;
    double overlap = end1 + end2 + (n - 1) * pitch;
    if (mode == 1 || mode == 3) {
        width = std::max(width, std::max(lTrans, 40.0));
        overlap = std::max(overlap, lLong + 10.0);
    }
    const Panels p = panelsFor(rx0, ry0, rx1, ry1);
    const double extX = width + 60.0, extY = 2.0 * overlap + 80.0;
    const double scale = pickScaleMargin(extX, extY, p.lx1 - p.lx0, p.y1 - p.y0 - 8.0, 16.0, fixedScale);
    const double k = 1.0 / scale;
    const double cx = (p.lx0 + p.lx1) / 2.0, base = (p.y0 + p.y1) / 2.0 - overlap * k;
    auto P = [&](double x, double y) { return Point {cx + x * k, base + y * k}; };
    s.text({p.lx0 + 6.0, p.y1}, "LIGAÇÃO POR SOBREPOSIÇÃO - VISTA EM PLANTA", 3.2, l.text, TextAlign::Left, 0.0, true);
    // perfil (abaixo) e chapa (acima) sobrepostos em `overlap`
    s.rect(P(-width / 2.0, -overlap - 40.0), P(width / 2.0, overlap * 0.0 + 0.0), l.outline);
    s.line(P(-width / 2.0 - 4.0, -overlap - 46.0), P(width / 2.0 + 4.0, -overlap - 34.0), l.outline);
    s.text(P(width / 2.0 + 6.0, -overlap - 30.0), "PERFIL FORMADO A FRIO t1 = " + fmtNum(val(c, "t1", 2.0), 2), 2.2, l.text);
    s.rect(P(-width / 2.0, -overlap), P(width / 2.0, 40.0), l.plate);
    s.text(P(width / 2.0 + 6.0, 20.0), "CHAPA t2 = " + fmtNum(val(c, "t2", 4.75), 2), 2.2, l.text);
    if (mode == 1 || mode == 3) {
        const double w = val(c, "leg", 3.0);
        s.line(P(-width / 2.0, -overlap), P(width / 2.0, -overlap), l.weld);
        s.line(P(-width / 2.0, -overlap), P(-width / 2.0, -overlap + lLong), l.weld);
        s.line(P(width / 2.0, -overlap), P(width / 2.0, -overlap + lLong), l.weld);
        s.text(P(0.0, -overlap * 0.5), "FILETE " + fmtNum(w, 1) + " mm  (L// = " + fmtNum(lLong, 0) + ", L⟂ = " + fmtNum(lTrans, 0) + ")", 2.2, l.text, TextAlign::Center);
    } else {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < lines; ++j) {
                const double x = (j - (lines - 1) / 2.0) * gauge;
                const double y = -overlap + end2 + i * pitch;
                s.circle(P(x, y), std::max(0.6, dh / 2.0 * k), l.bolt);
            }
        }
        if (n > 1) {
            dimLinear(s, l, P(width / 2.0 + 8.0, -overlap + end2), P(width / 2.0 + 8.0, -overlap + end2 + pitch), 6.0, "s = " + fmtNum(pitch, 0));
        }
        dimLinear(s, l, P(-width / 2.0 - 8.0, -overlap), P(-width / 2.0 - 8.0, -overlap + end2), -6.0, "e2 = " + fmtNum(end2, 0));
    }
    if (notes) {
        auto text = connectionNotes(d, "");
        const char *modes[] = {"PARAFUSOS COMUNS (10.3)", "SOLDA DE FILETE (10.2)", "PARAFUSOS AUTOATARRAXANTES (10.5)"};
        text.push_back(std::string("MEIO DE LIGAÇÃO: ") + modes[std::max(0, std::min(2, mode))]);
        text.push_back("ESPESSURA DAS PARTES LIGADAS <= 4,75 mm (NBR 14762:2010, 10.2.1 E 10.3.1); ACIMA DISSO, NBR 8800.");
        notesFinish(s, l, rx0, ry0, text, d, "ABNT NBR 14762:2010, SEÇÃO 10 - LIGAÇÕES.");
    }
    return scale;
}

double drawEndPlateDetail(Sheet &s, const SheetBuilder::ConnectionDetail &d, double rx0, double ry0, double rx1, double ry1, bool notes, double fixedScale)
{
    const L l = layers(s);
    const NodeConnection &c = d.connection;
    const double db = val(c, "db", 20.0), dh = Structura::Design::Connections::standardHoleDiameter(db);
    const double depth = val(c, "beamD", 300.0), bfl = val(c, "bfl", 150.0), tfl = val(c, "tfl", 10.0), tw = val(c, "tw", 6.0);
    const double width = val(c, "plateW", 160.0), gauge = val(c, "gauge", 90.0), pitch = val(c, "pitch", 70.0), firstRow = val(c, "firstRow", 40.0), edge = val(c, "edgeV", 35.0);
    const int tRows = std::max(1, static_cast<int>(val(c, "tRows", 2.0) + 0.5)), sRows = std::max(1, static_cast<int>(val(c, "sRows", 1.0) + 0.5));
    const double extTop = firstRow + (tRows - 1) * pitch + edge, extBot = firstRow + (sRows - 1) * pitch + edge;
    const double height = depth + extTop + extBot;
    const Panels p = panelsFor(rx0, ry0, rx1, ry1);
    const double extX = width + 120.0, extY = height + 80.0;
    const double scale = pickScaleMargin(extX, extY, p.rx1 - p.lx0, p.y1 - p.y0 - 8.0, 16.0, fixedScale);
    const double k = 1.0 / scale;
    const double cx = (p.lx0 + p.rx1) / 2.0, cy = (p.y0 + p.y1) / 2.0 - 4.0;
    auto P = [&](double u, double t) { return Point {cx + u * k, cy + (t - (extTop - extBot) / 2.0) * k}; };
    s.text({p.lx0 + 6.0, p.y1}, "LIGAÇÃO RÍGIDA - CHAPA DE EXTREMIDADE (VISTA FRONTAL)", 3.2, l.text, TextAlign::Left, 0.0, true);
    s.rect(P(-width / 2.0, -(depth / 2.0 + extBot)), P(width / 2.0, depth / 2.0 + extTop), l.plate);
    // perfil da viga atras da chapa
    s.rect(P(-bfl / 2.0, depth / 2.0 - tfl), P(bfl / 2.0, depth / 2.0), l.hidden);
    s.rect(P(-bfl / 2.0, -depth / 2.0), P(bfl / 2.0, -depth / 2.0 + tfl), l.hidden);
    s.rect(P(-tw / 2.0, -depth / 2.0 + tfl), P(tw / 2.0, depth / 2.0 - tfl), l.hidden);
    s.line(P(-width / 2.0 - 10.0, 0.0), P(width / 2.0 + 10.0, 0.0), l.centre);
    s.line(P(0.0, -(depth / 2.0 + extBot) - 10.0), P(0.0, depth / 2.0 + extTop + 10.0), l.centre);
    auto bolt = [&](double u, double t) { s.circle(P(u, t), std::max(0.6, dh / 2.0 * k), l.bolt); };
    for (int i = 0; i < tRows; ++i) {
        for (const double col : {-1.0, 1.0}) {
            bolt(col * gauge / 2.0, depth / 2.0 + firstRow + i * pitch);
        }
    }
    for (int j = 0; j < sRows; ++j) {
        for (const double col : {-1.0, 1.0}) {
            bolt(col * gauge / 2.0, -(depth / 2.0 + firstRow + j * pitch));
        }
    }
    const double top = depth / 2.0 + extTop;
    dimLinear(s, l, P(-gauge / 2.0, depth / 2.0 + firstRow), P(gauge / 2.0, depth / 2.0 + firstRow), 14.0, "g = " + fmtNum(gauge, 0));
    dimLinear(s, l, P(width / 2.0, depth / 2.0), P(width / 2.0, depth / 2.0 + firstRow), 8.0, "b = " + fmtNum(firstRow, 0));
    if (tRows > 1) {
        dimLinear(s, l, P(width / 2.0, depth / 2.0 + firstRow), P(width / 2.0, depth / 2.0 + firstRow + pitch), 8.0, "p = " + fmtNum(pitch, 0));
    }
    dimLinear(s, l, P(-width / 2.0, -(depth / 2.0 + extBot)), P(-width / 2.0, top), -10.0, "H = " + fmtNum(height, 0));
    dimLinear(s, l, P(-width / 2.0, -(depth / 2.0 + extBot) - 2.0), P(width / 2.0, -(depth / 2.0 + extBot) - 2.0), -8.0, "B = " + fmtNum(width, 0));
    if (notes) {
        auto text = connectionNotes(d, "");
        text.push_back("CHAPA DE EXTREMIDADE t = " + fmtNum(val(c, "plateT", 22.4), 1) + " mm; SOLDA DAS MESAS " + fmtNum(val(c, "weldF", 8.0), 0) + " mm, DA ALMA " + fmtNum(val(c, "weldW", 6.0), 0) + " mm.");
        text.push_back("PARAFUSOS TRACIONADOS: " + std::to_string(2 * tRows) + " (ACIMA DA MESA); DE CORTANTE: " + std::to_string(2 * sRows) + " (ABAIXO).");
        if (val(c, "cont", 0.0) != 0.0) {
            text.push_back("ENRIJECEDORES HORIZONTAIS NO PILAR " + fmtNum(val(c, "contW", 60.0), 0) + " x " + fmtNum(val(c, "contT", 9.5), 1) + " mm, ALINHADOS COM AS MESAS DA VIGA.");
        }
        notesFinish(s, l, rx0, ry0, text, d, "ABNT NBR 8800:2024, 5.7 E 6.3 - LIGAÇÃO RÍGIDA.");
    }
    return scale;
}

double drawTubularDetail(Sheet &s, const SheetBuilder::ConnectionDetail &d, double rx0, double ry0, double rx1, double ry1, bool notes, double fixedScale)
{
    const L l = layers(s);
    const NodeConnection &c = d.connection;
    const int kind = static_cast<int>(val(c, "kindT", 2.0) + 0.5);
    const bool plate = static_cast<int>(val(c, "braceKind", 0.0) + 0.5) == 1;
    const double d0 = val(c, "d0", 114.3), t0 = val(c, "t0", 6.0);
    struct Br {
        double d, t, theta;
    };
    std::vector<Br> braces;
    braces.push_back({val(c, "d1", 60.3), val(c, "t1", 4.0), val(c, "th1", 45.0)});
    const bool two = kind == 2 || kind == 3;
    if (two) {
        braces.push_back({val(c, "d2", 60.3), val(c, "t2", 4.0), val(c, "th2", 45.0)});
    }
    const double gapIn = val(c, "gap", 20.0);
    const double ecc = val(c, "ecc", 0.0);
    const double pi = 3.14159265358979;
    const Panels p = panelsFor(rx0, ry0, rx1, ry1);
    const double brLen = 2.4 * std::max(braces[0].d, 0.6 * d0) + 80.0;
    double maxHoriz = 0.0, maxVert = 0.0;
    for (const Br &b : braces) {
        maxHoriz = std::max(maxHoriz, brLen * std::cos(b.theta * pi / 180.0) + b.d);
        maxVert = std::max(maxVert, brLen * std::sin(b.theta * pi / 180.0));
    }
    const double span = maxHoriz + 0.6 * d0 + 40.0;
    const bool downToo = kind == 1 && !plate;
    const double vUp = d0 / 2.0 + maxVert + 20.0, vDown = downToo ? d0 / 2.0 + maxVert + 20.0 : d0 / 2.0 + 20.0;
    const double extX = 2.0 * span, extY = vUp + vDown;
    const double scale = pickScaleMargin(extX, extY, p.rx1 - p.lx0, p.y1 - p.y0 - 8.0, 16.0, fixedScale);
    const double k = 1.0 / scale;
    const double cx = (p.lx0 + p.rx1) / 2.0, cy = (p.y0 + p.y1) / 2.0 - 4.0 - (vUp - vDown) / 2.0 * k;
    auto P = [&](double x, double y) { return Point {cx + x * k, cy + y * k}; };
    s.text({p.lx0 + 6.0, p.y1}, "ELEVAÇÃO DO NÓ", 3.2, l.text, TextAlign::Left, 0.0, true);

    // banzo
    s.line(P(-span, d0 / 2.0), P(span, d0 / 2.0), l.outline);
    s.line(P(-span, -d0 / 2.0), P(span, -d0 / 2.0), l.outline);
    s.line(P(-span, d0 / 2.0 - t0), P(span, d0 / 2.0 - t0), l.hidden);
    s.line(P(-span, -d0 / 2.0 + t0), P(span, -d0 / 2.0 + t0), l.hidden);
    s.line(P(-span - 10.0, 0.0), P(span + 10.0, 0.0), l.centre);
    // diagonais
    std::vector<double> toes;
    for (std::size_t i = 0; i < braces.size(); ++i) {
        const Br &b = braces[i];
        const double th = b.theta * pi / 180.0;
        // sentido: diagonal 1 vai para a esquerda e para cima (K) ou para cima (T/Y/X); diagonal 2 para a direita
        double ux = std::cos(th), uy = std::sin(th);
        if (two) {
            ux = i == 0 ? -std::cos(th) : std::cos(th);
        } else {
            ux = std::cos(th);
        }
        const double nx = -uy, ny = ux;
        // eixo passa pelo ponto (xo, ecc) do banzo; no K, xo = 0
        const double ox = 0.0, oy = ecc;
        const double wHalf = b.d / 2.0;
        const std::array<double, 2> sides {-1.0, 1.0};
        std::vector<Point> edge;
        for (const double sg : sides) {
            // intersecao da aresta com a face superior do banzo (y = d0/2) ou com o eixo, conforme o caso
            const double px = ox + nx * wHalf * sg, py = oy + ny * wHalf * sg;
            const double tHit = std::abs(uy) > 1e-6 ? (d0 / 2.0 - py) / uy : 0.0;
            const double hx = px + ux * tHit, hy = py + uy * tHit;
            const double len = brLen;
            const double ex = hx + ux * len, ey = hy + uy * len;
            s.line(P(hx, hy), P(ex, ey), l.outline);
            edge.push_back(P(ex, ey));
            toes.push_back(hx);
        }
        s.line(edge[0], edge[1], l.outline);
        if (!plate) {
            // espessura da parede da diagonal (linhas tracejadas)
            for (const double sg : sides) {
                const double px = ox + nx * (wHalf - b.t) * sg, py = oy + ny * (wHalf - b.t) * sg;
                const double tHit = std::abs(uy) > 1e-6 ? (d0 / 2.0 - py) / uy : 0.0;
                s.line(P(px + ux * tHit, py + uy * tHit), P(px + ux * (tHit + brLen), py + uy * (tHit + brLen)), l.hidden);
            }
        }
        s.line(P(ox - ux * 10.0, oy - uy * 10.0), P(ox + ux * (brLen + 20.0), oy + uy * (brLen + 20.0)), l.centre);
        s.text(P(ox + ux * (brLen * 0.55) + nx * (wHalf + 10.0 * scale), oy + uy * (brLen * 0.55) + ny * (wHalf + 10.0 * scale)),
               (plate ? "CH. " : "Ø") + fmtNum(b.d, 1) + " x " + fmtNum(b.t, 1) + "  θ" + std::to_string(i + 1) + " = " + fmtNum(b.theta, 0) + "°", 2.2, l.text, TextAlign::Center);
    }
    if (kind == 1 && !plate) {
        // X: diagonal espelhada abaixo do banzo
        const Br &b = braces[0];
        const double th = b.theta * pi / 180.0;
        const double ux = std::cos(th), uy = -std::sin(th), nx = -uy, ny = ux;
        const double wHalf = b.d / 2.0;
        std::vector<Point> edge;
        for (const double sg : {-1.0, 1.0}) {
            const double px = nx * wHalf * sg, py = ny * wHalf * sg;
            const double tHit = (-d0 / 2.0 - py) / uy;
            const double hx = px + ux * tHit, hy = py + uy * tHit;
            s.line(P(hx, hy), P(hx + ux * brLen, hy + uy * brLen), l.outline);
            edge.push_back(P(hx + ux * brLen, hy + uy * brLen));
        }
        s.line(edge[0], edge[1], l.outline);
    }
    if (kind == 2 && toes.size() >= 4) {
        // afastamento entre os pes das diagonais, na face superior do banzo
        const double leftToe = std::max(toes[0], toes[1]), rightToe = std::min(toes[2], toes[3]);
        if (rightToe > leftToe) {
            dimLinear(s, l, P(leftToe, d0 / 2.0), P(rightToe, d0 / 2.0), -9.0, "g = " + fmtNum(gapIn, 0));
        }
    }
    s.text(P(span * 0.55, d0 / 2.0 + 14.0 * scale * 0.0 - d0 * 0.9), "BANZO Ø" + fmtNum(d0, 1) + " x " + fmtNum(t0, 1), 2.2, l.text, TextAlign::Center);
    if (std::abs(ecc) > 1e-6) {
        dimLinear(s, l, P(-span * 0.8, 0.0), P(-span * 0.8, ecc), 7.0, "e = " + fmtNum(ecc, 0));
    }

    if (notes) {
        auto text = connectionNotes(d, "");
        const char *kinds[] = {"T / Y", "X", "K / N COM AFASTAMENTO", "K / N COM SOBREPOSIÇÃO"};
        text.push_back(std::string("TIPO: ") + kinds[std::max(0, std::min(3, kind))] + (plate ? " - CHAPA TRANSVERSAL" : ""));
        text.push_back("BANZO Ø" + fmtNum(d0, 1) + " x " + fmtNum(t0, 1) + " mm; DIAGONAL 1 Ø" + fmtNum(braces[0].d, 1) + " x " + fmtNum(braces[0].t, 1) + " mm" +
                       (two ? "; DIAGONAL 2 Ø" + fmtNum(braces[1].d, 1) + " x " + fmtNum(braces[1].t, 1) + " mm" : ""));
        text.push_back(kind == 3 ? "SOBREPOSIÇÃO Ov = " + fmtNum(val(c, "overlap", 50.0), 0) + " %" : (kind == 2 ? "AFASTAMENTO g = " + fmtNum(gapIn, 0) + " mm" : "ECENTRICIDADE e = " + fmtNum(ecc, 0) + " mm"));
        text.push_back("SOLDA DE COMPOSIÇÃO AO REDOR DAS DIAGONAIS COM GARGANTA >= ESPESSURA DA DIAGONAL (RESISTÊNCIA TOTAL)");
        notesFinish(s, l, rx0, ry0, text, d, "CIDECT DESIGN GUIDE 1 (2008) - TABELAS 4.1, 4.3, 6.1 E 7.1.");
    }
    return scale;
}


// ---- detalhamento generico do catalogo: vistas frontal e superior geradas dos solidos, com linhas ocultas --------------------------------

struct CatalogGeometry {
    std::vector<Structura::Drawing::Triangle3D> faces;
    std::vector<Structura::Drawing::Edge3D> edges;
};

void addQuad(CatalogGeometry &g, const Structura::Drawing::Vec3 &a, const Structura::Drawing::Vec3 &b, const Structura::Drawing::Vec3 &c, const Structura::Drawing::Vec3 &d)
{
    g.faces.push_back({a, b, c});
    g.faces.push_back({a, c, d});
}

/// Converte os solidos (s, t, u) em faces e arestas no espaco do desenho: X = s, Y = u (profundidade da vista frontal), Z = t.
CatalogGeometry catalogGeometry(const std::vector<ConnectionSolids::Primitive> &primitives)
{
    using Vec3 = Structura::Drawing::Vec3;
    CatalogGeometry g;
    auto P = [](double s, double t, double u) { return Vec3 {s, u, t}; };
    for (const auto &p : primitives) {
        const int tag = static_cast<int>(p.role);
        auto edge = [&](const Vec3 &a, const Vec3 &b) { g.edges.push_back({a, b, tag}); };
        if (p.kind == ConnectionSolids::Kind::Box) {
            const double hs = p.size[0] / 2.0, ht = p.size[1] / 2.0, hu = p.size[2] / 2.0;
            Vec3 v[8];
            int n = 0;
            for (const double ds : {-1.0, 1.0}) {
                for (const double dt : {-1.0, 1.0}) {
                    for (const double du : {-1.0, 1.0}) {
                        v[n++] = P(p.a[0] + ds * hs, p.a[1] + dt * ht, p.a[2] + du * hu);
                    }
                }
            }
            // indices: bit2 = s, bit1 = t, bit0 = u
            auto V = [&](int s, int t, int u) { return v[s * 4 + t * 2 + u]; };
            addQuad(g, V(0, 0, 0), V(1, 0, 0), V(1, 1, 0), V(0, 1, 0));
            addQuad(g, V(0, 0, 1), V(0, 1, 1), V(1, 1, 1), V(1, 0, 1));
            addQuad(g, V(0, 0, 0), V(0, 0, 1), V(1, 0, 1), V(1, 0, 0));
            addQuad(g, V(0, 1, 0), V(1, 1, 0), V(1, 1, 1), V(0, 1, 1));
            addQuad(g, V(0, 0, 0), V(0, 1, 0), V(0, 1, 1), V(0, 0, 1));
            addQuad(g, V(1, 0, 0), V(1, 0, 1), V(1, 1, 1), V(1, 1, 0));
            for (int a = 0; a < 8; ++a) {
                for (int bit = 0; bit < 3; ++bit) {
                    const int b = a | (1 << bit);
                    if (b != a) {
                        edge(v[a], v[b]);
                    }
                }
            }
        } else if (p.kind == ConnectionSolids::Kind::Prism) {
            const auto &poly = p.polygon;
            const auto tri = Structura::Drawing::triangulatePolygon(poly);
            for (std::size_t i = 0; i + 2 < tri.size(); i += 3) {
                for (const double u : {p.u0, p.u1}) {
                    g.faces.push_back({P(poly[static_cast<std::size_t>(tri[i])][0], poly[static_cast<std::size_t>(tri[i])][1], u),
                                       P(poly[static_cast<std::size_t>(tri[i + 1])][0], poly[static_cast<std::size_t>(tri[i + 1])][1], u),
                                       P(poly[static_cast<std::size_t>(tri[i + 2])][0], poly[static_cast<std::size_t>(tri[i + 2])][1], u)});
                }
            }
            for (std::size_t i = 0; i < poly.size(); ++i) {
                const auto &a = poly[i], &b = poly[(i + 1) % poly.size()];
                addQuad(g, P(a[0], a[1], p.u0), P(b[0], b[1], p.u0), P(b[0], b[1], p.u1), P(a[0], a[1], p.u1));
                edge(P(a[0], a[1], p.u0), P(b[0], b[1], p.u0));
                edge(P(a[0], a[1], p.u1), P(b[0], b[1], p.u1));
                edge(P(a[0], a[1], p.u0), P(a[0], a[1], p.u1));
            }
        } else {
            // cilindro (parafuso): prisma de 8 lados ao redor do eixo
            const std::array<double, 3> axis {p.b[0] - p.a[0], p.b[1] - p.a[1], p.b[2] - p.a[2]};
            const double len = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
            if (len < 1e-9) {
                continue;
            }
            std::array<double, 3> w {axis[0] / len, axis[1] / len, axis[2] / len};
            const std::array<double, 3> ref = std::abs(w[0]) < 0.9 ? std::array<double, 3> {1, 0, 0} : std::array<double, 3> {0, 1, 0};
            std::array<double, 3> e1 {w[1] * ref[2] - w[2] * ref[1], w[2] * ref[0] - w[0] * ref[2], w[0] * ref[1] - w[1] * ref[0]};
            const double l1 = std::sqrt(e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2]);
            for (double &x : e1) {
                x /= l1;
            }
            const std::array<double, 3> e2 {w[1] * e1[2] - w[2] * e1[1], w[2] * e1[0] - w[0] * e1[2], w[0] * e1[1] - w[1] * e1[0]};
            constexpr int kSides = 8;
            std::array<Vec3, kSides> lo, hi;
            for (int i = 0; i < kSides; ++i) {
                const double ang = 2.0 * 3.14159265358979 * i / kSides;
                const double cs = std::cos(ang) * p.radius, sn = std::sin(ang) * p.radius;
                lo[static_cast<std::size_t>(i)] = P(p.a[0] + cs * e1[0] + sn * e2[0], p.a[1] + cs * e1[1] + sn * e2[1], p.a[2] + cs * e1[2] + sn * e2[2]);
                hi[static_cast<std::size_t>(i)] = P(p.b[0] + cs * e1[0] + sn * e2[0], p.b[1] + cs * e1[1] + sn * e2[1], p.b[2] + cs * e1[2] + sn * e2[2]);
            }
            for (int i = 0; i < kSides; ++i) {
                const auto a = static_cast<std::size_t>(i), b = static_cast<std::size_t>((i + 1) % kSides);
                addQuad(g, lo[a], lo[b], hi[b], hi[a]);
                edge(lo[a], lo[b]);
                edge(hi[a], hi[b]);
                if (i % 2 == 0) {
                    edge(lo[a], hi[a]);
                }
            }
            for (int i = 1; i + 1 < kSides; ++i) {
                g.faces.push_back({lo[0], lo[static_cast<std::size_t>(i)], lo[static_cast<std::size_t>(i + 1)]});
                g.faces.push_back({hi[0], hi[static_cast<std::size_t>(i)], hi[static_cast<std::size_t>(i + 1)]});
            }
        }
    }
    return g;
}

double drawCatalogDetail(Sheet &s, const SheetBuilder::ConnectionDetail &d, double rx0, double ry0, double rx1, double ry1, bool notes, double fixedScale)
{
    const L l = layers(s);
    const NodeConnection &c = d.connection;
    const auto prims = ConnectionSolids::build(c, true);
    const CatalogGeometry geo = catalogGeometry(prims);
    const Panels p = panelsFor(rx0, ry0, rx1, ry1);
    const double target = ConnectionSolids::extent(prims);
    const auto front = Structura::Drawing::computeHiddenLines(Structura::Drawing::cameraFromAngles(0.0, 0.0, {0.0, 0.0, 0.0}, 4.0 * target + 1000.0), geo.faces, geo.edges, true);
    const auto top = Structura::Drawing::computeHiddenLines(Structura::Drawing::cameraFromAngles(0.0, 90.0, {0.0, 0.0, 0.0}, 4.0 * target + 1000.0), geo.faces, geo.edges, true);
    if (!front.ok) {
        return 1.0;
    }
    const double fw = front.maxX - front.minX, fh = front.maxY - front.minY, tw = top.ok ? top.maxX - top.minX : 0.0, th = top.ok ? top.maxY - top.minY : 0.0;
    const double leftW = p.lx1 - p.lx0, rightW = p.rx1 - p.rx0, height = p.y1 - p.y0 - 8.0;
    const double scale = std::max(pickScaleMargin(fw + 60.0, fh + 60.0, leftW, height, 16.0, fixedScale), pickScaleMargin(tw + 60.0, th + 60.0, rightW, height, 16.0, fixedScale));
    const double k = 1.0 / scale;
    const double fcx = (front.minX + front.maxX) / 2.0, fcy = (front.minY + front.maxY) / 2.0;
    const double tcx = (top.minX + top.maxX) / 2.0, tcy = (top.minY + top.maxY) / 2.0;
    const double cxF = (p.lx0 + p.lx1) / 2.0, cyF = (p.y0 + p.y1) / 2.0 - 4.0;
    const double cxT = (p.rx0 + p.rx1) / 2.0, cyT = cyF;
    auto layerOf = [&](int tag) {
        switch (static_cast<ConnectionSolids::Role>(tag)) {
        case ConnectionSolids::Role::Plate: return l.plate;
        case ConnectionSolids::Role::Bolt: return l.bolt;
        case ConnectionSolids::Role::Weld: return l.weld;
        default: return l.outline;
        }
    };
    auto draw = [&](const Structura::Drawing::HiddenLineResult &view, double cx, double cy, double mx, double my) {
        for (const auto &seg : view.segments) {
            s.line({cx + (seg.x0 - mx) * k, cy + (seg.y0 - my) * k}, {cx + (seg.x1 - mx) * k, cy + (seg.y1 - my) * k}, seg.hidden ? l.hidden : layerOf(seg.tag));
        }
    };
    s.text({p.lx0 + 6.0, p.y1}, "VISTA FRONTAL (PLANO DA LIGAÇÃO)", 3.2, l.text, TextAlign::Left, 0.0, true);
    draw(front, cxF, cyF, fcx, fcy);
    if (top.ok) {
        s.text({p.rx0 + 6.0, p.y1}, "VISTA SUPERIOR", 3.2, l.text, TextAlign::Left, 0.0, true);
        draw(top, cxT, cyT, tcx, tcy);
    }
    // cotas da familia (vista frontal: s horizontal, t vertical)
    const auto *entry = ConnectionCatalog::find(c.type);
    if (entry) {
        const auto &ops = ConnectionCatalog::ops(entry->family);
        if (ops.dims) {
            for (const auto &dim : ops.dims(c)) {
                dimLinear(s, l, {cxF + (dim.s1 - fcx) * k, cyF + (dim.t1 - fcy) * k}, {cxF + (dim.s2 - fcx) * k, cyF + (dim.t2 - fcy) * k}, dim.offset, dim.text);
            }
        }
        if (notes) {
            auto text = connectionNotes(d, "");
            if (ops.notes) {
                for (const auto &line : ops.notes(c)) {
                    text.push_back(line);
                }
            }
            notesFinish(s, l, rx0, ry0, text, d, "ABNT NBR 8800:2024, SEÇÕES 5, 6.2, 6.3 E 6.5 - LIGAÇÃO CONFORME CATÁLOGO.");
        }
    }
    return scale;
}

} // namespace

std::vector<Structura::Design::Connections::Part> connectionParts(const SheetBuilder::ConnectionDetail &d)
{
    return ConnectionService::evaluate(d.connection, {}).parts;
}

void dimension(Sheet &s, int layer, Point p1, Point p2, double offset, const std::string &text, double textHeight)
{
    L l;
    l.dim = layer;
    dimLinear(s, l, p1, p2, offset, text, textHeight);
}

double drawConnectionDetail(Sheet &s, const SheetBuilder::ConnectionDetail &d, double rx0, double ry0, double rx1, double ry1, bool notes, double fixedScale, bool profileOutline)
{
    const L l = layers(s);
    const NodeConnection &c = d.connection;
    if (ConnectionCatalog::isCatalog(c.type)) {
        const auto *entry = ConnectionCatalog::find(c.type);
        if (entry && entry->legacyType >= 0) {
            SheetBuilder::ConnectionDetail legacy = d;
            legacy.connection = ConnectionCatalog::asLegacy(c);
            return drawConnectionDetail(s, legacy, rx0, ry0, rx1, ry1, notes, fixedScale, profileOutline);
        }
        return drawCatalogDetail(s, d, rx0, ry0, rx1, ry1, notes, fixedScale);
    }
    switch (c.type) {
    case NodeConnection::kGusset: return drawGussetDetail(s, d, rx0, ry0, rx1, ry1, notes, fixedScale);
    case NodeConnection::kSplice: return drawSpliceDetail(s, d, rx0, ry0, rx1, ry1, notes, fixedScale);
    case NodeConnection::kBasePlate: return drawBasePlateDetail(s, d, rx0, ry0, rx1, ry1, notes, fixedScale);
    case NodeConnection::kTubular: return drawTubularDetail(s, d, rx0, ry0, rx1, ry1, notes, fixedScale);
    case NodeConnection::kColdFormed: return drawColdFormedDetail(s, d, rx0, ry0, rx1, ry1, notes, fixedScale);
    case NodeConnection::kEndPlate: return drawEndPlateDetail(s, d, rx0, ry0, rx1, ry1, notes, fixedScale);
    default: break;
    }
    const bool angle = c.type == NodeConnection::kDoubleAngle;
    const double db = val(c, "db", 20.0);
    const double dh = Structura::Design::Connections::standardHoleDiameter(db);
    const int rows = std::max(1, static_cast<int>(val(c, "rows", 3.0) + 0.5));
    const double pitch = val(c, "pitch", 70.0);
    const double edgeV = val(c, "edgeV", 35.0);
    const double tw = val(c, "webT", 6.0);
    const double beamDepth = d.beamDepthMm > 0.0 ? d.beamDepthMm : val(c, "beamDepth", 0.0);
    const double tf = d.beamFlangeMm > 0.0 ? d.beamFlangeMm : 10.0;
    const double H = (rows - 1) * pitch + 2.0 * edgeV;

    // geometria especifica
    double legWeb = 0, a = 0, edgeH = 0, setback = 0, plateT = 0, W = 0, gaugeSupport = 0, supEdge = 0, angleT = 0;
    if (angle) {
        angleT = val(c, "angleT", 9.5);
        a = val(c, "gaugeWeb", 50.0);
        legWeb = a + val(c, "webEnd", 30.0);
        gaugeSupport = val(c, "gaugeSupport", 50.0);
        supEdge = val(c, "supEdge", 35.0);
        setback = angleT;
        W = legWeb;
    } else {
        plateT = val(c, "plateT", 9.5);
        a = val(c, "a", 60.0);
        edgeH = val(c, "edgeH", 35.0);
        setback = val(c, "setback", 12.0);
        W = a + edgeH;
    }
    const double depth = beamDepth > 0.0 ? beamDepth : H + 60.0;
    const double yHalf = std::max(depth / 2.0, H / 2.0) + 12.0;
    const double xMin = -28.0, xMax = W + 70.0;

    // regioes: elevacao (esquerda) e corte (direita)
    const double reserve = 28.0;
    const double totalW = rx1 - rx0, totalH = ry1 - ry0 - 10.0;
    const double elevW = totalW * 0.58 - reserve, secW = totalW * 0.38 - reserve;
    // corte em planta: x de -15 a xMax; y de -(largura) a +(largura)
    const double secHalf = angle ? std::max(gaugeSupport + supEdge + tw / 2.0 + 10.0, 40.0) : std::max(plateT + tw + 40.0, 40.0);
    const double scale = pickScale(std::max(xMax - xMin, xMax + 15.0 + 20.0), std::max(2.0 * yHalf, 2.0 * secHalf), std::min(elevW, secW), totalH - 2.0 * reserve, fixedScale);
    const double k = 1.0 / scale;
    const double ex = rx0 + reserve + 6.0 - xMin * k;
    const double ey = ry0 + 10.0 + (totalH) / 2.0;
    auto E = [&](double x, double y) { return Point {ex + x * k, ey + y * k}; };

    // ---------------------------------------------------------------- vista (elevacao pela alma)
    // apoio
    s.rect(E(xMin, -yHalf), E(0.0, yHalf), l.outline);
    hatchRect(s, l, ex + xMin * k, ey - yHalf * k, ex, ey + yHalf * k, 2.2);
    s.text(E(xMin / 2.0, yHalf + 3.0 * scale), "APOIO", 2.4, l.text, TextAlign::Center);
    // viga: mesas e alma
    if (beamDepth > 0.0) {
        s.line(E(setback, depth / 2.0), E(xMax, depth / 2.0), l.outline);
        s.line(E(setback, -depth / 2.0), E(xMax, -depth / 2.0), l.outline);
        if (profileOutline) {   // mesas: faces internas (detalhe do perfil)
            s.line(E(setback, depth / 2.0 - tf), E(xMax, depth / 2.0 - tf), l.outline);
            s.line(E(setback, -depth / 2.0 + tf), E(xMax, -depth / 2.0 + tf), l.outline);
        }
    }
    s.line(E(setback, -depth / 2.0), E(setback, depth / 2.0), l.outline);
    s.line(E(xMax, -depth / 2.0), E(xMax, depth / 2.0), l.hidden);   // corte da vista
    s.line(E(xMin, 0.0), E(xMax + 10.0, 0.0), l.centre);
    // chapa / cantoneira (aba da alma)
    const double plateW = angle ? legWeb : W;
    s.rect(E(angle ? 0.0 : 0.0, -H / 2.0), E(plateW, H / 2.0), l.plate);
    // parafusos (linha de furos)
    for (int i = 0; i < rows; ++i) {
        const double y = (i - (rows - 1) / 2.0) * pitch;
        s.circle(E(a, y), std::max(0.6, dh / 2.0 * k), l.bolt);
        centreCross(s, l, E(a, y), std::max(1.5, dh * k * 0.9));
    }
    // solda (chapa de cisalhamento): triangulos nas extremidades da chapa junto ao apoio
    if (!angle) {
        const double leg = val(c, "weldLeg", 6.0);
        const double t = std::max(1.6, leg * k);
        for (const double sign : {1.0, -1.0}) {
            const Point corner = E(0.0, sign * H / 2.0);
            s.polyline({corner, {corner.x + t, corner.y}, {corner.x, corner.y - sign * t}}, true, l.weld, true);
        }
        const Point lead = E(0.0, H / 2.0);
        s.line({lead.x + 1.5, lead.y - 1.0}, {lead.x + 14.0, lead.y + 9.0}, l.weld);
        s.text({lead.x + 15.0, lead.y + 9.0}, "FILETE " + fmtNum(leg, 0) + " mm - 2 LADOS", 2.2, l.weld == 0 ? l.text : l.text);
    }
    // cotas da vista
    {
        const double off = 10.0;
        for (int i = 0; i + 1 < rows; ++i) {
            const double y0 = (i - (rows - 1) / 2.0) * pitch, y1 = (i + 1 - (rows - 1) / 2.0) * pitch;
            dimLinear(s, l, E(a, y0), E(a, y1), off + 6.0, fmtNum(pitch, 0));
        }
        dimLinear(s, l, E(a, -(rows - 1) * pitch / 2.0 - edgeV), E(a, -(rows - 1) * pitch / 2.0), off + 6.0, fmtNum(edgeV, 0));
        dimLinear(s, l, E(a, (rows - 1) * pitch / 2.0), E(a, (rows - 1) * pitch / 2.0 + edgeV), off + 6.0, fmtNum(edgeV, 0));
        dimLinear(s, l, E(plateW, -H / 2.0), E(plateW, H / 2.0), off + 18.0, fmtNum(H, 0));
        // horizontais: apoio -> linha de furos -> fim da chapa
        dimLinear(s, l, E(0.0, -H / 2.0), E(a, -H / 2.0), -(off), fmtNum(a, 0));
        dimLinear(s, l, E(a, -H / 2.0), E(plateW, -H / 2.0), -(off), fmtNum(plateW - a, 0));
        dimLinear(s, l, E(0.0, -H / 2.0), E(plateW, -H / 2.0), -(off + 8.0), fmtNum(plateW, 0));
        if (setback > 0.0) {
            dimLinear(s, l, E(0.0, depth / 2.0), E(setback, depth / 2.0), off, fmtNum(setback, 0));
        }
    }
    s.text({rx0 + reserve + 6.0, ry1 - 4.0}, "VISTA", 3.2, l.text, TextAlign::Left, 0.0, true);

    // ---------------------------------------------------------------- corte A-A (em planta)
    const double sx = rx0 + totalW * 0.58 + reserve + 8.0;
    const double sy = ey;
    auto Sx = [&](double x, double y) { return Point {sx + x * k, sy + y * k}; };
    s.text({sx - 4.0, ry1 - 4.0}, "CORTE A-A (PLANTA)", 3.2, l.text, TextAlign::Left, 0.0, true);
    s.rect(Sx(-14.0, -secHalf), Sx(0.0, secHalf), l.outline);
    hatchRect(s, l, sx - 14.0 * k, sy - secHalf * k, sx, sy + secHalf * k, 2.0);
    const double webStart = setback;
    if (!angle) {
        const double plateY0 = 0.0;   // chapa de um lado da alma
        s.rect(Sx(0.0, plateY0), Sx(W, plateY0 + plateT), l.plate);
        s.rect(Sx(webStart, plateY0 + plateT), Sx(W + 60.0, plateY0 + plateT + tw), l.outline);
        // flange (linha de corte) indicada pela largura simbolica
        // parafuso
        const double yTop = plateY0 + plateT + tw + 10.0, yBot = -10.0;
        s.rect(Sx(a - db / 2.0, yBot), Sx(a + db / 2.0, yTop), l.bolt);
        s.rect(Sx(a - 0.8 * db, yTop), Sx(a + 0.8 * db, yTop + 0.7 * db), l.bolt);
        s.rect(Sx(a - 0.8 * db, yBot - 0.8 * db), Sx(a + 0.8 * db, yBot), l.bolt);
        // solda: triangulos dos dois lados da chapa
        const double leg = val(c, "weldLeg", 6.0);
        const double t = std::max(1.5, leg * k);
        const Point p0 = Sx(0.0, plateY0), p1 = Sx(0.0, plateY0 + plateT);
        s.polyline({p0, {p0.x + t, p0.y}, {p0.x, p0.y - t}}, true, l.weld, true);
        s.polyline({p1, {p1.x + t, p1.y}, {p1.x, p1.y + t}}, true, l.weld, true);
        dimLinear(s, l, Sx(0.0, -secHalf * 0.0 - 12.0), Sx(a, -12.0), 0.0, fmtNum(a, 0));
        s.text(Sx(W + 5.0, plateY0 + plateT / 2.0 - 3.0), "CH. " + fmtNum(plateT, 1) + " mm", 2.2, l.text);
        s.text(Sx(W + 5.0, plateY0 + plateT + tw / 2.0 + 1.0), "ALMA " + fmtNum(tw, 1) + " mm", 2.2, l.text);
    } else {
        const double ty = tw / 2.0;
        s.rect(Sx(webStart, -ty), Sx(W + 60.0, ty), l.outline);   // alma
        for (const double sign : {1.0, -1.0}) {
            // aba da alma (junto a alma) e aba do apoio (junto ao apoio)
            const double yIn = sign * ty, yOut = sign * (ty + angleT);
            s.rect(Sx(0.0, std::min(yIn, yOut)), Sx(legWeb, std::max(yIn, yOut)), l.plate);
            const double yEnd = sign * (ty + gaugeSupport + supEdge);
            s.rect(Sx(0.0, std::min(yIn, yEnd)), Sx(angleT, std::max(yIn, yEnd)), l.plate);
            // parafuso do apoio (horizontal) na linha de furos da aba do apoio
            const double yb = sign * (ty + gaugeSupport);
            s.rect(Sx(-10.0, yb - db / 2.0), Sx(angleT + 14.0, yb + db / 2.0), l.bolt);
            s.rect(Sx(angleT + 14.0, yb - 0.8 * db), Sx(angleT + 14.0 + 0.7 * db, yb + 0.8 * db), l.bolt);
        }
        // parafuso da alma (atravessa cantoneiras e alma)
        const double yTop = ty + angleT + 10.0;
        s.rect(Sx(a - db / 2.0, -yTop), Sx(a + db / 2.0, yTop), l.bolt);
        s.rect(Sx(a - 0.8 * db, yTop), Sx(a + 0.8 * db, yTop + 0.7 * db), l.bolt);
        s.rect(Sx(a - 0.8 * db, -yTop - 0.7 * db), Sx(a + 0.8 * db, -yTop), l.bolt);
        s.text(Sx(legWeb + 5.0, ty + angleT / 2.0 + 1.0), "2L t=" + fmtNum(angleT, 1) + " mm", 2.2, l.text);
        s.text(Sx(legWeb + 5.0, -2.0), "ALMA " + fmtNum(tw, 1) + " mm", 2.2, l.text);
        dimLinear(s, l, Sx(0.0, -secHalf + 6.0), Sx(a, -secHalf + 6.0), 0.0, fmtNum(a, 0));
    }
    s.text(Sx(-12.0, secHalf + 2.0 * scale), "APOIO", 2.4, l.text);

    // ---------------------------------------------------------------- notas
    if (notes) {
        std::vector<std::string> lines;
        lines.push_back(std::string("LIGAÇÃO: ") + ConnectionService::typeName(c.type).toStdString() + " - NÓ " + std::to_string(c.nodeId) + ", VIGA " + std::to_string(c.memberId));
        if (!d.beamSection.isEmpty()) {
            lines.push_back("VIGA: " + d.beamSection.toStdString());
        }
        lines.push_back("PARAFUSOS: " + std::to_string(rows) + " x d" + fmtNum(db, 0) + " " + c.grade.toStdString() + " (furo " + fmtNum(dh, 1) + " mm)");
        if (angle) {
            lines.push_back("2 CANTONEIRAS t = " + fmtNum(angleT, 1) + " mm; PARAFUSOS NO APOIO: 2 x " + std::to_string(rows));
        } else {
            lines.push_back("CHAPA " + fmtNum(plateT, 1) + " x " + fmtNum(W, 0) + " x " + fmtNum(H, 0) + " mm");
            lines.push_back("SOLDA DE FILETE " + fmtNum(val(c, "weldLeg", 6.0), 0) + " mm EM AMBOS OS LADOS, ELETRODO fw = " + fmtNum(val(c, "weldFw", 485.0), 0) + " MPa");
        }
        if (d.hasForces) {
            lines.push_back("ESFORÇOS DE CÁLCULO: V = " + fmtNum(d.shearKN, 1) + " kN; N = " + fmtNum(d.axialKN, 1) + " kN; APROVEIT. η = " + fmtNum(d.eta, 2));
        }
        lines.push_back("NBR 8800:2024 - 6.2, 6.3 E 6.5. COTAS EM mm.");
        notesBlock(s, l, rx0 + 6.0, 52.0, lines);
    }
    return scale;
}

} // namespace Structura::App::Services::internal
