#include "license_client.h"
#include "winhttp_client.h"
#include "json_helper.h"
#include "ed25519_compact.h"
#include "license_config.h"
#include "export_gatekeeper.h"
#include "hkdf.h"
#include "sha256.h"
#include "../Structuradll/src/core/solver_security.h"
#include "../Structuradll/src/bridge_dll/interface_dll.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#endif

namespace structura_license {

// Token de licenca: b64url(payload_json) "." b64url(assinatura Ed25519 de DOM_TOKEN||payload).
bool VerifyLicenseTokenSignature(const std::string& token,
                                 const uint8_t serverPubKey[32],
                                 std::string& outPayloadJson) {
    outPayloadJson.clear();
    if (token.empty() || token.size() > 2048) return false;
    const size_t dot = token.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= token.size()) return false;

    std::vector<uint8_t> payload, sig;
    if (!Base64Decode(token.substr(0, dot), payload) || payload.empty()) return false;
    if (!Base64Decode(token.substr(dot + 1), sig) || sig.size() != 64) return false;

    // DOM_TOKEN = b"structura-token-v1\0" (19 bytes, com o NUL final)
    static const char kDomToken[19] = {'s','t','r','u','c','t','u','r','a','-','t','o','k','e','n','-','v','1','\0'};
    std::vector<uint8_t> blob(kDomToken, kDomToken + sizeof(kDomToken));
    blob.insert(blob.end(), payload.begin(), payload.end());

    if (!Ed25519VerifyCompact(blob.data(), blob.size(), sig.data(), serverPubKey)) return false;
    outPayloadJson.assign(payload.begin(), payload.end());
    return true;
}

// --- ReactionAccumulator ---
void ReactionAccumulator::AddReaction(double x, double y, double z,
                                      double Rx, double Ry, double Rz,
                                      double Mx, double My, double Mz) {
    // Forças globais
    totals_.F[0] += Rx;
    totals_.F[1] += Ry;
    totals_.F[2] += Rz;

    // Momento em torno da origem M = r x F + M_aplicado
    // r x F = [ y*Rz - z*Ry, z*Rx - x*Rz, x*Ry - y*Rx ]
    totals_.M_origin[0] += (y * Rz - z * Ry) + Mx;
    totals_.M_origin[1] += (z * Rx - x * Rz) + My;
    totals_.M_origin[2] += (x * Ry - y * Rx) + Mz;
}

// --- Hardware Fingerprint ---
std::string GetHardwareFingerprint() {
    Sha256 h;
#ifdef _WIN32
    // CPUID
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0);
    h.Update(cpuInfo, sizeof(cpuInfo));
    __cpuid(cpuInfo, 1);
    // EBX[31:24] = APIC ID inicial do nucleo LOGICO onde a thread esta rodando: muda a cada execucao
    // conforme o agendador. Incluí-lo gerava um fingerprint diferente por sessao (DEVICE_IN_USE
    // na propria maquina). Mascara-se esse byte; o restante (assinatura, flags) e estavel.
    cpuInfo[1] &= 0x00FFFFFF;
    h.Update(cpuInfo, sizeof(cpuInfo));

    // Volume Serial Number do drive C:
    DWORD serial = 0;
    if (GetVolumeInformationW(L"C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0)) {
        h.Update(&serial, sizeof(serial));
    }

    // Machine GUID do Registro do Windows
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        wchar_t guid[128] = {0};
        DWORD size = sizeof(guid);
        if (RegQueryValueExW(hKey, L"MachineGuid", NULL, NULL, reinterpret_cast<LPBYTE>(guid), &size) == ERROR_SUCCESS) {
            h.Update(guid, size);
        }
        RegCloseKey(hKey);
    }
#else
    h.Update("generic_posix_fingerprint", 25);
#endif
    return h.FinalHex();
}

// --- Result Digest Canônico ---
std::string ComputeResultDigest(const double* displacements, size_t dispCount,
                                const double* reactions, size_t reacCount) {
    Sha256 h;
    h.Update("structura_results_v1", 20);
    if (displacements && dispCount > 0) {
        for (size_t i = 0; i < dispCount; ++i) {
            // Arredonda para 10 casas decimais para evitar variações de FPU entre arquiteturas
            double v = std::round(displacements[i] * 1e10) / 1e10;
            h.Update(&v, sizeof(v));
        }
    }
    if (reactions && reacCount > 0) {
        for (size_t i = 0; i < reacCount; ++i) {
            double v = std::round(reactions[i] * 1e10) / 1e10;
            h.Update(&v, sizeof(v));
        }
    }
    return h.FinalHex();
}

