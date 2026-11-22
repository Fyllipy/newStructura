"""Normalizacao/validacao de identidade (CPF, e-mail). Nunca loga o CPF."""
from __future__ import annotations
import re

_EMAIL = re.compile(r"^[^@\s]{1,64}@[^@\s]{1,189}\.[^@\s]{2,}$")


def normalize_email(email: str) -> str:
    return email.strip().lower()


def is_valid_email(email: str) -> bool:
    return bool(_EMAIL.match(email)) and len(email) <= 254


def normalize_cpf(cpf: str) -> str:
    return re.sub(r"\D", "", cpf or "")


def is_valid_cpf(cpf: str) -> bool:
    d = normalize_cpf(cpf)
    if len(d) != 11 or d == d[0] * 11:
        return False
    for n in (9, 10):
        s = sum(int(d[i]) * (n + 1 - i) for i in range(n))
        if int(d[n]) != (s * 10 % 11) % 10:
            return False
    return True


def mask_cpf(cpf: str) -> str:
    d = normalize_cpf(cpf)
    return f"***.***.***-{d[-2:]}" if len(d) >= 2 else "***"
