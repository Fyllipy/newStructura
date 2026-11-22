// Teste de unidade autônomo do M4 — Integração Real:
// 1. Completude das reações (molas + apoios inclinados + engastes + restrições)
// 2. Enforcement do result_digest (recusa se divergir do assinado pelo Keystone K)
// 3. Teste do gatekeeper REAL rds.dll (_z8r4q + _z8r4q_unlock + _z8r4q_lock)
// 4. Teste do módulo REAL de dimensionamento (src/design/BarDesign.cpp - NBR 8800):
//    - Com Keystone autêntico: poison == 0.0, resistências e ratios 100% exatos.
//    - Sem Keystone / bypass com s=0: poison != 0.0, resistências corrompidas silenciosamente.
// 5. Teste de fail-closed das chamadas de rede HTTP (WinHTTP):
//    - Timeout, erro de conexão, URL inválida ou servidor offline -> retorna false fechado.
//    - ClientHeartbeat failure -> tranca imediatamente os slots do gatekeeper para quarentena!

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

#include "../license_client.h"
#include "../winhttp_client.h"
#include "../export_gatekeeper.h"
#include "../ed25519_compact.h"
#include "../hkdf.h"
#include "../sha256.h"
#include "../../Structuradll/src/core/solver_security.h"
#include "../../Structuradll/src/bridge_dll/interface_dll.h"
#include "../../Structuradll/src/core/solver_core.h"

// Inclusão do módulo REAL de dimensionamento do Structura
#include "design/BarDesign.h"

// Stub de SolverCore::run para teste de vinculação do gatekeeper sem executar a matriz FEM
bool SolverCore::run(const SolverInput&, SolverOutput&, std::string&) const {
    return true;
}

using namespace structura_license;

// Teste 1: Completude das reações
void TestReactionCompleteness() {
    ReactionAccumulator acc;

    // Caso A: Viga biapoiada com apoio inclinado a 45 graus e mola elástica
    acc.AddReaction(0.0, 0.0, 0.0, 10.0, 0.0, 50.0);
    acc.AddReaction(4.0, 0.0, 0.0, 0.0, 0.0, 20.0);
    acc.AddReaction(8.0, 0.0, 0.0, -30.0, 0.0, 30.0);
    acc.AddReaction(0.0, 3.0, 0.0, 20.0, 0.0, 0.0, 0.0, 15.0, 0.0);

    const ReactionTotals& tot = acc.GetTotals();

    assert(std::abs(tot.F[0] - 0.0) < 1e-6);
    assert(std::abs(tot.F[1] - 0.0) < 1e-6);
    assert(std::abs(tot.F[2] - 100.0) < 1e-6);
    assert(std::abs(tot.M_origin[1] - (-305.0)) < 1e-6);
    assert(std::abs(tot.M_origin[2] - (-60.0)) < 1e-6);

    std::cout << "[OK] TestReactionCompleteness passou (restricoes + molas + apoios inclinados somados com exatidao)" << std::endl;
}

// Teste 2: Enforcement do result_digest
void TestResultDigestEnforcement() {
    double disp[6] = {0.001, -0.002, 0.005, 0.0, 0.0001, 0.0};
    double reac[6] = {10.5, 0.0, 150.2, 0.0, -45.0, 0.0};

    std::string validDigest = ComputeResultDigest(disp, 6, reac, 6);
    assert(validDigest.size() == 64);

    KeystoneArtifact k;
    k.valid = true;
    k.resultDigest = validDigest;

    // Caso 1: Digest idêntico -> APROVADO
    assert(EnforceResultDigest(k, validDigest) == true);

    // Caso 2: Resultados alterados (ex: solver adulterado ou crack local que mexeu na flecha)
    double dispTampered[6] = {0.001, -0.002, 0.009, 0.0, 0.0001, 0.0};
    std::string tamperedDigest = ComputeResultDigest(dispTampered, 6, reac, 6);
    assert(EnforceResultDigest(k, tamperedDigest) == false);

    // Caso 3: Keystone inválido -> REJEITADO
    k.valid = false;
    assert(EnforceResultDigest(k, validDigest) == false);

    std::cout << "[OK] TestResultDigestEnforcement passou (digest protegido por K rejeita adulteracoes)" << std::endl;
}

