"""Provisionamento de usuarios/licencas (usado por seed e, no M7, pelo admin)."""
from __future__ import annotations
from dataclasses import dataclass
from datetime import datetime, timedelta
from sqlalchemy.orm import Session

from . import crypto, totp
from .identity import normalize_cpf, normalize_email, is_valid_cpf, is_valid_email
from .config import settings
from .models import User, License
from .timeutil import utcnow



class ProvisionError(ValueError):
    pass


@dataclass
class Provisioned:
    user: User
    license: License
    totp_secret: str | None      # mostrado UMA vez ao provisionar
    totp_uri: str | None


def create_user_with_license(db: Session, email: str, password: str, cpf: str,
                             days: int = 30, enroll_2fa: bool = True,
                             is_admin: bool = False, must_change_password: bool = False,
                             plan: str = "mensal") -> Provisioned:
    email = normalize_email(email)
    if not is_valid_email(email):
        raise ProvisionError("email invalido")
    if not is_valid_cpf(cpf):
        raise ProvisionError("cpf invalido")
    if len(password) < settings.MIN_PASSWORD_LEN or len(password) > 256:
        raise ProvisionError("senha fora da politica")
    cpf_n = normalize_cpf(cpf)
    secret = totp.gen_secret() if enroll_2fa else None
    user = User(
        email=email,
        password_hash=crypto.hash_password(password),
        cpf_enc=crypto.cpf_encrypt(cpf_n),
        cpf_hmac=crypto.cpf_hmac(cpf_n),
        totp_secret_enc=crypto.totp_encrypt(secret) if secret else None,
        totp_enabled=bool(secret),
        status="ativa",
        is_admin=is_admin,
        must_change_password=must_change_password,
    )
    db.add(user)
    db.flush()
    lic = License(user_id=user.id, status="ativa",
                  current_period_end=utcnow() + timedelta(days=days), plan=plan)
    db.add(lic)
    db.flush()
    return Provisioned(user, lic, secret, totp.otpauth_uri(secret, email) if secret else None)


def generate_temp_password() -> str:
    """Senha temporaria legivel (sem caracteres ambiguos), ~60 bits de entropia. Mostrada UMA vez ao admin."""
    import secrets
    alphabet = "abcdefghjkmnpqrstuvwxyzABCDEFGHJKMNPQRSTUVWXYZ23456789"
    return "".join(secrets.choice(alphabet) for _ in range(12))
