"""Cria um usuario comum com licenca. O 2FA NAO e pre-cadastrado: no primeiro login o app abre o
assistente (QR Code) para o proprio usuario configurar o autenticador.

Entradas: --email, --cpf, --days (licenca) e a senha inicial (STRUCTURA_USER_PASSWORD ou prompt oculto).
O CPF fica guardado (cifrado) na conta; nao e usado no login.

Uso:  python -m app.create_user --email cliente@exemplo.com --cpf 12345678909 --days 30
"""
from __future__ import annotations
import argparse
import getpass
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from app.db import Base, SessionLocal, engine
from app.users import ProvisionError, create_user_with_license


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Cria usuario + licenca (2FA configurado no primeiro login).")
    ap.add_argument("--email", required=True)
    ap.add_argument("--cpf", required=True)
    ap.add_argument("--days", type=int, default=30)
    a = ap.parse_args(argv)
    password = os.environ.get("STRUCTURA_USER_PASSWORD")
    if not password:
        if not sys.stdin.isatty():
            print("ERRO: defina STRUCTURA_USER_PASSWORD (entrada nao interativa).", file=sys.stderr)
            return 2
        password = getpass.getpass("Senha inicial do usuario (oculta): ")
        if getpass.getpass("Repita a senha: ") != password:
            print("ERRO: as senhas nao conferem.", file=sys.stderr)
            return 2
    Base.metadata.create_all(bind=engine)
    db = SessionLocal()
    try:
        p = create_user_with_license(db, a.email, password, a.cpf, days=a.days, enroll_2fa=False)
        db.commit()
        print(f"Usuario {p.user.id} criado ({p.user.email}); licenca {p.license.id} por {a.days} dias.")
        print("No primeiro login o app mostra o assistente de 2FA (QR Code).")
        return 0
    except ProvisionError as e:
        db.rollback()
        print(f"ERRO: {e}", file=sys.stderr)
        return 1
    finally:
        db.close()


if __name__ == "__main__":
    sys.exit(main())
