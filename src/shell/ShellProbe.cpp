#include "shell/ShellProbe.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace Structura::Shell {

std::array<double, 4> q4Shape(double xi, double eta)
{
    return {0.25 * (1.0 - xi) * (1.0 - eta), 0.25 * (1.0 + xi) * (1.0 - eta), 0.25 * (1.0 + xi) * (1.0 + eta), 0.25 * (1.0 - xi) * (1.0 + eta)};
}

double q4Interpolate(const std::array<double, 4> &nodalValues, double xi, double eta)
{
    const auto n = q4Shape(xi, eta);
    double v = 0.0;
    for (std::size_t i = 0; i < 4; ++i) {
        if (std::abs(n[i]) < 1e-12) {
            continue;   // no sem influencia neste ponto (ex.: sobre outro canto ou aresta)
        }
        if (!std::isfinite(nodalValues[i])) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        v += n[i] * nodalValues[i];
    }
    return v;
}

bool q4NaturalCoordinates(const std::array<std::array<double, 3>, 4> &c, const std::array<double, 3> &p, double &xi, double &eta, double *distance, bool *outside)
{
    for (const auto &q : c) {
        for (const double v : q) {
            if (!std::isfinite(v)) {
                return false;
            }
        }
    }
    for (const double v : p) {
        if (!std::isfinite(v)) {
            return false;
        }
    }
    // escala do elemento (maior diagonal) para criterios relativos
    double size = 0.0;
    for (int a = 0; a < 4; ++a) {
        for (int b = a + 1; b < 4; ++b) {
            double d2 = 0.0;
            for (int k = 0; k < 3; ++k) {
                const double d = c[static_cast<std::size_t>(a)][static_cast<std::size_t>(k)] - c[static_cast<std::size_t>(b)][static_cast<std::size_t>(k)];
                d2 += d * d;
            }
            size = std::max(size, std::sqrt(d2));
        }
    }
    if (!(size > 0.0)) {
        return false;
    }
    double x = 0.0, y = 0.0;
    bool converged = false;
    for (int it = 0; it < 50; ++it) {
        const auto n = q4Shape(x, y);
        // derivadas das funcoes de forma
        const std::array<double, 4> dx {-0.25 * (1.0 - y), 0.25 * (1.0 - y), 0.25 * (1.0 + y), -0.25 * (1.0 + y)};
        const std::array<double, 4> dy {-0.25 * (1.0 - x), -0.25 * (1.0 + x), 0.25 * (1.0 + x), 0.25 * (1.0 - x)};
        double r[3] = {0, 0, 0}, jx[3] = {0, 0, 0}, jy[3] = {0, 0, 0};
        for (std::size_t i = 0; i < 4; ++i) {
            for (std::size_t k = 0; k < 3; ++k) {
                r[k] += n[i] * c[i][k];
                jx[k] += dx[i] * c[i][k];
                jy[k] += dy[i] * c[i][k];
            }
        }
        for (std::size_t k = 0; k < 3; ++k) {
            r[k] -= p[k];
        }
        // equacoes normais (J^T J) d = -J^T r
        const double a11 = jx[0] * jx[0] + jx[1] * jx[1] + jx[2] * jx[2];
        const double a12 = jx[0] * jy[0] + jx[1] * jy[1] + jx[2] * jy[2];
        const double a22 = jy[0] * jy[0] + jy[1] * jy[1] + jy[2] * jy[2];
        const double b1 = -(jx[0] * r[0] + jx[1] * r[1] + jx[2] * r[2]);
        const double b2 = -(jy[0] * r[0] + jy[1] * r[1] + jy[2] * r[2]);
        const double det = a11 * a22 - a12 * a12;
        if (!(det > 1e-24 * size * size * size * size)) {
            return false;   // elemento degenerado nesta posicao
        }
        double dxi = (b1 * a22 - b2 * a12) / det;
        double deta = (a11 * b2 - a12 * b1) / det;
        // passo limitado: evita saltos longe do elemento
        const double step = std::max(std::abs(dxi), std::abs(deta));
        if (step > 1.0) {
            dxi /= step;
            deta /= step;
        }
        x += dxi;
        y += deta;
        if (std::abs(dxi) < 1e-13 && std::abs(deta) < 1e-13) {
            converged = true;
            break;
        }
    }
    if (!converged) {
        return false;
    }
    constexpr double kEdgeTolerance = 1e-6;
    const bool out = std::abs(x) > 1.0 + kEdgeTolerance || std::abs(y) > 1.0 + kEdgeTolerance;
    xi = std::clamp(x, -1.0, 1.0);
    eta = std::clamp(y, -1.0, 1.0);
    if (outside) {
        *outside = out;
    }
    if (distance) {
        const auto n = q4Shape(xi, eta);
        double d2 = 0.0;
        for (std::size_t k = 0; k < 3; ++k) {
            double s = 0.0;
            for (std::size_t i = 0; i < 4; ++i) {
                s += n[i] * c[i][k];
            }
            d2 += (s - p[k]) * (s - p[k]);
        }
        *distance = std::sqrt(d2);
    }
    return true;
}

