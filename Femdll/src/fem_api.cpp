#include "fem_interface.h"

#include "fem_model.h"
#include "../../Structuradll/src/core/solver_security.h"

#include <cmath>
#include <cstring>
#include <map>
#include <string>

namespace {

std::uint64_t g_mefsSessionKey = 0;

struct Handle {
    fem::Model model;
    std::map<int, int> materialIndex, nodeIndex, shellIndex, beamIndex;
    std::string lastError;
};

Handle *get(void *h)
{
    return static_cast<Handle *>(h);
}

bool validCase(const Handle *h, int c)
{
    return h && c >= 0 && c < static_cast<int>(h->model.cases.size());
}

} // namespace

extern "C" {

void *fem_create()
{
    if (g_mefsSessionKey == 0) {
        return nullptr;
    }
    return new Handle();
}

void fem_destroy(void *handle)
{
    delete get(handle);
}

int fem_version()
{
    return 1;
}

int fem_add_material(void *h, int id, double E, double nu, double density, double alpha)
{
    Handle *s = get(h);
    if (!s || E <= 0.0 || nu <= -1.0 || nu >= 0.5) {
        return 1;
    }
    fem::Material m;
    m.id = id;
    m.E = E;
    m.nu = nu;
    m.density = density;
    m.alpha = alpha;
    s->materialIndex[id] = static_cast<int>(s->model.materials.size());
    s->model.materials.push_back(m);
    return 0;
}

int fem_set_material_orthotropy(void *h, int id, double E2, double nu12, double G12, double bend1, double bend2, double ax, double ay, double az)
{
    Handle *s = get(h);
    if (!s || E2 <= 0.0 || G12 <= 0.0 || bend1 <= 0.0 || bend2 <= 0.0 || std::abs(nu12) >= 1.0) {
        return 1;
    }
    const auto it = s->materialIndex.find(id);
    if (it == s->materialIndex.end()) {
        return 2;
    }
    fem::Material &m = s->model.materials[static_cast<std::size_t>(it->second)];
    const fem::Vec3 axis(ax, ay, az);
    if (axis.norm() < 1e-12) {
        return 3;
    }
    m.orthotropic = true;
    m.E2 = E2;
    m.nu12 = nu12;
    m.G12 = G12;
    m.bend1 = bend1;
    m.bend2 = bend2;
    m.axis = axis.normalized();
    // estabilidade do estado plano: 1 - nu12 nu21 > 0
    if (1.0 - nu12 * nu12 * E2 / m.E <= 0.0) {
        m.orthotropic = false;
        return 4;
    }
    return 0;
}

int fem_add_node(void *h, int id, double x, double y, double z, const int *restraint6)
{
    Handle *s = get(h);
    if (!s) {
        return 1;
    }
    fem::Node n;
    n.id = id;
    n.x = fem::Vec3(x, y, z);
    if (restraint6) {
        for (int i = 0; i < 6; ++i) {
            n.fixed[i] = restraint6[i];
        }
    }
    s->nodeIndex[id] = static_cast<int>(s->model.nodes.size());
    s->model.nodes.push_back(n);
    return 0;
}

int fem_add_shell(void *h, int id, int n1, int n2, int n3, int n4, int materialId, double thickness)
{
    Handle *s = get(h);
    if (!s || thickness <= 0.0) {
        return 1;
    }
    const int ids[4] = {n1, n2, n3, n4};
    fem::Shell sh;
    sh.id = id;
    for (int i = 0; i < 4; ++i) {
        const auto it = s->nodeIndex.find(ids[i]);
        if (it == s->nodeIndex.end()) {
            return 2;
        }
        sh.nodes[i] = it->second;
    }
    const auto mit = s->materialIndex.find(materialId);
    if (mit == s->materialIndex.end()) {
        return 3;
    }
    sh.material = mit->second;
    sh.thickness = thickness;
    s->shellIndex[id] = static_cast<int>(s->model.shells.size());
    s->model.shells.push_back(sh);
    return 0;
}

int fem_add_beam(void *h, int id, int ni, int nj, int materialId, double A, double Iy, double Iz, double J, double refX, double refY, double refZ)
{
    Handle *s = get(h);
    if (!s) {
        return 1;
    }
    const auto a = s->nodeIndex.find(ni), b = s->nodeIndex.find(nj);
    const auto m = s->materialIndex.find(materialId);
    if (a == s->nodeIndex.end() || b == s->nodeIndex.end() || m == s->materialIndex.end()) {
        return 2;
    }
    fem::Beam beam;
    beam.id = id;
    beam.nodes = {a->second, b->second};
    beam.material = m->second;
    beam.A = A;
    beam.Iy = Iy;
    beam.Iz = Iz;
    beam.J = J;
    beam.refY = fem::Vec3(refX, refY, refZ);
    s->beamIndex[id] = static_cast<int>(s->model.beams.size());
    s->model.beams.push_back(beam);
    return 0;
}

int fem_add_spring(void *h, int node, const double *k6, int compressionOnly, double nx, double ny, double nz)
{
    Handle *s = get(h);
    if (!s || !k6) {
        return 1;
    }
    const auto it = s->nodeIndex.find(node);
    if (it == s->nodeIndex.end()) {
        return 2;
    }
    fem::Spring sp;
    sp.node = it->second;
    for (int i = 0; i < 6; ++i) {
        sp.k[i] = k6[i];
    }
    sp.compressionOnly = compressionOnly != 0;
    sp.normal = fem::Vec3(nx, ny, nz);
    s->model.springs.push_back(sp);
    return 0;
}

int fem_add_tie_masked(void *h, int slave, int count, const int *masters, const double *weights, int dofMask)
{
    const int before = h ? static_cast<int>(get(h)->model.ties.size()) : 0;
    const int code = fem_add_tie(h, slave, count, masters, weights);
    if (code == 0) {
        Handle *s = get(h);
        if (static_cast<int>(s->model.ties.size()) > before) {
            s->model.ties.back().dofMask = dofMask & 63;
        }
    }
    return code;
}

int fem_add_tie(void *h, int slave, int count, const int *masters, const double *weights)
{
    Handle *s = get(h);
    if (!s || !masters || !weights || count < 1 || count > 8) {
        return 1;
    }
    fem::Tie tie;
    const auto sl = s->nodeIndex.find(slave);
    if (sl == s->nodeIndex.end()) {
        return 2;
    }
    tie.slave = sl->second;
    for (int i = 0; i < count; ++i) {
        const auto it = s->nodeIndex.find(masters[i]);
        if (it == s->nodeIndex.end()) {
            return 2;
        }
        tie.masters.push_back(it->second);
        tie.weights.push_back(weights[i]);
    }
    s->model.ties.push_back(std::move(tie));
    return 0;
}

int fem_add_case(void *h)
{
    Handle *s = get(h);
    if (!s) {
        return -1;
    }
    s->model.cases.emplace_back();
    return static_cast<int>(s->model.cases.size()) - 1;
}

int fem_load_node(void *h, int c, int node, const double *f6)
{
    Handle *s = get(h);
    if (!validCase(s, c) || !f6) {
        return 1;
    }
    const auto it = s->nodeIndex.find(node);
    if (it == s->nodeIndex.end()) {
        return 2;
    }
    fem::NodalLoad l;
    l.node = it->second;
    for (int i = 0; i < 6; ++i) {
        l.f[i] = f6[i];
    }
    s->model.cases[c].nodal.push_back(l);
    return 0;
}

int fem_load_shell_pressure(void *h, int c, int shell, const double *p4)
{
    Handle *s = get(h);
    if (!validCase(s, c) || !p4) {
        return 1;
    }
    const auto it = s->shellIndex.find(shell);
    if (it == s->shellIndex.end()) {
        return 2;
    }
    fem::ShellPressure p;
    p.shell = it->second;
    for (int i = 0; i < 4; ++i) {
        p.p[i] = p4[i];
    }
    s->model.cases[c].pressure.push_back(p);
    return 0;
}

int fem_load_shell_body(void *h, int c, int shell, double bx, double by, double bz)
{
    Handle *s = get(h);
    if (!validCase(s, c)) {
        return 1;
    }
    const auto it = s->shellIndex.find(shell);
    if (it == s->shellIndex.end()) {
        return 2;
    }
    fem::ShellBody b;
    b.shell = it->second;
    b.b = fem::Vec3(bx, by, bz);
    s->model.cases[c].body.push_back(b);
    return 0;
}

int fem_load_shell_thermal(void *h, int c, int shell, double uniform, double gradient)
{
    Handle *s = get(h);
    if (!validCase(s, c)) {
        return 1;
    }
    const auto it = s->shellIndex.find(shell);
    if (it == s->shellIndex.end()) {
        return 2;
    }
    fem::ShellThermal t;
    t.shell = it->second;
    t.uniform = uniform;
    t.gradient = gradient;
    s->model.cases[c].thermal.push_back(t);
    return 0;
}

int fem_load_beam_uniform(void *h, int c, int beam, double qx, double qy, double qz)
{
    Handle *s = get(h);
    if (!validCase(s, c)) {
        return 1;
    }
    const auto it = s->beamIndex.find(beam);
    if (it == s->beamIndex.end()) {
        return 2;
    }
    fem::BeamUniform u;
    u.beam = it->second;
    u.q = fem::Vec3(qx, qy, qz);
    s->model.cases[c].beamUniform.push_back(u);
    return 0;
}

int fem_solve(void *h, int errorSize, char *error)
{
    const std::uint64_t rtPoison = solver_sec::antiDebugPoison(
        reinterpret_cast<const void *>(&fem_solve),
        reinterpret_cast<const void *>(&fem_create));
    if (g_mefsSessionKey == 0 || rtPoison != 0ULL) {
        return 99;
    }
    Handle *s = get(h);
    if (!s) {
        return 1;
    }
    std::string message;
    fem::SolveOptions options;
    const bool ok = s->model.solve(options, message);
    if (!ok && error && errorSize > 0) {
        std::strncpy(error, message.c_str(), static_cast<std::size_t>(errorSize) - 1);
        error[errorSize - 1] = '\0';
        solver_sec::cipherCString(error, static_cast<std::size_t>(errorSize), g_mefsSessionKey);
    }
    return ok ? 0 : 2;
}

int fem_spring_iterations(void *h)
{
    return get(h) ? get(h)->model.springIterations : 0;
}

double fem_max_warp(void *h)
{
    return get(h) ? get(h)->model.maxWarp : 0.0;
}

int fem_get_displacements(void *h, int c, double *out)
{
    Handle *s = get(h);
    if (!s || c < 0 || c >= static_cast<int>(s->model.results.size()) || !out) {
        return 1;
    }
    const auto &d = s->model.results[c].displacements;
    std::memcpy(out, d.data(), d.size() * sizeof(double));
    return 0;
}

int fem_get_reactions(void *h, int c, double *out)
{
    Handle *s = get(h);
    if (!s || c < 0 || c >= static_cast<int>(s->model.results.size()) || !out) {
        return 1;
    }
    const auto &d = s->model.results[c].reactions;
    std::memcpy(out, d.data(), d.size() * sizeof(double));
    return 0;
}

int fem_get_shell_results(void *h, int c, double *out40, double *axes9)
{
    Handle *s = get(h);
    if (!s || c < 0 || c >= static_cast<int>(s->model.results.size()) || !out40) {
        return 1;
    }
    const auto &shells = s->model.results[c].shells;
    for (std::size_t i = 0; i < shells.size(); ++i) {
        std::memcpy(out40 + 40 * i, shells[i].values.data(), 40 * sizeof(double));
        if (axes9) {
            std::memcpy(axes9 + 9 * i, shells[i].axes.data(), 9 * sizeof(double));
        }
    }
    return 0;
}

int fem_get_beam_forces(void *h, int c, double *out12)
{
    Handle *s = get(h);
    if (!s || c < 0 || c >= static_cast<int>(s->model.results.size()) || !out12) {
        return 1;
    }
    const auto &beams = s->model.results[c].beams;
    for (std::size_t i = 0; i < beams.size(); ++i) {
        std::memcpy(out12 + 12 * i, beams[i].data(), 12 * sizeof(double));
    }
    return 0;
}

FEM_GATE_API int _z8m2p(std::uint64_t nonce, std::uint64_t authTag, std::uint64_t *outTable, std::uint32_t slotCount)
{
    int rc = -1;
    std::uint64_t poison = 0ULL;
    std::uint64_t effectiveNonce = 0ULL;
    std::uint32_t state = 0x6A10B1U;

    auto seal = [&](std::uint32_t slot, auto fnPtr) {
        outTable[slot] = solver_sec::sealPointer(
            reinterpret_cast<std::uintptr_t>(fnPtr),
            solver_sec::mbaXor(effectiveNonce, poison),
            slot,
            solver_sec::kDomainMefs);
    };

    while (state != 0U) {
        switch (state) {
        case 0x6A10B1U:
            if (outTable == nullptr || slotCount < solver_sec::Mefs_SlotCount || nonce == 0) {
                rc = -1;
                state = 0U;
            } else {
                poison = solver_sec::antiDebugPoison(
                    reinterpret_cast<const void *>(&_z8m2p),
                    reinterpret_cast<const void *>(&fem_solve));
                state = solver_sec::opaqueTrue(nonce ^ slotCount) ? 0x4B20C2U : 0x8800FFU;
            }
            break;
        case 0x4B20C2U: {
            const std::uint64_t expectedTag = solver_sec::computeAuthTag(
                solver_sec::mbaXor(nonce, poison), solver_sec::kDomainMefs);
            if (authTag != expectedTag) {
                g_mefsSessionKey = 0;
                rc = -2;
                state = 0U;
            } else {
                effectiveNonce = nonce;
                g_mefsSessionKey = solver_sec::mbaXor(
                    solver_sec::deriveSessionKey(nonce, authTag, solver_sec::kDomainMefs),
                    poison);
                state = solver_sec::opaqueFalse(authTag) ? 0x8800FFU : 0x2C30D3U;
            }
            break;
        }
        case 0x2C30D3U:
            seal(solver_sec::Mefs_Create, &fem_create);
            seal(solver_sec::Mefs_Destroy, &fem_destroy);
            seal(solver_sec::Mefs_Version, &fem_version);
            seal(solver_sec::Mefs_AddMaterial, &fem_add_material);
            seal(solver_sec::Mefs_SetMaterialOrthotropy, &fem_set_material_orthotropy);
            seal(solver_sec::Mefs_AddNode, &fem_add_node);
            seal(solver_sec::Mefs_AddShell, &fem_add_shell);
            seal(solver_sec::Mefs_AddBeam, &fem_add_beam);
            seal(solver_sec::Mefs_AddSpring, &fem_add_spring);
            seal(solver_sec::Mefs_AddTie, &fem_add_tie);
            seal(solver_sec::Mefs_AddTieMasked, &fem_add_tie_masked);
            seal(solver_sec::Mefs_AddCase, &fem_add_case);
            state = solver_sec::opaqueTrue(effectiveNonce) ? 0x9D40E4U : 0x8800FFU;
            break;
        case 0x9D40E4U:
            seal(solver_sec::Mefs_LoadNode, &fem_load_node);
            seal(solver_sec::Mefs_LoadShellPressure, &fem_load_shell_pressure);
            seal(solver_sec::Mefs_LoadShellBody, &fem_load_shell_body);
            seal(solver_sec::Mefs_LoadShellThermal, &fem_load_shell_thermal);
            seal(solver_sec::Mefs_LoadBeamUniform, &fem_load_beam_uniform);
            seal(solver_sec::Mefs_Solve, &fem_solve);
            seal(solver_sec::Mefs_SpringIterations, &fem_spring_iterations);
            seal(solver_sec::Mefs_MaxWarp, &fem_max_warp);
            seal(solver_sec::Mefs_GetDisplacements, &fem_get_displacements);
            seal(solver_sec::Mefs_GetReactions, &fem_get_reactions);
            seal(solver_sec::Mefs_GetShellResults, &fem_get_shell_results);
            seal(solver_sec::Mefs_GetBeamForces, &fem_get_beam_forces);
            rc = 0;
            state = 0U;
            break;
        default:
            rc = -3;
            state = 0U;
            break;
        }
    }
    return rc;
}

}
