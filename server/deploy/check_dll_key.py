"""Confere QUAL chave decifra rds.enc/mefs.enc (sem imprimir chaves). Uso:
  python server/deploy/check_dll_key.py [caminho\\rds.enc]
Testa a chave de cada versao presente no banco local, a derivada de cada versao e a de server/deploy/bundle/dll_key.txt."""
from __future__ import annotations
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

import nacl.exceptions  # noqa: E402
import nacl.secret  # noqa: E402

from app import integrity  # noqa: E402
from app.db import SessionLocal  # noqa: E402
from app.models import IntegrityManifest  # noqa: E402


def opens(blob: bytes, key_hex: str) -> bool:
    try:
        nacl.secret.SecretBox(bytes.fromhex(key_hex)).decrypt(blob)
        return True
    except (nacl.exceptions.CryptoError, ValueError):
        return False


def main() -> int:
    enc = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE.parent.parent / "rds.enc"
    blob = enc.read_bytes()
    print(f"arquivo: {enc} ({len(blob)} bytes)")
    db = SessionLocal()
    try:
        for row in db.query(IntegrityManifest).order_by(IntegrityManifest.version):
            stored = bool(row.dll_release_key) and opens(blob, row.dll_release_key)
            derived = opens(blob, integrity.derive_dll_key_for_version(row.version))
            print(f"  versao {row.version:>5}: chave gravada abre={stored}  chave derivada abre={derived}")
    finally:
        db.close()
    bundle = HERE / "bundle" / "dll_key.txt"
    if bundle.is_file():
        print(f"  bundle/dll_key.txt abre={opens(blob, bundle.read_text().strip())}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
