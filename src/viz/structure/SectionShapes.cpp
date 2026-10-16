#include "viz/structure/SectionShapes.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Structura::Visualization {

namespace {

using Point = SectionShape::Point;
using Loop = SectionShape::Loop;
constexpr double kPi = 3.14159265358979323846;
constexpr int kArcSteps = 12;

QString n(double v)
{
    return QString::number(v, 'f', 3);
}

// ── Mini parser de caminhos SVG (M L H V A Q Z, coordenadas absolutas) ────

bool samePoint(const Point &a, const Point &b)
{
    return std::abs(a[0] - b[0]) <= 1e-9 && std::abs(a[1] - b[1]) <= 1e-9;
}

void appendPoint(Loop &loop, const Point &p)
{
    if (loop.empty() || !samePoint(loop.back(), p)) {
        loop.push_back(p);
    }
}

void appendQuadratic(Loop &loop, const Point &current, const Point &control, const Point &end)
{
    for (int i = 1; i <= kArcSteps; ++i) {
        const double t = static_cast<double>(i) / kArcSteps;
        appendPoint(loop, {(1 - t) * (1 - t) * current[0] + 2 * (1 - t) * t * control[0] + t * t * end[0],
                           (1 - t) * (1 - t) * current[1] + 2 * (1 - t) * t * control[1] + t * t * end[1]});
    }
}

double unitAngle(double ux, double uy, double vx, double vy)
{
    return std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
}

void appendArc(Loop &loop, const Point &start, double rx, double ry, double rotationDeg, bool largeArc, bool sweep, const Point &end)
{
    rx = std::abs(rx);
    ry = std::abs(ry);
    if (rx <= 1e-12 || ry <= 1e-12 || samePoint(start, end)) {
        appendPoint(loop, end);
        return;
    }
    const double phi = std::fmod(rotationDeg, 360.0) * kPi / 180.0;
    const double cosPhi = std::cos(phi);
    const double sinPhi = std::sin(phi);
    const double dx = (start[0] - end[0]) / 2.0;
    const double dy = (start[1] - end[1]) / 2.0;
    const double xPrime = cosPhi * dx + sinPhi * dy;
    const double yPrime = -sinPhi * dx + cosPhi * dy;

    const double radiusScale = (xPrime * xPrime) / (rx * rx) + (yPrime * yPrime) / (ry * ry);
    if (radiusScale > 1.0) {
        const double scale = std::sqrt(radiusScale);
        rx *= scale;
        ry *= scale;
    }
    const double numerator = std::max(0.0, rx * rx * ry * ry - rx * rx * yPrime * yPrime - ry * ry * xPrime * xPrime);
    const double denominator = rx * rx * yPrime * yPrime + ry * ry * xPrime * xPrime;
    double coefficient = denominator > 1e-20 ? std::sqrt(numerator / denominator) : 0.0;
    if (largeArc == sweep) {
        coefficient = -coefficient;
    }
    const double cxPrime = coefficient * (rx * yPrime / ry);
    const double cyPrime = coefficient * (-ry * xPrime / rx);
    const double cx = cosPhi * cxPrime - sinPhi * cyPrime + (start[0] + end[0]) / 2.0;
    const double cy = sinPhi * cxPrime + cosPhi * cyPrime + (start[1] + end[1]) / 2.0;

    const double ux = (xPrime - cxPrime) / rx;
    const double uy = (yPrime - cyPrime) / ry;
    const double vx = (-xPrime - cxPrime) / rx;
    const double vy = (-yPrime - cyPrime) / ry;
    const double startAngle = unitAngle(1.0, 0.0, ux, uy);
    double delta = unitAngle(ux, uy, vx, vy);
    if (!sweep && delta > 0.0) {
        delta -= 2.0 * kPi;
    } else if (sweep && delta < 0.0) {
        delta += 2.0 * kPi;
    }
    const int samples = std::max(2, static_cast<int>(std::ceil(std::abs(delta) / (kPi / 2.0) * kArcSteps)));
    for (int i = 1; i <= samples; ++i) {
        const double angle = startAngle + delta * i / samples;
        const double ex = rx * std::cos(angle);
        const double ey = ry * std::sin(angle);
        appendPoint(loop, {cosPhi * ex - sinPhi * ey + cx, sinPhi * ex + cosPhi * ey + cy});
    }
}

/// Converte um caminho SVG (possivelmente com varios subcaminhos) em lacos fechados.
std::vector<Loop> svgLoops(const QString &path)
{
    static const QRegularExpression token(QStringLiteral("[A-Za-z]|[-+]?(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][-+]?\\d+)?"));
    QStringList tokens;
    auto it = token.globalMatch(path);
    while (it.hasNext()) {
        tokens << it.next().captured();
    }

    std::vector<Loop> loops;
    Loop current;
    Point cursor {0.0, 0.0};
    Point startPoint {0.0, 0.0};
    int index = 0;
    QChar command;

    auto number = [&]() {
        if (index >= tokens.size() || tokens[index].at(0).isLetter()) {
            throw std::runtime_error("Caminho SVG incompleto.");
        }
        return tokens[index++].toDouble();
    };

    while (index < tokens.size()) {
        if (tokens[index].at(0).isLetter()) {
            command = tokens[index++].at(0).toUpper();
        }
        if (command == QLatin1Char('Z')) {
            if (!current.empty()) {
                loops.push_back(current);
            }
            current.clear();
            cursor = startPoint;
            command = QChar();
            continue;
        }
        if (command == QLatin1Char('M')) {
            const double x = number();
            const double y = number();
            if (!current.empty()) {
                loops.push_back(current);
                current.clear();
            }
            cursor = {x, y};
            startPoint = cursor;
            appendPoint(current, cursor);
            command = QLatin1Char('L');
        } else if (command == QLatin1Char('L')) {
            const double x = number();
            const double y = number();
            cursor = {x, y};
            appendPoint(current, cursor);
        } else if (command == QLatin1Char('H')) {
            cursor = {number(), cursor[1]};
            appendPoint(current, cursor);
        } else if (command == QLatin1Char('V')) {
            cursor = {cursor[0], number()};
            appendPoint(current, cursor);
        } else if (command == QLatin1Char('Q')) {
            const double cx = number();
            const double cy = number();
            const double x = number();
            const double y = number();
            appendQuadratic(current, cursor, {cx, cy}, {x, y});
            cursor = {x, y};
        } else if (command == QLatin1Char('A')) {
            const double rx = number();
            const double ry = number();
            const double rotation = number();
            const bool large = number() != 0.0;
            const bool sweep = number() != 0.0;
            const double x = number();
            const double y = number();
            appendArc(current, cursor, rx, ry, rotation, large, sweep, {x, y});
            cursor = {x, y};
        } else {
            throw std::runtime_error("Comando SVG nao suportado.");
        }
    }
    if (!current.empty()) {
        loops.push_back(current);
    }
    for (Loop &loop : loops) {
        if (loop.size() > 1 && samePoint(loop.front(), loop.back())) {
            loop.pop_back();
        }
    }
    return loops;
}

// ── Geradores de caminhos (formulas dos perfis) ───────────────────────────

QString rectPath(double x, double y, double w, double h)
{
    return QStringLiteral("M %1,%2 H %3 V %4 H %1 Z").arg(n(x), n(y), n(x + w), n(y + h));
}

QString circlePath(double d)
{
    const double r = d / 2.0;
    return QStringLiteral("M %1,0 A %2,%2 0 1 1 %3,0 A %2,%2 0 1 1 %1,0 Z").arg(n(r), n(r), n(-r));
}

QString roundedRectPath(double x, double y, double w, double h, double radius)
{
    radius = std::min({std::max(0.0, radius), w / 2.0, h / 2.0});
    const double x2 = x + w;
    const double y2 = y + h;
    if (radius == 0.0) {
        return rectPath(x, y, w, h);
    }
    return QStringLiteral("M %1,%2 H %3 A %4,%4 0 0 1 %5,%6 V %7 A %4,%4 0 0 1 %3,%8 H %1 A %4,%4 0 0 1 %9,%7 V %6 A %4,%4 0 0 1 %1,%2 Z")
        .arg(n(x + radius), n(y), n(x2 - radius), n(radius), n(x2), n(y + radius), n(y2 - radius), n(y2), n(x));
}

QString wPath(double d, double bf, double tw, double tf, double r)
{
    if (std::min({d, bf, tw, tf, r}) <= 0.0) {
        throw std::runtime_error("As dimensoes do perfil W devem ser maiores que zero.");
    }
    if (d <= 2.0 * tf) {
        throw std::runtime_error("A altura do perfil W deve ser maior que duas vezes tf.");
    }
    if (bf <= tw + 2.0 * r) {
        throw std::runtime_error("A mesa do perfil W nao comporta o raio informado.");
    }
    if (d <= 2.0 * (tf + r)) {
        throw std::runtime_error("O raio informado nao comporta a altura livre da alma.");
    }
    const double hd = d / 2.0;
    const double hbf = bf / 2.0;
    const double htw = tw / 2.0;
    const double yTop = -hd;
    const double yTopInner = yTop + tf;
    const double yBottomInner = hd - tf;
    const double yBottom = hd;
    return QStringLiteral("M %1,%2 H %3 V %4 H %5 A %6,%6 0 0 0 %7,%8 V %9 A %6,%6 0 0 0 %5,%10 H %3 V %11 H %1 V %10 H %12 A %6,%6 0 0 0 %13,%14 V %8 A %6,%6 0 0 0 %12,%4 H %1 Z")
        .arg(n(-hbf), n(yTop), n(hbf), n(yTopInner), n(htw + r), n(r), n(htw), n(yTopInner + r))
        .arg(n(yBottomInner - r), n(yBottomInner), n(yBottom), n(-htw - r), n(-htw), n(yBottomInner - r));
}

QString uPath(double d, double bf, double tw, double tf, double r, double r1, double angleDeg)
{
    const double radius = r > 0.0 ? r : std::max(2.0, 1.5 * tf);
    const double tip = r1 > 0.0 ? r1 : std::max(1.5, std::min(tf, 0.8 * tf));
    if (std::min({d, bf, tw, tf, radius, tip}) <= 0.0) {
        throw std::runtime_error("As dimensoes do perfil U devem ser maiores que zero.");
    }
    if (d <= 2.0 * tf) {
        throw std::runtime_error("A altura do perfil U deve ser maior que duas vezes tf.");
    }
    if (bf <= tw + radius + tip) {
        throw std::runtime_error("A largura do perfil U nao comporta os raios informados.");
    }
    if (d <= 2.0 * (tf + radius)) {
        throw std::runtime_error("Os raios informados nao comportam a altura livre da alma.");
    }
    const double angle = angleDeg > 0.0 ? angleDeg : 2.0;
    const double halfD = d / 2.0;
    const double xOuter = -bf / 2.0;
    const double xInner = xOuter + tw;
    const double xTip = xOuter + bf;
    const double yTop = -halfD;
    const double yBottom = halfD;
    const double rise = (bf - tw - radius - tip) * std::tan(angle * kPi / 180.0);
    const double topInner = yTop + tf + rise;
    const double bottomInner = yBottom - tf - rise;
    return QStringLiteral("M %1,%2 H %3 A %4,%4 0 0 1 %5,%6 L %7,%8 Q %9,%8 %9,%10 V %11 Q %9,%12 %7,%12 L %5,%13 A %4,%4 0 0 1 %3,%14 H %1 V %2 Z")
        .arg(n(xOuter), n(yTop), n(xTip), n(tip), n(xTip - tip), n(yTop + tip), n(xInner + radius), n(topInner))
        .arg(n(xInner), n(topInner + radius), n(bottomInner - radius), n(bottomInner), n(yBottom - tip), n(yBottom));
}

QString lPath(double b, double t, double r1, double r2)
{
    const double inner = r1 > 0.0 ? r1 : t;
    const double tip = r2 > 0.0 ? r2 : std::max(0.75, 0.8 * t);
    if (std::min({b, t, inner, tip}) <= 0.0) {
        throw std::runtime_error("As dimensoes e os raios do perfil L devem ser maiores que zero.");
    }
    if (b <= t + inner || b <= t) {
        throw std::runtime_error("A largura do perfil L deve comportar a espessura e o raio interno.");
    }
    const double xLeft = -b / 2.0;
    const double xWeb = xLeft + t;
    const double xRight = xLeft + b;
    const double yTop = -b / 2.0;
    const double yBottom = b / 2.0;
    return QStringLiteral("M %1,%2 H %3 A %4,%5 0 0 1 %6,%7 V %8 A %9,%9 0 0 0 %10,%11 H %12 A %4,%5 0 0 1 %13,%14 H %1 V %2 Z")
        .arg(n(xLeft), n(yTop), n(xWeb - tip), n(tip), n(t), n(xWeb), n(yTop + t), n(yBottom - t - inner))
        .arg(n(inner), n(xWeb + inner), n(yBottom - t), n(xRight - tip), n(xRight), n(yBottom));
}

QString tPath(double d, double bf, double tw, double tf, double r1, double r2)
{
    const double inner = r1 > 0.0 ? r1 : tf;
    const double tip = r2 > 0.0 ? r2 : std::max(0.75, 0.8 * tf);
    if (std::min({d, bf, tw, tf, inner, tip}) <= 0.0) {
        throw std::runtime_error("As dimensoes e os raios do perfil T devem ser maiores que zero.");
    }
    if (d <= tf || bf <= tw + 2.0 * inner) {
        throw std::runtime_error("A geometria do perfil T nao comporta as dimensoes informadas.");
    }
    const double xl = -bf / 2.0;
    const double xr = bf / 2.0;
    const double xwl = -tw / 2.0;
    const double xwr = tw / 2.0;
    const double yTop = -d / 2.0;
    const double yBottom = d / 2.0;
    const double yFlange = yBottom - tf;
    return QStringLiteral("M %1,%2 H %3 V %4 A %5,%5 0 0 0 %6,%7 H %8 A %9,%10 0 0 1 %11,%12 H %13 A %9,%10 0 0 1 %14,%7 H %15 A %5,%5 0 0 0 %1,%4 V %2 Z")
        .arg(n(xwl), n(yTop), n(xwr), n(yFlange - inner), n(inner), n(xwr + inner), n(yFlange), n(xr - tip))
        .arg(n(tip), n(tf), n(xr), n(yBottom), n(xl), n(xl + tip), n(xwl - inner));
}

QString iPath(double d, double bf, double tw, double tf, double r1, double r2, double angleDeg)
{
    const double inner = r1 > 0.0 ? r1 : tf;
    const double tip = r2 > 0.0 ? r2 : std::max(0.75, 0.8 * tf);
    if (std::min({d, bf, tw, tf, inner, tip}) <= 0.0) {
        throw std::runtime_error("As dimensoes e os raios do perfil I devem ser maiores que zero.");
    }
    if (d <= 2.0 * tf || bf <= tw + 2.0 * inner) {
        throw std::runtime_error("A geometria do perfil I nao comporta as dimensoes informadas.");
    }
    const double angle = angleDeg > 0.0 ? angleDeg : 2.0;
    const double xl = -bf / 2.0;
    const double xr = bf / 2.0;
    const double xwl = -tw / 2.0;
    const double xwr = tw / 2.0;
    const double yTop = -d / 2.0;
    const double yBottom = d / 2.0;
    const double freeHalf = bf / 2.0 - tw / 2.0 - inner - tip;
    const double rise = freeHalf * std::tan(angle * kPi / 180.0);
    const double topInner = yTop + tf + rise;
    const double bottomInner = yBottom - tf - rise;
    return QStringLiteral("M %1,%2 H %3 A %4,%4 0 0 1 %5,%6 L %7,%8 Q %9,%8 %9,%10 V %11 Q %9,%12 %7,%12 L %5,%13 A %4,%4 0 0 1 %3,%14 H %1 A %4,%4 0 0 1 %15,%13 L %16,%12 Q %17,%12 %17,%11 V %10 Q %17,%8 %16,%8 L %15,%6 A %4,%4 0 0 1 %1,%2 Z")
        .arg(n(xl), n(yTop), n(xr), n(tip), n(xr - tip), n(yTop + tip), n(xwr + inner), n(topInner))
        .arg(n(xwr), n(topInner + inner), n(bottomInner - inner), n(bottomInner), n(yBottom - tip), n(yBottom))
        .arg(n(xl + tip), n(xwl - inner), n(xwl));
}

// Perfis formados a frio (raio interno t, externo 2t)
QString formedUPath(double d, double bf, double t)
{
    if (d <= 4.0 * t || bf <= 2.0 * t || t <= 0.0) {
        throw std::runtime_error("A secao U formada requer d > 4t e bf > 2t.");
    }
    const double x0 = -bf / 2.0;
    const double x1 = x0 + t;
    const double x2 = bf / 2.0;
    const double y0 = -d / 2.0;
    const double y1 = y0 + t;
    const double y2 = d / 2.0 - t;
    const double y3 = d / 2.0;
    const double ri = t;
    const double ro = 2.0 * t;
    return QStringLiteral("M %1,%2 H %3 V %4 H %5 Q %6,%4 %6,%7 V %8 Q %6,%9 %5,%9 H %3 V %10 H %1 Q %11,%10 %11,%12 V %13 Q %11,%2 %1,%2 Z")
        .arg(n(x0 + ro), n(y0), n(x2), n(y1), n(x1 + ri), n(x1), n(y1 + ri), n(y2 - ri))
        .arg(n(y2), n(y3), n(x0), n(y3 - ro), n(y0 + ro));
}

QString formedCPath(double d, double bf, double c, double t)
{
    if (t <= 0.0 || c <= 0.0 || d <= 2.0 * (t + c) || bf <= 3.0 * t) {
        throw std::runtime_error("A secao C formada requer d > 2(t + c), bf > 3t e c > 0.");
    }
    const double x0 = -bf / 2.0;
    const double x1 = x0 + t;
    const double x2 = bf / 2.0;
    const double y0 = -d / 2.0;
    const double y1 = y0 + t;
    const double y2 = d / 2.0 - t;
    const double y3 = d / 2.0;
    const double ri = t;
    const double ro = 2.0 * t;
    return QStringLiteral("M %1,%2 H %3 Q %4,%2 %4,%5 V %6 H %7 V %8 Q %7,%9 %10,%9 H %11 Q %12,%9 %12,%8 V %13 Q %12,%14 %11,%14 H %10 Q %7,%14 %7,%15 V %16 H %4 V %17 Q %4,%18 %3,%18 H %1 Q %19,%18 %19,%17 V %5 Q %19,%2 %1,%2 Z")
        .arg(n(x0 + ro), n(y0), n(x2 - ro), n(x2), n(y0 + ro), n(y1 + c), n(x2 - t), n(y1 + ri))
        .arg(n(y1), n(x2 - t - ri), n(x1 + ri), n(x1), n(y2 - ri), n(y2), n(y2 - ri), n(y2 - c))
        .arg(n(y3 - ro), n(y3), n(x0));
}

QString formedZPath(double d, double bf, double t)
{
    if (d <= 4.0 * t || bf <= 2.0 * t || t <= 0.0) {
        throw std::runtime_error("A secao Z formada requer d > 4t e bf > 2t.");
    }
    const double xl = -t / 2.0;
    const double xr = t / 2.0;
    const double y0 = -d / 2.0;
    const double y1 = y0 + t;
    const double y2 = d / 2.0 - t;
    const double y3 = d / 2.0;
    const double ri = t;
    const double ro = 2.0 * t;
    return QStringLiteral("M %1,%2 H %3 A %4,%4 0 0 1 %5,%6 V %7 A %8,%8 0 0 0 %9,%10 H %11 V %12 H %13 A %4,%4 0 0 1 %14,%15 V %16 A %8,%8 0 0 0 %17,%18 H %1 V %2 Z")
        .arg(n(xl - bf), n(y0), n(xr - ro), n(ro), n(xr), n(y0 + ro), n(y2 - ri), n(ri))
        .arg(n(xr + ri), n(y2), n(xr + bf), n(y3), n(xl + ro), n(xl), n(y3 - ro), n(y1 + ri))
        .arg(n(xl - ri), n(y1));
}

QString formedLPath(double d, double bf, double t)
{
    if (t <= 0.0 || d <= t || bf <= t) {
        throw std::runtime_error("A secao L formada requer d > t e bf > t.");
    }
    const double x0 = -bf / 2.0;
    const double x1 = x0 + t;
    const double x2 = bf / 2.0;
    const double y0 = -d / 2.0;
    const double y1 = d / 2.0;
    const double ri = t;
    const double ro = 2.0 * t;
    return QStringLiteral("M %1,%2 H %3 V %4 Q %3,%5 %6,%5 H %7 V %8 H %9 Q %1,%8 %1,%10 V %2 Z")
        .arg(n(x0), n(y0), n(x1), n(y1 - t - ri), n(y1 - t), n(x1 + ri), n(x2), n(y1))
        .arg(n(x0 + ro), n(y1 - ro));
}

QString formedCartolaPath(double d, double bf, double c, double t)
{
    if (t <= 0.0 || c <= 0.0 || d <= 4.0 * t || bf <= 5.0 * t) {
        throw std::runtime_error("A secao Cartola requer d > 4t, bf > 5t e c > 0.");
    }
    const double xl = -bf / 2.0;
    const double xr = bf / 2.0;
    const double y0 = -d / 2.0;
    const double y3 = d / 2.0;
    const double ri = t;
    const double ro = 2.0 * t;
    const double y1 = y0 + t;
    const double y2 = y3 - t;
    return QStringLiteral("M %1,%2 H %3 A %4,%4 0 0 0 %5,%6 V %7 A %8,%8 0 0 1 %9,%10 H %11 A %8,%8 0 0 1 %12,%7 V %6 A %4,%4 0 0 0 %13,%2 H %14 V %15 H %16 A %8,%8 0 0 1 %17,%18 V %19 A %4,%4 0 0 0 %20,%21 H %9 A %4,%4 0 0 0 %22,%19 V %23 A %8,%8 0 0 1 %24,%15 H %25 V %2 Z")
        .arg(n(xl - c), n(y2), n(xl - t), n(ri), n(xl), n(y2 - t), n(y0 + ro), n(ro))
        .arg(n(xl + 2.0 * t), n(y0), n(xr - ro), n(xr), n(xr + t), n(xr + c), n(y3), n(xr + t))
        .arg(n(xr - t), n(y3 - ro), n(y1 + t), n(xr - 2.0 * t), n(y1), n(xl + t))
        .arg(n(y3 - ro), n(xl - t), n(xl - c));
}

// Concreto (formas idealizadas)
QString concreteLPath(double b, double h, double t)
{
    const double x0 = -b / 2.0;
    const double y0 = -h / 2.0;
    const double x1 = x0 + b;
    const double y1 = y0 + h;
    return QStringLiteral("M %1,%2 H %3 V %4 H %5 V %6 H %1 Z").arg(n(x0), n(y0), n(x0 + t), n(y1 - t), n(x1), n(y1));
}

QString concreteTPath(double b, double h, double t)
{
    const double x0 = -b / 2.0;
    const double y0 = -h / 2.0;
    const double x1 = x0 + b;
    const double y1 = y0 + h;
    return QStringLiteral("M %1,%2 H %3 V %4 H %5 V %6 H %7 V %4 H %1 Z")
        .arg(n(x0), n(y0), n(x1), n(y0 + t), n(t / 2.0), n(y1), n(-t / 2.0));
}

QString concreteIPath(double b, double h, double t)
{
    const double x0 = -b / 2.0;
    const double y0 = -h / 2.0;
    const double x1 = x0 + b;
    const double y1 = y0 + h;
    return QStringLiteral("M %1,%2 H %3 V %4 H %5 V %6 H %3 V %7 H %1 V %6 H %8 V %4 H %1 Z")
        .arg(n(x0), n(y0), n(x1), n(y0 + t), n(t / 2.0), n(y1 - t), n(y1), n(-t / 2.0));
}

QString concreteUPath(double b, double h, double t)
{
    const double x0 = -b / 2.0;
    const double y0 = -h / 2.0;
    const double x1 = x0 + b;
    const double y1 = y0 + h;
    return QStringLiteral("M %1,%2 H %3 V %4 H %5 V %2 H %6 V %7 H %1 Z")
        .arg(n(x0), n(y0), n(x0 + t), n(y1 - t), n(x1 - t), n(x1), n(y1));
}

QString concretePlusPath(double b, double h, double t)
{
    const double x0 = -b / 2.0;
    const double x1 = b / 2.0;
    const double y0 = -h / 2.0;
    const double y1 = h / 2.0;
    const double xv0 = -t / 2.0;
    const double xv1 = t / 2.0;
    const double yh0 = -t / 2.0;
    const double yh1 = t / 2.0;
    return QStringLiteral("M %1,%2 H %3 V %4 H %5 V %6 H %3 V %7 H %1 V %6 H %8 V %4 H %1 Z")
        .arg(n(xv0), n(y0), n(xv1), n(yh0), n(x1), n(yh1), n(y1), n(x0));
}

// ── Utilidades de poligono ────────────────────────────────────────────────

double signedArea(const Loop &loop)
{
    double twice = 0.0;
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const Point &p = loop[i];
        const Point &q = loop[(i + 1) % loop.size()];
        twice += p[0] * q[1] - q[0] * p[1];
    }
    return twice / 2.0;
}

