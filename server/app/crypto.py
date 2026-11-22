from __future__ import annotations
import base64
import json
import time
import os
import hmac
import hashlib
from typing import Any

from nacl.signing import SigningKey, VerifyKey
from argon2 import PasswordHasher
from argon2.exceptions import VerifyMismatchError, InvalidHashError, VerificationError
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes

from .config import settings

# Argon2id (argon2-cffi usa o perfil RFC 9106 por padrao).
_ph = PasswordHasher()

# Separacao de dominio: uma assinatura valida num contexto nunca vale em outro.
DOM_TOKEN = b"structura-token-v1\x00"
DOM_KEYSTONE = b"structura-keystone-v1\x00"
DOM_MANIFEST = b"structura-manifest-v1\x00"
DOM_GATE = b"structura-gate-v1\x00"
DOM_ADMIN_TOKEN = b"structura-admin-token-v1\x00"


def _b64e(b: bytes) -> str:
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode()


def _b64d(s: str) -> bytes:
    return base64.urlsafe_b64decode(s + "=" * (-len(s) % 4))


def _require(name: str, value: str) -> str:
    if not value:
        raise RuntimeError(f"{name} ausente no .env — rode `python gen_keys.py`.")
    return value


# ---------------- chave de assinatura do servidor (Ed25519) ----------------
def _signing_key() -> SigningKey:
    return SigningKey(_b64d(_require("SERVER_SIGNING_SEED_B64", settings.SERVER_SIGNING_SEED_B64)))


def server_pubkey_b64() -> str:
    return _b64e(bytes(_signing_key().verify_key))


def sign_blob(data: bytes, domain: bytes) -> str:
    return _b64e(_signing_key().sign(domain + data).signature)


def verify_blob(data: bytes, sig_b64: str, domain: bytes) -> bool:
    try:
        VerifyKey(bytes(_signing_key().verify_key)).verify(domain + data, _b64d(sig_b64))
        return True
    except Exception:
        return False


# ---------------- token de licenca (JSON assinado com exp) ----------------
def issue_token(payload: dict[str, Any], ttl_seconds: int) -> tuple[str, int]:
    body = dict(payload)
    now = int(time.time())
    exp = now + ttl_seconds
    body.update({"typ": "license", "iat": now, "exp": exp})
    raw = json.dumps(body, separators=(",", ":"), sort_keys=True).encode()
    return _b64e(raw) + "." + sign_blob(raw, DOM_TOKEN), exp


def verify_token(token: str, allow_expired: bool = False) -> dict[str, Any] | None:
    try:
        if not isinstance(token, str) or len(token) > 2048:
            return None
        b, sig = token.split(".", 1)
        raw = _b64d(b)
        if not verify_blob(raw, sig, DOM_TOKEN):
            return None
        payload = json.loads(raw)
        if payload.get("typ") != "license":
            return None
        if not allow_expired and int(payload.get("exp", 0)) < int(time.time()):
            return None
        return payload
    except Exception:
        return None


# ---------------- token administrativo (M7: 2FA obrigatorio) ----------------
def password_version(password_hash: str) -> str:
    """Impressao curta do hash da senha: trocar a senha invalida todos os tokens admin emitidos antes."""
    return hashlib.sha256(password_hash.encode()).hexdigest()[:16]


def issue_admin_token(user_id: int, email: str, ttl_seconds: int = 3600, pwv: str = "") -> tuple[str, int]:
    now = int(time.time())
    exp = now + ttl_seconds
    body = {"typ": "admin", "user_id": user_id, "email": email, "iat": now, "exp": exp, "pwv": pwv}
    raw = json.dumps(body, separators=(",", ":"), sort_keys=True).encode()
    return _b64e(raw) + "." + sign_blob(raw, DOM_ADMIN_TOKEN), exp


def verify_admin_token(token: str) -> dict[str, Any] | None:
    try:
        if not isinstance(token, str) or len(token) > 2048:
            return None
        b, sig = token.split(".", 1)
        raw = _b64d(b)
        if not verify_blob(raw, sig, DOM_ADMIN_TOKEN):
            return None
        payload = json.loads(raw)
        if payload.get("typ") != "admin":
            return None
        if int(payload.get("exp", 0)) < int(time.time()):
            return None
        return payload
    except Exception:
        return None


# ---------------- senhas ----------------
def hash_password(pw: str) -> str:
    return _ph.hash(pw)


def verify_password(hashed: str, pw: str) -> bool:
    try:
        return _ph.verify(hashed, pw)
    except (VerifyMismatchError, InvalidHashError, VerificationError):
        return False


_DUMMY_HASH = _ph.hash("dummy-password-for-timing")


def burn_password_time(pw: str) -> None:
    """Gasta o mesmo tempo de uma verificacao real (usuario inexistente -> sem oraculo de timing)."""
    verify_password(_DUMMY_HASH, pw)


# ---------------- keystone K ----------------
def keystone_digest(model_hash: str, result_digest: str, nonce: str,
                    license_id: int, exp: int, ts: int) -> bytes:
    ctx = "|".join([model_hash, result_digest, nonce, str(license_id), str(exp), str(ts)]).encode()
    return hashlib.sha256(ctx).digest()


def sign_keystone(model_hash: str, result_digest: str, nonce: str,
                  license_id: int, exp: int, ts: int) -> str:
    return sign_blob(keystone_digest(model_hash, result_digest, nonce, license_id, exp, ts), DOM_KEYSTONE)


# ---------------- passe do Nivel B (/license/check) ----------------
def gate_pass_digest(feature: str, license_id: int, nonce: str, exp: int) -> bytes:
    return hashlib.sha256("|".join([feature, str(license_id), nonce, str(exp)]).encode()).digest()


def sign_gate_pass(feature: str, license_id: int, nonce: str, exp: int) -> str:
    return sign_blob(gate_pass_digest(feature, license_id, nonce, exp), DOM_GATE)


# ---------------- CPF em repouso ----------------
def cpf_hmac(cpf: str) -> str:
    key = _b64d(_require("CPF_HMAC_KEY_B64", settings.CPF_HMAC_KEY_B64))
    return hmac.new(key, cpf.encode(), hashlib.sha256).hexdigest()


def cpf_encrypt(cpf: str) -> str:
    key = _b64d(_require("CPF_ENC_KEY_B64", settings.CPF_ENC_KEY_B64))
    nonce = os.urandom(12)
    ct = AESGCM(key).encrypt(nonce, cpf.encode(), b"cpf")
    return _b64e(nonce + ct)


def cpf_decrypt(blob: str) -> str:
    key = _b64d(_require("CPF_ENC_KEY_B64", settings.CPF_ENC_KEY_B64))
    raw = _b64d(blob)
    return AESGCM(key).decrypt(raw[:12], raw[12:], b"cpf").decode()


# ---------------- segredo TOTP em repouso (subchave HKDF, AAD propria) ----------------
def _totp_key() -> bytes:
    master = _b64d(_require("CPF_ENC_KEY_B64", settings.CPF_ENC_KEY_B64))
    return HKDF(algorithm=hashes.SHA256(), length=32, salt=None, info=b"structura-totp-v1").derive(master)


def totp_encrypt(secret: str) -> str:
    nonce = os.urandom(12)
    return _b64e(nonce + AESGCM(_totp_key()).encrypt(nonce, secret.encode(), b"totp"))


def totp_decrypt(blob: str) -> str:
    raw = _b64d(blob)
    return AESGCM(_totp_key()).decrypt(raw[:12], raw[12:], b"totp").decode()


def sha256_hex(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()
