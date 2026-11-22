#pragma once
// Integridade local do Structura (PLAN M2). Replica EXATAMENTE o formato de
// server/app/manifest.py (ver cabecalho la): manifesto TEXTO assinado (Ed25519, chave de RELEASE
// publica embutida), raiz SHA-256 sobre linhas F/D, contagem de arquivos por pasta.
//
// Regras: arquivo a mais, a menos, alterado, diretorio extra, symlink/junction => FALHA.
// O cliente NAO decide nada sozinho: o resultado vira a "atestacao" enviada ao servidor, que
// compara com o manifesto assinado (atestacao server-side). Fail-closed em qualquer erro.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace structura_license {

struct Manifest {
    int version = 0;
    std::string exe;                       // caminho relativo do EXE
    std::vector<std::string> exclude;      // diretorios fora do escopo
    std::map<std::string, std::string> files;  // relpath -> sha256 hex  (ordem = bytes UTF-8)
    std::map<std::string, int> dirs;           // relpath -> n arquivos diretos ("." = raiz)
};

struct Scan {
    std::map<std::string, std::string> files;
    std::map<std::string, int> dirs;
};

// Atestacao enviada ao servidor (mesmos campos de POST /auth/login -> integrity).
struct Attestation {
    int manifestVersion = 0;
    std::string filesRoot;
    int fileCount = 0;
    std::string exeHash;
};

enum class IntegrityStatus {
    Ok = 0,
    ManifestMissing,    // manifest.txt/.sig ausente
    ManifestBadSig,     // assinatura invalida (adulterado ou chave errada)
    ManifestMalformed,  // nao canonico / duplicatas / caminho perigoso
    ScanError,          // erro de leitura, symlink/junction, arquivo especial
    FileModified,
    FileMissing,
    FileExtra,          // arquivo ou diretorio a mais
    DirCountMismatch,
};

// Verificador Ed25519 injetado (implementacao real: libsodium crypto_sign_verify_detached;
// ver ed25519_sodium.cpp). Retorna true so se a assinatura for valida.
using Ed25519VerifyFn = bool (*)(const uint8_t* msg, size_t msgLen,
                                 const uint8_t sig[64], const uint8_t pub[32]);

// Pure helpers (testaveis).
bool ParseManifest(const std::string& text, Manifest& out);     // parser estrito + forma canonica
std::string RenderManifest(const Manifest& m);
std::string ComputeRoot(const std::map<std::string, std::string>& files,
                        const std::map<std::string, int>& dirs);
bool Base64UrlDecode(const std::string& in, std::vector<uint8_t>& out);

// Disco.
bool ScanDirectory(const std::string& installDirUtf8, const std::vector<std::string>& exclude, Scan& out);

// Carrega manifest.txt + manifest.sig da pasta, VERIFICA a assinatura (antes de parsear) e parseia.
IntegrityStatus LoadManifest(const std::string& installDirUtf8, const uint8_t releasePubKey[32],
                             Ed25519VerifyFn verify, Manifest& out);

// Compara varredura x manifesto (diagnostico LOCAL; ao servidor vai so a atestacao).
IntegrityStatus CompareScan(const Manifest& m, const Scan& s);

Attestation MakeAttestation(const Manifest& m, const Scan& s);

// Atalho: carrega, varre, compara e (se Ok) preenche a atestacao.
IntegrityStatus VerifyInstall(const std::string& installDirUtf8, const uint8_t releasePubKey[32],
                              Ed25519VerifyFn verify, Attestation& outAtt);

}  // namespace structura_license