Point loopCentroid(const Loop &loop, double area)
{
    double cx = 0.0;
    double cy = 0.0;
    for (std::size_t i = 0; i < loop.size(); ++i) {
        const Point &p = loop[i];
        const Point &q = loop[(i + 1) % loop.size()];
        const double cross = p[0] * q[1] - q[0] * p[1];
        cx += (p[0] + q[0]) * cross;
        cy += (p[1] + q[1]) * cross;
    }
    return {cx / (6.0 * area), cy / (6.0 * area)};
}

double param(const SectionShapes::Parameters &p, const char *key, double fallback = 0.0)
{
    return p.value(QString::fromLatin1(key), fallback);
}

std::vector<Loop> rawLoops(const QString &family, const SectionShapes::Parameters &p)
{
    auto sv = [](const QString &path) { return svgLoops(path); };
    auto circleLoop = [](double d) {
        Loop loop;
        const double r = d / 2.0;
        constexpr int steps = 48;
        for (int i = 0; i < steps; ++i) {
            const double a = 2.0 * kPi * i / steps;
            loop.push_back({r * std::cos(a), r * std::sin(a)});
        }
        return loop;
    };
    auto rectLoop = [](double w, double h) {
        return Loop {{-w / 2, -h / 2}, {w / 2, -h / 2}, {w / 2, h / 2}, {-w / 2, h / 2}};
    };

    if (family == QLatin1String("W Laminado")) {
        return sv(wPath(param(p, "d"), param(p, "bf"), param(p, "tw", 6.0), param(p, "tf", 8.0), param(p, "r", 10.0)));
    }
    if (family == QLatin1String("U Laminado") || family == QLatin1String("U Inclinado Laminado")) {
        return sv(uPath(param(p, "d"), param(p, "bf"), param(p, "tw"), param(p, "tf"), param(p, "r"), param(p, "r1"),
                        param(p, "flange_angle_deg", 2.0)));
    }
    if (family == QLatin1String("L Laminado")) {
        return sv(lPath(param(p, "b"), param(p, "t"), param(p, "r1"), param(p, "r2")));
    }
    if (family == QLatin1String("I Laminado")) {
        return sv(iPath(param(p, "d"), param(p, "bf"), param(p, "tw"), param(p, "tf"), param(p, "r1"), param(p, "r2"),
                        param(p, "flange_angle_deg", 2.0)));
    }
    if (family == QLatin1String("T Laminado")) {
        return sv(tPath(param(p, "d"), param(p, "bf"), param(p, "tw"), param(p, "tf"), param(p, "r1"), param(p, "r2")));
    }
    if (family == QLatin1String("U Formado")) {
        return sv(formedUPath(param(p, "d"), param(p, "bf"), param(p, "t")));
    }
    if (family == QLatin1String("C Formado")) {
        return sv(formedCPath(param(p, "d"), param(p, "bf"), param(p, "c"), param(p, "t")));
    }
    if (family == QLatin1String("Z Formado")) {
        return sv(formedZPath(param(p, "d"), param(p, "bf"), param(p, "t")));
    }
    if (family == QLatin1String("L Formado")) {
        return sv(formedLPath(param(p, "d"), param(p, "bf"), param(p, "t")));
    }
    if (family == QLatin1String("Cartola Formado")) {
        return sv(formedCartolaPath(param(p, "d"), param(p, "bf"), param(p, "c"), param(p, "t")));
    }
    if (family == QLatin1String("Tubular Circular")) {
        const double outer = param(p, "D");
        const double inner = outer - 2.0 * param(p, "t");
        if (inner <= 0.0) {
            throw std::runtime_error("A espessura deve ser menor que o raio externo.");
        }
        return {circleLoop(outer), circleLoop(inner)};
    }
    if (family == QLatin1String("Barra Circular") || family == QLatin1String("Circular")) {
        return {circleLoop(param(p, "d"))};
    }
    if (family == QLatin1String("Tubular Quadrado") || family == QLatin1String("Tubular Retangular")) {
        const double w = param(p, "b");
        const double h = family == QLatin1String("Tubular Quadrado") ? w : param(p, "h");
        const double t = param(p, "t");
        if (w <= 4.0 * t || h <= 4.0 * t) {
            throw std::runtime_error("As dimensoes externas devem ser maiores que quatro vezes a espessura.");
        }
        auto outer = sv(roundedRectPath(-w / 2.0, -h / 2.0, w, h, 2.0 * t));
        auto inner = sv(roundedRectPath(-w / 2.0 + t, -h / 2.0 + t, w - 2.0 * t, h - 2.0 * t, t));
        outer.insert(outer.end(), inner.begin(), inner.end());
        return outer;
    }
    if (family == QLatin1String("Barra Quadrada")) {
        return {rectLoop(param(p, "b"), param(p, "b"))};
    }
    if (family == QLatin1String("Barra Retangular") || family == QLatin1String("Retangular")) {
        return {rectLoop(param(p, "b"), param(p, "h"))};
    }
    if (family == QLatin1String("Tipo L")) {
        return sv(concreteLPath(param(p, "b"), param(p, "h"), param(p, "t")));
    }
    if (family == QLatin1String("Tipo T")) {
        return sv(concreteTPath(param(p, "b"), param(p, "h"), param(p, "t")));
    }
    if (family == QLatin1String("Tipo I")) {
        return sv(concreteIPath(param(p, "b"), param(p, "h"), param(p, "t")));
    }
    if (family == QLatin1String("Tipo U")) {
        return sv(concreteUPath(param(p, "b"), param(p, "h"), param(p, "t")));
    }
    if (family == QLatin1String("Tipo +")) {
        return sv(concretePlusPath(param(p, "b"), param(p, "h"), param(p, "t")));
    }
    if (family == QLatin1String("Retangular Vazado")) {
        const double b = param(p, "b");
        const double h = param(p, "h");
        const double t = param(p, "t");
        if (b <= 2.0 * t || h <= 2.0 * t) {
            throw std::runtime_error("A espessura deve ser menor que metade das dimensoes.");
        }
        return {rectLoop(b, h), rectLoop(b - 2.0 * t, h - 2.0 * t)};
    }
    if (family == QLatin1String("Circular Vazado")) {
        const double inner = param(p, "d") - 2.0 * param(p, "t");
        if (inner <= 0.0) {
            throw std::runtime_error("A espessura deve ser menor que o raio externo.");
        }
        return {circleLoop(param(p, "d")), circleLoop(inner)};
    }
    throw std::runtime_error("Familia de perfil desconhecida.");
}

