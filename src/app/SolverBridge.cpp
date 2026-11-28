#include "SolverBridge.h"

#include "BarFrame.h"

#include <unordered_map>
#include "../../Structuradll/src/core/solver_security.h"
#include "../../client/dll_cipher.h"
#include "../../client/license_client.h"
#include "../../client/license_config.h"
#include "../../client/export_gatekeeper.h"
#include "../../client/sha256.h"

#include <QLibrary>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>

namespace Structura::App {

namespace {

using StructuraSolveFn = int (*) (
    int, const int*, const double*, const double*,
    int, const int*, const double*, const double*, const double*, const double*,
    int, const int*, const double*, const double*, const double*, const int*,
    int, const int*, const int*, const int*, const int*, const int*,
    int, const int*, const double*,
    int, const int*, const int*, const double*, const double*, const double*,
    double*, double*, double*, double*,
    int, char*);

using StructuraSolveV2Fn = int (*) (
    int, const int*, const double*, const double*,
    int, const int*, const double*, const double*, const double*, const double*,
    int, const int*, const double*, const double*, const double*, const int*,
    int, const int*, const int*, const int*, const int*, const int*,
    int, const int*, const double*,
    int, const int*, const int*, const double*, const double*, const double*,
    int, const int*, const int*, const double*, const double*, const double*,
    double*, double*, double*, double*,
    int, char*);

using StructuraSolveV4Fn = int (*) (
    int, const int*, const double*, const double*,
    int, const int*, const double*, const double*, const double*, const double*,
    int, const int*, const double*, const double*, const double*, const int*,
    int, const int*, const int*, const int*, const int*, const int*,
    int, const int*, const double*,
    int, const int*, const int*, const double*, const double*, const double*,
    int, const int*, const int*, const double*, const double*, const double*,
    const double*, const double*, const int*,
    double*, double*, double*, double*,
    int, char*);

using StructuraSolveV3Fn = int (*) (
    int, const int*, const double*, const double*,
    int, const int*, const double*, const double*, const double*, const double*,
    int, const int*, const double*, const double*, const double*, const int*,
    int, const int*, const int*, const int*, const int*, const int*,
    int, const int*, const double*,
    int, const int*, const int*, const double*, const double*, const double*,
    int, const int*, const int*, const double*, const double*, const double*,
    const double*, const double*,
    double*, double*, double*, double*,
    int, char*);

[[nodiscard]] std::string errorFromLibrary(QLibrary &library)
{
    const QString message = library.errorString();
    if (message.isEmpty()) {
        return "Unable to load solver shared library.";
    }
    return message.toStdString();
}

} // namespace

namespace {

constexpr double kPi = 3.14159265358979323846;

/// Giro de compensacao entre o referencial do programa e a base do solver (ver app/BarFrame.h); soma-se ao giro da secao enviado ao solver.
double referenceToSolverBaseDeg(const SolverBridge::ModelData &model, const SolverBridge::MemberData &member)
{
    const SolverBridge::NodeData *a = nullptr;
    const SolverBridge::NodeData *b = nullptr;
    for (const auto &node : model.nodes) {
        if (node.id == member.nodeI) {
            a = &node;
        }
        if (node.id == member.nodeJ) {
            b = &node;
        }
    }
    if (!a || !b) {
        return 0.0;
    }
    Structura::App::BarFrame::Vec3 x {b->x - a->x, b->y - a->y, b->z - a->z};
    const double length = Structura::App::BarFrame::norm(x);
    if (length < 1e-12) {
        return 0.0;
    }
    x = {x[0] / length, x[1] / length, x[2] / length};
    return Structura::App::BarFrame::referenceToSolverBaseDeg(x);
}

/// Giro da secao (quarto de volta, 90 ou 270 graus): as rotacoes em torno de y e z trocam de lugar entre a barra e a secao.
bool isQuarterTurn(double degrees)
{
    return std::abs(std::fmod(std::abs(degrees), 180.0) - 90.0) < 1e-6;
}

void swapPair(std::array<int, 12> &v, std::size_t a, std::size_t b)
{
    std::swap(v[a], v[b]);
}

/**
 * O solver gira os eixos locais da barra junto com a secao (y', z' = y, z girados em torno de x). Para o resto do programa, porem, o giro da
 * secao so muda a geometria e as propriedades geometricas do perfil: os eixos locais da barra seguem sendo os definidos pela geometria. Esta funcao
 * converte para o referencial do solver tudo o que foi dado nos eixos locais da barra (cargas e momentos locais, articulacoes, molas e rotulas),
 * e o final de solve() devolve os esforcos e o vetor auxiliar nos eixos locais da barra.
 *   v_y' = cos.v_y + sen.v_z ;  v_z' = -sen.v_y + cos.v_z
 */
SolverBridge::ModelData toSolverFrame(const SolverBridge::ModelData &in)
{
    using Data = SolverBridge::ModelData;
    Data out = in;
    std::unordered_map<int, double> rotation;
    for (const auto &member : in.members) {
        if (member.sectionRotationDeg != 0.0) {
            rotation[member.id] = member.sectionRotationDeg;
        }
    }
    auto forward = [](double degrees, double &a, double &b) {
        const double c = std::cos(degrees * kPi / 180.0), s = std::sin(degrees * kPi / 180.0);
        const double na = c * a + s * b, nb = -s * a + c * b;
        a = na;
        b = nb;
    };
    for (auto &load : out.memberLoads) {
        const auto it = rotation.find(load.memberId);
        if (it != rotation.end() && load.coordSystem == SolverBridge::MemberLoadCoordSystem::Local) {
            forward(it->second, load.qy, load.qz);
        }
    }
    for (auto &moment : out.memberMoments) {
        const auto it = rotation.find(moment.memberId);
        if (it != rotation.end() && moment.coordSystem == SolverBridge::MemberLoadCoordSystem::Local) {
            forward(it->second, moment.my, moment.mz);
        }
    }
    for (auto &action : out.memberActions) {
        const auto it = rotation.find(action.memberId);
        if (it != rotation.end() && action.coordSystem == SolverBridge::MemberLoadCoordSystem::Local) {
            forward(it->second, action.v1[1], action.v1[2]);
            forward(it->second, action.v2[1], action.v2[2]);
        }
    }
    for (auto &member : out.members) {
        if (isQuarterTurn(member.sectionRotationDeg)) {
            for (const std::size_t base : {std::size_t {0}, std::size_t {6}}) {
                swapPair(member.releases, base + 1, base + 2);
                swapPair(member.releases, base + 4, base + 5);
            }
        }
    }
    for (auto &spring : out.endSprings) {
        const auto it = rotation.find(spring.memberId);
        if (it != rotation.end() && isQuarterTurn(it->second)) {
            for (const std::size_t base : {std::size_t {0}, std::size_t {6}}) {
                std::swap(spring.k[base + 1], spring.k[base + 2]);
                std::swap(spring.k[base + 4], spring.k[base + 5]);
            }
        }
    }
    for (auto &hinge : out.plasticHinges) {
        const auto it = rotation.find(hinge.memberId);
        if (it != rotation.end() && isQuarterTurn(it->second)) {
            const int base = hinge.dof >= 9 ? 9 : 3;
            const int local = hinge.dof - base;
            if (local == 1 || local == 2) {
                hinge.dof = base + (local == 1 ? 2 : 1);
            }
        }
    }
    return out;
}

// Carga de barra em GLOBAL. coordSystem Local usa a mesma convencao de eixos do solver/servidor
// (x = I->J; k = Z, Y ou X global, o primeiro nao paralelo; z = x*k; y = z*x; giro da secao em x).
// O servidor de licencas confere o equilibrio com a mesma formula: as duas pontas PRECISAM coincidir.
std::array<double, 3> memberLoadToGlobal(const SolverBridge::MemberLoadData &ml,
                                         const std::array<double, 3> &ri, const std::array<double, 3> &rj,
                                         double rotationDeg)
{
    std::array<double, 3> q{{ml.qx, ml.qy, ml.qz}};
    if (ml.coordSystem == SolverBridge::MemberLoadCoordSystem::Global) {
        return q;
    }
    auto cross = [](const std::array<double, 3> &a, const std::array<double, 3> &b) {
        return std::array<double, 3>{{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}};
    };
    auto norm = [](const std::array<double, 3> &v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); };
    std::array<double, 3> x{{rj[0] - ri[0], rj[1] - ri[1], rj[2] - ri[2]}};
    const double L = norm(x);
    if (L < 1e-10) {
        return q;
    }
    for (double &c : x) c /= L;
    const std::array<std::array<double, 3>, 4> ks{{{{0.0, 0.0, 1.0}}, {{0.0, 1.0, 0.0}}, {{1.0, 0.0, 0.0}}, {{0.7071067811865476, 0.7071067811865476, 0.0}}}};
    std::array<double, 3> z{{0.0, 0.0, 0.0}};
    for (const auto &k : ks) {
        z = cross(x, k);
        if (norm(z) > 1e-10) break;
    }
    const double zn = norm(z);
    if (zn < 1e-10) {
        return q;
    }
    for (double &c : z) c /= zn;
    std::array<double, 3> y = cross(z, x);
    const double yn = norm(y);
    for (double &c : y) c /= yn;
    if (std::abs(rotationDeg) > 0.0) {
        const double r = rotationDeg * 3.14159265358979323846 / 180.0;
        const double c = std::cos(r), sn = std::sin(r);
        const auto y0 = y, z0 = z;
        for (int i = 0; i < 3; ++i) {
            y[i] = c * y0[i] + sn * z0[i];
            z[i] = -sn * y0[i] + c * z0[i];
        }
    }
    return {{q[0] * x[0] + q[1] * y[0] + q[2] * z[0],
             q[0] * x[1] + q[1] * y[1] + q[2] * z[1],
             q[0] * x[2] + q[1] * y[2] + q[2] * z[2]}};
}

std::string buildModelJsonAndReactions(const SolverBridge::ModelData &model,
                                      structura_license::ReactionTotals &outReactions)
{
    double F[3] = {0.0, 0.0, 0.0};
    double M[3] = {0.0, 0.0, 0.0};

    std::unordered_map<int, std::array<double, 3>> nodeCoords;
    for (const auto &n : model.nodes) {
        nodeCoords[n.id] = {{n.x, n.y, n.z}};
    }
    // Nos das cascas acopladas ("Acoplar as barras"): ficam em model.shell, nao em model.nodes, mas as cargas nodais
    // sobre eles estao em model.nodalLoads. Sem as coordenadas deles o cliente descartava essas cargas do total de
    // reacoes enquanto o servidor (que recompoe o lado aplicado pelo JSON) as somava na origem: EQUILIBRIUM_FAIL.
    std::vector<const SolverBridge::ShellNodeData *> shellOnlyNodes;
    if (model.shell) {
        for (const auto &n : model.shell->nodes) {
            if (nodeCoords.emplace(n.id, std::array<double, 3>{{n.x, n.y, n.z}}).second) {
                shellOnlyNodes.push_back(&n);
            }
        }
    }

    // 1. Cargas Nodais
    for (const auto &nl : model.nodalLoads) {
        auto it = nodeCoords.find(nl.nodeId);
        if (it != nodeCoords.end()) {
            double x = it->second[0], y = it->second[1], z = it->second[2];
            double fx = nl.values[0], fy = nl.values[1], fz = nl.values[2];
            double mx = nl.values[3], my = nl.values[4], mz = nl.values[5];
            F[0] += fx; F[1] += fy; F[2] += fz;
            M[0] += (y * fz - z * fy) + mx;
            M[1] += (z * fx - x * fz) + my;
            M[2] += (x * fy - y * fx) + mz;
        }
    }

    // 2. Cargas de Barra (Uniformemente distribuídas)
    for (const auto &ml : model.memberLoads) {
        const SolverBridge::MemberData *foundMember = nullptr;
        for (const auto &m : model.members) {
            if (m.id == ml.memberId) {
                foundMember = &m;
                break;
            }
        }
        if (foundMember) {
            auto itI = nodeCoords.find(foundMember->nodeI);
            auto itJ = nodeCoords.find(foundMember->nodeJ);
            if (itI != nodeCoords.end() && itJ != nodeCoords.end()) {
                double xi = itI->second[0], yi = itI->second[1], zi = itI->second[2];
                double xj = itJ->second[0], yj = itJ->second[1], zj = itJ->second[2];
                double dx = xj - xi, dy = yj - yi, dz = zj - zi;
                double L = std::sqrt(dx*dx + dy*dy + dz*dz);
                double xm = 0.5 * (xi + xj), ym = 0.5 * (yi + yj), zm = 0.5 * (zi + zj);

                const auto qg = memberLoadToGlobal(ml, {{xi, yi, zi}}, {{xj, yj, zj}}, foundMember->sectionRotationDeg);
                double fx = qg[0] * L, fy = qg[1] * L, fz = qg[2] * L;
                F[0] += fx; F[1] += fy; F[2] += fz;
                M[0] += (ym * fz - zm * fy);
                M[1] += (zm * fx - xm * fz);
                M[2] += (xm * fy - ym * fx);
            }
        }
    }

    outReactions.F[0] = -F[0];
    outReactions.F[1] = -F[1];
    outReactions.F[2] = -F[2];
    outReactions.M_origin[0] = -M[0];
    outReactions.M_origin[1] = -M[1];
    outReactions.M_origin[2] = -M[2];

    std::string json = "{\"nodes\":[";
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"id\":" + std::to_string(model.nodes[i].id) +
                ",\"x\":" + std::to_string(model.nodes[i].x) +
                ",\"y\":" + std::to_string(model.nodes[i].y) +
                ",\"z\":" + std::to_string(model.nodes[i].z) + "}";
    }
    for (const auto *n : shellOnlyNodes) {
        json += std::string(json.back() == '[' ? "" : ",") +
                "{\"id\":" + std::to_string(n->id) +
                ",\"x\":" + std::to_string(n->x) +
                ",\"y\":" + std::to_string(n->y) +
                ",\"z\":" + std::to_string(n->z) + "}";
    }
    json += "],\"members\":[";
    for (size_t i = 0; i < model.members.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"id\":" + std::to_string(model.members[i].id) +
                ",\"node_i\":" + std::to_string(model.members[i].nodeI) +
                ",\"node_j\":" + std::to_string(model.members[i].nodeJ) +
                ",\"section_rotation_deg\":" + std::to_string(model.members[i].sectionRotationDeg) + "}";
    }
    json += "],\"nodal_loads\":[";
    for (size_t i = 0; i < model.nodalLoads.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"node_id\":" + std::to_string(model.nodalLoads[i].nodeId) + ",\"values\":[";
        for (int k = 0; k < 6; ++k) {
            if (k > 0) json += ",";
            json += std::to_string(model.nodalLoads[i].values[k]);
        }
        json += "]}";
    }
    json += "],\"member_loads\":[";
    for (size_t i = 0; i < model.memberLoads.size(); ++i) {
        if (i > 0) json += ",";
        // sempre enviado em GLOBAL (coord_system 1): a conversao local->global ja foi feita acima
        std::array<double, 3> qg{{model.memberLoads[i].qx, model.memberLoads[i].qy, model.memberLoads[i].qz}};
        for (const auto &m : model.members) {
            if (m.id == model.memberLoads[i].memberId) {
                auto ci = nodeCoords.find(m.nodeI), cj = nodeCoords.find(m.nodeJ);
                if (ci != nodeCoords.end() && cj != nodeCoords.end()) {
                    qg = memberLoadToGlobal(model.memberLoads[i], ci->second, cj->second, m.sectionRotationDeg);
                }
                break;
            }
        }
        json += "{\"member_id\":" + std::to_string(model.memberLoads[i].memberId) +
                ",\"coord_system\":1" +
                ",\"q\":[" + std::to_string(qg[0]) + "," + std::to_string(qg[1]) + "," + std::to_string(qg[2]) + "]}";
    }
    json += "],\"member_moments\":[],\"member_actions\":[]}";

