"""M1 — auth e nucleo de licenciamento: login/logout/heartbeat, 2FA, 1 maquina, fail-closed, LGPD."""
import json
import logging
import time
from datetime import timedelta

import pytest
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError

from app import crypto, totp
from app.config import settings
from app.db import SessionLocal
from app.models import AuditLog, Device, License, User
from app.timeutil import utcnow
from app.users import create_user_with_license
from helpers import INTEG

EMAIL, PW, CPF = "Fulano@Exemplo.com", "uma-senha-forte-123", "12345678909"
FP1, FP2 = "c" * 64, "d" * 64


@pytest.fixture()
def acct(client):
    db = SessionLocal()
    p = create_user_with_license(db, EMAIL, PW, CPF)
    db.commit()
    out = {"user_id": p.user.id, "license_id": p.license.id, "secret": p.totp_secret}
    db.close()
    return out


def body(acct=None, fp=FP1, password=PW, cpf=CPF, email=EMAIL, code="auto", secret=None, at=None):
    b = {"email": email, "password": password, "cpf": cpf, "fingerprint": fp,
         "client_version": "1.0.0", "integrity": dict(INTEG)}
    if code == "auto" and (acct or secret):
        code = totp.code_at((acct or {}).get("secret") or secret, at)
    if code and code != "auto":
        b["totp"] = code
    return b


def login(client, acct, **kw):
    return client.post("/auth/login", json=body(acct, **kw))


def hb(client, token, fp=FP1):
    return client.post("/license/heartbeat", json={"license_token": token, "fingerprint": fp, "integrity": INTEG})


def ok_login(client, acct, **kw):
    r = login(client, acct, **kw)
    assert r.status_code == 200, r.text
    return r.json()["license_token"]


def reset_totp_step():
    db = SessionLocal()
    for u in db.scalars(select(User)):
        u.totp_last_step = 0
    db.commit()
    db.close()


def reset_lock():
    db = SessionLocal()
    for u in db.scalars(select(User)):
        u.locked_until = None
        u.failed_logins = 0
    db.commit()
    db.close()


# ================= login basico =================
def test_login_ok_token_signed_and_minimal(client, acct):
    r = login(client, acct)
    assert r.status_code == 200
    tok = r.json()["license_token"]
    p = crypto.verify_token(tok)
    assert p["typ"] == "license" and p["license_id"] == acct["license_id"] and p["fp"] == FP1
    assert p["exp"] - p["iat"] == settings.TOKEN_TTL_SECONDS
    dump = json.dumps(p).lower()
    assert CPF not in dump and "fulano" not in dump  # sem PII no token
    assert r.json()["expires_at"].endswith("Z")


def test_login_accepts_formatted_cpf_and_email_case(client, acct):
    assert login(client, acct, cpf="123.456.789-09", email="  FULANO@exemplo.com ").status_code == 200


def test_generic_errors_do_not_reveal_which_field(client, acct):
    a = login(client, acct, password="errada-errada-1")
    c = login(client, acct, email="naoexiste@x.com")
    assert a.status_code == c.status_code == 401
    assert a.json() == c.json() == {"detail": "CREDENCIAIS"}


def test_unknown_user_burns_password_time(client, monkeypatch):
    called = []
    monkeypatch.setattr(crypto, "burn_password_time", lambda pw: called.append(1))
    import app.main as m
    monkeypatch.setattr(m.crypto, "burn_password_time", lambda pw: called.append(1))
    r = client.post("/auth/login", json=body(secret="JBSWY3DPEHPK3PXP", email="x@y.com"))
    assert r.status_code == 401 and called


# ================= lockout =================
def test_lockout_after_threshold_even_with_correct_credentials(client, acct):
    for _ in range(settings.LOCKOUT_THRESHOLD):
        assert login(client, acct, password="errada-errada-1").status_code == 401
    r = login(client, acct)  # credenciais certas, mas conta bloqueada
    assert r.status_code == 401 and r.json() == {"detail": "CREDENCIAIS"}
    db = SessionLocal()
    u = db.get(User, acct["user_id"])
    u.locked_until = utcnow() - timedelta(seconds=1)  # lockout expira
    db.commit(); db.close()
    assert login(client, acct).status_code == 200


# ================= 2FA =================
def test_totp_rfc6238_vector():
    s = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ"  # "12345678901234567890"
    assert totp.code_at(s, 59) == "287082"
    assert totp.code_at(s, 1111111109) == "081804"


def test_login_requires_totp(client, acct):
    r = login(client, acct, code=None)
    assert r.status_code == 401 and r.json()["detail"] == "TOTP_REQUIRED"