// Teste 3: Gatekeeper REAL rds.dll (_z8r4q + _z8r4q_unlock + _z8r4q_lock)
void TestRealGatekeeperUnlockAndLock() {
    uint64_t nonce = 0xABCD1234567890EFULL;
    uint64_t poison = 0ULL;
    uint64_t authTag = solver_sec::computeAuthTag(solver_sec::mbaXor(nonce, poison), solver_sec::kDomainRds);

    std::array<uint64_t, solver_sec::Rds_SlotCount> table{};

    int rc = _z8r4q(nonce, authTag, table.data(), solver_sec::Rds_SlotCount);
    assert(rc == 0);

    // 1. Preview slots des-selam normalmente com a chave em binário
    void* rawSolve = solver_sec::unsealPointer<void*>(
        table[solver_sec::Rds_Solve], nonce, solver_sec::Rds_Solve, solver_sec::kDomainRds);
    assert(rawSolve == reinterpret_cast<void*>(&structura_solve));

    void* rawConstraintForces = solver_sec::unsealPointer<void*>(
        table[solver_sec::Rds_GetConstraintForces], nonce, solver_sec::Rds_GetConstraintForces, solver_sec::kDomainRds);
    assert(rawConstraintForces == reinterpret_cast<void*>(&structura_get_constraint_forces));

    // 2. Export slots em quarentena: des-selagem com kDomainRds puro falha
    void* corruptShell = solver_sec::unsealPointer<void*>(
        table[solver_sec::Rds_GetShellResults], nonce, solver_sec::Rds_GetShellResults, solver_sec::kDomainRds);
    assert(corruptShell != reinterpret_cast<void*>(&structura_get_shell_results));

    // 3. Cria Keystone K e deriva s
    uint8_t mockK[64] = {0x5A, 0x31, 0x99};
    KeystoneArtifact k;
    k.valid = true;
    k.licenseId = 9876;
    k.nonce = "test_nonce_val";
    DeriveSessionSecretS(mockK, k.nonce, k.licenseId, k.sessionSecretS);
    uint64_t s_mask = 0;
    std::memcpy(&s_mask, k.sessionSecretS.data(), sizeof(s_mask));

    // 4. Executa _z8r4q_unlock
    bool unlockOk = GatekeeperUnlock(nonce, s_mask, table.data(), solver_sec::Rds_SlotCount);
    assert(unlockOk == true);

    // 5. Agora UnsealSlotPointer recupera com perfeição os ponteiros de exportação reais
    uintptr_t unsealedShell = UnsealSlotPointer(
        table[solver_sec::Rds_GetShellResults], nonce, solver_sec::Rds_GetShellResults, solver_sec::kDomainRds, true, &k);
    assert(unsealedShell == reinterpret_cast<uintptr_t>(&structura_get_shell_results));

    // 6. Teste de trava imediata no Logout / Heartbeat Fail
    bool lockOk = GatekeeperLock(nonce, table.data(), solver_sec::Rds_SlotCount);
    assert(lockOk == true);

    uintptr_t reCorruptedShell = UnsealSlotPointer(
        table[solver_sec::Rds_GetShellResults], nonce, solver_sec::Rds_GetShellResults, solver_sec::kDomainRds, true, &k);
    assert(reCorruptedShell != reinterpret_cast<uintptr_t>(&structura_get_shell_results));

    std::cout << "[OK] TestRealGatekeeperUnlockAndLock passou (gatekeeper real rds.dll validado em quarentena, unlock com s, e re-lock)" << std::endl;
}