// Parametro helpers para a tabela de familias
SectionParameter P(const char *key, const char *label, double value, bool optional = false)
{
    return {QString::fromLatin1(key), QString::fromUtf8(label), value, optional};
}

} // namespace

const QVector<SectionFamily> &SectionShapes::families()
{
    static const QVector<SectionFamily> list = {
        {QStringLiteral("W Laminado"), QStringLiteral("Aco"),
         {P("d", "Altura d (mm)", 310), P("bf", "Mesa bf (mm)", 165), P("tw", "Alma tw (mm)", 5.8), P("tf", "Mesa tf (mm)", 9.7), P("r", "Raio r (mm)", 10)}},
        {QStringLiteral("I Laminado"), QStringLiteral("Aco"),
         {P("d", "Altura d (mm)", 203), P("bf", "Mesa bf (mm)", 101), P("tw", "Alma tw (mm)", 5.8), P("tf", "Mesa tf (mm)", 10.8),
          P("r1", "Raio interno r1 (mm)", 0, true), P("r2", "Raio da ponta r2 (mm)", 0, true), P("flange_angle_deg", "Inclinacao da mesa (graus)", 0, true)}},
        {QStringLiteral("U Laminado"), QStringLiteral("Aco"),
         {P("d", "Altura d (mm)", 203), P("bf", "Mesa bf (mm)", 57), P("tw", "Alma tw (mm)", 5.6), P("tf", "Mesa tf (mm)", 9.5),
          P("r", "Raio da alma r (mm)", 0, true), P("r1", "Raio da ponta r1 (mm)", 0, true), P("flange_angle_deg", "Inclinacao da mesa (graus)", 0, true)}},
        {QStringLiteral("U Inclinado Laminado"), QStringLiteral("Aco"),
         {P("d", "Altura d (mm)", 203), P("bf", "Mesa bf (mm)", 57), P("tw", "Alma tw (mm)", 5.6), P("tf", "Mesa tf (mm)", 9.5),
          P("r", "Raio da alma r (mm)", 0, true), P("r1", "Raio da ponta r1 (mm)", 0, true), P("flange_angle_deg", "Inclinacao da mesa (graus)", 8, true)}},
        {QStringLiteral("L Laminado"), QStringLiteral("Aco"),
         {P("b", "Aba b (mm)", 76), P("t", "Espessura t (mm)", 6.35), P("r1", "Raio interno r1 (mm)", 0, true), P("r2", "Raio da ponta r2 (mm)", 0, true)}},
        {QStringLiteral("T Laminado"), QStringLiteral("Aco"),
         {P("d", "Altura d (mm)", 150), P("bf", "Mesa bf (mm)", 150), P("tw", "Alma tw (mm)", 8), P("tf", "Mesa tf (mm)", 10),
          P("r1", "Raio interno r1 (mm)", 0, true), P("r2", "Raio da ponta r2 (mm)", 0, true)}},
        {QStringLiteral("U Formado"), QStringLiteral("Aco"), {P("d", "Altura d (mm)", 150), P("bf", "Mesa bf (mm)", 60), P("t", "Espessura t (mm)", 3)}},
        {QStringLiteral("C Formado"), QStringLiteral("Aco"),
         {P("d", "Altura d (mm)", 150), P("bf", "Mesa bf (mm)", 60), P("c", "Enrijecedor c (mm)", 20), P("t", "Espessura t (mm)", 3)}},
        {QStringLiteral("Z Formado"), QStringLiteral("Aco"), {P("d", "Altura d (mm)", 150), P("bf", "Mesa bf (mm)", 60), P("t", "Espessura t (mm)", 3)}},
        {QStringLiteral("L Formado"), QStringLiteral("Aco"), {P("d", "Altura d (mm)", 60), P("bf", "Aba bf (mm)", 60), P("t", "Espessura t (mm)", 3)}},
        {QStringLiteral("Cartola Formado"), QStringLiteral("Aco"),
         {P("d", "Altura d (mm)", 40), P("bf", "Largura bf (mm)", 80), P("c", "Aba c (mm)", 20), P("t", "Espessura t (mm)", 3)}},
        {QStringLiteral("Tubular Circular"), QStringLiteral("Aco"), {P("D", "Diametro D (mm)", 114.3), P("t", "Espessura t (mm)", 4.5)}},
        {QStringLiteral("Tubular Quadrado"), QStringLiteral("Aco"), {P("b", "Lado b (mm)", 100), P("t", "Espessura t (mm)", 5)}},
        {QStringLiteral("Tubular Retangular"), QStringLiteral("Aco"), {P("b", "Largura b (mm)", 100), P("h", "Altura h (mm)", 60), P("t", "Espessura t (mm)", 4)}},
        {QStringLiteral("Barra Circular"), QStringLiteral("Aco"), {P("d", "Diametro d (mm)", 20)}},
        {QStringLiteral("Barra Quadrada"), QStringLiteral("Aco"), {P("b", "Lado b (mm)", 20)}},
        {QStringLiteral("Barra Retangular"), QStringLiteral("Aco"), {P("b", "Largura b (mm)", 20), P("h", "Altura h (mm)", 40)}},
        {QStringLiteral("Retangular"), QStringLiteral("Concreto"), {P("b", "Largura b (mm)", 200), P("h", "Altura h (mm)", 400)}},
        {QStringLiteral("Circular"), QStringLiteral("Concreto"), {P("d", "Diametro d (mm)", 300)}},
        {QStringLiteral("Tipo L"), QStringLiteral("Concreto"), {P("b", "Largura b (mm)", 400), P("h", "Altura h (mm)", 600), P("t", "Espessura t (mm)", 150)}},
        {QStringLiteral("Tipo T"), QStringLiteral("Concreto"), {P("b", "Largura b (mm)", 600), P("h", "Altura h (mm)", 500), P("t", "Espessura t (mm)", 150)}},
        {QStringLiteral("Tipo I"), QStringLiteral("Concreto"), {P("b", "Largura b (mm)", 600), P("h", "Altura h (mm)", 800), P("t", "Espessura t (mm)", 150)}},
        {QStringLiteral("Tipo U"), QStringLiteral("Concreto"), {P("b", "Largura b (mm)", 500), P("h", "Altura h (mm)", 500), P("t", "Espessura t (mm)", 100)}},
        {QStringLiteral("Tipo +"), QStringLiteral("Concreto"), {P("b", "Largura b (mm)", 600), P("h", "Altura h (mm)", 600), P("t", "Espessura t (mm)", 150)}},
        {QStringLiteral("Retangular Vazado"), QStringLiteral("Concreto"), {P("b", "Largura b (mm)", 400), P("h", "Altura h (mm)", 600), P("t", "Espessura t (mm)", 60)}},
        {QStringLiteral("Circular Vazado"), QStringLiteral("Concreto"), {P("d", "Diametro d (mm)", 400), P("t", "Espessura t (mm)", 60)}},
    };
    return list;
}

