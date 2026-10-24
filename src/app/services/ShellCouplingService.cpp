#include "app/services/ShellCouplingService.h"

#include "shell/ShellTributary.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <unordered_map>

namespace Structura::App::Services {

namespace {

constexpr double kCoincidenceTol = 1.0e-5;   // m: no de barra = no da malha
constexpr double kPlaneTol = 1.0e-3;         // m: no de barra sobre o plano de um elemento de casca
constexpr double kAxisTol = 1.0e-4;          // m: no da malha sobre o eixo de uma barra

std::uint64_t hashBytes(std::uint64_t h, const void *data, std::size_t size)
{
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t i = 0; i < size; ++i) {
        h ^= bytes[i];
        h *= 1099511628211ull;
    }
    return h;
}

template <typename T>
std::uint64_t hashValue(std::uint64_t h, const T &value)
{
    return hashBytes(h, &value, sizeof(T));
}

std::string signature(const Structura::Shell::Model &shell, const SolverBridge::ModelData &model, double forceUnit)
{
    std::uint64_t h = 1469598103934665603ull;
    const std::string fp = Structura::Shell::fingerprint(shell);
    h = hashBytes(h, fp.data(), fp.size());
    h = hashValue(h, forceUnit);
    for (const auto &n : model.nodes) {
        h = hashValue(h, n.id);
        h = hashValue(h, n.x);
        h = hashValue(h, n.y);
        h = hashValue(h, n.z);
    }
    for (const auto &m : model.members) {
        h = hashValue(h, m.id);
        h = hashValue(h, m.nodeI);
        h = hashValue(h, m.nodeJ);
    }
    return std::to_string(h) + ":" + std::to_string(fp.size()) + ":" + std::to_string(model.nodes.size());
}

std::mutex g_cacheMutex;
std::string g_cacheKey;
std::shared_ptr<const ShellCouplingService::Mapping> g_cacheValue;

using Vec3 = std::array<double, 3>;

struct CellKey {
    long long x, y, z;
    bool operator==(const CellKey &o) const { return x == o.x && y == o.y && z == o.z; }
};
struct CellHash {
    std::size_t operator()(const CellKey &k) const
    {
        return static_cast<std::size_t>(k.x * 73856093LL) ^ static_cast<std::size_t>(k.y * 19349663LL) ^ static_cast<std::size_t>(k.z * 83492791LL);
    }
};

} // namespace

std::vector<ShellCouplingService::BarCuts> ShellCouplingService::barCuts(const Structura::Shell::Mesh &mesh, const SolverBridge::ModelData &model)
{
    std::unordered_map<int, Vec3> position;
    for (const auto &n : model.nodes) {
        position[n.id] = {n.x, n.y, n.z};
    }
    std::vector<BarCuts> out;
    for (const auto &member : model.members) {
        const auto a = position.find(member.nodeI), b = position.find(member.nodeJ);
        if (a == position.end() || b == position.end()) {
            continue;
        }
        const Vec3 ab {b->second[0] - a->second[0], b->second[1] - a->second[1], b->second[2] - a->second[2]};
        const double len2 = ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2];
        if (len2 < 1e-12) {
            continue;
        }
        const double len = std::sqrt(len2);
        BarCuts cuts;
        cuts.memberId = member.id;
        std::vector<std::pair<double, Vec3>> found;
        for (const Vec3 &p : mesh.nodes) {
            const Vec3 ap {p[0] - a->second[0], p[1] - a->second[1], p[2] - a->second[2]};
            const double t = (ap[0] * ab[0] + ap[1] * ab[1] + ap[2] * ab[2]) / len2;
            if (t * len <= kCoincidenceTol || (1.0 - t) * len <= kCoincidenceTol) {
                continue;
            }
            const Vec3 d {ap[0] - t * ab[0], ap[1] - t * ab[1], ap[2] - t * ab[2]};
            if (std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) <= kAxisTol) {
                found.emplace_back(t, p);
            }
        }
        if (found.empty()) {
            continue;
        }
        std::sort(found.begin(), found.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
        for (const auto &[t, p] : found) {
            if (cuts.parameters.empty() || (t - cuts.parameters.back()) * len > kCoincidenceTol) {
                cuts.parameters.push_back(t);
                cuts.points.push_back(p);
            }
        }
        out.push_back(std::move(cuts));
    }
    return out;
}