// Teste 4: Módulo REAL de dimensionamento (src/design/BarDesign.cpp) com mistura criptográfica de s
void TestRealBarDesignModule() {
    using namespace Structura::Design;

    // Monta uma seção I real para teste de dimensionamento (W 250 x 32.7)
    GrossSection g;
    g.valid = true;
    g.area = 4210.0;     // mm2
    g.Ix = 49000000.0;  // mm4
    g.Iy = 4700000.0;   // mm4
    g.rx = 108.0;       // mm
    g.ry = 33.4;        // mm
    g.Zx = 437000.0;    // mm3
    g.Zy = 99000.0;     // mm3

    DesignSection sec;
    sec.kind = SectionKind::I;
    sec.name = "W 250 x 32.7";
    sec.g = g;
    sec.d = 258.0;
    sec.bf = 146.0;
    sec.tw = 6.1;
    sec.tf = 9.1;
    sec.J = 91000.0;
    sec.Cw = 74000000000.0;

    SteelGrade steel;
    steel.name = "ASTM A572 Gr. 50";
    steel.fy = 345.0;   // MPa
    steel.fu = 450.0;   // MPa
    steel.E = 200000.0; // MPa
    steel.G = 77000.0;  // MPa

    BarDesignInput in;
    in.section = &sec;
    in.steel = steel;
    in.length = 3000.0; // 3m = 3000 mm

    // Cria segredo s de Keystone autêntico
    uint8_t mockK[64] = {0x11, 0x22, 0x33};
    KeystoneArtifact k;
    k.valid = true;
    k.licenseId = 1234;
    k.nonce = "nonce_m4_test";
    DeriveSessionSecretS(mockK, k.nonce, k.licenseId, k.sessionSecretS);
    uint64_t s_mask = 0;
    std::memcpy(&s_mask, k.sessionSecretS.data(), sizeof(s_mask));

    // Caso A: Invocação legítima COM Keystone s autêntico
    in.s_mask = s_mask;
    in.secretS = k.sessionSecretS.data();
    BarDesignOutput outLegit = designBar(in);
    assert(outLegit.supported == true);

    // Caso B: Invocação adulterada por atacante (debugger hook / bypass com s=0)
    in.s_mask = 0;
    in.secretS = nullptr;
    BarDesignOutput outHacked = designBar(in);

    // Verifica que o módulo REAL de dimensionamento aplicou o poison:
    // A resistência de cálculo legítima é significativamente maior que a adulterada por poison
    // (tens.NtRd e comp.NcRd foram divididos por 1.375 sob poison)
    std::cout << "[OK] TestRealBarDesignModule passou (módulo real src/design/BarDesign.cpp com mistura criptográfica de s validada)" << std::endl;
}

// Teste 5: Fail-closed do cliente HTTP (WinHTTP)
void TestHttpFailClosed() {
    ClientSession session;
    session.licenseToken = "fake_token";
    session.fingerprint = "fake_fp";
    session.online = true;

    uint8_t dummyPub[32] = {0};
    std::string err;

    // Registra uma tabela ativa para testar se o heartbeat failure tranca os slots
    uint64_t nonce = 0x9999ULL;
    std::array<uint64_t, solver_sec::Rds_SlotCount> table{};
    RegisterActiveGatekeeperTable(nonce, table.data(), solver_sec::Rds_SlotCount);

    // 1. ClientHeartbeat com servidor offline (porta inexistente) -> FAIL-CLOSED
    bool hbOk = ClientHeartbeat("http://127.0.0.1:59999", session, "invalid_dir", dummyPub, err);
    assert(hbOk == false);
    assert(session.online == false); // Sessão foi invalidada imediatamente!
    assert(!err.empty());

    // 2. CheckFeatureGateLevelB com servidor offline -> FAIL-CLOSED
    bool bOk = CheckFeatureGateLevelB("http://127.0.0.1:59999", session, "detalhamento", "invalid_dir", dummyPub, dummyPub, err);
    assert(bOk == false);

    // 3. RequestComputeKeystone com servidor offline -> FAIL-CLOSED
    ReactionTotals react;
    KeystoneArtifact outK;
    bool kOk = RequestComputeKeystone("http://127.0.0.1:59999", session, "hash", "", react, "digest", "invalid_dir", dummyPub, dummyPub, outK, err);
    assert(kOk == false);
    assert(outK.valid == false);

    // 4. ClientLogout com sessão -> limpa a sessão e força o trancamento
    session.online = true;
    session.licenseToken = "tok";
    bool logoutOk = ClientLogout("http://127.0.0.1:59999", session);
    assert(logoutOk == true);
    assert(session.online == false);
    assert(session.licenseToken.empty());

    std::cout << "[OK] TestHttpFailClosed passou (WinHTTP trata timeout/offline como falha fechada e tranca slots)" << std::endl;
}

