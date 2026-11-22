"""Gera UM segredo avulso e o imprime na tela (nada e gravado em arquivo).

  python server/deploy/gen_secret.py key         # 32 bytes aleatorios em base64url  -> CPF_ENC_KEY_B64 / CPF_HMAC_KEY_B64
  python server/deploy/gen_secret.py signing     # seed Ed25519 + chave publica correspondente (SERVER_SIGNING_SEED_B64)
  python server/deploy/gen_secret.py password    # senha forte aleatoria (para contas/admin)
  python server/deploy/gen_secret.py pubkey SEED # mostra a chave publica de uma seed (base64url) existente

CUIDADO: o que aparece na tela e segredo. Nao cole em chat/e-mail/print. Guarde direto no gerenciador de senhas ou no .env.
Antes de trocar qualquer chave em producao leia docs/SEGREDOS_E_CHAVES.md (algumas trocas invalidam dados e instalacoes).
"""
from __future__ import annotations
import base64
import os
import secrets
import sys

from nacl.signing import SigningKey


def b64(b: bytes) -> str:
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode()


def b64d(s: str) -> bytes:
    s = s.strip()
    return base64.urlsafe_b64decode(s + "=" * (-len(s) % 4))


def main() -> int:
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    if cmd == "key":
        print(b64(os.urandom(32)))
    elif cmd == "signing":
        sk = SigningKey.generate()
        print("SEED (segredo):        ", b64(sk.encode()))
        print("PUBLICA (base64url):   ", b64(bytes(sk.verify_key)))
        print("PUBLICA (hex):         ", bytes(sk.verify_key).hex())
    elif cmd == "pubkey" and len(sys.argv) > 2:
        pub = SigningKey(b64d(sys.argv[2])).verify_key
        print("PUBLICA (base64url):   ", b64(bytes(pub)))
        print("PUBLICA (hex):         ", bytes(pub).hex())
    elif cmd == "password":
        alphabet = "abcdefghjkmnpqrstuvwxyzABCDEFGHJKMNPQRSTUVWXYZ23456789-_"
        print("".join(secrets.choice(alphabet) for _ in range(20)))
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