def test_login_rejects_wrong_totp_and_counts_failure(client, acct):
    good = totp.code_at(acct["secret"])
    bad = "000000" if good != "000000" else "111111"
    r = login(client, acct, code=bad)
    assert r.status_code == 401 and r.json()["detail"] == "CREDENCIAIS"
    db = SessionLocal()
    assert db.get(User, acct["user_id"]).failed_logins == 1
    db.close()


def test_totp_replay_rejected(client, acct):
    code = totp.code_at(acct["secret"])
    assert login(client, acct, code=code).status_code == 200
    assert login(client, acct, code=code).status_code == 401       # mesmo codigo, reuso
    nxt = totp.code_at(acct["secret"], time.time() + 30)
    assert login(client, acct, code=nxt).status_code == 200          # proximo passo vale


def test_totp_replay_race_only_one_wins(client, acct):
    """Duas sessoes leram o mesmo last_step (corrida): so o UPDATE condicional passa."""
    import app.main as m
    code = totp.code_at(acct["secret"])
    s1, s2 = SessionLocal(), SessionLocal()
    u1, u2 = s1.get(User, acct["user_id"]), s2.get(User, acct["user_id"])
    assert u1.totp_last_step == u2.totp_last_step == 0
    assert m._check_totp(s1, u1, code) is True
    s1.commit()
    assert m._check_totp(s2, u2, code) is False     # u2 ainda via last_step=0 em memoria
    s1.close(); s2.close()


def test_locked_account_burns_password_time(client, acct, monkeypatch):
    import app.main as m
    for _ in range(settings.LOCKOUT_THRESHOLD):
        login(client, acct, password="errada-errada-1")
    calls = []
    monkeypatch.setattr(m.crypto, "burn_password_time", lambda pw: calls.append(1))
    assert login(client, acct).status_code == 401
    assert calls


def test_totp_old_code_outside_window_rejected(client, acct):
    old = totp.code_at(acct["secret"], time.time() - 120)
    assert login(client, acct, code=old).status_code == 401


def test_totp_secret_encrypted_at_rest(client, acct):
    db = SessionLocal()
    u = db.get(User, acct["user_id"])
    assert acct["secret"] not in u.totp_secret_enc
    assert crypto.totp_decrypt(u.totp_secret_enc) == acct["secret"]
    db.close()


def test_enrollment_flow_and_no_reenroll(client):
    db = SessionLocal()
    create_user_with_license(db, "novo@exemplo.com", PW, CPF, enroll_2fa=False)
    db.commit(); db.close()
    cred = {"email": "novo@exemplo.com", "password": PW, "cpf": CPF}
    r = client.post("/auth/login", json={**cred, "fingerprint": FP1, "integrity": INTEG})
    assert r.status_code == 403 and r.json()["detail"] == "ENROLL_2FA"
    s = client.post("/auth/2fa/setup", json=cred)
    assert s.status_code == 200
    secret = s.json()["secret"]
    assert s.json()["otpauth_uri"].startswith("otpauth://totp/")
    # ainda nao confirmado: login segue negado
    assert client.post("/auth/login", json={**cred, "fingerprint": FP1, "integrity": INTEG,
                                            "totp": totp.code_at(secret)}).status_code == 403
    assert client.post("/auth/2fa/confirm", json={**cred, "totp": "000000"}).status_code == 401
    assert client.post("/auth/2fa/confirm", json={**cred, "totp": totp.code_at(secret)}).status_code == 204
    assert client.post("/auth/2fa/setup", json=cred).status_code == 409   # atacante nao re-inscreve
    nxt = totp.code_at(secret, time.time() + 30)
    r = client.post("/auth/login", json={**cred, "fingerprint": FP1, "integrity": INTEG, "totp": nxt})
    assert r.status_code == 200


def test_setup_requires_valid_credentials(client, acct):
    r = client.post("/auth/2fa/setup", json={"email": EMAIL, "password": "errada-errada-1", "cpf": CPF})
    assert r.status_code == 401


# ================= 1 maquina ativa =================
def test_second_device_rejected_then_logout_frees(client, acct):
    t1 = ok_login(client, acct, fp=FP1)
    r = login(client, acct, fp=FP2, code=totp.code_at(acct["secret"], time.time() + 30))
    assert r.status_code == 409 and r.json()["detail"] == "DEVICE_IN_USE"
    assert client.post("/auth/logout", json={"license_token": t1}).status_code == 204
    reset_totp_step()  # (so o teste: a janela de 30 s so comporta 2 codigos distintos)
    t2 = ok_login(client, acct, fp=FP2)
    assert hb(client, t2, FP2).status_code == 200
    assert hb(client, t1, FP1).status_code == 403   # token antigo morto, mesmo nao expirado