std::shared_ptr<const ShellCouplingService::Mapping> ShellCouplingService::mapping(const Structura::Shell::Model &shell, const SolverBridge::ModelData &model, double forceUnit)
{
    const std::string key = signature(shell, model, forceUnit);
    {
        const std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cacheValue && g_cacheKey == key) {
            return g_cacheValue;
        }
    }
    auto result = std::make_shared<Mapping>();
    Mapping &m = *result;
    m.forceUnit = forceUnit > 0.0 ? forceUnit : 1.0;
    m.mesh = Structura::Shell::generateMesh(shell);
    for (const std::string &w : m.mesh.warnings) {
        m.warnings.push_back(w);
    }
    auto part = std::make_shared<SolverBridge::ShellPart>();
    if (!m.mesh.empty()) {
        // nos de barras por celula (busca de coincidencia)
        std::unordered_map<CellKey, std::vector<std::size_t>, CellHash> grid;
        auto cellOf = [](const Vec3 &p) { return CellKey {std::llround(p[0] / 1e-3), std::llround(p[1] / 1e-3), std::llround(p[2] / 1e-3)}; };
        int maxId = 0;
        for (std::size_t i = 0; i < model.nodes.size(); ++i) {
            const auto &n = model.nodes[i];
            maxId = std::max(maxId, n.id);
            grid[cellOf({n.x, n.y, n.z})].push_back(i);
        }
        auto findBarNode = [&](const Vec3 &p) -> int {
            const CellKey c = cellOf(p);
            for (long long dx = -1; dx <= 1; ++dx) {
                for (long long dy = -1; dy <= 1; ++dy) {
                    for (long long dz = -1; dz <= 1; ++dz) {
                        const auto it = grid.find({c.x + dx, c.y + dy, c.z + dz});
                        if (it == grid.end()) {
                            continue;
                        }
                        for (const std::size_t i : it->second) {
                            const auto &n = model.nodes[i];
                            if (std::hypot(std::hypot(n.x - p[0], n.y - p[1]), n.z - p[2]) <= kCoincidenceTol) {
                                return n.id;
                            }
                        }
                    }
                }
            }
            return 0;
        };
        const Structura::Shell::Job job = Structura::Shell::buildJob(shell, m.mesh, {});
        for (const std::string &w : job.warnings) {
            m.warnings.push_back(w);
        }
        m.nodeIdOfMeshNode.assign(m.mesh.nodes.size(), 0);
        std::map<int, std::array<int, 6>> barRestraint;
        int nextId = maxId + 1;
        std::vector<char> merged(m.mesh.nodes.size(), 0);
        std::unordered_map<int, char> mergedBar;
        for (std::size_t n = 0; n < m.mesh.nodes.size(); ++n) {
            const int barId = findBarNode(m.mesh.nodes[n]);
            if (barId != 0) {
                merged[n] = 1;
                mergedBar[barId] = 1;
                ++m.mergedNodes;
                if (shell.monolithic) {
                    // ligacao monolitica: o no da malha e o no da barra (6 graus de liberdade)
                    m.nodeIdOfMeshNode[n] = barId;
                    auto &flags = barRestraint[barId];
                    for (std::size_t c = 0; c < 6; ++c) {
                        flags[c] = std::max(flags[c], job.nodes[n].fixed[c]);
                    }
                } else {
                    // casca APOIADA na barra: no proprio da casca ligado ao da barra so nas translacoes (as rotacoes ficam livres,
                    // a laje transmite reacoes e nao momentos); os quinhoes de carga chegam a viga como forcas nodais
                    SolverBridge::ShellNodeData node;
                    node.id = nextId++;
                    node.x = m.mesh.nodes[n][0];
                    node.y = m.mesh.nodes[n][1];
                    node.z = m.mesh.nodes[n][2];
                    node.restraints = job.nodes[n].fixed;
                    m.nodeIdOfMeshNode[n] = node.id;
                    part->nodes.push_back(node);
                    SolverBridge::ShellTieData tie;
                    tie.slave = node.id;
                    tie.masters = {barId};
                    tie.weights = {1.0};
                    tie.dofMask = 7;
                    part->ties.push_back(std::move(tie));
                }
            } else {
                SolverBridge::ShellNodeData node;
                node.id = nextId++;
                node.x = m.mesh.nodes[n][0];
                node.y = m.mesh.nodes[n][1];
                node.z = m.mesh.nodes[n][2];
                node.restraints = job.nodes[n].fixed;
                m.nodeIdOfMeshNode[n] = node.id;
                part->nodes.push_back(node);
            }
        }
        for (const auto &[id, flags] : barRestraint) {
            if (std::any_of(flags.begin(), flags.end(), [](int f) { return f != 0; })) {
                m.barRestraints.emplace_back(id, flags);
            }
        }
        // elementos
        for (std::size_t e = 0; e < m.mesh.quads.size(); ++e) {
            const auto &q = m.mesh.quads[e];
            const auto &material = shell.materials[static_cast<std::size_t>(q.material)];
            SolverBridge::ShellElementData element;
            element.id = static_cast<int>(e) + 1;
            for (std::size_t i = 0; i < 4; ++i) {
                element.nodes[i] = m.nodeIdOfMeshNode[static_cast<std::size_t>(q.n[i])];
            }
            element.e = material.E / m.forceUnit;
            element.nu = material.nu;
            element.density = material.unitWeight / m.forceUnit / 9.80665;
            element.thickness = q.thickness;
            element.alpha = material.alpha;
            part->elements.push_back(element);
        }
        // nos pendentes de malhas nao conformes
        for (const auto &t : m.mesh.ties) {
            SolverBridge::ShellTieData tie;
            tie.slave = m.nodeIdOfMeshNode[static_cast<std::size_t>(t.slave)];
            for (std::size_t i = 0; i < t.masters.size(); ++i) {
                tie.masters.push_back(m.nodeIdOfMeshNode[static_cast<std::size_t>(t.masters[i])]);
                tie.weights.push_back(t.weights[i]);
            }
            part->ties.push_back(std::move(tie));
        }
        // nos de barras sobre o plano de elementos de casca
        for (const auto &bn : model.nodes) {
            if (mergedBar.count(bn.id)) {
                continue;
            }
            const auto host = Structura::Shell::hostTie(m.mesh, {bn.x, bn.y, bn.z}, kPlaneTol);
            if (!host) {
                continue;
            }
            SolverBridge::ShellTieData tie;
            tie.slave = bn.id;
            tie.dofMask = shell.monolithic ? 63 : 7;
            for (std::size_t i = 0; i < host->masters.size(); ++i) {
                tie.masters.push_back(m.nodeIdOfMeshNode[static_cast<std::size_t>(host->masters[i])]);
                tie.weights.push_back(host->weights[i]);
            }
            part->ties.push_back(std::move(tie));
            m.tiedBarNodes.push_back(bn.id);
        }
        // molas dos apoios elasticos
        for (const auto &s : job.springs) {
            SolverBridge::ShellSpringData spring;
            spring.node = m.nodeIdOfMeshNode[static_cast<std::size_t>(s.node)];
            for (std::size_t c = 0; c < 6; ++c) {
                spring.k[c] = s.k[c] / m.forceUnit;
            }
            spring.compressionOnly = s.compressionOnly;
            spring.normal = s.normal;
            part->springs.push_back(spring);
        }
        for (const auto &cut : barCuts(m.mesh, model)) {
            m.interiorShellNodes += static_cast<int>(cut.parameters.size());
        }
        if (m.interiorShellNodes > 0) {
            m.warnings.push_back(std::to_string(m.interiorShellNodes) + " no(s) de casca caem sobre o eixo de barras sem ser extremidade delas: "
                                 "as barras nao estao ligadas as cascas nesses pontos (use \"Dividir barras nos nos das cascas\").");
        }
    }
    m.part = part;
    const std::shared_ptr<const Mapping> shared = result;
    {
        const std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_cacheKey = key;
        g_cacheValue = shared;
    }
    return shared;
}

