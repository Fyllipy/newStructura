"""Regressao da auditoria: contrato com o cliente C++ e endurecimentos de autenticacao.

Causa raiz do 'e-mail, CPF ou senha incorretos' com credenciais corretas: o cliente exige
`ok` (e `license_id`/`user_id`) na resposta 200 do login; o servidor nao enviava, e o cliente
convertia o sucesso em LOGIN_REJECTED -> mensagem de credenciais invalidas."""
import base64
import json

import pytest

from app import crypto, ratelimit, totp
from app.config import settings
from app.db import SessionLocal
from app.users import create_user_with_license
from helpers import INTEG

EMAIL, PW, CPF = "admin@exemplo.com", "uma-senha-forte-123", "12345678909"
FP = "e" * 64


@pytest.fixture()
def acct(client):
    db = SessionLocal()
    p = create_user_with_license(db, EMAIL, PW, CPF)
    db.commit()
    out = {"user_id": p.user.id, "license_id": p.license.id, "secret": p.totp_secret}
    db.close()
    return out


def _body(acct, **over):
    b = {"email": EMAIL, "password": PW, "cpf": CPF, "fingerprint": FP, "integrity": dict(INTEG),
         "totp": totp.code_at(acct["secret"])}
    b.update(over)
    return b


def test_login_response_matches_client_contract(client, acct):
    r = client.post("/auth/login", json=_body(acct))
    assert r.status_code == 200
    j = r.json()
    assert j["ok"] is True
    assert j["license_id"] == acct["license_id"] and j["user_id"] == acct["user_id"]
    assert j["license_token"] and j["expires_at"]
    # o id exposto confere com o do token assinado (cliente deriva s/K a partir dele)
    p = crypto.verify_token(j["license_token"])
    assert p["license_id"] == j["license_id"] and p["user_id"] == j["user_id"]


def test_heartbeat_response_matches_client_contract(client, acct):
    tok = client.post("/auth/login", json=_body(acct)).json()["license_token"]
    r = client.post("/license/heartbeat", json={"license_token": tok, "fingerprint": FP, "integrity": INTEG})
    assert r.status_code == 200
    j = r.json()
    assert j["ok"] is True and j["license_id"] == acct["license_id"] and j["license_token"]


def test_empty_totp_string_is_not_a_422(client, acct):
    """Formulario com 2FA vazio enviava "totp":"" -> 422 INVALID_REQUEST. Agora cai no fluxo normal."""
    r = client.post("/auth/login", json=_body(acct, totp=""))
    assert r.status_code == 401 and r.json()["detail"] == "TOTP_REQUIRED"


def test_login_with_dll_key_does_not_crash(client, acct):
    """Regressao: `integrity` nao era importado em main.py (NameError -> 500 quando a linha do
    manifesto nao tinha dll_release_key)."""
    from app.models import IntegrityManifest
    db = SessionLocal()
    row = db.get(IntegrityManifest, INTEG["manifest_version"])
    row.dll_release_key = None
    db.commit()
    db.close()
    r = client.post("/auth/login", json=_body(acct))
    assert r.status_code == 200
    assert len(r.json()["dll_key"]) == 64


def test_x_forwarded_for_ignored_by_default(client, acct, monkeypatch):
    monkeypatch.setattr(settings, "TRUST_PROXY_HEADERS", False)
    from app.main import _extract_ip

    class R:
        headers = {"x-forwarded-for": "6.6.6.6"}
        class client:  # noqa: N801
            host = "10.0.0.5"
    assert _extract_ip(R()) == "10.0.0.5"
    monkeypatch.setattr(settings, "TRUST_PROXY_HEADERS", True)
    R.headers = {"x-forwarded-for": "6.6.6.6, 9.9.9.9"}
    assert _extract_ip(R()) == "9.9.9.9"   # salto anexado pelo proxy, nao o forjavel


def test_per_ip_failure_limit(client, acct, monkeypatch):
    monkeypatch.setattr(settings, "AUTH_FAIL_LIMIT_PER_IP", 3)
    ratelimit.reset()
    for i in range(3):   # emails inexistentes: nao travam conta alguma, mas contam por IP
        r = client.post("/auth/login", json=_body(acct, email=f"x{i}@exemplo.com"))
        assert r.status_code == 401
    r = client.post("/auth/login", json=_body(acct))   # ate credenciais corretas: IP bloqueado
    assert r.status_code == 429 and r.json()["detail"] == "TOO_MANY_ATTEMPTS"


def test_create_admin_has_no_hardcoded_defaults_and_enforces_policy(client, monkeypatch):
    from app import create_admin
    assert not hasattr(create_admin, "DEFAULT_PASSWORD") and not hasattr(create_admin, "DEFAULT_CPF")
    with pytest.raises(create_admin.AdminInputError):
        create_admin.create_or_update_admin("a@exemplo.com", CPF, "123456")         # 6 < minimo 7
    with pytest.raises(create_admin.AdminInputError):
        create_admin.create_or_update_admin("a@exemplo.com", "11111111111", "Senha-longa-123")
    res = create_admin.create_or_update_admin("a@exemplo.com", CPF, "Senha-longa-123")
    assert res["action"] == "criado" and "password" not in res and res["totp_secret"]


def test_token_verifiable_like_the_cpp_client(client, acct):
    """Reproduz byte a byte a verificacao de VerifyLicenseTokenSignature (client/license_client.cpp):
    blob = "structura-token-v1\0" || payload ; Ed25519 com a chave publica do servidor."""
    from nacl.signing import VerifyKey
    tok = client.post("/auth/login", json=_body(acct)).json()["license_token"]
    b64, sig = tok.split(".", 1)
    pad = lambda s: s + "=" * (-len(s) % 4)  # noqa: E731
    payload = base64.urlsafe_b64decode(pad(b64))
    pub = base64.urlsafe_b64decode(pad(client.get("/pubkey").json()["server_pubkey_b64"]))
    VerifyKey(pub).verify(b"structura-token-v1\x00" + payload, base64.urlsafe_b64decode(pad(sig)))
    assert json.loads(payload)["license_id"] == acct["license_id"]
    # token adulterado nao verifica
    from nacl.exceptions import BadSignatureError
    with pytest.raises(BadSignatureError):
        VerifyKey(pub).verify(b"structura-token-v1\x00" + payload.replace(b'"user_id":', b'"user_id":9'),
                              base64.urlsafe_b64decode(pad(sig)))


def test_password_policy_min_7(client):
    from app.users import create_user_with_license, ProvisionError
    db = SessionLocal()
    with pytest.raises(ProvisionError):
        create_user_with_license(db, "p6@exemplo.com", "abc123", CPF)
    create_user_with_license(db, "p7@exemplo.com", "abc1234", CPF)
    db.rollback()
    db.close()


def test_health_endpoint(client):
    r = client.get("/health")
    assert r.status_code == 200 and r.json() == {"status": "ok", "db": "ok"}
