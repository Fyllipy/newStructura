#include "dll_cipher.h"
#include "license_client.h"
extern "C" {
#include "tweetnacl.h"
}
#include "sha256.h"
#include "../Structuradll/src/core/solver_security.h"

#include <cstring>
#include <filesystem>
#include <fstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace structura_license {

void SecureWipe(void* ptr, size_t len) noexcept {
    if (ptr == nullptr || len == 0) {
        return;
    }
    volatile unsigned char* vptr = static_cast<volatile unsigned char*>(ptr);
    while (len--) {
        *vptr++ = 0;
    }
}

bool DeriveDllSessionKey(const ClientSession& session, uint8_t outKey[32]) {
    std::string keyHex = session.dllKey;
    if (!session.online) {
        const char* testEnvKey = std::getenv("STRUCTURA_TEST_DLL_KEY");
        if (testEnvKey && std::strlen(testEnvKey) == 64) {
            keyHex = testEnvKey;
        } else {
            return false;
        }
    }

    // Caso 1: DLL_RELEASE_KEY entregue autoritativamente pelo servidor no login (M5) ou ambiente de testes
    if (!keyHex.empty()) {
        if (keyHex.size() != 64) {
            return false;
        }
        for (size_t i = 0; i < 32; ++i) {
            char high = keyHex[2 * i];
            char low = keyHex[2 * i + 1];
            auto fromHex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int h = fromHex(high);
            int l = fromHex(low);
            if (h < 0 || l < 0) return false;
            outKey[i] = static_cast<uint8_t>((h << 4) | l);
        }

        // Injeção de anti-debug poison: se houver debugger ativo ou breakpoints, corrompe a chave
        uint64_t poison = CheckExeAntiDebugPoison();
        if (poison != 0) {
            uint64_t* p = reinterpret_cast<uint64_t*>(outKey);
            *p ^= poison;
        }
        return true;
    }

    // Caso 2: Sessões locais sintetizadas em testes unitários sem servidor HTTP completo
    if (session.licenseToken.empty() || session.fingerprint.empty()) {
        return false;
    }

    Sha256 h;
    h.Update(session.licenseToken.data(), session.licenseToken.size());
    h.Update(session.fingerprint.data(), session.fingerprint.size());
    std::string licIdStr = std::to_string(session.licenseId);
    h.Update(licIdStr.data(), licIdStr.size());
    const std::string dom = OBF_STR("STRUCTURA_DLL_CIPHER_V1");
    h.Update(dom.data(), dom.size());
    h.Final(outKey);

    uint64_t poison = CheckExeAntiDebugPoison();
    if (poison != 0) {
        uint64_t* p = reinterpret_cast<uint64_t*>(outKey);
        *p ^= poison;
    }
    return true;
}

bool EncryptDllBuffer(const uint8_t* rawData, size_t rawLen,
                      const uint8_t key[32],
                      std::vector<uint8_t>& outEncrypted) {
    outEncrypted.clear();
    if (rawData == nullptr || rawLen == 0 || rawLen > 256 * 1024 * 1024 || key == nullptr) {
        return false;
    }

    // Gera nonce pseudo-aleatório de 24 bytes com alta entropia
    uint8_t nonce[24];
    for (size_t i = 0; i < 24; i += 8) {
        uint64_t nVal = solver_sec::generateNonce(rawData + i);
        std::memcpy(nonce + i, &nVal, 8);
    }

    // Buffer de mensagem formatado com 32 zeros à frente para o TweetNaCl
    const size_t zeroBytes = 32;
    const size_t boxZeroBytes = 16;
    const size_t paddedLen = rawLen + zeroBytes;
    std::vector<uint8_t> m(paddedLen, 0);
    std::memcpy(m.data() + zeroBytes, rawData, rawLen);

    std::vector<uint8_t> c(paddedLen, 0);
    int rc = crypto_secretbox(c.data(), m.data(), static_cast<unsigned long long>(paddedLen), nonce, key);
    SecureWipe(m.data(), m.size());

    if (rc != 0) {
        SecureWipe(c.data(), c.size());
        return false;
    }

    // Formato do envelope cifrado:
    // [0..23]: Nonce (24 bytes)
    // [24..]: Ciphertext autenticado (a partir do offset BOXZEROBYTES = 16 de c)
    const size_t ciphertextLen = paddedLen - boxZeroBytes;
    outEncrypted.resize(24 + ciphertextLen);
    std::memcpy(outEncrypted.data(), nonce, 24);
    std::memcpy(outEncrypted.data() + 24, c.data() + boxZeroBytes, ciphertextLen);

    SecureWipe(c.data(), c.size());
    SecureWipe(nonce, sizeof(nonce));
    return true;
}

