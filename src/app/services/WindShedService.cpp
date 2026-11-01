#include "WindShedService.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace Structura::App::Services {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kPlaneTolerance = 0.05;   // m: coluna sobre o plano da parede
constexpr double kPositionTolerance = 1e-3;

// ---------------------------------------------------------------------------------------------------------------------
// Tabela 7 (telhados de duas aguas): theta e colunas {EFI, GHJ, EG, FH} por faixa de h/b
// ---------------------------------------------------------------------------------------------------------------------
struct RoofRow {
    double theta;
    double ce[4];
};

const RoofRow kTwoPitch0[] = {   // h/b <= 1/2
    {0, {-0.8, -0.4, -0.8, -0.4}}, {5, {-0.9, -0.4, -0.8, -0.4}}, {10, {-1.2, -0.4, -0.8, -0.6}}, {15, {-1.0, -0.4, -0.8, -0.6}},
    {20, {-0.4, -0.4, -0.7, -0.6}}, {30, {0.0, -0.4, -0.7, -0.6}}, {45, {0.3, -0.5, -0.7, -0.6}}, {60, {0.7, -0.6, -0.7, -0.6}}};
const RoofRow kTwoPitch1[] = {   // 1/2 < h/b <= 3/2
    {0, {-0.8, -0.6, -1.0, -0.6}}, {5, {-0.9, -0.6, -0.9, -0.6}}, {10, {-1.1, -0.6, -0.8, -0.6}}, {15, {-1.0, -0.6, -0.8, -0.6}},
    {20, {-0.7, -0.5, -0.8, -0.6}}, {30, {-0.2, -0.5, -0.8, -0.8}}, {45, {0.2, -0.5, -0.8, -0.8}}, {60, {0.6, -0.5, -0.8, -0.8}}};
const RoofRow kTwoPitch2[] = {   // 3/2 < h/b <= 6
    {0, {-0.8, -0.6, -0.9, -0.7}}, {5, {-0.8, -0.6, -0.8, -0.8}}, {10, {-0.8, -0.6, -0.8, -0.8}}, {15, {-0.8, -0.6, -0.8, -0.8}},
    {20, {-0.8, -0.6, -0.8, -0.8}}, {30, {-1.0, -0.5, -0.8, -0.7}}, {40, {-0.2, -0.5, -0.8, -0.7}}, {50, {0.2, -0.5, -0.8, -0.7}}, {60, {0.5, -0.5, -0.8, -0.7}}};

// Tabela 8 (telhados de uma agua)
struct OnePitchRow {
    double theta;
    double ce[10];
};
const OnePitchRow kOnePitch[] = {
    {5, {-1.0, -0.5, -1.0, -0.9, -1.0, -0.5, -0.9, -1.0, -0.5, -1.0}},
    {10, {-1.0, -0.5, -1.0, -0.8, -1.0, -0.5, -0.8, -1.0, -0.4, -1.0}},
    {15, {-0.9, -0.5, -1.0, -0.7, -1.0, -0.5, -0.6, -1.0, -0.3, -1.0}},
    {20, {-0.8, -0.5, -1.0, -0.6, -0.9, -0.5, -0.5, -1.0, -0.2, -1.0}},
    {25, {-0.7, -0.5, -1.0, -0.6, -0.8, -0.5, -0.3, -0.9, -0.1, -0.9}},
    {30, {-0.5, -0.5, -1.0, -0.6, -0.8, -0.5, -0.1, -0.6, 0.0, -0.6}}};

template <typename Row, std::size_t N, typename Pick>
double interpolateTheta(const Row (&rows)[N], double theta, Pick pick)
{
    if (theta <= rows[0].theta) {
        return pick(rows[0]);
    }
    for (std::size_t i = 1; i < N; ++i) {
        if (theta <= rows[i].theta) {
            const double t = (theta - rows[i - 1].theta) / (rows[i].theta - rows[i - 1].theta);
            return pick(rows[i - 1]) + (pick(rows[i]) - pick(rows[i - 1])) * t;
        }
    }
    return pick(rows[N - 1]);
}

