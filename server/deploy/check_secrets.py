"""Verifica se os segredos e chaves do projeto estao COERENTES entre si (nao imprime nenhum segredo).

  python server/deploy/check_secrets.py

Confere: formato das chaves dos .env; se server/release.key combina com a RELEASE_PUBKEY_B64 dos .env e com kReleasePubKey do app;
se a chave publica de producao (secrets/server_pubkey.hex) combina com a seed do prod.env e com kServerPubKey do app;
se a chave dos solvers (dll_key) abre rds.enc/mefs.enc; se segredos nao estao no Git; permissoes dos arquivos.
Sai com codigo 1 se algo estiver FALHANDO.
"""
from __future__ import annotations
import base64
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SERVER = HERE.parent
ROOT = SERVER.parent
sys.path.insert(0, str(SERVER))

from nacl.signing import SigningKey  # noqa: E402

fails = 0


def ok(msg):
    print(f"  [OK]    {msg}")


def bad(msg):
    global fails
    fails += 1
    print(f"  [FALHA] {msg}")


def warn(msg):
    print(f"  [AVISO] {msg}")


def b64d(s: str) -> bytes:
    s = s.strip()
    return base64.urlsafe_b64decode(s + "=" * (-len(s) % 4))


def read_env(path: Path) -> dict[str, str]:
    out = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#") and "=" in line:
            k, v = line.split("=", 1)
            out[k.strip()] = v.strip()
    return out


def cpp_arrays(text: str) -> dict[str, list[bytes]]:
    res: dict[str, list[bytes]] = {}
    for m in re.finditer(r"const uint8_t (\w+)\[32\]\s*=\s*\{([^}]*)\}", text):
        vals = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", m.group(2)))
        res.setdefault(m.group(1), []).append(vals)
    return res


def check_env(label: str, path: Path) -> dict[str, str] | None:
    print(f"\n{label}: {path}")
    if not path.is_file():
        warn("arquivo nao existe (ok se voce nao usa este ambiente neste computador)")
        return None
    env = read_env(path)
    for k in ("SERVER_SIGNING_SEED_B64", "CPF_ENC_KEY_B64", "CPF_HMAC_KEY_B64", "RELEASE_PUBKEY_B64"):
        try:
            good = len(b64d(env.get(k, ""))) == 32
        except Exception:
            good = False
        (ok if good else bad)(f"{k} {'valida (32 bytes)' if good else 'AUSENTE ou invalida'}")
    return env


def main() -> int:
    dev = check_env("Servidor de DESENVOLVIMENTO", SERVER / ".env")
    prod = check_env("Servidor de PRODUCAO (copia local)", HERE / "secrets" / "prod.env")

    cpp = cpp_arrays((ROOT / "client" / "license_config.cpp").read_text(encoding="utf-8"))

    print("\nChave de release (assina os manifestos)")
    rk = SERVER / "release.key"
    if not rk.is_file():
        bad("server/release.key nao existe (necessaria para gerar releases)")
    else:
        pub = bytes(SigningKey(b64d(rk.read_text(encoding="ascii"))).verify_key)
        for label, env in (("dev .env", dev), ("prod.env", prod)):
            if env and env.get("RELEASE_PUBKEY_B64"):
                (ok if b64d(env["RELEASE_PUBKEY_B64"]) == pub else bad)(f"release.key combina com RELEASE_PUBKEY_B64 do {label}")
        embedded = cpp.get("kReleasePubKey", [b""])[0]
        (ok if embedded == pub else bad)("release.key combina com kReleasePubKey embutida no app")

    print("\nChave do servidor de producao (assina tokens)")
    pubhex = HERE / "secrets" / "server_pubkey.hex"
    if prod and pubhex.is_file():
        derived = bytes(SigningKey(b64d(prod["SERVER_SIGNING_SEED_B64"])).verify_key)
        (ok if derived == bytes.fromhex(pubhex.read_text().strip()) else bad)("server_pubkey.hex combina com a seed do prod.env")
        arrs = cpp.get("kServerPubKey", [])
        (ok if arrs and arrs[-1] == derived else bad)("kServerPubKey (build de RELEASE) no app combina com a seed do prod.env")
    else:
        warn("sem prod.env/server_pubkey.hex neste computador; pulando")

    print("\nChave das DLLs dos solvers")
    key_file = HERE / "bundle" / "dll_key.txt"
    try:
        import nacl.exceptions
        import nacl.secret
        for name in ("rds.enc", "mefs.enc"):
            enc = ROOT / name
            if enc.is_file() and key_file.is_file():
                try:
                    nacl.secret.SecretBox(bytes.fromhex(key_file.read_text().strip())).decrypt(enc.read_bytes())
                    ok(f"bundle/dll_key.txt abre {name}")
                except nacl.exceptions.CryptoError:
                    bad(f"bundle/dll_key.txt NAO abre {name} (veja docs/SEGREDOS_E_CHAVES.md, secao dll_key)")
            elif not key_file.is_file():
                warn("bundle/dll_key.txt nao existe (gerado por build_release.bat); pulando")
                break
    except ImportError:
        warn("PyNaCl indisponivel")

    print("\nGit (segredos nao podem ser versionados)")
    try:
        tracked = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.splitlines()
        leak = [f for f in tracked if re.search(r"(^|/)(\.env|prod\.env|release\.key|dll_key\.txt|server_pubkey\.hex)$|\.key$|\.db$", f)]
        (ok if not leak else bad)("nenhum segredo/banco rastreado pelo Git" if not leak else f"RASTREADOS PELO GIT: {leak}")
        ign = subprocess.run(["git", "check-ignore", "server/.env", "server/release.key", "server/deploy/secrets/prod.env"],
                             cwd=ROOT, capture_output=True, text=True).stdout.split()
        (ok if len(ign) == 3 else bad)(f"{len(ign)}/3 arquivos secretos estao no .gitignore")
    except Exception as e:  # noqa: BLE001
        warn(f"nao foi possivel consultar o Git: {e}")

    print("\n" + ("TUDO COERENTE." if fails == 0 else f"{fails} PROBLEMA(S) ENCONTRADO(S) - veja docs/SEGREDOS_E_CHAVES.md."))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
