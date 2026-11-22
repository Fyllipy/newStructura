#include "license_config.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#endif

#include "json_helper.h"

namespace structura_license {

namespace {

// Chave Pública Ed25519 de Release (assina os manifestos de integridade v2)
// Derivada da chave publica de release do servidor (b64: jLiZDIlCKNI6uGt-eqgbqre37huCF0bQh3pFiHsLKpM)
const uint8_t kReleasePubKey[32] = {
    0x8c, 0xb8, 0x99, 0x0c, 0x89, 0x42, 0x28, 0xd2,
    0x3a, 0xb8, 0x6b, 0x7e, 0x7a, 0xa8, 0x1b, 0xaa,
    0xb7, 0xb7, 0xee, 0x1b, 0x82, 0x17, 0x46, 0xd0,
    0x87, 0x7a, 0x45, 0x88, 0x7b, 0x0b, 0x2a, 0x93
};

// Chave Pública Ed25519 do Servidor (assina tokens, Keystone K e pass_sig Nível B).
// Build de DESENVOLVIMENTO (STRUCTURA_DEV_BUILD): chave do servidor local. Build de RELEASE: chave do
// servidor de PRODUCAO (server/deploy/gen_prod_env.py -> secrets/server_pubkey.hex).
#if defined(STRUCTURA_DEV_BUILD)
const uint8_t kServerPubKey[32] = {
    0xcb, 0xb3, 0x48, 0x5a, 0x9c, 0x25, 0x01, 0xc0,
    0xf5, 0x84, 0x97, 0x9e, 0xa3, 0xac, 0xef, 0x0c,
    0x75, 0x87, 0x25, 0x7a, 0x2f, 0x04, 0x0f, 0x9f,
    0xb0, 0x94, 0x24, 0x0f, 0x2a, 0xc1, 0xfc, 0x76
};
#else
const uint8_t kServerPubKey[32] = {
    0xab, 0xb8, 0xed, 0x41, 0xcd, 0xbb, 0xac, 0xb7,
    0x4f, 0x39, 0xa8, 0xeb, 0xfd, 0x00, 0x6d, 0x3f,
    0x99, 0x3b, 0xff, 0x1d, 0xd3, 0x41, 0x11, 0xe1,
    0x1e, 0x76, 0x02, 0x87, 0xef, 0xb0, 0xa5, 0xa5
};
#endif

#if defined(_WIN32)
std::filesystem::path GetSessionPathFs() {
    const wchar_t* overridePath = _wgetenv(L"STRUCTURA_SESSION_FILE");
    if (overridePath && std::wcslen(overridePath) > 0) {
        return std::filesystem::path(overridePath);
    }
    wchar_t appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData))) {
        std::filesystem::path dir = std::filesystem::path(appData) / L"Structura";
        CreateDirectoryW(dir.c_str(), NULL);
        std::filesystem::path edDir = dir / L"Structura-Metalicas";
        CreateDirectoryW(edDir.c_str(), NULL);
        return edDir / L"session.dat";
    }
    const wchar_t* home = _wgetenv(L"APPDATA");
    if (!home) home = _wgetenv(L"USERPROFILE");
    if (home) {
        return std::filesystem::path(home) / L"session.dat";
    }
    return std::filesystem::path(L"session.dat");
}
#endif

#if defined(_WIN32)
// Sessao em repouso protegida por DPAPI (escopo do usuario Windows): um arquivo copiado para
// outra conta/maquina e inutil, e o token nao fica legivel por outros processos/usuarios.
constexpr char kSessionMagic[4] = {'S', 'D', 'P', '1'};

bool DpapiProtect(const std::string& plain, std::string& out) {
    DATA_BLOB in{static_cast<DWORD>(plain.size()), reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()))};
    DATA_BLOB enc{};
    if (!CryptProtectData(&in, L"Structura session", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &enc))
        return false;
    out.assign(kSessionMagic, sizeof(kSessionMagic));
    out.append(reinterpret_cast<const char*>(enc.pbData), enc.cbData);
    LocalFree(enc.pbData);
    return true;
}

bool DpapiUnprotect(const std::string& blob, std::string& out) {
    if (blob.size() <= sizeof(kSessionMagic)) return false;
    DATA_BLOB in{static_cast<DWORD>(blob.size() - sizeof(kSessionMagic)),
                 reinterpret_cast<BYTE*>(const_cast<char*>(blob.data())) + sizeof(kSessionMagic)};
    DATA_BLOB dec{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &dec))
        return false;
    out.assign(reinterpret_cast<const char*>(dec.pbData), dec.cbData);
    SecureZeroMemory(dec.pbData, dec.cbData);
    LocalFree(dec.pbData);
    return true;
}
#endif

} // namespace