// Zona de coeficiente constante entre s = from e s = to (m, medidos a partir da borda de barlavento)
struct Zone {
    double from {0.0};
    double to {0.0};
    double value {0.0};
};

/// Media do coeficiente na faixa [s0, s1], ponderada pelo comprimento (as zonas cobrem [0, infinito); abaixo de 0 vale a primeira zona).
double zoneAverage(const std::vector<Zone> &zones, double s0, double s1)
{
    if (zones.empty()) {
        return 0.0;
    }
    if (s1 < s0) {
        std::swap(s0, s1);
    }
    if (s1 - s0 < 1e-9) {
        for (const Zone &z : zones) {
            if (s0 >= z.from && s0 <= z.to) {
                return z.value;
            }
        }
        return zones.back().value;
    }
    double acc = 0.0;
    for (std::size_t i = 0; i < zones.size(); ++i) {
        const double from = i == 0 ? -1e12 : zones[i].from;
        const double to = i + 1 == zones.size() ? 1e12 : zones[i].to;
        const double overlap = std::min(s1, to) - std::max(s0, from);
        if (overlap > 0.0) {
            acc += overlap * zones[i].value;
        }
    }
    return acc / (s1 - s0);
}

std::vector<Zone> single(double value)
{
    return {{0.0, 1e12, value}};
}

/// Cpi de 6.3.2.1-a/b/c e 6.3.2.2 para a direcao do vento.
std::vector<double> cpiList(const WindShedService::Parameters &p, bool transverseWind, double leewardWallCe)
{
    using M = WindShedService::CpiMode;
    switch (p.cpiMode) {
    case M::Sealed: return {-0.2, 0.0};
    case M::FourFacesPermeable: return {-0.3, 0.0};
    case M::TwoOppositeFaces: {
        // a face permeavel perpendicular ao vento: +0,2; face impermeavel perpendicular ao vento: -0,3
        const bool windHitsPermeable = (transverseWind == p.transversePermeable);
        return {windHitsPermeable ? 0.2 : -0.3};
    }
    case M::DominantWindward: return {WindShedService::cpiDominantWindward(p.openingRatio)};
    case M::DominantLeeward: return {leewardWallCe};
    case M::DominantHighSuction: return {WindShedService::cpiDominantHighSuction(p.openingRatio)};
    case M::Custom:
        if (std::abs(p.cpiFirst - p.cpiSecond) < 1e-12) {
            return {p.cpiFirst};
        }
        return {p.cpiFirst, p.cpiSecond};
    }
    return {0.0};
}

QString signedNumber(double value, int decimals = 2)
{
    return QStringLiteral("%1%2").arg(value >= 0.0 ? QStringLiteral("+") : QString()).arg(QString::number(value, 'f', decimals).replace(QLatin1Char('.'), QLatin1Char(',')));
}

struct B {
    int id {0};
    double u0 {0}, w0 {0}, z0 {0}, u1 {0}, w1 {0}, z1 {0};
    double length {0.0};
    double uMid() const { return 0.5 * (u0 + u1); }
    double wMid() const { return 0.5 * (w0 + w1); }
};

/// Faixas de influencia ao longo de um eixo: pontos medios entre posicoes vizinhas; extremos prolongados por `overhang`.
std::vector<std::pair<double, double>> stripsFor(const std::vector<double> &sortedPositions, double overhang)
{
    std::vector<std::pair<double, double>> out(sortedPositions.size());
    const std::size_t n = sortedPositions.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double lo = i == 0 ? sortedPositions[i] - overhang : 0.5 * (sortedPositions[i - 1] + sortedPositions[i]);
        const double hi = i + 1 == n ? sortedPositions[i] + overhang : 0.5 * (sortedPositions[i] + sortedPositions[i + 1]);
        out[i] = {lo, hi};
    }
    return out;
}

