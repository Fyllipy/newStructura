#include "ed25519_compact.h"
#include <vector>
#include <cstring>

extern "C" {
#include "tweetnacl.h"

void randombytes(unsigned char *ptr, unsigned long long length) {
    for (unsigned long long i = 0; i < length; ++i) {
        ptr[i] = 0;
    }
}
}

namespace structura_license {

bool Ed25519VerifyCompact(const uint8_t* msg, size_t msgLen,
                          const uint8_t sig[64], const uint8_t pub[32]) {
    if (sig == nullptr || pub == nullptr) return false;
    unsigned long long smlen = 64 + msgLen;
    std::vector<uint8_t> sm(smlen);
    std::memcpy(sm.data(), sig, 64);
    if (msgLen > 0 && msg != nullptr) {
        std::memcpy(sm.data() + 64, msg, msgLen);
    }
    std::vector<uint8_t> m_out(smlen);
    unsigned long long m_out_len = 0;
    int rc = crypto_sign_open(m_out.data(), &m_out_len, sm.data(), smlen, pub);
    return (rc == 0);
}

} // namespace structura_license
