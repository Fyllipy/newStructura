#include <filesystem>
#include <iostream>
#include <string>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#endif
namespace fs = std::filesystem;

std::string PathToUtf8(const fs::path& p) {
#ifdef _WIN32
    std::wstring ws = p.wstring();
    if (ws.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), NULL, 0, NULL, NULL);
    if (n <= 0) return {};
    std::string u8(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), &u8[0], n, NULL, NULL);
    std::replace(u8.begin(), u8.end(), '\\', '/');
    return u8;
#else
    return p.generic_string();
#endif
}

fs::path Utf8ToPath(const std::string& u8) {
#ifdef _WIN32
    if (u8.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, u8.data(), (int)u8.size(), NULL, 0);
    if (n <= 0) return {};
    std::wstring ws(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, u8.data(), (int)u8.size(), &ws[0], n);
    return fs::path(ws);
#else
    return fs::u8path(u8);
#endif
}

int main() {
    fs::path p = Utf8ToPath("ção.dll");
    std::cout << "PathToUtf8: " << PathToUtf8(p) << std::endl;
    try {
        std::cout << "generic_u8string: " << p.filename().generic_u8string() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "generic_u8string threw: " << e.what() << std::endl;
    }
    return 0;
}