bool DecryptDllInMemory(const uint8_t* encData, size_t encLen,
                        const uint8_t key[32],
                        std::vector<uint8_t>& outDecrypted) {
    outDecrypted.clear();
    const size_t zeroBytes = 32;
    const size_t boxZeroBytes = 16;
    // Tamanho mínimo: 24 (nonce) + 16 (tag Poly1305) + pelo menos 1 byte
    if (encData == nullptr || encLen <= (24 + boxZeroBytes) || encLen > 300 * 1024 * 1024 || key == nullptr) {
        return false;
    }

    const uint8_t* nonce = encData;
    const uint8_t* cipherPayload = encData + 24;
    const size_t cipherPayloadLen = encLen - 24;

    const size_t paddedLen = cipherPayloadLen + boxZeroBytes;
    std::vector<uint8_t> c(paddedLen, 0);
    // Preenche 16 zeros à frente para TweetNaCl e copia o ciphertext
    std::memcpy(c.data() + boxZeroBytes, cipherPayload, cipherPayloadLen);

    std::vector<uint8_t> m(paddedLen, 0);
    int rc = crypto_secretbox_open(m.data(), c.data(), static_cast<unsigned long long>(paddedLen), nonce, key);
    SecureWipe(c.data(), c.size());

    if (rc != 0) {
        SecureWipe(m.data(), m.size());
        return false; // Falha fechada: chave errada ou payload adulterado
    }

    // Plaintext extraído após o offset ZEROBYTES (32)
    const size_t plainLen = paddedLen - zeroBytes;
    outDecrypted.resize(plainLen);
    std::memcpy(outDecrypted.data(), m.data() + zeroBytes, plainLen);

    SecureWipe(m.data(), m.size());
    return true;
}

bool DecryptDllFileInMemory(const std::string& encFilePath,
                            const ClientSession& session,
                            std::vector<uint8_t>& outDecryptedDll) {
    outDecryptedDll.clear();
    const char* testEnvKey = std::getenv("STRUCTURA_TEST_DLL_KEY");
    if (!session.online && (!testEnvKey || std::strlen(testEnvKey) != 64)) {
        return false;
    }

    std::filesystem::path fsPath = std::filesystem::u8path(encFilePath);
    std::ifstream file(fsPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    std::streamsize size = file.tellg();
    if (size <= 0 || size > 256 * 1024 * 1024) { // limite de sanidade 256MB
        return false;
    }
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> encBuffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(encBuffer.data()), size)) {
        return false;
    }

    uint8_t key[32];
    if (!DeriveDllSessionKey(session, key)) {
        SecureWipe(encBuffer.data(), encBuffer.size());
        return false;
    }

    bool ok = DecryptDllInMemory(encBuffer.data(), encBuffer.size(), key, outDecryptedDll);
    SecureWipe(key, sizeof(key));
    SecureWipe(encBuffer.data(), encBuffer.size());
    return ok;
}

// --- Métodos de ScopedDecryptedDll ---

ScopedDecryptedDll::~ScopedDecryptedDll() {
    UnloadAndCleanup();
}

ScopedDecryptedDll::ScopedDecryptedDll(ScopedDecryptedDll&& other) noexcept
    : handle(other.handle),
      tempFilePath(std::move(other.tempFilePath)),
      trashFilePath(std::move(other.trashFilePath)),
      errorMessage(std::move(other.errorMessage)),
      ok(other.ok) {
    other.handle = nullptr;
    other.ok = false;
}

