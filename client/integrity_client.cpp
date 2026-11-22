#include "integrity_client.h"
#include "sha256.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace structura_license {
namespace {

fs::path Utf8ToFsPath(const std::string& u8) {
#ifdef _WIN32
    if (u8.empty()) return fs::path();
    int n = MultiByteToWideChar(CP_UTF8, 0, u8.data(), static_cast<int>(u8.size()), nullptr, 0);
    if (n <= 0) return fs::u8path(u8);
    std::wstring ws(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, u8.data(), static_cast<int>(u8.size()), &ws[0], n);
    return fs::path(ws);
#else
    return fs::u8path(u8);
#endif
}

std::string FsPathToUtf8(const fs::path& p) {
#ifdef _WIN32
    std::wstring ws = p.wstring();
    if (ws.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return p.string();
    std::string u8(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), &u8[0], n, nullptr, nullptr);
    std::replace(u8.begin(), u8.end(), '\\', '/');
    return u8;
#else
    return p.generic_string();
#endif
}

const char* kHeader = "STRUCTURA-MANIFEST 1";
// Prefixo de dominio da assinatura (igual a DOM_MANIFEST do servidor, incluindo o \0 final).
const char kDomManifest[] = "structura-manifest-v1";  // + '\0'
const char* kManifestFiles[] = {"manifest.txt", "manifest.sig"};

bool IsIgnoredRootFile(const std::string& name) {
    if (name == kManifestFiles[0] || name == kManifestFiles[1]) return true;
#ifdef _WIN32
    if (_stricmp(name.c_str(), "uninstall.exe") == 0) return true;
#else
    if (name == "Uninstall.exe" || name == "uninstall.exe") return true;
#endif
    return false;
}

bool IsHex64(const std::string& s) {
    if (s.size() != 64) return false;
    for (char c : s) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

bool IsCanonicalUInt(const std::string& s, bool allowZero, long& out) {
    if (s.empty() || s.size() > 9) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    if (s.size() > 1 && s[0] == '0') return false;
    out = std::stol(s);
    return allowZero || out >= 1;
}

bool CheckPath(const std::string& p, bool allowDot) {
    if (allowDot && p == ".") return true;
    if (p.empty() || p[0] == '/') return false;
    for (unsigned char c : p) if (c == '\\' || c < 32) return false;
    size_t start = 0;
    while (true) {
        size_t e = p.find('/', start);
        std::string seg = p.substr(start, e == std::string::npos ? std::string::npos : e - start);
        if (seg.empty() || seg == "." || seg == "..") return false;
        if (e == std::string::npos) break;
        start = e + 1;
    }
    return true;
}

bool StartsWith(const std::string& s, const char* pre) { return s.rfind(pre, 0) == 0; }

bool ReadFile(const fs::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool HashFile(const fs::path& p, std::string& hex) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    Sha256 h;
    char buf[65536];
    while (f) {
        f.read(buf, sizeof buf);
        std::streamsize n = f.gcount();
        if (n > 0) h.Update(buf, size_t(n));
    }
    if (f.bad()) return false;
    hex = h.FinalHex();
    return true;
}

bool IsReparse(const fs::path& p) {
#ifdef _WIN32
    DWORD a = GetFileAttributesW(p.wstring().c_str());
    if (a == INVALID_FILE_ATTRIBUTES) return true;  // nao conseguimos ler => falha
    return (a & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    std::error_code ec;
    return fs::is_symlink(fs::symlink_status(p, ec));
#endif
}

bool IsExcluded(const std::vector<std::string>& ex, const std::string& rel) {
    return std::find(ex.begin(), ex.end(), rel) != ex.end();
}

bool ScanRec(const fs::path& root, const fs::path& cur, const std::string& rel,
             const std::vector<std::string>& exclude, Scan& out) {
    std::error_code ec;
    int count = 0;
    std::vector<fs::directory_entry> entries;
    for (fs::directory_iterator it(cur, ec), end; !ec && it != end; it.increment(ec)) entries.push_back(*it);
    if (ec) return false;
    for (const auto& e : entries) {
        const fs::path& p = e.path();
        std::string name = FsPathToUtf8(p.filename());
        std::string relp = rel == "." ? name : rel + "/" + name;
        if (IsReparse(p)) return false;                       // symlink/junction => falha
        std::error_code e2;
        fs::file_status st = fs::symlink_status(p, e2);
        if (e2) return false;
        if (fs::is_directory(st)) {
            if (IsExcluded(exclude, relp)) continue;
            if (!ScanRec(root, p, relp, exclude, out)) return false;
        } else if (fs::is_regular_file(st)) {
            if (rel == "." && IsIgnoredRootFile(name)) continue;
            std::string hex;
            if (!HashFile(p, hex)) return false;
            out.files[relp] = hex;
            ++count;
        } else {
            return false;  // dispositivo/fifo/etc.
        }
    }
    out.dirs[rel] = count;
    return true;
}

}  // namespace

std::string ComputeRoot(const std::map<std::string, std::string>& files,
                        const std::map<std::string, int>& dirs) {
    Sha256 h;
    for (const auto& kv : files) h.Update("F " + kv.first + "\t" + kv.second + "\n");
    for (const auto& kv : dirs) h.Update("D " + kv.first + "\t" + std::to_string(kv.second) + "\n");
    return h.FinalHex();
}

std::string RenderManifest(const Manifest& m) {
    std::string s = std::string(kHeader) + "\nversion " + std::to_string(m.version) + "\nexe " + m.exe + "\n";
    std::vector<std::string> ex = m.exclude;
    std::sort(ex.begin(), ex.end());
    for (const auto& d : ex) s += "exclude " + d + "\n";
    for (const auto& kv : m.files) s += "F " + kv.first + "\t" + kv.second + "\n";
    for (const auto& kv : m.dirs) s += "D " + kv.first + "\t" + std::to_string(kv.second) + "\n";
    return s;
}

bool ParseManifest(const std::string& text, Manifest& out) {
    out = Manifest();
    if (text.empty() || text.back() != '\n' || text.find('\r') != std::string::npos) return false;
    std::vector<std::string> lines;
    {
        std::string body = text.substr(0, text.size() - 1);
        size_t start = 0;
        while (true) {
            size_t e = body.find('\n', start);
            lines.push_back(body.substr(start, e == std::string::npos ? std::string::npos : e - start));
            if (e == std::string::npos) break;
            start = e + 1;
        }
    }
    if (lines.size() < 4 || lines[0] != kHeader) return false;
    if (!StartsWith(lines[1], "version ") || !StartsWith(lines[2], "exe ")) return false;
    long v = 0;
    if (!IsCanonicalUInt(lines[1].substr(8), false, v)) return false;
    out.version = int(v);
    out.exe = lines[2].substr(4);
    if (!CheckPath(out.exe, false)) return false;
    for (size_t i = 3; i < lines.size(); ++i) {
        const std::string& ln = lines[i];
        if (StartsWith(ln, "exclude ")) {
            std::string d = ln.substr(8);
            if (!CheckPath(d, false)) return false;
            if (std::find(out.exclude.begin(), out.exclude.end(), d) != out.exclude.end()) return false;
            if (!out.files.empty() || !out.dirs.empty()) return false;
            out.exclude.push_back(d);
        } else if (StartsWith(ln, "F ")) {
            std::string rest = ln.substr(2);
            size_t t = rest.find('\t');
            if (t == std::string::npos) return false;
            std::string p = rest.substr(0, t), h = rest.substr(t + 1);
            if (!CheckPath(p, false) || !IsHex64(h) || out.files.count(p) || !out.dirs.empty()) return false;
            if (IsIgnoredRootFile(p)) return false;
            out.files[p] = h;
        } else if (StartsWith(ln, "D ")) {
            std::string rest = ln.substr(2);
            size_t t = rest.find('\t');
            if (t == std::string::npos) return false;
            std::string p = rest.substr(0, t);
            long c = 0;
            if (!CheckPath(p, true) || !IsCanonicalUInt(rest.substr(t + 1), true, c) || out.dirs.count(p)) return false;
            out.dirs[p] = int(c);
        } else {
            return false;
        }
    }
    if (!out.files.count(out.exe)) return false;
    if (RenderManifest(out) != text) return false;  // forma canonica unica
    std::map<std::string, int> counts;
    for (const auto& kv : out.files) {
        size_t s = kv.first.rfind('/');
        counts[s == std::string::npos ? "." : kv.first.substr(0, s)]++;
    }
    for (const auto& kv : counts) {
        auto it = out.dirs.find(kv.first);
        if (it == out.dirs.end() || it->second != kv.second) return false;
    }
    for (const auto& kv : out.dirs) {
        auto it = counts.find(kv.first);
        if (kv.second != (it == counts.end() ? 0 : it->second)) return false;
    }
    return true;
}

bool Base64UrlDecode(const std::string& in, std::vector<uint8_t>& out) {
    out.clear();
    uint32_t acc = 0; int bits = 0;
    for (char c : in) {
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '-') v = 62;
        else if (c == '_') v = 63;
        else return false;  // sem padding '=' nem espacos
        acc = (acc << 6) | uint32_t(v); bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back(uint8_t(acc >> bits)); acc &= (1u << bits) - 1; }
    }
    return true;
}

