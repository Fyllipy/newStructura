#include "EffectiveLengthService.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>

namespace Structura::App::Services {

namespace {

using Vec3 = std::array<double, 3>;

Vec3 sub(const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 add(const Vec3 &a, const Vec3 &b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 mul(const Vec3 &a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3 &a, const Vec3 &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double norm(const Vec3 &a) { return std::sqrt(dot(a, a)); }

constexpr int kSamples = 24;
constexpr double kPi = 3.14159265358979323846;

struct Geometry {
    Vec3 pi {}, pj {};
    Vec3 x {}, y {}, z {};
    double length {0.0};
    std::size_t oi {0}, oj {0};
};

// integral do quadrado da curvatura do deslocamento transversal (planos locais y e z) para a forma `shape`: a energia de flexao e EI vezes isto
std::array<double, 2> bendingEnergy(const Geometry &g, const std::vector<double> &shape)
{
    const Vec3 ui {shape[g.oi], shape[g.oi + 1], shape[g.oi + 2]}, uj {shape[g.oj], shape[g.oj + 1], shape[g.oj + 2]};
    const Vec3 ti {shape[g.oi + 3], shape[g.oi + 4], shape[g.oi + 5]}, tj {shape[g.oj + 3], shape[g.oj + 4], shape[g.oj + 5]};
    const Vec3 du = sub(uj, ui);
    const Vec3 chordSlope = mul(sub(du, mul(g.x, dot(du, g.x))), 1.0 / g.length);
    const Vec3 si = sub(cross(ti, g.x), chordSlope);
    const Vec3 sj = sub(cross(tj, g.x), chordSlope);
    std::array<double, 2> energy {0.0, 0.0};
    std::array<double, kSamples + 1> vyAll {}, vzAll {};
    const double dx = g.length / kSamples;
    for (int s = 0; s <= kSamples; ++s) {
        const double t = static_cast<double>(s) / kSamples;
        // deslocamento transversal total: interpolacao das extremidades mais a flecha pelas rotacoes (Hermite)
        const double h1 = t - 2.0 * t * t + t * t * t;
        const double h2 = -t * t + t * t * t;
        const Vec3 v = add(add(mul(ui, 1.0 - t), mul(uj, t)), add(mul(si, g.length * h1), mul(sj, g.length * h2)));
        vyAll[static_cast<std::size_t>(s)] = dot(v, g.y);
        vzAll[static_cast<std::size_t>(s)] = dot(v, g.z);
    }
    for (int s = 1; s < kSamples; ++s) {
        const std::size_t u = static_cast<std::size_t>(s);
        const double cy = (vyAll[u + 1] - 2.0 * vyAll[u] + vyAll[u - 1]) / (dx * dx);
        const double cz = (vzAll[u + 1] - 2.0 * vzAll[u] + vzAll[u - 1]) / (dx * dx);
        energy[0] += cy * cy * dx;
        energy[1] += cz * cz * dx;
    }
    return energy;
}

} // namespace

EffectiveLengthService::Output EffectiveLengthService::compute(const SolverBridge::ModelData &model, const SolverBridge::Result &result, const Options &options)
{
    Output out;
    if (result.bucklingFactors.empty() || result.bucklingModes.empty()) {
        out.notes << QStringLiteral("Nenhum modo de flambagem: não há barras comprimidas na combinação.");
        return out;
    }
    out.firstLambda = result.bucklingFactors.front();
    if (out.firstLambda > 1.0) {
        out.amplification = 1.0 / (1.0 - 1.0 / out.firstLambda);
        out.swayClass = out.amplification <= 1.10 ? 0 : (out.amplification <= 1.40 ? 1 : 2);
    } else {
        out.swayClass = 2;
        out.notes << QStringLiteral("λ1 < 1: a estrutura é instável sob as cargas da combinação.");
    }

    std::unordered_map<int, std::size_t> nodeIndex;
    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        nodeIndex[model.nodes[i].id] = i;
    }
    std::unordered_map<int, const SolverBridge::MaterialData *> materials;
    for (const auto &m : model.materials) {
        materials[m.id] = &m;
    }
    std::unordered_map<int, const SolverBridge::SectionData *> sections;
    for (const auto &s : model.sections) {
        sections[s.id] = &s;
    }
    std::unordered_map<int, const SolverBridge::MemberResult *> memberResults;
    for (const auto &r : result.memberResults) {
        memberResults[r.memberId] = &r;
    }

    struct Item {
        const SolverBridge::MemberData *member {nullptr};
        Geometry geometry;
        double axial {0.0};
        double ei[2] {0.0, 0.0};   // [plano y (flexao em torno de z local), plano z]
    };
    std::vector<Item> items;
    double maxCompression = 0.0;
    std::unordered_map<int, int> degree;   // barras por no
    for (const auto &m : model.members) {
        ++degree[m.nodeI];
        ++degree[m.nodeJ];
    }
    for (const auto &member : model.members) {
        const auto ni = nodeIndex.find(member.nodeI), nj = nodeIndex.find(member.nodeJ);
        const auto rr = memberResults.find(member.id);
        const auto mat = materials.find(member.materialId);
        const auto sec = sections.find(member.sectionId);
        if (ni == nodeIndex.end() || nj == nodeIndex.end() || rr == memberResults.end() || mat == materials.end() || sec == sections.end() || member.taper) {
            continue;
        }
        const auto &a = model.nodes[ni->second], &b = model.nodes[nj->second];
        Item item;
        item.member = &member;
        item.geometry.pi = {a.x, a.y, a.z};
        item.geometry.pj = {b.x, b.y, b.z};
        const Vec3 chord = sub(item.geometry.pj, item.geometry.pi);
        item.geometry.length = norm(chord);
        if (item.geometry.length < 1e-9) {
            continue;
        }
        item.geometry.x = mul(chord, 1.0 / item.geometry.length);
        const auto &aux = rr->second->auxK;
        const Vec3 vAux {aux[0], aux[1], aux[2]};
        Vec3 z = cross(item.geometry.x, vAux);
        const double zl = norm(z);
        if (zl < 1e-9) {
            continue;
        }
        item.geometry.z = mul(z, 1.0 / zl);
        item.geometry.y = cross(item.geometry.z, item.geometry.x);
        item.geometry.oi = ni->second * 6;
        item.geometry.oj = nj->second * 6;
        const auto &lf = rr->second->localForces;
        item.axial = 0.5 * (lf[6] - lf[0]);
        item.ei[0] = mat->second->e * sec->second->iz;
        item.ei[1] = mat->second->e * sec->second->iy;
        if (item.axial < 0.0) {
            maxCompression = std::max(maxCompression, -item.axial);
        }
        items.push_back(item);
    }
    if (maxCompression <= 0.0) {
        out.notes << QStringLiteral("Nenhuma barra comprimida.");
        return out;
    }

    // grupos: barras colineares ligadas por nos de grau 2 (colunas divididas em trechos)
    std::vector<int> parent(items.size());
    for (std::size_t i = 0; i < parent.size(); ++i) {
        parent[i] = static_cast<int>(i);
    }
    auto root = [&](int i) {
        while (parent[static_cast<std::size_t>(i)] != i) {
            i = parent[static_cast<std::size_t>(i)] = parent[static_cast<std::size_t>(parent[static_cast<std::size_t>(i)])];
        }
        return i;
    };
    for (std::size_t i = 0; i < items.size(); ++i) {
        for (std::size_t j = i + 1; j < items.size(); ++j) {
            const auto &a = *items[i].member, &b = *items[j].member;
            const int shared = (a.nodeI == b.nodeI || a.nodeI == b.nodeJ) ? a.nodeI : ((a.nodeJ == b.nodeI || a.nodeJ == b.nodeJ) ? a.nodeJ : -1);
            if (shared >= 0 && degree[shared] == 2 && std::abs(dot(items[i].geometry.x, items[j].geometry.x)) > 0.9999
                && a.sectionId == b.sectionId && a.materialId == b.materialId) {
                parent[static_cast<std::size_t>(root(static_cast<int>(i)))] = root(static_cast<int>(j));
            }
        }
    }
    std::unordered_map<int, double> groupLength;
    std::unordered_map<int, int> groupCount;
    for (std::size_t i = 0; i < items.size(); ++i) {
        groupLength[root(static_cast<int>(i))] += items[i].geometry.length;
        ++groupCount[root(static_cast<int>(i))];
    }

    // energia por modo, barra e plano; participacao em relacao ao maior valor do modo
    const std::size_t modes = result.bucklingModes.size();
    std::vector<std::vector<std::array<double, 2>>> energy(modes, std::vector<std::array<double, 2>>(items.size(), {0.0, 0.0}));
    std::vector<double> peak(modes, 0.0);
    for (std::size_t k = 0; k < modes; ++k) {
        if (result.bucklingModes[k].size() < model.nodes.size() * 6U) {
            continue;
        }
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (items[i].axial >= 0.0 || -items[i].axial < options.minAxialRatio * maxCompression) {
                continue;
            }
            const auto e = bendingEnergy(items[i].geometry, result.bucklingModes[k]);
            energy[k][i] = {items[i].ei[0] * e[0], items[i].ei[1] * e[1]};
            peak[k] = std::max({peak[k], energy[k][i][0], energy[k][i][1]});
        }
    }

    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i].axial >= 0.0 || -items[i].axial < options.minAxialRatio * maxCompression) {
            continue;
        }
        MemberResult row;
        row.memberId = items[i].member->id;
        row.length = items[i].geometry.length;
        row.groupSize = groupCount[root(static_cast<int>(i))];
        row.groupLength = groupLength[root(static_cast<int>(i))];
        row.axial = items[i].axial;
        // plano x = flexao em torno do eixo de maior inercia (= plano do deslocamento transversal segundo o outro eixo local)
        const bool strongIsZ = items[i].ei[0] >= items[i].ei[1];
        for (int plane = 0; plane < 2; ++plane) {
            PlaneResult found;
            for (std::size_t k = 0; k < modes && !found.found; ++k) {
                if (peak[k] <= 0.0) {
                    continue;
                }
                // participacao do grupo de trechos colineares: a do trecho que mais flete (um trecho perto de um ponto de inflexao ou de um apoio quase nao flete no modo)
                double participation = 0.0;
                for (std::size_t j = 0; j < items.size(); ++j) {
                    if (root(static_cast<int>(j)) == root(static_cast<int>(i))) {
                        participation = std::max(participation, energy[k][j][static_cast<std::size_t>(plane)] / peak[k]);
                    }
                }
                if (participation >= options.minParticipation) {
                    found.found = true;
                    found.mode = static_cast<int>(k) + 1;
                    found.lambda = result.bucklingFactors[k];
                    found.participation = participation;
                    found.ncr = found.lambda * (-items[i].axial);
                    const double ei = items[i].ei[static_cast<std::size_t>(plane)];
                    found.k = kPi * std::sqrt(ei / found.ncr) / row.groupLength;
                    found.effectiveLength = found.k * row.groupLength;
                    found.suspicious = found.k < options.kMin || found.k > options.kMax;
                }
            }
            // plano 0 = deslocamento em y local (flexao em torno de z, I = Iz); plano 1 = deslocamento em z local (I = Iy)
            const bool isX = (plane == 0) == strongIsZ;
            (isX ? row.x : row.y) = found;
        }
        if (row.x.found || row.y.found) {
            out.members.append(row);
        }
    }
    std::sort(out.members.begin(), out.members.end(), [](const MemberResult &a, const MemberResult &b) { return a.memberId < b.memberId; });
    if (out.members.isEmpty()) {
        out.notes << QStringLiteral("Nenhuma barra atingiu a participação mínima nos modos calculados: peça mais modos ou reduza o limite.");
    }
    return out;
}

} // namespace Structura::App::Services