// --- Verificação de Anti-Debug e Anti-Tamper para o EXE (Marco M5 e M6) ---
uint64_t CheckExeAntiDebugPoison() {
    // Array com as funções mais críticas do EXE e do Gatekeeper
    // A varredura inspeciona prologs contra breakpoints INT3 (0xCC), hooks inline JMP (0xE9),
    // além de consultar a PEB (BeingDebugged, NtGlobalFlag), portas de debug do kernel,
    // registradores Dr0-Dr3 e single-stepping via RDTSC.
    // Marco M6: Inclui o prologo de CheckVmPoison para garantir que qualquer hook ou remocao
    // da verificacao de VM corrompa imediatamente o anti-debug poison.
    const void* criticalFns[] = {
        reinterpret_cast<const void*>(&ClientLogin),
        reinterpret_cast<const void*>(&RequestComputeKeystone),
        reinterpret_cast<const void*>(&CheckVmPoison),
        reinterpret_cast<const void*>(&ExportGatekeeper::RealDxfExport),
        reinterpret_cast<const void*>(&ExportGatekeeper::RealNormativeCheck),
        reinterpret_cast<const void*>(&ExportGatekeeper::RealQuantityExport),
        reinterpret_cast<const void*>(&ExportGatekeeper::RealPdfExport)
    };
    return solver_sec::antiDebugPoison(criticalFns, sizeof(criticalFns) / sizeof(criticalFns[0]));
}

// --- Derivação de s = HKDF-SHA256(K, nonce, "finalize"||license_id) com mistura de poison (M5/M6) ---
bool DeriveSessionSecretS(const uint8_t sigK[64],
                          const std::string& nonce,
                          int licenseId,
                          const std::string& vmAllowToken,
                          std::array<uint8_t, 32>& outS) {
    std::string info = "finalize" + std::to_string(licenseId);
    HkdfSha256_32(sigK, 64,
                  reinterpret_cast<const uint8_t*>(nonce.data()), nonce.size(),
                  reinterpret_cast<const uint8_t*>(info.data()), info.size(),
                  outS.data());

    // M5 + M6: Misturar o poison do anti-debug e da deteccao de VM na chave de sessao s.
    // Se debugger, hook, breakpoint ou VM nao autorizada estiverem presentes, a chave s e corrompida silenciosamente,
    // garantindo que os unseals de exportação e os cálculos físicos fiquem degradados sem crash.
    uint64_t adbgPoison = CheckExeAntiDebugPoison();
    uint64_t vmPoison = CheckVmPoison(vmAllowToken);
    uint64_t poison = adbgPoison | vmPoison;
    if (poison != 0ULL) {
        solver_sec::poisonKeystoneSecret(outS.data(), poison);
    }
    return true;
}

bool DeriveSessionSecretS(const uint8_t sigK[64],
                          const std::string& nonce,
                          int licenseId,
                          std::array<uint8_t, 32>& outS) {
    return DeriveSessionSecretS(sigK, nonce, licenseId, GetActiveClientSession().vmAllowToken, outS);
}

// --- Enforcement do result_digest ---
bool EnforceResultDigest(const KeystoneArtifact& keystone,
                         const std::string& actualResultDigest) {
    if (!keystone.valid) return false;
    if (keystone.resultDigest.empty() || actualResultDigest.empty()) return false;
    // Comparação de tempo constante
    if (keystone.resultDigest.size() != actualResultDigest.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < keystone.resultDigest.size(); ++i) {
        diff |= static_cast<unsigned char>(keystone.resultDigest[i] ^ actualResultDigest[i]);
    }
    return diff == 0;
}

// --- Unseal de Slots com Amarra de Keystone ---
uintptr_t UnsealSlotPointer(uint64_t sealedValue,
                            uint64_t nonce,
                            uint32_t slot,
                            uint64_t domain,
                            bool isExportSlot,
                            const KeystoneArtifact* keystoneOrNull) {
    if (!isExportSlot) {
        // Slots de cálculo e preview normais: desselam com a rotina padrão do binário
        return reinterpret_cast<uintptr_t>(
            solver_sec::unsealPointer<void*>(sealedValue, nonce, slot, domain)
        );
    }

    // Slots de EXPORT/IMPRESSÃO/RELATÓRIO:
    // Exigem obrigatoriamente Keystone K válido e chave s derivada
    if (keystoneOrNull == nullptr || !keystoneOrNull->valid) {
        // Sem K válido: corrompe deliberadamente o ponteiro -> falha fechada segura
        return 0;
    }

    // Mistura os primeiros 8 bytes da chave s no keystream da des-selagem
    uint64_t s_mask = 0;
    std::memcpy(&s_mask, keystoneOrNull->sessionSecretS.data(), sizeof(s_mask));

    // Des-selagem autenticada por s:
    // Modifica o domínio/keystream com s_mask
    uint64_t dynamicDomain = domain ^ s_mask;
    return reinterpret_cast<uintptr_t>(
        solver_sec::unsealPointer<void*>(sealedValue, nonce, slot, dynamicDomain)
    );
}

// --- Gatekeeper Real rds (resolução dinâmica sem link estático com rds.dll) ---

namespace {
struct ActiveGatekeeperContext {
    uint64_t nonce = 0;
    uint64_t* outTable = nullptr;
    uint32_t slotCount = 0;
    GatekeeperUnlockFn unlockFn = nullptr;
    GatekeeperLockFn lockFn = nullptr;
};
ActiveGatekeeperContext g_activeGatekeeper{};
ClientSession g_activeClientSession{};
}

void SetActiveClientSession(const ClientSession& session) {
    g_activeClientSession = session;
}

ClientSession GetActiveClientSession() {
    return g_activeClientSession;
}