std::shared_ptr<const ShellCouplingService::Mapping> ShellCouplingService::attach(SolverBridge::ModelData &model, const Structura::Shell::Model &shell,
                                                                                  const std::string &loadCase, double forceUnit)
{
    if (shell.objects.empty()) {
        return nullptr;
    }
    const auto map = mapping(shell, model, forceUnit);
    if (!map || map->mesh.empty() || !map->part || map->part->empty()) {
        return nullptr;
    }
    model.shell = map->part;
    for (const auto &[id, flags] : map->barRestraints) {
        for (auto &node : model.nodes) {
            if (node.id != id) {
                continue;
            }
            for (std::size_t c = 0; c < 6; ++c) {
                node.restraints[c] = std::max(node.restraints[c], flags[c]);
            }
        }
    }
    const Structura::Shell::Job job = Structura::Shell::buildJob(shell, map->mesh, {loadCase});
    if (!job.cases.empty()) {
        const auto &c = job.cases.front();
        const double f = map->forceUnit;
        for (const auto &p : c.pressure) {
            SolverBridge::ShellLoadData load;
            load.shellId = p.shell + 1;
            load.kind = 0;
            for (std::size_t i = 0; i < 4; ++i) {
                load.v[i] = p.p[i] / f;
            }
            model.shellLoads.push_back(load);
        }
        for (const auto &b : c.body) {
            SolverBridge::ShellLoadData load;
            load.shellId = b.shell + 1;
            load.kind = 1;
            load.v = {b.b[0] / f, b.b[1] / f, b.b[2] / f, 0.0};
            model.shellLoads.push_back(load);
        }
        for (const auto &t : c.thermal) {
            SolverBridge::ShellLoadData load;
            load.shellId = t.shell + 1;
            load.kind = 2;
            load.v = {t.uniform, t.gradient, 0.0, 0.0};
            model.shellLoads.push_back(load);
        }
        for (const auto &n : c.nodal) {
            SolverBridge::NodalLoadData load;
            load.nodeId = map->nodeIdOfMeshNode[static_cast<std::size_t>(n.node)];
            for (std::size_t k = 0; k < 6; ++k) {
                load.values[k] = n.f[k] / f;
            }
            model.nodalLoads.push_back(load);
        }
    }
    return map;
}