bool ScanDirectory(const std::string& installDirUtf8, const std::vector<std::string>& exclude, Scan& out) {
    out = Scan();
    std::error_code ec;
    fs::path root = Utf8ToFsPath(installDirUtf8);
    if (!fs::is_directory(root, ec) || ec) return false;
    return ScanRec(root, root, ".", exclude, out);
}

IntegrityStatus LoadManifest(const std::string& dir, const uint8_t pub[32], Ed25519VerifyFn verify, Manifest& out) {
    if (!verify) return IntegrityStatus::ManifestBadSig;  // fail-closed
    fs::path root = Utf8ToFsPath(dir);
    std::string text, sigText;
    if (!ReadFile(root / "manifest.txt", text) || !ReadFile(root / "manifest.sig", sigText))
        return IntegrityStatus::ManifestMissing;
    while (!sigText.empty() && (sigText.back() == '\n' || sigText.back() == '\r')) sigText.pop_back();
    std::vector<uint8_t> sig;
    if (!Base64UrlDecode(sigText, sig) || sig.size() != 64) return IntegrityStatus::ManifestBadSig;
    std::string msg(kDomManifest, sizeof kDomManifest);  // inclui o '\0' final
    msg += text;
    if (!verify(reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), sig.data(), pub))
        return IntegrityStatus::ManifestBadSig;          // assinatura ANTES de parsear
    if (!ParseManifest(text, out)) return IntegrityStatus::ManifestMalformed;
    return IntegrityStatus::Ok;
}

