#include "app/services/FabricationModel.h"
#include "app/services/CutListService.h"
#include "app/services/MemberFabrication.h"
#include "app/services/SheetBuilderInternal.h"

#include "drawing/DrawingSymbols.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace Structura::App::Services::internal {

using namespace Structura::Drawing;

namespace {

std::string num(double v, int decimals = 0)
{
    char buffer[48];
    std::snprintf(buffer, sizeof buffer, "%.*f", decimals, v);
    std::string s = buffer;
    for (char &c : s) {
        if (c == '.') {
            c = ',';
        }
    }
    return s;
}

std::string numTrim(double v)
{
    return num(v, std::abs(v - std::round(v)) < 0.05 ? 0 : 1);
}

std::string clip(const std::string &s, std::size_t n)
{
    return s.size() <= n ? s : s.substr(0, n > 1 ? n - 1 : 0) + ".";
}

double pickPlateScale(double w, double h, double cw, double ch)
{
    static const double scales[] = {1, 2, 2.5, 4, 5, 8, 10, 15, 20, 25, 30, 40, 50};
    for (const double s : scales) {
        if (w / s <= cw && h / s <= ch) {
            return s;
        }
    }
    return 100.0;
}

/// Valores distintos (ordenados, com tolerancia de 0,05 mm) de uma lista.
std::vector<double> distinct(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    std::vector<double> out;
    for (const double v : values) {
        if (out.empty() || v - out.back() > 0.05) {
            out.push_back(v);
        }
    }
    return out;
}

void table(Sheet &s, int lineLayer, int textLayer, double x, double y, const std::vector<double> &widths, const std::vector<std::vector<std::string>> &rows, const std::string &title)
{
    const double rowH = 4.6;
    double total = 0.0;
    for (const double w : widths) {
        total += w;
    }
    s.text({x, y + 1.0}, title, 2.8, textLayer, TextAlign::Left, 0.0, true);
    double yy = y;
    for (std::size_t r = 0; r < rows.size(); ++r) {
        s.line({x, yy}, {x + total, yy}, lineLayer);
        double xx = x;
        for (std::size_t c = 0; c < widths.size() && c < rows[r].size(); ++c) {
            s.text({xx + 1.0, yy - rowH + 1.3}, clip(rows[r][c], static_cast<std::size_t>(std::max(2.0, widths[c] / 1.35))), 2.0, textLayer, TextAlign::Left, 0.0, r == 0);
            xx += widths[c];
        }
        yy -= rowH;
    }
    s.line({x, yy}, {x + total, yy}, lineLayer);
    double xx = x;
    for (std::size_t c = 0; c <= widths.size(); ++c) {
        s.line({xx, y}, {xx, yy}, lineLayer);
        if (c < widths.size()) {
            xx += widths[c];
        }
    }
}

} // namespace

double drawPlatePanel(Sheet &s, const FabPart &p, double cx0, double cyTop, double cw, double ch)
{
    const int lineLayer = s.addLayer("TABELA", {0, 0, 0}, 0.18);
    const int textLayer = s.addLayer("TEXTO", {0, 0, 0}, 0.18);
    const int plateLayer = s.addLayer("CHAPAS", {20, 60, 140}, 0.35);
    const int holeLayer = s.addLayer("FUROS", {150, 60, 0}, 0.25);
    const int centreLayer = s.addLayer("EIXOS", {200, 40, 40}, 0.13, LineStyle::DashDot);
    const int dimLayer = s.addLayer("COTAS", {0, 120, 0}, 0.13);
    (void)lineLayer;
    const double marginX = 22.0, marginTop = 9.0, marginBottom = 16.0;
    const double scale = pickPlateScale(p.width, p.height, cw - 2.0 * marginX, ch - marginTop - marginBottom);
    const double k = 1.0 / scale;
    const double ox = cx0 + marginX + std::max(0.0, (cw - 2.0 * marginX - p.width * k) / 2.0);
    const double oy = cyTop - marginTop - p.height * k - std::max(0.0, (ch - marginTop - marginBottom - p.height * k) / 2.0);
    auto P = [&](double a, double b) { return Point {ox + a * k, oy + b * k}; };
    s.text({cx0 + cw / 2.0, cyTop - 4.0}, p.mark.toStdString() + " — " + p.name.toStdString() + (p.quantity > 1 ? "  (" + std::to_string(p.quantity) + " pç)" : std::string()), 2.3, textLayer, TextAlign::Center, 0.0, true);
    std::vector<Point> outline;
    for (const auto &v : p.outline) {
        outline.push_back(P(v[0], v[1]));
    }
    s.polyline(outline, true, plateLayer);
    std::vector<double> as {0.0, p.width}, bs {0.0, p.height};
    for (const auto &h : p.holes) {
        drawBoltHole(s, P(h.a, h.b), h.diameter * k, holeLayer, centreLayer);
        as.push_back(h.a);
        bs.push_back(h.b);
    }
    const auto xs = distinct(as), ys = distinct(bs);
    for (std::size_t j = 0; j + 1 < xs.size(); ++j) {
        dimension(s, dimLayer, P(xs[j], 0.0), P(xs[j + 1], 0.0), -7.0, numTrim(xs[j + 1] - xs[j]), 1.9);
    }
    if (xs.size() > 2) {
        dimension(s, dimLayer, P(0.0, 0.0), P(p.width, 0.0), -13.0, numTrim(p.width), 1.9);
    }
    for (std::size_t j = 0; j + 1 < ys.size(); ++j) {
        dimension(s, dimLayer, P(0.0, ys[j]), P(0.0, ys[j + 1]), 7.0, numTrim(ys[j + 1] - ys[j]), 1.9);
    }
    if (ys.size() > 2) {
        dimension(s, dimLayer, P(0.0, 0.0), P(0.0, p.height), 13.0, numTrim(p.height), 1.9);
    }
    if (!p.rectangular) {
        for (const auto &v : p.outline) {
            s.text({P(v[0], v[1]).x + 1.5, P(v[0], v[1]).y + 1.5}, "(" + numTrim(v[0]) + "; " + numTrim(v[1]) + ")", 1.5, textLayer);
        }
    }
    if (!p.holes.empty()) {
        s.text({cx0 + cw / 2.0, cyTop - ch + 2.0}, std::to_string(p.holes.size()) + " furo(s) Ø" + numTrim(p.holes.front().diameter) + " — " + num(p.massKg, 2) + " kg/pç — esc. 1:" + num(scale, scale == std::round(scale) ? 0 : 1), 1.9, textLayer, TextAlign::Center);
    } else {
        s.text({cx0 + cw / 2.0, cyTop - ch + 2.0}, num(p.massKg, 2) + " kg/pç — esc. 1:" + num(scale, scale == std::round(scale) ? 0 : 1), 1.9, textLayer, TextAlign::Center);
    }
    return scale;
}