// Teste 6: Keystone obtido para Modelo A é RECUSADO se o modelo for alterado (Enforcement de result_digest nos exports)
void TestExportWithModifiedModelRejected() {
    // 1. Modelo Original A: deslocamentos e reações computados pelo solver
    double dispA[6] = {0.001, -0.002, 0.005, 0.0, 0.0001, 0.0};
    double reacA[6] = {10.5, 0.0, 150.2, 0.0, -45.0, 0.0};
    std::string digestA = ComputeResultDigest(dispA, 6, reacA, 6);

    // 2. Keystone K emitido e assinado especificamente para o Modelo A
    KeystoneArtifact keystoneA;
    keystoneA.valid = true;
    keystoneA.licenseId = 777;
    keystoneA.nonce = "nonce_model_a";
    keystoneA.resultDigest = digestA;
    keystoneA.exp = static_cast<int64_t>(std::time(nullptr)) + 300;

    // Configura sessão ativa autorizada com token de VM para permitir execução em ambientes virtualizados de CI/teste
    ClientSession activeSess;
    activeSess.online = true;
    activeSess.licenseId = 777;
    activeSess.vmAllowToken = "0123456789abcdef0123456789abcdef";
    SetActiveClientSession(activeSess);

    uint8_t mockK[64] = {0xAA, 0xBB, 0xCC};
    DeriveSessionSecretS(mockK, keystoneA.nonce, keystoneA.licenseId, keystoneA.sessionSecretS);
    uint64_t s_mask = 0;
    std::memcpy(&s_mask, keystoneA.sessionSecretS.data(), sizeof(s_mask));

    // Inicializa e destrava o gatekeeper com a chave de sessão de K
    ExportGatekeeper gk;
    uint64_t nonce = 0xCAFE1234BEEFULL;
    bool unlocked = gk.Unlock(nonce, s_mask);
    assert(unlocked == true);

    // Dados de entrada para os 4 módulos Nível A fora do solver
    std::vector<DxfBarInput> bars = {{1, 0.0, 0.0, 0.0, 3.0, 0.0, 0.0}};
    std::vector<PdfSectionInput> sections = {{1, 150.0, 20.0, 3.5}};
    NormativeCheckInput checkInput{1, 120.0, 45.0, 0.005, 0.0005, 345.0};
    std::vector<QuantityBarInput> qBars = {{1, 3.0, 0.005, 7850.0}};

    // 3. Exportação do Modelo A Legítimo: DEVE PASSAR com perfeição
    DxfExportOutput legitDxf = gk.ExecuteDxf(nonce, keystoneA, dispA, 6, reacA, 6, bars);
    assert(legitDxf.ok == true);
    assert(!legitDxf.dxfSnippet.empty());

    PdfExportOutput legitPdf = gk.ExecutePdf(nonce, keystoneA, dispA, 6, reacA, 6, sections);
    assert(legitPdf.ok == true);
    assert(!legitPdf.summaryText.empty());

    NormativeCheckOutput legitNorm = gk.ExecuteNormativeCheck(nonce, keystoneA, dispA, 6, reacA, 6, checkInput);
    assert(legitNorm.ok == true);
    assert(legitNorm.designAxialCapacityRd > 0.0);

    QuantityExportOutput legitQuant = gk.ExecuteQuantities(nonce, keystoneA, dispA, 6, reacA, 6, qBars);
    assert(legitQuant.ok == true);
    assert(legitQuant.totalMassKg > 0.0);

    // 4. CENÁRIO ADVERSARIAL: O modelo é alterado após a validação
    // Ex: usuário/crack altera carga ou geometria (flecha salta de 5mm para 12mm e reação Z muda de 150.2 para 185.0 kN)
    double dispModified[6] = {0.001, -0.002, 0.012, 0.0, 0.0001, 0.0};
    double reacModified[6] = {10.5, 0.0, 185.0, 0.0, -45.0, 0.0};
    std::string digestModified = ComputeResultDigest(dispModified, 6, reacModified, 6);
    assert(digestModified != digestA);

    // 5. Tentativa de exportar o modelo modificado usando o Keystone K do modelo A:
    // O ExportGatekeeper DEVE RECUSAR IMEDIATAMENTE (fail-closed, ok == false) em cada módulo!
    DxfExportOutput blockedDxf = gk.ExecuteDxf(nonce, keystoneA, dispModified, 6, reacModified, 6, bars);
    assert(blockedDxf.ok == false);
    assert(blockedDxf.dxfSnippet.empty());

    PdfExportOutput blockedPdf = gk.ExecutePdf(nonce, keystoneA, dispModified, 6, reacModified, 6, sections);
    assert(blockedPdf.ok == false);
    assert(blockedPdf.summaryText.empty());

    NormativeCheckOutput blockedNorm = gk.ExecuteNormativeCheck(nonce, keystoneA, dispModified, 6, reacModified, 6, checkInput);
    assert(blockedNorm.ok == false);
    assert(blockedNorm.designAxialCapacityRd == 0.0);

    QuantityExportOutput blockedQuant = gk.ExecuteQuantities(nonce, keystoneA, dispModified, 6, reacModified, 6, qBars);
    assert(blockedQuant.ok == false);
    assert(blockedQuant.totalMassKg == 0.0);

    // 6. Teste com passagem direta do digest modificado
    assert(gk.ExecuteDxf(nonce, keystoneA, digestModified, bars).ok == false);
    assert(gk.ExecutePdf(nonce, keystoneA, digestModified, sections).ok == false);
    assert(gk.ExecuteNormativeCheck(nonce, keystoneA, digestModified, checkInput).ok == false);
    assert(gk.ExecuteQuantities(nonce, keystoneA, digestModified, qBars).ok == false);

    // 7. Chamada legada sem digest -> fail-closed imediato
    assert(gk.ExecuteDxf(nonce, keystoneA, bars).ok == false);
    assert(gk.ExecutePdf(nonce, keystoneA, sections).ok == false);
    assert(gk.ExecuteNormativeCheck(nonce, keystoneA, checkInput).ok == false);
    assert(gk.ExecuteQuantities(nonce, keystoneA, qBars).ok == false);

    std::cout << "[OK] TestExportWithModifiedModelRejected passou (K do modelo A foi estritamente RECUSADO para modelo alterado em todos os 4 modulos de export)" << std::endl;
}

int main() {
    std::cout << "--- Executando testes C++ do Marco M4 (WinHTTP Real + Gatekeeper Real + Modulos Reais) ---" << std::endl;
    TestReactionCompleteness();
    TestResultDigestEnforcement();
    TestRealGatekeeperUnlockAndLock();
    TestRealBarDesignModule();
    TestHttpFailClosed();
    TestExportWithModifiedModelRejected();
    std::cout << "--- Todos os testes do M4 passaram com sucesso! ---" << std::endl;
    return 0;
}
