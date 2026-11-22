#pragma once

#include <cstddef>
#include <cstdint>

namespace structura_license {

// Verificação nativa de assinatura destacada Ed25519 (RFC 8032 / TweetNaCl).
// Retorna true se a assinatura for matematicamente válida, false em caso contrário.
// 100% livre de dependências externas (sem necessidade de libsodium externa).
bool Ed25519VerifyCompact(const uint8_t* msg, size_t msgLen,
                          const uint8_t sig[64], const uint8_t pub[32]);

} // namespace structura_license
