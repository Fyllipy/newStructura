#pragma once
// Cliente C++ de Licenciamento, Integridade e Keystone (M4).
// Base: SPEC.md (§6-§9, §16), PLAN.md (M4) e PATCH M3.
//
// Regras inegociáveis:
// 1. Zero trust & Fail-closed: sem licença válida, integridade adulterada, ou rede offline,
//    nenhum cálculo (Nível A) nem detalhamento/memorial (Nível B) é permitido.
// 2. Offline = somente-leitura (visualiza modelos salvos, sem calcular ou exportar).
// 3. Completude das reações: soma restrições + molas + apoios inclinados + forças de vínculo.
// 4. Enforcement do result_digest: na finalização/export, o digest real é comparado contra o
//    digest autenticado pelo Keystone K emitido pelo servidor.
// 5. Chave de sessão s = HKDF-SHA256(K, nonce, "finalize"||license_id):
//    - Slots de cálculo/preview usam a chave em binário (ou tag padrão de handshake).
//    - Slots de EXPORT/IMPRESSÃO/RELATÓRIO só desselam corretamente com a chave s.

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "integrity_client.h"
#include "vm_detect.h"

namespace structura_license {

// --- Totais de Reação para Validação Física (ΣF=0, ΣM=0) ---
struct ReactionTotals {
    double F[3] = {0.0, 0.0, 0.0};
    double M_origin[3] = {0.0, 0.0, 0.0};
};

// Acumulador de reações completas (apoios rígidos + molas nodais + apoios inclinados/vínculos)
class ReactionAccumulator {
public:
    ReactionAccumulator() { Reset(); }
    void Reset() {
        totals_.F[0] = totals_.F[1] = totals_.F[2] = 0.0;
        totals_.M_origin[0] = totals_.M_origin[1] = totals_.M_origin[2] = 0.0;
    }

    // Adiciona reação em um ponto arbitrário r=(x, y, z)
    // Rx, Ry, Rz: forças de reação globais
    // Mx, My, Mz: momentos de reação globais (se houver, ex: engaste)
    void AddReaction(double x, double y, double z,
                     double Rx, double Ry, double Rz,
                     double Mx = 0.0, double My = 0.0, double Mz = 0.0);

    const ReactionTotals& GetTotals() const { return totals_; }

private:
    ReactionTotals totals_;
};

// --- Estrutura de Keystone Validado ---
struct KeystoneArtifact {
    bool valid = false;
    std::string modelHash;
    std::string resultDigest;
    std::string nonce;
    int licenseId = 0;
    int exp = 0;
    int ts = 0;
    std::string signatureB64;
    std::string watermarkToken; // Marca d'agua forense emitida e assinada pelo servidor (M6)
    std::array<uint8_t, 32> sessionSecretS{}; // s = HKDF(K)
};

// --- Sessão do Cliente ---
struct ClientSession {
    std::string licenseToken;
    std::string expiresAt;
    std::string fingerprint;
    std::string dllKey; // DLL_RELEASE_KEY entregue pelo servidor no login (32 bytes em hex)
    std::string vmAllowToken; // Token de autorizacao de VM se verificado em allowlist (M6)
    std::string watermarkToken; // Ultima marca d'agua forense recebida (M6)
    int licenseId = 0;
    int userId = 0;
    bool online = false;
    bool mustChangePassword = false;   // senha temporaria do admin: o app exige a troca antes de abrir
};

// Verifica a assinatura Ed25519 do servidor sobre o token de licenca e devolve o payload JSON
// (fonte autoritativa de license_id/user_id/exp). false = token forjado/malformado.
bool VerifyLicenseTokenSignature(const std::string& token,
                                 const uint8_t serverPubKey[32],
                                 std::string& outPayloadJson);

// Gerenciamento da Sessão Ativa Global
void SetActiveClientSession(const ClientSession& session);
ClientSession GetActiveClientSession();

// --- API Principal do Cliente ---

// Inicialização e Fingerprint de Hardware
std::string GetHardwareFingerprint();

// Autenticação (Nível EXE)
bool ClientLogin(const std::string& serverUrl,
                 const std::string& email,
                 const std::string& password,
                 const std::string& totpCode,
                 const std::string& installDir,
                 const uint8_t releasePubKey[32],
                 ClientSession& outSession,
                 std::string& outError);

bool ClientTwoFASetup(const std::string& serverUrl,
                      const std::string& email,
                      const std::string& password,
                           std::string& outSecret,
                      std::string& outOtpAuthUri,
                      std::string& outError);

bool ClientTwoFAConfirm(const std::string& serverUrl,
                        const std::string& email,
                        const std::string& password,
                               const std::string& totpCode,
                        std::string& outError);

// Troca de senha (exige a senha atual). Usada no primeiro acesso com senha temporaria.
bool ClientChangePassword(const std::string& serverUrl,
                          const std::string& email,
                          const std::string& currentPassword,
                          const std::string& newPassword,
                          std::string& outError);

bool ClientLogout(const std::string& serverUrl,
                  ClientSession& session);

bool ClientHeartbeat(const std::string& serverUrl,
                     ClientSession& session,
                     const std::string& installDir,
                     const uint8_t releasePubKey[32],
                     std::string& outError);

// Gate Nível B (Detalhamento / Memorial): exige licença online, fail-closed se offline
bool CheckFeatureGateLevelB(const std::string& serverUrl,
                            const ClientSession& session,
                            const std::string& featureName, // "detalhamento" ou "memorial"
                            const std::string& installDir,
                            const uint8_t releasePubKey[32],
                            const uint8_t serverPubKey[32],
                            std::string& outError);

// Gate Nível A (Cálculo / Keystone):
// Valida equilíbrio no servidor e obtém Keystone K assinado
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
                            std::string& outError);