double drawFabricationTables(Sheet &s, const FabAssembly &fab, double x0, double yTop, double width, double maxHeight)
{
    const int lineLayer = s.addLayer("TABELA", {0, 0, 0}, 0.18);
    const int textLayer = s.addLayer("TEXTO", {0, 0, 0}, 0.18);
    double y = yTop;
    const double rowH = 4.6;
    auto fits = [&](std::size_t rows) { return (yTop - y) + rowH * static_cast<double>(rows) + 8.0 <= maxHeight; };
    {
        std::vector<std::vector<std::string>> rows {{"MARCA", "CHAPA", "QTD", "kg/pç", "kg"}};
        for (const auto &p : fab.parts) {
            if (p.kind == FabPart::Kind::Plate) {
                rows.push_back({p.mark.toStdString(), p.name.toStdString(), std::to_string(p.quantity), num(p.massKg, 2), num(p.massKg * p.quantity, 2)});
            }
        }
        if (rows.size() == 1) {
            rows.push_back({"—", "sem chapas avulsas", "", "", ""});
        }
        if (fits(rows.size())) {
            table(s, lineLayer, textLayer, x0, y, {width * 0.13, width * 0.45, width * 0.1, width * 0.16, width * 0.16}, rows, "CHAPAS");
            y -= rowH * static_cast<double>(rows.size()) + 8.0;
        }
    }
    {
        std::vector<std::vector<std::string>> rows {{"MARCA", "PARAFUSO", "L (mm)", "QTD"}};
        for (const auto &b : fab.bolts) {
            rows.push_back({b.mark.toStdString(), "d" + numTrim(b.diameter) + " " + b.grade.toStdString(), numTrim(b.length), std::to_string(b.quantity)});
        }
        if (rows.size() == 1) {
            rows.push_back({"—", "", "", ""});
        }
        if (fits(rows.size())) {
            table(s, lineLayer, textLayer, x0, y, {width * 0.15, width * 0.45, width * 0.2, width * 0.2}, rows, "PARAFUSOS (porca e 2 arruelas)");
            y -= rowH * static_cast<double>(rows.size()) + 8.0;
        }
    }
    {
        std::vector<std::vector<std::string>> rows {{"MARCA", "PERNA", "COMPR.", "QTD", "TOTAL"}};
        for (const auto &w : fab.welds) {
            rows.push_back({w.mark.toStdString(), numTrim(w.leg), numTrim(w.length), std::to_string(w.quantity), numTrim(w.totalLength)});
        }
        if (rows.size() == 1) {
            rows.push_back({"—", "", "", "", ""});
        }
        if (fits(rows.size())) {
            table(s, lineLayer, textLayer, x0, y, {width * 0.15, width * 0.2, width * 0.22, width * 0.18, width * 0.25}, rows, "SOLDAS DE FILETE (mm)");
            y -= rowH * static_cast<double>(rows.size()) + 8.0;
        }
    }
    s.text({x0, y + 1.0}, "Massa: chapas " + num(fab.plateMassKg, 1) + " kg + parafusos " + num(fab.boltMassKg, 1) + " kg", 2.0, textLayer);
    return yTop - y;
}

