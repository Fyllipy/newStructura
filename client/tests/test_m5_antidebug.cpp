// Teste de unidade autônomo do Marco M5 — Anti-Tamper / Anti-Debug / Ofuscação / DLL Cifrada
// Base: SPEC.md (§9), PLAN.md (M5).

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <fstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "../license_client.h"
#include "../dll_cipher.h"
#include "../export_gatekeeper.h"
#include "../../Structuradll/src/core/solver_security.h"
#include "../../Structuradll/src/bridge_dll/interface_dll.h"
#include "../../Structuradll/src/core/solver_core.h"

// Inclusão do módulo real de dimensionamento do Structura
#include "design/BarDesign.h"

using namespace structura_license;

// Teste 1: Detecção de Software Breakpoints (0xCC, 0xCD 0x03, 0x0F 0x0B)
void TestSoftwareBreakpointDetection() {
    // 1. Prólogo pristino de função x64 (push rbp; mov rbp, rsp; sub rsp, 0x20)
    alignas(16) unsigned char cleanPrologue[32] = {
        0x55,                               // push rbp
        0x48, 0x89, 0xE5,                   // mov rbp, rsp
        0x48, 0x83, 0xEC, 0x20,             // sub rsp, 0x20
        0x48, 0x8B, 0x05, 0x10, 0x00, 0x00, 0x00, // mov rax, [rip+0x10]
        0xC3                                // ret
    };
    uint64_t pClean = solver_sec::detail_adbg::scanFunctionBreakpoint(cleanPrologue);
    assert(pClean == 0ULL);

    // 2. Breakpoint INT 3 (0xCC) inserido no prólogo (ex: atacante colocou breakpoint na entrada)
    alignas(16) unsigned char bpInt3[32];
    std::memcpy(bpInt3, cleanPrologue, sizeof(cleanPrologue));
    bpInt3[0] = 0xCC; // INT 3 no byte 0
    uint64_t pBp0 = solver_sec::detail_adbg::scanFunctionBreakpoint(bpInt3);
    assert(pBp0 != 0ULL);
    assert((pBp0 & 0xBAD0CC0000000000ULL) == 0xBAD0CC0000000000ULL);

    // Breakpoint INT 3 inserido no meio da instrução (byte 4)
    std::memcpy(bpInt3, cleanPrologue, sizeof(cleanPrologue));
    bpInt3[4] = 0xCC;
    uint64_t pBp4 = solver_sec::detail_adbg::scanFunctionBreakpoint(bpInt3);
    assert(pBp4 != 0ULL);

    // 3. Breakpoint de 2 bytes INT 3 (0xCD 0x03)
    alignas(16) unsigned char bpCd03[32];
    std::memcpy(bpCd03, cleanPrologue, sizeof(cleanPrologue));
    bpCd03[0] = 0xCD;
    bpCd03[1] = 0x03;
    uint64_t pCd = solver_sec::detail_adbg::scanFunctionBreakpoint(bpCd03);
    assert(pCd == 0xBAD0CD0300000002ULL);

    // 4. Instrução de exceção UD2 (0x0F 0x0B)
    alignas(16) unsigned char bpUd2[32];
    std::memcpy(bpUd2, cleanPrologue, sizeof(cleanPrologue));
    bpUd2[0] = 0x0F;
    bpUd2[1] = 0x0B;
    uint64_t pUd2 = solver_sec::detail_adbg::scanFunctionBreakpoint(bpUd2);
    assert(pUd2 == 0xBAD00F0B00000004ULL);

    std::cout << "[OK] TestSoftwareBreakpointDetection passou (detecta 0xCC, 0xCD 0x03 e UD2 em prologos)" << std::endl;
}

