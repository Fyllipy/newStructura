#pragma once
// HMAC-SHA256 e HKDF-SHA256 (RFC 2104 / RFC 5869) autônomos em C++ para o cliente.
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "sha256.h"

namespace structura_license {

inline void HmacSha256(const uint8_t* key, size_t keyLen,
                       const uint8_t* msg, size_t msgLen,
                       uint8_t out[32]) {
    uint8_t k[64] = {0};
    if (keyLen > 64) {
        Sha256 h;
        h.Update(key, keyLen);
        h.Final(k);
    } else {
        std::memcpy(k, key, keyLen);
    }

    uint8_t i_key_pad[64];
    uint8_t o_key_pad[64];
    for (int i = 0; i < 64; ++i) {
        i_key_pad[i] = k[i] ^ 0x36;
        o_key_pad[i] = k[i] ^ 0x5c;
    }

    uint8_t inner[32];
    Sha256 h_in;
    h_in.Update(i_key_pad, 64);
    h_in.Update(msg, msgLen);
    h_in.Final(inner);

    Sha256 h_out;
    h_out.Update(o_key_pad, 64);
    h_out.Update(inner, 32);
    h_out.Final(out);
}

// HKDF-Extract(salt, ikm) -> prk
inline void HkdfExtract(const uint8_t* salt, size_t saltLen,
                        const uint8_t* ikm, size_t ikmLen,
                        uint8_t prk[32]) {
    uint8_t default_salt[32] = {0};
    if (salt == nullptr || saltLen == 0) {
        salt = default_salt;
        saltLen = 32;
    }
    HmacSha256(salt, saltLen, ikm, ikmLen, prk);
}

// HKDF-Expand(prk, info, length=32) -> okm
inline void HkdfExpand(const uint8_t prk[32],
                       const uint8_t* info, size_t infoLen,
                       uint8_t okm[32]) {
    // Para L=32 e HashLen=32: T(1) = HMAC-Hash(PRK, info || 0x01)
    std::vector<uint8_t> msg;
    msg.reserve(infoLen + 1);
    if (info && infoLen > 0) {
        msg.insert(msg.end(), info, info + infoLen);
    }
    msg.push_back(0x01);
    HmacSha256(prk, 32, msg.data(), msg.size(), okm);
}

// HKDF-SHA256 completo gerando 32 bytes de chave
inline void HkdfSha256_32(const uint8_t* ikm, size_t ikmLen,
                          const uint8_t* salt, size_t saltLen,
                          const uint8_t* info, size_t infoLen,
                          uint8_t out[32]) {
    uint8_t prk[32];
    HkdfExtract(salt, saltLen, ikm, ikmLen, prk);
    HkdfExpand(prk, info, infoLen, out);
}

} // namespace structura_license
