"""TOTP RFC 6238 (SHA-1, 6 digitos, 30 s) sem dependencia externa. Com anti-replay por passo."""
from __future__ import annotations
import base64
import hashlib
import hmac
import os
import struct
import time
from urllib.parse import quote

PERIOD = 30
DIGITS = 6


def gen_secret() -> str:
    return base64.b32encode(os.urandom(20)).decode().rstrip("=")


def _hotp(secret_b32: str, counter: int) -> str:
    key = base64.b32decode(secret_b32 + "=" * (-len(secret_b32) % 8))
    mac = hmac.new(key, struct.pack(">Q", counter), hashlib.sha1).digest()
    o = mac[-1] & 0x0F
    code = (struct.unpack(">I", mac[o:o + 4])[0] & 0x7FFFFFFF) % (10 ** DIGITS)
    return str(code).zfill(DIGITS)


def code_at(secret_b32: str, at: float | None = None) -> str:
    return _hotp(secret_b32, int((time.time() if at is None else at) // PERIOD))


def verify(secret_b32: str, code: str, last_step: int = 0, window: int = 1,
           at: float | None = None) -> int | None:
    """Retorna o passo aceito (para gravar como last_step) ou None. Rejeita reuso (step <= last_step)."""
    if not (isinstance(code, str) and len(code) == DIGITS and code.isascii() and code.isdigit()):
        return None
    cur = int((time.time() if at is None else at) // PERIOD)
    hit = None
    for step in range(cur - window, cur + window + 1):  # sem early-exit: tempo constante
        if hmac.compare_digest(_hotp(secret_b32, step), code) and step > last_step:
            hit = step if hit is None else max(hit, step)
    return hit


def otpauth_uri(secret_b32: str, account: str, issuer: str = "Structura") -> str:
    return (f"otpauth://totp/{quote(issuer)}:{quote(account)}?secret={secret_b32}"
            f"&issuer={quote(issuer)}&algorithm=SHA1&digits={DIGITS}&period={PERIOD}")