const SectionFamily *SectionShapes::findFamily(const QString &name)
{
    for (const SectionFamily &family : families()) {
        if (family.name == name) {
            return &family;
        }
    }
    return nullptr;
}

std::optional<SectionShape> SectionShapes::build(const QString &family, const Parameters &parameters, bool mirrored, QString *error)
{
    try {
        std::vector<Loop> loops = rawLoops(family, parameters);
        if (loops.empty()) {
            throw std::runtime_error("Contorno vazio.");
        }
        for (Loop &loop : loops) {
            if (loop.size() < 3) {
                throw std::runtime_error("O contorno da secao precisa de pelo menos tres pontos.");
            }
        }

        // Centroide ponderado (contorno externo menos furos).
        double totalArea = 0.0;
        double sumX = 0.0;
        double sumY = 0.0;
        for (std::size_t i = 0; i < loops.size(); ++i) {
            const double area = signedArea(loops[i]);
            if (std::abs(area) <= 1e-12) {
                throw std::runtime_error("O contorno possui area nula.");
            }
            const Point centroid = loopCentroid(loops[i], area);
            const double weight = (i == 0 ? 1.0 : -1.0) * std::abs(area);
            totalArea += weight;
            sumX += weight * centroid[0];
            sumY += weight * centroid[1];
        }
        if (totalArea <= 1e-12) {
            throw std::runtime_error("A secao possui area util nula.");
        }

        SectionShape shape;
        shape.centroid = {sumX / totalArea, sumY / totalArea};
        bool first = true;
        for (const Loop &loop : loops) {
            Loop centered;
            centered.reserve(loop.size());
            for (const Point &point : loop) {
                // SVG tem y para baixo: inverte para que a altura cresca para cima (eixo local y).
                double a = point[0] - shape.centroid[0];
                const double b = -(point[1] - shape.centroid[1]);
                if (mirrored) {
                    a = -a;
                }
                centered.push_back({a, b});
                if (first) {
                    shape.bounds = {a, a, b, b};
                    first = false;
                }
                shape.bounds[0] = std::min(shape.bounds[0], a);
                shape.bounds[1] = std::max(shape.bounds[1], a);
                shape.bounds[2] = std::min(shape.bounds[2], b);
                shape.bounds[3] = std::max(shape.bounds[3], b);
            }
            shape.loops.push_back(std::move(centered));
        }
        return shape;
    } catch (const std::exception &ex) {
        if (error) {
            *error = QString::fromUtf8(ex.what());
        }
        return std::nullopt;
    }
}

