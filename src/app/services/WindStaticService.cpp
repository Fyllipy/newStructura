#include "WindStaticService.h"

#include <algorithm>
#include <cmath>

namespace Structura::App::Services {

namespace {

// Tabela 6: coeficientes de forma externos Ce de paredes de edificacoes de planta retangular a x b (b a menor dimensao).
// Cada linha: alfa = 0 (A1B1, A2B2, C, D) e alfa = 90 (A, B, C1D1, C2D2) e o cpe medio.
struct WallRow {
    double alpha0[4];
    double alpha90[4];
    double cpeMean;
};

// [faixa de h/b][faixa de a/b: 0 = 1 a 3/2; 1 = 2 a 4]
const WallRow kWalls[3][2] = {
    {{{-0.8, -0.5, 0.7, -0.4}, {0.7, -0.4, -0.8, -0.4}, -0.9}, {{-0.8, -0.4, 0.7, -0.3}, {0.7, -0.5, -0.9, -0.5}, -1.0}},   // h/b <= 1/2
    {{{-0.9, -0.5, 0.7, -0.5}, {0.7, -0.5, -0.9, -0.5}, -1.1}, {{-0.9, -0.4, 0.7, -0.3}, {0.7, -0.6, -0.9, -0.5}, -1.1}},   // 1/2 < h/b <= 3/2
    {{{-1.0, -0.6, 0.8, -0.6}, {0.8, -0.6, -1.0, -0.6}, -1.2}, {{-1.0, -0.5, 0.8, -0.3}, {0.8, -0.6, -1.0, -0.6}, -1.2}},   // 3/2 < h/b <= 6
};

} // namespace

char WindStaticService::windClass(double largestFrontalDimension)
{
    if (largestFrontalDimension <= 20.0) {
        return 'A';
    }
    return largestFrontalDimension <= 50.0 ? 'B' : 'C';
}

double WindStaticService::s2(int category, char cls, double z)
{
    // Tabela 1 (bm, p por classe A/B/C) e Tabela 2 (Fr)
    static const double bm[5][3] = {{1.10, 1.11, 1.12}, {1.00, 1.00, 1.00}, {0.94, 0.94, 0.93}, {0.86, 0.85, 0.84}, {0.74, 0.73, 0.71}};
    static const double p[5][3] = {{0.06, 0.065, 0.07}, {0.085, 0.09, 0.10}, {0.10, 0.105, 0.115}, {0.12, 0.125, 0.135}, {0.15, 0.16, 0.175}};
    static const double fr[3] = {1.00, 0.98, 0.95};
    static const double zg[5] = {250.0, 300.0, 350.0, 420.0, 500.0};
    const int c = std::clamp(category, 1, 5) - 1;
    const int k = cls == 'A' ? 0 : (cls == 'C' ? 2 : 1);
    // S2 = bm Fr (z/10)^p ate zg; abaixo de 5 m vale o de 5 m e, na categoria V, o de 10 m (5.3.3)
    const double zEffective = std::min(std::max(z, c == 4 ? 10.0 : 5.0), zg[c]);
    return bm[c][k] * fr[k] * std::pow(zEffective / 10.0, p[c][k]);
}

void WindStaticService::wallCoefficients(double heightOverB, double aOverB, double alphaDegrees, double &windward, double &leeward)
{
    const int band = heightOverB <= 0.5 ? 0 : (heightOverB <= 1.5 ? 1 : 2);
    auto pick = [&](const WallRow &row, double &w, double &l) {
        if (alphaDegrees < 45.0) {
            w = row.alpha0[2];   // C
            l = row.alpha0[3];   // D
        } else {
            w = row.alpha90[0];  // A
            l = row.alpha90[1];  // B
        }
    };
    double w0, l0, w1, l1;
    pick(kWalls[band][0], w0, l0);
    pick(kWalls[band][1], w1, l1);
    // a/b <= 3/2: primeira linha; >= 2: segunda; entre 3/2 e 2: interpolacao linear (nota 2)
    const double t = std::clamp((aOverB - 1.5) / 0.5, 0.0, 1.0);
    windward = w0 + (w1 - w0) * t;
    leeward = l0 + (l1 - l0) * t;
}

double WindStaticService::s1Topographic(double thetaDegrees, double z, double dt)
{
    constexpr double kPi = 3.14159265358979323846;
    if (dt <= 0.0 || thetaDegrees <= 3.0) {
        return 1.0;
    }
    // g(theta): S1 = 1 + (2,5 - z/dt) g(theta)
    auto g = [&](double theta) {
        if (theta <= 3.0) {
            return 0.0;
        }
        if (theta < 6.0) {
            return std::tan(3.0 * kPi / 180.0) * (theta - 3.0) / 3.0;
        }
        if (theta <= 17.0) {
            return std::tan((theta - 3.0) * kPi / 180.0);
        }
        if (theta < 45.0) {
            const double g17 = std::tan(14.0 * kPi / 180.0);
            return g17 + (0.31 - g17) * (theta - 17.0) / 28.0;
        }
        return 0.31;
    };
    return std::max(1.0, 1.0 + (2.5 - z / dt) * g(thetaDegrees));
}

std::array<double, 4> WindStaticService::wallRow(double heightOverB, double aOverB, bool alpha90)
{
    const int band = heightOverB <= 0.5 ? 0 : (heightOverB <= 1.5 ? 1 : 2);
    const double t = std::clamp((aOverB - 1.5) / 0.5, 0.0, 1.0);
    std::array<double, 4> out {};
    for (int i = 0; i < 4; ++i) {
        const double first = alpha90 ? kWalls[band][0].alpha90[i] : kWalls[band][0].alpha0[i];
        const double second = alpha90 ? kWalls[band][1].alpha90[i] : kWalls[band][1].alpha0[i];
        out[i] = first + (second - first) * t;
    }
    return out;
}

WindStaticService::Result WindStaticService::compute(const Parameters &parameters, const Building &building, int directions, int eccentricity)
{
    Result result;
    const double lx = building.xMax - building.xMin, ly = building.yMax - building.yMin;
    if (lx <= 0.0 || ly <= 0.0 || building.levels.size() < 2) {
        result.warnings << QStringLiteral("Informe a planta (largura e comprimento) e ao menos dois níveis (base e um pavimento).");
        return result;
    }
    result.a = std::max(lx, ly);
    result.b = std::min(lx, ly);
    result.h = building.levels.last() - building.levels.first();
    if (result.h <= 0.0) {
        result.warnings << QStringLiteral("A altura da edificação é nula.");
        return result;
    }
    const double hOverB = result.h / result.b, aOverB = result.a / result.b;
    if (hOverB > 6.0) {
        result.warnings << QStringLiteral("h/b = %1 excede 6, o limite da Tabela 6: foram usados os coeficientes da última faixa.").arg(hOverB, 0, 'f', 2);
    }
    if (aOverB > 4.0) {
        result.warnings << QStringLiteral("a/b = %1 excede 4, o limite da Tabela 6: foram usados os coeficientes de 2 ≤ a/b ≤ 4.").arg(aOverB, 0, 'f', 2);
    }
    if (result.h > 80.0) {
        result.warnings << QStringLiteral("Altura acima de 80 m: a classe da edificação deve seguir o Anexo A (intervalo de tempo da rajada); foi usada a classe C.");
    }
    if (parameters.s1 <= 0.0 || parameters.s3 <= 0.0 || parameters.v0 <= 0.0) {
        result.warnings << QStringLiteral("V0, S1 e S3 devem ser positivos.");
        return result;
    }

    const char *names[4] = {"+X", "-X", "+Y", "-Y"};
    for (int direction = 0; direction < 4; ++direction) {
        if (((directions >> direction) & 1) == 0) {
            continue;
        }
        const bool alongX = direction <= MinusX;
        const double width = alongX ? ly : lx;   // largura da fachada de barlavento (perpendicular ao vento)
        const double alpha = width <= result.b + 1e-9 ? 0.0 : 90.0;
        double windward = 0.0, leeward = 0.0;
        wallCoefficients(hOverB, aOverB, alpha, windward, leeward);
        if (parameters.highTurbulence) {
            leeward *= 2.0 / 3.0;   // 6.1.3.2 a)
        }
        const char cls = result.h > 80.0 ? 'C' : windClass(std::max(width, result.h));
        const double e = (parameters.neighbourhood ? 0.15 : 0.075) * width;
        QVector<int> variants;
        if (eccentricity == 0 || eccentricity == 1) {
            variants.append(0);
        }
        if (eccentricity >= 1) {
            variants.append(1);
            variants.append(-1);
        }
        for (const int variant : variants) {
            Case c;
            c.direction = static_cast<Direction>(direction);
            c.eccentricity = variant;
            c.eccentricityDistance = variant * e;
            c.windClass = cls;
            c.alpha = alpha;
            c.windwardCoefficient = windward;
            c.leewardCoefficient = leeward;
            c.faceWidth = width;
            c.name = QStringLiteral("VENTO %1").arg(QLatin1String(names[direction]));
            if (variant != 0) {
                c.name += variant > 0 ? QStringLiteral(" e+") : QStringLiteral(" e-");
            }
            const int n = static_cast<int>(building.levels.size());
            for (int i = 1; i < n; ++i) {
                FloorLoad f;
                f.z = building.levels[i] - building.levels.first();
                const double lower = 0.5 * (building.levels[i] - building.levels[i - 1]);
                const double upper = i + 1 < n ? 0.5 * (building.levels[i + 1] - building.levels[i]) : parameters.parapet;
                f.tributary = lower + upper;
                f.s2 = s2(parameters.category, cls, f.z);
                const double vk = parameters.v0 * parameters.s1 * f.s2 * parameters.s3;
                f.q = 0.613 * vk * vk / 1000.0;
                // forca na direcao do vento: pressao no barlavento (Ce > 0 empurra) mais succao no sotavento (Ce < 0 puxa no mesmo sentido)
                f.force = f.q * (windward - leeward) * width * f.tributary;
                c.floors.append(f);
            }
            result.cases.append(c);
        }
    }
    return result;
}

} // namespace Structura::App::Services
