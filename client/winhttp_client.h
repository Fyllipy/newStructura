#pragma once

#include <string>

namespace structura_license {

struct HttpResponse {
    int statusCode = 0;       // HTTP status (0 = erro de rede/timeout/fail-closed)
    std::string body;
    std::string error;
    bool ok() const { return statusCode >= 200 && statusCode < 300; }
};

// Faz requisição HTTP/HTTPS síncrona com WinHTTP.
// Timeout padrão de 5000ms. Falhas de rede ou timeout retornam statusCode = 0 (fail-closed).
HttpResponse HttpPost(const std::string& url,
                      const std::string& jsonBody,
                      const std::string& bearerToken = "",
                      int timeoutMs = 5000);

HttpResponse HttpGet(const std::string& url,
                     const std::string& bearerToken = "",
                     int timeoutMs = 5000);

} // namespace structura_license