// Teste 2: Detecção de Inline Hooks (0xE9, 0xEB, 0xFF 0x25, 0x48 0xB8)
void TestInlineHookDetection() {
    // 1. Hook tipo JMP rel32 (0xE9 - padrão MinHook, Detours, x64dbg)
    alignas(16) unsigned char hookRel32[32] = {
        0xE9, 0x78, 0x56, 0x34, 0x12, // jmp rel32 (salto para payload injetado)
        0x90, 0x90, 0x90, 0xC3
    };
    uint64_t pHook32 = solver_sec::detail_adbg::scanFunctionBreakpoint(hookRel32);
    assert(pHook32 != 0ULL);
    assert((pHook32 & 0xBAD0E90000000000ULL) == 0xBAD0E90000000000ULL);

    // 2. Hook tipo JMP rel8 (0xEB - salto curto)
    alignas(16) unsigned char hookRel8[32] = {
        0xEB, 0x0E, 0x90, 0x90, 0xC3
    };
    uint64_t pHook8 = solver_sec::detail_adbg::scanFunctionBreakpoint(hookRel8);
    assert(pHook8 != 0ULL);
    assert((pHook8 & 0xBAD0EB0000000000ULL) == 0xBAD0EB0000000000ULL);

    // 3. Hook tipo JMP [rip+disp32] (0xFF 0x25 - 64-bit indirect trampoline)
    alignas(16) unsigned char hookInd[32] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, // jmp qword ptr [rip]
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22
    };
    uint64_t pHookInd = solver_sec::detail_adbg::scanFunctionBreakpoint(hookInd);
    assert(pHookInd == 0xBAD0FF2500000006ULL);

    // 4. Hook tipo MOV RAX, imm64; JMP RAX (0x48 0xB8 ... 0xFF 0xE0)
    alignas(16) unsigned char hookRax[32] = {
        0x48, 0xB8, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, // mov rax, 0x8877665544332211
        0xFF, 0xE0,                                                 // jmp rax
        0xC3
    };
    uint64_t pHookRax = solver_sec::detail_adbg::scanFunctionBreakpoint(hookRax);
    assert(pHookRax == 0xBAD048B800000007ULL);

    std::cout << "[OK] TestInlineHookDetection passou (detecta trampolines JMP 0xE9, 0xEB, 0xFF 0x25 e 0x48 0xB8)" << std::endl;
}

// Teste 3: Envenenamento Silencioso de s e Degradação Física dos Cálculos (M5)
void TestPoisonKeystoneSecretAndPhysicsDegradation() {
    using namespace Structura::Design;

    // 1. Gera segredo s genuíno (sem envenenamento)
    uint8_t mockK[64] = {0x01, 0x02, 0x03, 0x04};
    std::array<uint8_t, 32> secretClean{};
    DeriveSessionSecretS(mockK, "nonce_clean", 100, secretClean);

    uint64_t maskClean = 0;
    std::memcpy(&maskClean, secretClean.data(), sizeof(maskClean));

    // Sem poison: desvio físico deve ser 0.0 (100% exato)
    double poisonClean = solver_sec::computeKeystonePoison(maskClean, secretClean.data());
    assert(poisonClean == 0.0);

    // 2. Simula detecção de anti-debug (poison != 0) e aplica poisonKeystoneSecret
    std::array<uint8_t, 32> secretPoisoned = secretClean;
    const uint64_t adbgPoisonMask = 0xD3B60001A5A5F00DULL; // flag de PEB BeingDebugged
    solver_sec::poisonKeystoneSecret(secretPoisoned.data(), adbgPoisonMask);

    // O segredo foi irreversivelmente alterado
    assert(secretPoisoned != secretClean);

    // Quando secretS envenenado é avaliado contra a máscara antiga ou vice-versa:
    double poisonEval = solver_sec::computeKeystonePoison(maskClean, secretPoisoned.data());
    assert(poisonEval > 0.0); // Desvio substancial ativado!

    // 3. Verificação do Impacto Físico no módulo real BarDesign (NBR 8800)
    GrossSection g;
    g.valid = true;
    g.area = 4210.0;
    g.Ix = 49000000.0;
    g.Iy = 4700000.0;
    g.rx = 108.0;
    g.ry = 33.4;
    g.Zx = 437000.0;
    g.Zy = 99000.0;

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
    steel.fy = 345.0;
    steel.fu = 450.0;
    steel.E = 200000.0;
    steel.G = 77000.0;

    BarDesignInput inClean;
    inClean.section = &sec;
    inClean.steel = steel;
    inClean.length = 3000.0;
    inClean.s_mask = maskClean;
    inClean.secretS = secretClean.data();

    BarDesignOutput outPristine = designBar(inClean);
    assert(outPristine.supported == true);

    // Caso B: Invocação sob envenenamento (debugger ativo)
    BarDesignInput inPoisoned = inClean;
    inPoisoned.s_mask = maskClean;
    inPoisoned.secretS = secretPoisoned.data();

    BarDesignOutput outDegraded = designBar(inPoisoned);
    assert(outDegraded.supported == true);

    // Confirma degradação silenciosa das resistências de cálculo via RealNormativeCheck (NBR 8800):
    NormativeCheckInput checkIn{1, 500.0, 100.0, 0.005, 0.0005, 345.0};
    NormativeCheckOutput normClean = ExportGatekeeper::RealNormativeCheck(checkIn, maskClean, secretClean.data());
    NormativeCheckOutput normPoisoned = ExportGatekeeper::RealNormativeCheck(checkIn, maskClean, secretPoisoned.data());
    assert(normClean.ok == true && normPoisoned.ok == true);
    assert(normClean.designAxialCapacityRd > normPoisoned.designAxialCapacityRd);
    assert(normPoisoned.utilizationRatio > normClean.utilizationRatio);
    double capRatio = normClean.designAxialCapacityRd / normPoisoned.designAxialCapacityRd;
    assert(capRatio > 1.20); // Capacidade de resistência reduzida sob poison

    // 4. Verificação no Módulo Real de Quantitativos
    std::vector<QuantityBarInput> qBars = {{1, 10.0, 0.005, 7850.0}};
    QuantityExportOutput qPristine = ExportGatekeeper::RealQuantityExport(qBars, maskClean, secretClean.data());
    QuantityExportOutput qPoisoned = ExportGatekeeper::RealQuantityExport(qBars, maskClean, secretPoisoned.data());
    assert(qPoisoned.totalMassKg > qPristine.totalMassKg); // Massa adulterada silenciosamente (+40%)

    std::cout << "[OK] TestPoisonKeystoneSecretAndPhysicsDegradation passou (poison corrompe silenciosamente sem crash)" << std::endl;
}

