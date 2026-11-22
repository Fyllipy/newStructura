"""Gera os SEGREDOS DE PRODUCAO (nunca reutiliza os de desenvolvimento) em server/deploy/secrets/.

Saidas (a pasta secrets/ esta no .gitignore; nunca versione nem envie por canal inseguro):
  secrets/prod.env          -> vai para a VM (scp) como /opt/structura/server/.env
  secrets/server_pubkey.hex -> chave PUBLICA do servidor (32 bytes hex); embutida no cliente

A chave de RELEASE e a mesma ja usada nos builds (so a parte PUBLICA vai ao servidor, copiada de server/.env).
Recusa sobrescrever: trocar as chaves invalida tokens, CPFs cifrados e segredos 2FA ja gravados em producao.

Uso: python server/deploy/gen_prod_env.py
"""
from __future__ import annotations
import base64
import os
import sys
from pathlib import Path

from nacl.signing import SigningKey

ROOT = Path(__file__).resolve().parent
SERVER = ROOT.parent
OUT = ROOT / "secrets"


def b64(b: bytes) -> str:
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode()


def read_dev_release_pub() -> str:
    for line in (SERVER / ".env").read_text(encoding="utf-8").splitlines():
        if line.startswith("RELEASE_PUBKEY_B64="):
            v = line.split("=", 1)[1].strip()
            if v:
                return v
    raise SystemExit("RELEASE_PUBKEY_B64 nao encontrado em server/.env (rode o build de release ao menos uma vez).")


def main() -> int:
    target = OUT / "prod.env"
    if target.exists():
        print("ERRO: secrets/prod.env ja existe; recusando sobrescrever (apague manualmente se tiver certeza).",
              file=sys.stderr)
        return 1
    OUT.mkdir(parents=True, exist_ok=True)
    sk = SigningKey.generate()
    text = "\n".join([
        "# PRODUCAO - gerado por gen_prod_env.py. NAO versionar.",
        "DATABASE_URL=sqlite:////opt/structura/data/structura.db",
        f"SERVER_SIGNING_SEED_B64={b64(sk.encode())}",
        f"CPF_ENC_KEY_B64={b64(os.urandom(32))}",
        f"CPF_HMAC_KEY_B64={b64(os.urandom(32))}",
        f"RELEASE_PUBKEY_B64={read_dev_release_pub()}",
        "TRUST_PROXY_HEADERS=true",
        "EXPOSE_DOCS=false",
        "REQUIRE_2FA=true",
        "TELEMETRY_RETENTION_DAYS=180",
        "ADMIN_TOKEN_TTL_SECONDS=1800",
        "",
    ])
    fd = os.open(target, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        f.write(text)
    (OUT / "server_pubkey.hex").write_text(bytes(sk.verify_key).hex() + "\n", encoding="ascii")
    print(f"OK: {target} criado (segredos de producao; guarde backup seguro, ex.: gerenciador de senhas).")
    print("Chave publica do servidor (hex):", bytes(sk.verify_key).hex())
    return 0


if __name__ == "__main__":
    sys.exit(main())