double drawFabricationSheet(Sheet &s, const SheetBuilder::ConnectionDetail &d, double rx0, double ry0, double rx1, double ry1)
{
    const int lineLayer = s.addLayer("TABELA", {0, 0, 0}, 0.18);
    const int textLayer = s.addLayer("TEXTO", {0, 0, 0}, 0.18);
    const int plateLayer = s.addLayer("CHAPAS", {20, 60, 140}, 0.35);
    const int holeLayer = s.addLayer("FUROS", {150, 60, 0}, 0.25);
    const int centreLayer = s.addLayer("EIXOS", {200, 40, 40}, 0.13, LineStyle::DashDot);
    const int dimLayer = s.addLayer("COTAS", {0, 120, 0}, 0.13);
    const int weldLayer = s.addLayer("SOLDA", {200, 0, 0}, 0.3);

    const FabAssembly fab = FabricationModel::fromConnection(d.connection);
    std::vector<const FabPart *> plates, members;
    for (const auto &p : fab.parts) {
        (p.kind == FabPart::Kind::Plate ? plates : members).push_back(&p);
    }

    // rodape de tabelas
    const std::size_t memberRows = [&]() {
        std::size_t n = 0;
        for (const auto *m : members) {
            n += std::min<std::size_t>(m->holes.size(), 6);
        }
        return n;
    }();
    const std::size_t tableRows = std::max<std::size_t>({static_cast<std::size_t>(fab.bolts.size()) + 1, static_cast<std::size_t>(fab.welds.size()) + 1, memberRows + 1, 3}) + 1;
    const double tableH = std::min(12.0 + 4.6 * static_cast<double>(tableRows), (ry1 - ry0) * 0.45);

    s.text({rx0 + 1.0, ry1 - 4.0}, "PEÇAS DA LIGAÇÃO — NÓ " + std::to_string(d.connection.nodeId) + ", VIGA " + std::to_string(d.connection.memberId), 3.4, textLayer, TextAlign::Left, 0.0, true);
    s.text({rx0 + 1.0, ry1 - 9.0},
           "Chapas: " + std::to_string(fab.plateCount()) + " peça(s), " + num(fab.plateMassKg, 1) + " kg — parafusos: " + num(fab.boltMassKg, 1) + " kg — furos padrão (NBR 8800, Tabela 14)", 2.3, textLayer);

    // ---- chapas
    const double areaTop = ry1 - 14.0, areaBottom = ry0 + tableH + 6.0;
    double lastScale = 1.0;
    if (!plates.empty()) {
        const int n = static_cast<int>(plates.size());
        const int columns = n <= 1 ? 1 : (n <= 4 ? 2 : 3);
        const int rowsCount = (n + columns - 1) / columns;
        const double cw = (rx1 - rx0) / columns, ch = (areaTop - areaBottom) / rowsCount;
        for (int i = 0; i < n; ++i) {
            const FabPart &p = *plates[static_cast<std::size_t>(i)];
            const int col = i % columns, row = i / columns;
            const double cx0 = rx0 + col * cw, cyTop = areaTop - row * ch;
            const double marginX = 32.0, marginTop = 24.0, marginBottom = 26.0;
            const double scale = pickPlateScale(p.width, p.height, cw - 2.0 * marginX, ch - marginTop - marginBottom);
            lastScale = scale;
            const double k = 1.0 / scale;
            const double ox = cx0 + marginX + std::max(0.0, (cw - 2.0 * marginX - p.width * k) / 2.0);
            const double oy = cyTop - marginTop - p.height * k - std::max(0.0, (ch - marginTop - marginBottom - p.height * k) / 2.0);
            auto P = [&](double a, double b) { return Point {ox + a * k, oy + b * k}; };
            s.rect({cx0 + 1.0, cyTop - ch + 1.0}, {cx0 + cw - 1.0, cyTop - 1.0}, lineLayer);
            // legenda da peca
            s.text({cx0 + 4.0, cyTop - 6.0}, p.mark.toStdString() + " — " + p.name.toStdString(), 3.0, textLayer, TextAlign::Left, 0.0, true);
            std::string info = std::to_string(p.quantity) + " PEÇA(S) — " + num(p.massKg, 2) + " kg/pç";
            if (!p.material.isEmpty()) {
                info += " — " + p.material.toStdString();
            }
            s.text({cx0 + 4.0, cyTop - 11.0}, info, 2.2, textLayer);
            s.text({cx0 + 4.0, cyTop - 15.5}, "ESC. 1:" + num(scale, scale == std::round(scale) ? 0 : 1) + " — " + std::to_string(p.holes.size()) + " furo(s)", 2.0, textLayer);
            // contorno
            std::vector<Point> outline;
            for (const auto &v : p.outline) {
                outline.push_back(P(v[0], v[1]));
            }
            s.polyline(outline, true, plateLayer);
            // furos, eixos e cotas encadeadas
            std::vector<double> as {0.0, p.width}, bs {0.0, p.height};
            for (const auto &h : p.holes) {
                drawBoltHole(s, P(h.a, h.b), h.diameter * k, holeLayer, centreLayer);
                as.push_back(h.a);
                bs.push_back(h.b);
            }
            const auto xs = distinct(as), ys = distinct(bs);
            for (std::size_t j = 0; j + 1 < xs.size(); ++j) {
                dimension(s, dimLayer, P(xs[j], 0.0), P(xs[j + 1], 0.0), -9.0, numTrim(xs[j + 1] - xs[j]));
            }
            if (xs.size() > 2) {
                dimension(s, dimLayer, P(0.0, 0.0), P(p.width, 0.0), -17.0, numTrim(p.width));
            }
            for (std::size_t j = 0; j + 1 < ys.size(); ++j) {
                dimension(s, dimLayer, P(0.0, ys[j]), P(0.0, ys[j + 1]), 9.0, numTrim(ys[j + 1] - ys[j]));
            }
            if (ys.size() > 2) {
                dimension(s, dimLayer, P(0.0, 0.0), P(0.0, p.height), 17.0, numTrim(p.height));
            }
            // contorno poligonal: cota dos vertices em relacao ao canto
            if (!p.rectangular) {
                for (const auto &v : p.outline) {
                    s.text({P(v[0], v[1]).x + 1.5, P(v[0], v[1]).y + 1.5}, "(" + numTrim(v[0]) + "; " + numTrim(v[1]) + ")", 1.6, textLayer);
                }
            }
            if (!p.holes.empty()) {
                const auto &h = p.holes.front();
                s.text({P(h.a, h.b).x + 4.0, P(h.a, h.b).y + 3.0}, "Ø" + numTrim(h.diameter), 2.0, textLayer);
            }
        }
    } else {
        s.text({rx0 + 6.0, (areaTop + areaBottom) / 2.0}, "A ligação não tem chapas avulsas (só soldas e furação nos perfis).", 2.6, textLayer);
    }

    // ---- tabelas
    const double ty = ry0 + tableH;
    const double third = (rx1 - rx0 - 8.0) / 3.0;
    {
        std::vector<std::vector<std::string>> rows {{"MARCA", "Ø", "CLASSE", "L (mm)", "QTD"}};
        for (const auto &b : fab.bolts) {
            rows.push_back({b.mark.toStdString(), numTrim(b.diameter), b.grade.toStdString(), numTrim(b.length), std::to_string(b.quantity)});
        }
        if (fab.bolts.empty()) {
            rows.push_back({"—", "", "", "", ""});
        }
        table(s, lineLayer, textLayer, rx0 + 2.0, ty, {third * 0.2, third * 0.15, third * 0.3, third * 0.2, third * 0.15}, rows, "PARAFUSOS (porca e 2 arruelas por parafuso)");
    }
    {
        std::vector<std::vector<std::string>> rows {{"MARCA", "PERNA", "COMPR.", "QTD", "TOTAL"}};
        for (const auto &w : fab.welds) {
            rows.push_back({w.mark.toStdString(), numTrim(w.leg), numTrim(w.length), std::to_string(w.quantity), numTrim(w.totalLength)});
        }
        if (fab.welds.empty()) {
            rows.push_back({"—", "", "", "", ""});
        }
        table(s, lineLayer, textLayer, rx0 + 4.0 + third, ty, {third * 0.2, third * 0.2, third * 0.2, third * 0.15, third * 0.25}, rows, "SOLDAS DE FILETE (mm)");
    }
    {
        std::vector<std::vector<std::string>> rows {{"PERFIL", "A", "B", "Ø"}};
        for (const auto *m : members) {
            std::size_t shown = 0;
            for (const auto &h : m->holes) {
                if (++shown > 6) {
                    rows.push_back({"...", "", "", ""});
                    break;
                }
                rows.push_back({numTrim(m->thickness) + "mm", numTrim(h.a), numTrim(h.b), numTrim(h.diameter)});
            }
        }
        if (rows.size() == 1) {
            rows.push_back({"—", "", "", ""});
        }
        table(s, lineLayer, textLayer, rx0 + 6.0 + 2.0 * third, ty, {third * 0.34, third * 0.22, third * 0.22, third * 0.22}, rows, "FURAÇÃO NOS PERFIS LIGADOS (a, b = mm da peça)");
    }

    // simbolo de solda de exemplo: perna e comprimento da primeira solda, junto a legenda
    if (!fab.welds.empty()) {
        const FabWeld &w = fab.welds.front();
        WeldSymbol symbol;
        symbol.tail = {rx1 - 60.0, areaBottom + 12.0};
        symbol.arrowTip = {symbol.tail.x - 14.0, symbol.tail.y + 8.0};
        symbol.size = w.leg;
        symbol.length = w.length;
        symbol.bothSides = true;
        symbol.field = false;
        s.text({symbol.arrowTip.x - 16.0, symbol.tail.y + 12.0}, "SÍMBOLO DE SOLDA (FILETE DOS DOIS LADOS)", 1.8, textLayer);
        drawWeldSymbol(s, symbol, weldLayer, textLayer);
    }
    return lastScale;
}