void RegisterActiveGatekeeperTable(uint64_t nonce, uint64_t* outTable, uint32_t slotCount,
                                   GatekeeperUnlockFn unlockFn,
                                   GatekeeperLockFn lockFn) {
    g_activeGatekeeper.nonce = nonce;
    g_activeGatekeeper.outTable = outTable;
    g_activeGatekeeper.slotCount = slotCount;
    if (unlockFn) g_activeGatekeeper.unlockFn = unlockFn;
    if (lockFn) g_activeGatekeeper.lockFn = lockFn;
}

bool GatekeeperUnlock(uint64_t nonce,
                      uint64_t s_mask,
                      uint64_t* outTable,
                      uint32_t slotCount,
                      GatekeeperUnlockFn unlockFn) {
    if (outTable == nullptr || slotCount == 0 || nonce == 0 || s_mask == 0) {
        return false;
    }
    if (!unlockFn) {
        unlockFn = g_activeGatekeeper.unlockFn;
    }
    if (unlockFn) {
        return (unlockFn(nonce, s_mask, outTable, slotCount) == 0);
    }
    return false;
}

bool GatekeeperLock(uint64_t nonce,
                    uint64_t* outTable,
                    uint32_t slotCount,
                    GatekeeperLockFn lockFn) {
    if (outTable == nullptr || slotCount == 0 || nonce == 0) {
        return false;
    }
    if (!lockFn) {
        lockFn = g_activeGatekeeper.lockFn;
    }
    if (lockFn) {
        return (lockFn(nonce, outTable, slotCount) == 0);
    }
    return false;
}

static void TriggerSessionLock() {
    if (g_activeGatekeeper.outTable && g_activeGatekeeper.nonce) {
        GatekeeperLock(g_activeGatekeeper.nonce, g_activeGatekeeper.outTable, g_activeGatekeeper.slotCount);
    }
    GetGlobalExportGatekeeper().Lock(g_activeGatekeeper.nonce);
}

// --- Funções Reais de Autenticação e Licenciamento (WinHTTP) ---


#ifdef _WIN32
namespace {

std::string WideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::string();
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), &out[0], n, nullptr, nullptr);
    return out;
}

std::string OsVersionString() {
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    auto fn = ntdll ? reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion")) : nullptr;
    RTL_OSVERSIONINFOW vi{};
    vi.dwOSVersionInfoSize = sizeof(vi);
    if (fn && fn(&vi) == 0) {
        return "Windows " + std::to_string(vi.dwMajorVersion) + "." + std::to_string(vi.dwMinorVersion) +
               "." + std::to_string(vi.dwBuildNumber);
    }
    return "Windows";
}

std::string CpuBrandString() {
    int r[4] = {0};
    char brand[49] = {0};
    __cpuid(r, static_cast<int>(0x80000000));
    if (static_cast<unsigned>(r[0]) < 0x80000004u) return std::string();
    for (unsigned i = 0; i < 3; ++i) {
        __cpuid(r, static_cast<int>(0x80000002u + i));
        std::memcpy(brand + i * 16, r, 16);
    }
    std::string b(brand);
    size_t a = b.find_first_not_of(' ');
    return a == std::string::npos ? std::string() : b.substr(a);
}

} // namespace
#endif

// Bloco "telemetry" enviado no login e no heartbeat (antifraude). O IP NAO e enviado: o servidor o observa.
// Nada de senha, CPF, nome de usuario do Windows nem conteudo de projetos.
std::string BuildTelemetryJson() {
    std::string os, host, cpu, loc, tz;
#ifdef _WIN32
    os = OsVersionString();
    cpu = CpuBrandString();
    wchar_t hn[MAX_COMPUTERNAME_LENGTH + 1] = {0};
    DWORD hlen = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameW(hn, &hlen)) host = WideToUtf8(std::wstring(hn, hlen));
    wchar_t ln[LOCALE_NAME_MAX_LENGTH] = {0};
    if (GetUserDefaultLocaleName(ln, LOCALE_NAME_MAX_LENGTH) > 0) loc = WideToUtf8(ln);
    TIME_ZONE_INFORMATION tzi{};
    GetTimeZoneInformation(&tzi);
    const int off = -static_cast<int>(tzi.Bias);   // minutos a leste de UTC
    char tzb[32];
    snprintf(tzb, sizeof(tzb), "UTC%c%02d:%02d", off < 0 ? '-' : '+', std::abs(off) / 60, std::abs(off) % 60);
    tz = tzb;
#endif
#ifdef STRUCTURA_APP_VERSION
    const std::string ver = STRUCTURA_APP_VERSION;
#else
    const std::string ver = "dev";
#endif
    return "\"telemetry\":{"
        "\"app_version\":\"" + EscapeJson(ver) + "\","
        "\"os_version\":\"" + EscapeJson(os) + "\","
        "\"hostname\":\"" + EscapeJson(host) + "\","
        "\"cpu\":\"" + EscapeJson(cpu) + "\","
        "\"locale\":\"" + EscapeJson(loc) + "\","
        "\"timezone\":\"" + EscapeJson(tz) + "\""
    "}";
}

