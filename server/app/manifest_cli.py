"""Publica/revoga manifestos no servidor.  Uso:
  python -m app.manifest_cli publish --text manifest.txt --sig manifest.sig [--status current|allowed]
  python -m app.manifest_cli set-status --version 6 --status revoked
  python -m app.manifest_cli list
So aceita manifesto assinado pela chave de release (RELEASE_PUBKEY_B64)."""
from __future__ import annotations
import argparse
import sys
from pathlib import Path

from .config import settings
from .db import Base, SessionLocal, engine
from . import integrity, manifest as mf
from . import models  # noqa: F401


def main(argv=None) -> int:
    settings.validate_secrets()
    Base.metadata.create_all(bind=engine)
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("publish")
    p.add_argument("--text", required=True)
    p.add_argument("--sig", required=True)
    p.add_argument("--status", default="current")
    p.add_argument("--dll-key", help="Chave hex de 32 bytes (64 chars); se omitida, e derivada deterministicamente")
    p.add_argument("--overwrite", action="store_true", help="Atualiza manifesto ja publicado para a mesma versao")
    s = sub.add_parser("set-status")
    s.add_argument("--version", type=int, required=True)
    s.add_argument("--status", required=True)
    sub.add_parser("list")
    a = ap.parse_args(argv)
    if a.cmd == "publish" and a.dll_key is not None:
        import re
        if not re.fullmatch(r"[0-9a-fA-F]{64}", a.dll_key.strip()):
            # Antes, um --dll-key vazio era ignorado em silencio e ficava a chave antiga (erro dificil de achar).
            print("ERRO: --dll-key invalida (esperado 64 caracteres hexadecimais). Nada foi publicado.", file=sys.stderr)
            return 2
        a.dll_key = a.dll_key.strip().lower()
    db = SessionLocal()
    try:
        if a.cmd == "publish":
            text = Path(a.text).read_text(encoding="utf-8")
            sig = Path(a.sig).read_text(encoding="ascii").strip()
            m = integrity.publish(db, text, sig, a.status, dll_key=a.dll_key, overwrite=a.overwrite)
            db.commit()
            print(f"publicado v{m.version} ({a.status}): {m.file_count} arquivos")
        elif a.cmd == "set-status":
            ok = integrity.set_status(db, a.version, a.status)
            db.commit()
            print("ok" if ok else "versao inexistente")
            return 0 if ok else 1
        else:
            for v, st in integrity.list_versions(db):
                print(v, st)
        return 0
    except mf.ManifestError as e:
        db.rollback()
        print(f"ERRO: {e}", file=sys.stderr)
        return 1
    finally:
        db.close()


if __name__ == "__main__":
    sys.exit(main())