IntegrityStatus CompareScan(const Manifest& m, const Scan& s) {
    for (const auto& kv : m.files) {
        auto it = s.files.find(kv.first);
        if (it == s.files.end()) return IntegrityStatus::FileMissing;
        if (it->second != kv.second) return IntegrityStatus::FileModified;
    }
    for (const auto& kv : s.files)
        if (!m.files.count(kv.first)) return IntegrityStatus::FileExtra;
    for (const auto& kv : s.dirs)
        if (!m.dirs.count(kv.first)) return IntegrityStatus::FileExtra;       // diretorio extra
    for (const auto& kv : m.dirs) {
        auto it = s.dirs.find(kv.first);
        if (it == s.dirs.end()) return IntegrityStatus::FileMissing;
        if (it->second != kv.second) return IntegrityStatus::DirCountMismatch;
    }
    return IntegrityStatus::Ok;
}

Attestation MakeAttestation(const Manifest& m, const Scan& s) {
    Attestation a;
    a.manifestVersion = m.version;
    a.filesRoot = ComputeRoot(s.files, s.dirs);
    a.fileCount = int(s.files.size());
    auto it = s.files.find(m.exe);
    a.exeHash = it == s.files.end() ? std::string(64, '0') : it->second;
    return a;
}

IntegrityStatus VerifyInstall(const std::string& dir, const uint8_t pub[32], Ed25519VerifyFn verify, Attestation& att) {
    att = Attestation();
    Manifest m;
    IntegrityStatus st = LoadManifest(dir, pub, verify, m);
    if (st != IntegrityStatus::Ok) return st;
    Scan s;
    if (!ScanDirectory(dir, m.exclude, s)) return IntegrityStatus::ScanError;
    att = MakeAttestation(m, s);   // sempre a atestacao REAL (o servidor decide; nunca forjar "ok")
    return CompareScan(m, s);
}

}  // namespace structura_license