bool ClientLogin(const std::string& serverUrl,
                 const std::string& email,
                 const std::string& password,
                 const std::string& totpCode,
                 const std::string& installDir,
                 const uint8_t releasePubKey[32],
                 ClientSession& outSession,
                 std::string& outError) {
    outSession = ClientSession{};
    outError.clear();

    if (serverUrl.empty() || email.empty() || password.empty()) {
        outError = "MISSING_CREDENTIALS";
        return false;
    }

    std::string fp = GetHardwareFingerprint();

    // M5: Anti-debug no login da sessão
    uint64_t startupPoison = CheckExeAntiDebugPoison();
    if (startupPoison != 0ULL) {
        fp += "_adbg_poisoned";
    }

    // M6: Detecção de VM / Hypervisor
    auto vmInfo = DetectVirtualMachine();

    Attestation att{};
    IntegrityStatus stat = VerifyInstall(installDir, releasePubKey, Ed25519VerifyCompact, att);
    if (stat != IntegrityStatus::Ok) {
        outError = "INTEGRITY_CHECK_FAILED";
        return false;
    }

    std::string jsonBody = "{"
        "\"email\":\"" + EscapeJson(email) + "\","
        "\"password\":\"" + EscapeJson(password) + "\","
        // totp vazio e OMITIDO (o servidor exige 6 digitos quando presente)
        + (totpCode.empty() ? std::string() : "\"totp\":\"" + EscapeJson(totpCode) + "\",") +
        "\"fingerprint\":\"" + EscapeJson(fp) + "\","
        "\"is_vm\":" + (vmInfo.isVm ? "true" : "false") + ","
        "\"vm_vendor\":\"" + EscapeJson(vmInfo.vendor) + "\"," +
        BuildTelemetryJson() + "," +
        "\"integrity\":{"
            "\"manifest_version\":" + std::to_string(att.manifestVersion) + ","
            "\"files_root\":\"" + EscapeJson(att.filesRoot) + "\","
            "\"file_count\":" + std::to_string(att.fileCount) + ","
            "\"exe_hash\":\"" + EscapeJson(att.exeHash) + "\""
        "}"
    "}";

    HttpResponse resp = HttpPost(serverUrl + OBF_STR("/auth/login"), jsonBody, "", 5000);
    if (resp.statusCode == 0) {
        outError = "SERVER_OFFLINE_OR_TIMEOUT";
        return false;
    }

    if (resp.statusCode != 200) {
        bool reqSetup = JsonGetBool(resp.body, "requires_2fa_setup");
        std::string detail = JsonGetString(resp.body, "detail");
        if (reqSetup || detail == "ENROLL_2FA" || detail == "REQUIRES_2FA_SETUP") {
            outError = "REQUIRES_2FA_SETUP";
            return false;
        }
        outError = detail.empty() ? ("HTTP_" + std::to_string(resp.statusCode)) : detail;
        return false;
    }

    // Sucesso = HTTP 200 + token assinado. `ok` ausente conta como true (so `"ok":false`
    // explicito rejeita): antes, a ausencia do campo virava LOGIN_REJECTED e a UI mostrava
    // "e-mail, CPF ou senha incorretos" mesmo com o login aceito pelo servidor.
    bool ok = JsonGetBool(resp.body, "ok", true);
    bool reqTotp = JsonGetBool(resp.body, "requires_totp");

    if (reqTotp) {
        outError = "REQUIRES_2FA_TOTP";
        return false;
    }

    if (!ok) {
        std::string reason = JsonGetString(resp.body, "reason");
        outError = reason.empty() ? "LOGIN_REJECTED" : reason;
        return false;
    }

    outSession.licenseToken = JsonGetString(resp.body, "license_token");
    std::string tokenPayload;
    if (outSession.licenseToken.empty()) {
        outSession = ClientSession{};
        outError = "INVALID_SERVER_RESPONSE";
        return false;
    }
    // Servidor falso/MITM nao consegue forjar o token: a assinatura Ed25519 e verificada
    // contra a chave publica embutida (antes qualquer 200 com um JSON qualquer era aceito).
    if (!VerifyLicenseTokenSignature(outSession.licenseToken, GetServerPublicKey(), tokenPayload)) {
        outSession = ClientSession{};
        outError = "SERVER_SIGNATURE_INVALID";
        return false;
    }
    outSession.expiresAt = JsonGetString(resp.body, "expires_at");
    outSession.dllKey = JsonGetString(resp.body, "dll_key");
    outSession.vmAllowToken = JsonGetString(resp.body, "vm_allow_token");
    // ids vem do payload ASSINADO (fonte autoritativa); usados no HKDF de s e na verificacao de K/passe
    outSession.licenseId = JsonGetInt(tokenPayload, "license_id");
    outSession.userId = JsonGetInt(tokenPayload, "user_id");
    const bool mustChange = JsonGetBool(resp.body, "must_change_password");
    outSession.fingerprint = fp;
    outSession.online = true;
    outSession.mustChangePassword = mustChange;

    // M6: Inspeciona se a VM detectada foi recusada pelo poison
    uint64_t vmPoison = CheckVmPoison(outSession.vmAllowToken);
    if (vmPoison != 0ULL) {
        outSession = ClientSession{};
        outError = "VM_NOT_ALLOWED";
        return false;
    }

    SetActiveClientSession(outSession);

    return true;
}

