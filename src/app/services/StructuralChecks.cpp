#include "StructuralChecks.h"

#include "viz/postprocessor/UniformLoadForceEvaluator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <unordered_map>

namespace Structura::App::Services {

namespace {

using Structura::Visualization::InternalForces;
using Structura::Visualization::UniformLoadForceEvaluator;

struct NodeKey {
    static long long of(double z) { return std::llround(z * 1000.0); }
};

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Flechas
// ---------------------------------------------------------------------------------------------------------------------

QVector<StructuralChecks::DeflectionResult> StructuralChecks::deflections(const Structura::App::SolverBridge::ModelData &geometry,
                                                                          const std::vector<ResultSet> &sets,
                                                                          const DesignSettings &settings, double lengthFactorToMm,
                                                                          const InertiaRatio &inertiaRatio)
{
    std::unordered_map<int, Structura::App::SolverBridge::MaterialData> materials;
    for (const auto &m : geometry.materials) {
        materials[m.id] = m;
    }
    std::unordered_map<int, Structura::App::SolverBridge::SectionData> sections;
    for (const auto &s : geometry.sections) {
        sections[s.id] = s;
    }
    std::unordered_map<int, Structura::App::SolverBridge::MemberData> members;
    std::unordered_map<int, int> attached;
    for (const auto &m : geometry.members) {
        members[m.id] = m;
        ++attached[m.nodeI];
        ++attached[m.nodeJ];
    }
    std::unordered_map<int, bool> restrained;
    for (const auto &n : geometry.nodes) {
        bool any = false;
        for (const int r : n.restraints) {
            any = any || r != 0;
        }
        restrained[n.id] = any;
    }
    auto isFree = [&](int node) { return attached[node] == 1 && !restrained[node]; };

    // Tabela B.1: vigas que suportam pilares tem limite L/500. Um no livre onde um pilar chega por cima e nenhum
    // pilar continua por baixo (o pilar nasce na viga) marca as vigas horizontais ligadas a ele.
    std::unordered_map<int, std::array<double, 3>> position;
    for (const auto &n : geometry.nodes) {
        position[n.id] = {n.x, n.y, n.z};
    }
    std::unordered_map<int, bool> columnAbove, columnBelow;
    for (const auto &m : geometry.members) {
        const auto a = position.find(m.nodeI), b = position.find(m.nodeJ);
        if (a == position.end() || b == position.end()) {
            continue;
        }
        const double dxy = std::hypot(b->second[0] - a->second[0], b->second[1] - a->second[1]);
        const double dzv = b->second[2] - a->second[2];
        if (std::abs(dzv) < 4.0 * dxy || std::abs(dzv) < 1e-9) {
            continue;   // nao e vertical (inclinacao > ~14 graus)
        }
        const int low = dzv > 0.0 ? m.nodeI : m.nodeJ, high = dzv > 0.0 ? m.nodeJ : m.nodeI;
        columnAbove[low] = true;    // pilar sobe a partir deste no
        columnBelow[high] = true;   // pilar desce ate este no
    }
    auto supportsColumns = [&](int node) { return columnAbove[node] && !columnBelow[node] && !restrained[node]; };

    std::map<int, DeflectionResult> worst;
    constexpr int kSamples = 80;
    for (const ResultSet &set : sets) {
        for (const auto &data : DesignService::buildElements(geometry, set)) {
            const auto &el = data.element;
            const auto mit = members.find(el.externalId);
            if (mit == members.end()) {
                continue;
            }
            const auto mat = materials.find(mit->second.materialId);
            const auto sec = sections.find(mit->second.sectionId);
            if (mat == materials.end() || sec == sections.end()) {
                continue;
            }
            UniformLoadForceEvaluator evaluator(el);
            const double length = evaluator.length();
            if (length < 1e-9) {
                continue;
            }
            const double dz = std::abs(el.node2[2] - el.node1[2]) / length;
            const auto params = settings.paramsFor(el.externalId);
            int denominator = params.deflDenominator;
            if (denominator <= 0) {
                if (dz < 0.02) {
                    denominator = supportsColumns(mit->second.nodeI) || supportsColumns(mit->second.nodeJ) ? 500 : 350;
                } else if (dz < 0.5) {
                    denominator = 250;
                } else {
                    continue;   // pilares (limite por andar) e diagonais
                }
            }
            const bool freeI = isFree(mit->second.nodeI);
            const bool freeJ = isFree(mit->second.nodeJ);
            const bool cantilever = freeI != freeJ;

            std::array<double, 2> stiffness {1.0, 1.0};
            if (inertiaRatio) {
                double mzMax = 0.0, myMax = 0.0;
                for (int i = 0; i <= kSamples; ++i) {
                    const InternalForces f = evaluator.evaluate(length * i / kSamples);
                    mzMax = std::max(mzMax, std::abs(f.Mz));
                    myMax = std::max(myMax, std::abs(f.My));
                }
                stiffness = inertiaRatio(el.externalId, mzMax, myMax);
            }
            const double eiz = mat->second.e * sec->second.iz * stiffness[0];
            const double eiy = mat->second.e * sec->second.iy * stiffness[1];
            std::vector<double> vy(kSamples + 1, 0.0), vz(kSamples + 1, 0.0), sy(kSamples + 1, 0.0), sz(kSamples + 1, 0.0);
            const double h = length / kSamples;
            double prevKy = 0.0, prevKz = 0.0;
            for (int i = 0; i <= kSamples; ++i) {
                const InternalForces f = evaluator.evaluate(h * i);
                const double ky = eiz > 0 ? -f.Mz / eiz : 0.0;
                const double kz = eiy > 0 ? f.My / eiy : 0.0;
                if (i > 0) {
                    sy[i] = sy[i - 1] + 0.5 * h * (ky + prevKy);
                    sz[i] = sz[i - 1] + 0.5 * h * (kz + prevKz);
                    vy[i] = vy[i - 1] + 0.5 * h * (sy[i] + sy[i - 1]);
                    vz[i] = vz[i - 1] + 0.5 * h * (sz[i] + sz[i - 1]);
                }
                prevKy = ky;
                prevKz = kz;
            }
            double best = 0.0;
            double bestX = 0.0;
            for (int i = 0; i <= kSamples; ++i) {
                const double x = h * i;
                double dy, dzz;
                if (cantilever) {
                    const int a = freeI ? kSamples : 0;   // extremidade apoiada
                    const double xa = h * a;
                    dy = vy[i] - vy[a] - sy[a] * (x - xa);
                    dzz = vz[i] - vz[a] - sz[a] * (x - xa);
                } else {
                    dy = vy[i] - vy[kSamples] * x / length;
                    dzz = vz[i] - vz[kSamples] * x / length;
                }
                const double d = std::hypot(dy, dzz);
                if (d > best) {
                    best = d;
                    bestX = x;
                }
            }
            DeflectionResult r;
            r.memberId = el.externalId;
            r.combination = set.name;
            r.deflectionMm = best * lengthFactorToMm;
            r.positionMm = bestX * lengthFactorToMm;
            r.lengthMm = length * lengthFactorToMm;
            r.denominator = denominator;
            r.cantilever = cantilever;
            r.limitMm = (cantilever ? 2.0 : 1.0) * r.lengthMm / denominator;
            r.ratio = r.limitMm > 0 ? r.deflectionMm / r.limitMm : 0.0;
            auto it = worst.find(r.memberId);
            if (it == worst.end() || r.ratio > it->second.ratio) {
                worst[r.memberId] = r;
            }
        }
    }
    QVector<DeflectionResult> out;
    for (const auto &kv : worst) {
        out.append(kv.second);
    }
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// Andares e B2
// ---------------------------------------------------------------------------------------------------------------------

StructuralChecks::Output StructuralChecks::stories(const Structura::App::SolverBridge::ModelData &geometry,
                                                   const std::vector<ResultSet> &sets, const DesignSettings &settings,
                                                   double forceFactorToN, double lengthFactorToMm, bool ultimate)
{
    Output out;
    std::unordered_map<int, std::array<double, 3>> pos;
    for (const auto &n : geometry.nodes) {
        pos[n.id] = {n.x, n.y, n.z};
    }

    // Pilares e niveis
    struct Column {
        int id;
        int nodeI, nodeJ;
        double zLow, zHigh;
        bool iIsLow;
    };
    std::vector<Column> columns;
    std::map<long long, double> levels;
    for (const auto &m : geometry.members) {
        const auto &a = pos[m.nodeI];
        const auto &b = pos[m.nodeJ];
        const double len = std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
        if (len < 1e-9 || std::abs(a[2] - b[2]) / len < 0.9) {
            continue;
        }
        Column c {m.id, m.nodeI, m.nodeJ, std::min(a[2], b[2]), std::max(a[2], b[2]), a[2] <= b[2]};
        columns.push_back(c);
        levels[NodeKey::of(c.zLow)] = c.zLow;
        levels[NodeKey::of(c.zHigh)] = c.zHigh;
    }
    if (columns.empty() || levels.size() < 2) {
        out.warnings.append(QStringLiteral("Nao ha pilares verticais: B2 e deslocamentos por andar nao calculados."));
        return out;
    }
    std::vector<double> z;
    for (const auto &kv : levels) {
        z.push_back(kv.second);
    }
    for (std::size_t i = 0; i + 1 < z.size(); ++i) {
        Story s;
        s.index = static_cast<int>(i);
        s.zBottom = z[i];
        s.zTop = z[i + 1];
        out.stories.append(s);
    }
    auto storyOfZ = [&](double zValue) {
        for (const Story &s : out.stories) {
            if (zValue > s.zBottom + 1e-6 && zValue <= s.zTop + 1e-6) {
                return s.index;
            }
        }
        return zValue <= out.stories.first().zBottom + 1e-6 ? 0 : static_cast<int>(out.stories.size()) - 1;
    };
    std::unordered_map<int, int> columnStory;
    for (const Column &c : columns) {
        const int idx = storyOfZ(c.zLow + 1e-4);
        out.stories[idx].columns.append(c.id);
        columnStory[c.id] = idx;
        out.storyOfMember[c.id] = idx;
    }
    for (const auto &m : geometry.members) {
        if (out.storyOfMember.contains(m.id)) {
            continue;
        }
        const double zmid = 0.5 * (pos[m.nodeI][2] + pos[m.nodeJ][2]);
        out.storyOfMember[m.id] = storyOfZ(zmid);
    }

    const double rs = std::max(0.1, settings.rs);
    // B2 = 1 / (1 - (1/Rs)(dh/h)(SN/SH)) (C.2.3); `factor` < 1 aplica a rigidez reduzida (dh / factor)
    auto b2Raw = [&](double driftMm, double heightMm, double sumN, double shear, double factor) {
        if (sumN <= 0.0 || shear < 1e-6 * sumN || driftMm <= 0.0) {
            return 1.0;
        }
        const double denom = 1.0 - (1.0 / rs) * (driftMm / factor / heightMm) * (sumN / shear);
        return denom <= 0.05 ? 20.0 : std::max(1.0, 1.0 / denom);
    };
    auto b2Value = [&](double driftMm, const StoryResult &r, double shear) { return b2Raw(driftMm, r.heightMm, r.sumN, shear, 1.0); };
    for (const ResultSet &set : sets) {
        if (!set.result) {
            continue;
        }
        std::unordered_map<int, std::array<double, 6>> disp;
        for (const auto &nr : set.result->nodeResults) {
            disp[nr.nodeId] = nr.displacements;
        }
        const auto elements = DesignService::buildElements(geometry, set);
        std::unordered_map<int, const DesignService::ElementData *> byId;
        for (const auto &e : elements) {
            byId[e.element.externalId] = &e;
        }
        // estrutura nt: a cortante do andar de B2 e a da estrutura lt (= original - nt), C.2.3
        std::vector<DesignService::ElementData> ntElements;
        std::unordered_map<int, const DesignService::ElementData *> ntById;
        if (set.nt) {
            DesignService::ResultSet ntSet = set;
            ntSet.result = set.nt;
            ntElements = DesignService::buildElements(geometry, ntSet);
            for (const auto &e : ntElements) {
                ntById[e.element.externalId] = &e;
            }
        }

        double topDisplacement = 0.0;
        for (const Story &story : out.stories) {
            StoryResult r;
            r.story = story.index;
            r.combination = set.name;
            r.heightMm = (story.zTop - story.zBottom) * lengthFactorToMm;
            double sumN = 0.0, hx = 0.0, hy = 0.0, driftX = 0.0, driftY = 0.0, topDisp = 0.0;
            for (const int id : story.columns) {
                const auto eit = byId.find(id);
                if (eit == byId.end()) {
                    continue;
                }
                const auto &el = eit->second->element;
                const auto &ef = el.endForces;
                sumN += std::max(0.0, -0.5 * (ef.N1 + ef.N2));
                double vy1 = ef.Vy1, vz1 = ef.Vz1;
                if (const auto ntIt = ntById.find(id); ntIt != ntById.end()) {
                    vy1 -= ntIt->second->element.endForces.Vy1;
                    vz1 -= ntIt->second->element.endForces.Vz1;
                }
                hx += vy1 * el.yLocal[0] + vz1 * el.zLocal[0];
                hy += vy1 * el.yLocal[1] + vz1 * el.zLocal[1];
                const auto di = disp.find(eit->second->nodeI);
                const auto dj = disp.find(eit->second->nodeJ);
                if (di != disp.end() && dj != disp.end()) {
                    const auto &top = (pos[eit->second->nodeI][2] >= pos[eit->second->nodeJ][2]) ? di->second : dj->second;
                    const auto &bot = (pos[eit->second->nodeI][2] >= pos[eit->second->nodeJ][2]) ? dj->second : di->second;
                    driftX = std::max(driftX, std::abs(top[0] - bot[0]));
                    driftY = std::max(driftY, std::abs(top[1] - bot[1]));
                    topDisp = std::max(topDisp, std::hypot(top[0], top[1]));
                }
            }
            r.sumN = sumN * forceFactorToN;
            r.sumHx = std::abs(hx) * forceFactorToN;
            r.sumHy = std::abs(hy) * forceFactorToN;
            r.driftXMm = driftX * lengthFactorToMm;
            r.driftYMm = driftY * lengthFactorToMm;
            r.heightFromBaseMm = (story.zTop - z.front()) * lengthFactorToMm;
            topDisplacement = std::max(topDisplacement, topDisp * lengthFactorToMm);
            r.topDisplacementMm = topDisp * lengthFactorToMm;

            r.b2x = b2Value(r.driftXMm, r, r.sumHx);
            r.b2y = b2Value(r.driftYMm, r, r.sumHy);
            if (r.b2x >= 20.0 || r.b2y >= 20.0) {
                out.warnings.append(QStringLiteral("Combinacao %1, andar %2: estrutura muito flexivel (B2 > 20); use analise de segunda ordem.")
                                        .arg(set.name).arg(story.index + 1));
            }
            out.storyResults.append(r);
        }
        // B2 por barra nessa combinacao (maior entre as duas direcoes)
        QMap<int, double> &perMember = out.b2[set.name];
        for (auto it = out.storyOfMember.constBegin(); it != out.storyOfMember.constEnd(); ++it) {
            for (const StoryResult &r : out.storyResults) {
                if (r.combination == set.name && r.story == it.value()) {
                    perMember[it.key()] = std::max(r.b2x, r.b2y);
                }
            }
        }
        // topo em relacao a base: ultimo andar
        for (StoryResult &r : out.storyResults) {
            if (r.combination == set.name && r.story == out.stories.size() - 1) {
                r.topDisplacementMm = topDisplacement;
            }
        }
    }
    if (ultimate && !out.storyResults.isEmpty()) {
        // Classificacao (4.10.4): maior B2 nominal entre andares e combinacoes ultimas (4.10.4.8: B2 aproxima a razao entre os deslocamentos de 2a e 1a ordem)
        double maxB2 = 1.0;
        for (const StoryResult &r : std::as_const(out.storyResults)) {
            maxB2 = std::max({maxB2, r.b2x, r.b2y});
        }
        out.maxB2Nominal = maxB2;
        out.swayClass = maxB2 <= 1.10 + 1e-9 ? 0 : (maxB2 <= 1.40 + 1e-9 ? 1 : 2);
        const bool imperfections = settings.notionalActive() || settings.geometricActive();
        const QString imperfectionNote = imperfections ? QString() : QStringLiteral(" As imperfeicoes geometricas iniciais (4.10.7.1.1: forcas nocionais de 0,3 % ou desaprumo h/333) NAO estao consideradas: ative-as em Parametros de dimensionamento.");
        if (out.swayClass == 0) {
            out.warnings.append(QStringLiteral("Estrutura de pequena deslocabilidade (B2 maximo %1 <= 1,10, NBR 8800, 4.10.4).%2 Os efeitos globais de 2a ordem so podem ser desprezados se N_Sd <= 50 % N_y em todas as barras que contribuem "
                                               "para a estabilidade lateral e as imperfeicoes forem somadas a todas as combinacoes (4.10.7.1.4); aqui B1 e B2 sao aplicados de qualquer modo.")
                                    .arg(maxB2, 0, 'f', 3).arg(imperfectionNote));
        } else if (out.swayClass == 1) {
            const double tau = std::clamp(settings.stiffnessReduction, 0.5, 1.0);
            out.stiffnessFactor = tau;
            for (StoryResult &r : out.storyResults) {
                r.b2x = b2Raw(r.driftXMm, r.heightMm, r.sumN, r.sumHx, tau);
                r.b2y = b2Raw(r.driftYMm, r.heightMm, r.sumN, r.sumHy, tau);
            }
            out.warnings.append(QStringLiteral("Estrutura de media deslocabilidade (B2 nominal maximo %1, entre 1,10 e 1,40; NBR 8800, 4.10.4): B1 e B2 calculados com a rigidez reduzida a %2 (4.10.7.1.2 e 4.10.7.1.3).%3")
                                    .arg(maxB2, 0, 'f', 3).arg(tau, 0, 'f', 2).arg(imperfectionNote));
        } else {
            out.warnings.append(QStringLiteral("Estrutura de GRANDE deslocabilidade (B2 maximo %1 > 1,40, NBR 8800, 4.10.4): exige analise rigorosa com nao linearidade geometrica e de material e imperfeicoes (4.10.7.2); "
                                               "ative a analise de 2a ordem em Parametros de dimensionamento. B1 e B2 sao apenas indicativos.").arg(maxB2, 0, 'f', 3));
        }
    }
    // B2 por barra em cada combinacao (maior entre as duas direcoes)
    out.b2.clear();
    for (auto it = out.storyOfMember.constBegin(); it != out.storyOfMember.constEnd(); ++it) {
        for (const StoryResult &r : std::as_const(out.storyResults)) {
            if (r.story == it.value()) {
                out.b2[r.combination][it.key()] = std::max(r.b2x, r.b2y);
            }
        }
    }
    out.warnings.removeDuplicates();
    return out;
}

std::vector<int> StructuralChecks::storyLevelNodes(const Structura::App::SolverBridge::ModelData &geometry)
{
    std::unordered_map<int, std::array<double, 3>> pos;
    for (const auto &n : geometry.nodes) {
        pos[n.id] = {n.x, n.y, n.z};
    }
    std::map<long long, double> levels;
    std::vector<std::pair<int, int>> columnNodes;
    for (const auto &m : geometry.members) {
        const auto &a = pos[m.nodeI];
        const auto &b = pos[m.nodeJ];
        const double len = std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
        if (len < 1e-9 || std::abs(a[2] - b[2]) / len < 0.9) {
            continue;
        }
        levels[NodeKey::of(std::min(a[2], b[2]))] = std::min(a[2], b[2]);
        levels[NodeKey::of(std::max(a[2], b[2]))] = std::max(a[2], b[2]);
        columnNodes.emplace_back(m.nodeI, m.nodeJ);
    }
    std::vector<int> nodes;
    if (levels.size() < 2) {
        return nodes;
    }
    const double base = levels.begin()->second;
    // todos os nos situados nos niveis dos andares (acima da base): as contencoes ficticias impedem o deslocamento horizontal de cada andar
    for (const auto &n : geometry.nodes) {
        if (n.z <= base + 1e-6) {
            continue;
        }
        const auto level = levels.find(NodeKey::of(n.z));
        if (level != levels.end() && std::abs(level->second - n.z) < 1e-6) {
            nodes.push_back(n.id);
        }
    }
    return nodes;
}

} // namespace Structura::App::Services
