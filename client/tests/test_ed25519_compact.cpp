#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include "ed25519_compact.h"

int main() {
    // Chave pública gerada pelo Python pynacl:
    // abe6bcdf94cd132b9e8c52e02188dc0796529971518630c52d3d1572fd914a2d
    const char* pkHex = "abe6bcdf94cd132b9e8c52e02188dc0796529971518630c52d3d1572fd914a2d";
    // Assinatura gerada pelo Python:
    const char* sigHex = "6199f6ff40f0092fd7cbdd2868296f3c01f4adc4c47d7b83a61746d47a74331bef835e9e4b45ef7ae0da5093aa8efd034b58c85eed45f11a0969ef7a0d2b090f";
    std::string msg = "test message";

    uint8_t pk[32];
    for (int i = 0; i < 32; ++i) {
        unsigned int byteVal;
        sscanf(pkHex + 2 * i, "%02x", &byteVal);
        pk[i] = static_cast<uint8_t>(byteVal);
    }

    uint8_t sig[64];
    for (int i = 0; i < 64; ++i) {
        unsigned int byteVal;
        sscanf(sigHex + 2 * i, "%02x", &byteVal);
        sig[i] = static_cast<uint8_t>(byteVal);
    }

    bool ok = structura_license::Ed25519VerifyCompact(
        reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), sig, pk);

    std::cout << "Ed25519VerifyCompact result: " << (ok ? "VALID" : "INVALID") << std::endl;
    assert(ok == true);

    // Mensagem adulterada -> deve falhar
    std::string badMsg = "test message 2";
    bool badOk = structura_license::Ed25519VerifyCompact(
        reinterpret_cast<const uint8_t*>(badMsg.data()), badMsg.size(), sig, pk);
    assert(badOk == false);

    std::cout << "Compact Ed25519 test PASSED!" << std::endl;
    return 0;
}