int ShellCouplingService::attachTributary(SolverBridge::ModelData &model, const Structura::Shell::Model &shell, const std::string &loadCase, double forceUnit,
                                          std::vector<std::string> *warnings)
{
    using namespace Structura::Shell;
    if (shell.objects.empty() || model.members.empty()) {
        return 0;
    }
    std::unordered_map<int, Vec3> position;
    for (const auto &n : model.nodes) {
        position[n.id] = {n.x, n.y, n.z};
    }
    std::vector<TributaryBar> bars;
    for (const auto &m : model.members) {
        const auto a = position.find(m.nodeI), b = position.find(m.nodeJ);
        if (a != position.end() && b != position.end()) {
            bars.push_back({m.id, a->second, b->second});
        }
    }
    const double unit = forceUnit > 0.0 ? forceUnit : 1.0;
    std::map<int, Vec3> total;
    for (const Object &plate : shell.objects) {
        if (!plate.visible || plate.kind != Kind::Plate) {
            continue;
        }
        Vec3 force {0.0, 0.0, 0.0};
        bool any = false;
        for (const Load &load : shell.loads) {
            if (load.caseName != loadCase || (load.object != 0 && load.object != plate.id)) {
                continue;
            }
            Vec3 f {};
            if (surfaceForce(shell, plate, load, f)) {
                for (std::size_t k = 0; k < 3; ++k) {
                    force[k] += f[k];
                }
                any = true;
            }
        }
        if (!any) {
            continue;
        }
        for (const TributaryLoad &q : tributaryLoads(plate, force, bars, kPlaneTol * 2.0, warnings)) {
            Vec3 &sum = total[q.barId];
            for (std::size_t k = 0; k < 3; ++k) {
                sum[k] += q.q[k] / unit;
            }
        }
    }
    for (const auto &[barId, q] : total) {
        SolverBridge::MemberLoadData load;
        load.memberId = barId;
        load.coordSystem = SolverBridge::MemberLoadCoordSystem::Global;
        load.qx = q[0];
        load.qy = q[1];
        load.qz = q[2];
        model.memberLoads.push_back(load);
    }
    return static_cast<int>(total.size());
}

Structura::Shell::CaseResult ShellCouplingService::caseResult(const Mapping &mapping, const SolverBridge::Result &result, const std::string &name)
{
    Structura::Shell::CaseResult out;
    out.name = name;
    std::unordered_map<int, const SolverBridge::NodeResult *> nodes;
    for (const auto &n : result.nodeResults) {
        nodes[n.nodeId] = &n;
    }
    for (const auto &n : result.shellNodeResults) {
        nodes[n.nodeId] = &n;
    }
    const double f = mapping.forceUnit;
    out.displacement.assign(mapping.mesh.nodes.size(), {});
    out.reaction.assign(mapping.mesh.nodes.size(), {});
    for (std::size_t n = 0; n < mapping.mesh.nodes.size(); ++n) {
        const auto it = nodes.find(mapping.nodeIdOfMeshNode[n]);
        if (it == nodes.end()) {
            continue;
        }
        for (std::size_t c = 0; c < 6; ++c) {
            out.displacement[n][c] = it->second->displacements[c];
            out.reaction[n][c] = it->second->reactions[c] * f;
        }
    }
    out.elements.assign(mapping.mesh.quads.size(), {});
    for (const auto &e : result.shellResults) {
        const std::size_t index = static_cast<std::size_t>(e.shellId - 1);
        if (index >= out.elements.size()) {
            continue;
        }
        for (std::size_t k = 0; k < 40; ++k) {
            out.elements[index].v[k] = e.values[k] * f;
        }
        out.elements[index].axes = e.axes;
    }
    return out;
}

} // namespace Structura::App::Services