bool ClientTwoFASetup(const std::string& serverUrl,
                      const std::string& email,
                      const std::string& password,
                      std::string& outSecret,
                      std::string& outOtpAuthUri,
                      std::string& outError) {
    outSecret.clear();
    outOtpAuthUri.clear();
    outError.clear();

    if (serverUrl.empty() || email.empty() || password.empty()) {
        outError = "MISSING_CREDENTIALS";
        return false;
    }

    std::string jsonBody = "{"
        "\"email\":\"" + EscapeJson(email) + "\","
        "\"password\":\"" + EscapeJson(password) + "\""
    "}";

    HttpResponse resp = HttpPost(serverUrl + OBF_STR("/auth/2fa/setup"), jsonBody, "", 5000);
    if (resp.statusCode == 0) {
        outError = "SERVER_OFFLINE_OR_TIMEOUT";
        return false;
    }
    if (resp.statusCode != 200) {
        std::string detail = JsonGetString(resp.body, "detail");
        outError = detail.empty() ? ("HTTP_" + std::to_string(resp.statusCode)) : detail;
        return false;
    }

    outSecret = JsonGetString(resp.body, "secret");
    outOtpAuthUri = JsonGetString(resp.body, "otpauth_uri");
    if (outSecret.empty() || outOtpAuthUri.empty()) {
        outError = "INVALID_2FA_SETUP_RESPONSE";
        return false;
    }
    return true;
}

bool ClientTwoFAConfirm(const std::string& serverUrl,
                        const std::string& email,
                        const std::string& password,
                        const std::string& totpCode,
                        std::string& outError) {
    outError.clear();

    if (serverUrl.empty() || email.empty() || password.empty() || totpCode.empty()) {
        outError = "MISSING_CREDENTIALS";
        return false;
    }

    std::string jsonBody = "{"
        "\"email\":\"" + EscapeJson(email) + "\","
        "\"password\":\"" + EscapeJson(password) + "\","
        "\"totp\":\"" + EscapeJson(totpCode) + "\""
    "}";

    HttpResponse resp = HttpPost(serverUrl + OBF_STR("/auth/2fa/confirm"), jsonBody, "", 5000);
    if (resp.statusCode == 0) {
        outError = "SERVER_OFFLINE_OR_TIMEOUT";
        return false;
    }
    if (resp.statusCode == 204 || resp.statusCode == 200) {
        return true;
    }
    std::string detail = JsonGetString(resp.body, "detail");
    outError = detail.empty() ? ("HTTP_" + std::to_string(resp.statusCode)) : detail;
    return false;
}

bool ClientChangePassword(const std::string& serverUrl,
                          const std::string& email,
                          const std::string& currentPassword,
                          const std::string& newPassword,
                          std::string& outError) {
    outError.clear();
    if (serverUrl.empty() || email.empty() || currentPassword.empty() || newPassword.empty()) {
        outError = "MISSING_CREDENTIALS";
        return false;
    }
    std::string jsonBody = "{"
        "\"email\":\"" + EscapeJson(email) + "\","
        "\"password\":\"" + EscapeJson(currentPassword) + "\","
        "\"new_password\":\"" + EscapeJson(newPassword) + "\""
    "}";
    HttpResponse resp = HttpPost(serverUrl + OBF_STR("/auth/password/change"), jsonBody, "", 5000);
    if (resp.statusCode == 0) {
        outError = "SERVER_OFFLINE_OR_TIMEOUT";
        return false;
    }
    if (resp.statusCode == 204 || resp.statusCode == 200) {
        return true;
    }
    std::string detail = JsonGetString(resp.body, "detail");
    outError = detail.empty() ? ("HTTP_" + std::to_string(resp.statusCode)) : detail;
    return false;
}

bool ClientLogout(const std::string& serverUrl,
                  ClientSession& session) {
    TriggerSessionLock();
    SetActiveClientSession(ClientSession{});

    if (!session.online || session.licenseToken.empty()) {
        session = ClientSession{};
        return true;
    }

    std::string jsonBody = "{"
        "\"license_token\":\"" + EscapeJson(session.licenseToken) + "\","
        "\"fingerprint\":\"" + EscapeJson(session.fingerprint) + "\""
    "}";

    HttpPost(serverUrl + OBF_STR("/auth/logout"), jsonBody, session.licenseToken, 3000);

    session = ClientSession{};
    return true;
}

