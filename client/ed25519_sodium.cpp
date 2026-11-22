// Ed25519 real via libsodium (gratuito, ISC). Linke com -lsodium. Chave publica embutida
// (release e servidor) e a unica "chave" no binario. NAO ha segredos aqui.
#include "integrity_client.h"
#include <sodium.h>

namespace structura_license {

bool Ed25519VerifySodium(const uint8_t* msg, size_t len, const uint8_t sig[64], const uint8_t pub[32]) {
    if (sodium_init() < 0) return false;  // fail-closed
    return crypto_sign_verify_detached(sig, msg, static_cast<unsigned long long>(len), pub) == 0;
}

}  // namespace structura_license
