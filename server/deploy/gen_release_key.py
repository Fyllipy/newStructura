"""Gera um NOVO par de chaves de RELEASE (assina os manifestos de integridade dos instaladores).

Cria server/release.key (a chave PRIVADA, 'seed' base64url; fica SO na maquina de build, 0600, fora do Git) e imprime a chave
PUBLICA, que precisa ir em dois lugares: RELEASE_PUBKEY_B64 do servidor (.env) e kReleasePubKey em client/license_config.cpp.

ATENCAO: trocar a chave de release invalida TODOS os instaladores ja distribuidos (o app antigo nao reconhece manifestos novos
e o servidor deixa de aceitar os antigos). So faca se a chave vazou. Veja docs/SEGREDOS_E_CHAVES.md, secao "Chave de release".
Recusa sobrescrever um release.key existente. Uso:  python server/deploy/gen_release_key.py [--out caminho]
"""
from __future__ import annotations
import argparse
import base64
import os
import sys
from pathlib import Path

from nacl.signing import SigningKey

DEFAULT = Path(__file__).resolve().parents[1] / "release.key"


def b64(b: bytes) -> str:
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(DEFAULT))
    a = ap.parse_args()
    out = Path(a.out)
    if out.exists():
        print(f"ERRO: {out} ja existe; recusando sobrescrever. Mova/renomeie o antigo (guarde uma copia!) se for trocar de verdade.",
              file=sys.stderr)
        return 1
    sk = SigningKey.generate()
    fd = os.open(out, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "w", encoding="ascii") as f:
        f.write(b64(sk.encode()) + "\n")
    print(f"Chave PRIVADA gravada em {out} (faca backup num cofre; nunca no Git).")
    print("Chave PUBLICA (nao e segredo):")
    print("  RELEASE_PUBKEY_B64=" + b64(bytes(sk.verify_key)))
    print("Array C++ para kReleasePubKey (client/license_config.cpp):  python server/deploy/pubkey_to_cpp.py "
          + b64(bytes(sk.verify_key)) + " --name kReleasePubKey")
    return 0


if __name__ == "__main__":
    sys.exit(main())
