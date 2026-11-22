#include <windows.h>
#include <winhttp.h>
#include <iostream>

int main() {
    HINTERNET hSession = WinHttpOpen(L"Structura/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (hSession) {
        std::cout << "WinHTTP is available and functioning!" << std::endl;
        WinHttpCloseHandle(hSession);
    }
    return 0;
}
