"""CPF fora do login, assistente de 1o acesso (2FA) e telemetria antifraude."""
import pytest

from app import crypto, totp
from app.db import SessionLocal
from app.models import TelemetryEvent, User
from app.users import create_user_with_license
from helpers import INTEG

PW, CPF, FP = "uma-senha-forte-123", "12345678909", "8" * 64
TEL = {"app_version": "0.9.0", "os_version": "Windows 11 10.0.26200", "hostname": "PC-JOAO",
       "cpu": "AMD Ryzen 7", "locale": "pt-BR", "timezone": "UTC-03:00"}


def _mk(email, admin=False, enroll=True):
    db = SessionLocal()
    p = create_user_with_license(db, email, PW, CPF, is_admin=admin, enroll_2fa=enroll)
    db.commit()
    out = {"id": p.user.id, "secret": p.totp_secret, "email": email}
    db.close()
    return out


def _login(client, email, code=None, **extra):
    b = {"email": email, "password": PW, "fingerprint": FP, "integrity": dict(INTEG), **extra}
    if code:
        b["totp"] = code
    return client.post("/auth/login", json=b)


def _admin_headers(client):
    a = _mk("adm@exemplo.com", admin=True)
    r = client.post("/admin/auth/login", json={"email": a["email"], "password": PW,
                                               "totp": totp.code_at(a["secret"])})
    assert r.status_code == 200, r.text   # sem CPF
    return {"Authorization": f"Bearer {r.json()['admin_token']}"}


def test_login_without_cpf_and_cpf_stays_registered(client):
    u = _mk("a@exemplo.com")
    assert _login(client, u["email"], totp.code_at(u["secret"])).status_code == 200
    db = SessionLocal()
    row = db.get(User, u["id"])
    assert crypto.cpf_decrypt(row.cpf_enc) == CPF and row.cpf_hmac     # continua cadastrado
    db.close()


def test_legacy_client_sending_cpf_still_works(client):
    u = _mk("b@exemplo.com")
    assert _login(client, u["email"], totp.code_at(u["secret"]), cpf="000.000.000-00").status_code == 200


def test_first_login_wizard_flow(client):
    """Conta sem 2FA: login -> ENROLL_2FA -> setup (QR/segredo) -> confirm -> login com o MESMO codigo."""
    u = _mk("novo@exemplo.com", enroll=False)
    r = _login(client, u["email"])
    assert r.status_code == 403 and r.json()["requires_2fa_setup"] is True
    s = client.post("/auth/2fa/setup", json={"email": u["email"], "password": PW})
    assert s.status_code == 200 and s.json()["otpauth_uri"].startswith("otpauth://totp/")
    secret = s.json()["secret"]
    code = totp.code_at(secret)
    assert client.post("/auth/2fa/confirm", json={"email": u["email"], "password": PW, "totp": code}).status_code == 204
    r = _login(client, u["email"], code)       # mesmo codigo: nao pode cair em replay
    assert r.status_code == 200, r.text
    # setup nao pode ser refeito depois de ativo
    assert client.post("/auth/2fa/setup", json={"email": u["email"], "password": PW}).status_code == 409


def test_wizard_wrong_code_rejected(client):
    u = _mk("novo2@exemplo.com", enroll=False)
    client.post("/auth/2fa/setup", json={"email": u["email"], "password": PW})
    assert client.post("/auth/2fa/confirm", json={"email": u["email"], "password": PW,
                                                  "totp": "000000"}).status_code == 401


def test_telemetry_recorded_on_login_and_failure(client):
    u = _mk("t@exemplo.com")
    assert _login(client, u["email"], totp.code_at(u["secret"]), telemetry=TEL).status_code == 200
    client.post("/auth/login", json={"email": u["email"], "password": "errada-errada-1", "fingerprint": FP,
                                      "integrity": dict(INTEG)})
    client.post("/auth/login", json={"email": "ghost@exemplo.com", "password": "x", "fingerprint": FP,
                                      "integrity": dict(INTEG)})
    db = SessionLocal()
    rows = db.query(TelemetryEvent).order_by(TelemetryEvent.id).all()
    kinds = [(r.event, r.detail) for r in rows]
    assert ("login", "bound") in kinds and ("login_failed", "bad_password") in kinds
    assert ("login_failed", "unknown_user") in kinds
    ok = next(r for r in rows if r.event == "login")
    assert ok.email == "t@exemplo.com" and ok.ip and ok.hostname == "PC-JOAO" and ok.app_version == "0.9.0"
    assert ok.user_id == u["id"] and ok.license_id and ok.fingerprint == FP
    db.close()


def test_telemetry_never_stores_password_or_cpf(client):
    u = _mk("p@exemplo.com")
    _login(client, u["email"], totp.code_at(u["secret"]), cpf=CPF, telemetry=TEL)
    db = SessionLocal()
    blob = " ".join(str(v) for r in db.query(TelemetryEvent) for v in vars(r).values())
    assert PW not in blob and CPF not in blob
    db.close()


def test_admin_telemetry_query_filters_and_signals(client):
    h = _admin_headers(client)
    for i in range(3):    # 3 contas no mesmo IP -> sinal
        u = _mk(f"shared{i}@exemplo.com")
        assert _login(client, u["email"], totp.code_at(u["secret"]), telemetry=TEL).status_code == 200
    r = client.get("/admin/telemetry", params={"email": "shared1"}, headers=h)
    assert r.status_code == 200 and len(r.json()) >= 1 and all("shared1" in e["email"] for e in r.json())
    assert client.get("/admin/telemetry", params={"email": "%"}, headers=h).json() == []
    sig = client.get("/admin/telemetry/signals", headers=h).json()
    assert any(s["kind"] == "ip_many_accounts" and s["count"] >= 3 for s in sig)
    assert client.get("/admin/telemetry").status_code == 401
    assert client.get("/admin/telemetry/signals").status_code == 401


def test_telemetry_purge(client):
    from datetime import timedelta
    from app import telemetry
    from app.timeutil import utcnow
    db = SessionLocal()
    db.add(TelemetryEvent(event="login", ip="1.1.1.1", ts=utcnow() - timedelta(days=9999)))
    db.add(TelemetryEvent(event="login", ip="2.2.2.2"))
    db.commit()
    assert telemetry.purge(db) == 1
    assert db.query(TelemetryEvent).count() == 1
    db.close()


def test_admin_sees_cpf_per_account_masked_and_reveal_is_audited(client):
    from app.models import AuditLog
    h = _admin_headers(client)
    u = _mk("cpfuser@exemplo.com")
    users = client.get("/admin/users", params={"search": "cpfuser"}, headers=h).json()
    assert users[0]["cpf_masked"] == "***.***.***-09" and CPF not in str(users)
    r = client.get(f"/admin/users/{u['id']}/cpf", headers=h)
    assert r.status_code == 200 and r.json()["cpf"] == "123.456.789-09"
    db = SessionLocal()
    assert db.query(AuditLog).filter(AuditLog.action == "reveal_cpf", AuditLog.target == f"user:{u['id']}").count() == 1
    db.close()
    assert client.get(f"/admin/users/{u['id']}/cpf").status_code == 401
    assert client.get("/admin/users/9999/cpf", headers=h).status_code == 404