namespace {

/// Torcao de um retangulo macico (Saint-Venant, formula aproximada), lados a >= b.
double rectangleTorsion(double a, double b)
{
    if (a < b) {
        std::swap(a, b);
    }
    return a * b * b * b * (1.0 / 3.0 - 0.21 * (b / a) * (1.0 - std::pow(b, 4) / (12.0 * std::pow(a, 4))));
}

} // namespace

std::optional<SectionProperties> SectionShapes::properties(const QString &family, const Parameters &p, QString *error)
{
    const auto shape = build(family, p, false, error);
    if (!shape) {
        return std::nullopt;
    }

    double area = 0.0;
    double iz = 0.0; // integral de b^2 dA
    double iy = 0.0; // integral de a^2 dA
    for (std::size_t i = 0; i < shape->loops.size(); ++i) {
        const Loop &loop = shape->loops[i];
        const double sign = i == 0 ? 1.0 : -1.0;
        const double orientation = signedArea(loop) >= 0.0 ? 1.0 : -1.0;
        for (std::size_t k = 0; k < loop.size(); ++k) {
            const Point &p0 = loop[k];
            const Point &p1 = loop[(k + 1) % loop.size()];
            const double cross = p0[0] * p1[1] - p1[0] * p0[1];
            area += sign * orientation * cross / 2.0;
            iz += sign * orientation * cross * (p0[1] * p0[1] + p0[1] * p1[1] + p1[1] * p1[1]) / 12.0;
            iy += sign * orientation * cross * (p0[0] * p0[0] + p0[0] * p1[0] + p1[0] * p1[0]) / 12.0;
        }
    }

    // Constante de torcao aproximada por familia (pre-dimensionamento).
    double j = 0.0;
    const bool open = family.contains(QLatin1String("Laminado")) || family.contains(QLatin1String("Formado"));
    if (family == QLatin1String("Tubular Circular") || family == QLatin1String("Circular Vazado")) {
        const double outer = family == QLatin1String("Tubular Circular") ? param(p, "D") : param(p, "d");
        const double inner = outer - 2.0 * param(p, "t");
        j = kPi / 32.0 * (std::pow(outer, 4) - std::pow(inner, 4));
    } else if (family == QLatin1String("Barra Circular") || family == QLatin1String("Circular")) {
        j = kPi * std::pow(param(p, "d"), 4) / 32.0;
    } else if (family == QLatin1String("Tubular Quadrado") || family == QLatin1String("Tubular Retangular")
               || family == QLatin1String("Retangular Vazado")) {
        const double w = param(p, "b");
        const double h = family == QLatin1String("Tubular Quadrado") ? w : param(p, "h");
        const double t = param(p, "t");
        const double am = (w - t) * (h - t);
        const double perimeter = 2.0 * ((w - t) + (h - t));
        j = 4.0 * am * am * t / perimeter;
    } else if (family == QLatin1String("Barra Quadrada")) {
        j = rectangleTorsion(param(p, "b"), param(p, "b"));
    } else if (family == QLatin1String("Barra Retangular") || family == QLatin1String("Retangular")) {
        j = rectangleTorsion(param(p, "b"), param(p, "h"));
    } else if (family == QLatin1String("W Laminado") || family == QLatin1String("I Laminado")) {
        const double tw = param(p, "tw", 6.0);
        const double tf = param(p, "tf", 8.0);
        j = (2.0 * param(p, "bf") * std::pow(tf, 3) + (param(p, "d") - tf) * std::pow(tw, 3)) / 3.0;
    } else if (family == QLatin1String("U Laminado") || family == QLatin1String("U Inclinado Laminado")) {
        j = (2.0 * param(p, "bf") * std::pow(param(p, "tf"), 3) + (param(p, "d") - param(p, "tf")) * std::pow(param(p, "tw"), 3)) / 3.0;
    } else if (family == QLatin1String("T Laminado")) {
        j = (param(p, "bf") * std::pow(param(p, "tf"), 3) + (param(p, "d") - param(p, "tf") / 2.0) * std::pow(param(p, "tw"), 3)) / 3.0;
    } else if (family == QLatin1String("L Laminado")) {
        j = 2.0 * (param(p, "b") - param(p, "t") / 2.0) * std::pow(param(p, "t"), 3) / 3.0;
    } else if (open) {
        // Formados: soma dos retangulos finos ao longo da linha media.
        const double t = param(p, "t");
        double length = 0.0;
        if (family == QLatin1String("U Formado")) {
            length = param(p, "d") + 2.0 * param(p, "bf") - 2.0 * t;
        } else if (family == QLatin1String("C Formado")) {
            length = param(p, "d") + 2.0 * param(p, "bf") + 2.0 * param(p, "c") - 4.0 * t;
        } else if (family == QLatin1String("Z Formado")) {
            length = param(p, "d") + 2.0 * param(p, "bf") - 2.0 * t;
        } else if (family == QLatin1String("L Formado")) {
            length = param(p, "d") + param(p, "bf") - t;
        } else {
            length = param(p, "bf") + 2.0 * param(p, "d") + 2.0 * param(p, "c") - 4.0 * t;
        }
        j = length * std::pow(t, 3) / 3.0;
    } else {
        // Concreto em T/L/I/U/+: soma dos retangulos componentes.
        const double b = param(p, "b");
        const double h = param(p, "h");
        const double t = param(p, "t");
        if (family == QLatin1String("Tipo T")) {
            j = rectangleTorsion(b, t) + rectangleTorsion(h - t, t);
        } else if (family == QLatin1String("Tipo I")) {
            j = 2.0 * rectangleTorsion(b, t) + rectangleTorsion(h - 2.0 * t, t);
        } else if (family == QLatin1String("Tipo L")) {
            j = rectangleTorsion(h, t) + rectangleTorsion(b - t, t);
        } else if (family == QLatin1String("Tipo U")) {
            j = 2.0 * rectangleTorsion(h, t) + rectangleTorsion(b - 2.0 * t, t);
        } else {
            j = rectangleTorsion(b, t) + rectangleTorsion(h, t);
        }
    }

    constexpr double mm = 1e-3;
    SectionProperties result;
    result.area = std::abs(area) * mm * mm;
    result.iz = std::abs(iz) * std::pow(mm, 4);
    result.iy = std::abs(iy) * std::pow(mm, 4);
    result.j = std::abs(j) * std::pow(mm, 4);
    return result;
}

} // namespace Structura::Visualization
