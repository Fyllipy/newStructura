#include "shell/ShellMeshQuality.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

namespace Structura::Shell {

namespace {

constexpr double kPi = 3.14159265358979323846;

Vec3 sub(const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double dot(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3 &a, const Vec3 &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double norm(const Vec3 &a) { return std::sqrt(dot(a, a)); }

QuadQuality evaluateQuad(const Mesh &mesh, const MeshQuad &q, const MeshQualityLimits &lim)
{
    QuadQuality out;
    std::array<Vec3, 4> p;
    for (std::size_t i = 0; i < 4; ++i) {
        p[i] = mesh.nodes[static_cast<std::size_t>(q.n[i])];
    }
    // plano medio: normal pelas diagonais, origem no centroide
    Vec3 normal = cross(sub(p[2], p[0]), sub(p[3], p[1]));
    const double doubleArea = norm(normal);
    out.area = 0.5 * doubleArea;
    double maxEdge = 0.0, minEdge = 1e300;
    for (std::size_t i = 0; i < 4; ++i) {
        const double l = norm(sub(p[(i + 1) % 4], p[i]));
        maxEdge = std::max(maxEdge, l);
        minEdge = std::min(minEdge, l);
    }
    if (!(maxEdge > 0.0) || !std::isfinite(doubleArea) || out.area <= lim.degenerateArea * maxEdge * maxEdge) {
        out.level = QualityLevel::Error;
        out.reason = "elemento degenerado (area nula ou nos coincidentes)";
        out.aspect = minEdge > 0.0 ? maxEdge / minEdge : 1e300;
        out.minAngle = 0.0;
        out.maxAngle = 180.0;
        out.minJacobian = 0.0;
        return out;
    }
    normal = {normal[0] / doubleArea, normal[1] / doubleArea, normal[2] / doubleArea};
    out.aspect = minEdge > 0.0 ? maxEdge / minEdge : 1e300;
    Vec3 centroid {0, 0, 0};
    for (const Vec3 &v : p) {
        for (std::size_t k = 0; k < 3; ++k) {
            centroid[k] += 0.25 * v[k];
        }
    }
    for (const Vec3 &v : p) {
        out.warp = std::max(out.warp, std::abs(dot(sub(v, centroid), normal)));
    }
    const double meanEdge = std::sqrt(out.area);   // lado equivalente h
    out.warpOverSize = out.warp / meanEdge;
    out.warpOverThickness = q.thickness > 0.0 ? out.warp / q.thickness : 0.0;
    // angulos e jacobiano por canto, projetados no plano medio; sen(angulo) com sinal = (e_prox x e_ant).n / (|e_prox||e_ant|) = detJ normalizado
    out.minAngle = 180.0;
    out.maxAngle = 0.0;
    out.minJacobian = 1e300;
    for (std::size_t i = 0; i < 4; ++i) {
        const Vec3 next = sub(p[(i + 1) % 4], p[i]), prev = sub(p[(i + 3) % 4], p[i]);
        const auto project = [&](const Vec3 &v) {
            const double d = dot(v, normal);
            return Vec3 {v[0] - d * normal[0], v[1] - d * normal[1], v[2] - d * normal[2]};
        };
        const Vec3 a = project(next), b = project(prev);
        const double la = norm(a), lb = norm(b);
        const double c = std::clamp(dot(a, b) / (la * lb), -1.0, 1.0);
        // nos anti-horarios vistos da normal: em um canto convexo (proximo x anterior).n > 0 (quadrado (0,0),(1,0),(1,1),(0,1): +1)
        const double convexSine = dot(cross(a, b), normal) / (la * lb);
        const double angle = std::acos(c) * 180.0 / kPi;
        const double effective = convexSine > 0.0 ? angle : 360.0 - angle;   // canto reentrante: > 180 graus
        out.minAngle = std::min(out.minAngle, effective);
        out.maxAngle = std::max(out.maxAngle, effective);
        out.minJacobian = std::min(out.minJacobian, convexSine);
    }
    auto raise = [&](QualityLevel level, const std::string &why) {
        if (level > out.level) {
            out.level = level;
            out.reason = why;
        }
    };
    if (out.minJacobian <= 0.0 || out.maxAngle >= 180.0) {
        raise(QualityLevel::Error, "elemento nao convexo ou invertido (jacobiano <= 0 num canto)");
    }
    if (out.minAngle < lim.angleErrorLow || out.maxAngle > lim.angleErrorHigh) {
        raise(QualityLevel::Error, "angulo interno fora de [1, 179] graus (elemento quase degenerado)");
    }
    if (out.aspect > lim.aspectError) {
        raise(QualityLevel::Error, "razao de aspecto acima de 1000 (elemento quase colapsado)");
    }
    if (out.warpOverSize > lim.warpErrorOverSize || out.warpOverThickness > lim.warpErrorOverThickness) {
        raise(QualityLevel::Error, "empenamento excessivo (o desvio fora do plano e descartado na rigidez do elemento)");
    }
    if (out.minAngle < lim.angleWarnLow || out.maxAngle > lim.angleWarnHigh) {
        raise(QualityLevel::Warning, "angulo interno fora de [45, 135] graus (distorcao angular; ex.: elementos triangulares do polo de domos, ~4-6 % de erro local nos esforcos de membrana)");
    }
    if (out.aspect > lim.aspectWarn) {
        raise(QualityLevel::Warning, "razao de aspecto acima de 32");
    }
    if (out.warpOverSize > lim.warpWarnOverSize || out.warpOverThickness > lim.warpWarnOverThickness) {
        raise(QualityLevel::Warning, "empenamento do elemento acima do recomendado");
    }
    return out;
}

std::string worstList(const std::vector<std::size_t> &indices)
{
    std::ostringstream s;
    for (std::size_t i = 0; i < indices.size() && i < 3; ++i) {
        s << (i ? ", " : "") << indices[i] + 1;
    }
    return s.str();
}

} // namespace

MeshQualityReport analyzeMeshQuality(const Mesh &mesh, const Model &model, const MeshQualityLimits &limits)
{
    MeshQualityReport report;
    report.quads.reserve(mesh.quads.size());
    std::map<std::string, std::vector<std::size_t>> byReason[3];
    for (std::size_t e = 0; e < mesh.quads.size(); ++e) {
        QuadQuality quality = evaluateQuad(mesh, mesh.quads[e], limits);
        report.minAngle = std::min(report.minAngle, quality.minAngle);
        report.maxAngle = std::max(report.maxAngle, quality.maxAngle);
        report.maxAspect = std::max(report.maxAspect, quality.aspect);
        report.maxWarpOverSize = std::max(report.maxWarpOverSize, quality.warpOverSize);
        switch (quality.level) {
        case QualityLevel::Acceptable: ++report.acceptable; break;
        case QualityLevel::Warning: ++report.warnings; break;
        case QualityLevel::Error: ++report.errors; break;
        }
        if (quality.level != QualityLevel::Acceptable) {
            byReason[static_cast<int>(quality.level)][quality.reason].push_back(e);
        }
        report.quads.push_back(std::move(quality));
    }
    // arestas compartilhadas: orientacao consistente e angulo entre normais
    struct EdgeUse {
        std::size_t quad;
        bool forward;   // percorrida de min para max
    };
    std::map<std::pair<int, int>, std::vector<EdgeUse>> edges;
    for (std::size_t e = 0; e < mesh.quads.size(); ++e) {
        const MeshQuad &q = mesh.quads[e];
        for (std::size_t i = 0; i < 4; ++i) {
            const int a = q.n[i], b = q.n[(i + 1) % 4];
            edges[{std::min(a, b), std::max(a, b)}].push_back({e, a < b});
        }
    }
    std::vector<std::size_t> flipped, sharp;
    for (const auto &[key, uses] : edges) {
        if (uses.size() != 2) {
            continue;   // borda livre ou juncao de 3+ elementos (placas em T): sem regra de orientacao
        }
        const MeshQuad &qa = mesh.quads[uses[0].quad], &qb = mesh.quads[uses[1].quad];
        if (qa.object != qb.object) {
            continue;
        }
        if (uses[0].forward == uses[1].forward) {
            ++report.flippedNormalEdges;
            flipped.push_back(uses[0].quad);
            continue;
        }
        if (qa.zone != qb.zone) {
            continue;
        }
        const Vec3 na = quadNormal(mesh, qa), nb = quadNormal(mesh, qb);
        const double angle = std::acos(std::clamp(dot(na, nb), -1.0, 1.0)) * 180.0 / kPi;
        report.maxDihedral = std::max(report.maxDihedral, angle);
        if (angle > limits.dihedralWarn) {
            ++report.sharpDihedralEdges;
            sharp.push_back(uses[0].quad);
        }
    }
    (void)model;
    auto line = [&](const char *prefix, const std::string &reason, const std::vector<std::size_t> &list) {
        std::ostringstream s;
        s << prefix << reason << ": " << list.size() << " elemento(s) (ex.: " << worstList(list) << ")";
        report.messages.push_back(s.str());
    };
    for (const auto &[reason, list] : byReason[2]) {
        line("ERRO de malha - ", reason, list);
    }
    if (report.flippedNormalEdges > 0) {
        std::ostringstream s;
        s << "ERRO de malha - normais opostas entre elementos vizinhos do mesmo objeto: " << report.flippedNormalEdges << " aresta(s) (ex.: elemento " << flipped.front() + 1 << ")";
        report.messages.push_back(s.str());
    }
    for (const auto &[reason, list] : byReason[1]) {
        line("Aviso de malha - ", reason, list);
    }
    if (report.sharpDihedralEdges > 0) {
        std::ostringstream s;
        s << "Aviso de malha - angulo entre normais vizinhas acima de 45 graus (maximo " << static_cast<int>(report.maxDihedral + 0.5) << "): "
          << report.sharpDihedralEdges << " aresta(s) (ex.: elemento " << sharp.front() + 1 << "); refine a malha em superficies curvas";
        report.messages.push_back(s.str());
    }
    return report;
}

} // namespace Structura::Shell