bool ClientHeartbeat(const std::string& serverUrl,
                     ClientSession& session,
                     const std::string& installDir,
                     const uint8_t releasePubKey[32],
                     std::string& outError) {
    outError.clear();

    if (!session.online || session.licenseToken.empty()) {
        TriggerSessionLock();
        outError = "SESSION_INACTIVE";
        return false;
    }

    // M5: Anti-debug durante heartbeat periódico
    uint64_t hbPoison = CheckExeAntiDebugPoison();
    if (hbPoison != 0ULL) {
        TriggerSessionLock();
        session.online = false;
        outError = "DEBUGGER_DETECTED_ON_HEARTBEAT";
        return false;
    }

    // M6: Detecção e poison de VM durante heartbeat
    uint64_t vmPoison = CheckVmPoison(session.vmAllowToken);
    if (vmPoison != 0ULL) {
        TriggerSessionLock();
        session.online = false;
        outError = "VM_NOT_ALLOWED";
        return false;
    }

    auto vmInfo = DetectVirtualMachine();

    Attestation att{};
    IntegrityStatus stat = VerifyInstall(installDir, releasePubKey, Ed25519VerifyCompact, att);
    if (stat != IntegrityStatus::Ok) {
        TriggerSessionLock();
        session.online = false;
        outError = "INTEGRITY_VIOLATION_DURING_HEARTBEAT";
        return false;
    }

    std::string jsonBody = "{"
        "\"license_token\":\"" + EscapeJson(session.licenseToken) + "\","
        "\"fingerprint\":\"" + EscapeJson(session.fingerprint) + "\","
        "\"is_vm\":" + (vmInfo.isVm ? "true" : "false") + ","
        "\"vm_vendor\":\"" + EscapeJson(vmInfo.vendor) + "\"," +
        BuildTelemetryJson() + "," +
        "\"integrity\":{"
            "\"manifest_version\":" + std::to_string(att.manifestVersion) + ","
            "\"files_root\":\"" + EscapeJson(att.filesRoot) + "\","
            "\"file_count\":" + std::to_string(att.fileCount) + ","
            "\"exe_hash\":\"" + EscapeJson(att.exeHash) + "\""
        "}"
    "}";

    HttpResponse resp = HttpPost(serverUrl + OBF_STR("/license/heartbeat"), jsonBody, session.licenseToken, 5000);

    if (resp.statusCode != 200) {
        TriggerSessionLock();
        session.online = false;
        outError = (resp.statusCode == 0) ? "HEARTBEAT_TIMEOUT_OFFLINE" : "HEARTBEAT_REJECTED";
        return false;
    }

    // `ok` ausente = true (o servidor so responde 200 com corpo de sucesso; erros vem em 4xx/5xx).
    bool ok = JsonGetBool(resp.body, "ok", true);
    if (!ok) {
        TriggerSessionLock();
        session.online = false;
        outError = JsonGetString(resp.body, "reason");
        return false;
    }

    // O servidor rotaciona o token a cada heartbeat: precisa ser adotado (e verificado).
    std::string newToken = JsonGetString(resp.body, "license_token");
    if (!newToken.empty()) {
        std::string payload;
        if (!VerifyLicenseTokenSignature(newToken, GetServerPublicKey(), payload)) {
            TriggerSessionLock();
            session.online = false;
            outError = "SERVER_SIGNATURE_INVALID";
            return false;
        }
        session.licenseToken = newToken;
        int lid = JsonGetInt(payload, "license_id");
        if (lid > 0) session.licenseId = lid;
        int uid = JsonGetInt(payload, "user_id");
        if (uid > 0) session.userId = uid;
    }
    session.expiresAt = JsonGetString(resp.body, "expires_at");
    std::string newVmToken = JsonGetString(resp.body, "vm_allow_token");
    if (!newVmToken.empty()) {
        session.vmAllowToken = newVmToken;
    }
    return true;
}

bool CheckFeatureGateLevelB(const std::string& serverUrl,
                            const ClientSession& session,
                            const std::string& featureName,
                            const std::string& installDir,
                            const uint8_t releasePubKey[32],
                            const uint8_t serverPubKey[32],
                            std::string& outError) {
    outError.clear();

    if (!session.online || session.licenseToken.empty()) {
        outError = "FEATURE_BLOCKED_OFFLINE";
        return false;
    }

    Attestation att{};
    IntegrityStatus stat = VerifyInstall(installDir, releasePubKey, Ed25519VerifyCompact, att);
    if (stat != IntegrityStatus::Ok) {
        outError = "INTEGRITY_CHECK_FAILED";
        return false;
    }

    uint64_t nonceVal = solver_sec::generateNonce(&session);
    char nonceBuf[32];
    snprintf(nonceBuf, sizeof(nonceBuf), "%016llx", static_cast<unsigned long long>(nonceVal));
    std::string nonce = nonceBuf;
    auto vmInfo = DetectVirtualMachine();

    std::string jsonBody = "{"
        "\"license_token\":\"" + EscapeJson(session.licenseToken) + "\","
        "\"fingerprint\":\"" + EscapeJson(session.fingerprint) + "\","
        "\"feature\":\"" + EscapeJson(featureName) + "\","
        "\"nonce\":\"" + EscapeJson(nonce) + "\","
        "\"is_vm\":" + (vmInfo.isVm ? "true" : "false") + ","
        "\"vm_vendor\":\"" + EscapeJson(vmInfo.vendor) + "\","
        "\"integrity\":{"
            "\"manifest_version\":" + std::to_string(att.manifestVersion) + ","
            "\"files_root\":\"" + EscapeJson(att.filesRoot) + "\","
            "\"file_count\":" + std::to_string(att.fileCount) + ","
            "\"exe_hash\":\"" + EscapeJson(att.exeHash) + "\""
        "}"
    "}";

    HttpResponse resp = HttpPost(serverUrl + OBF_STR("/license/check"), jsonBody, session.licenseToken, 5000);
    if (resp.statusCode != 200) {
        TriggerSessionLock();
        outError = (resp.statusCode == 0) ? "NETWORK_TIMEOUT_FEATURE_BLOCKED" : "CHECK_REJECTED";
        return false;
    }

    bool ok = JsonGetBool(resp.body, "ok");
    if (!ok) {
        TriggerSessionLock();
        outError = JsonGetString(resp.body, "reason");
        return false;
    }

    std::string passSigB64 = JsonGetString(resp.body, "pass_sig");
    int exp = JsonGetInt(resp.body, "exp");

    if (passSigB64.empty()) {
        TriggerSessionLock();
        outError = "MISSING_PASS_SIGNATURE";
        return false;
    }

    std::vector<uint8_t> sigPass;
    if (!Base64Decode(passSigB64, sigPass) || sigPass.size() != 64) {
        TriggerSessionLock();
        outError = "PASS_SIGNATURE_INVALID";
        return false;
    }

    // Reconstrução do contexto exato assinado pelo servidor:
    // feature | license_id | nonce | exp (conforme crypto.py::sign_gate_pass)
    std::string ctxStr = featureName + "|" + std::to_string(session.licenseId) + "|" +
                         nonce + "|" + std::to_string(exp);

    Sha256 ctxHash;
    ctxHash.Update(ctxStr.data(), ctxStr.size());
    uint8_t ctxDigest[32];
    ctxHash.Final(ctxDigest);

    // Domínio de separação rigoroso: DOM_GATE = b"structura-gate-v1\x00" (18 bytes)
    static const uint8_t kDomGate[18] = {
        's','t','r','u','c','t','u','r','a','-','g','a','t','e','-','v','1','\0'
    };
    std::vector<uint8_t> signedBlob;
    signedBlob.reserve(18 + 32);
    signedBlob.insert(signedBlob.end(), kDomGate, kDomGate + 18);
    signedBlob.insert(signedBlob.end(), ctxDigest, ctxDigest + 32);

    bool sigValid = Ed25519VerifyCompact(
        signedBlob.data(), signedBlob.size(),
        sigPass.data(), serverPubKey);

    if (!sigValid) {
        TriggerSessionLock();
        outError = "PASS_SIGNATURE_INVALID";
        return false;
    }

    int64_t nowSec = static_cast<int64_t>(std::time(nullptr));
    if (exp > 0 && nowSec > exp) {
        TriggerSessionLock();
        outError = "PASS_EXPIRED";
        return false;
    }

    // M6: Marca d'agua forense autoritativa para o modulo Nivel B
    std::string wm = JsonGetString(resp.body, "watermark_token");
    if (!wm.empty()) {
        const_cast<ClientSession&>(session).watermarkToken = wm;
    }

    return true;
}

