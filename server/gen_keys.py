"""Gera as chaves necessarias no formato do .env.

Uso:
  python gen_keys.py            # imprime as 3 linhas (cole no secret manager / .env)
  python gen_keys.py --write    # cria .env a partir de .env.example (RECUSA sobrescrever)

Nunca versione o .env. Em producao use o secret manager do provedor.
A chave PUBLICA impressa e a unica que vai embutida no cliente.
"""
import argparse
import base64
import os
import sys
from pathlib import Path

from nacl.signing import SigningKey


def b64(b: bytes) -> str:
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode()


def generate() -> dict[str, str]:
    sk = SigningKey.generate()
    rk = SigningKey.generate()  # chave de RELEASE (assina manifestos)
    return {
        "SERVER_SIGNING_SEED_B64": b64(sk.encode()),  # 32 bytes (seed)
        "CPF_ENC_KEY_B64": b64(os.urandom(32)),
        "CPF_HMAC_KEY_B64": b64(os.urandom(32)),
        "RELEASE_PUBKEY_B64": b64(bytes(rk.verify_key)),  # so a PUBLICA vai ao servidor
        "_PUBKEY": b64(bytes(sk.verify_key)),
        "_RELEASE_SEED": b64(rk.encode()),  # PRIVADA: nunca no .env do servidor
    }


def render_env(template: str, keys: dict[str, str]) -> str:
    out = []
    for line in template.splitlines():
        name = line.split("=", 1)[0]
        out.append(f"{name}={keys[name]}" if name in keys and not name.startswith("_") else line)
    return "\n".join(out) + "\n"


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--dir", default=str(Path(__file__).parent))
    args = ap.parse_args(argv)
    keys = generate()
    if args.write:
        d = Path(args.dir)
        target = d / ".env"
        if target.exists():
            print("ERRO: .env ja existe; recusando sobrescrever (apague manualmente).", file=sys.stderr)
            return 1
        text = render_env((d / ".env.example").read_text(encoding="utf-8"), keys)
        fd = os.open(target, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write(text)
        print(f".env criado em {target} (nao versione). Chave PUBLICA para o cliente:")
    else:
        for k in ("SERVER_SIGNING_SEED_B64", "CPF_ENC_KEY_B64", "CPF_HMAC_KEY_B64", "RELEASE_PUBKEY_B64"):
            print(f"{k}={keys[k]}")
        print("\n# Chave PUBLICA do servidor (embuta no cliente):")
    print("SERVER_PUBKEY_B64=" + keys["_PUBKEY"])
    print("\n# Chave PRIVADA de RELEASE (assina manifestos). Guarde OFFLINE / secret do CI.")
    print("# NAO colocar no servidor, no repo nem no pacote do cliente:")
    print("RELEASE_SIGNING_SEED_B64=" + keys["_RELEASE_SEED"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
