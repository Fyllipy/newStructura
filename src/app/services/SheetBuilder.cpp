#include "SheetBuilder.h"

#include "app/services/SheetBuilderInternal.h"

#include <QMap>

#include "drawing/HiddenLine.h"
#include "viz/structure/SectionShapes.h"

#include <memory>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace Structura::App::Services {

using namespace Structura::Drawing;

namespace {

constexpr double kTol = 1e-3;   // m
constexpr double kPi = 3.14159265358979323846;

struct Region {
    double x0, y0, x1, y1;   // mm do papel
    double width() const { return x1 - x0; }
    double height() const { return y1 - y0; }
};

struct Layers {
    int frame {0}, text {0}, textSmall {0}, column {0}, beam {0}, brace {0}, tie {0}, truss {0}, axis {0}, dim {0}, hinge {0}, table {0}, plan {0}, centre {0}, flange {0}, face {0}, hidden {0};
};

Layers makeLayers(Sheet &s)
{
    Layers l;
    l.frame = s.addLayer("MOLDURA", {0, 0, 0}, 0.5);
    l.text = s.addLayer("TEXTO", {0, 0, 0}, 0.18);
    l.textSmall = s.addLayer("ROTULOS", {40, 40, 40}, 0.13);
    l.column = s.addLayer("COLUNAS", {20, 60, 140}, 0.18);
    l.beam = s.addLayer("VIGAS", {0, 0, 0}, 0.18);
    l.brace = s.addLayer("DIAGONAIS", {30, 130, 60}, 0.18);
    l.truss = s.addLayer("TRELICAS", {142, 68, 173}, 0.18);
    l.tie = s.addLayer("TIRANTES", {230, 126, 34}, 0.18, LineStyle::Dashed);
    l.axis = s.addLayer("EIXOS", {200, 40, 40}, 0.13, LineStyle::DashDot);
    l.dim = s.addLayer("COTAS", {0, 120, 0}, 0.13);
    l.face = s.addLayer("COTAS_FACES", {0, 90, 160}, 0.13);
    l.hinge = s.addLayer("ROTULAS", {0, 0, 0}, 0.18);
    l.table = s.addLayer("TABELAS", {0, 0, 0}, 0.18);
    l.plan = s.addLayer("PLANTA_PILARES", {20, 60, 140}, 0.18);
    l.centre = s.addLayer("EIXO_BARRA", {150, 150, 150}, 0.09, LineStyle::DashDot);
    l.flange = s.addLayer("DETALHES_PERFIL", {90, 90, 90}, 0.09);
    l.hidden = s.addLayer("OCULTO", {140, 140, 140}, 0.09, LineStyle::Dashed);
    return l;
}

std::string num(double v, int decimals = 0)
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

std::string levelText(double z)
{
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%+.2f", z);
    std::string s = buffer;
    for (char &c : s) {
        if (c == '.') {
            c = ',';
        }
    }
    return s;
}

struct V3 {
    double x {0}, y {0}, z {0};
};
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator*(V3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double norm(V3 a) { return std::sqrt(dot(a, a)); }
V3 unit(V3 a)
{
    const double n = norm(a);
    return n > 1e-12 ? a * (1.0 / n) : V3 {1, 0, 0};
}

struct Axes {
    V3 x, y, z;
};

/// Eixos locais da barra, como no solver: K = Z global (Y se a barra for vertical), z = x cross K, y = z cross x; giro em torno de x.
Axes localAxes(V3 a, V3 b, double rotationDeg)
{
    const V3 x = unit(b - a);
    V3 k {0, 0, 1};
    if (norm(cross(x, k)) < 1e-6) {
        k = {0, 1, 0};
    }
    V3 z = unit(cross(x, k));
    V3 y = unit(cross(z, x));
    if (std::abs(rotationDeg) > 1e-9) {
        const double c = std::cos(rotationDeg * kPi / 180.0), s = std::sin(rotationDeg * kPi / 180.0);
        const V3 y0 = y, z0 = z;
        y = y0 * c + z0 * s;
        z = y0 * (-s) + z0 * c;
    }
    return {x, y, z};
}

struct Outline {
    std::vector<std::vector<std::array<double, 2>>> loops;   ///< mm: a = largura (eixo z local), b = altura (eixo y local)
};

/// Contorno da secao (parametrico) de uma barra; nullptr sem geometria.
const Outline *outlineOf(const SheetBuilder::Bar &bar)
{
    static std::map<QString, std::shared_ptr<Outline>> cache;
    if (bar.family.isEmpty()) {
        return nullptr;
    }
    QString key = bar.family + (bar.mirrored ? QStringLiteral("|m") : QStringLiteral("|"));
    for (auto it = bar.geometry.constBegin(); it != bar.geometry.constEnd(); ++it) {
        key += QStringLiteral("|%1=%2").arg(it.key()).arg(it.value(), 0, 'g', 8);
    }
    const auto found = cache.find(key);
    if (found != cache.end()) {
        return found->second.get();
    }
    std::shared_ptr<Outline> outline;
    if (const auto shape = Structura::Visualization::SectionShapes::build(bar.family, bar.geometry, bar.mirrored)) {
        outline = std::make_shared<Outline>();
        for (const auto &loop : shape->loops) {
            outline->loops.emplace_back();
            for (const auto &p : loop) {
                outline->loops.back().push_back({p[0], p[1]});
            }
        }
    }
    cache[key] = outline;
    return outline.get();
}

/// Vertices de dobra / arestas do contorno: cantos vivos e as duas extremidades (tangentes) de cada dobra arredondada.
std::vector<std::array<double, 2>> foldVertices(const std::vector<std::array<double, 2>> &loop)
{
    std::vector<std::array<double, 2>> out;
    const int n = static_cast<int>(loop.size());
    if (n < 3) {
        return out;
    }
    std::vector<double> turn(static_cast<std::size_t>(n), 0.0);
    for (int i = 0; i < n; ++i) {
        const auto &p0 = loop[static_cast<std::size_t>((i + n - 1) % n)];
        const auto &p1 = loop[static_cast<std::size_t>(i)];
        const auto &p2 = loop[static_cast<std::size_t>((i + 1) % n)];
        const double ax = p1[0] - p0[0], ay = p1[1] - p0[1], bx = p2[0] - p1[0], by = p2[1] - p1[1];
        const double la = std::hypot(ax, ay), lb = std::hypot(bx, by);
        if (la < 1e-9 || lb < 1e-9) {
            continue;
        }
        const double c = std::clamp((ax * bx + ay * by) / (la * lb), -1.0, 1.0);
        turn[static_cast<std::size_t>(i)] = std::acos(c) * 180.0 / kPi;
    }
    // trechos curtos (dobras arredondadas) x trechos longos (faces planas); agrupa os vertices de uma mesma dobra
    auto segmentLength = [&](int i) {
        const auto &p = loop[static_cast<std::size_t>(i % n)];
        const auto &q = loop[static_cast<std::size_t>((i + 1) % n)];
        return std::hypot(q[0] - p[0], q[1] - p[1]);
    };
    constexpr double kShort = 8.0;   // mm
    int start = -1;
    for (int i = 0; i < n; ++i) {
        if (segmentLength(i + n - 1) >= kShort) {   // segmento anterior longo: inicio de grupo
            start = i;
            break;
        }
    }
    if (start < 0) {
        return out;   // contorno todo curvo (circulo): sem linhas de dobra
    }
    int i = 0;
    while (i < n) {
        const int idx = (start + i) % n;
        if (turn[static_cast<std::size_t>(idx)] < 1.0) {
            ++i;
            continue;
        }
        int j = i;
        double total = turn[static_cast<std::size_t>(idx)];
        while (j + 1 < n && segmentLength(start + j) < kShort && turn[static_cast<std::size_t>((start + j + 1) % n)] >= 1.0) {
            ++j;
            total += turn[static_cast<std::size_t>((start + j) % n)];
        }
        if (total >= 20.0) {
            out.push_back(loop[static_cast<std::size_t>(idx)]);
            if (j > i) {
                out.push_back(loop[static_cast<std::size_t>((start + j) % n)]);
            }
        }
        i = j + 1;
    }
    return out;
}

QMap<int, SheetBuilder::Node> nodeMap(const SheetBuilder::Input &in)
{
    QMap<int, SheetBuilder::Node> map;
    for (const auto &n : in.nodes) {
        map.insert(n.id, n);
    }
    return map;
}

V3 pos(const SheetBuilder::Node &n) { return {n.x * 1000.0, n.y * 1000.0, n.z * 1000.0}; }   // mm

double barLength(const SheetBuilder::Bar &bar, const QMap<int, SheetBuilder::Node> &nodes)
{
    const auto a = nodes.value(bar.nodeI), b = nodes.value(bar.nodeJ);
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

bool isVertical(const SheetBuilder::Node &a, const SheetBuilder::Node &b)
{
    return std::abs(a.x - b.x) < kTol && std::abs(a.y - b.y) < kTol && std::abs(a.z - b.z) > kTol;
}

bool isHorizontal(const SheetBuilder::Node &a, const SheetBuilder::Node &b)
{
    return std::abs(a.z - b.z) < kTol;
}

struct Projected {
    double u {0}, v {0};   // mm do modelo
};

int roleLayer(const SheetBuilder::Bar &bar, bool vertical, bool horizontal, const Layers &l)
{
    if (bar.memberType == 2) return l.tie;
    if (bar.memberType == 1) return l.truss;
    if (vertical) return l.column;
    if (horizontal) return l.beam;
    return l.brace;
}

double chooseScale(double mw, double mh, double w, double h, double fixed)
{
    if (fixed > 0.0) {
        return fixed;
    }
    static const double scales[] = {1, 2, 5, 10, 20, 25, 50, 75, 100, 125, 150, 200, 250, 300, 400, 500, 750, 1000, 1500, 2000, 5000};
    for (const double s : scales) {
        if (mw / s <= w && mh / s <= h) {
            return s;
        }
    }
    return 10000.0;
}

QString letterName(int index)
{
    QString s;
    int n = index;
    do {
        s.prepend(QChar('A' + n % 26));
        n = n / 26 - 1;
    } while (n >= 0);
    return s;
}

std::vector<double> distinct(std::vector<double> values, double tolMm)
{
    std::sort(values.begin(), values.end());
    std::vector<double> out;
    for (const double v : values) {
        if (out.empty() || v - out.back() > tolMm) {
            out.push_back(v);
        }
    }
    return out;
}

std::string clip(const std::string &text, std::size_t max)
{
    return text.size() <= max ? text : text.substr(0, max > 1 ? max - 1 : 1) + ".";
}

/// Colocacao de rotulos sem sobreposicao (caixas alinhadas aos eixos).
class LabelPlacer
{
public:
    struct Box {
        double x0, y0, x1, y1;
    };
    static Box boxOf(Point center, const std::string &text, double height, double angleDeg)
    {
        const double w = static_cast<double>(text.size()) * height * 0.56, h = height * 1.1;
        const double r = angleDeg * kPi / 180.0;
        const double bw = std::abs(w * std::cos(r)) + std::abs(h * std::sin(r));
        const double bh = std::abs(w * std::sin(r)) + std::abs(h * std::cos(r));
        return {center.x - bw / 2.0, center.y - bh / 2.0, center.x + bw / 2.0, center.y + bh / 2.0};
    }
    bool free(const Box &b) const
    {
        for (const auto &o : m_boxes) {
            if (b.x0 < o.x1 && b.x1 > o.x0 && b.y0 < o.y1 && b.y1 > o.y0) {
                return false;
            }
        }
        return true;
    }
    void add(const Box &b) { m_boxes.push_back(b); }

private:
    std::vector<Box> m_boxes;
};

} // namespace

void SheetBuilder::paperSize(Paper paper, double &width, double &height)
{
    switch (paper) {
    case Paper::A4: width = 297; height = 210; break;
    case Paper::A3: width = 420; height = 297; break;
    case Paper::A2: width = 594; height = 420; break;
    case Paper::A1: width = 841; height = 594; break;
    }
}

QString SheetBuilder::paperName(Paper paper)
{
    switch (paper) {
    case Paper::A4: return QStringLiteral("A4");
    case Paper::A3: return QStringLiteral("A3");
    case Paper::A2: return QStringLiteral("A2");
    case Paper::A1: return QStringLiteral("A1");
    }
    return QStringLiteral("A3");
}

QHash<int, QString> SheetBuilder::assignMarks(const Input &input, QVector<MarkRow> *rows)
{
    const auto nodes = nodeMap(input);
    struct Key {
        QString prefix;
        QString section;
        long length {0};
        bool operator<(const Key &o) const { return std::tie(prefix, section, length) < std::tie(o.prefix, o.section, o.length); }
    };
    std::map<Key, QVector<int>> groups;
    for (const auto &bar : input.bars) {
        const auto a = nodes.value(bar.nodeI), b = nodes.value(bar.nodeJ);
        QString prefix = QStringLiteral("D");
        if (bar.memberType == 2) {
            prefix = QStringLiteral("T");
        } else if (bar.memberType == 1) {
            prefix = QStringLiteral("R");
        } else if (isVertical(a, b)) {
            prefix = QStringLiteral("P");
        } else if (isHorizontal(a, b)) {
            prefix = QStringLiteral("V");
        }
        Key key {prefix, bar.section, std::lround(barLength(bar, nodes) * 1000.0)};
        groups[key].append(bar.id);
    }
    QHash<int, QString> marks;
    QMap<QString, int> counters;
    QVector<MarkRow> list;
    for (const auto &kv : groups) {
        const int n = ++counters[kv.first.prefix];
        const QString mark = QStringLiteral("%1%2").arg(kv.first.prefix).arg(n);
        MarkRow row;
        row.mark = mark;
        row.section = kv.first.section;
        row.count = kv.second.size();
        row.lengthMm = static_cast<double>(kv.first.length);
        row.bars = kv.second;
        for (const auto &bar : input.bars) {
            if (bar.id == kv.second.first()) {
                row.massPerMeter = bar.massPerMeter;
            }
        }
        row.unitMass = row.massPerMeter * row.lengthMm / 1000.0;
        row.totalMass = row.unitMass * row.count;
        for (const int id : kv.second) {
            marks.insert(id, mark);
        }
        list.append(row);
    }
    if (rows) {
        *rows = list;
    }
    return marks;
}

QVector<SheetBuilder::ViewDef> SheetBuilder::availableViews(const Input &input)
{
    QVector<ViewDef> views;
    const auto nodes = nodeMap(input);
    if (input.bars.isEmpty()) {
        return views;
    }
    {
        ViewDef v;
        v.id = QStringLiteral("plan-all");
        v.title = QStringLiteral("Planta geral (projeção)");
        v.kind = ViewKind::PlanProjection;
        v.barCount = input.bars.size();
        views.append(v);
    }
    std::map<long, int> levels;
    for (const auto &bar : input.bars) {
        const auto a = nodes.value(bar.nodeI), b = nodes.value(bar.nodeJ);
        if (isHorizontal(a, b) && !isVertical(a, b)) {
            ++levels[std::lround(a.z * 1000.0)];
        }
    }
    for (const auto &kv : levels) {
        ViewDef v;
        v.id = QStringLiteral("plan-%1").arg(kv.first);
        v.title = QStringLiteral("Planta — nível %1 m").arg(QString::fromStdString(levelText(kv.first / 1000.0)));
        v.kind = ViewKind::PlanLevel;
        v.coordinate = kv.first / 1000.0;
        v.barCount = kv.second;
        views.append(v);
    }
    std::map<long, int> ys, xs;
    for (const auto &bar : input.bars) {
        const auto a = nodes.value(bar.nodeI), b = nodes.value(bar.nodeJ);
        if (std::abs(a.y - b.y) < kTol) {
            ++ys[std::lround(a.y * 1000.0)];
        }
        if (std::abs(a.x - b.x) < kTol) {
            ++xs[std::lround(a.x * 1000.0)];
        }
    }
    for (const auto &kv : ys) {
        if (kv.second < 2) {
            continue;
        }
        ViewDef v;
        v.id = QStringLiteral("elev-xz-%1").arg(kv.first);
        v.title = QStringLiteral("Elevação X–Z — y = %1 m").arg(QString::number(kv.first / 1000.0, 'f', 2));
        v.kind = ViewKind::ElevationXZ;
        v.coordinate = kv.first / 1000.0;
        v.barCount = kv.second;
        views.append(v);
    }
    for (const auto &kv : xs) {
        if (kv.second < 2) {
            continue;
        }
        ViewDef v;
        v.id = QStringLiteral("elev-yz-%1").arg(kv.first);
        v.title = QStringLiteral("Elevação Y–Z — x = %1 m").arg(QString::number(kv.first / 1000.0, 'f', 2));
        v.kind = ViewKind::ElevationYZ;
        v.coordinate = kv.first / 1000.0;
        v.barCount = kv.second;
        views.append(v);
    }
    {
        ViewDef v;
        v.id = QStringLiteral("bar-list");
        v.title = QStringLiteral("Lista de barras e materiais");
        v.kind = ViewKind::BarList;
        v.barCount = input.bars.size();
        views.append(v);
    }
    int planLevels = 0, xz = 0, yz = 0;
    for (const auto &v : views) {
        planLevels += v.kind == ViewKind::PlanLevel ? 1 : 0;
        xz += v.kind == ViewKind::ElevationXZ ? 1 : 0;
        yz += v.kind == ViewKind::ElevationYZ ? 1 : 0;
    }
    int seenXz = 0, seenYz = 0;
    for (auto &v : views) {
        if (v.kind == ViewKind::PlanLevel) {
            v.defaultOn = planLevels <= 6 && v.barCount >= 4;
        } else if (v.kind == ViewKind::ElevationXZ) {
            v.defaultOn = xz <= 6 || seenXz == 0;
            ++seenXz;
        } else if (v.kind == ViewKind::ElevationYZ) {
            v.defaultOn = yz <= 4 || seenYz == 0;
            ++seenYz;
        }
    }
    for (int i = 0; i < input.connections.size(); ++i) {
        const auto &c = input.connections[i].connection;
        ViewDef v;
        v.id = QStringLiteral("conn-%1").arg(c.id);
        v.title = QStringLiteral("Ligação — nó %1, viga %2").arg(c.nodeId).arg(c.memberId);
        v.kind = ViewKind::ConnectionDetail;
        v.connectionIndex = i;
        v.barCount = 1;
        views.append(v);
    }
    // vistas 3D (em linhas): as salvas no projeto ou, se nao houver, uma isometrica padrao
    if (input.views3d.isEmpty()) {
        ViewDef v;
        v.id = QStringLiteral("view3d-default");
        v.title = QStringLiteral("Vista 3D — isométrica");
        v.kind = ViewKind::View3D;
        v.barCount = input.bars.size();
        v.defaultOn = false;
        views.append(v);
    }
    for (int i = 0; i < input.views3d.size(); ++i) {
        ViewDef v;
        v.id = QStringLiteral("view3d-%1").arg(i);
        v.title = QStringLiteral("Vista 3D — %1").arg(input.views3d[i].name.isEmpty() ? QStringLiteral("vista %1").arg(i + 1) : input.views3d[i].name);
        v.kind = ViewKind::View3D;
        v.view3dIndex = i;
        v.barCount = input.bars.size();
        v.defaultOn = true;
        views.append(v);
    }
    for (auto &v : views) {
        const QString custom = input.info.sheetTitles.value(v.id);
        if (!custom.isEmpty()) {
            v.title = custom;
        }
    }
    return views;
}

// ---------------------------------------------------------------------------------------------------------------------
// moldura e carimbo
// ---------------------------------------------------------------------------------------------------------------------

namespace {

void cell(Sheet &s, const Layers &l, double x, double y, double w, double h, const std::string &label, const std::string &value, double valueHeight = 2.6, bool bold = false)
{
    s.rect({x, y}, {x + w, y + h}, l.table);
    s.text({x + 1.2, y + h - 2.6}, label, 1.6, l.textSmall);
    const std::size_t maxChars = static_cast<std::size_t>(std::max(4.0, (w - 2.4) / (valueHeight * 0.55)));
    s.text({x + 1.2, y + 1.6}, clip(value, maxChars), valueHeight, l.text, TextAlign::Left, 0.0, bold);
}

double titleBlockHeight(const SheetBuilder::Info &info)
{
    const double base = 62.0;
    const int rows = std::min<int>(6, info.revisions.size());
    return base + (rows > 0 ? 4.5 * (rows + 1) : 0.0);
}

/// Desenha o carimbo no canto inferior direito (e a tabela de revisoes acima dele). Devolve a regiao livre para o desenho.
Region drawTitleBlock(Sheet &s, const Layers &l, const SheetBuilder::Info &info, const std::string &title, const std::string &scaleText, int index, int total,
                      bool generalNotes, bool withFrame = true)
{
    const double m = 10.0, left = 20.0;
    if (withFrame) {
        s.rect({left, m}, {s.width - m, s.height - m}, l.frame);
        s.rect({5.0, 5.0}, {s.width - 5.0, s.height - 5.0}, l.frame);
    }
    const double bw = std::min(190.0, s.width - left - m - 20.0);
    const double x0 = s.width - m - bw;
    const double rowF = 10.0, rowE = 10.0, rowD = 14.0, rowC = 10.0, rowB = 9.0, rowA = 9.0;
    double y = m;
    cell(s, l, x0, y, bw * 0.45, rowF, "RESPONSÁVEL TÉCNICO", info.author.toStdString() + (info.registration.isEmpty() ? "" : "  " + info.registration.toStdString()));
    cell(s, l, x0 + bw * 0.45, y, bw * 0.275, rowF, "DESENHO", info.drafter.toStdString());
    cell(s, l, x0 + bw * 0.725, y, bw * 0.275, rowF, "VERIFICAÇÃO", info.checker.toStdString());
    y += rowF;
    cell(s, l, x0, y, bw * 0.3, rowE, "ESCALA", scaleText);
    cell(s, l, x0 + bw * 0.3, y, bw * 0.25, rowE, "DATA", info.date.toStdString());
    cell(s, l, x0 + bw * 0.55, y, bw * 0.2, rowE, "REVISÃO", info.revision.toStdString());
    cell(s, l, x0 + bw * 0.75, y, bw * 0.25, rowE, "FOLHA", std::to_string(index) + " / " + std::to_string(total), 3.0, true);
    y += rowE;
    const std::string number = (info.drawingPrefix.isEmpty() ? std::string() : info.drawingPrefix.toStdString() + "-") + (index < 10 ? "0" : "") + std::to_string(index);
    cell(s, l, x0, y, bw * 0.72, rowD, "PRANCHA", clip(title, 46), 3.2, true);
    cell(s, l, x0 + bw * 0.72, y, bw * 0.28, rowD, "Nº DO DESENHO", number, 3.0, true);
    y += rowD;
    cell(s, l, x0, y, bw, rowC, "PROJETO", info.project.toStdString(), 3.0, true);
    y += rowC;
    cell(s, l, x0, y, bw * 0.5, rowB, "CLIENTE", info.client.toStdString());
    cell(s, l, x0 + bw * 0.5, y, bw * 0.5, rowB, "OBRA / LOCAL", info.site.toStdString());
    y += rowB;
    cell(s, l, x0, y, bw, rowA, "EMPRESA", info.company.isEmpty() ? "STRUCTURA 3D" : info.company.toStdString(), 3.4, true);
    y += rowA;
    const int revRows = std::min<int>(6, info.revisions.size());
    if (revRows > 0) {
        const double rh = 4.5;
        const std::vector<double> widths = {bw * 0.08, bw * 0.17, bw * 0.55, bw * 0.20};
        const std::vector<std::string> head = {"REV", "DATA", "DESCRIÇÃO", "AUTOR"};
        double yy = y;
        double xx = x0;
        for (std::size_t c = 0; c < widths.size(); ++c) {
            s.rect({xx, yy}, {xx + widths[c], yy + rh}, l.table);
            s.text({xx + 1.0, yy + 1.2}, head[c], 1.8, l.textSmall, TextAlign::Left, 0.0, true);
            xx += widths[c];
        }
        yy += rh;
        const int first = std::max(0, static_cast<int>(info.revisions.size()) - revRows);
        for (int i = first; i < info.revisions.size(); ++i) {
            const auto &r = info.revisions[i];
            const std::vector<std::string> cells = {r.rev.toStdString(), r.date.toStdString(), r.description.toStdString(), r.author.toStdString()};
            xx = x0;
            for (std::size_t c = 0; c < widths.size(); ++c) {
                s.rect({xx, yy}, {xx + widths[c], yy + rh}, l.table);
                const std::size_t maxChars = static_cast<std::size_t>(std::max(3.0, (widths[c] - 2.0) / 1.3));
                s.text({xx + 1.0, yy + 1.2}, clip(cells[c], maxChars), 2.1, l.text);
                xx += widths[c];
            }
            yy += rh;
        }
    }
    if (generalNotes && !info.notes.isEmpty()) {
        const QStringList lines = info.notes.split(QLatin1Char('\n'));
        double ny = m + 4.0 + 4.0 * (lines.size() - 1);
        s.text({left + 6.0, ny + 5.0}, "NOTAS GERAIS", 2.6, l.text, TextAlign::Left, 0.0, true);
        for (const QString &line : lines) {
            s.text({left + 6.0, ny}, line.toStdString(), 2.3, l.text);
            ny -= 4.0;
        }
    }
    const double bh = titleBlockHeight(info);
    return {left + 5.0, m + bh + 6.0, s.width - m - 5.0, s.height - m - 6.0};
}

} // namespace

Sheet SheetBuilder::previewTitleBlock(const Info &info)
{
    Sheet s;
    s.name = "Carimbo";
    s.width = 220.0;
    s.height = titleBlockHeight(info) + 24.0;
    const Layers l = makeLayers(s);
    drawTitleBlock(s, l, info, "Título da prancha", "1:100", 1, 8, false, false);
    return s;
}

// ---------------------------------------------------------------------------------------------------------------------
// desenho de uma vista
// ---------------------------------------------------------------------------------------------------------------------

namespace {

struct ViewContext {
    const SheetBuilder::Input &input;
    const QMap<int, SheetBuilder::Node> &nodes;
    const QHash<int, QString> &marks;
    const SheetBuilder::Options &options;
    bool compact {false};   ///< vista em celula de folha composta: margens menores
};

void dimensionChain(Sheet &s, const Layers &l, const std::vector<double> &coordsMm, bool horizontal, double base, double sign, double k, double origin, bool overall)
{
    if (coordsMm.size() < 2) {
        return;
    }
    auto toPaper = [&](double c) { return origin + c * k; };
    const double ext = 1.5;
    const double h = 2.2;
    for (std::size_t i = 0; i < coordsMm.size(); ++i) {
        const double u = toPaper(coordsMm[i]);
        if (horizontal) {
            s.line({u, base - sign * ext}, {u, base}, l.dim);
            s.line({u - 1.0, base - 1.0}, {u + 1.0, base + 1.0}, l.dim);
        } else {
            s.line({base - sign * ext, u}, {base, u}, l.dim);
            s.line({base - 1.0, u - 1.0}, {base + 1.0, u + 1.0}, l.dim);
        }
    }
    const double first = toPaper(coordsMm.front()), last = toPaper(coordsMm.back());
    if (horizontal) {
        s.line({first, base}, {last, base}, l.dim);
    } else {
        s.line({base, first}, {base, last}, l.dim);
    }
    auto label = [&](double a, double b, double value) {
        const double mid = (a + b) / 2.0;
        const std::string t = num(value, 0);
        if (horizontal) {
            s.text({mid, base + 0.8}, t, h, l.dim, TextAlign::Center);
        } else {
            s.text({base - 0.8, mid}, t, h, l.dim, TextAlign::Center, 90.0);
        }
    };
    if (overall) {
        label(first, last, coordsMm.back() - coordsMm.front());
        return;
    }
    for (std::size_t i = 0; i + 1 < coordsMm.size(); ++i) {
        label(toPaper(coordsMm[i]), toPaper(coordsMm[i + 1]), coordsMm[i + 1] - coordsMm[i]);
    }
}

struct Item {
    const SheetBuilder::Bar *bar;
    Projected a, b;       ///< eixo (centroide) da barra, ja deslocado pela excentricidade
    V3 a3, b3;            ///< idem, em 3D (mm)
    bool vertical, horizontal;
    Axes axes;
    Projected nodeA, nodeB;   ///< posicao dos nos (para eixos de referencia e cotas)
};

/// Posicao do centroide da barra em um de seus nos: no + y*dy + z*dz (eixos locais girados).
V3 centroidAt(const V3 &node, const Axes &axes, double dy, double dz)
{
    return node + axes.y * dy + axes.z * dz;
}

/// Meia extensao (mm) da secao de `other` ao longo da direcao `dir` (vetor unitario 3D).
double halfExtentAlong(const SheetBuilder::Bar &other, const Axes &axes, V3 dir)
{
    return 0.5 * (other.depthMm * std::abs(dot(axes.y, dir)) + other.widthMm * std::abs(dot(axes.z, dir)));
}

void drawView(Sheet &s, const Layers &l, const Region &region, const SheetBuilder::ViewDef &view, const ViewContext &ctx, double &scaleOut)
{
    using Kind = SheetBuilder::ViewKind;
    using Detail = SheetBuilder::ProfileDetail;
    const auto &nodes = ctx.nodes;
    V3 U {1, 0, 0}, Vv {0, 1, 0};
    if (view.kind == Kind::ElevationXZ) {
        Vv = {0, 0, 1};
    } else if (view.kind == Kind::ElevationYZ) {
        U = {0, 1, 0};
        Vv = {0, 0, 1};
    }
    auto project = [&](const V3 &p) { return Projected {dot(p, U), dot(p, Vv)}; };

    std::vector<Item> items;
    std::vector<Item> planColumns;
    for (const auto &bar : ctx.input.bars) {
        const auto na = nodes.value(bar.nodeI), nb = nodes.value(bar.nodeJ);
        const bool vertical = isVertical(na, nb);
        const bool horizontal = isHorizontal(na, nb);
        const V3 n3a = pos(na), n3b = pos(nb);
        const Axes axes = localAxes(n3a, n3b, bar.rotationDeg);
        const V3 a3 = centroidAt(n3a, axes, bar.offsetIy, bar.offsetIz) + axes.x * bar.trimI;
        const V3 b3 = centroidAt(n3b, axes, bar.offsetJy, bar.offsetJz) - axes.x * bar.trimJ;
        Item item {&bar, project(a3), project(b3), a3, b3, vertical, horizontal, axes, project(n3a), project(n3b)};
        bool take = false;
        switch (view.kind) {
        case Kind::PlanProjection:
            take = !vertical;
            if (vertical) {
                planColumns.push_back(item);
            }
            break;
        case Kind::PlanLevel:
            take = horizontal && !vertical && std::abs(na.z - view.coordinate) < kTol;
            if (vertical && (std::abs(na.z - view.coordinate) < kTol || std::abs(nb.z - view.coordinate) < kTol)) {
                planColumns.push_back(item);
            }
            break;
        case Kind::ElevationXZ: take = std::abs(na.y - view.coordinate) < kTol && std::abs(nb.y - view.coordinate) < kTol; break;
        case Kind::ElevationYZ: take = std::abs(na.x - view.coordinate) < kTol && std::abs(nb.x - view.coordinate) < kTol; break;
        default: break;
        }
        if (take) {
            items.push_back(item);
        }
    }
    if (items.empty() && planColumns.empty()) {
        return;
    }
    double minU = 1e300, maxU = -1e300, minV = 1e300, maxV = -1e300;
    auto grow = [&](const Projected &p) {
        minU = std::min(minU, p.u);
        maxU = std::max(maxU, p.u);
        minV = std::min(minV, p.v);
        maxV = std::max(maxV, p.v);
    };
    for (const auto &it : items) {
        grow(it.a);
        grow(it.b);
        grow(it.nodeA);
        grow(it.nodeB);
    }
    for (const auto &it : planColumns) {
        grow(it.a);
        grow(it.nodeA);
    }
    const double reserve = ctx.compact ? 26.0 : 34.0;
    const double titleH = 12.0;
    const double availW = region.width() - 2.0 * reserve, availH = region.height() - 2.0 * reserve - titleH;
    const double scale = chooseScale(maxU - minU, maxV - minV, availW, availH, ctx.options.scaleDenominator);
    scaleOut = scale;
    const double k = 1.0 / scale;
    const double drawW = (maxU - minU) * k, drawH = (maxV - minV) * k;
    const double ox = region.x0 + reserve + (availW - drawW) / 2.0 - minU * k;
    const double oy = region.y0 + reserve + (availH - drawH) / 2.0 - minV * k;
    auto P = [&](const Projected &p) { return Point {ox + p.u * k, oy + p.v * k}; };

    // eixos
    std::vector<double> us, vs;
    for (const auto &it : items) {
        us.push_back(it.nodeA.u);
        us.push_back(it.nodeB.u);
        vs.push_back(it.nodeA.v);
        vs.push_back(it.nodeB.v);
    }
    for (const auto &it : planColumns) {
        us.push_back(it.nodeA.u);
        vs.push_back(it.nodeA.v);
    }
    const auto gridU = distinct(us, 1.0), gridV = distinct(vs, 1.0);
    const bool plan = view.kind == Kind::PlanProjection || view.kind == Kind::PlanLevel;
    // elevacoes com muitos niveis (cobertura inclinada): so extremos e niveis onde terminam pilares
    std::vector<double> levelV = gridV;
    if (!plan && gridV.size() > 8) {
        std::vector<double> keep {gridV.front(), gridV.back()};
        for (const auto &it : items) {
            if (it.vertical) {
                keep.push_back(it.nodeA.v);
                keep.push_back(it.nodeB.v);
            }
        }
        levelV = distinct(keep, 1.0);
    }
    auto axisNameFor = [&](double coordMm, bool xAxis, int fallbackIndex) {
        for (const auto &level : ctx.input.axes.levels) {
            const auto &list = xAxis ? level.x : level.y;
            for (const auto &a : list) {
                if (std::abs(a.value * 1000.0 - coordMm) < 2.0) {
                    return a.label.toStdString();
                }
            }
        }
        return xAxis ? letterName(fallbackIndex).toStdString() : std::to_string(fallbackIndex + 1);
    };
    const double balloon = 3.6;
    const double extend = 14.0;
    for (std::size_t i = 0; i < gridU.size() && gridU.size() <= 40; ++i) {
        const double x = ox + gridU[i] * k;
        const bool uIsX = view.kind != Kind::ElevationYZ;
        const std::string name = axisNameFor(gridU[i], plan || uIsX, static_cast<int>(i));
        s.line({x, oy + minV * k - extend}, {x, oy + maxV * k + extend}, l.axis);
        s.circle({x, oy + maxV * k + extend + balloon}, balloon, l.axis);
        s.text({x, oy + maxV * k + extend + balloon - 1.0}, name, 2.6, l.text, TextAlign::Center);
    }
    if (plan) {
        for (std::size_t i = 0; i < gridV.size() && gridV.size() <= 40; ++i) {
            const double y = oy + gridV[i] * k;
            s.line({ox + minU * k - extend, y}, {ox + maxU * k + extend, y}, l.axis);
            s.circle({ox + minU * k - extend - balloon, y}, balloon, l.axis);
            s.text({ox + minU * k - extend - balloon, y - 1.0}, axisNameFor(gridV[i], false, static_cast<int>(i)), 2.6, l.text, TextAlign::Center);
        }
    } else {
        for (const double z : levelV) {
            const double y = oy + z * k;
            s.line({ox + minU * k - extend, y}, {ox + maxU * k + extend, y}, l.axis);
            s.text({ox + minU * k - extend - 1.5, y + 0.8}, levelText(z / 1000.0), 2.2, l.text, TextAlign::Right);
        }
    }

    // barras: contorno do perfil (largura visivel pela projecao da caixa d x bf)
    LabelPlacer placer;
    const bool outline = ctx.options.profiles != Detail::Axis;
    struct Pending {
        Item item;
        Point a, b;
        double halfBand;
    };
    std::vector<Pending> pending;
    for (const auto &it : items) {
        const int layer = roleLayer(*it.bar, it.vertical, it.horizontal, l);
        const Point a = P(it.a), b = P(it.b);
        const double len = std::hypot(b.x - a.x, b.y - a.y);
        double halfBand = 0.0;
        if (len > 1e-6) {
            const double ux = (b.x - a.x) / len, uy = (b.y - a.y) / len;
            const V3 T3 = U * (-uy) + Vv * ux;
            const double widthMm = it.bar->depthMm * std::abs(dot(it.axes.y, T3)) + it.bar->widthMm * std::abs(dot(it.axes.z, T3));
            const double wp = widthMm * k;
            if (outline && wp >= 0.9) {
                halfBand = wp / 2.0;
                const double nx = -uy * halfBand, ny = ux * halfBand;
                s.polyline({{a.x + nx, a.y + ny}, {b.x + nx, b.y + ny}, {b.x - nx, b.y - ny}, {a.x - nx, a.y - ny}}, true, layer);
                if (ctx.options.profiles == Detail::OutlineWithFlanges && halfBand >= 1.0) {
                    // linhas de dobra e arestas longitudinais do perfil (projecao dos vertices do contorno)
                    std::vector<double> offsets;
                    if (const Outline *outline = outlineOf(*it.bar)) {
                        for (const auto &loop : outline->loops) {
                            for (const auto &v : foldVertices(loop)) {
                                offsets.push_back(v[0] * dot(it.axes.z, T3) + v[1] * dot(it.axes.y, T3));
                            }
                        }
                    } else if (it.bar->iShape && it.bar->flangeMm > 0.0 && std::abs(dot(it.axes.y, T3)) > 0.7) {
                        const double o = (it.bar->depthMm / 2.0 - it.bar->flangeMm) * std::abs(dot(it.axes.y, T3));
                        offsets = {o, -o};
                    }
                    std::sort(offsets.begin(), offsets.end());
                    double lastOffset = -1e300;
                    int drawn = 0;
                    for (const double off : offsets) {
                        const double po = off * k;
                        if (std::abs(off - lastOffset) < 0.05 || std::abs(po) >= halfBand - 0.12 || drawn >= 18) {
                            continue;
                        }
                        lastOffset = off;
                        ++drawn;
                        s.line({a.x + (-uy) * po, a.y + ux * po}, {b.x + (-uy) * po, b.y + ux * po}, l.flange);
                    }
                }
                if (halfBand > 1.0) {
                    s.line(a, b, l.centre);
                }
            } else {
                s.line(a, b, layer);
            }
            if (it.bar->hingeI || it.bar->hingeJ) {
                const double inset = 1.8 + halfBand;
                if (it.bar->hingeI) {
                    s.circle({a.x + ux * inset, a.y + uy * inset}, 0.9, l.hinge);
                }
                if (it.bar->hingeJ) {
                    s.circle({b.x - ux * inset, b.y - uy * inset}, 0.9, l.hinge);
                }
            }
        }
        pending.push_back({it, a, b, halfBand});
    }
    // pilares em planta: contorno real (I ou retangulo)
    std::set<std::pair<long, long>> done;
    for (const auto &it : planColumns) {
        const auto key = std::make_pair(std::lround(it.nodeA.u), std::lround(it.nodeA.v));
        if (!done.insert(key).second) {
            continue;
        }
        const Point c = P(it.a);
        const double boost = std::max(1.0, 1.6 / std::max(1e-6, std::max(it.bar->depthMm, it.bar->widthMm) * k));
        auto map2 = [&](double zc, double yc) {
            const V3 v = it.axes.z * (zc * boost) + it.axes.y * (yc * boost);
            return Point {c.x + dot(v, U) * k, c.y + dot(v, Vv) * k};
        };
        const double bf = it.bar->widthMm, d = it.bar->depthMm;
        const Outline *outline = outlineOf(*it.bar);
        if (outline && ctx.options.profiles != Detail::Axis) {
            const bool hollow = outline->loops.size() > 1;
            for (std::size_t li = 0; li < outline->loops.size(); ++li) {
                std::vector<Point> shape;
                for (const auto &p : outline->loops[li]) {
                    shape.push_back(map2(p[0], p[1]));
                }
                s.polyline(shape, true, l.plan, !hollow);
            }
        } else {
            std::vector<Point> shape;
            for (const auto &p : std::vector<std::pair<double, double>> {{-bf / 2, d / 2}, {bf / 2, d / 2}, {bf / 2, -d / 2}, {-bf / 2, -d / 2}}) {
                shape.push_back(map2(p.first, p.second));
            }
            s.polyline(shape, true, l.plan, true);
        }
        if (ctx.options.barLabels) {
            const std::string text = ctx.marks.value(it.bar->id).toStdString();
            const double reach = std::max(it.bar->widthMm, it.bar->depthMm) * k * 0.5 + 1.4;
            const Point at {c.x + reach + 0.5 + static_cast<double>(text.size()) * 0.55, c.y + reach};
            const auto box = LabelPlacer::boxOf(at, text, 1.9, 0.0);
            if (placer.free(box)) {
                placer.add(box);
                s.text(at, text, 1.9, l.textSmall, TextAlign::Center);
            }
        }
    }
    // rotulos das barras sem sobreposicao
    if (ctx.options.barLabels) {
        for (const auto &p : pending) {
            const double len = std::hypot(p.b.x - p.a.x, p.b.y - p.a.y);
            if (len < 8.0) {
                continue;
            }
            double angle = std::atan2(p.b.y - p.a.y, p.b.x - p.a.x) * 180.0 / kPi;
            if (angle > 90.0 + 1e-9) angle -= 180.0;
            if (angle <= -90.0 + 1e-9) angle += 180.0;
            const double rad = angle * kPi / 180.0;
            const double dx = std::cos(rad), dy = std::sin(rad);
            const double nx = -dy, ny = dx;
            const std::string mark = ctx.marks.value(p.item.bar->id).toStdString();
            const std::string full = len > 30.0 ? mark + " " + p.item.bar->section.toStdString() : mark;
            const double mx = (p.a.x + p.b.x) / 2.0, my = (p.a.y + p.b.y) / 2.0;
            bool placed = false;
            for (const std::string &text : {full, mark}) {
                for (const double side : {1.0, -1.0}) {
                    for (const double shift : {0.0, 0.2, -0.2, 0.34, -0.34}) {
                        const double off = p.halfBand + 1.5 + (side < 0 ? 1.0 : 0.0);
                        const Point at {mx + dx * shift * len + nx * off * side, my + dy * shift * len + ny * off * side};
                        const auto box = LabelPlacer::boxOf(at, text, 1.9, angle);
                        if (placer.free(box)) {
                            placer.add(box);
                            s.text(at, text, 1.9, l.textSmall, TextAlign::Center, angle);
                            placed = true;
                            break;
                        }
                    }
                    if (placed) break;
                }
                if (placed) break;
            }
        }
    }
    // cotas entre faces (vao livre) de cada barra no plano da vista
    if (ctx.options.faceDimensions) {
        for (const auto &p : pending) {
            const double len = std::hypot(p.b.x - p.a.x, p.b.y - p.a.y);
            if (len < 30.0) {
                continue;
            }
            const V3 axis = unit(p.item.b3 - p.item.a3);
            const double length3 = norm(p.item.b3 - p.item.a3);
            if (std::abs(len * scale - length3) > 0.02 * length3) {
                continue;   // barra fora do plano (so projecao)
            }
            // posicoes ao longo do eixo (a partir do inicio S do centroide) das faces das barras que chegam em cada no
            const V3 S = p.item.a3, E = p.item.b3;
            auto faceAt = [&](int nodeId, bool atStart) {
                double best = atStart ? 0.0 : length3;
                for (const auto &other : ctx.input.bars) {
                    if (other.id == p.item.bar->id || (other.nodeI != nodeId && other.nodeJ != nodeId)) {
                        continue;
                    }
                    const auto na = nodes.value(other.nodeI), nb = nodes.value(other.nodeJ);
                    const V3 oa = pos(na), ob = pos(nb);
                    if (std::abs(dot(unit(ob - oa), axis)) > 0.85) {
                        continue;   // colinear
                    }
                    const Axes oaxes = localAxes(oa, ob, other.rotationDeg);
                    const bool otherStartsHere = other.nodeI == nodeId;
                    const V3 centre = otherStartsHere ? centroidAt(oa, oaxes, other.offsetIy, other.offsetIz) : centroidAt(ob, oaxes, other.offsetJy, other.offsetJz);
                    const double half = halfExtentAlong(other, oaxes, axis);
                    if (atStart) {
                        best = std::max(best, dot(centre - S, axis) + half);
                    } else {
                        best = std::min(best, length3 + dot(centre - E, axis) - half);
                    }
                }
                return best;
            };
            const double tI = faceAt(p.item.bar->nodeI, true), tJ = faceAt(p.item.bar->nodeJ, false);
            const double face = tJ - tI;
            if (std::abs(tI) + std::abs(length3 - tJ) < 1.0 || face < 10.0) {
                continue;
            }
            const double ux = (p.b.x - p.a.x) / len, uy = (p.b.y - p.a.y) / len;
            Point q1 {p.a.x + ux * tI * k, p.a.y + uy * tI * k};
            Point q2 {p.a.x + ux * tJ * k, p.a.y + uy * tJ * k};
            if (ux < -1e-9 || (std::abs(ux) < 1e-9 && uy < 0)) {
                std::swap(q1, q2);
            }
            internal::dimension(s, l.face, q1, q2, -(p.halfBand + 5.0), num(face, 0), 2.0);
        }
    }
    // cotas entre eixos
    if (ctx.options.dimensions) {
        const double leftX = ox + minU * k, bottomY = oy + minV * k;
        if (gridU.size() <= 40) {
            dimensionChain(s, l, gridU, true, bottomY - 8.0, -1.0, k, ox, false);
            dimensionChain(s, l, {gridU.front(), gridU.back()}, true, bottomY - 15.0, -1.0, k, ox, true);
        }
        if (levelV.size() <= 40) {
            dimensionChain(s, l, levelV, false, leftX - extend - 2.0 * balloon - 7.0, -1.0, k, oy, false);
            dimensionChain(s, l, {levelV.front(), levelV.back()}, false, leftX - extend - 2.0 * balloon - 14.0, -1.0, k, oy, true);
        }
    }
    const double tx = region.x0 + region.width() / 2.0;
    s.text({tx, region.y0 + 4.0}, view.title.toUpper().toStdString(), 3.6, l.text, TextAlign::Center, 0.0, true);
    s.text({tx, region.y0 + 0.0}, "ESC. 1:" + num(scale, 0), 2.6, l.text, TextAlign::Center);
}


// ---------------------------------------------------------------------------------------------------------------------
// vista 3D em linhas (arestas visiveis por remocao de linhas ocultas)
// ---------------------------------------------------------------------------------------------------------------------

namespace {

/// Triangulos de um anel entre o contorno externo e um furo (aproximacao por vizinhanca).
void ringTriangles(const std::vector<std::array<double, 2>> &outer, const std::vector<std::array<double, 2>> &inner, std::vector<std::array<std::array<double, 2>, 3>> &out)
{
    auto nearest = [](const std::vector<std::array<double, 2>> &loop, const std::array<double, 2> &p) {
        std::size_t best = 0;
        double bestD = 1e300;
        for (std::size_t i = 0; i < loop.size(); ++i) {
            const double d = std::hypot(loop[i][0] - p[0], loop[i][1] - p[1]);
            if (d < bestD) {
                bestD = d;
                best = i;
            }
        }
        return best;
    };
    for (std::size_t i = 0; i < outer.size(); ++i) {
        const auto &a = outer[i], &b = outer[(i + 1) % outer.size()];
        const std::array<double, 2> mid {(a[0] + b[0]) / 2, (a[1] + b[1]) / 2};
        out.push_back({a, b, inner[nearest(inner, mid)]});
    }
    for (std::size_t i = 0; i < inner.size(); ++i) {
        const auto &a = inner[i], &b = inner[(i + 1) % inner.size()];
        const std::array<double, 2> mid {(a[0] + b[0]) / 2, (a[1] + b[1]) / 2};
        out.push_back({a, b, outer[nearest(outer, mid)]});
    }
}

void draw3DView(Sheet &s, const Layers &l, const Region &region, const SheetBuilder::ViewDef &view, const ViewContext &ctx, double &scaleOut)
{
    using Vec3 = Structura::Drawing::Vec3;
    const auto &nodes = ctx.nodes;
    Structura::App::ProjectData::SavedView3D saved;
    if (view.view3dIndex >= 0 && view.view3dIndex < ctx.input.views3d.size()) {
        saved = ctx.input.views3d[view.view3dIndex];
    }
    // limites do modelo (mm)
    V3 lo {1e300, 1e300, 1e300}, hi {-1e300, -1e300, -1e300};
    for (const auto &n : ctx.input.nodes) {
        const V3 p = pos(n);
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
    if (lo.x > hi.x) {
        return;
    }
    const V3 centre {(lo.x + hi.x) / 2, (lo.y + hi.y) / 2, (lo.z + hi.z) / 2};
    const double diagonal = norm(hi - lo);
    Structura::Drawing::Camera3D camera;
    if (saved.explicitCamera) {
        camera.position = {saved.position[0] * 1000.0, saved.position[1] * 1000.0, saved.position[2] * 1000.0};
        camera.target = {saved.target[0] * 1000.0, saved.target[1] * 1000.0, saved.target[2] * 1000.0};
        camera.up = saved.up;
        camera.perspective = saved.perspective;
        camera.fovDeg = saved.fovDeg;
    } else {
        camera = Structura::Drawing::cameraFromAngles(saved.azimuthDeg, saved.elevationDeg, {centre.x, centre.y, centre.z}, std::max(diagonal, 1000.0) * (saved.perspective ? 1.6 : 3.0));
        camera.perspective = saved.perspective;
        camera.fovDeg = saved.fovDeg;
    }

    std::vector<Structura::Drawing::Triangle3D> faces;
    std::vector<Structura::Drawing::Edge3D> edges;
    int barIndex = 0;
    for (const auto &bar : ctx.input.bars) {
        const auto na = nodes.value(bar.nodeI), nb = nodes.value(bar.nodeJ);
        const V3 n3a = pos(na), n3b = pos(nb);
        const Axes axes = localAxes(n3a, n3b, bar.rotationDeg);
        const V3 a3 = centroidAt(n3a, axes, bar.offsetIy, bar.offsetIz) + axes.x * bar.trimI;
        const V3 b3 = centroidAt(n3b, axes, bar.offsetJy, bar.offsetJz) - axes.x * bar.trimJ;
        const int role = bar.memberType == 2 ? 4 : (bar.memberType == 1 ? 3 : (isVertical(na, nb) ? 0 : (isHorizontal(na, nb) ? 1 : 2)));
        const int tag = barIndex * 8 + role;
        ++barIndex;
        std::vector<std::vector<std::array<double, 2>>> loops;
        if (const Outline *outline = outlineOf(bar)) {
            loops = outline->loops;
        } else if (bar.widthMm > 0.0 && bar.depthMm > 0.0) {
            const double hw = bar.widthMm / 2, hd = bar.depthMm / 2;
            loops = {{{-hw, hd}, {hw, hd}, {hw, -hd}, {-hw, -hd}}};
        }
        auto vertex = [&](const V3 &base, const std::array<double, 2> &p) {
            const V3 v = base + axes.z * p[0] + axes.y * p[1];
            return Vec3 {v.x, v.y, v.z};
        };
        if (loops.empty()) {
            edges.push_back({{a3.x, a3.y, a3.z}, {b3.x, b3.y, b3.z}, tag});
            continue;
        }
        for (std::size_t li = 0; li < loops.size(); ++li) {
            const auto &loop = loops[li];
            const std::size_t n = loop.size();
            for (std::size_t i = 0; i < n; ++i) {
                const auto &p = loop[i], &q = loop[(i + 1) % n];
                const Vec3 pa = vertex(a3, p), qa = vertex(a3, q), pb = vertex(b3, p), qb = vertex(b3, q);
                faces.push_back({pa, qa, qb});
                faces.push_back({pa, qb, pb});
                edges.push_back({pa, qa, tag});   // contorno das tampas
                edges.push_back({pb, qb, tag});
            }
            for (const auto &fold : foldVertices(loop)) {   // arestas longitudinais / linhas de dobra
                edges.push_back({vertex(a3, fold), vertex(b3, fold), tag});
            }
        }
        // tampas
        auto cap = [&](const V3 &base) {
            if (loops.size() == 1) {
                const auto idx = Structura::Drawing::triangulatePolygon(loops[0]);
                for (std::size_t i = 0; i + 2 < idx.size(); i += 3) {
                    faces.push_back({vertex(base, loops[0][static_cast<std::size_t>(idx[i])]), vertex(base, loops[0][static_cast<std::size_t>(idx[i + 1])]),
                                     vertex(base, loops[0][static_cast<std::size_t>(idx[i + 2])])});
                }
            } else {
                std::vector<std::array<std::array<double, 2>, 3>> ring;
                for (std::size_t li = 1; li < loops.size(); ++li) {
                    ringTriangles(loops[0], loops[li], ring);
                }
                for (const auto &t : ring) {
                    faces.push_back({vertex(base, t[0]), vertex(base, t[1]), vertex(base, t[2])});
                }
            }
        };
        cap(a3);
        cap(b3);
    }

    const auto result = Structura::Drawing::computeHiddenLines(camera, faces, edges, ctx.options.hiddenLines3D);
    if (!result.ok) {
        return;
    }
    const double w = std::max(1e-9, result.maxX - result.minX), h = std::max(1e-9, result.maxY - result.minY);
    const double reserve = ctx.compact ? 12.0 : 20.0, titleH = 12.0;
    const double availW = region.width() - 2.0 * reserve, availH = region.height() - 2.0 * reserve - titleH;
    double k = std::min(availW / w, availH / h);
    if (!camera.perspective) {
        const double scale = chooseScale(w, h, availW, availH, ctx.options.scaleDenominator);
        scaleOut = scale;
        k = 1.0 / scale;
    } else {
        scaleOut = 0.0;
    }
    const double ox = region.x0 + reserve + (availW - w * k) / 2.0 - result.minX * k;
    const double oy = region.y0 + reserve + (availH - h * k) / 2.0 - result.minY * k;
    const int roleLayers[5] = {l.column, l.beam, l.brace, l.truss, l.tie};
    std::vector<std::pair<double, int>> longest(static_cast<std::size_t>(barIndex), {0.0, -1});
    for (std::size_t i = 0; i < result.segments.size(); ++i) {
        const auto &seg = result.segments[i];
        const Point a {ox + seg.x0 * k, oy + seg.y0 * k}, b {ox + seg.x1 * k, oy + seg.y1 * k};
        if (std::hypot(b.x - a.x, b.y - a.y) < 0.05) {
            continue;
        }
        s.line(a, b, seg.hidden ? l.hidden : roleLayers[seg.tag & 7]);
        if (!seg.hidden) {
            const auto bar = static_cast<std::size_t>(seg.tag >> 3);
            const double len = std::hypot(b.x - a.x, b.y - a.y);
            if (bar < longest.size() && len > longest[bar].first) {
                longest[bar] = {len, static_cast<int>(i)};
            }
        }
    }
    if (ctx.options.barLabels) {
        LabelPlacer placer;
        int index = 0;
        for (const auto &bar : ctx.input.bars) {
            const auto &pick = longest[static_cast<std::size_t>(index++)];
            if (pick.second < 0 || pick.first < 14.0) {
                continue;
            }
            const auto &seg = result.segments[static_cast<std::size_t>(pick.second)];
            const Point a {ox + seg.x0 * k, oy + seg.y0 * k}, b {ox + seg.x1 * k, oy + seg.y1 * k};
            double angle = std::atan2(b.y - a.y, b.x - a.x) * 180.0 / kPi;
            if (angle > 90.0 + 1e-9) angle -= 180.0;
            if (angle <= -90.0 + 1e-9) angle += 180.0;
            const double rad = angle * kPi / 180.0;
            const std::string text = ctx.marks.value(bar.id).toStdString();
            for (const double side : {1.0, -1.0}) {
                const Point at {(a.x + b.x) / 2 - std::sin(rad) * 1.8 * side, (a.y + b.y) / 2 + std::cos(rad) * 1.8 * side};
                const auto box = LabelPlacer::boxOf(at, text, 1.9, angle);
                if (placer.free(box)) {
                    placer.add(box);
                    s.text(at, text, 1.9, l.textSmall, TextAlign::Center, angle);
                    break;
                }
            }
        }
    }
    const double tx = region.x0 + region.width() / 2.0;
    std::string subtitle = camera.perspective ? "PERSPECTIVA" : "PROJEÇÃO ORTOGONAL";
    if (!saved.explicitCamera) {
        subtitle += "  AZ " + num(saved.azimuthDeg, 0) + "°  EL " + num(saved.elevationDeg, 0) + "°";
    }
    s.text({tx, region.y0 + 4.0}, view.title.toUpper().toStdString(), 3.6, l.text, TextAlign::Center, 0.0, true);
    s.text({tx, region.y0 + 0.0}, (scaleOut > 0.0 ? "ESC. 1:" + num(scaleOut, 0) + "   " : std::string("SEM ESCALA   ")) + subtitle, 2.6, l.text, TextAlign::Center);
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// lista de barras
// ---------------------------------------------------------------------------------------------------------------------

std::vector<Sheet> barListSheets(const SheetBuilder::Input &input, double w, double h, int &sheetCounter, const QVector<SheetBuilder::MarkRow> &rows, const std::string &baseTitle)
{
    std::vector<Sheet> sheets;
    struct PartRow {
        std::string description;
        int quantity;
        double massEach;
        double total;
    };
    std::vector<PartRow> partRows;
    for (const auto &detail : input.connections) {
        for (const auto &part : internal::connectionParts(detail)) {
            bool merged = false;
            for (auto &row : partRows) {
                if (row.description == part.description && std::abs(row.massEach - part.massEach) < 1e-9) {
                    row.quantity += part.quantity;
                    row.total += part.quantity * part.massEach;
                    merged = true;
                }
            }
            if (!merged) {
                partRows.push_back({part.description, part.quantity, part.massEach, part.quantity * part.massEach});
            }
        }
    }
    const double rowH = 6.0;
    const double bandBottom = 10.0 + titleBlockHeight(input.info) + 8.0;
    const double top = h - 10.0 - 22.0;
    const int perPage = std::max(5, static_cast<int>((top - bandBottom - rowH) / rowH));
    struct Line {
        std::vector<std::string> cells;
        bool section {false};
        bool total {false};
    };
    std::vector<Line> lines;
    double totalMass = 0.0;
    for (const auto &r : rows) {
        lines.push_back({{r.mark.toStdString(), r.section.toStdString(), std::to_string(r.count), num(r.lengthMm, 0), num(r.massPerMeter, 2), num(r.unitMass, 1), num(r.totalMass, 1)}});
        totalMass += r.totalMass;
    }
    lines.push_back({{"", "TOTAL DE BARRAS", "", "", "", "", num(totalMass, 1)}, false, true});
    double connMass = 0.0;
    if (!partRows.empty()) {
        lines.push_back({{"LIGAÇÕES (peças)"}, true});
        for (const auto &p : partRows) {
            lines.push_back({{"", p.description, std::to_string(p.quantity), "", "", num(p.massEach, 2), num(p.total, 1)}});
            connMass += p.total;
        }
        lines.push_back({{"", "TOTAL DE LIGAÇÕES", "", "", "", "", num(connMass, 1)}, false, true});
        lines.push_back({{"", "TOTAL GERAL", "", "", "", "", num(totalMass + connMass, 1)}, false, true});
    }
    const int pages = std::max(1, static_cast<int>((lines.size() + perPage - 1) / perPage));
    const std::vector<double> widths = {22.0, 120.0, 18.0, 28.0, 24.0, 28.0, 30.0};
    const std::vector<std::string> headers = {"MARCA", "PERFIL / PEÇA", "QTD.", "COMP. (mm)", "kg/m", "MASSA UNIT. (kg)", "MASSA TOTAL (kg)"};
    for (int page = 0; page < pages; ++page) {
        Sheet s;
        s.width = w;
        s.height = h;
        s.name = baseTitle + std::string(pages > 1 ? " (" + std::to_string(page + 1) + "/" + std::to_string(pages) + ")" : "");
        s.fileStem = "lista-de-barras" + std::string(pages > 1 ? "-" + std::to_string(page + 1) : "");
        const Layers l = makeLayers(s);
        drawTitleBlock(s, l, input.info, s.name, "—", ++sheetCounter, 0, true);
        const double x0 = 25.0;
        double y = top;
        s.text({x0, h - 10.0 - 8.0}, "LISTA DE BARRAS E MATERIAIS", 4.5, l.text, TextAlign::Left, 0.0, true);
        double x = x0;
        for (std::size_t c = 0; c < headers.size(); ++c) {
            s.rect({x, y - rowH}, {x + widths[c], y}, l.table);
            s.text({x + 1.5, y - rowH + 1.8}, headers[c], 2.2, l.text, TextAlign::Left, 0.0, true);
            x += widths[c];
        }
        y -= rowH;
        const int start = page * perPage;
        for (int i = start; i < std::min(static_cast<int>(lines.size()), start + perPage); ++i) {
            const auto &line = lines[static_cast<std::size_t>(i)];
            x = x0;
            if (line.section) {
                s.rect({x0, y - rowH}, {x0 + 270.0, y}, l.table);
                s.text({x0 + 1.5, y - rowH + 1.8}, line.cells[0], 2.4, l.text, TextAlign::Left, 0.0, true);
            } else {
                for (std::size_t c = 0; c < widths.size(); ++c) {
                    s.rect({x, y - rowH}, {x + widths[c], y}, l.table);
                    if (c < line.cells.size()) {
                        const bool right = c >= 2;
                        s.text({right ? x + widths[c] - 1.5 : x + 1.5, y - rowH + 1.8}, line.cells[c], 2.3, l.text, right ? TextAlign::Right : TextAlign::Left, 0.0, line.total);
                    }
                    x += widths[c];
                }
            }
            y -= rowH;
        }
        sheets.push_back(std::move(s));
    }
    return sheets;
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// montagem
// ---------------------------------------------------------------------------------------------------------------------

std::vector<Sheet> SheetBuilder::build(const Input &input, const Options &options)
{
    std::vector<Sheet> sheets;
    const auto views = availableViews(input);
    QVector<MarkRow> rows;
    const auto marks = assignMarks(input, &rows);
    const auto nodes = nodeMap(input);
    double w = 420, h = 297;
    paperSize(options.paper, w, h);
    ViewContext ctx {input, nodes, marks, options};
    int counter = 0;
    const double bh = titleBlockHeight(input.info);
    for (const auto &view : views) {
        const bool chosen = options.views.isEmpty() ? view.defaultOn : options.views.contains(view.id);
        if (!chosen) {
            continue;
        }
        if (view.kind == ViewKind::BarList) {
            auto list = barListSheets(input, w, h, counter, rows, view.title.toStdString());
            for (auto &s : list) {
                sheets.push_back(std::move(s));
            }
            continue;
        }
        Sheet s;
        s.width = w;
        s.height = h;
        s.name = view.title.toStdString();
        s.fileStem = QStringLiteral("%1-%2").arg(++counter, 2, 10, QLatin1Char('0')).arg(view.id).toStdString();
        const Layers l = makeLayers(s);
        double scale = 0.0;
        const Region region {25.0, 10.0 + bh + 6.0, w - 15.0, h - 16.0};
        if (view.kind == ViewKind::ConnectionDetail) {
            scale = internal::drawConnectionDetail(s, input.connections[view.connectionIndex], region.x0, region.y0, region.x1, region.y1, options.barLabels, options.scaleDenominator,
                                                   options.profiles != ProfileDetail::Axis);
        } else if (view.kind == ViewKind::View3D) {
            draw3DView(s, l, region, view, ctx, scale);
        } else {
            drawView(s, l, region, view, ctx, scale);
        }
        drawTitleBlock(s, l, input.info, view.title.toStdString(), scale > 0 ? "1:" + num(scale, 0) : "—", counter, 0, view.kind != ViewKind::ConnectionDetail);
        sheets.push_back(std::move(s));
    }
    // folhas compostas: varias vistas na mesma folha, cada uma com a sua escala
    for (const auto &layout : options.customSheets) {
        Sheet s;
        s.width = w;
        s.height = h;
        s.name = layout.name.isEmpty() ? std::string("Folha") : layout.name.toStdString();
        s.fileStem = QStringLiteral("%1-composta").arg(++counter, 2, 10, QLatin1Char('0')).toStdString();
        const Layers l = makeLayers(s);
        const Region area {25.0, 10.0 + bh + 6.0, w - 15.0, h - 16.0};
        s.areaX0 = area.x0;
        s.areaY0 = area.y0;
        s.areaX1 = area.x1;
        s.areaY1 = area.y1;
        const int n = static_cast<int>(layout.views.size());
        int autoCount = 0;
        for (const auto &p : layout.views) {
            autoCount += p.manual ? 0 : 1;
        }
        const int columns = autoCount <= 1 ? 1 : (autoCount <= 4 ? 2 : 3);
        const int rowCount = std::max(1, (autoCount + columns - 1) / columns);
        const double gap = 3.0;
        double commonScale = -1.0;
        bool mixed = false;
        int autoIndex = 0;
        for (int i = 0; i < n; ++i) {
            const auto &spot = layout.views[i];
            Region cell {};
            if (spot.manual) {
                const double cw = std::clamp(spot.w, 0.05, 1.0) * area.width(), ch = std::clamp(spot.h, 0.05, 1.0) * area.height();
                const double cx = area.x0 + std::clamp(spot.x, 0.0, 1.0) * area.width();
                const double cyTop = area.y1 - std::clamp(spot.y, 0.0, 1.0) * area.height();
                cell = Region {cx, cyTop - ch, cx + cw, cyTop};
            } else {
                const int row = autoIndex / columns, col = autoIndex % columns;
                const int inRow = (row == rowCount - 1 && autoCount % columns != 0) ? autoCount % columns : columns;   // ultima linha ocupa a largura toda
                const double cw = (area.width() - gap * (inRow - 1)) / inRow, ch = (area.height() - gap * (rowCount - 1)) / rowCount;
                const double cx = area.x0 + col * (cw + gap), cy = area.y1 - (row + 1) * ch - row * gap;
                cell = Region {cx, cy, cx + cw, cy + ch};
                ++autoIndex;
            }
            s.cells.push_back({i, cell.x0, cell.y0, cell.x1, cell.y1});
            if (n > 1) {
                s.rect({cell.x0, cell.y0}, {cell.x1, cell.y1}, l.table);
            }
            const auto &placement = spot;
            const ViewDef *def = nullptr;
            for (const auto &v : views) {
                if (v.id == placement.viewId && v.kind != ViewKind::BarList) {
                    def = &v;
                }
            }
            if (!def) {
                continue;
            }
            Options local = options;
            local.scaleDenominator = placement.scale;
            ViewContext cellCtx {input, nodes, marks, local, n > 1 || spot.manual};
            double scale = 0.0;
            if (def->kind == ViewKind::ConnectionDetail) {
                scale = internal::drawConnectionDetail(s, input.connections[def->connectionIndex], cell.x0, cell.y0, cell.x1, cell.y1, options.barLabels, placement.scale,
                                                       options.profiles != ProfileDetail::Axis);
            } else if (def->kind == ViewKind::View3D) {
                draw3DView(s, l, cell, *def, cellCtx, scale);
            } else {
                drawView(s, l, cell, *def, cellCtx, scale);
            }
            if (commonScale < 0.0) {
                commonScale = scale;
            } else if (std::abs(commonScale - scale) > 1e-6) {
                mixed = true;
            }
        }
        const std::string scaleText = mixed ? "INDICADA" : (commonScale > 0 ? "1:" + num(commonScale, 0) : "—");
        drawTitleBlock(s, l, input.info, s.name, scaleText, counter, 0, false);
        sheets.push_back(std::move(s));
    }
    // cores e espessuras escolhidas pelo usuario
    for (auto &sheet : sheets) {
        for (auto &layer : sheet.layers) {
            layer.weight *= options.lineWeightScale;
            if (!options.coloredLines) {
                layer.rgb = {0, 0, 0};
            }
        }
    }
    const std::string total = std::to_string(sheets.size());
    for (auto &sheet : sheets) {
        for (auto &entity : sheet.entities) {
            if (auto *t = std::get_if<Text>(&entity)) {
                if (t->text.size() > 4 && t->text.compare(t->text.size() - 4, 4, " / 0") == 0) {
                    t->text.replace(t->text.size() - 1, 1, total);
                }
            }
        }
    }
    return sheets;
}

} // namespace Structura::App::Services