void drawConnectionBomSheet(Sheet &s, const std::vector<SheetBuilder::ConnectionDetail> &details, double rx0, double ry0, double rx1, double ry1)
{
    (void)ry0;
    const int lineLayer = s.addLayer("TABELA", {0, 0, 0}, 0.18);
    const int textLayer = s.addLayer("TEXTO", {0, 0, 0}, 0.18);
    QVector<Structura::App::ProjectData::NodeConnection> connections;
    for (const auto &d : details) {
        connections.push_back(d.connection);
    }
    const FabBom bom = FabricationModel::bom(connections);
    s.text({rx0 + 1.0, ry1 - 4.0}, "LISTA DE PEÇAS DAS LIGAÇÕES", 3.6, textLayer, TextAlign::Left, 0.0, true);
    s.text({rx0 + 1.0, ry1 - 9.5},
           std::to_string(bom.connectionCount) + " ligação(ões) — chapas " + num(bom.plateMassKg, 1) + " kg, parafusos " + num(bom.boltMassKg, 1) + " kg (massas líquidas, sem perdas de corte)", 2.3, textLayer);
    const double width = rx1 - rx0 - 4.0;
    double y = ry1 - 20.0;
    {
        std::vector<std::vector<std::string>> rows {{"ITEM", "PEÇA", "AÇO", "FUROS", "QTD", "kg/pç", "TOTAL kg", "LIGAÇÕES"}};
        int item = 0;
        for (const auto &p : bom.plates) {
            std::string ids;
            for (const int id : p.connections) {
                ids += (ids.empty() ? "" : ",") + std::to_string(id);
            }
            rows.push_back({"C" + std::to_string(++item), p.name.toStdString(), p.material.toStdString(), std::to_string(p.holes), std::to_string(p.quantity), num(p.unitMassKg, 2),
                            num(p.totalMassKg, 1), ids});
        }
        if (bom.plates.isEmpty()) {
            rows.push_back({"—", "", "", "", "", "", "", ""});
        }
        table(s, lineLayer, textLayer, rx0 + 2.0, y, {width * 0.07, width * 0.28, width * 0.13, width * 0.07, width * 0.07, width * 0.1, width * 0.1, width * 0.18}, rows, "CHAPAS");
        y -= 4.6 * static_cast<double>(rows.size()) + 12.0;
    }
    {
        std::vector<std::vector<std::string>> rows {{"Ø (mm)", "COMPRIMENTO (mm)", "CLASSE", "QTD", "TOTAL kg"}};
        for (const auto &b : bom.bolts) {
            rows.push_back({numTrim(b.diameter), numTrim(b.length), b.grade.toStdString(), std::to_string(b.quantity), num(b.totalMassKg, 1)});
        }
        if (bom.bolts.isEmpty()) {
            rows.push_back({"—", "", "", "", ""});
        }
        table(s, lineLayer, textLayer, rx0 + 2.0, y, {width * 0.12, width * 0.22, width * 0.26, width * 0.12, width * 0.14}, rows, "PARAFUSOS (porca e 2 arruelas incluídas)");
        y -= 4.6 * static_cast<double>(rows.size()) + 12.0;
    }
    {
        std::vector<std::vector<std::string>> rows {{"PERNA (mm)", "CORDÕES", "COMPR. TOTAL (m)"}};
        for (const auto &w : bom.welds) {
            rows.push_back({numTrim(w.leg), std::to_string(w.beads), num(w.totalLength / 1000.0, 2)});
        }
        if (bom.welds.isEmpty()) {
            rows.push_back({"—", "", ""});
        }
        table(s, lineLayer, textLayer, rx0 + 2.0, y, {width * 0.16, width * 0.16, width * 0.24}, rows, "SOLDAS DE FILETE");
    }
}