def test_stale_device_released_after_7_days(client, acct):
    t1 = ok_login(client, acct, fp=FP1)
    db = SessionLocal()
    d = db.scalar(select(Device))
    d.last_heartbeat_at = utcnow() - timedelta(days=settings.DEVICE_STALE_DAYS, seconds=5)
    db.commit(); db.close()
    t2 = ok_login(client, acct, fp=FP2, code=totp.code_at(acct["secret"], time.time() + 30))
    assert hb(client, t2, FP2).status_code == 200
    assert hb(client, t1, FP1).status_code == 403
    db = SessionLocal()
    assert len(db.scalars(select(Device).where(Device.active.is_(True))).all()) == 1
    db.close()


def test_not_stale_before_7_days(client, acct):
    ok_login(client, acct, fp=FP1)
    db = SessionLocal()
    d = db.scalar(select(Device))
    d.last_heartbeat_at = utcnow() - timedelta(days=settings.DEVICE_STALE_DAYS - 1)
    db.commit(); db.close()
    r = login(client, acct, fp=FP2, code=totp.code_at(acct["secret"], time.time() + 30))
    assert r.status_code == 409


def test_db_enforces_single_active_device(client, acct):
    ok_login(client, acct)
    db = SessionLocal()
    db.add(Device(license_id=acct["license_id"], fingerprint_hash=FP2, active=True))
    with pytest.raises(IntegrityError):
        db.commit()
    db.close()


# ================= heartbeat / logout / token =================
def test_heartbeat_renews_and_updates(client, acct):
    t1 = ok_login(client, acct)
    r = hb(client, t1)
    assert r.status_code == 200 and r.json()["status"] == "ativa"
    assert crypto.verify_token(r.json()["license_token"])["did"] == crypto.verify_token(t1)["did"]


def test_heartbeat_wrong_fingerprint_rejected(client, acct):
    t1 = ok_login(client, acct)
    assert hb(client, t1, FP2).status_code == 403


def test_tampered_token_rejected(client, acct):
    t = ok_login(client, acct)
    b, sig = t.split(".")
    forged = json.loads(crypto._b64d(b))
    forged["license_id"] = 999
    nb = crypto._b64e(json.dumps(forged, separators=(",", ":"), sort_keys=True).encode())
    assert hb(client, nb + "." + sig).status_code == 403
    tampered_sig = t[:-2] + ("BB" if t.endswith("AA") else "AA")
    assert hb(client, tampered_sig).status_code == 403
    assert hb(client, "lixo.lixo-lixo").status_code == 403


def test_expired_token_rejected_but_logout_still_frees(client, acct):
    t = ok_login(client, acct)
    p = crypto.verify_token(t)
    old, _ = crypto.issue_token({k: p[k] for k in ("license_id", "user_id", "fp", "did")}, -10)
    assert crypto.verify_token(old) is None
    assert hb(client, old).status_code == 403
    assert client.post("/auth/logout", json={"license_token": old}).status_code == 204
    assert hb(client, t).status_code == 403   # vinculo liberado


def test_logout_garbage_is_204_and_harmless(client, acct):
    t = ok_login(client, acct)
    assert client.post("/auth/logout", json={"license_token": "x" * 40}).status_code == 204
    assert hb(client, t).status_code == 200


def test_token_domain_separation(client, acct):
    t = ok_login(client, acct)
    b, sig = t.split(".")
    raw = crypto._b64d(b)
    assert crypto.verify_blob(raw, sig, crypto.DOM_TOKEN)
    assert not crypto.verify_blob(raw, sig, crypto.DOM_KEYSTONE)
    k = crypto.sign_keystone("m" * 64, "r" * 64, "n" * 32, 1, 2, 3)
    assert not crypto.verify_blob(crypto.keystone_digest("m" * 64, "r" * 64, "n" * 32, 1, 2, 3), k, crypto.DOM_TOKEN)


def test_token_without_typ_rejected():
    raw = json.dumps({"exp": int(time.time()) + 100, "license_id": 1}).encode()
    t = crypto._b64e(raw) + "." + crypto.sign_blob(raw, crypto.DOM_TOKEN)
    assert crypto.verify_token(t) is None


# ================= kill switch / estados da licenca =================
@pytest.mark.parametrize("status", ["suspensa", "revogada", "expirada"])
def test_license_state_blocks_immediately(client, acct, status):
    t = ok_login(client, acct)
    db = SessionLocal()
    db.get(License, acct["license_id"]).status = status
    db.commit(); db.close()
    assert hb(client, t).status_code == 403            # efeito imediato, token ainda "valido"
    r = login(client, acct, code=totp.code_at(acct["secret"], time.time() + 30))
    assert r.status_code == 403 and r.json()["detail"] == "LICENSE_INACTIVE"


