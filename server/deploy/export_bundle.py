"""Exporta o pacote de release para publicar no servidor de PRODUCAO.

Le o manifesto assinado gerado pelo build (dist/Structura-Metalicas/manifest.txt|sig) e a DLL_RELEASE_KEY, e grava em
server/deploy/bundle/. Esse pacote e enviado a VM e publicado com:  manifest_cli publish ... --dll-key ...

A DLL_RELEASE_KEY e a chave com que rds.enc/mefs.enc foram CIFRADOS (build_solvers.bat -> encrypt_dll.py --version N).
Ela NAO depende da versao do manifesto do app: os .enc sao gerados uma vez e reaproveitados em varios releases. Por isso a
chave vem de "server/deploy/solver_key_version.txt" (versao usada no encrypt_dll; hoje 2). Se voce re-cifrar os solvers com
outra --version, atualize esse arquivo. O script CONFERE que a chave abre rds.enc e mefs.enc e aborta se nao abrir
(antes, uma chave errada so aparecia no cliente como DECIPHER_FAILED_MAC_MISMATCH).

A chave e secreta (protege as DLLs em repouso): nao versione nem compartilhe.
Uso: python server/deploy/export_bundle.py [--dist dist/Structura-Metalicas]
"""
from __future__ import annotations
import argparse
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SERVER = HERE.parent
ROOT = SERVER.parent
sys.path.insert(0, str(SERVER))

import nacl.exceptions  # noqa: E402
import nacl.secret  # noqa: E402

from app import manifest as mf  # noqa: E402
from app.db import SessionLocal  # noqa: E402
from app import integrity  # noqa: E402
from app.models import IntegrityManifest  # noqa: E402


def _opens(blob: bytes, key_hex: str) -> bool:
    try:
        nacl.secret.SecretBox(bytes.fromhex(key_hex)).decrypt(blob)
        return True
    except (nacl.exceptions.CryptoError, ValueError):
        return False


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dist", default=str(ROOT / "dist" / "Structura-Metalicas"))
    ap.add_argument("--enc-dir", default=str(ROOT), help="pasta com rds.enc e mefs.enc (padrao: raiz do repo)")
    ap.add_argument("--out", default=str(HERE / "bundle"), help="pasta de saida (padrao: server/deploy/bundle; a edicao Demo usa bundle/demo)")
    a = ap.parse_args()
    dist = Path(a.dist)
    text = (dist / "manifest.txt").read_text(encoding="utf-8")
    sig = (dist / "manifest.sig").read_text(encoding="ascii").strip()
    from app.config import settings
    m = mf.load_verified(text, sig, settings.RELEASE_PUBKEY_B64)   # confere a assinatura de release

    ver_file = HERE / "solver_key_version.txt"
    solver_ver = int(ver_file.read_text().strip()) if ver_file.is_file() else 2
    db = SessionLocal()
    try:
        row = db.get(IntegrityManifest, solver_ver)
        key = (row.dll_release_key if row and row.dll_release_key else None) or integrity.derive_dll_key_for_version(solver_ver)
    finally:
        db.close()

    # Trava de seguranca: a chave tem que abrir os .enc que vao no instalador.
    for name in ("rds.enc", "mefs.enc"):
        for base in (Path(a.enc_dir), dist):
            f = base / name
            if f.is_file() and not _opens(f.read_bytes(), key):
                print(f"ERRO: a chave da versao {solver_ver} NAO abre {f}. Ajuste server/deploy/solver_key_version.txt "
                      f"para a --version usada em build_solvers.bat (encrypt_dll.py) ou rode build_solvers.bat de novo.",
                      file=sys.stderr)
                return 1

    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    (out / "manifest.txt").write_text(text, encoding="utf-8", newline="\n")
    (out / "manifest.sig").write_text(sig + "\n", encoding="ascii")
    fd = os.open(out / "dll_key.txt", os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w", encoding="ascii") as f:
        f.write(key + "\n")
    print(f"bundle manifesto v{m.version} ({m.file_count} arquivos), chave dos solvers v{solver_ver} (verificada) exportado em {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