    return json;
}

} // namespace

SolverBridge::SolverBridge(QString libraryPath)
    : m_libraryPath(std::move(libraryPath))
{
}

SolverBridge::LicenseSummary SolverBridge::licenseSummary(const ModelData &model)
{
    structura_license::ReactionTotals reactions;
    LicenseSummary out;
    out.modelJson = buildModelJsonAndReactions(model, reactions);
    for (int i = 0; i < 3; ++i) {
        out.reactionForce[static_cast<std::size_t>(i)] = reactions.F[i];
        out.reactionMoment[static_cast<std::size_t>(i)] = reactions.M_origin[i];
    }
    return out;
}

SolverBridge::Result SolverBridge::solve(const ModelData &input, bool isPreview) const
{
    // giro da secao: o solver gira os eixos locais, o programa nao (ver toSolverFrame)
    bool anyRotation = false;
    for (const auto &member : input.members) {
        anyRotation = anyRotation || member.sectionRotationDeg != 0.0;
    }
    std::vector<double> baseOffsetDeg(input.members.size(), 0.0);   // verticais: referencia do programa x base do solver
    for (std::size_t i = 0; i < input.members.size(); ++i) {
        baseOffsetDeg[i] = referenceToSolverBaseDeg(input, input.members[i]);
    }
    ModelData converted;
    if (anyRotation) {
        converted = toSolverFrame(input);
    }
    const ModelData &model = anyRotation ? converted : input;
    // Marco M5: Carga segura da DLL cifrada em repouso (decrypt-to-temp + LoadLibrary + wipe)
    auto resolveSolverPath = [](const std::string &pathOrName) -> std::string {
        if (pathOrName.empty()) {
            return pathOrName;
        }
        std::filesystem::path p = std::filesystem::u8path(pathOrName);
        if (std::filesystem::exists(p)) {
            return pathOrName;
        }
        std::filesystem::path appDir = std::filesystem::u8path(structura_license::GetAppInstallDir());
        std::filesystem::path candidate = appDir / p.filename();
        if (std::filesystem::exists(candidate)) {
            return candidate.u8string();
        }
        return pathOrName;
    };

    const auto session = structura_license::GetActiveClientSession();
    std::string encName = "rds.enc";
    if (!m_libraryPath.isEmpty()) {
        std::filesystem::path p = std::filesystem::u8path(m_libraryPath.toUtf8().toStdString());
        if (p.extension() == ".dll") {
            p.replace_extension(".enc");
        }
        encName = p.u8string();
    }
    const std::string encPath = resolveSolverPath(encName);
#if defined(STRUCTURA_DEV_BUILD)
    // Desenvolvimento: se o chamador informou uma .dll existente, ela e a DLL em claro (sem depender de servidor)
    std::string plainPath = resolveSolverPath("rds.dll");
    if (!m_libraryPath.isEmpty()) {
        std::filesystem::path candidate = std::filesystem::u8path(m_libraryPath.toUtf8().toStdString());
        candidate.replace_extension(".dll");   // o chamador pode informar a .dll ou a .enc: vale a .dll vizinha, se existir
        if (std::filesystem::exists(candidate)) {
            plainPath = candidate.u8string();
        }
    }
    structura_license::ScopedDecryptedDll solverDll = structura_license::LoadEncryptedDllWithFallback(
        encPath, plainPath, session);
#else
    // Build de Release: Fail-closed estrito sem fallback para texto claro
    structura_license::ScopedDecryptedDll solverDll = structura_license::LoadEncryptedDll(
        encPath, session);
#endif

    if (!solverDll.ok) {
        throw SolverBridgeError("Nao foi possivel carregar a biblioteca do solver (rds): " + solverDll.errorMessage);
    }

    using GatekeeperFn = int (*)(std::uint64_t, std::uint64_t, std::uint64_t *, std::uint32_t);
    const std::string gateName = OBF_STR("_z8r4q");
    auto gatekeeper = reinterpret_cast<GatekeeperFn>(solverDll.resolve(gateName.c_str()));
    if (!gatekeeper) {
        throw SolverBridgeError("Failed to authenticate solver shared library (rds).");
    }
    const std::uint64_t nonce = solver_sec::generateNonce(solverDll.handle);
    const std::uint64_t authTag = solver_sec::computeAuthTag(nonce, solver_sec::kDomainRds);
    const std::uint64_t sessionKey = solver_sec::deriveSessionKey(nonce, authTag, solver_sec::kDomainRds);
    std::array<std::uint64_t, solver_sec::Rds_SlotCount> sealedTable {};
    if (gatekeeper(nonce, authTag, sealedTable.data(), solver_sec::Rds_SlotCount) != 0) {
        throw SolverBridgeError("Solver library authentication handshake rejected (rds).");
    }

    auto unlockFn = reinterpret_cast<structura_license::GatekeeperUnlockFn>(solverDll.resolve("_z8r4q_unlock"));
    auto lockFn = reinterpret_cast<structura_license::GatekeeperLockFn>(solverDll.resolve("_z8r4q_lock"));

    uint64_t s_mask = 0;

    // Marco M4/M6/Licenciamento: Validação de Keystone obrigatória para cálculo oficial (não preview)
    if (!isPreview) {
        structura_license::ReactionTotals reactions;
        std::string modelJson = buildModelJsonAndReactions(model, reactions);

        structura_license::Sha256 hashModel;
        hashModel.Update(modelJson.data(), modelJson.size());
        std::string modelHash = hashModel.FinalHex();

        char nonceBuf[32];
        snprintf(nonceBuf, sizeof(nonceBuf), "%016llx", static_cast<unsigned long long>(nonce));
        structura_license::Sha256 hashDigest;
        std::string digestContext = modelHash + ":" + nonceBuf;
        hashDigest.Update(digestContext.data(), digestContext.size());
        std::string resultDigest = hashDigest.FinalHex();

        structura_license::RegisterActiveGatekeeperTable(nonce, sealedTable.data(), solver_sec::Rds_SlotCount, unlockFn, lockFn);

        structura_license::KeystoneArtifact keystone;
        std::string keystoneError;
        bool keystoneOk = structura_license::RequestComputeKeystone(
            structura_license::GetLicenseServerUrl(),
            session,
            modelHash,
            modelJson,
            reactions,
            resultDigest,
            structura_license::GetAppInstallDir(),
            structura_license::GetReleasePublicKey(),
            structura_license::GetServerPublicKey(),
            keystone,
            keystoneError
        );

#if !defined(STRUCTURA_DEV_BUILD)
        // Build comercial: sem Keystone valido o calculo e sempre bloqueado (nao existe modo de teste por ambiente).
        if (!keystoneOk) {
            throw SolverBridgeError("Cálculo bloqueado: falha na validação do Keystone com o servidor de licenças (" + keystoneError + ").");
        }
#endif
        if (keystoneOk) {
            std::memcpy(&s_mask, keystone.sessionSecretS.data(), sizeof(s_mask));
            structura_license::GatekeeperUnlock(nonce, s_mask, sealedTable.data(), solver_sec::Rds_SlotCount, unlockFn);
            structura_license::GetGlobalExportGatekeeper().Unlock(nonce, s_mask);
        }
#if defined(STRUCTURA_DEV_BUILD)
        else if (std::getenv("STRUCTURA_TEST_DLL_KEY")) {
            s_mask = 0x5A5A5A5A5A5A5A5AULL;
            if (unlockFn) {
                unlockFn(nonce, s_mask, sealedTable.data(), solver_sec::Rds_SlotCount);
            }
        }
#endif
    }

    auto resolveSlot = [&](solver_sec::RdsSlot slot) -> QFunctionPointer {
        if (solver_sec::isRdsExportSlot(static_cast<uint32_t>(slot)) && s_mask == 0) {
            return nullptr;
        }
        const uint64_t domain = (s_mask != 0 && solver_sec::isRdsExportSlot(static_cast<uint32_t>(slot)))
            ? (solver_sec::kDomainRds ^ s_mask)
            : solver_sec::kDomainRds;
        return solver_sec::unsealPointer<QFunctionPointer>(
            sealedTable[static_cast<std::size_t>(slot)],
            nonce,
            static_cast<std::uint32_t>(slot),
            domain);
    };

    auto *rawSymbolV4 = resolveSlot(solver_sec::Rds_SolveV4);
    auto *rawSymbolV3 = resolveSlot(solver_sec::Rds_SolveV3);
    auto *rawSymbolV2 = resolveSlot(solver_sec::Rds_SolveV2);
    auto *rawSymbol = resolveSlot(solver_sec::Rds_Solve);
    if (!rawSymbolV4 && !rawSymbolV3 && !rawSymbolV2 && !rawSymbol) {
        throw SolverBridgeError("Failed to resolve exported function structura_solve.");
    }
    if (!rawSymbolV4 && !rawSymbolV3 && !rawSymbolV2 && !model.memberMoments.empty()) {
        throw SolverBridgeError("A DLL do solver nao suporta momentos distribuidos em barras (structura_solve_v2 ausente).");
    }
    const bool needsPlacement = std::any_of(model.members.begin(), model.members.end(),
                                            [](const MemberData &member) { return member.hasPlacement(); });
    const bool needsReleases = std::any_of(model.members.begin(), model.members.end(),
                                           [](const MemberData &member) { return member.hasReleases(); });
    if (!rawSymbolV4 && needsReleases) {
        throw SolverBridgeError("A DLL do solver nao suporta articulacoes de extremidade (structura_solve_v4 ausente). "
                                "Recompile Structuradll e copie rds.dll para a raiz.");
    }
    if (!rawSymbolV4 && !rawSymbolV3 && needsPlacement) {
        throw SolverBridgeError("A DLL do solver nao suporta giro/excentricidade de secao (structura_solve_v3 ausente). "
                                "Recompile Structuradll e copie rds.dll para a raiz.");
    }

    using SetOptionsFn = void (*)(int, int, double);
    auto setOptions = reinterpret_cast<SetOptionsFn>(resolveSlot(solver_sec::Rds_SetOptions));
    if (model.secondOrder && !setOptions) {
        throw SolverBridgeError("A DLL do solver nao suporta analise de segunda ordem (structura_set_options ausente). "
                                "Recompile Structuradll e copie rds.dll para a raiz.");
    }
    if (setOptions) {
        setOptions(model.secondOrder ? 1 : 0, 0, 0.0);
    }
    using SetAnalysisFn = void (*)(int, int, int);
    auto setAnalysis = reinterpret_cast<SetAnalysisFn>(resolveSlot(solver_sec::Rds_SetAnalysisOptions));
    using SetShearAreasFn = void (*)(int, const int *, const double *, const double *);
    auto setShearAreas = reinterpret_cast<SetShearAreasFn>(resolveSlot(solver_sec::Rds_SetShearAreas));
    if ((model.shearDeformation || model.stabilityFunctions || model.loadSteps > 1) && (!setAnalysis || (model.shearDeformation && !setShearAreas))) {
        throw SolverBridgeError("A DLL do solver nao suporta deformacao por cortante/funcoes de estabilidade (structura_set_analysis_options ausente). "
                                "Recompile Structuradll e copie rds.dll para a raiz.");
    }
    if (setAnalysis) {
        setAnalysis(model.shearDeformation ? 1 : 0, model.stabilityFunctions ? 1 : 0, model.loadSteps);
    }
    if (setShearAreas) {
        std::vector<int> shearIds;
        std::vector<double> shearY, shearZ;
        if (model.shearDeformation) {
            for (const auto &section : model.sections) {
                shearIds.push_back(section.id);
                shearY.push_back(section.shearAreaY);
                shearZ.push_back(section.shearAreaZ);
            }
        }
        setShearAreas(static_cast<int>(shearIds.size()), shearIds.data(), shearY.data(), shearZ.data());
    }
    {
        using SetWarpingSectionsFn = void (*)(int, const int *, const double *);
        using SetWarpingMembersFn = void (*)(int, const int *, const int *, const int *);
        auto setWarpingSections = reinterpret_cast<SetWarpingSectionsFn>(resolveSlot(solver_sec::Rds_SetWarpingSections));
        auto setWarpingMembers = reinterpret_cast<SetWarpingMembersFn>(resolveSlot(solver_sec::Rds_SetWarpingMembers));
        std::vector<int> sectionIds, memberIds, modeStart, modeEnd;
        std::vector<double> constants;
        for (const auto &member : model.members) {
            if (member.warping) {
                memberIds.push_back(member.id);
                modeStart.push_back(member.warpingStartMode);
                modeEnd.push_back(member.warpingEndMode);
            }
        }
        if (!memberIds.empty()) {
            for (const auto &section : model.sections) {
                if (section.warpingConstant > 0.0) {
                    sectionIds.push_back(section.id);
                    constants.push_back(section.warpingConstant);
                }
            }
        }
        if (!memberIds.empty() && (!setWarpingSections || !setWarpingMembers)) {
            throw SolverBridgeError("A DLL do solver nao suporta torcao com empenamento (structura_set_warping_* ausente). "
                                    "Recompile Structuradll e copie rds.dll para a raiz.");
        }
        if (setWarpingSections && setWarpingMembers) {
            setWarpingSections(static_cast<int>(sectionIds.size()), sectionIds.data(), constants.data());
            setWarpingMembers(static_cast<int>(memberIds.size()), memberIds.data(), modeStart.data(), modeEnd.data());
        }
        using SetShearCentersFn = void (*)(int, const int *, const double *, const double *);
        auto setShearCenters = reinterpret_cast<SetShearCentersFn>(resolveSlot(solver_sec::Rds_SetShearCenters));
        std::vector<int> centerIds;
        std::vector<double> centerY, centerZ;
        for (const auto &section : model.sections) {
            if (section.warpingConstant > 0.0 && (section.shearCenterY != 0.0 || section.shearCenterZ != 0.0)) {
                centerIds.push_back(section.id);
                centerY.push_back(section.shearCenterY);
                centerZ.push_back(section.shearCenterZ);
            }
        }
        if (!centerIds.empty() && !memberIds.empty() && !setShearCenters) {
            throw SolverBridgeError("A DLL do solver nao suporta o centro de cisalhamento fora do centroide (structura_set_shear_centers ausente). "
                                    "Recompile Structuradll e copie rds.dll para a raiz.");
        }
        if (setShearCenters) {
            setShearCenters(static_cast<int>(centerIds.size()), centerIds.data(), centerY.data(), centerZ.data());
        }
    }
    {
        using SetTaperFn = void (*)(int, const int *, const double *, const double *, const double *, const double *, const double *, const int *);
        auto setTaper = reinterpret_cast<SetTaperFn>(resolveSlot(solver_sec::Rds_SetTaper));
        std::vector<int> ids, segments;
        std::vector<double> d1, d2, bf, tw, tf;
        for (const auto &member : model.members) {
            if (member.taper) {
                ids.push_back(member.id);
                d1.push_back(member.taperD1);
                d2.push_back(member.taperD2);
                bf.push_back(member.taperBf);
                tw.push_back(member.taperTw);
                tf.push_back(member.taperTf);
                segments.push_back(member.taperSegments);
            }
        }
        if (!ids.empty() && !setTaper) {
            throw SolverBridgeError("A DLL do solver nao suporta barras de secao variavel (structura_set_taper ausente). "
                                    "Recompile Structuradll e copie rds.dll para a raiz.");
        }
        if (setTaper) {
            setTaper(static_cast<int>(ids.size()), ids.data(), d1.data(), d2.data(), bf.data(), tw.data(), tf.data(), segments.data());
        }
    }
    using SetRigidZonesFn = void (*)(int, const int *, const double *, const double *);
    auto setRigidZones = reinterpret_cast<SetRigidZonesFn>(resolveSlot(solver_sec::Rds_SetRigidZones));
    {
        std::vector<int> rigidIds;
        std::vector<double> rigidStart, rigidEnd;
        for (const auto &member : model.members) {
            if (member.rigidStart > 0.0 || member.rigidEnd > 0.0) {
                rigidIds.push_back(member.id);
                rigidStart.push_back(member.rigidStart);
                rigidEnd.push_back(member.rigidEnd);
            }
        }
        if (!rigidIds.empty() && !setRigidZones) {
            throw SolverBridgeError("A DLL do solver nao suporta zonas rigidas (structura_set_rigid_zones ausente). "
                                    "Recompile Structuradll e copie rds.dll para a raiz.");
        }
        if (setRigidZones) {
            setRigidZones(static_cast<int>(rigidIds.size()), rigidIds.data(), rigidStart.data(), rigidEnd.data());
        }
    }
    {
        using SetMemberActionsFn = void (*)(int, const int *, const int *, const int *, const double *, const double *, const double *, const double *);
        auto setMemberActions = reinterpret_cast<SetMemberActionsFn>(resolveSlot(solver_sec::Rds_SetMemberActions));
        if (!model.memberActions.empty() && !setMemberActions) {
            throw SolverBridgeError("A DLL do solver nao suporta cargas de barra com posicao (trapezoidais, pontuais, temperatura; structura_set_member_actions ausente). "
                                    "Recompile Structuradll e copie rds.dll para a raiz.");
        }
        if (setMemberActions) {
            std::vector<int> ids, kinds, systems;
            std::vector<double> a, b, v1, v2;
            for (const auto &action : model.memberActions) {
                ids.push_back(action.memberId);
                kinds.push_back(action.kind);
                systems.push_back(action.coordSystem == MemberLoadCoordSystem::Global ? 1 : 0);
                a.push_back(action.a);
                b.push_back(action.b);
                v1.insert(v1.end(), action.v1.begin(), action.v1.end());
                v2.insert(v2.end(), action.v2.begin(), action.v2.end());
            }
            setMemberActions(static_cast<int>(ids.size()), ids.data(), kinds.data(), systems.data(), a.data(), b.data(), v1.data(), v2.data());
        }
    }
    {
        // semirrigidez, rotulas plasticas, vinculos, ligacoes entre nos e controle da analise nao linear (estado global da DLL: sempre redefinido)
        auto *setEndSprings = reinterpret_cast<void (*)(int, const int *, const double *)>(resolveSlot(solver_sec::Rds_SetEndSprings));
        auto *setHinges = reinterpret_cast<void (*)(int, const int *, const int *, const double *, const double *, const double *, const double *, const double *)>(
            resolveSlot(solver_sec::Rds_SetPlasticHinges));
        auto *setConstraints = reinterpret_cast<void (*)(int, const int *, const int *, const int *, const int *, int, const int *, const int *, const int *, const int *,
                                                         int, const int *, const double *, const int *, int, const int *, const int *, const int *, const double *, const int *)>(
            resolveSlot(solver_sec::Rds_SetConstraints));
        auto *setLinks = reinterpret_cast<void (*)(int, const int *, const int *, const int *, const double *, const int *, const double *, const double *, const double *,
                                                   const int *, const double *, const double *)>(resolveSlot(solver_sec::Rds_SetLinks));
        auto *setNonlinear = reinterpret_cast<void (*)(int, int, int, double, int, int, int, int, int, double, double)>(resolveSlot(solver_sec::Rds_SetNonlinearOptions));
        const bool needsNew = !model.endSprings.empty() || !model.plasticHinges.empty() || !model.diaphragms.empty() || !model.rigidLinks.empty()
                              || !model.inclinedSupports.empty() || !model.constraints.empty() || !model.links.empty() || model.nonlinear.force;
        if (needsNew && (!setEndSprings || !setHinges || !setConstraints || !setLinks || !setNonlinear)) {
            throw SolverBridgeError("A DLL do solver nao suporta molas de extremidade, rotulas plasticas, vinculos, ligacoes entre nos ou analise nao linear "
                                    "(funcoes structura_set_* ausentes). Recompile Structuradll e copie rds.dll para a raiz.");
        }
        if (setEndSprings) {
            std::vector<int> ids;
            std::vector<double> k;
            for (const auto &spring : model.endSprings) {
                ids.push_back(spring.memberId);
                k.insert(k.end(), spring.k.begin(), spring.k.end());
            }
            setEndSprings(static_cast<int>(ids.size()), ids.data(), k.data());
        }
        if (setHinges) {
            std::vector<int> ids, dofs;
            std::vector<double> kEl, my, ratio, ny, exponent;
            for (const auto &h : model.plasticHinges) {
                ids.push_back(h.memberId);
                dofs.push_back(h.dof);
                kEl.push_back(h.kElastic);
                my.push_back(h.yieldMoment);
                ratio.push_back(h.hardeningRatio);
                ny.push_back(h.axialYield);
                exponent.push_back(h.exponent);
            }
            setHinges(static_cast<int>(ids.size()), ids.data(), dofs.data(), kEl.data(), my.data(), ratio.data(), ny.data(), exponent.data());
        }
        if (setConstraints) {
            std::vector<int> diaMasters, diaNormals, diaCounts, diaSlaves, linkMasters, linkSlaves, linkKinds, linkDofs, incNodes, incMasks, genCounts, genNodes, genDofs, genReport;
            std::vector<double> incAxes, genCoefs;
            for (const auto &d : model.diaphragms) {
                diaMasters.push_back(d.masterNodeId);
                diaNormals.push_back(d.normalAxis);
                diaCounts.push_back(static_cast<int>(d.slaveNodeIds.size()));
                diaSlaves.insert(diaSlaves.end(), d.slaveNodeIds.begin(), d.slaveNodeIds.end());
            }
            for (const auto &l : model.rigidLinks) {
                linkMasters.push_back(l.masterNodeId);
                linkSlaves.push_back(l.slaveNodeId);
                linkKinds.push_back(l.kind);
                linkDofs.push_back(l.dof);
            }
            for (const auto &s : model.inclinedSupports) {
                incNodes.push_back(s.nodeId);
                incAxes.insert(incAxes.end(), s.axes.begin(), s.axes.end());
                incMasks.push_back(s.mask);
            }
            for (const auto &c : model.constraints) {
                genCounts.push_back(static_cast<int>(c.nodeIds.size()));
                genNodes.insert(genNodes.end(), c.nodeIds.begin(), c.nodeIds.end());
                genDofs.insert(genDofs.end(), c.dofs.begin(), c.dofs.end());
                genCoefs.insert(genCoefs.end(), c.coefs.begin(), c.coefs.end());
                genReport.push_back(c.reportReaction ? 1 : 0);
            }
            setConstraints(static_cast<int>(diaMasters.size()), diaMasters.data(), diaNormals.data(), diaCounts.data(), diaSlaves.data(),
                           static_cast<int>(linkMasters.size()), linkMasters.data(), linkSlaves.data(), linkKinds.data(), linkDofs.data(),
                           static_cast<int>(incNodes.size()), incNodes.data(), incAxes.data(), incMasks.data(),
                           static_cast<int>(genCounts.size()), genCounts.data(), genNodes.data(), genDofs.data(), genCoefs.data(), genReport.data());
        }
        if (setLinks) {
            std::vector<int> ids, nodeI, nodeJ, kinds, tableCounts;
            std::vector<double> axes, k, p1, p2, tableDef, tableForce;
            for (const auto &link : model.links) {
                ids.push_back(link.id);
                nodeI.push_back(link.nodeI);
                nodeJ.push_back(link.nodeJ);
                axes.insert(axes.end(), link.axes.begin(), link.axes.end());
                for (const auto &law : link.laws) {
                    kinds.push_back(law.kind);
                    k.push_back(law.k);
                    p1.push_back(law.p1);
                    p2.push_back(law.p2);
                    tableCounts.push_back(static_cast<int>(law.table.size()));
                    for (const auto &point : law.table) {
                        tableDef.push_back(point.first);
                        tableForce.push_back(point.second);
                    }
                }
            }
            setLinks(static_cast<int>(ids.size()), ids.data(), nodeI.data(), nodeJ.data(), axes.data(), kinds.data(), k.data(), p1.data(), p2.data(), tableCounts.data(),
                     tableDef.data(), tableForce.data());
        }
        if (auto *setSolverOptions = reinterpret_cast<void (*)(int, int, int, double, int)>(resolveSlot(solver_sec::Rds_SetSolverOptions))) {
            setSolverOptions(model.linearSolver, model.eigenSolver, 400, 1e-10, 5000);
        }
        if (setNonlinear) {
            const auto &o = model.nonlinear;
            setNonlinear(o.force ? 1 : 0, o.steps, o.maxIterations, o.tolerance, o.criteria, o.maxCutbacks, o.controlMode, o.controlNodeId, o.controlDof, o.controlTarget,
                         o.loadFactorMax);
        }
    }
    using SetBucklingFn = void (*)(int);
    auto setBuckling = reinterpret_cast<SetBucklingFn>(resolveSlot(solver_sec::Rds_SetBuckling));
    if (model.bucklingModes > 0 && !setBuckling) {
        throw SolverBridgeError("A DLL do solver nao suporta flambagem elastica (structura_set_buckling ausente). "
                                "Recompile Structuradll e copie rds.dll para a raiz.");
    }
    if (setBuckling) {
        setBuckling(model.secondOrder ? 0 : model.bucklingModes);
    }
    using SetModalFn = void (*)(int, int, const int *, const double *, int, const int *, const double *);
    auto setModal = reinterpret_cast<SetModalFn>(resolveSlot(solver_sec::Rds_SetModal));
    if (model.modalModes > 0 && !setModal) {
        throw SolverBridgeError("A DLL do solver nao suporta analise modal (structura_set_modal ausente). "
                                "Recompile Structuradll e copie rds.dll para a raiz.");
    }
    if (setModal) {
        std::vector<int> densityIds, massNodes;
        std::vector<double> densities, masses;
        for (const auto &material : model.materials) {
            densityIds.push_back(material.id);
            densities.push_back(material.density);
        }
        for (const auto &extra : model.extraMasses) {
            massNodes.push_back(extra.first);
            masses.push_back(extra.second);
        }
        setModal(model.secondOrder ? 0 : model.modalModes, static_cast<int>(densityIds.size()), densityIds.data(), densities.data(),
                 static_cast<int>(massNodes.size()), massNodes.data(), masses.data());
    }
    std::vector<int> tensionOnlyIds;
    for (const auto &member : model.members) {
        if (member.tensionOnly) {
            tensionOnlyIds.push_back(member.id);
        }
    }
    using SetTensionOnlyFn = void (*)(int, const int *);
    auto setTensionOnly = reinterpret_cast<SetTensionOnlyFn>(resolveSlot(solver_sec::Rds_SetTensionOnly));
    if (!tensionOnlyIds.empty() && !setTensionOnly) {
        throw SolverBridgeError("A DLL do solver nao suporta tirantes (structura_set_tension_only ausente). "
                                "Recompile Structuradll e copie rds.dll para a raiz.");
    }
    if (setTensionOnly) {
        setTensionOnly(static_cast<int>(tensionOnlyIds.size()), tensionOnlyIds.empty() ? nullptr : tensionOnlyIds.data());
    }

    std::vector<int> materialIds;
    std::vector<double> materialE;
    std::vector<double> materialG;
    materialIds.reserve(model.materials.size());
    materialE.reserve(model.materials.size());
    materialG.reserve(model.materials.size());
    for (const auto &material : model.materials) {
        materialIds.push_back(material.id);
        materialE.push_back(material.e);
        materialG.push_back(material.g);
    }

    std::vector<int> sectionIds;
    std::vector<double> sectionA;
    std::vector<double> sectionIz;
    std::vector<double> sectionIy;
    std::vector<double> sectionJ;
    sectionIds.reserve(model.sections.size());
    sectionA.reserve(model.sections.size());
    sectionIz.reserve(model.sections.size());
    sectionIy.reserve(model.sections.size());
    sectionJ.reserve(model.sections.size());
    for (const auto &section : model.sections) {
        sectionIds.push_back(section.id);
        sectionA.push_back(section.area);
        sectionIz.push_back(section.iz);
        sectionIy.push_back(section.iy);
        sectionJ.push_back(section.j);
    }

    std::vector<int> nodeIds;
    std::vector<double> nodeX;
    std::vector<double> nodeY;
    std::vector<double> nodeZ;
    std::vector<int> nodeRestraints6;
    nodeIds.reserve(model.nodes.size());
    nodeX.reserve(model.nodes.size());
    nodeY.reserve(model.nodes.size());
    nodeZ.reserve(model.nodes.size());
    nodeRestraints6.reserve(model.nodes.size() * 6);
    for (const auto &node : model.nodes) {
        nodeIds.push_back(node.id);
        nodeX.push_back(node.x);
        nodeY.push_back(node.y);
        nodeZ.push_back(node.z);
        for (const int restraint : node.restraints) {
            nodeRestraints6.push_back(restraint != 0 ? 1 : 0);
        }
    }

    // nos das cascas (alem dos nos das barras)
    const std::size_t barNodeCount = model.nodes.size();
    if (model.shell) {
        for (const auto &node : model.shell->nodes) {
            nodeIds.push_back(node.id);
            nodeX.push_back(node.x);
            nodeY.push_back(node.y);
            nodeZ.push_back(node.z);
            for (const int restraint : node.restraints) {
                nodeRestraints6.push_back(restraint != 0 ? 1 : 0);
            }
        }
    }
    const std::size_t totalNodeCount = nodeIds.size();

    // estado das cascas na DLL (global): sempre redefinido, para nao herdar o da chamada anterior
    using ClearShellsFn = void (*)();
    using SetShellsFn = int (*)(int, const int *, const int *, const double *, const double *, const double *, const double *,
                                int, const int *, const int *, const double *,
                                int, const int *, const int *, const int *, const double *, const int *,
                                int, const int *, const double *, const int *, const double *);
    using GetShellResultsFn = int (*)(int, double *, double *);
    auto clearShells = reinterpret_cast<ClearShellsFn>(resolveSlot(solver_sec::Rds_ClearShells));
    auto setShells = reinterpret_cast<SetShellsFn>(resolveSlot(solver_sec::Rds_SetShells));
    auto getShellResults = reinterpret_cast<GetShellResultsFn>(resolveSlot(solver_sec::Rds_GetShellResults));
    const bool hasShells = model.shell && !model.shell->empty();
    if (hasShells && (!setShells || !getShellResults || !rawSymbolV4)) {
        throw SolverBridgeError("A DLL do solver nao suporta cascas acopladas (structura_set_shells ausente). "
                                "Recompile Structuradll e copie rds.dll para a raiz.");
    }
    if (!hasShells && clearShells) {
        clearShells();
    }
    if (hasShells) {
        std::vector<int> shellIds, shellNodes4, loadShells, loadKinds, tieSlaves, tieCounts, tieMasters, tieMasks, springNodes, springCompression;
        std::vector<double> shellE, shellNu, shellDensity, shellThickness, loadValues, tieWeights, springK, springNormals;
        for (const auto &e : model.shell->elements) {
            shellIds.push_back(e.id);
            shellNodes4.insert(shellNodes4.end(), e.nodes.begin(), e.nodes.end());
            shellE.push_back(e.e);
            shellNu.push_back(e.nu);
            shellDensity.push_back(e.density);
            shellThickness.push_back(e.thickness);
        }
        for (const auto &l : model.shellLoads) {
            loadShells.push_back(l.shellId);
            loadKinds.push_back(l.kind);
            loadValues.insert(loadValues.end(), l.v.begin(), l.v.end());
        }
        for (const auto &t : model.shell->ties) {
            tieSlaves.push_back(t.slave);
            tieCounts.push_back(static_cast<int>(t.masters.size()));
            tieMasters.insert(tieMasters.end(), t.masters.begin(), t.masters.end());
            tieWeights.insert(tieWeights.end(), t.weights.begin(), t.weights.end());
            tieMasks.push_back(t.dofMask);
        }
        for (const auto &s : model.shell->springs) {
            springNodes.push_back(s.node);
            springK.insert(springK.end(), s.k.begin(), s.k.end());
            springCompression.push_back(s.compressionOnly ? 1 : 0);
            springNormals.insert(springNormals.end(), s.normal.begin(), s.normal.end());
        }
        const int code = setShells(static_cast<int>(shellIds.size()), shellIds.data(), shellNodes4.data(), shellE.data(), shellNu.data(), shellDensity.data(),
                                   shellThickness.data(), static_cast<int>(loadShells.size()), loadShells.data(), loadKinds.data(), loadValues.data(),
                                   static_cast<int>(tieSlaves.size()), tieSlaves.data(), tieCounts.data(), tieMasters.data(), tieWeights.data(), tieMasks.data(),
                                   static_cast<int>(springNodes.size()), springNodes.data(), springK.data(), springCompression.data(), springNormals.data());
        if (code != 0) {
            throw SolverBridgeError("Dados de cascas invalidos para o solver.");
        }
        using SetShellAlphaFn = int (*)(int, const double *);
        auto setShellAlpha = reinterpret_cast<SetShellAlphaFn>(resolveSlot(solver_sec::Rds_SetShellAlpha));
        std::vector<double> shellAlpha;
        for (const auto &e : model.shell->elements) {
            shellAlpha.push_back(e.alpha);
        }
        const bool anyOrtho = std::any_of(model.shell->elements.begin(), model.shell->elements.end(), [](const auto &e) { return e.orthotropic; });
        if (anyOrtho) {
            using SetShellOrthoFn = int (*)(int, const int *, const double *, const double *, const double *, const double *, const double *, const double *);
            auto setOrtho = reinterpret_cast<SetShellOrthoFn>(resolveSlot(solver_sec::Rds_SetShellOrthotropy));
            if (!setOrtho) {
                throw SolverBridgeError("A DLL do solver nao suporta material ortotropico em cascas (structura_set_shell_orthotropy ausente). "
                                        "Recompile Structuradll e copie rds.dll para a raiz.");
            }
            std::vector<int> flags;
            std::vector<double> e2, nu12, g12, b1, b2, axes;
            for (const auto &e : model.shell->elements) {
                flags.push_back(e.orthotropic ? 1 : 0);
                e2.push_back(e.e2);
                nu12.push_back(e.nu12);
                g12.push_back(e.g12);
                b1.push_back(e.bend1);
                b2.push_back(e.bend2);
                axes.insert(axes.end(), e.axis.begin(), e.axis.end());
            }
            if (setOrtho(static_cast<int>(flags.size()), flags.data(), e2.data(), nu12.data(), g12.data(), b1.data(), b2.data(), axes.data()) != 0) {
                throw SolverBridgeError("Dados de ortotropia das cascas invalidos para o solver.");
            }
        }
        if (setShellAlpha) {
            setShellAlpha(static_cast<int>(shellAlpha.size()), shellAlpha.data());
        } else if (std::any_of(shellAlpha.begin(), shellAlpha.end(), [](double a) { return std::abs(a - 1.2e-5) > 1e-12; })) {
            throw SolverBridgeError("A DLL do solver nao aceita o coeficiente de dilatacao das cascas (structura_set_shell_alpha ausente). "
                                    "Recompile Structuradll e copie rds.dll para a raiz.");
        }
    }

    std::vector<int> memberIds;
    std::vector<int> memberNodeI;
    std::vector<int> memberNodeJ;
    std::vector<int> memberMaterialId;
    std::vector<int> memberSectionId;
    std::vector<double> memberSectionRotationDeg;
    std::vector<double> memberOffsets4;
    std::vector<int> memberReleases12;
    memberIds.reserve(model.members.size());
    memberNodeI.reserve(model.members.size());
    memberNodeJ.reserve(model.members.size());
    memberMaterialId.reserve(model.members.size());
    memberSectionId.reserve(model.members.size());
    for (const auto &member : model.members) {
        memberIds.push_back(member.id);
        memberNodeI.push_back(member.nodeI);
        memberNodeJ.push_back(member.nodeJ);
        memberMaterialId.push_back(member.materialId);
        memberSectionId.push_back(member.sectionId);
        memberSectionRotationDeg.push_back(member.sectionRotationDeg + baseOffsetDeg[static_cast<std::size_t>(&member - model.members.data())]);
        memberOffsets4.push_back(member.offsetY1);
        memberOffsets4.push_back(member.offsetZ1);
        memberOffsets4.push_back(member.offsetY2);
        memberOffsets4.push_back(member.offsetZ2);
        for (const int flag : member.releases) {
            memberReleases12.push_back(flag);
        }
    }

    std::vector<int> nodalLoadNodeIds;
    std::vector<double> nodalLoadValues6;
    nodalLoadNodeIds.reserve(model.nodalLoads.size());
    nodalLoadValues6.reserve(model.nodalLoads.size() * 6);
    for (const auto &load : model.nodalLoads) {
        nodalLoadNodeIds.push_back(load.nodeId);
        for (const double value : load.values) {
            nodalLoadValues6.push_back(value);
        }
    }

    std::vector<int> memberLoadMemberIds;
    std::vector<int> memberLoadCoordSystem;
    std::vector<double> memberLoadQx;
    std::vector<double> memberLoadQy;
    std::vector<double> memberLoadQz;
    memberLoadMemberIds.reserve(model.memberLoads.size());
    memberLoadCoordSystem.reserve(model.memberLoads.size());
    memberLoadQx.reserve(model.memberLoads.size());
    memberLoadQy.reserve(model.memberLoads.size());
    memberLoadQz.reserve(model.memberLoads.size());
    for (const auto &load : model.memberLoads) {
        memberLoadMemberIds.push_back(load.memberId);
        memberLoadCoordSystem.push_back(load.coordSystem == MemberLoadCoordSystem::Global ? 1 : 0);
        memberLoadQx.push_back(load.qx);
        memberLoadQy.push_back(load.qy);
        memberLoadQz.push_back(load.qz);
    }

    std::vector<int> memberMomentMemberIds;
    std::vector<int> memberMomentCoordSystem;
    std::vector<double> memberMomentMx;
    std::vector<double> memberMomentMy;
    std::vector<double> memberMomentMz;
    for (const auto &moment : model.memberMoments) {
        memberMomentMemberIds.push_back(moment.memberId);
        memberMomentCoordSystem.push_back(moment.coordSystem == MemberLoadCoordSystem::Global ? 1 : 0);
        memberMomentMx.push_back(moment.mx);
        memberMomentMy.push_back(moment.my);
        memberMomentMz.push_back(moment.mz);
    }

    std::vector<double> outNodeDisplacements6(totalNodeCount * 6, 0.0);
    std::vector<double> outNodeReactions6(totalNodeCount * 6, 0.0);
    // (o minimo de 1 evita ponteiro nulo quando nao ha barras: modelo so de cascas)
    std::vector<double> outMemberLocalForces12(std::max<std::size_t>(1, model.members.size() * 12), 0.0);
    std::vector<double> outMemberAuxK3(std::max<std::size_t>(1, model.members.size() * 3), 0.0);

    std::vector<char> errorBuffer(2048, '\0');

    const int returnCode = rawSymbolV4
        ? reinterpret_cast<StructuraSolveV4Fn>(rawSymbolV4)(
        static_cast<int>(materialIds.size()), materialIds.data(), materialE.data(), materialG.data(),
        static_cast<int>(sectionIds.size()), sectionIds.data(), sectionA.data(), sectionIz.data(), sectionIy.data(), sectionJ.data(),
        static_cast<int>(nodeIds.size()), nodeIds.data(), nodeX.data(), nodeY.data(), nodeZ.data(), nodeRestraints6.data(),
        static_cast<int>(memberIds.size()), memberIds.data(), memberNodeI.data(), memberNodeJ.data(), memberMaterialId.data(), memberSectionId.data(),
        static_cast<int>(nodalLoadNodeIds.size()), nodalLoadNodeIds.data(), nodalLoadValues6.data(),
        static_cast<int>(memberLoadMemberIds.size()), memberLoadMemberIds.data(), memberLoadCoordSystem.data(),
        memberLoadQx.data(), memberLoadQy.data(), memberLoadQz.data(),
        static_cast<int>(memberMomentMemberIds.size()), memberMomentMemberIds.data(), memberMomentCoordSystem.data(),
        memberMomentMx.data(), memberMomentMy.data(), memberMomentMz.data(),
        memberSectionRotationDeg.data(), memberOffsets4.data(), memberReleases12.data(),
        outNodeDisplacements6.data(), outNodeReactions6.data(), outMemberLocalForces12.data(), outMemberAuxK3.data(),
        static_cast<int>(errorBuffer.size()), errorBuffer.data())
        : rawSymbolV3
        ? reinterpret_cast<StructuraSolveV3Fn>(rawSymbolV3)(
        static_cast<int>(materialIds.size()), materialIds.data(), materialE.data(), materialG.data(),
        static_cast<int>(sectionIds.size()), sectionIds.data(), sectionA.data(), sectionIz.data(), sectionIy.data(), sectionJ.data(),
        static_cast<int>(nodeIds.size()), nodeIds.data(), nodeX.data(), nodeY.data(), nodeZ.data(), nodeRestraints6.data(),
        static_cast<int>(memberIds.size()), memberIds.data(), memberNodeI.data(), memberNodeJ.data(), memberMaterialId.data(), memberSectionId.data(),
        static_cast<int>(nodalLoadNodeIds.size()), nodalLoadNodeIds.data(), nodalLoadValues6.data(),
        static_cast<int>(memberLoadMemberIds.size()), memberLoadMemberIds.data(), memberLoadCoordSystem.data(),
        memberLoadQx.data(), memberLoadQy.data(), memberLoadQz.data(),
        static_cast<int>(memberMomentMemberIds.size()), memberMomentMemberIds.data(), memberMomentCoordSystem.data(),
        memberMomentMx.data(), memberMomentMy.data(), memberMomentMz.data(),
        memberSectionRotationDeg.data(), memberOffsets4.data(),
        outNodeDisplacements6.data(), outNodeReactions6.data(), outMemberLocalForces12.data(), outMemberAuxK3.data(),
        static_cast<int>(errorBuffer.size()), errorBuffer.data())
        : rawSymbolV2
        ? reinterpret_cast<StructuraSolveV2Fn>(rawSymbolV2)(
        static_cast<int>(materialIds.size()), materialIds.data(), materialE.data(), materialG.data(),
        static_cast<int>(sectionIds.size()), sectionIds.data(), sectionA.data(), sectionIz.data(), sectionIy.data(), sectionJ.data(),
        static_cast<int>(nodeIds.size()), nodeIds.data(), nodeX.data(), nodeY.data(), nodeZ.data(), nodeRestraints6.data(),
        static_cast<int>(memberIds.size()), memberIds.data(), memberNodeI.data(), memberNodeJ.data(), memberMaterialId.data(), memberSectionId.data(),
        static_cast<int>(nodalLoadNodeIds.size()), nodalLoadNodeIds.data(), nodalLoadValues6.data(),
        static_cast<int>(memberLoadMemberIds.size()), memberLoadMemberIds.data(), memberLoadCoordSystem.data(),
        memberLoadQx.data(), memberLoadQy.data(), memberLoadQz.data(),
        static_cast<int>(memberMomentMemberIds.size()), memberMomentMemberIds.data(), memberMomentCoordSystem.data(),
        memberMomentMx.data(), memberMomentMy.data(), memberMomentMz.data(),
        outNodeDisplacements6.data(), outNodeReactions6.data(), outMemberLocalForces12.data(), outMemberAuxK3.data(),
        static_cast<int>(errorBuffer.size()), errorBuffer.data())
        : reinterpret_cast<StructuraSolveFn>(rawSymbol)(
        static_cast<int>(materialIds.size()), materialIds.data(), materialE.data(), materialG.data(),
        static_cast<int>(sectionIds.size()), sectionIds.data(), sectionA.data(), sectionIz.data(), sectionIy.data(), sectionJ.data(),
        static_cast<int>(nodeIds.size()), nodeIds.data(), nodeX.data(), nodeY.data(), nodeZ.data(), nodeRestraints6.data(),
        static_cast<int>(memberIds.size()), memberIds.data(), memberNodeI.data(), memberNodeJ.data(), memberMaterialId.data(), memberSectionId.data(),
        static_cast<int>(nodalLoadNodeIds.size()), nodalLoadNodeIds.data(), nodalLoadValues6.data(),
        static_cast<int>(memberLoadMemberIds.size()), memberLoadMemberIds.data(), memberLoadCoordSystem.data(),
        memberLoadQx.data(), memberLoadQy.data(), memberLoadQz.data(),
        outNodeDisplacements6.data(), outNodeReactions6.data(), outMemberLocalForces12.data(), outMemberAuxK3.data(),
        static_cast<int>(errorBuffer.size()), errorBuffer.data());

    if (returnCode != 0) {
        solver_sec::cipherCString(errorBuffer.data(), errorBuffer.size(), sessionKey);
        std::string errorMessage(errorBuffer.data());
        if (errorMessage.empty()) {
            errorMessage = "structura_solve returned an error without message.";
        }
        throw SolverBridgeError(errorMessage);
    }

    Result result;
    result.nodeResults.resize(model.nodes.size());
    result.memberResults.resize(model.members.size());

    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        auto &nodeResult = result.nodeResults[i];
        nodeResult.nodeId = model.nodes[i].id;
        for (int dof = 0; dof < 6; ++dof) {
            nodeResult.displacements[static_cast<std::size_t>(dof)] = outNodeDisplacements6[i * 6 + static_cast<std::size_t>(dof)];
            nodeResult.reactions[static_cast<std::size_t>(dof)] = outNodeReactions6[i * 6 + static_cast<std::size_t>(dof)];
        }
    }

    if (hasShells) {
        for (std::size_t i = barNodeCount; i < totalNodeCount; ++i) {
            NodeResult nodeResult;
            nodeResult.nodeId = nodeIds[i];
            for (int dof = 0; dof < 6; ++dof) {
                nodeResult.displacements[static_cast<std::size_t>(dof)] = outNodeDisplacements6[i * 6 + static_cast<std::size_t>(dof)];
                nodeResult.reactions[static_cast<std::size_t>(dof)] = outNodeReactions6[i * 6 + static_cast<std::size_t>(dof)];
            }
            result.shellNodeResults.push_back(nodeResult);
        }
        const std::size_t count = model.shell->elements.size();
        std::vector<double> values(count * 40U), axes(count * 9U);
        if (getShellResults(static_cast<int>(count), values.data(), axes.data()) == static_cast<int>(count)) {
            for (std::size_t e = 0; e < count; ++e) {
                ShellElementResult r;
                r.shellId = model.shell->elements[e].id;
                std::copy(values.begin() + static_cast<std::ptrdiff_t>(e * 40U), values.begin() + static_cast<std::ptrdiff_t>(e * 40U + 40U), r.values.begin());
                std::copy(axes.begin() + static_cast<std::ptrdiff_t>(e * 9U), axes.begin() + static_cast<std::ptrdiff_t>(e * 9U + 9U), r.axes.begin());
                result.shellResults.push_back(r);
            }
        }
    }

    for (std::size_t i = 0; i < model.members.size(); ++i) {
        auto &memberResult = result.memberResults[i];
        memberResult.memberId = model.members[i].id;
        for (int j = 0; j < 12; ++j) {
            memberResult.localForces[static_cast<std::size_t>(j)] = outMemberLocalForces12[i * 12 + static_cast<std::size_t>(j)];
        }
        for (int j = 0; j < 3; ++j) {
            memberResult.auxK[static_cast<std::size_t>(j)] = outMemberAuxK3[i * 3 + static_cast<std::size_t>(j)];
        }
        // do referencial girado do solver para os eixos locais da barra: v_y = cos.v_y' - sen.v_z' ; v_z = sen.v_y' + cos.v_z'
        const double rotationDeg = model.members[i].sectionRotationDeg;
        if (rotationDeg != 0.0) {
            std::array<double, 3> x {0.0, 0.0, 0.0};
            bool found = false;
            {
                const NodeData *a = nullptr;
                const NodeData *b = nullptr;
                for (const auto &node : model.nodes) {
                    if (node.id == model.members[i].nodeI) {
                        a = &node;
                    }
                    if (node.id == model.members[i].nodeJ) {
                        b = &node;
                    }
                }
                if (a && b) {
                    x = {b->x - a->x, b->y - a->y, b->z - a->z};
                    const double length = std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
                    if (length > 1e-12) {
                        x = {x[0] / length, x[1] / length, x[2] / length};
                        found = true;
                    }
                }
            }
            const double c = std::cos(rotationDeg * kPi / 180.0), sn = std::sin(rotationDeg * kPi / 180.0);
            auto back = [c, sn](double &vy, double &vz) {
                const double y = c * vy - sn * vz, z = sn * vy + c * vz;
                vy = y;
                vz = z;
            };
            for (const std::size_t base : {std::size_t {0}, std::size_t {6}}) {
                back(memberResult.localForces[base + 1], memberResult.localForces[base + 2]);   // Vy, Vz
                back(memberResult.localForces[base + 4], memberResult.localForces[base + 5]);   // My, Mz
            }
            (void)0;
        }
        // eixo y local (referencial do programa) a partir da geometria: o auxiliar enviado aos consumidores e o proprio y (z = x cross y; y = z cross x)
        {
            const NodeData *na = nullptr;
            const NodeData *nb = nullptr;
            for (const auto &node : model.nodes) {
                if (node.id == model.members[i].nodeI) {
                    na = &node;
                }
                if (node.id == model.members[i].nodeJ) {
                    nb = &node;
                }
            }
            if (na && nb) {
                Structura::App::BarFrame::Vec3 x {nb->x - na->x, nb->y - na->y, nb->z - na->z};
                const double length = Structura::App::BarFrame::norm(x);
                if (length > 1e-12) {
                    x = {x[0] / length, x[1] / length, x[2] / length};
                    const auto y = Structura::App::BarFrame::axisY(x, Structura::App::BarFrame::referenceAux(x));
                    if (Structura::App::BarFrame::norm(y) > 0.5) {
                        memberResult.auxK = y;
                    }
                }
            }
        }
    }

    {
        std::vector<int> warpIds;
        std::vector<std::size_t> warpIndex;
        for (std::size_t i = 0; i < model.members.size(); ++i) {
            if (model.members[i].warping) {
                warpIds.push_back(model.members[i].id);
                warpIndex.push_back(i);
            }
        }
        using GetWarpingFn = int (*)(int, const int *, double *);
        auto getWarping = reinterpret_cast<GetWarpingFn>(resolveSlot(solver_sec::Rds_GetWarping));
        if (!warpIds.empty() && getWarping) {
            std::vector<double> values(warpIds.size() * 4U, 0.0);
            const bool used = getWarping(static_cast<int>(warpIds.size()), warpIds.data(), values.data()) > 0;
            for (std::size_t k = 0; k < warpIds.size(); ++k) {
                auto &memberResult = result.memberResults[warpIndex[k]];
                const double *v = values.data() + k * 4U;
                memberResult.hasWarping = used;
                memberResult.warpingRate = {v[0], v[1]};
                memberResult.warpingForce = {v[2], v[3]};
            }
        }
    }

    if (model.bucklingModes > 0 && !model.secondOrder) {
        using GetBucklingFn = int (*)(int, int, double *, double *);
        auto getBuckling = reinterpret_cast<GetBucklingFn>(resolveSlot(solver_sec::Rds_GetBuckling));
        if (getBuckling) {
            const int nodeCount = static_cast<int>(totalNodeCount);
            std::vector<double> factors(static_cast<std::size_t>(model.bucklingModes));
            std::vector<double> shapes(static_cast<std::size_t>(model.bucklingModes) * totalNodeCount * 6U);
            const int count = getBuckling(model.bucklingModes, nodeCount, factors.data(), shapes.data());
            for (int m = 0; m < count; ++m) {
                result.bucklingFactors.push_back(factors[static_cast<std::size_t>(m)]);
                const auto begin = shapes.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(m) * totalNodeCount * 6U);
                result.bucklingModes.emplace_back(begin, begin + static_cast<std::ptrdiff_t>(model.nodes.size() * 6U));
                if (totalNodeCount > model.nodes.size()) {
                    result.bucklingShellShapes.emplace_back(begin + static_cast<std::ptrdiff_t>(model.nodes.size() * 6U), begin + static_cast<std::ptrdiff_t>(totalNodeCount * 6U));
                }
            }
        }
    }

    if (model.modalModes > 0 && !model.secondOrder) {
        using GetModalFn = int (*)(int, int, double *, double *, double *);
        auto getModal = reinterpret_cast<GetModalFn>(resolveSlot(solver_sec::Rds_GetModal));
        if (getModal) {
            const int nodeCount = static_cast<int>(totalNodeCount);
            std::vector<double> omega(static_cast<std::size_t>(model.modalModes));
            std::vector<double> ratios(static_cast<std::size_t>(model.modalModes) * 3U);
            std::vector<double> shapes(static_cast<std::size_t>(model.modalModes) * totalNodeCount * 6U);
            const int count = getModal(model.modalModes, nodeCount, omega.data(), ratios.data(), shapes.data());
            for (int m = 0; m < count; ++m) {
                result.modalOmega.push_back(omega[static_cast<std::size_t>(m)]);
                result.modalMassRatio.push_back({ratios[static_cast<std::size_t>(m) * 3U], ratios[static_cast<std::size_t>(m) * 3U + 1U], ratios[static_cast<std::size_t>(m) * 3U + 2U]});
                const auto begin = shapes.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(m) * totalNodeCount * 6U);
                result.modalShapes.emplace_back(begin, begin + static_cast<std::ptrdiff_t>(model.nodes.size() * 6U));
                if (totalNodeCount > model.nodes.size()) {
                    result.modalShellShapes.emplace_back(begin + static_cast<std::ptrdiff_t>(model.nodes.size() * 6U), begin + static_cast<std::ptrdiff_t>(totalNodeCount * 6U));
                }
            }
        }
    }

    {
        // analise incremental nao linear e vinculos: resultados globais da DLL
        using GetSummaryFn = void (*)(double *);
        using GetCurveFn = int (*)(int, double *);
        using GetHingesFn = int (*)(int, int *, double *);
        using GetLinksFn = int (*)(int, int *, double *, double *);
        using GetConstraintForcesFn = int (*)(int, double *);
        auto getSummary = reinterpret_cast<GetSummaryFn>(resolveSlot(solver_sec::Rds_GetNonlinearSummary));
        auto getCurve = reinterpret_cast<GetCurveFn>(resolveSlot(solver_sec::Rds_GetNonlinearCurve));
        auto getHinges = reinterpret_cast<GetHingesFn>(resolveSlot(solver_sec::Rds_GetHingeResults));
        auto getLinks = reinterpret_cast<GetLinksFn>(resolveSlot(solver_sec::Rds_GetLinkResults));
        auto getConstraintForces = reinterpret_cast<GetConstraintForcesFn>(resolveSlot(solver_sec::Rds_GetConstraintForces));
        if (getSummary && getCurve && getHinges && getLinks) {
            double summary[8] = {0, 0, 0, 0, 0, 0, 0, 0};
            getSummary(summary);
            result.nonlinear.used = summary[0] != 0.0;
            if (result.nonlinear.used) {
                result.nonlinear.converged = summary[1] != 0.0;
                result.nonlinear.loadFactor = summary[2];
                result.nonlinear.steps = static_cast<int>(summary[3]);
                result.nonlinear.iterations = static_cast<int>(summary[4]);
                const int curveCount = static_cast<int>(summary[5]), hingeCount = static_cast<int>(summary[6]), linkCount = static_cast<int>(summary[7]);
                std::vector<double> curve(static_cast<std::size_t>(std::max(curveCount, 1)) * 2U);
                const int gotCurve = getCurve(curveCount, curve.data());
                for (int i = 0; i < gotCurve; ++i) {
                    result.nonlinear.curve.push_back({curve[static_cast<std::size_t>(i) * 2U], curve[static_cast<std::size_t>(i) * 2U + 1U]});
                }
                std::vector<int> hingeIds(static_cast<std::size_t>(std::max(hingeCount, 1)) * 2U);
                std::vector<double> hingeValues(static_cast<std::size_t>(std::max(hingeCount, 1)) * 3U);
                const int gotHinges = getHinges(hingeCount, hingeIds.data(), hingeValues.data());
                for (int i = 0; i < gotHinges; ++i) {
                    HingeResult h;
                    h.memberId = hingeIds[static_cast<std::size_t>(i) * 2U];
                    h.dof = hingeIds[static_cast<std::size_t>(i) * 2U + 1U];
                    for (const auto &member : model.members) {
                        if (member.id == h.memberId && isQuarterTurn(member.sectionRotationDeg)) {
                            const int base = h.dof >= 9 ? 9 : 3;
                            const int local = h.dof - base;
                            if (local == 1 || local == 2) {
                                h.dof = base + (local == 1 ? 2 : 1);
                            }
                        }
                    }
                    h.plasticRotation = hingeValues[static_cast<std::size_t>(i) * 3U];
                    h.moment = hingeValues[static_cast<std::size_t>(i) * 3U + 1U];
                    h.yielded = hingeValues[static_cast<std::size_t>(i) * 3U + 2U] != 0.0;
                    result.nonlinear.hinges.push_back(h);
                }
                std::vector<int> linkIds(static_cast<std::size_t>(std::max(linkCount, 1)));
                std::vector<double> deformation(static_cast<std::size_t>(std::max(linkCount, 1)) * 6U), force(static_cast<std::size_t>(std::max(linkCount, 1)) * 6U);
                const int gotLinks = getLinks(linkCount, linkIds.data(), deformation.data(), force.data());
                for (int i = 0; i < gotLinks; ++i) {
                    LinkResult l;
                    l.id = linkIds[static_cast<std::size_t>(i)];
                    for (std::size_t k = 0; k < 6; ++k) {
                        l.deformation[k] = deformation[static_cast<std::size_t>(i) * 6U + k];
                        l.force[k] = force[static_cast<std::size_t>(i) * 6U + k];
                    }
                    result.nonlinear.links.push_back(l);
                }
            }
        }
        if (getConstraintForces) {
            std::vector<double> forces(4096, 0.0);
            const int count = getConstraintForces(static_cast<int>(forces.size()), forces.data());
            result.constraintForces.assign(forces.begin(), forces.begin() + count);
        }
    }

    using GetWarningsFn = int (*)(char *, int);
    if (auto getWarnings = reinterpret_cast<GetWarningsFn>(resolveSlot(solver_sec::Rds_GetWarnings))) {
        std::vector<char> text(8192, '\0');
        if (getWarnings(text.data(), static_cast<int>(text.size())) > 0) {
            solver_sec::cipherCString(text.data(), text.size(), sessionKey);
            result.warnings = QString::fromUtf8(text.data()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        }
    }

    return result;
}

} // namespace Structura::App
