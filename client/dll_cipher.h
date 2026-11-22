#pragma once
// Proteção de DLLs Cifradas em Repouso e Decifração Segura em Memória (Marco M5).
// Base: SPEC.md (§9), PLAN.md (M5).
//
// Primitiva Criptográfica: Cifra Autenticada XSalsa20-Poly1305 via TweetNaCl (crypto_secretbox).
// Fornece confidencialidade e integridade criptográfica com chave de 256 bits e nonce de 192 bits.
//
// Regras:
// 1. Em repouso no disco, a DLL é armazenada cifrada.
// 2. A decifração ocorre estritamente em memória, vinculada à ClientSession ativa.
// 3. Sem login/sessão válida, a chave não pode ser derivada e a DLL permanece indecifrável (fail-closed).
// 4. Buffers transitórios são limpos com limpeza segura (SecureWipe).

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "license_client.h"

namespace structura_license {

// Limpeza segura de memória volátil (impede otimização de eliminação de escrita pelo compilador)
void SecureWipe(void* ptr, size_t len) noexcept;

// Deriva a chave simétrica de 256 bits da DLL a partir da sessão ativa do usuário
bool DeriveDllSessionKey(const ClientSession& session, uint8_t outKey[32]);

// Cifra um buffer de DLL bruta gerando o envelope criptográfico (nonce 24B + ciphertext autenticado)
bool EncryptDllBuffer(const uint8_t* rawData, size_t rawLen,
                      const uint8_t key[32],
                      std::vector<uint8_t>& outEncrypted);

// Decifra o envelope criptográfico diretamente em memória RAM (retorna false se adulterado ou chave incorreta)
bool DecryptDllInMemory(const uint8_t* encData, size_t encLen,
                        const uint8_t key[32],
                        std::vector<uint8_t>& outDecrypted);

// Lê arquivo cifrado do disco e decifra diretamente para memória utilizando a sessão ativa
bool DecryptDllFileInMemory(const std::string& encFilePath,
                            const ClientSession& session,
                            std::vector<uint8_t>& outDecryptedDll);

// --- Carga Segura Decrypt-to-Temp + LoadLibrary (Marco M5) ---
// Decifra o binário em RAM, grava em arquivo temporário com nome imprevisível (nonce/GUID)
// em GetTempPath(), executa LoadLibrary, e apaga imediatamente o arquivo e o buffer em memória.
struct ScopedDecryptedDll {
    void* handle = nullptr;
    std::string tempFilePath;
    std::string trashFilePath;
    std::string errorMessage;
    bool ok = false;

    ScopedDecryptedDll() = default;
    ~ScopedDecryptedDll();

    ScopedDecryptedDll(const ScopedDecryptedDll&) = delete;
    ScopedDecryptedDll& operator=(const ScopedDecryptedDll&) = delete;

    ScopedDecryptedDll(ScopedDecryptedDll&& other) noexcept;
    ScopedDecryptedDll& operator=(ScopedDecryptedDll&& other) noexcept;

    void* resolve(const char* symbol) const;
    void UnloadAndCleanup();
};

// Carga estrita de DLL cifrada em repouso (obrigatoria em builds de producao/release)
ScopedDecryptedDll LoadEncryptedDll(const std::string& encFilePath,
                                   const ClientSession& session);

#if defined(STRUCTURA_DEV_BUILD)
// Carga com fallback para desenvolvimento local (prioriza DLL cifrada; aceita texto claro APENAS em dev)
ScopedDecryptedDll LoadEncryptedDllWithFallback(const std::string& encFilePath,
                                               const std::string& fallbackPlainDllPath,
                                               const ClientSession& session);
#endif


} // namespace structura_license