ProbeHit probeQuad(const RenderMesh &mesh, int quad, const std::array<double, 3> &viewPoint)
{
    ProbeHit hit;
    if (quad < 0 || static_cast<std::size_t>(quad) * 4 + 3 >= mesh.points.size()) {
        return hit;
    }
    const std::size_t base = static_cast<std::size_t>(quad) * 4;
    auto load = [&](const std::vector<std::array<float, 3>> &pts) {
        std::array<std::array<double, 3>, 4> c {};
        for (std::size_t i = 0; i < 4; ++i) {
            for (std::size_t k = 0; k < 3; ++k) {
                c[i][k] = static_cast<double>(pts[base + i][k]);
            }
        }
        return c;
    };
    const auto view = load(mesh.points);
    double xi = 0.0, eta = 0.0, distance = 0.0;
    bool outside = false;
    if (!q4NaturalCoordinates(view, viewPoint, xi, eta, &distance, &outside)) {
        return hit;
    }
    hit.valid = true;
    hit.quad = quad;
    hit.xi = xi;
    hit.eta = eta;
    hit.outside = outside;
    hit.distance = distance;
    hit.element = static_cast<std::size_t>(quad) < mesh.quadElement.size() ? mesh.quadElement[static_cast<std::size_t>(quad)] : quad;
    hit.objectIndex = static_cast<std::size_t>(quad) < mesh.quadObject.size() ? mesh.quadObject[static_cast<std::size_t>(quad)] : -1;
    if (hit.objectIndex >= 0) {
        const auto o = static_cast<std::size_t>(hit.objectIndex);
        hit.objectId = o < mesh.objectIds.size() ? mesh.objectIds[o] : -1;
        if (o < mesh.objectNames.size()) {
            hit.objectName = mesh.objectNames[o];
        }
    }
    hit.title = mesh.title;
    hit.unit = mesh.unit;
    hit.smoothed = mesh.smoothed;

    // posicao global do ponto do elemento: na geometria sem deformada, se a vista esta deformada
    const double upm = mesh.unitsPerMeter > 0.0 ? mesh.unitsPerMeter : 1.0;
    const auto shape = q4Shape(xi, eta);
    const bool hasRest = mesh.restPoints.size() == mesh.points.size();
    const auto rest = hasRest ? load(mesh.restPoints) : view;
    hit.deformed = hasRest;
    for (std::size_t k = 0; k < 3; ++k) {
        double pr = 0.0, pv = 0.0;
        for (std::size_t i = 0; i < 4; ++i) {
            pr += shape[i] * rest[i][k];
            pv += shape[i] * view[i][k];
        }
        hit.position[k] = pr / upm;
        hit.viewPosition[k] = pv / upm;
    }

    if (mesh.hasValues && mesh.scalars.size() == mesh.points.size() && mesh.solverScalars.size() == mesh.points.size()) {
        const std::array<double, 4> shown {mesh.scalars[base], mesh.scalars[base + 1], mesh.scalars[base + 2], mesh.scalars[base + 3]};
        const std::array<double, 4> own {mesh.solverScalars[base], mesh.solverScalars[base + 1], mesh.solverScalars[base + 2], mesh.solverScalars[base + 3]};
        const double v = q4Interpolate(shown, xi, eta);
        const double s = q4Interpolate(own, xi, eta);
        hit.hasValue = std::isfinite(v);
        hit.value = hit.hasValue ? v : 0.0;
        hit.hasSolverValue = std::isfinite(s);
        hit.solverValue = hit.hasSolverValue ? s : 0.0;
    }
    return hit;
}

std::string describeProbe(const ProbeHit &hit)
{
    if (!hit.valid) {
        return "Sem elemento sob o cursor.";
    }
    char buffer[512];
    std::string name = hit.objectName.empty() ? std::string("-") : hit.objectName;
    if (!hit.hasValue) {
        std::snprintf(buffer, sizeof(buffer), "Elemento %d (%s) | %s: sem valor neste ponto", hit.element, name.c_str(), hit.title.empty() ? "grandeza" : hit.title.c_str());
    } else {
        std::snprintf(buffer, sizeof(buffer), "Elemento %d (%s) | %s = %.5g %s | x = %.3f, y = %.3f, z = %.3f m", hit.element, name.c_str(), hit.title.c_str(), hit.value, hit.unit.c_str(),
                      hit.position[0], hit.position[1], hit.position[2]);
    }
    return buffer;
}

} // namespace Structura::Shell