ScopedDecryptedDll& ScopedDecryptedDll::operator=(ScopedDecryptedDll&& other) noexcept {
    if (this != &other) {
        UnloadAndCleanup();
        handle = other.handle;
        tempFilePath = std::move(other.tempFilePath);
        trashFilePath = std::move(other.trashFilePath);
        errorMessage = std::move(other.errorMessage);
        ok = other.ok;
        other.handle = nullptr;
        other.ok = false;
    }
    return *this;
}

void* ScopedDecryptedDll::resolve(const char* symbol) const {
    if (!handle || !symbol) {
        return nullptr;
    }
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), symbol));
#else
    return nullptr;
#endif
}

void ScopedDecryptedDll::UnloadAndCleanup() {
#if defined(_WIN32)
    if (handle) {
        FreeLibrary(static_cast<HMODULE>(handle));
        handle = nullptr;
    }
    if (!trashFilePath.empty()) {
        DeleteFileA(trashFilePath.c_str());
        trashFilePath.clear();
    }
    if (!tempFilePath.empty()) {
        DeleteFileA(tempFilePath.c_str());
        tempFilePath.clear();
    }
#endif
    ok = false;
}

// --- Carga Segura: decrypt-to-temp + LoadLibrary ---

ScopedDecryptedDll LoadEncryptedDll(const std::string& encFilePath,
                                   const ClientSession& session) {
    ScopedDecryptedDll res;

    // 1. Validar sessão ativa e integridade das credenciais
    uint8_t key[32] = {0};
    if (!DeriveDllSessionKey(session, key)) {
        res.ok = false;
        res.errorMessage = "INVALID_SESSION";
        return res; // Fail-closed: aborta sem gravar nada em disco e sem tentar LoadLibrary
    }

    // 2. Verificar existência do arquivo cifrado em disco
    {
        std::filesystem::path fsPath = std::filesystem::u8path(encFilePath);
        std::ifstream checkFile(fsPath, std::ios::binary);
        if (!checkFile.is_open()) {
            SecureWipe(key, sizeof(key));
            res.ok = false;
            res.errorMessage = "ENC_FILE_NOT_FOUND";
            return res;
        }
    }

    // 3. Decifrar em RAM com validação estrita de MAC (XSalsa20-Poly1305)
    std::vector<uint8_t> decBuffer;
    bool decOk = DecryptDllFileInMemory(encFilePath, session, decBuffer);
    SecureWipe(key, sizeof(key));

    if (!decOk || decBuffer.empty()) {
        SecureWipe(decBuffer.data(), decBuffer.size());
        decBuffer.clear();
        res.ok = false;
        res.errorMessage = "DECIPHER_FAILED_MAC_MISMATCH";
        return res; // Fail-closed: MAC adulterado ou chave inválida -> nada gravado em disco
    }

#if defined(_WIN32)
    // 3. Obter diretório temporário do usuário
    wchar_t tempDir[MAX_PATH] = {0};
    DWORD dirLen = GetTempPathW(MAX_PATH, tempDir);
    if (dirLen == 0 || dirLen >= MAX_PATH) {
        SecureWipe(decBuffer.data(), decBuffer.size());
        res.ok = false;
        res.errorMessage = "TEMP_PATH_FAILED";
        return res;
    }

    // 4. Gerar nome de arquivo temporário imprevisível derivado de dois nonces criptográficos
    uint64_t n1 = solver_sec::generateNonce(&session);
    uint64_t n2 = solver_sec::mix64(n1 ^ 0xBAADF00DCAFEBABEULL);
    wchar_t tempFile[MAX_PATH] = {0};
    _snwprintf(tempFile, MAX_PATH, L"%sst_%016llx_%016llx.dll", tempDir,
               static_cast<unsigned long long>(n1),
               static_cast<unsigned long long>(n2));

    // Grava o buffer decifrado no arquivo temporário
    HANDLE hFile = CreateFileW(tempFile, GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD createErr = GetLastError();
        SecureWipe(decBuffer.data(), decBuffer.size());
        res.ok = false;
        res.errorMessage = "TEMP_FILE_CREATE_FAILED_" + std::to_string(createErr);
        return res;
    }

    DWORD written = 0;
    BOOL writeOk = WriteFile(hFile, decBuffer.data(), static_cast<DWORD>(decBuffer.size()), &written, NULL);
    CloseHandle(hFile);

    // Imediatamente executa SecureWipe no buffer decifrado em RAM
    SecureWipe(decBuffer.data(), decBuffer.size());
    decBuffer.clear();

    if (!writeOk || written == 0) {
        DeleteFileW(tempFile);
        res.ok = false;
        res.errorMessage = "TEMP_FILE_WRITE_FAILED";
        return res;
    }

    // Guarda caminho temporário em UTF-8
    char tempPathA[MAX_PATH * 2] = {0};
    WideCharToMultiByte(CP_UTF8, 0, tempFile, -1, tempPathA, sizeof(tempPathA), NULL, NULL);
    res.tempFilePath = tempPathA;

    // 5. LoadLibrary no arquivo temporário
    HMODULE hMod = LoadLibraryW(tempFile);

    // 6. Imediatamente após o load (sucesso ou falha):
    if (hMod == nullptr) {
        DWORD err = GetLastError();
        DeleteFileW(tempFile); // Garante remoção mesmo no caminho de erro — sem órfãos!
        res.ok = false;
        res.errorMessage = "LOAD_LIBRARY_FAILED_" + std::to_string(err);
        return res;
    }

    // Sucesso: DLL carregada em memória com HMODULE válido
    res.ok = true;
    res.handle = static_cast<void*>(hMod);

    // Apague o arquivo temporário imediatamente após o load:
    if (!DeleteFileW(tempFile)) {
        // No Windows NT, LoadLibrary mantém o mapeamento de seção enquanto ativo.
        // Movemos para um nome transitório oculto .trash e agendamos deleção garantida:
        std::wstring trashFile = std::wstring(tempFile) + L".trash";
        if (MoveFileExW(tempFile, trashFile.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            char trashPathA[MAX_PATH * 2] = {0};
            WideCharToMultiByte(CP_UTF8, 0, trashFile.c_str(), -1, trashPathA, sizeof(trashPathA), NULL, NULL);
            res.trashFilePath = trashPathA;
            MoveFileExW(trashFile.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
            DeleteFileW(trashFile.c_str());
        }
    }
    // O arquivo temporário decifrado 'tempFile' não existe mais no diretório temporário!
#else
    res.ok = false;
    res.errorMessage = "UNSUPPORTED_PLATFORM";
#endif

    return res;
}

#if defined(STRUCTURA_DEV_BUILD)
ScopedDecryptedDll LoadEncryptedDllWithFallback(const std::string& encFilePath,
                                               const std::string& fallbackPlainDllPath,
                                               const ClientSession& session) {
    // Se o arquivo cifrado existe em disco, prioriza carga cifrada com autenticação
    std::ifstream checkEnc(encFilePath, std::ios::binary);
    if (checkEnc.good()) {
        checkEnc.close();
        return LoadEncryptedDll(encFilePath, session);
    }
    checkEnc.close();

    // Caso contrário, carrega DLL em texto claro (modo fallback de desenvolvimento)
    ScopedDecryptedDll res;
#if defined(_WIN32)
    HMODULE hMod = LoadLibraryA(fallbackPlainDllPath.c_str());
    if (hMod != nullptr) {
        res.ok = true;
        res.handle = static_cast<void*>(hMod);
    } else {
        res.ok = false;
        res.errorMessage = "FALLBACK_LOAD_FAILED_" + std::to_string(GetLastError());
    }
#else
    res.ok = false;
    res.errorMessage = "UNSUPPORTED_PLATFORM";
#endif
    return res;
}
#endif


} // namespace structura_license
