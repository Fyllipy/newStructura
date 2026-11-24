#include "shell/ShellSupports.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace Structura::Shell {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

SupportResolution resolveSupports(const Model &model, const Mesh &mesh)
{
    SupportResolution out;
    // arestas livres: aresta usada por um unico elemento
    std::map<std::pair<int, int>, int> edgeCount;
    std::map<std::pair<int, int>, int> edgeObject;
    for (const MeshQuad &q : mesh.quads) {
        for (int i = 0; i < 4; ++i) {
            const int a = q.n[static_cast<std::size_t>(i)], b = q.n[static_cast<std::size_t>((i + 1) % 4)];
            const auto key = std::make_pair(std::min(a, b), std::max(a, b));
            ++edgeCount[key];
            edgeObject[key] = q.object;
        }
    }
    std::vector<std::set<int>> nodeObjects(mesh.nodes.size());
    for (const MeshQuad &q : mesh.quads) {
        for (const int n : q.n) {
            nodeObjects[static_cast<std::size_t>(n)].insert(q.object);
        }
    }
    for (std::size_t index = 0; index < model.supports.size(); ++index) {
        const Support &s = model.supports[index];
        std::vector<int> selected;
        auto belongs = [&](int node) {
            if (s.object == 0) {
                return true;
            }
            for (const int obj : nodeObjects[static_cast<std::size_t>(node)]) {
                if (model.objects[static_cast<std::size_t>(obj)].id == s.object) {
                    return true;
                }
            }
            return false;
        };
        auto limited = [&](int node) {
            if (s.limitAxis < 0 || s.limitAxis > 2) {
                return true;
            }
            const double c = mesh.nodes[static_cast<std::size_t>(node)][static_cast<std::size_t>(s.limitAxis)];
            return s.limitBelow ? c <= s.limitValue + s.tol : c >= s.limitValue - s.tol;
        };
        if (s.selector == Support::Plane) {
            const std::size_t axis = static_cast<std::size_t>(std::clamp(s.axis, 0, 2));
            for (std::size_t n = 0; n < mesh.nodes.size(); ++n) {
                if (std::abs(mesh.nodes[n][axis] - s.value) <= s.tol && belongs(static_cast<int>(n)) && limited(static_cast<int>(n))) {
                    selected.push_back(static_cast<int>(n));
                }
            }
            if (s.legs > 0 && !selected.empty()) {
                // centro do anel e angulos dos apoios pontuais
                Vec3 center {0.0, 0.0, 0.0};
                for (const int n : selected) {
                    for (int c = 0; c < 3; ++c) {
                        center[static_cast<std::size_t>(c)] += mesh.nodes[static_cast<std::size_t>(n)][static_cast<std::size_t>(c)] / static_cast<double>(selected.size());
                    }
                }
                const std::size_t a1 = axis == 0 ? 1 : 0, a2 = axis == 2 ? 1 : 2;
                std::vector<int> chosen;
                for (int leg = 0; leg < s.legs; ++leg) {
                    const double angle = (s.legStart + 360.0 * leg / s.legs) * kPi / 180.0;
                    int best = -1;
                    double bestDiff = 1e9;
                    for (const int n : selected) {
                        const Vec3 &p = mesh.nodes[static_cast<std::size_t>(n)];
                        const double a = std::atan2(p[a2] - center[a2], p[a1] - center[a1]);
                        double diff = std::abs(std::remainder(a - angle, 2.0 * kPi));
                        // prefere os nos de maior raio (borda do anel)
                        diff += 1e-6 * (1.0 - std::hypot(p[a1] - center[a1], p[a2] - center[a2]));
                        if (diff < bestDiff) {
                            bestDiff = diff;
                            best = n;
                        }
                    }
                    if (best >= 0) {
                        chosen.push_back(best);
                    }
                }
                selected = chosen;
            }
        } else if (s.selector == Support::FreeEdge) {
            std::set<int> nodes;
            for (const auto &[edge, count] : edgeCount) {
                if (count != 1) {
                    continue;
                }
                if (s.object != 0 && model.objects[static_cast<std::size_t>(edgeObject[edge])].id != s.object) {
                    continue;
                }
                nodes.insert(edge.first);
                nodes.insert(edge.second);
            }
            for (const int n : nodes) {
                if (limited(n)) {
                    selected.push_back(n);
                }
            }
        } else if (s.selector == Support::Nodes) {
            // pontos guardados por coordenadas: cada um procura o no da malha atual dentro de `tol`; os que sumiram sao relatados
            std::set<int> found;
            int missing = 0;
            for (const auto &p : s.points) {
                int best = -1;
                double bestDist = s.tol;
                for (std::size_t n = 0; n < mesh.nodes.size(); ++n) {
                    const Vec3 &q = mesh.nodes[n];
                    const double d = std::sqrt((q[0] - p[0]) * (q[0] - p[0]) + (q[1] - p[1]) * (q[1] - p[1]) + (q[2] - p[2]) * (q[2] - p[2]));
                    if (d <= bestDist && belongs(static_cast<int>(n))) {
                        bestDist = d;
                        best = static_cast<int>(n);
                    }
                }
                if (best < 0) {
                    ++missing;
                } else {
                    found.insert(best);
                }
            }
            selected.assign(found.begin(), found.end());
            if (missing > 0) {
                out.warnings.push_back("Apoio por nos: " + std::to_string(missing) + " ponto(s) nao coincidem com nos da malha atual (a malha foi refeita?).");
                if (selected.empty()) {
                    continue;   // o aviso especifico basta
                }
            }
        } else {
            int best = -1;
            double bestDist = 1e300;
            for (std::size_t n = 0; n < mesh.nodes.size(); ++n) {
                const Vec3 &p = mesh.nodes[n];
                const double d = std::hypot(std::hypot(p[0] - s.value, p[1] - s.value2), p[2] - s.value3);
                if (d < bestDist && belongs(static_cast<int>(n))) {
                    bestDist = d;
                    best = static_cast<int>(n);
                }
            }
            if (best >= 0) {
                selected.push_back(best);
            }
        }
        if (selected.empty()) {
            out.warnings.push_back("Um apoio nao encontrou nos (confira a posicao ou a tolerancia).");
            continue;
        }
        out.supports.push_back({static_cast<int>(index), std::move(selected)});
    }
    return out;
}

