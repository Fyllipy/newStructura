"""Destrava uma conta bloqueada por tentativas falhas (uso local no servidor).
Uso: python -m app.unlock_user admin@exemplo.com"""
from __future__ import annotations
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from app.db import SessionLocal
from app.identity import normalize_email
from app.models import User, AuditLog


def main(argv=None) -> int:
    args = sys.argv[1:] if argv is None else argv
    if len(args) != 1:
        print("uso: python -m app.unlock_user <email>", file=sys.stderr)
        return 2
    db = SessionLocal()
    try:
        u = db.query(User).filter(User.email == normalize_email(args[0])).first()
        if u is None:
            print("usuario nao encontrado", file=sys.stderr)
            return 1
        u.failed_logins = 0
        u.locked_until = None
        db.add(AuditLog(actor="cli", action="unlock_user", target=f"user:{u.id}", metadata_masked=""))
        db.commit()
        print(f"conta {u.id} destravada")
        return 0
    finally:
        db.close()


if __name__ == "__main__":
    sys.exit(main())
