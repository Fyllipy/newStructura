"""Cria ou atualiza o usuario administrador com 2FA pre-inscrito.

Nenhuma credencial fica no codigo-fonte. Entradas, em ordem de precedencia:
  e-mail : --email | STRUCTURA_ADMIN_EMAIL | prompt
  CPF    : --cpf   | STRUCTURA_ADMIN_CPF   | prompt
  senha  : STRUCTURA_ADMIN_PASSWORD | prompt oculto (getpass)   [NUNCA por argv: vaza no historico/ps]

Uso:
  python -m app.create_admin --email admin@exemplo.com
"""
from __future__ import annotations
import argparse
import getpass
import os
import sys
from pathlib import Path

# Garante inclusao do modulo server no sys.path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from app.db import Base, engine, SessionLocal
from app.models import User
from app.users import create_user_with_license
from app import crypto, totp
from app.identity import normalize_cpf, normalize_email, is_valid_cpf, is_valid_email

ADMIN_MIN_PASSWORD_LEN = 7


class AdminInputError(ValueError):
    pass


def _validate(email: str, cpf: str, password: str) -> None:
    if not is_valid_email(normalize_email(email)):
        raise AdminInputError("e-mail invalido")
    if not is_valid_cpf(cpf):
        raise AdminInputError("CPF invalido")
    if len(password) < ADMIN_MIN_PASSWORD_LEN or len(password) > 256:
        raise AdminInputError(f"a senha do administrador deve ter ao menos {ADMIN_MIN_PASSWORD_LEN} caracteres")


def create_or_update_admin(email: str, cpf: str, password: str) -> dict:
    _validate(email, cpf, password)
    Base.metadata.create_all(bind=engine)
    db = SessionLocal()
    try:
        norm_email = normalize_email(email)
        norm_cpf = normalize_cpf(cpf)
        existing = db.query(User).filter(User.email == norm_email).first()

        secret = totp.gen_secret()
        uri = totp.otpauth_uri(secret, norm_email)

        if existing:
            existing.password_hash = crypto.hash_password(password)
            existing.cpf_enc = crypto.cpf_encrypt(norm_cpf)
            existing.cpf_hmac = crypto.cpf_hmac(norm_cpf)
            existing.totp_secret_enc = crypto.totp_encrypt(secret)
            existing.totp_enabled = True
            existing.totp_last_step = 0
            existing.failed_logins = 0
            existing.locked_until = None
            existing.is_admin = True
            existing.status = "ativa"
            db.commit()
            user_id = existing.id
            action = "atualizado"
        else:
            prov = create_user_with_license(
                db=db, email=norm_email, password=password, cpf=norm_cpf,
                days=3650, enroll_2fa=True, is_admin=True,
            )
            secret = prov.totp_secret
            uri = prov.totp_uri
            db.commit()
            user_id = prov.user.id
            action = "criado"

        return {"action": action, "user_id": user_id, "email": norm_email,
                "totp_secret": secret, "otpauth_uri": uri}
    finally:
        db.close()


def _ask(value: str | None, env: str, prompt: str, secret: bool = False) -> str:
    value = value or os.environ.get(env)
    if value:
        return value
    if not sys.stdin.isatty():
        raise AdminInputError(f"informe {env} (entrada nao interativa)")
    return (getpass.getpass(prompt) if secret else input(prompt)).strip()


def main(argv=None):
    parser = argparse.ArgumentParser(description="Criar/Atualizar usuario administrador do Structura.")
    parser.add_argument("--email", default=None, help="E-mail do administrador")
    parser.add_argument("--cpf", default=None, help="CPF do administrador")
    args = parser.parse_args(argv)
    try:
        email = _ask(args.email, "STRUCTURA_ADMIN_EMAIL", "E-mail do admin: ")
        cpf = _ask(args.cpf, "STRUCTURA_ADMIN_CPF", "CPF do admin: ")
        password = os.environ.get("STRUCTURA_ADMIN_PASSWORD")
        if not password:
            password = _ask(None, "STRUCTURA_ADMIN_PASSWORD", "Senha do admin (oculta): ", secret=True)
            if sys.stdin.isatty() and getpass.getpass("Repita a senha: ") != password:
                raise AdminInputError("as senhas nao conferem")
        res = create_or_update_admin(email, cpf, password)
    except AdminInputError as e:
        print(f"ERRO: {e}", file=sys.stderr)
        return 2

    print("=" * 71)
    print(f"USUARIO ADMINISTRADOR {res['action'].upper()} COM SUCESSO")
    print("=" * 71)
    print(f"  User ID : {res['user_id']}")
    print(f"  E-mail  : {res['email']}")
    print("  Admin   : Sim | 2FA ativo: Sim")
    print("-" * 71)
    print("  Cadastre o segredo abaixo no autenticador AGORA (nao sera exibido de novo):")
    print(f"  TOTP Secret (Base32) : {res['totp_secret']}")
    print(f"  otpauth URI          : {res['otpauth_uri']}")
    print("=" * 71)
    return 0


if __name__ == "__main__":
    sys.exit(main())