bool RequestComputeKeystone(const std::string& serverUrl,
                            const ClientSession& session,
                            const std::string& modelHash,
                            const std::string& modelJsonOrEmptyIfCached,
                            const ReactionTotals& reactions,
                            const std::string& resultDigest,
                            const std::string& installDir,
                            const uint8_t releasePubKey[32],
                            const uint8_t serverPubKey[32],
                            KeystoneArtifact& outKeystone,
                            std::string& outError) {
    outKeystone = KeystoneArtifact{};
    outError.clear();

    if (!session.online || session.licenseToken.empty()) {
        outError = "COMPUTE_BLOCKED_OFFLINE_READONLY";
        return false;
    }

    Attestation att{};
    IntegrityStatus stat = VerifyInstall(installDir, releasePubKey, Ed25519VerifyCompact, att);
    if (stat != IntegrityStatus::Ok) {
        outError = "INTEGRITY_CHECK_FAILED";
        return false;
    }

    // Geração do nonce real de 64 bits via solver_sec::generateNonce
    // Combina std::chrono::high_resolution_clock, contador de ciclos e ponteiro da stack frame (&session)
    // misturados com a permutação de alta avalanche SplitMix64 (mix64) e bit menos significativo forçado a 1.
    // É formatado como string hexadecimal de 16 caracteres em minúsculas com zero à esquerda ("%016llx").
    uint64_t nonceVal = solver_sec::generateNonce(&session);
    char nonceBuf[32];
    snprintf(nonceBuf, sizeof(nonceBuf), "%016llx", static_cast<unsigned long long>(nonceVal));
    std::string nonce = nonceBuf;

    ReactionTotals effectiveReactions = reactions;

    // M5: Anti-debug antes de submeter o cálculo ao servidor
    uint64_t computePoison = CheckExeAntiDebugPoison();
    if (computePoison != 0ULL) {
        // Se houver debugger/hook ativo antes do cálculo, envenena sutilmente as reações
        // para que o servidor recuse o cálculo por desequilíbrio (EQUILIBRIUM_FAIL)
        effectiveReactions.F[0] += ((computePoison & 0xFF) + 1.0) * 0.1;
    }

    // M6: Detecção de VM / Hypervisor
    auto vmInfo = DetectVirtualMachine();

    std::string jsonBody = "{"
        "\"license_token\":\"" + EscapeJson(session.licenseToken) + "\","
        "\"fingerprint\":\"" + EscapeJson(session.fingerprint) + "\","
        "\"model_hash\":\"" + EscapeJson(modelHash) + "\","
        "\"nonce\":\"" + EscapeJson(nonce) + "\","
        "\"is_vm\":" + (vmInfo.isVm ? "true" : "false") + ","
        "\"vm_vendor\":\"" + EscapeJson(vmInfo.vendor) + "\","
        "\"reaction_totals\":{"
            "\"F\":[" + std::to_string(effectiveReactions.F[0]) + "," +
                        std::to_string(effectiveReactions.F[1]) + "," +
                        std::to_string(effectiveReactions.F[2]) + "],"
            "\"M_origin\":[" + std::to_string(effectiveReactions.M_origin[0]) + "," +
                               std::to_string(effectiveReactions.M_origin[1]) + "," +
                               std::to_string(effectiveReactions.M_origin[2]) + "]"
        "},"
        "\"result_digest\":\"" + EscapeJson(resultDigest) + "\","
        "\"integrity\":{"
            "\"manifest_version\":" + std::to_string(att.manifestVersion) + ","
            "\"files_root\":\"" + EscapeJson(att.filesRoot) + "\","
            "\"file_count\":" + std::to_string(att.fileCount) + ","
            "\"exe_hash\":\"" + EscapeJson(att.exeHash) + "\""
        "}";

    if (!modelJsonOrEmptyIfCached.empty()) {
        jsonBody += ",\"model\":" + modelJsonOrEmptyIfCached;
    }
    jsonBody += "}";

    HttpResponse resp = HttpPost(serverUrl + OBF_STR("/compute/validate"), jsonBody, session.licenseToken, 8000);
    if (resp.statusCode != 200) {
        TriggerSessionLock();
        outError = (resp.statusCode == 0) ? "NETWORK_TIMEOUT_COMPUTE_BLOCKED" : "COMPUTE_VALIDATE_REJECTED";
        return false;
    }

    bool ok = JsonGetBool(resp.body, "ok");
    if (!ok) {
        TriggerSessionLock();
        outError = JsonGetString(resp.body, "reason");
        return false;
    }

    std::string sigKB64 = JsonGetString(resp.body, "K");
    int exp = JsonGetInt(resp.body, "exp");
    int ts = JsonGetInt(resp.body, "ts");

    if (sigKB64.empty()) {
        outError = "MISSING_KEYSTONE_K";
        return false;
    }

    std::vector<uint8_t> sigK;
    if (!Base64Decode(sigKB64, sigK) || sigK.size() != 64) {
        outError = "INVALID_KEYSTONE_FORMAT";
        return false;
    }

    std::string ctxStr = modelHash + "|" + resultDigest + "|" + nonce + "|" +
                         std::to_string(session.licenseId) + "|" +
                         std::to_string(exp) + "|" + std::to_string(ts);

    Sha256 ctxHash;
    ctxHash.Update(ctxStr.data(), ctxStr.size());
    uint8_t ctxDigest[32];
    ctxHash.Final(ctxDigest);

    std::vector<uint8_t> signedBlob;
    static const uint8_t kDomKeystone[22] = {
        's','t','r','u','c','t','u','r','a','-','k','e','y','s','t','o','n','e','-','v','1','\0'
    };
    signedBlob.reserve(22 + 32);
    signedBlob.insert(signedBlob.end(), kDomKeystone, kDomKeystone + 22);
    signedBlob.insert(signedBlob.end(), ctxDigest, ctxDigest + 32);

    bool sigValid = Ed25519VerifyCompact(
        signedBlob.data(), signedBlob.size(),
        sigK.data(), serverPubKey);

    if (!sigValid) {
        TriggerSessionLock();
        outError = "KEYSTONE_SIGNATURE_TAMPERED";
        return false;
    }

    outKeystone.valid = true;
    outKeystone.modelHash = modelHash;
    outKeystone.resultDigest = resultDigest;
    outKeystone.nonce = nonce;
    outKeystone.licenseId = session.licenseId;
    outKeystone.exp = exp;
    outKeystone.ts = ts;
    outKeystone.signatureB64 = sigKB64;

    // M6: Extração de marca d'água forense e token de allowlist de VM
    outKeystone.watermarkToken = JsonGetString(resp.body, "watermark_token");
    std::string newVmToken = JsonGetString(resp.body, "vm_allow_token");
    if (!newVmToken.empty()) {
        const_cast<ClientSession&>(session).vmAllowToken = newVmToken;
    }

    // M6: Inspeciona poison de VM
    uint64_t vmPoison = CheckVmPoison(session.vmAllowToken);
    if (vmPoison != 0ULL) {
        TriggerSessionLock();
        outError = "VM_NOT_ALLOWED";
        return false;
    }

    // Nota de auditoria: a amarração criptográfica autoritativa do result_digest
    // é garantida pela assinatura Ed25519 do servidor sobre o contexto verificado acima.
    // A validação abaixo confirma explicitamente que o digest não é nulo/vazio:
    if (!EnforceResultDigest(outKeystone, resultDigest)) {
        TriggerSessionLock();
        outError = "EMPTY_OR_CORRUPT_RESULT_DIGEST";
        return false;
    }

    DeriveSessionSecretS(sigK.data(), nonce, session.licenseId, session.vmAllowToken, outKeystone.sessionSecretS);

    uint64_t s_mask = 0;
    std::memcpy(&s_mask, outKeystone.sessionSecretS.data(), sizeof(s_mask));

    if (g_activeGatekeeper.outTable && g_activeGatekeeper.nonce) {
        GatekeeperUnlock(g_activeGatekeeper.nonce, s_mask, g_activeGatekeeper.outTable, g_activeGatekeeper.slotCount);
    }
    GetGlobalExportGatekeeper().Unlock(g_activeGatekeeper.nonce, s_mask);

    return true;
}

} // namespace structura_license
