#include "winhttp_client.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <vector>
#include <cstdio>
#include "license_config.h"

namespace structura_license {

namespace {

// Certificate pinning (SPKI SHA-256). Sem pins configurados = apenas a validacao normal do Windows.
bool ServerCertMatchesPins(HINTERNET hRequest) {
    const auto& pins = GetServerCertPins();
    if (pins.empty()) return true;
    PCCERT_CONTEXT cert = nullptr;
    DWORD sz = sizeof(cert);
    if (!WinHttpQueryOption(hRequest, WINHTTP_OPTION_SERVER_CERT_CONTEXT, &cert, &sz) || !cert) return false;
    BYTE hash[32];
    DWORD hlen = sizeof(hash);
    bool ok = CryptHashPublicKeyInfo(0, CALG_SHA_256, 0, X509_ASN_ENCODING,
                                     &cert->pCertInfo->SubjectPublicKeyInfo, hash, &hlen) && hlen == 32;
    CertFreeCertificateContext(cert);
    if (!ok) return false;
    char hex[65];
    for (int i = 0; i < 32; ++i) snprintf(hex + i * 2, 3, "%02x", hash[i]);
    for (const auto& pin : pins) if (pin == hex) return true;
    return false;
}

std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int req = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (req <= 0) return L"";
    std::wstring out(req, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &out[0], req);
    return out;
}

HttpResponse ExecuteRequest(const std::string& method,
                           const std::string& url,
                           const std::string& jsonBody,
                           const std::string& bearerToken,
                           int timeoutMs) {
    HttpResponse resp;
    std::wstring wideUrl = Utf8ToWide(url);
    if (wideUrl.empty()) {
        resp.error = "URL_EMPTY_OR_INVALID";
        return resp;
    }

    URL_COMPONENTS urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = static_cast<DWORD>(-1);
    urlComp.dwHostNameLength = static_cast<DWORD>(-1);
    urlComp.dwUrlPathLength = static_cast<DWORD>(-1);
    urlComp.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &urlComp)) {
        resp.error = "INVALID_URL_FORMAT";
        return resp;
    }

    std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring path = (urlComp.dwUrlPathLength > 0)
        ? std::wstring(urlComp.lpszUrlPath, urlComp.dwUrlPathLength + urlComp.dwExtraInfoLength)
        : L"/";
    INTERNET_PORT port = urlComp.nPort;
    bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

    HINTERNET hSession = WinHttpOpen(L"StructuraLicensing/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        resp.error = "WINHTTP_INIT_FAILED";
        return resp;
    }

    // Configura timeouts determinísticos (fail-closed em timeout)
    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        resp.error = "CONNECTION_FAILED";
        return resp;
    }

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    std::wstring methodW = Utf8ToWide(method);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, methodW.c_str(), path.c_str(),
                                           nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        resp.error = "OPEN_REQUEST_FAILED";
        return resp;
    }

    // Monta cabeçalhos
    std::wstring headers = L"Content-Type: application/json\r\nAccept: application/json\r\n";
    if (!bearerToken.empty()) {
        headers += L"Authorization: Bearer " + Utf8ToWide(bearerToken) + L"\r\n";
    }

    BOOL sendOk = WinHttpSendRequest(
        hRequest,
        headers.c_str(),
        static_cast<DWORD>(headers.length()),
        jsonBody.empty() ? nullptr : const_cast<char*>(jsonBody.data()),
        static_cast<DWORD>(jsonBody.size()),
        static_cast<DWORD>(jsonBody.size()),
        0);

    if (!sendOk) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        resp.error = (err == ERROR_WINHTTP_TIMEOUT) ? "REQUEST_TIMEOUT" : "SEND_REQUEST_FAILED";
        return resp;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        resp.error = (err == ERROR_WINHTTP_TIMEOUT) ? "RESPONSE_TIMEOUT" : "RECEIVE_RESPONSE_FAILED";
        return resp;
    }

    if (isHttps && !ServerCertMatchesPins(hRequest)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        resp.error = "CERT_PIN_MISMATCH";
        return resp;   // statusCode 0 = fail-closed
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
        resp.statusCode = static_cast<int>(statusCode);
    }

    // Leitura do corpo da resposta
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead) && bytesRead > 0) {
            resp.body.append(buffer.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return resp;
}

} // namespace

HttpResponse HttpPost(const std::string& url,
                      const std::string& jsonBody,
                      const std::string& bearerToken,
                      int timeoutMs) {
    return ExecuteRequest("POST", url, jsonBody, bearerToken, timeoutMs);
}

HttpResponse HttpGet(const std::string& url,
                     const std::string& bearerToken,
                     int timeoutMs) {
    return ExecuteRequest("GET", url, "", bearerToken, timeoutMs);
}

} // namespace structura_license
#else
// Fallback não-Windows se necessário para cross-compile
namespace structura_license {
HttpResponse HttpPost(const std::string&, const std::string&, const std::string&, int) {
    return {0, "", "NOT_SUPPORTED_NON_WINDOWS"};
}
HttpResponse HttpGet(const std::string&, const std::string&, int) {
    return {0, "", "NOT_SUPPORTED_NON_WINDOWS"};
}
}
#endif