// Teste 4: Cifra de DLL em Repouso e Decifração Segura em Memória (TweetNaCl XSalsa20-Poly1305)
void TestDllCipherInReposeAndMemory() {
    // 1. Simula DLL bruta com cabeçalho MZ/PE
    std::vector<uint8_t> fakeDll(2048, 0x90);
    fakeDll[0] = 'M';
    fakeDll[1] = 'Z';
    fakeDll[0x3C] = 0x80;
    fakeDll[0x80] = 'P';
    fakeDll[0x81] = 'E';

    // Cria sessão do cliente
    ClientSession session;
    session.online = true;
    session.licenseToken = "token_m5_session_valid";
    session.fingerprint = "fp_unique_hw_machine";
    session.licenseId = 554433;

    // 2. Derivação de chave de envelope vinculada à sessão
    uint8_t key[32];
    bool keyOk = DeriveDllSessionKey(session, key);
    assert(keyOk == true);

    // 3. Cifra a DLL em repouso
    std::vector<uint8_t> encryptedDll;
    bool encOk = EncryptDllBuffer(fakeDll.data(), fakeDll.size(), key, encryptedDll);
    assert(encOk == true);
    // Envelope deve conter 24 bytes de nonce + tag Poly1305 + ciphertext
    assert(encryptedDll.size() > fakeDll.size() + 24);
    // Os bytes cifrados não revelam o cabeçalho MZ/PE
    assert(encryptedDll[24] != 'M' || encryptedDll[25] != 'Z');

    // 4. Decifração em memória legítima
    std::vector<uint8_t> decryptedDll;
    bool decOk = DecryptDllInMemory(encryptedDll.data(), encryptedDll.size(), key, decryptedDll);
    assert(decOk == true);
    assert(decryptedDll.size() == fakeDll.size());
    assert(std::memcmp(decryptedDll.data(), fakeDll.data(), fakeDll.size()) == 0);

    // 5. Tentativa de decifração com chave errada (sessão de outro usuário/máquina)
    ClientSession hackerSession = session;
    hackerSession.fingerprint = "fp_attacker_machine_tampered";
    uint8_t wrongKey[32];
    DeriveDllSessionKey(hackerSession, wrongKey);

    std::vector<uint8_t> failedDec;
    bool decWrongKey = DecryptDllInMemory(encryptedDll.data(), encryptedDll.size(), wrongKey, failedDec);
    assert(decWrongKey == false);
    assert(failedDec.empty()); // Fail-closed absoluto: Poly1305 rejeita a decifração

    // 6. Tentativa de decifração com payload corrompido/adulterado no disco
    std::vector<uint8_t> tamperedEnc = encryptedDll;
    tamperedEnc[50] ^= 0x55; // Altera 1 byte do ciphertext
    bool decTampered = DecryptDllInMemory(tamperedEnc.data(), tamperedEnc.size(), key, failedDec);
    assert(decTampered == false);
    assert(failedDec.empty()); // Fail-closed absoluto

    // 7. Limpeza Segura (SecureWipe)
    SecureWipe(key, sizeof(key));
    for (size_t i = 0; i < sizeof(key); ++i) {
        assert(key[i] == 0);
    }

    std::cout << "[OK] TestDllCipherInReposeAndMemory passou (TweetNaCl authenticated secretbox protege DLL em repouso)" << std::endl;
}

