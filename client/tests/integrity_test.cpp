// Utilitario de teste do cliente de integridade (NAO vai no produto).
//   integrity_test selftest            -> vetores SHA-256
//   integrity_test parse <manifest>    -> OK|BAD (parser estrito)
//   integrity_test verify <dir>        -> status + atestacao (verificador de assinatura FALSO = sempre true;
//                                         a assinatura e testada em Python/pytest e em producao via libsodium)
//   integrity_test failclosed <dir>    -> verificador que recusa => deve dar ManifestBadSig
//   integrity_test nullverify <dir>    -> sem verificador => deve falhar fechado
#include "../integrity_client.h"
#include "../sha256.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace structura_license;

static const char* Name(IntegrityStatus s) {
    switch (s) {
        case IntegrityStatus::Ok: return "Ok";
        case IntegrityStatus::ManifestMissing: return "ManifestMissing";
        case IntegrityStatus::ManifestBadSig: return "ManifestBadSig";
        case IntegrityStatus::ManifestMalformed: return "ManifestMalformed";
        case IntegrityStatus::ScanError: return "ScanError";
        case IntegrityStatus::FileModified: return "FileModified";
        case IntegrityStatus::FileMissing: return "FileMissing";
        case IntegrityStatus::FileExtra: return "FileExtra";
        case IntegrityStatus::DirCountMismatch: return "DirCountMismatch";
    }
    return "?";
}

static bool AlwaysTrue(const uint8_t*, size_t, const uint8_t*, const uint8_t*) { return true; }
static bool AlwaysFalse(const uint8_t*, size_t, const uint8_t*, const uint8_t*) { return false; }

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    std::string cmd = argv[1];
    if (cmd == "selftest") {
        bool ok = Sha256::HashHex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
        ok = ok && Sha256::HashHex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
        ok = ok && Sha256::HashHex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
                       "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1";
        std::string million(1000000, 'a');
        ok = ok && Sha256::HashHex(million) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";
        std::puts(ok ? "OK" : "BAD");
        return ok ? 0 : 1;
    }
    if (argc < 3) return 2;
    if (cmd == "parse") {
        std::ifstream f(argv[2], std::ios::binary);
        std::stringstream ss; ss << f.rdbuf();
        Manifest m;
        bool ok = ParseManifest(ss.str(), m);
        std::puts(ok ? "OK" : "BAD");
        return ok ? 0 : 1;
    }
    uint8_t pub[32] = {0};
    Attestation att;
    Ed25519VerifyFn fn = AlwaysTrue;
    if (cmd == "failclosed") fn = AlwaysFalse;
    if (cmd == "nullverify") fn = nullptr;

    std::string targetDir = argv[2];
#ifdef _WIN32
    int argc_w;
    LPWSTR* argv_w = CommandLineToArgvW(GetCommandLineW(), &argc_w);
    if (argv_w && argc_w >= 3) {
        std::wstring ws = argv_w[2];
        int n = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
        if (n > 0) {
            targetDir.assign(n, 0);
            WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), &targetDir[0], n, nullptr, nullptr);
        }
        LocalFree(argv_w);
    }
#endif

    IntegrityStatus st = VerifyInstall(targetDir, pub, fn, att);
    std::printf("status=%s\nversion=%d\nroot=%s\ncount=%d\nexe=%s\n", Name(st), att.manifestVersion,
                att.filesRoot.c_str(), att.fileCount, att.exeHash.c_str());
    return 0;
}