void drawCutListSheet(Sheet &s, const SheetBuilder::Input &input, const SheetBuilder::Options &options, double rx0, double ry0, double rx1, double ry1)
{
    (void)ry0;
    const int lineLayer = s.addLayer("TABELA", {0, 0, 0}, 0.18);
    const int textLayer = s.addLayer("TEXTO", {0, 0, 0}, 0.18);
    const auto members = MemberFabrication::build(input);
    QVector<Structura::App::ProjectData::NodeConnection> connections;
    for (const auto &d : input.connections) {
        connections.push_back(d.connection);
    }
    CutListService::Options cut;
    cut.stockLength = options.stockLengthMm;
    cut.sheetLength = options.plateSheetLengthMm;
    cut.sheetWidth = options.plateSheetWidthMm;
    const auto plan = CutListService::compute(members, FabricationModel::bom(connections), cut);
    s.text({rx0 + 1.0, ry1 - 4.0}, "PLANO DE CORTE", 3.6, textLayer, TextAlign::Left, 0.0, true);
    s.text({rx0 + 1.0, ry1 - 9.5},
           "Barras de " + num(cut.stockLength, 0) + " mm, serra " + num(cut.kerf, 0) + " mm — chapas de " + num(cut.sheetLength, 0) + " x " + num(cut.sheetWidth, 0) +
               " mm — heurística de primeiro encaixe (não é ótimo): confira o aproveitamento antes de comprar.",
           2.2, textLayer);
    const double width = rx1 - rx0 - 4.0;
    double y = ry1 - 20.0;
    {
        std::vector<std::vector<std::string>> rows {{"PERFIL", "BARRAS", "USO (mm)", "COMPRADO (mm)", "PERDA (mm)", "APROV.", "kg COMPRADOS"}};
        for (const auto &b : plan.bars) {
            rows.push_back({b.section.toStdString(), std::to_string(b.barCount), num(b.usedLength, 0), num(b.stockLengthTotal, 0), num(b.wasteLength, 0),
                            num(100.0 * b.utilization(), 1) + "%", num(b.stockLengthTotal / 1000.0 * b.massPerMeter, 1)});
        }
        if (plan.bars.isEmpty()) {
            rows.push_back({"—", "", "", "", "", "", ""});
        }
        table(s, lineLayer, textLayer, rx0 + 2.0, y, {width * 0.24, width * 0.1, width * 0.14, width * 0.16, width * 0.14, width * 0.1, width * 0.12}, rows, "PERFIS");
        y -= 4.6 * static_cast<double>(rows.size()) + 10.0;
    }
    {
        std::vector<std::vector<std::string>> rows {{"BARRA", "PERFIL", "PADRÃO DE CORTE (marca: mm)"}};
        int shown = 0;
        for (const auto &b : plan.bars) {
            int index = 0;
            for (const auto &pattern : b.patterns) {
                ++index;
                if (++shown > 24) {
                    break;
                }
                std::string text;
                for (const auto &piece : pattern) {
                    text += (text.empty() ? "" : " | ") + piece.first.toStdString() + ": " + num(piece.second, 0);
                }
                rows.push_back({std::to_string(index), b.section.toStdString(), text});
            }
        }
        if (shown > 24) {
            rows.push_back({"...", "", "padrões omitidos (limite de 24 linhas na prancha)"});
        }
        table(s, lineLayer, textLayer, rx0 + 2.0, y, {width * 0.07, width * 0.2, width * 0.73}, rows, "PADRÕES DE CORTE DOS PERFIS");
        y -= 4.6 * static_cast<double>(rows.size()) + 10.0;
    }
    {
        std::vector<std::vector<std::string>> rows {{"ESPESSURA", "PEÇAS", "CHAPAS", "ÁREA ÚTIL (m2)", "ÁREA COMPRADA (m2)", "APROV."}};
        for (const auto &sh : plan.sheets) {
            rows.push_back({numTrim(sh.thickness) + " mm", std::to_string(sh.pieceCount), std::to_string(sh.sheetCount), num(sh.usedArea / 1e6, 3), num(sh.sheetArea / 1e6, 3),
                            num(100.0 * sh.utilization(), 1) + "%"});
        }
        if (plan.sheets.isEmpty()) {
            rows.push_back({"—", "", "", "", "", ""});
        }
        table(s, lineLayer, textLayer, rx0 + 2.0, y, {width * 0.16, width * 0.12, width * 0.12, width * 0.2, width * 0.24, width * 0.16}, rows, "CHAPAS (peças das ligações)");
        y -= 4.6 * static_cast<double>(rows.size()) + 10.0;
    }
    for (const auto &w : plan.warnings) {
        s.text({rx0 + 2.0, y}, "AVISO: " + clip(w.toStdString(), 150), 2.1, textLayer);
        y -= 4.5;
    }
}