// Teste 5: Ofuscação de Strings (OBF_STR) em Tempo de Compilação
void TestStringObfuscationMacro() {
    // A string original não deve existir de forma contígua e plana no binário
    std::string s1 = OBF_STR("STRUCTURA_KEYSTONE_V1");
    assert(s1 == "STRUCTURA_KEYSTONE_V1");

    std::string s2 = OBF_STR("/compute/validate");
    assert(s2 == "/compute/validate");

    std::string s3 = OBF_STR("ntdll.dll");
    assert(s3 == "ntdll.dll");

    std::cout << "[OK] TestStringObfuscationMacro passou (OBF_STR decifra strings de forma limpa em runtime)" << std::endl;
}

// Teste 6: Fluxo Completo de Carga Segura Decrypt-to-Temp + LoadLibrary + Deleção Imediata
void TestDecryptToTempLoadLibraryAndCleanup() {
    // 1. Cria sessão de teste válida
    ClientSession session;
    session.online = true;
    session.licenseToken = "TEST_TOKEN_M5_CIPHER_DLL_V1";
    session.fingerprint = "TEST_FP_M5_HW_UUID_999";
    session.licenseId = 7788;

    // 2. Lê a DLL real rds.dll do repositório
    std::ifstream ifs("rds.dll", std::ios::binary);
    assert(ifs.is_open());
    std::vector<uint8_t> rawDll((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();
    assert(rawDll.size() > 1024);

    // Deriva chave e cifra a DLL
    uint8_t key[32];
    assert(DeriveDllSessionKey(session, key));
    std::vector<uint8_t> encData;
    assert(EncryptDllBuffer(rawDll.data(), rawDll.size(), key, encData));

    // Salva rds_test.enc no diretório temporário do sistema
    char tempDirA[MAX_PATH];
    GetTempPathA(MAX_PATH, tempDirA);
    std::string validEncPath = std::string(tempDirA) + "rds_test.enc";
    std::ofstream ofs(validEncPath, std::ios::binary);
    assert(ofs.is_open());
    ofs.write(reinterpret_cast<const char*>(encData.data()), encData.size());
    ofs.close();

    // 3. Teste Obrigatório A: Adulterar 1 byte do arquivo cifrado em disco
    std::vector<uint8_t> tamperedData = encData;
    tamperedData[tamperedData.size() - 5] ^= 0x55; // adultera ciphertext / MAC Poly1305
    std::string tamperedEncPath = std::string(tempDirA) + "rds_tampered.enc";
    std::ofstream ofsBad(tamperedEncPath, std::ios::binary);
    assert(ofsBad.is_open());
    ofsBad.write(reinterpret_cast<const char*>(tamperedData.data()), tamperedData.size());
    ofsBad.close();

    // Confirma que a decifração falha (MAC rejeitado) e NENHUM arquivo temporário é criado
    ScopedDecryptedDll resTampered = LoadEncryptedDll(tamperedEncPath, session);
    assert(resTampered.ok == false);
    assert(resTampered.handle == nullptr);
    assert(resTampered.errorMessage == "DECIPHER_FAILED_MAC_MISMATCH");
    assert(resTampered.tempFilePath.empty()); // Nenhum arquivo temporário criado

    // 4. Teste Obrigatório B: Load com sucesso e confirmação de que o arquivo temporário decifrado NÃO PERMANECE no diretório
    ScopedDecryptedDll resClean = LoadEncryptedDll(validEncPath, session);
    if (!resClean.ok) {
        std::cerr << "resClean FAILED: " << resClean.errorMessage << std::endl;
    }
    assert(resClean.ok == true);
    assert(resClean.handle != nullptr);
    assert(!resClean.tempFilePath.empty());

    // Verifica que o arquivo temporário decifrado foi removido do disco logo após o load
    // GetFileAttributes deve retornar INVALID_FILE_ATTRIBUTES indicando que o arquivo não existe mais
#if defined(_WIN32)
    DWORD attr = GetFileAttributesA(resClean.tempFilePath.c_str());
    assert(attr == INVALID_FILE_ATTRIBUTES);
#endif

    // Confirma que a DLL carregada é totalmente funcional: resolve _z8r4q
    using GatekeeperFn = int (*)(std::uint64_t, std::uint64_t, std::uint64_t *, std::uint32_t);
    auto gatekeeper = reinterpret_cast<GatekeeperFn>(resClean.resolve("_z8r4q"));
    assert(gatekeeper != nullptr);

    // Executa handshake com o gatekeeper real da DLL carregada
    uint64_t nonce = 0x1122334455667788ULL;
    uint64_t authTag = solver_sec::computeAuthTag(nonce, solver_sec::kDomainRds);
    std::array<uint64_t, solver_sec::Rds_SlotCount> sealedTable{};
    int rc = gatekeeper(nonce, authTag, sealedTable.data(), solver_sec::Rds_SlotCount);
    assert(rc == 0);

    // Unload e limpeza final
    std::string savedTrash = resClean.trashFilePath;
    resClean.UnloadAndCleanup();
    assert(resClean.handle == nullptr);
#if defined(_WIN32)
    if (!savedTrash.empty()) {
        DWORD attrTrash = GetFileAttributesA(savedTrash.c_str());
        assert(attrTrash == INVALID_FILE_ATTRIBUTES);
    }
#endif

    // Limpeza dos arquivos de teste em disco
    std::remove(validEncPath.c_str());
    std::remove(tamperedEncPath.c_str());

    std::cout << "[OK] TestDecryptToTempLoadLibraryAndCleanup passou (MAC rejeita adulteracao sem temp file; temp file removido apos load)" << std::endl;
}

void TestReleasePathRejectsMissingEncWithoutFallback() {
    std::cout << "[*] Executando TestReleasePathRejectsMissingEncWithoutFallback..." << std::endl;

    ClientSession session;
    session.online = true;
    session.licenseToken = "sample_active_token_jwt";
    session.fingerprint = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    session.licenseId = 100;

    // Confirma que 'rds.dll' e 'mefs.dll' existem em texto claro na raiz
    assert(GetFileAttributesA("rds.dll") != INVALID_FILE_ATTRIBUTES);
    assert(GetFileAttributesA("mefs.dll") != INVALID_FILE_ATTRIBUTES);

    // No modo de Release, STRUCTURA_DEV_BUILD nao esta definido.
    // O caminho real de producao em SolverBridge e ShellSolverBridge chama diretamente LoadEncryptedDll.
    // Simulamos a chamada com o arquivo .enc inexistente (ex: rds.enc ausente):
    const std::string missingRdsEnc = "rds_non_existent_release_path.enc";
    ScopedDecryptedDll resRds = LoadEncryptedDll(missingRdsEnc, session);

    // O sistema DEVE falhar fechado imediatamente, recusando abrir o solver e SEM tentar carregar rds.dll em claro
    assert(resRds.ok == false);
    assert(resRds.handle == nullptr);
    assert(resRds.errorMessage == "ENC_FILE_NOT_FOUND");
    assert(resRds.tempFilePath.empty());

    const std::string missingMefsEnc = "mefs_non_existent_release_path.enc";
    ScopedDecryptedDll resMefs = LoadEncryptedDll(missingMefsEnc, session);
    assert(resMefs.ok == false);
    assert(resMefs.handle == nullptr);
    assert(resMefs.errorMessage == "ENC_FILE_NOT_FOUND");
    assert(resMefs.tempFilePath.empty());

    std::cout << "[OK] TestReleasePathRejectsMissingEncWithoutFallback passou (fail-closed garantido em Release sem fallback para texto claro)" << std::endl;
}

void TestRealRepoEncryptedDllsWithDeliveredKey() {
    std::cout << "[*] Executando TestRealRepoEncryptedDllsWithDeliveredKey..." << std::endl;

    // 1. Confirma que os arquivos reais rds.enc e mefs.enc existem no repositório
    assert(GetFileAttributesA("rds.enc") != INVALID_FILE_ATTRIBUTES);
    assert(GetFileAttributesA("mefs.enc") != INVALID_FILE_ATTRIBUTES);

    // 2. Chave de release da versao 2 (0.9.0) gerada por server/tools/encrypt_dll.py e entregue no login
    const std::string validDllKey = "689e4efd8e345fb49aeeae4ab42d29689d2eba244eedfcebc3900bdf92066d10";
    const std::string oldV1Key = "bf7e76affa7cbad50f5bc33d9a5e9a6c869be411f79289bae1baf202cd1f7c63";
    ClientSession validSession;
    validSession.online = true;
    validSession.dllKey = validDllKey;
    validSession.licenseToken = "active_jwt_token";
    validSession.fingerprint = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

    // Confirma que a chave antiga v1 é rejeitada pelo envelope de autenticacao Poly1305 da versao 2
    ClientSession oldSession = validSession;
    oldSession.dllKey = oldV1Key;
    ScopedDecryptedDll rejectedOld = LoadEncryptedDll("rds.enc", oldSession);
    assert(rejectedOld.ok == false);

    // 3. Testa carga real de rds.enc
    ScopedDecryptedDll rdsModule = LoadEncryptedDll("rds.enc", validSession);
    assert(rdsModule.ok == true);
    assert(rdsModule.handle != nullptr);

    // Confirma que o gatekeeper _z8r4q responde perfeitamente no rds real
    using GatekeeperFn = int (*)(std::uint64_t, std::uint64_t, std::uint64_t *, std::uint32_t);
    auto gatekeeperRds = reinterpret_cast<GatekeeperFn>(rdsModule.resolve("_z8r4q"));
    assert(gatekeeperRds != nullptr);
    uint64_t nonceRds = 0x1122334455667788ULL;
    uint64_t authTagRds = solver_sec::computeAuthTag(nonceRds, solver_sec::kDomainRds);
    std::array<uint64_t, solver_sec::Rds_SlotCount> sealedTableRds{};
    assert(gatekeeperRds(nonceRds, authTagRds, sealedTableRds.data(), solver_sec::Rds_SlotCount) == 0);
    rdsModule.UnloadAndCleanup();

    // 4. Testa carga real de mefs.enc
    ScopedDecryptedDll mefsModule = LoadEncryptedDll("mefs.enc", validSession);
    assert(mefsModule.ok == true);
    assert(mefsModule.handle != nullptr);
    auto gatekeeperMefs = reinterpret_cast<GatekeeperFn>(mefsModule.resolve("_z8m2p"));
    assert(gatekeeperMefs != nullptr);
    uint64_t nonceMefs = 0x9988776655443322ULL;
    uint64_t authTagMefs = solver_sec::computeAuthTag(nonceMefs, solver_sec::kDomainMefs);
    std::array<uint64_t, solver_sec::Mefs_SlotCount> sealedTableMefs{};
    assert(gatekeeperMefs(nonceMefs, authTagMefs, sealedTableMefs.data(), solver_sec::Mefs_SlotCount) == 0);
    mefsModule.UnloadAndCleanup();

    // 5. Teste de chave errada / rotacionada: chave de outra versao DEVE falhar com MAC mismatch
    ClientSession badSession = validSession;
    badSession.dllKey = "1111111111111111111111111111111111111111111111111111111111111111"; // Chave de versao diferente
    ScopedDecryptedDll badModule = LoadEncryptedDll("rds.enc", badSession);
    assert(badModule.ok == false);
    assert(badModule.handle == nullptr);
    assert(badModule.errorMessage == "DECIPHER_FAILED_MAC_MISMATCH");
    assert(badModule.tempFilePath.empty());

    std::cout << "[OK] TestRealRepoEncryptedDllsWithDeliveredKey passou (rds.enc e mefs.enc decifrados e validados com chave de sessao/versao real)" << std::endl;
}

// Stub de SolverCore::run para teste autônomo sem matriz FEM
bool SolverCore::run(const SolverInput&, SolverOutput&, std::string&) const {
    return true;
}

int main() {
    std::cout << "--- Executando testes C++ do Marco M5 (Anti-Debug, Tampering, Ofuscacao e Cifra DLL) ---" << std::endl;
    TestSoftwareBreakpointDetection();
    TestInlineHookDetection();
    TestPoisonKeystoneSecretAndPhysicsDegradation();
    TestDllCipherInReposeAndMemory();
    TestStringObfuscationMacro();
    TestDecryptToTempLoadLibraryAndCleanup();
    TestReleasePathRejectsMissingEncWithoutFallback();
    TestRealRepoEncryptedDllsWithDeliveredKey();
    std::cout << "--- Todos os testes do M5 passaram com sucesso! ---" << std::endl;
    return 0;
}