bool classifySupport(const Support &s, SupportKind &kind)
{
    if (s.springPerArea > 0.0) {
        kind = s.compressionOnly ? SupportKind::CompressionOnly : SupportKind::Spring;
        return true;
    }
    int translations = 0, rotations = 0;
    for (int i = 0; i < 3; ++i) {
        translations += s.fixed[static_cast<std::size_t>(i)] ? 1 : 0;
        rotations += s.fixed[static_cast<std::size_t>(i + 3)] ? 1 : 0;
    }
    if (translations == 0 && rotations == 0) {
        return false;
    }
    if (translations == 3 && rotations == 3) {
        kind = SupportKind::Fixed;
    } else if (translations == 3 && rotations == 0) {
        kind = SupportKind::Pinned;
    } else if (translations >= 1 && translations <= 2 && rotations == 0) {
        kind = SupportKind::Roller;
    } else {
        kind = SupportKind::Partial;
    }
    return true;
}

const char *supportKindName(SupportKind kind)
{
    switch (kind) {
    case SupportKind::Fixed:
        return "Engaste";
    case SupportKind::Pinned:
        return "Articulado";
    case SupportKind::Roller:
        return "Apoio movel";
    case SupportKind::Partial:
        return "Restricao parcial";
    case SupportKind::Spring:
        return "Mola";
    case SupportKind::CompressionOnly:
        return "So compressao";
    }
    return "";
}

std::vector<SupportMark> supportMarks(const Model &model, const Mesh &mesh, const SupportResolution &resolution)
{
    std::map<int, SupportMark> byNode;
    std::map<int, SupportKind> springKind;
    for (const ResolvedSupport &r : resolution.supports) {
        const Support &s = model.supports[static_cast<std::size_t>(r.support)];
        SupportKind own;
        if (!classifySupport(s, own)) {
            continue;
        }
        const bool spring = own == SupportKind::Spring || own == SupportKind::CompressionOnly;
        for (const int n : r.nodes) {
            auto it = byNode.find(n);
            if (it == byNode.end()) {
                SupportMark m;
                m.node = n;
                m.position = mesh.nodes[static_cast<std::size_t>(n)];
                it = byNode.emplace(n, m).first;
            }
            SupportMark &m = it->second;
            if (spring) {
                // mola: translacoes elasticas (so-compressao: apenas na normal); nao e "restricao rigida"
                springKind[n] = (springKind.count(n) && springKind[n] == SupportKind::Spring) ? SupportKind::Spring : own;
                m.normal = s.springNormal;
            } else {
                for (int c = 0; c < 6; ++c) {
                    m.fixed[static_cast<std::size_t>(c)] = m.fixed[static_cast<std::size_t>(c)] || s.fixed[static_cast<std::size_t>(c)] ? 1 : 0;
                }
            }
        }
    }
    std::vector<SupportMark> marks;
    for (auto &[node, m] : byNode) {
        const auto sp = springKind.find(node);
        if (sp != springKind.end()) {
            m.kind = sp->second;   // a mola prevalece na exibicao
        } else {
            Support probe;
            probe.fixed = m.fixed;
            SupportKind k = SupportKind::Fixed;
            if (!classifySupport(probe, k)) {
                continue;
            }
            m.kind = k;
        }
        marks.push_back(m);
    }
    return marks;
}

} // namespace Structura::Shell