void drawMemberFabricationSheet(Sheet &s, const FabMember &m, double rx0, double ry0, double rx1, double ry1)
{
    const int lineLayer = s.addLayer("TABELA", {0, 0, 0}, 0.18);
    const int textLayer = s.addLayer("TEXTO", {0, 0, 0}, 0.18);
    const int outLayer = s.addLayer("PERFIS", {0, 0, 0}, 0.35);
    const int holeLayer = s.addLayer("FUROS", {150, 60, 0}, 0.25);
    const int centreLayer = s.addLayer("EIXOS", {200, 40, 40}, 0.13, LineStyle::DashDot);
    const int dimLayer = s.addLayer("COTAS", {0, 120, 0}, 0.13);
    const int hiddenLayer = s.addLayer("OCULTO", {120, 120, 120}, 0.13, LineStyle::Dashed);

    const double depth = m.profile.depthMm > 1.0 ? m.profile.depthMm : 200.0;
    const double flange = m.profile.flangeMm > 0.0 ? m.profile.flangeMm : 0.0;
    s.text({rx0 + 1.0, ry1 - 4.0}, "PEÇA " + m.mark.toStdString() + " — " + m.section.toStdString(), 3.8, textLayer, TextAlign::Left, 0.0, true);
    std::string bars;
    for (const int id : m.bars) {
        bars += (bars.empty() ? "" : ", ") + std::to_string(id);
    }
    s.text({rx0 + 1.0, ry1 - 9.5},
           std::to_string(m.quantity) + " peça(s) — comprimento de corte " + num(m.cutLength, 0) + " mm (entre nós " + num(m.nodeLength, 0) + ", entre faces " + num(m.faceLength, 0) + ") — " +
               num(m.assemblyMassKg, 1) + " kg/pç (perfil " + num(m.unitMassKg, 1) + " + chapas soldadas ao perfil), " + num(m.assemblyMassKg * m.quantity, 1) + " kg no total",
           2.3, textLayer);
    s.text({rx0 + 1.0, ry1 - 14.0}, "Barras: " + clip(bars, 120), 2.2, textLayer);

    // elevacao pela alma
    const double areaTop = ry1 - 22.0;
    const double tableH = 60.0;
    const double areaBottom = ry0 + tableH + 12.0;
    const double availW = rx1 - rx0 - 70.0, availH = areaTop - areaBottom - 40.0;
    static const double scales[] = {5, 8, 10, 12.5, 15, 20, 25, 30, 40, 50, 75, 100, 150, 200};
    double scale = 200.0;
    for (const double sc : scales) {
        if (m.cutLength / sc <= availW && depth / sc <= availH) {
            scale = sc;
            break;
        }
    }
    const double k = 1.0 / scale;
    const double ox = rx0 + 35.0, oy = areaTop - 20.0 - depth * k / 2.0;
    auto P = [&](double x, double y) { return Point {ox + x * k, oy + y * k}; };
    const double L = m.cutLength;
    {
        // contorno com os recortes das mesas (cope) nas quatro quinas
        double cl[2][2] = {{0, 0}, {0, 0}}, cd[2][2] = {{0, 0}, {0, 0}};   // [extremidade I/J][0 = inferior, 1 = superior]
        for (const auto &c : m.copes) {
            const int e = c.end == 'I' ? 0 : 1, t = c.top ? 1 : 0;
            cl[e][t] = std::max(cl[e][t], c.length);
            cd[e][t] = std::max(cd[e][t], c.depth);
        }
        const double H = depth / 2.0;
        std::vector<Point> outline;
        if (cl[0][0] > 0.0) {
            outline.push_back(P(0.0, -H + cd[0][0]));
            outline.push_back(P(cl[0][0], -H + cd[0][0]));
            outline.push_back(P(cl[0][0], -H));
        } else {
            outline.push_back(P(0.0, -H));
        }
        if (cl[1][0] > 0.0) {
            outline.push_back(P(L - cl[1][0], -H));
            outline.push_back(P(L - cl[1][0], -H + cd[1][0]));
            outline.push_back(P(L, -H + cd[1][0]));
        } else {
            outline.push_back(P(L, -H));
        }
        if (cl[1][1] > 0.0) {
            outline.push_back(P(L, H - cd[1][1]));
            outline.push_back(P(L - cl[1][1], H - cd[1][1]));
            outline.push_back(P(L - cl[1][1], H));
        } else {
            outline.push_back(P(L, H));
        }
        if (cl[0][1] > 0.0) {
            outline.push_back(P(cl[0][1], H));
            outline.push_back(P(cl[0][1], H - cd[0][1]));
            outline.push_back(P(0.0, H - cd[0][1]));
        } else {
            outline.push_back(P(0.0, H));
        }
        // extremidades com corte inclinado: o eixo mantem o comprimento de corte e as quinas se deslocam +- (h/2) tan(a) (cortes em espelho nas duas pontas)
        const double x0Paper = P(0.0, 0.0).x, xLPaper = P(L, 0.0).x, yMid = P(0.0, 0.0).y;
        const double shiftI = std::tan(m.endAngleI * 3.14159265358979323846 / 180.0) * H * k;
        const double shiftJ = std::tan(m.endAngleJ * 3.14159265358979323846 / 180.0) * H * k;
        for (auto &pt : outline) {
            if (std::abs(pt.x - x0Paper) < 1e-6 && shiftI > 1e-9) {
                pt.x += pt.y > yMid ? -shiftI : shiftI;
            } else if (std::abs(pt.x - xLPaper) < 1e-6 && shiftJ > 1e-9) {
                pt.x += pt.y > yMid ? shiftJ : -shiftJ;
            }
        }
        s.polyline(outline, true, outLayer);
        for (const auto &c : m.copes) {
            const double x0 = c.end == 'I' ? 0.0 : L - c.length;
            const double yy = c.top ? depth / 2.0 : -depth / 2.0;
            dimension(s, dimLayer, P(x0, yy), P(x0 + c.length, yy), c.top ? 6.0 : -6.0, numTrim(c.length));
            s.text({P(c.end == 'I' ? c.length : L - c.length, yy).x + (c.end == 'I' ? 2.0 : -2.0), P(0.0, yy).y + (c.top ? -3.5 : 2.0)},
                   "RECORTE " + numTrim(c.length) + " x " + numTrim(c.depth), 1.8, textLayer, c.end == 'I' ? TextAlign::Left : TextAlign::Right);
        }
    }
    if (m.endAngleI > 0.01 || m.endAngleJ > 0.01) {
        s.text({rx0 + 1.0, ry1 - 18.0}, "CORTE INCLINADO NA EXTREMIDADE: I " + numTrim(m.endAngleI) + "°, J " + numTrim(m.endAngleJ) + "° EM RELAÇÃO À NORMAL DO EIXO", 2.2, textLayer);
    }
    if (flange > 0.0) {
        s.line(P(0.0, depth / 2.0 - flange), P(L, depth / 2.0 - flange), outLayer);
        s.line(P(0.0, -depth / 2.0 + flange), P(L, -depth / 2.0 + flange), outLayer);
    }
    s.line({P(0.0, 0.0).x - 6.0, P(0.0, 0.0).y}, {P(L, 0.0).x + 6.0, P(0.0, 0.0).y}, centreLayer);
    s.text({P(0.0, depth / 2.0).x, P(0.0, depth / 2.0).y + 3.0}, "EXT. I" + (m.gapI > 0 ? " (afastada " + numTrim(m.gapI) + " mm da face do apoio)" : std::string()), 2.0, textLayer);
    s.text({P(L, depth / 2.0).x, P(L, depth / 2.0).y + 3.0}, "EXT. J" + (m.gapJ > 0 ? " (afastada " + numTrim(m.gapJ) + " mm da face do apoio)" : std::string()), 2.0, textLayer, TextAlign::Right);

    // furos da alma e das mesas (as mesas aparecem tracejadas sobre a alma)
    std::vector<double> xs {0.0, L};
    for (const auto &h : m.holes) {
        const double x = h.end == 'I' ? h.x : L - h.x;
        const bool web = h.face == QStringLiteral("alma");
        const double y = web ? h.y : (h.y >= 0 ? depth / 2.0 - flange / 2.0 : -depth / 2.0 + flange / 2.0);
        if (web) {
            drawBoltHole(s, P(x, y), h.diameter * k, holeLayer, centreLayer);
        } else {
            s.circle(P(x, y), std::max(0.4, h.diameter * k / 2.0), hiddenLayer);
        }
        xs.push_back(x);
    }
    const auto xd = distinct(xs);
    for (std::size_t j = 0; j + 1 < xd.size(); ++j) {
        if ((xd[j + 1] - xd[j]) * k > 4.0 || xd.size() <= 6) {
            dimension(s, dimLayer, P(xd[j], -depth / 2.0), P(xd[j + 1], -depth / 2.0), -7.0, numTrim(xd[j + 1] - xd[j]));
        }
    }
    dimension(s, dimLayer, P(0.0, -depth / 2.0), P(L, -depth / 2.0), -15.0, numTrim(L));
    dimension(s, dimLayer, P(0.0, -depth / 2.0), P(0.0, depth / 2.0), 7.0, numTrim(depth));
    s.text({rx0 + 1.0, areaTop - 4.0}, "ELEVAÇÃO PELA ALMA — ESC. 1:" + num(scale, scale == std::round(scale) ? 0 : 1), 2.6, textLayer, TextAlign::Left, 0.0, true);

    // secao transversal
    {
        const double bf = m.profile.widthMm > 1.0 ? m.profile.widthMm : depth * 0.5;
        const double tw = m.profile.webMm > 0.0 ? m.profile.webMm : 6.0;
        const double secScale = std::max(scale, 5.0);
        const double kk = 1.0 / secScale;
        const double cx = rx1 - 40.0, cy = areaBottom + 24.0 + depth * kk / 2.0;
        auto Q = [&](double a, double b) { return Point {cx + a * kk, cy + b * kk}; };
        if (m.profile.iShape && flange > 0.0) {
            s.polyline({Q(-bf / 2, depth / 2), Q(bf / 2, depth / 2), Q(bf / 2, depth / 2 - flange), Q(tw / 2, depth / 2 - flange), Q(tw / 2, -depth / 2 + flange), Q(bf / 2, -depth / 2 + flange),
                        Q(bf / 2, -depth / 2), Q(-bf / 2, -depth / 2), Q(-bf / 2, -depth / 2 + flange), Q(-tw / 2, -depth / 2 + flange), Q(-tw / 2, depth / 2 - flange),
                        Q(-bf / 2, depth / 2 - flange)},
                       true, outLayer);
        } else {
            s.rect(Q(-bf / 2, -depth / 2), Q(bf / 2, depth / 2), outLayer);
        }
        dimension(s, dimLayer, Q(-bf / 2, -depth / 2), Q(bf / 2, -depth / 2), -6.0, numTrim(bf));
        s.text({cx, cy + depth * kk / 2.0 + 5.0}, "SEÇÃO — ESC. 1:" + num(secScale, 0), 2.0, textLayer, TextAlign::Center, 0.0, true);
    }

    // tabelas: furacao e chapas das ligacoes
    const double ty = ry0 + tableH;
    const double w = (rx1 - rx0 - 8.0) / 2.0;
    {
        std::vector<std::vector<std::string>> rows {{"EXT.", "FACE", "x (mm)", "y (mm)", "Ø"}};
        std::size_t shown = 0;
        for (const auto &h : m.holes) {
            if (++shown > 9) {
                rows.push_back({"...", "", "", "", ""});
                break;
            }
            rows.push_back({std::string(1, h.end), h.face.toStdString(), numTrim(h.x), numTrim(h.y), numTrim(h.diameter)});
        }
        if (m.holes.isEmpty()) {
            rows.push_back({"—", "sem furação", "", "", ""});
        }
        table(s, lineLayer, textLayer, rx0 + 2.0, ty, {w * 0.12, w * 0.2, w * 0.24, w * 0.24, w * 0.2}, rows, "FURAÇÃO (x da extremidade; y da linha de centro)");
    }
    {
        std::vector<std::vector<std::string>> rows {{"LIGAÇÃO", "EXT.", "CHAPA", "QTD"}};
        std::size_t shown = 0;
        for (const auto &a : m.attached) {
            if (++shown > 9) {
                rows.push_back({"...", "", "", ""});
                break;
            }
            rows.push_back({std::to_string(a.connectionId), std::string(1, a.end), std::string(a.welded ? "[P] " : "[A] ") + a.plateMark.toStdString() + " " + a.name.toStdString(), std::to_string(a.quantity)});
        }
        if (m.attached.isEmpty()) {
            rows.push_back({"—", "", "sem chapas de ligação", ""});
        }
        table(s, lineLayer, textLayer, rx0 + 6.0 + w, ty, {w * 0.16, w * 0.1, w * 0.58, w * 0.16}, rows, "CHAPAS ([P] soldada ao perfil, [A] ao apoio)");
    }
}