std::vector<double> uniquePositions(std::vector<double> values)
{
    std::sort(values.begin(), values.end());
    std::vector<double> out;
    for (const double v : values) {
        if (out.empty() || v - out.back() > kPositionTolerance * 10.0) {
            out.push_back(v);
        }
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Tabelas
// ---------------------------------------------------------------------------------------------------------------------

double WindShedService::twoPitchCe(double heightOverB, double thetaDegrees, int column)
{
    column = std::clamp(column, 0, 3);
    const auto pick = [column](const RoofRow &r) { return r.ce[column]; };
    if (heightOverB <= 0.5) {
        return interpolateTheta(kTwoPitch0, thetaDegrees, pick);
    }
    return heightOverB <= 1.5 ? interpolateTheta(kTwoPitch1, thetaDegrees, pick) : interpolateTheta(kTwoPitch2, thetaDegrees, pick);
}

double WindShedService::onePitchCe(double thetaDegrees, int column)
{
    column = std::clamp(column, 0, 9);
    return interpolateTheta(kOnePitch, thetaDegrees, [column](const OnePitchRow &r) { return r.ce[column]; });
}

std::array<double, 4> WindShedService::wallCe(double heightOverB, double aOverB, bool alpha90)
{
    return WindStaticService::wallRow(heightOverB, aOverB, alpha90);
}

double WindShedService::cpiDominantWindward(double ratio)
{
    static const double x[] = {1.0, 1.5, 2.0, 3.0, 6.0};
    static const double y[] = {0.1, 0.3, 0.5, 0.6, 0.8};
    if (ratio <= x[0]) {
        return y[0];
    }
    for (int i = 1; i < 5; ++i) {
        if (ratio <= x[i]) {
            return y[i - 1] + (y[i] - y[i - 1]) * (ratio - x[i - 1]) / (x[i] - x[i - 1]);
        }
    }
    return y[4];
}

double WindShedService::cpiDominantHighSuction(double ratio)
{
    static const double x[] = {0.25, 0.5, 0.75, 1.0, 1.5, 3.0};
    static const double y[] = {-0.4, -0.5, -0.6, -0.7, -0.8, -0.9};
    if (ratio <= x[0]) {
        return y[0];
    }
    for (int i = 1; i < 6; ++i) {
        if (ratio <= x[i]) {
            return y[i - 1] + (y[i] - y[i - 1]) * (ratio - x[i - 1]) / (x[i] - x[i - 1]);
        }
    }
    return y[5];
}

// ---------------------------------------------------------------------------------------------------------------------
// Calculo
// ---------------------------------------------------------------------------------------------------------------------

WindShedService::Result WindShedService::compute(const Parameters &parameters, const QVector<Bar> &bars)
{
    Result result;
    if (parameters.site.v0 <= 0.0 || parameters.site.s1 <= 0.0 || parameters.site.s3 <= 0.0) {
        result.warnings << QStringLiteral("V0, S1 e S3 devem ser positivos.");
        return result;
    }
    // coordenadas: u transversal a cumeeira, w ao longo da cumeeira
    const bool ridgeY = parameters.ridgeAxis == RidgeAxis::Y;
    std::vector<B> all;
    for (const Bar &bar : bars) {
        B b;
        b.id = bar.id;
        b.u0 = ridgeY ? bar.a[0] : bar.a[1];
        b.w0 = ridgeY ? bar.a[1] : bar.a[0];
        b.z0 = bar.a[2];
        b.u1 = ridgeY ? bar.b[0] : bar.b[1];
        b.w1 = ridgeY ? bar.b[1] : bar.b[0];
        b.z1 = bar.b[2];
        b.length = std::sqrt(std::pow(b.u1 - b.u0, 2) + std::pow(b.w1 - b.w0, 2) + std::pow(b.z1 - b.z0, 2));
        if (b.length > 1e-6) {
            all.push_back(b);
        }
    }
    if (all.empty()) {
        result.warnings << QStringLiteral("Nenhuma barra no modelo.");
        return result;
    }
    auto toGlobal = [&](double u, double w, double z) -> Vec3 { return ridgeY ? Vec3 {u, w, z} : Vec3 {w, u, z}; };

    double uMin = 1e300, uMax = -1e300, wMin = 1e300, wMax = -1e300, zMin = 1e300;
    for (const B &b : all) {
        uMin = std::min({uMin, b.u0, b.u1});
        uMax = std::max({uMax, b.u0, b.u1});
        wMin = std::min({wMin, b.w0, b.w1});
        wMax = std::max({wMax, b.w0, b.w1});
        zMin = std::min({zMin, b.z0, b.z1});
    }
    std::vector<B> columns, rafters;
    for (const B &b : all) {
        const double dz = std::abs(b.z1 - b.z0) / b.length;
        const double dw = std::abs(b.w1 - b.w0) / b.length;
        const double du = std::abs(b.u1 - b.u0) / b.length;
        if (dz > 0.9) {
            columns.push_back(b);
        } else if (dz > 0.017 && dz < 0.85 && dw < 0.15 && du > 0.5) {
            rafters.push_back(b);
        }
    }
    result.columns = static_cast<int>(columns.size());
    result.rafters = static_cast<int>(rafters.size());
    if (rafters.empty()) {
        result.warnings << QStringLiteral("Nenhuma barra de cobertura inclinada num plano transversal à cumeeira (%1) foi encontrada: indique o eixo da cumeeira correto.").arg(ridgeY ? QStringLiteral("Y") : QStringLiteral("X"));
        return result;
    }
    if (columns.empty()) {
        result.warnings << QStringLiteral("Nenhuma coluna (barra vertical) encontrada: só a cobertura recebe carga.");
    }

    // geometria: inclinacao, lados alto e baixo, beiral e cumeeira
    double rise = 0.0, run = 0.0, zRidge = -1e300, zEave = 1e300, uRidge = 0.0;
    int rising = 0, falling = 0;
    for (const B &r : rafters) {
        rise += std::abs(r.z1 - r.z0);
        run += std::abs(r.u1 - r.u0);
        const double slope = (r.z1 - r.z0) / (r.u1 - r.u0);
        (slope > 0 ? rising : falling) += 1;
        for (const auto &pt : {std::array<double, 2> {r.u0, r.z0}, std::array<double, 2> {r.u1, r.z1}}) {
            if (pt[1] > zRidge) {
                zRidge = pt[1];
                uRidge = pt[0];
            }
            zEave = std::min(zEave, pt[1]);
        }
    }
    Geometry& g = result.geometry;
    g.zBase = zMin;
    g.a = wMax - wMin;
    g.b = uMax - uMin;
    g.theta = std::atan(rise / std::max(run, 1e-9)) * 180.0 / kPi;
    g.twoPitch = rising > 0 && falling > 0;
    g.h = zEave - zMin;
    g.ridgeHeight = zRidge - zMin;
    g.uRidge = uRidge;
    g.highSideAtMinU = std::abs(uRidge - uMin) < std::abs(uRidge - uMax);
    if (g.a <= 0.0 || g.b <= 0.0) {
        result.warnings << QStringLiteral("Planta degenerada (a ou b nulo).");
        return result;
    }
    if (g.h <= 0.0) {
        result.warnings << QStringLiteral("A altura do beiral é nula: confira se há colunas sob a cobertura.");
        g.h = 1e-3;
    }
    if (g.a < g.b) {
        result.warnings << QStringLiteral("A cumeeira está ao longo da menor dimensão em planta (a = %1 m < b = %2 m): as Tabelas 6 a 8 admitem a ≥ b; os coeficientes foram usados mesmo assim com a/b = 1.")
                               .arg(g.a, 0, 'f', 2).arg(g.b, 0, 'f', 2);
    }
    const double aOverB = std::max(1.0, g.a / g.b);
    const double hOverB = g.h / g.b;
    if (hOverB > 6.0) {
        result.warnings << QStringLiteral("h/b = %1 excede 6, o limite das Tabelas 6 e 7: usada a última faixa.").arg(hOverB, 0, 'f', 2);
    }
    if (aOverB > 4.0) {
        result.warnings << QStringLiteral("a/b = %1 excede 4, o limite da Tabela 6: usados os coeficientes de 2 ≤ a/b ≤ 4.").arg(aOverB, 0, 'f', 2);
    }
    if (g.twoPitch && g.theta > 60.0) {
        result.warnings << QStringLiteral("Inclinação de %1° excede 60°, o limite da Tabela 7.").arg(g.theta, 0, 'f', 1);
    }
    if (!g.twoPitch) {
        if (hOverB >= 2.0) {
            result.warnings << QStringLiteral("Telhado de uma água com h/b = %1 ≥ 2: fora do campo da Tabela 8.").arg(hOverB, 0, 'f', 2);
        }
        if (g.theta < 5.0 || g.theta > 30.0) {
            result.warnings << QStringLiteral("Inclinação de %1° fora de 5° a 30° (Tabela 8): usado o valor extremo mais próximo.").arg(g.theta, 0, 'f', 1);
        }
    }
    {
        // telhados multiplos: mais de uma cumeeira (ou varias aguas na mesma direcao em vaos diferentes)
        std::vector<double> peaks;
        for (const B &r : rafters) {
            const double zt = std::max(r.z0, r.z1);
            if (zt > zRidge - 0.05) {
                peaks.push_back(r.z0 > r.z1 ? r.u0 : r.u1);
            }
        }
        const auto distinct = uniquePositions(peaks);
        if (g.twoPitch && distinct.size() > 1 && distinct.back() - distinct.front() > 0.5) {
            result.warnings << QStringLiteral("Mais de uma cumeeira à mesma altura: telhados múltiplos (Tabelas 10 a 12) não são cobertos; as águas foram tratadas como as de um telhado único.");
        }
    }

    const double eaveZ = g.h, ridgeZ = g.ridgeHeight;
    const double zRef = parameters.reference == ReferenceHeight::Ridge ? ridgeZ : (parameters.reference == ReferenceHeight::Eave ? eaveZ : 0.5 * (eaveZ + ridgeZ));
    if (ridgeZ > 80.0) {
        result.warnings << QStringLiteral("Altura acima de 80 m: usar o Anexo A para a classe da edificação; adotada a classe C.");
    }

    // posicoes de faixas: colunas por plano de parede, tesouras ao longo de w
    std::vector<B> wallU[2], wallW[2];   // [0] = plano minimo, [1] = plano maximo
    for (const B &c : columns) {
        const double u = c.uMid(), w = c.wMid();
        if (std::abs(u - uMin) <= kPlaneTolerance) {
            wallU[0].push_back(c);
        }
        if (std::abs(u - uMax) <= kPlaneTolerance) {
            wallU[1].push_back(c);
        }
        if (std::abs(w - wMin) <= kPlaneTolerance) {
            wallW[0].push_back(c);
        }
        if (std::abs(w - wMax) <= kPlaneTolerance) {
            wallW[1].push_back(c);
        }
    }

    struct Dir {
        const char *label;
        bool transverse;
        int sign;   // +1: vento no sentido crescente da coordenada (u ou w)
    };
    std::vector<Dir> dirs;
    if (parameters.transverse) {
        dirs.push_back({"T+", true, +1});
        dirs.push_back({"T-", true, -1});
    }
    if (parameters.longitudinal) {
        dirs.push_back({"L+", false, +1});
        dirs.push_back({"L-", false, -1});
    }

    const double x1 = std::min(std::max(g.b / 3.0, g.a / 4.0), 2.0 * g.h);        // comprimento das zonas de alta succao (a 0 graus)
    const double xEnd = std::min(2.0 * g.h, g.b / 2.0);                            // zonas C1D1 / C2D2 (a 90 graus)
    const auto row0 = WindStaticService::wallRow(hOverB, aOverB, false);          // A1B1, A2B2, C, D
    const auto row90 = WindStaticService::wallRow(hOverB, aOverB, true);          // A, B, C1D1, C2D2
    const double tA = std::clamp(aOverB - 1.0, 0.0, 1.0);
    const double a3b3 = row0[1] + (-0.2 - row0[1]) * tA;                           // nota 3 da Tabela 6

    for (const Dir &dir : dirs) {
        // classe e pressao dinamica
        const double frontal = std::max(dir.transverse ? g.a : g.b, ridgeZ);
        const char windClass = WindStaticService::windClass(frontal);
        const double s2 = WindStaticService::s2(parameters.site.category, ridgeZ > 80.0 ? 'C' : windClass, zRef);
        const double vk = parameters.site.v0 * parameters.site.s1 * s2 * parameters.site.s3;
        const double q = 0.613 * vk * vk / 1000.0;   // kN/m2

        // Ce das superfícies por direcao
        const double leewardWall = dir.transverse ? row90[1] : row0[3];
        const auto cpis = cpiList(parameters, dir.transverse, leewardWall);

        for (const double cpi : cpis) {
            Case c;
            c.direction = QString::fromLatin1(dir.label);
            c.q = q;
            c.cpi = cpi;
            c.windClass = windClass;
            c.name = QStringLiteral("VENTO %1 cpi=%2").arg(c.direction, signedNumber(cpi));
            c.notes << QStringLiteral("Vento %1: q = %2 kN/m² (V0 = %3 m/s, S1 = %4, S2 = %5 em z = %6 m, S3 = %7, classe %8); Cpi = %9.")
                           .arg(c.direction).arg(q, 0, 'f', 3).arg(parameters.site.v0, 0, 'f', 1).arg(parameters.site.s1, 0, 'f', 2).arg(s2, 0, 'f', 3)
                           .arg(zRef, 0, 'f', 1).arg(parameters.site.s3, 0, 'f', 2).arg(QChar::fromLatin1(windClass)).arg(signedNumber(cpi));

            std::map<int, Vec3> acc;
            auto addLoad = [&](int id, double pressureNet, double width, const Vec3 &outward) {
                Vec3 &v = acc[id];
                for (int k = 0; k < 3; ++k) {
                    v[k] += -pressureNet * width * outward[k];
                }
            };

            // distancia de barlavento ao longo de u ou de w para o sentido do vento
            auto windwardU = [&](double u) { return dir.sign > 0 ? u - uMin : uMax - u; };
            auto windwardW = [&](double w) { return dir.sign > 0 ? w - wMin : wMax - w; };

            // --- paredes ---
            // planos u = uMin / uMax: colunas com faixas ao longo de w
            for (int side = 0; side < 2; ++side) {
                const auto &wall = wallU[side];
                if (wall.empty()) {
                    continue;
                }
                std::vector<double> pos;
                for (const B &col : wall) {
                    pos.push_back(col.wMid());
                }
                const auto positions = uniquePositions(pos);
                const auto strips = stripsFor(positions, parameters.overhang);
                const double outwardSign = side == 0 ? -1.0 : 1.0;
                for (const B &col : wall) {
                    const auto it = std::lower_bound(positions.begin(), positions.end(), col.wMid() - kPositionTolerance * 10.0);
                    const std::size_t index = static_cast<std::size_t>(std::min<std::ptrdiff_t>(it - positions.begin(), static_cast<std::ptrdiff_t>(positions.size()) - 1));
                    const double s0 = strips[index].first, s1 = strips[index].second;
                    double ce = 0.0;
                    if (dir.transverse) {
                        // parede perpendicular ao vento: barlavento A / sotavento B
                        const bool windward = (dir.sign > 0) == (side == 0);
                        ce = windward ? row90[0] : row90[1];
                    } else {
                        // parede paralela ao vento: A1B1, A2B2, A3B3 ao longo do comprimento a partir da extremidade de barlavento
                        const std::vector<Zone> zones {{0.0, x1, row0[0]}, {x1, g.a / 2.0, row0[1]}, {g.a / 2.0, 1e12, a3b3}};
                        const double d0 = windwardW(s0), d1 = windwardW(s1);
                        ce = zoneAverage(zones, std::min(d0, d1), std::max(d0, d1));
                    }
                    Vec3 outward = toGlobal(outwardSign, 0.0, 0.0);
                    addLoad(col.id, q * (ce - cpi), s1 - s0, outward);
                }
            }
            // planos w = wMin / wMax (oitoes): colunas com faixas ao longo de u
            for (int side = 0; side < 2; ++side) {
                const auto &wall = wallW[side];
                if (wall.empty()) {
                    continue;
                }
                std::vector<double> pos;
                for (const B &col : wall) {
                    pos.push_back(col.uMid());
                }
                const auto positions = uniquePositions(pos);
                const auto strips = stripsFor(positions, parameters.overhang);
                const double outwardSign = side == 0 ? -1.0 : 1.0;
                for (const B &col : wall) {
                    const auto it = std::lower_bound(positions.begin(), positions.end(), col.uMid() - kPositionTolerance * 10.0);
                    const std::size_t index = static_cast<std::size_t>(std::min<std::ptrdiff_t>(it - positions.begin(), static_cast<std::ptrdiff_t>(positions.size()) - 1));
                    const double s0 = strips[index].first, s1 = strips[index].second;
                    double ce = 0.0;
                    if (dir.transverse) {
                        // parede paralela ao vento: C1D1 ate 2h ou b/2 do beiral de barlavento, C2D2 alem
                        const std::vector<Zone> zones {{0.0, xEnd, row90[2]}, {xEnd, 1e12, row90[3]}};
                        const double d0 = windwardU(s0), d1 = windwardU(s1);
                        ce = zoneAverage(zones, std::min(d0, d1), std::max(d0, d1));
                    } else {
                        const bool windward = (dir.sign > 0) == (side == 0);
                        ce = windward ? row0[2] : row0[3];
                    }
                    Vec3 outward = toGlobal(0.0, outwardSign, 0.0);
                    addLoad(col.id, q * (ce - cpi), s1 - s0, outward);
                }
            }

            // --- cobertura ---
            {
                std::vector<double> pos;
                for (const B &r : rafters) {
                    pos.push_back(r.wMid());
                }
                const auto positions = uniquePositions(pos);
                const auto strips = stripsFor(positions, parameters.overhang);
                for (const B &r : rafters) {
                    const auto it = std::lower_bound(positions.begin(), positions.end(), r.wMid() - kPositionTolerance * 10.0);
                    const std::size_t index = static_cast<std::size_t>(std::min<std::ptrdiff_t>(it - positions.begin(), static_cast<std::ptrdiff_t>(positions.size()) - 1));
                    const double s0 = strips[index].first, s1 = strips[index].second;
                    const double slopeSigned = (r.z1 - r.z0) / (r.u1 - r.u0);   // dz/du
                    const double thetaBar = std::atan(std::abs(slopeSigned)) * 180.0 / kPi;
                    double ce = 0.0;
                    if (g.twoPitch) {
                        if (dir.transverse) {
                            const bool windwardSlope = (slopeSigned * dir.sign) > 0.0;   // sobe no sentido do vento
                            ce = twoPitchCe(hOverB, thetaBar, windwardSlope ? 0 : 1);
                        } else {
                            const double fh = twoPitchCe(hOverB, thetaBar, 3);
                            const double ij = fh + (-0.2 - fh) * tA;
                            const std::vector<Zone> zones {{0.0, x1, twoPitchCe(hOverB, thetaBar, 2)}, {x1, g.a / 2.0, fh}, {g.a / 2.0, 1e12, ij}};
                            const double d0 = windwardW(s0), d1 = windwardW(s1);
                            ce = zoneAverage(zones, std::min(d0, d1), std::max(d0, d1));
                        }
                    } else if (dir.transverse) {
                        // uma agua: o lado alto da Tabela 8 (H e I) e o de barlavento para alfa = 90 (vento do lado alto para o baixo)
                        const bool windFromHigh = (dir.sign > 0) == g.highSideAtMinU;
                        const double hi90 = onePitchCe(g.theta, windFromHigh ? 0 : 8), lj90 = onePitchCe(g.theta, windFromHigh ? 1 : 9);
                        // metade de barlavento (b/2 a partir da borda de barlavento) e a outra metade
                        const std::vector<Zone> zones {{0.0, g.b / 2.0, windFromHigh ? hi90 : lj90}, {g.b / 2.0, 1e12, windFromHigh ? lj90 : hi90}};
                        const double d0 = windwardU(r.u0), d1 = windwardU(r.u1);
                        ce = zoneAverage(zones, std::min(d0, d1), std::max(d0, d1));
                    } else {
                        const double second = onePitchCe(g.theta, 5);
                        const double ij = second + (-0.2 - second) * tA;
                        const std::vector<Zone> zones {{0.0, g.b / 2.0, onePitchCe(g.theta, 4)}, {g.b / 2.0, g.a / 2.0, second}, {g.a / 2.0, 1e12, ij}};
                        const double d0 = windwardW(s0), d1 = windwardW(s1);
                        ce = zoneAverage(zones, std::min(d0, d1), std::max(d0, d1));
                    }
                    // normal exterior da agua no plano (u, z): componente z positiva
                    const double tu = (r.u1 - r.u0) / r.length, tz = (r.z1 - r.z0) / r.length;
                    double nu = tu >= 0.0 ? -tz : tz;
                    double nz = std::abs(tu);
                    const double nn = std::sqrt(nu * nu + nz * nz);
                    nu /= nn;
                    nz /= nn;
                    addLoad(r.id, q * (ce - cpi), s1 - s0, toGlobal(nu, 0.0, nz));
                }
            }

            for (const auto &entry : acc) {
                const Vec3 &v = entry.second;
                if (std::abs(v[0]) + std::abs(v[1]) + std::abs(v[2]) > 1e-9) {
                    c.loads.append({entry.first, v});
                }
            }
            // memorial dos coeficientes
            if (dir.transverse) {
                c.notes << QStringLiteral("Paredes (Tabela 6, α = 90°, h/b = %1, a/b = %2): barlavento A = %3, sotavento B = %4, oitões C1D1 = %5 (até %6 m do beiral de barlavento) e C2D2 = %7.")
                               .arg(hOverB, 0, 'f', 2).arg(aOverB, 0, 'f', 2).arg(signedNumber(row90[0]), signedNumber(row90[1]), signedNumber(row90[2])).arg(xEnd, 0, 'f', 2).arg(signedNumber(row90[3]));
                if (g.twoPitch) {
                    c.notes << QStringLiteral("Cobertura de duas águas (Tabela 7, θ = %1°): barlavento EFI = %2, sotavento GHJ = %3.")
                                   .arg(g.theta, 0, 'f', 1).arg(signedNumber(twoPitchCe(hOverB, g.theta, 0)), signedNumber(twoPitchCe(hOverB, g.theta, 1)));
                } else {
                    c.notes << QStringLiteral("Cobertura de uma água (Tabela 8, θ = %1°): metades H e I / L e J, ver a tabela para o sentido do vento.").arg(g.theta, 0, 'f', 1);
                }
            } else {
                c.notes << QStringLiteral("Paredes (Tabela 6, α = 0°, h/b = %1, a/b = %2): laterais A1B1 = %3 até %4 m, A2B2 = %5 até a/2 e A3B3 = %6; oitão de barlavento C = %7 e de sotavento D = %8.")
                               .arg(hOverB, 0, 'f', 2).arg(aOverB, 0, 'f', 2).arg(signedNumber(row0[0])).arg(x1, 0, 'f', 2).arg(signedNumber(row0[1]), signedNumber(a3b3), signedNumber(row0[2])).arg(signedNumber(row0[3]));
                if (g.twoPitch) {
                    c.notes << QStringLiteral("Cobertura de duas águas (Tabela 7, α = 0°): EG = %1 até %2 m, FH = %3 até a/2 e partes I e J = %4 (nota 3).")
                                   .arg(signedNumber(twoPitchCe(hOverB, g.theta, 2))).arg(x1, 0, 'f', 2).arg(signedNumber(twoPitchCe(hOverB, g.theta, 3)))
                                   .arg(signedNumber(twoPitchCe(hOverB, g.theta, 3) + (-0.2 - twoPitchCe(hOverB, g.theta, 3)) * tA));
                }
            }
            result.cases.append(c);
        }
    }
    if (result.cases.isEmpty()) {
        result.warnings << QStringLiteral("Nenhum caso gerado: habilite o vento transversal e/ou o longitudinal.");
    }
    result.warnings << QStringLiteral("Os coeficientes são os de forma externos (Ce) das Tabelas 6 a 8, adequados à estrutura principal; elementos de vedação e terças pedem o cpe médio.");
    return result;
}

} // namespace Structura::App::Services
