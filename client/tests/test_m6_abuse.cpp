// Teste de unidade do Marco M6 — Concorrência, Velocity, Bloqueio de VM com Allowlist,
// Kill Switch de Export (keystone.exp) e Marca D'água Forense.
// Base: SPEC.md (§11), PLAN.md (M6) e Decisões de Desenho Travadas.

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "../license_client.h"
#include "../vm_detect.h"
#include "../export_gatekeeper.h"
#include "../json_helper.h"
#include "../sha256.h"
#include "../ed25519_compact.h"
#include "../../Structuradll/src/core/solver_security.h"
#include "../../Structuradll/src/bridge_dll/interface_dll.h"
#include "../../Structuradll/src/core/solver_core.h"

extern "C" {
#include "../tweetnacl.h"
}

using namespace structura_license;

static std::string Base64Encode(const uint8_t* data, size_t len) {
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t b = (static_cast<uint32_t>(data[i]) << 16);
        if (i + 1 < len) b |= (static_cast<uint32_t>(data[i + 1]) << 8);
        if (i + 2 < len) b |= static_cast<uint32_t>(data[i + 2]);
        out.push_back(tbl[(b >> 18) & 0x3F]);
        out.push_back(tbl[(b >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? tbl[(b >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < len) ? tbl[b & 0x3F] : '=');
    }
    return out;
}

// Stub de SolverCore::run para teste autônomo
bool SolverCore::run(const SolverInput&, SolverOutput&, std::string&) const {
    return true;
}

static void SetupFakeKeystone(KeystoneArtifact& k, int expDeltaSec, const std::string& wmToken = "") {
    k.valid = true;
    k.licenseId = 42;
    k.nonce = "0123456789abcdef";
    k.modelHash = "model_hash_test_123";
    int64_t now = static_cast<int64_t>(std::time(nullptr));
    k.ts = static_cast<int>(now);
    k.exp = static_cast<int>(now + expDeltaSec);
    k.watermarkToken = wmToken;

    // Assinatura fictícia de 64 bytes para derivação de s
    uint8_t sigK[64];
    for (size_t i = 0; i < 64; ++i) sigK[i] = static_cast<uint8_t>(i ^ 0x5A);

    // Deriva s
    DeriveSessionSecretS(sigK, k.nonce, k.licenseId, k.sessionSecretS);
}

// -------------------------------------------------------------
// Teste 1: Detecção de VM, Cadeia de Poison e Allowlist
// -------------------------------------------------------------
void TestVmDetectionAndPoison() {
    std::cout << "[M6-1] Testando Deteccao de VM e Cadeia de Poison..." << std::endl;

    // 1. Cenário: Sem VM (ambiente nativo)
    SetForceVmDetectionForTest(false);
    VmDetectionResult nativeRes = DetectVirtualMachine();
    if (!nativeRes.isVm) {
        uint64_t nativePoison = CheckVmPoison("");
        assert(nativePoison == 0ULL);
    }

    // 2. Cenário: VM detectada (simulada ou real) SEM allowlist do servidor
    SetForceVmDetectionForTest(true, "VMware Workstation Pro");
    VmDetectionResult vmRes = DetectVirtualMachine();
    assert(vmRes.isVm == true);
    assert(vmRes.vendor == "VMware Workstation Pro");

    uint64_t unallowedPoison = CheckVmPoison("");
    assert(unallowedPoison != 0ULL);
    assert((unallowedPoison & 0x8000000000000000ULL) != 0ULL); // Bit de veneno ativado

    // Confirma que o veneno de VM corrompe a chave de sessão s
    uint8_t sigK[64];
    for (size_t i = 0; i < 64; ++i) sigK[i] = static_cast<uint8_t>(i + 1);

    std::array<uint8_t, 32> poisonedS{};
    DeriveSessionSecretS(sigK, "nonce_vm_test", 100, "", poisonedS);

    // 3. Cenário: VM detectada COM token de allowlist válido entregue pelo servidor
    std::string validToken = "0123456789abcdef0123456789abcdef"; // 32 caracteres hex
    uint64_t allowedPoison = CheckVmPoison(validToken);
    assert(allowedPoison == 0ULL); // Veneno neutralizado pela allowlist!

    std::array<uint8_t, 32> pristineS{};
    DeriveSessionSecretS(sigK, "nonce_vm_test", 100, validToken, pristineS);

    // Confirma que a chave pristine diverge da chave envenenada
    assert(std::memcmp(poisonedS.data(), pristineS.data(), 32) != 0);

    // Restaura ambiente de teste
    SetForceVmDetectionForTest(false);
    std::cout << "  -> OK: Poison de VM ativado sem token e neutralizado com allowlist." << std::endl;
}

// -------------------------------------------------------------
// Teste 2: Kill Switch de Exportação por Keystone Expirado (keystone.exp)
// -------------------------------------------------------------
void TestExportGatekeeperKeystoneExpirationKillSwitch() {
    std::cout << "[M6-2] Testando Kill Switch de Exportacao por Keystone Expirado..." << std::endl;

    // Configura sessão ativa com allowlist válida para neutralizar poison de VM do host
    ClientSession activeSess;
    activeSess.online = true;
    activeSess.licenseId = 42;
    activeSess.vmAllowToken = "0123456789abcdef0123456789abcdef";
    SetActiveClientSession(activeSess);

    ExportGatekeeper gatekeeper;
    uint64_t nonce = 0xA1B2C3D4E5F60718ULL;
    gatekeeper.Initialize(nonce);

    double disp[3] = {0.001, 0.002, 0.003};
    double reac[3] = {100.0, 200.0, 300.0};
    std::string digest = ComputeResultDigest(disp, 3, reac, 3);

    std::vector<DxfBarInput> bars = { {1, 0, 0, 0, 10, 0, 0} };
    std::vector<PdfSectionInput> sections = { {1, 150.0, 50.0, 2.5} };
    NormativeCheckInput normIn{1, 500.0, 100.0, 0.005, 0.0005, 345.0};
    std::vector<QuantityBarInput> qtyBars = { {1, 10.0, 0.005, 7850.0} };

    // --- Caso A: Keystone EXPIRADO (exp = now - 10s) ---
    KeystoneArtifact expiredK;
    SetupFakeKeystone(expiredK, -10, "STRUCTURA-WM1:42:1700000000:expired:12345");
    expiredK.resultDigest = digest;

    uint64_t s_mask_exp = 0;
    std::memcpy(&s_mask_exp, expiredK.sessionSecretS.data(), sizeof(s_mask_exp));
    gatekeeper.Unlock(nonce, s_mask_exp);

    // Todos os 4 exports DEVEM falhar fechado imediatamente (ok == false)
    DxfExportOutput dxfExp = gatekeeper.ExecuteDxf(nonce, expiredK, digest, bars);
    assert(dxfExp.ok == false);
    assert(dxfExp.dxfSnippet.empty());

    PdfExportOutput pdfExp = gatekeeper.ExecutePdf(nonce, expiredK, digest, sections);
    assert(pdfExp.ok == false);
    assert(pdfExp.summaryText.empty());

    NormativeCheckOutput normExp = gatekeeper.ExecuteNormativeCheck(nonce, expiredK, digest, normIn);
    assert(normExp.ok == false);

    QuantityExportOutput qtyExp = gatekeeper.ExecuteQuantities(nonce, expiredK, digest, qtyBars);
    assert(qtyExp.ok == false);

    // --- Caso B: Keystone VÁLIDO (exp = now + 300s / 5 minutos) ---
    KeystoneArtifact validK;
    SetupFakeKeystone(validK, +300, "STRUCTURA-WM1:42:1700000000:valid:67890");
    validK.resultDigest = digest;

    uint64_t s_mask_val = 0;
    std::memcpy(&s_mask_val, validK.sessionSecretS.data(), sizeof(s_mask_val));
    gatekeeper.Unlock(nonce, s_mask_val);

    DxfExportOutput dxfVal = gatekeeper.ExecuteDxf(nonce, validK, digest, bars);
    assert(dxfVal.ok == true);
    assert(!dxfVal.dxfSnippet.empty());

    PdfExportOutput pdfVal = gatekeeper.ExecutePdf(nonce, validK, digest, sections);
    assert(pdfVal.ok == true);
    assert(!pdfVal.summaryText.empty());

    NormativeCheckOutput normVal = gatekeeper.ExecuteNormativeCheck(nonce, validK, digest, normIn);
    assert(normVal.ok == true);

    QuantityExportOutput qtyVal = gatekeeper.ExecuteQuantities(nonce, validK, digest, qtyBars);
    assert(qtyVal.ok == true);

    std::cout << "  -> OK: Keystone expirado (> 300s) bloqueia todos os 4 exports; Keystone valido desbloqueia." << std::endl;
}

// -------------------------------------------------------------
// Teste 3: Marca D'água Forense Estampada em Todos os Exports
// -------------------------------------------------------------
void TestForensicWatermarkStampedInAllFourExports() {
    std::cout << "[M6-3] Testando Estampa de Marca D'agua Forense nos 4 Exports..." << std::endl;

    ClientSession activeSess;
    activeSess.online = true;
    activeSess.licenseId = 42;
    activeSess.vmAllowToken = "0123456789abcdef0123456789abcdef";
    SetActiveClientSession(activeSess);

    ExportGatekeeper gatekeeper;
    uint64_t nonce = 0x1122334455667788ULL;
    gatekeeper.Initialize(nonce);

    double disp[3] = {0.001, 0.002, 0.003};
    double reac[3] = {100.0, 200.0, 300.0};
    std::string digest = ComputeResultDigest(disp, 3, reac, 3);

    std::string serverIssuedWm = "STRUCTURA-WM1:42:1773344000:export_full:abcdef0123456789abcdef0123456789";

    KeystoneArtifact k;
    SetupFakeKeystone(k, +300, serverIssuedWm);
    k.resultDigest = digest;

    uint64_t s_mask = 0;
    std::memcpy(&s_mask, k.sessionSecretS.data(), sizeof(s_mask));
    gatekeeper.Unlock(nonce, s_mask);

    // 1. DXF: estampa sob o grupo 999 do DXF
    std::vector<DxfBarInput> bars = { {1, 0, 0, 0, 10, 0, 0} };
    DxfExportOutput dxf = gatekeeper.ExecuteDxf(nonce, k, digest, bars);
    assert(dxf.ok == true);
    assert(dxf.forensicWatermark == serverIssuedWm);
    assert(dxf.dxfSnippet.find("999\nSTRUCTURA-FORENSIC-WM: " + serverIssuedWm) != std::string::npos);

    // 2. PDF / Relatório: estampa no rodapé do documento
    std::vector<PdfSectionInput> sections = { {1, 150.0, 50.0, 2.5} };
    PdfExportOutput pdf = gatekeeper.ExecutePdf(nonce, k, digest, sections);
    assert(pdf.ok == true);
    assert(pdf.forensicWatermark == serverIssuedWm);
    assert(pdf.summaryText.find("[MARCA DAGUA FORENSE: " + serverIssuedWm + "]") != std::string::npos);

    // 3. Verificações Normativas (Memorial): estampa no relatório
    NormativeCheckInput normIn{1, 500.0, 100.0, 0.005, 0.0005, 345.0};
    NormativeCheckOutput norm = gatekeeper.ExecuteNormativeCheck(nonce, k, digest, normIn);
    assert(norm.ok == true);
    assert(norm.forensicWatermark == serverIssuedWm);
    assert(norm.reportSnippet.find("[MARCA DAGUA FORENSE: " + serverIssuedWm + "]") != std::string::npos);

    // 4. Levantamento de Quantitativos: estampa no resumo de medição
    std::vector<QuantityBarInput> qtyBars = { {1, 10.0, 0.005, 7850.0} };
    QuantityExportOutput qty = gatekeeper.ExecuteQuantities(nonce, k, digest, qtyBars);
    assert(qty.ok == true);
    assert(qty.forensicWatermark == serverIssuedWm);
    assert(qty.reportSnippet.find("[MARCA DAGUA FORENSE: " + serverIssuedWm + "]") != std::string::npos);

    std::cout << "  -> OK: Token de marca d'agua forense estampado com fidelidade no DXF, PDF, Memorial e Quantitativos." << std::endl;
}

// -------------------------------------------------------------
// Teste 4: Integração de Parsing Real do Payload Wire do Servidor
// Valida que exp/ts chegam como inteiros Unix reais e NÃO como ISO-8601 strings
// -------------------------------------------------------------
void TestRealServerJsonPayloadParsingAndKeystoneVerification() {
    std::cout << "[M6-4] Testando Integracao Wire JSON real do /compute/validate..." << std::endl;

    // 1. Gera par de chaves Ed25519 para simular o servidor
    uint8_t serverPk[32];
    uint8_t serverSk[64];
    crypto_sign_keypair(serverPk, serverSk);

    std::string modelHash = "a1b2c3d4e5f60718293a4b5c6d7e8f90a1b2c3d4e5f60718293a4b5c6d7e8f90";
    double disp[3] = {0.001, 0.002, 0.003};
    double reac[3] = {100.0, 200.0, 300.0};
    std::string resultDigest = ComputeResultDigest(disp, 3, reac, 3);
    std::string nonce = "fedcba9876543210";
    int licenseId = 42;
    int ts = static_cast<int>(std::time(nullptr));
    int exp = ts + 300; // Validade curta real de 5 minutos

    // 2. O servidor assina o contexto com inteiros reais:
    // ctx = model_hash | result_digest | nonce | license_id | exp | ts
    std::string ctxStr = modelHash + "|" + resultDigest + "|" + nonce + "|" +
                         std::to_string(licenseId) + "|" +
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

    std::vector<uint8_t> sm(64 + signedBlob.size());
    unsigned long long smlen = 0;
    crypto_sign(sm.data(), &smlen, signedBlob.data(), signedBlob.size(), serverSk);
    uint8_t sigK[64];
    std::memcpy(sigK, sm.data(), 64);

    std::string sigKB64 = Base64Encode(sigK, 64);
    std::string wmToken = "STRUCTURA-WM1:42:" + std::to_string(ts) + ":compute_keystone:abcdef1234567890";

    // 3. Monta o payload JSON idêntico ao emitido pelo servidor na wire (com exp e ts numéricos!):
    std::string realWireJson = "{"
        "\"ok\":true,"
        "\"K\":\"" + sigKB64 + "\","
        "\"ts\":" + std::to_string(ts) + ","
        "\"exp\":" + std::to_string(exp) + ","
        "\"reason\":null,"
        "\"watermark_token\":\"" + wmToken + "\","
        "\"vm_allow_token\":null"
    "}";

    // 4. Simula o parsing do cliente em license_client.cpp:
    bool ok = JsonGetBool(realWireJson, "ok");
    assert(ok == true);

    std::string parsedSigKB64 = JsonGetString(realWireJson, "K");
    int parsedExp = JsonGetInt(realWireJson, "exp");
    int parsedTs = JsonGetInt(realWireJson, "ts");
    std::string parsedWm = JsonGetString(realWireJson, "watermark_token");

    // ASSERÇÕES CRÍTICAS DE ANTI-REGRESSÃO:
    // Confirma que parsedExp e parsedTs são a época Unix real (~1.79e9), e NÃO 2026!
    assert(parsedExp > 1700000000);
    assert(parsedTs > 1700000000);
    assert(parsedExp == exp);
    assert(parsedTs == ts);
    assert(parsedExp == parsedTs + 300);

    // 5. Decodifica a assinatura e reconstrói o contexto como o cliente faz:
    std::vector<uint8_t> clientSigK;
    bool decOk = Base64Decode(parsedSigKB64, clientSigK);
    assert(decOk && clientSigK.size() == 64);

    std::string clientCtxStr = modelHash + "|" + resultDigest + "|" + nonce + "|" +
                               std::to_string(licenseId) + "|" +
                               std::to_string(parsedExp) + "|" + std::to_string(parsedTs);

    Sha256 clientCtxHash;
    clientCtxHash.Update(clientCtxStr.data(), clientCtxStr.size());
    uint8_t clientCtxDigest[32];
    clientCtxHash.Final(clientCtxDigest);

    std::vector<uint8_t> clientSignedBlob;
    clientSignedBlob.reserve(22 + 32);
    clientSignedBlob.insert(clientSignedBlob.end(), kDomKeystone, kDomKeystone + 22);
    clientSignedBlob.insert(clientSignedBlob.end(), clientCtxDigest, clientCtxDigest + 32);

    bool sigValid = Ed25519VerifyCompact(
        clientSignedBlob.data(), clientSignedBlob.size(),
        clientSigK.data(), serverPk);

    // A assinatura DEVE validar com perfeição:
    assert(sigValid == true);

    // 6. Monta o KeystoneArtifact e testa com o ExportGatekeeper:
    KeystoneArtifact keystone;
    keystone.valid = true;
    keystone.modelHash = modelHash;
    keystone.resultDigest = resultDigest;
    keystone.nonce = nonce;
    keystone.licenseId = licenseId;
    keystone.exp = parsedExp;
    keystone.ts = parsedTs;
    keystone.signatureB64 = parsedSigKB64;
    keystone.watermarkToken = parsedWm;

    DeriveSessionSecretS(clientSigK.data(), nonce, licenseId, keystone.sessionSecretS);

    // Configura sessão ativa para autorizar VM no ambiente de teste
    ClientSession activeSess;
    activeSess.online = true;
    activeSess.licenseId = licenseId;
    activeSess.vmAllowToken = "0123456789abcdef0123456789abcdef";
    SetActiveClientSession(activeSess);

    ExportGatekeeper gatekeeper;
    uint64_t gateNonce = 0x9988776655443322ULL;
    gatekeeper.Initialize(gateNonce);

    uint64_t s_mask = 0;
    std::memcpy(&s_mask, keystone.sessionSecretS.data(), sizeof(s_mask));
    gatekeeper.Unlock(gateNonce, s_mask);

    std::vector<DxfBarInput> bars = { {1, 0, 0, 0, 10, 0, 0} };
    DxfExportOutput dxf = gatekeeper.ExecuteDxf(gateNonce, keystone, resultDigest, bars);

    // O export DEVE ter sucesso: o killswitch NÃO deve barrar porque parsedExp é época Unix real no futuro (+300s)
    assert(dxf.ok == true);
    assert(dxf.forensicWatermark == parsedWm);
    assert(!dxf.dxfSnippet.empty());

    // 7. Demonstração e contra-prova do bug que existia (se viesse como ISO string):
    std::string buggyIsoJson = "{\"exp\":\"2026-11-10T19:35:57Z\",\"ts\":\"2026-11-10T19:30:57Z\"}";
    int buggyExp = JsonGetInt(buggyIsoJson, "exp");
    assert(buggyExp == 2026); // Confirma que stoi truncava no ano 2026!

    std::cout << "  -> OK: Wire JSON parseia exp/ts numericos reais, assinatura Ed25519 valida 100% e export passa no killswitch." << std::endl;
}

// -------------------------------------------------------------
// Teste 5: Verificação Criptográfica Real do pass_sig do Nível B
// -------------------------------------------------------------
void TestRealServerJsonPayloadParsingAndLevelBGateVerification() {
    std::cout << "[M6-5] Testando Verificacao Criptografica Real do pass_sig do Nivel B..." << std::endl;

    // 1. Gera par de chaves Ed25519 do servidor
    uint8_t serverPk[32];
    uint8_t serverSk[64];
    crypto_sign_keypair(serverPk, serverSk);

    std::string feature = "detalhamento";
    int licenseId = 555;
    std::string nonce = "nonce_level_b_123";
    int exp = static_cast<int>(std::time(nullptr)) + 300;

    // 2. Monta o contexto assinado exatamente como o servidor faz (server/app/crypto.py::sign_gate_pass)
    std::string ctxStr = feature + "|" + std::to_string(licenseId) + "|" + nonce + "|" + std::to_string(exp);
    Sha256 ctxHash;
    ctxHash.Update(ctxStr.data(), ctxStr.size());
    uint8_t ctxDigest[32];
    ctxHash.Final(ctxDigest);

    static const uint8_t kDomGate[18] = {
        's','t','r','u','c','t','u','r','a','-','g','a','t','e','-','v','1','\0'
    };
    std::vector<uint8_t> signedBlob;
    signedBlob.reserve(18 + 32);
    signedBlob.insert(signedBlob.end(), kDomGate, kDomGate + 18);
    signedBlob.insert(signedBlob.end(), ctxDigest, ctxDigest + 32);

    std::vector<uint8_t> sm(64 + signedBlob.size());
    unsigned long long smlen = 0;
    crypto_sign(sm.data(), &smlen, signedBlob.data(), signedBlob.size(), serverSk);
    uint8_t passSig[64];
    std::memcpy(passSig, sm.data(), 64);
    std::string passSigB64 = Base64Encode(passSig, 64);

    // 3. Monta o payload JSON wire real devolvido por /license/check
    std::string wmToken = "STRUCTURA-WM1:555:" + std::to_string(exp) + ":detalhamento:fedcba9876543210fedcba9876543210";
    std::string realWireJson = "{"
        "\"ok\":true,"
        "\"exp\":" + std::to_string(exp) + ","
        "\"pass_sig\":\"" + passSigB64 + "\","
        "\"watermark_token\":\"" + wmToken + "\""
    "}";

    // 4. Teste de Assinatura Válida:
    int parsedExp = JsonGetInt(realWireJson, "exp");
    std::string parsedSigB64 = JsonGetString(realWireJson, "pass_sig");
    assert(parsedExp == exp);
    assert(!parsedSigB64.empty());

    std::vector<uint8_t> clientSigPass;
    bool decOk = Base64Decode(parsedSigB64, clientSigPass);
    assert(decOk && clientSigPass.size() == 64);

    std::string clientCtxStr = feature + "|" + std::to_string(licenseId) + "|" + nonce + "|" + std::to_string(parsedExp);
    Sha256 clientCtxHash;
    clientCtxHash.Update(clientCtxStr.data(), clientCtxStr.size());
    uint8_t clientCtxDigest[32];
    clientCtxHash.Final(clientCtxDigest);

    std::vector<uint8_t> clientSignedBlob;
    clientSignedBlob.reserve(18 + 32);
    clientSignedBlob.insert(clientSignedBlob.end(), kDomGate, kDomGate + 18);
    clientSignedBlob.insert(clientSignedBlob.end(), clientCtxDigest, clientCtxDigest + 32);

    bool sigValid = Ed25519VerifyCompact(
        clientSignedBlob.data(), clientSignedBlob.size(),
        clientSigPass.data(), serverPk);
    assert(sigValid == true);

    // 5. Teste Adversarial 1: Assinatura adulterada DEVE ser RECUSADA
    std::vector<uint8_t> tamperedSig = clientSigPass;
    tamperedSig[0] ^= 0xFF;
    bool tamperedValid = Ed25519VerifyCompact(
        clientSignedBlob.data(), clientSignedBlob.size(),
        tamperedSig.data(), serverPk);
    assert(tamperedValid == false);

    // 6. Teste Adversarial 2: Expiração trocada (replay / extensão não autorizada) DEVE ser RECUSADA
    std::string forgedCtxStr = feature + "|" + std::to_string(licenseId) + "|" + nonce + "|" + std::to_string(parsedExp + 3600);
    Sha256 forgedCtxHash;
    forgedCtxHash.Update(forgedCtxStr.data(), forgedCtxStr.size());
    uint8_t forgedCtxDigest[32];
    forgedCtxHash.Final(forgedCtxDigest);

    std::vector<uint8_t> forgedSignedBlob;
    forgedSignedBlob.reserve(18 + 32);
    forgedSignedBlob.insert(forgedSignedBlob.end(), kDomGate, kDomGate + 18);
    forgedSignedBlob.insert(forgedSignedBlob.end(), forgedCtxDigest, forgedCtxDigest + 32);

    bool forgedExpValid = Ed25519VerifyCompact(
        forgedSignedBlob.data(), forgedSignedBlob.size(),
        clientSigPass.data(), serverPk);
    assert(forgedExpValid == false);

    // 7. Teste Adversarial 3: Feature trocada ("memorial" ao invés de "detalhamento") DEVE ser RECUSADA
    std::string wrongFeatureCtx = "memorial|" + std::to_string(licenseId) + "|" + nonce + "|" + std::to_string(parsedExp);
    Sha256 wrongFeatureHash;
    wrongFeatureHash.Update(wrongFeatureCtx.data(), wrongFeatureCtx.size());
    uint8_t wrongFeatureDigest[32];
    wrongFeatureHash.Final(wrongFeatureDigest);

    std::vector<uint8_t> wrongFeatureBlob;
    wrongFeatureBlob.reserve(18 + 32);
    wrongFeatureBlob.insert(wrongFeatureBlob.end(), kDomGate, kDomGate + 18);
    wrongFeatureBlob.insert(wrongFeatureBlob.end(), wrongFeatureDigest, wrongFeatureDigest + 32);

    bool wrongFeatureValid = Ed25519VerifyCompact(
        wrongFeatureBlob.data(), wrongFeatureBlob.size(),
        clientSigPass.data(), serverPk);
    assert(wrongFeatureValid == false);

    // 8. Teste Adversarial 4: Nonce trocado DEVE ser RECUSADO
    std::string wrongNonceCtx = feature + "|" + std::to_string(licenseId) + "|other_nonce|" + std::to_string(parsedExp);
    Sha256 wrongNonceHash;
    wrongNonceHash.Update(wrongNonceCtx.data(), wrongNonceCtx.size());
    uint8_t wrongNonceDigest[32];
    wrongNonceHash.Final(wrongNonceDigest);

    std::vector<uint8_t> wrongNonceBlob;
    wrongNonceBlob.reserve(18 + 32);
    wrongNonceBlob.insert(wrongNonceBlob.end(), kDomGate, kDomGate + 18);
    wrongNonceBlob.insert(wrongNonceBlob.end(), wrongNonceDigest, wrongNonceDigest + 32);

    bool wrongNonceValid = Ed25519VerifyCompact(
        wrongNonceBlob.data(), wrongNonceBlob.size(),
        clientSigPass.data(), serverPk);
    assert(wrongNonceValid == false);

    std::cout << "  -> OK: pass_sig de Nivel B validado contra Ed25519; tamper de assinatura, exp, feature e nonce recusados estritamente." << std::endl;
}

int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "STRUCTURA CLIENT - SUITE DE TESTES DO MARCO M6 (ABUSO)" << std::endl;
    std::cout << "========================================================" << std::endl;

    TestVmDetectionAndPoison();
    TestExportGatekeeperKeystoneExpirationKillSwitch();
    TestForensicWatermarkStampedInAllFourExports();
    TestRealServerJsonPayloadParsingAndKeystoneVerification();
    TestRealServerJsonPayloadParsingAndLevelBGateVerification();

    std::cout << "\n========================================================" << std::endl;
    std::cout << "TODOS OS TESTES DO MARCO M6 PASSARAM COM SUCESSO!" << std::endl;
    std::cout << "========================================================" << std::endl;
    return 0;
}