void drawAssemblyScheduleSheet(Sheet &s, const SheetBuilder::Input &input, double rx0, double ry0, double rx1, double ry1)
{
    (void)ry0;
    const int lineLayer = s.addLayer("TABELA", {0, 0, 0}, 0.18);
    const int textLayer = s.addLayer("TEXTO", {0, 0, 0}, 0.18);
    const auto members = MemberFabrication::build(input);
    double total = 0.0;
    for (const auto &m : members) {
        total += m.assemblyMassKg * m.quantity;
    }
    s.text({rx0 + 1.0, ry1 - 4.0}, "QUADRO DE CONJUNTOS (PEÇAS DE PERFIL MARCADAS)", 3.6, textLayer, TextAlign::Left, 0.0, true);
    s.text({rx0 + 1.0, ry1 - 9.5}, std::to_string(members.size()) + " marca(s) — massa total de montagem " + num(total, 1) + " kg (perfis + chapas soldadas ao perfil; chapas soldadas ao apoio e parafusos estão na lista de peças das ligações)", 2.2, textLayer);
    const double width = rx1 - rx0 - 4.0;
    std::vector<std::vector<std::string>> rows {{"MARCA", "PERFIL", "QTD", "CORTE (mm)", "FACES (mm)", "FUROS", "RECORTES", "CHAPAS NO PERFIL", "kg/PEÇA", "kg TOTAL"}};
    std::size_t shown = 0;
    for (const auto &m : members) {
        if (++shown > 34) {
            rows.push_back({"...", "demais marcas omitidas (limite da prancha)", "", "", "", "", "", "", "", ""});
            break;
        }
        std::string plates;
        for (const auto &a : m.attached) {
            if (a.welded) {
                plates += (plates.empty() ? "" : "; ") + a.plateMark.toStdString() + (a.quantity > 1 ? " x" + std::to_string(a.quantity) : std::string());
            }
        }
        rows.push_back({m.mark.toStdString(), m.section.toStdString(), std::to_string(m.quantity), num(m.cutLength, 0), num(m.faceLength, 0), std::to_string(m.holes.size()),
                        std::to_string(m.copes.size()), plates.empty() ? "-" : plates, num(m.assemblyMassKg, 1), num(m.assemblyMassKg * m.quantity, 1)});
    }
    if (members.isEmpty()) {
        rows.push_back({"—", "", "", "", "", "", "", "", "", ""});
    }
    table(s, lineLayer, textLayer, rx0 + 2.0, ry1 - 20.0,
          {width * 0.07, width * 0.2, width * 0.05, width * 0.1, width * 0.1, width * 0.06, width * 0.08, width * 0.18, width * 0.08, width * 0.08}, rows, "MARCAS");
}

} // namespace Structura::App::Services::internal