std::string GetLicenseServerUrl() {
    // URL de producao (HTTPS). Vazia enquanto o servidor publico nao existe: ai vale o padrao de dev.
    #if defined(STRUCTURA_PRODUCTION_SERVER_URL)
    static const char kProductionUrl[] = STRUCTURA_PRODUCTION_SERVER_URL;
#else
    static const char kProductionUrl[] = "";
#endif
    const char* envUrl = std::getenv("STRUCTURA_LICENSE_SERVER");
    if (envUrl && std::strlen(envUrl) > 0) {
        const std::string u(envUrl);
#if defined(STRUCTURA_DEV_BUILD)
        return u;                                   // dev: qualquer endereco
#else
        // Release: o override so vale para HTTPS (impede redirecionar o app para http://servidor-falso).
        if (u.rfind("https://", 0) == 0) return u;
#endif
    }
    if (kProductionUrl[0] != 0) return std::string(kProductionUrl);
    return "http://127.0.0.1:8000";
}

// Pins SHA-256 (hex minusculo) do SubjectPublicKeyInfo do certificado do servidor de licencas.
// Lista vazia = pinning desativado. Preencher no deploy (inclua 2: atual + backup/rotacao).
const std::vector<std::string>& GetServerCertPins() {
    static const std::vector<std::string> kPins = {};
    return kPins;
}

const uint8_t* GetReleasePublicKey() {
    return kReleasePubKey;
}

const uint8_t* GetServerPublicKey() {
    return kServerPubKey;
}

std::string GetAppInstallDir() {
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::filesystem::path p(buffer);
        std::wstring ws = p.parent_path().wstring();
        int n = WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
        if (n > 0) {
            std::string u8(n, 0);
            WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), &u8[0], n, nullptr, nullptr);
            return u8;
        }
        return p.parent_path().string();
    }
#endif
    return ".";
}

std::string GetSessionFilePath() {
#if defined(_WIN32)
    return GetSessionPathFs().string();
#else
    const char* home = std::getenv("APPDATA");
    if (!home) home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/session.dat";
    }
    return "session.dat";
#endif
}

bool SaveSessionToDisk(const ClientSession& session) {
    if (!session.online || session.licenseToken.empty()) {
        ClearSessionOnDisk();
        return false;
    }

#if defined(_WIN32)
    std::filesystem::path path = GetSessionPathFs();
#else
    std::filesystem::path path = GetSessionFilePath();
#endif

    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }

    std::string json = "{"
        "\"license_token\":\"" + EscapeJson(session.licenseToken) + "\","
        "\"expires_at\":\"" + EscapeJson(session.expiresAt) + "\","
        "\"fingerprint\":\"" + EscapeJson(session.fingerprint) + "\","
        "\"dll_key\":\"" + EscapeJson(session.dllKey) + "\","
        "\"vm_allow_token\":\"" + EscapeJson(session.vmAllowToken) + "\","
        "\"watermark_token\":\"" + EscapeJson(session.watermarkToken) + "\","
        "\"license_id\":" + std::to_string(session.licenseId) + ","
        "\"user_id\":" + std::to_string(session.userId) + ","
        "\"online\":true"
    "}";

#if defined(_WIN32)
    std::string protectedBlob;
    if (!DpapiProtect(json, protectedBlob)) {
        return false;  // fail-closed: nunca grava o token em claro se a protecao falhar
    }
    ofs.write(protectedBlob.data(), static_cast<std::streamsize>(protectedBlob.size()));
#else
    ofs.write(json.data(), json.size());
#endif
    return true;
}

bool LoadSessionFromDisk(ClientSession& outSession) {
    outSession = ClientSession{};
#if defined(_WIN32)
    std::filesystem::path path = GetSessionPathFs();
#else
    std::filesystem::path path = GetSessionFilePath();
#endif

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        return false;
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string content = ss.str();
    if (content.empty()) {
        return false;
    }
#if defined(_WIN32)
    if (content.size() > sizeof(kSessionMagic) &&
        std::memcmp(content.data(), kSessionMagic, sizeof(kSessionMagic)) == 0) {
        std::string plain;
        if (!DpapiUnprotect(content, plain)) {
            return false;  // arquivo de outro usuario/maquina ou adulterado
        }
        content.swap(plain);
    }
    // Sem o magic = formato legado em texto claro: aceito UMA vez; o proximo SaveSessionToDisk regrava protegido.
#endif

    outSession.licenseToken = JsonGetString(content, "license_token");
    outSession.expiresAt = JsonGetString(content, "expires_at");
    outSession.fingerprint = JsonGetString(content, "fingerprint");
    outSession.dllKey = JsonGetString(content, "dll_key");
    outSession.vmAllowToken = JsonGetString(content, "vm_allow_token");
    outSession.watermarkToken = JsonGetString(content, "watermark_token");
    outSession.licenseId = JsonGetInt(content, "license_id");
    outSession.userId = JsonGetInt(content, "user_id");
    outSession.online = JsonGetBool(content, "online");

    return (!outSession.licenseToken.empty() && outSession.online);
}

void ClearSessionOnDisk() {
#if defined(_WIN32)
    std::filesystem::path path = GetSessionPathFs();
    DeleteFileW(path.c_str());
#else
    std::string path = GetSessionFilePath();
    std::remove(path.c_str());
#endif
}

} // namespace structura_license
