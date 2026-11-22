"""Regressao da auditoria de seguranca (painel admin + servidor)."""
import pytest

from app import crypto, totp
from app.db import SessionLocal
from app.models import User
from app.users import create_user_with_license
from helpers import INTEG

PW, CPF, FP = "uma-senha-forte-123", "12345678909", "9" * 64


def _mk(email, admin=False):
    db = SessionLocal()
    p = create_user_with_license(db, email, PW, CPF, is_admin=admin)
    db.commit()
    out = {"id": p.user.id, "secret": p.totp_secret, "email": email}
    db.close()
    return out


def _admin_login(client, a):
    r = client.post("/admin/auth/login", json={"email": a["email"], "password": PW, "cpf": CPF,
                                               "totp": totp.code_at(a["secret"])})
    assert r.status_code == 200, r.text
    return r.json()["admin_token"]


@pytest.mark.parametrize("path", [
    "/admin/assets/..%2f..%2f..%2fREADME.md",
    "/admin/assets/%2e%2e/%2e%2e/%2e%2e/README.md",
    "/admin/assets/..%5c..%5c..%5cREADME.md",
    "/admin/assets/../../../README.md",
])
def test_admin_assets_path_traversal_blocked(client, path):
    r = client.get(path)
    assert r.status_code in (404, 400), (path, r.status_code)
    assert "Structura Licensing" not in r.text


def test_security_headers_and_no_store(client):
    r = client.get("/health")
    assert r.headers["x-content-type-options"] == "nosniff"
    assert r.headers["x-frame-options"] == "DENY"
    assert r.headers["cache-control"] == "no-store"
    adm = client.get("/admin/")
    assert "frame-ancestors 'none'" in adm.headers["content-security-policy"]
    assert "script-src 'self'" in adm.headers["content-security-policy"]


def test_docs_disabled_by_default(client):
    for p in ("/docs", "/redoc", "/openapi.json"):
        assert client.get(p).status_code == 404


def test_admin_endpoints_require_token(client):
    for method, path in [("get", "/admin/users"), ("get", "/admin/metrics"), ("get", "/admin/licenses"),
                         ("post", "/admin/users/1/unlock"), ("delete", "/admin/devices/1")]:
        assert getattr(client, method)(path).status_code == 401, path


def test_admin_token_invalid_after_password_change(client):
    a = _mk("adm@exemplo.com", admin=True)
    tok = _admin_login(client, a)
    h = {"Authorization": f"Bearer {tok}"}
    assert client.get("/admin/metrics", headers=h).status_code == 200
    db = SessionLocal()
    u = db.get(User, a["id"])
    u.password_hash = crypto.hash_password("outra-senha-nova-1")
    db.commit()
    db.close()
    assert client.get("/admin/metrics", headers=h).status_code == 401


def test_non_admin_cannot_use_admin_api(client):
    u = _mk("user@exemplo.com")
    # token admin forjado com chave de outro dominio (token de licenca) nao vale
    r = client.post("/auth/login", json={"email": u["email"], "password": PW, "cpf": CPF, "fingerprint": FP,
                                          "integrity": dict(INTEG), "totp": totp.code_at(u["secret"])})
    lic_tok = r.json()["license_token"]
    assert client.get("/admin/users", headers={"Authorization": f"Bearer {lic_tok}"}).status_code == 401


def test_user_search_escapes_like_wildcards(client):
    a = _mk("adm2@exemplo.com", admin=True)
    _mk("zed@exemplo.com")
    h = {"Authorization": f"Bearer {_admin_login(client, a)}"}
    assert len(client.get("/admin/users", params={"search": "%"}, headers=h).json()) == 0
    assert len(client.get("/admin/users", params={"search": "zed"}, headers=h).json()) == 1


def test_totp_not_burned_by_integrity_failure_and_no_lockout(client):
    """Causa do 'credenciais incorretas' recorrente: INTEGRITY_FAIL consumia o TOTP; o retry com o mesmo
    codigo virava bad_totp e 5 retries travavam a conta (app e painel)."""
    u = _mk("retry@exemplo.com")
    code = totp.code_at(u["secret"])
    bad = dict(INTEG, files_root="0" * 64)
    body = {"email": u["email"], "password": PW, "cpf": CPF, "fingerprint": FP, "totp": code}
    for _ in range(6):
        r = client.post("/auth/login", json={**body, "integrity": bad})
        assert r.status_code == 409 and r.json()["detail"] == "INTEGRITY_FAIL"
    # mesmo codigo, agora com integridade correta: tem que entrar (e a conta nao pode estar travada)
    r = client.post("/auth/login", json={**body, "integrity": dict(INTEG)})
    assert r.status_code == 200, r.text
    # depois de usado com sucesso, o replay do mesmo codigo e recusado
    client.post("/auth/logout", json={"license_token": r.json()["license_token"]})
    r2 = client.post("/auth/login", json={**body, "integrity": dict(INTEG)})
    assert r2.status_code == 401


def test_admin_unlock_endpoint(client):
    a = _mk("adm3@exemplo.com", admin=True)
    victim = _mk("lock@exemplo.com")
    db = SessionLocal()
    from app.timeutil import utcnow
    from datetime import timedelta
    v = db.get(User, victim["id"])
    v.locked_until = utcnow() + timedelta(minutes=10)
    v.failed_logins = 3
    db.commit()
    db.close()
    h = {"Authorization": f"Bearer {_admin_login(client, a)}"}
    assert client.post(f"/admin/users/{victim['id']}/unlock", headers=h).status_code == 200
    db = SessionLocal()
    v = db.get(User, victim["id"])
    assert v.locked_until is None and v.failed_logins == 0
    db.close()