// Enforcement do result_digest na finalização/export:
// Compara o digest real recalculado dos resultados contra o digest assinado no Keystone
bool EnforceResultDigest(const KeystoneArtifact& keystone,
                         const std::string& actualResultDigest);

// Derivação de ponteiro protegido para slots de exportação usando a chave s do Keystone
// Para slots normais/preview: usa a chave em binário padrão.
// Para slots de exportação/finalização: exige s derivado de K válido.
uintptr_t UnsealSlotPointer(uint64_t sealedValue,
                            uint64_t nonce,
                            uint32_t slot,
                            uint64_t domain,
                            bool isExportSlot,
                            const KeystoneArtifact* keystoneOrNull);

// Cálculo do result_digest canônico (SHA-256 de deslocamentos e reações resumidas)
std::string ComputeResultDigest(const double* displacements, size_t dispCount,
                                const double* reactions, size_t reacCount);

// Derivação de s via HKDF-SHA256 (RFC 5869) com mistura de poison anti-debug e VM (M6)
bool DeriveSessionSecretS(const uint8_t sigK[64],
                          const std::string& nonce,
                          int licenseId,
                          std::array<uint8_t, 32>& outS);

bool DeriveSessionSecretS(const uint8_t sigK[64],
                          const std::string& nonce,
                          int licenseId,
                          const std::string& vmAllowToken,
                          std::array<uint8_t, 32>& outS);

// Verificação Central de Anti-Debug / Anti-Tamper para o EXE e DLLs (Marco M5).
// Inspeciona: PEB BeingDebugged, NtGlobalFlag, DebugPort, Hardware Breakpoints (Dr0-Dr3),
// Single-Stepping via temporização RDTSC e Software Breakpoints (0xCC) / Hooks (0xE9)
// nos prólogos de funções críticas. Retorna máscara != 0 se detectar violação.
uint64_t CheckExeAntiDebugPoison();

// --- Operações no Gatekeeper Real rds (resolvidas dinamicamente da DLL cifrada em repouso) ---

using GatekeeperUnlockFn = int (*)(uint64_t nonce, uint64_t s_mask, uint64_t* outTable, uint32_t slotCount);
using GatekeeperLockFn = int (*)(uint64_t nonce, uint64_t* outTable, uint32_t slotCount);

// Desbloqueia os slots de exportação do gatekeeper real com s_mask derivado do Keystone
bool GatekeeperUnlock(uint64_t nonce,
                      uint64_t s_mask,
                      uint64_t* outTable,
                      uint32_t slotCount,
                      GatekeeperUnlockFn unlockFn = nullptr);

// Tranca imediatamente os slots de exportação de volta ao domínio de quarentena
// (executado no logout, kill-switch e falha de heartbeat)
bool GatekeeperLock(uint64_t nonce,
                    uint64_t* outTable,
                    uint32_t slotCount,
                    GatekeeperLockFn lockFn = nullptr);

// Registra a tabela ativa para travamento automático em logout/heartbeat-fail
void RegisterActiveGatekeeperTable(uint64_t nonce, uint64_t* outTable, uint32_t slotCount,
                                   GatekeeperUnlockFn unlockFn = nullptr,
                                   GatekeeperLockFn lockFn = nullptr);

} // namespace structura_license