def test_period_end_in_past_blocks(client, acct):
    t = ok_login(client, acct)
    db = SessionLocal()
    db.get(License, acct["license_id"]).current_period_end = utcnow() - timedelta(seconds=1)
    db.commit(); db.close()
    assert hb(client, t).status_code == 403


def test_user_suspended_blocks(client, acct):
    t = ok_login(client, acct)
    db = SessionLocal()
    db.get(User, acct["user_id"]).status = "suspensa"
    db.commit(); db.close()
    assert hb(client, t).status_code == 403


def test_compute_fail_closed_after_logout(client, acct):
    t = ok_login(client, acct)
    client.post("/auth/logout", json={"license_token": t})
    req = {"license_token": t, "nonce": "ab" * 8, "fingerprint": FP1, "integrity": INTEG,
           "model_hash": "e" * 64, "reaction_totals": {"F": [0, 0, 0], "M_origin": [0, 0, 0]},
           "result_digest": "f" * 64}
    r = client.post("/compute/validate", json=req)
    assert r.status_code == 200 and r.json()["ok"] is False and r.json().get("K") is None


def test_integrity_malformed_rejected(client, acct):
    b = body(acct)
    b["integrity"]["files_root"] = "nao-e-hex"
    assert login(client, acct).status_code in (200, 401)  # (sanidade do helper)
    r = client.post("/auth/login", json=b)
    assert r.status_code == 422


# ================= LGPD: nada de PII =================
def test_validation_error_does_not_echo_secrets(client):
    r = client.post("/auth/login", json={"email": "a@b.com", "password": "SENHA-SECRETA-XYZ",
                                         "cpf": "12345678909"})  # faltam campos
    assert r.status_code == 422
    assert "SENHA-SECRETA-XYZ" not in r.text and "12345678909" not in r.text
    assert r.json() == {"detail": "INVALID_REQUEST"}


def test_no_pii_in_logs_or_audit(client, acct, caplog):
    caplog.set_level(logging.DEBUG)
    login(client, acct, password="errada-errada-1")
    ok_login(client, acct, code=totp.code_at(acct["secret"], time.time() + 30))
    text = caplog.text.lower()
    for pii in (CPF, "123.456", "fulano", PW, "errada-errada"):
        assert pii.lower() not in text
    db = SessionLocal()
    rows = db.scalars(select(AuditLog)).all()
    assert rows
    blob = " ".join(f"{r.actor} {r.action} {r.target} {r.metadata_masked}" for r in rows).lower()
    for pii in (CPF, "fulano", "exemplo.com", PW):
        assert pii.lower() not in blob
    db.close()


def test_cpf_encrypted_at_rest_and_hmac_search(client, acct):
    db = SessionLocal()
    u = db.get(User, acct["user_id"])
    assert CPF not in u.cpf_enc and CPF not in u.cpf_hmac
    assert crypto.cpf_decrypt(u.cpf_enc) == CPF
    assert u.cpf_hmac == crypto.cpf_hmac(CPF)
    assert crypto.cpf_encrypt(CPF) != crypto.cpf_encrypt(CPF)   # nonce aleatorio
    assert db.scalar(select(User).where(User.cpf_hmac == crypto.cpf_hmac(CPF))).id == u.id
    db.close()


def test_cpf_aad_binding():
    blob = crypto.cpf_encrypt(CPF)
    with pytest.raises(Exception):
        crypto.totp_decrypt(blob)   # contexto errado nao decifra


def test_password_is_argon2id(client, acct):
    db = SessionLocal()
    h = db.get(User, acct["user_id"]).password_hash
    assert h.startswith("$argon2id$") and PW not in h
    db.close()


def test_audit_log_is_immutable(client, acct):
    ok_login(client, acct)
    db = SessionLocal()
    row = db.scalar(select(AuditLog))
    row.action = "adulterado"
    with pytest.raises(RuntimeError):
        db.commit()
    db.rollback()
    db.delete(db.scalar(select(AuditLog)))
    with pytest.raises(RuntimeError):
        db.commit()
    db.close()


def test_provisioning_rules():
    from app.users import ProvisionError
    db = SessionLocal()
    with pytest.raises(ProvisionError):
        create_user_with_license(db, "a@b.com", "curta", CPF)
    with pytest.raises(ProvisionError):
        create_user_with_license(db, "a@b.com", PW, "11111111111")   # CPF invalido
    with pytest.raises(ProvisionError):
        create_user_with_license(db, "sem-arroba", PW, CPF)
    db.close()


def test_cpf_validator_and_mask():
    from app.identity import is_valid_cpf, mask_cpf
    assert is_valid_cpf("123.456.789-09") and not is_valid_cpf("123.456.789-10")
    assert mask_cpf("12345678909") == "***.***.***-09"
