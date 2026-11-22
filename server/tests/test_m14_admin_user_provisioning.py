"""Painel: criar usuario+licenca (senha temporaria, CPF, 2FA no 1o login), troca de senha, reset de senha/2FA."""
import pytest

from app import crypto, totp
from app.db import SessionLocal
from app.models import AuditLog, License, User
from app.users import create_user_with_license
from helpers import INTEG

PW, CPF, FP = "uma-senha-forte-123", "12345678909", "7" * 64


def _admin(client):
    db = SessionLocal()
    p = create_user_with_license(db, "adm@exemplo.com", PW, CPF, is_admin=True)
    db.commit()
    secret = p.totp_secret
    db.close()
    r = client.post("/admin/auth/login", json={"email": "adm@exemplo.com", "password": PW, "totp": totp.code_at(secret)})
    return {"Authorization": f"Bearer {r.json()['admin_token']}"}


def _login(client, email, password, code=None):
    b = {"email": email, "password": password, "fingerprint": FP, "integrity": dict(INTEG)}
    if code:
        b["totp"] = code
    return client.post("/auth/login", json=b)


def test_create_user_full_first_access_flow(client):
    h = _admin(client)
    r = client.post("/admin/users", headers=h, json={"email": "Novo@Exemplo.com", "cpf": "529.982.247-25",
                                                      "plan": "anual", "days": 365})
    assert r.status_code == 201, r.text
    j = r.json()
    tmp = j["temporary_password"]
    assert j["email"] == "novo@exemplo.com" and len(tmp) >= 10 and j["must_change_password"] is True

    # CPF guardado cifrado na conta; licenca com o plano pedido
    db = SessionLocal()
    u = db.get(User, j["user_id"])
    assert crypto.cpf_decrypt(u.cpf_enc) == "52998224725" and u.totp_enabled is False and u.must_change_password
    assert db.get(License, j["license_id"]).plan == "anual"
    db.close()

    # 1o login: sem 2FA -> assistente
    r = _login(client, "novo@exemplo.com", tmp)
    assert r.status_code == 403 and r.json()["requires_2fa_setup"] is True
    s = client.post("/auth/2fa/setup", json={"email": "novo@exemplo.com", "password": tmp}).json()
    code = totp.code_at(s["secret"])
    assert client.post("/auth/2fa/confirm", json={"email": "novo@exemplo.com", "password": tmp, "totp": code}).status_code == 204
    r = _login(client, "novo@exemplo.com", tmp, code)
    assert r.status_code == 200 and r.json()["must_change_password"] is True      # app forca a troca
    # troca obrigatoria
    ch = client.post("/auth/password/change", json={"email": "novo@exemplo.com", "password": tmp, "new_password": "minha-nova-senha-9"})
    assert ch.status_code == 204
    client.post("/auth/logout", json={"license_token": r.json()["license_token"]})
    assert _login(client, "novo@exemplo.com", tmp, totp.code_at(s["secret"])).status_code == 401   # senha velha morreu
    r2 = client.post("/auth/login", json={"email": "novo@exemplo.com", "password": "minha-nova-senha-9", "fingerprint": FP,
                                           "integrity": dict(INTEG), "totp": totp.code_at(s["secret"], __import__("time").time() + 30)})
    assert r2.status_code == 200 and r2.json()["must_change_password"] is False


def test_create_user_validations(client):
    h = _admin(client)
    ok = {"email": "a@exemplo.com", "cpf": CPF, "days": 30}
    assert client.post("/admin/users", headers=h, json=ok).status_code == 201
    assert client.post("/admin/users", headers=h, json=ok).status_code == 409                       # e-mail duplicado
    assert client.post("/admin/users", headers=h, json={**ok, "email": "b@exemplo.com", "cpf": "11111111111"}).status_code == 422
    assert client.post("/admin/users", headers=h, json={**ok, "email": "nao-e-email"}).status_code == 422
    assert client.post("/admin/users", headers=h, json={**ok, "email": "c@exemplo.com", "password": "123"}).status_code == 422
    assert client.post("/admin/users", json=ok).status_code == 401                                   # sem token
    r = client.post("/admin/users", headers=h, json={**ok, "email": "d@exemplo.com", "password": "senha-escolhida-1"})
    assert r.json()["temporary_password"] == "senha-escolhida-1"


def test_change_password_requires_current_password_and_difference(client):
    h = _admin(client)
    j = client.post("/admin/users", headers=h, json={"email": "e@exemplo.com", "cpf": CPF}).json()
    tmp = j["temporary_password"]
    assert client.post("/auth/password/change", json={"email": "e@exemplo.com", "password": "errada-errada-1",
                                                      "new_password": "outra-senha-123"}).status_code == 401
    assert client.post("/auth/password/change", json={"email": "e@exemplo.com", "password": tmp,
                                                      "new_password": tmp}).status_code == 422
    assert client.post("/auth/password/change", json={"email": "e@exemplo.com", "password": tmp,
                                                      "new_password": "123"}).status_code == 422


def test_reset_password_and_2fa(client):
    h = _admin(client)
    db = SessionLocal()
    p = create_user_with_license(db, "f@exemplo.com", PW, CPF)
    db.commit()
    uid, secret = p.user.id, p.totp_secret
    db.close()
    r = client.post(f"/admin/users/{uid}/reset-password", headers=h)
    assert r.status_code == 200 and r.json()["temporary_password"] != PW
    assert _login(client, "f@exemplo.com", PW, totp.code_at(secret)).status_code == 401           # senha antiga invalida
    assert client.post(f"/admin/users/{uid}/reset-2fa", headers=h).status_code == 200
    r = _login(client, "f@exemplo.com", r.json()["temporary_password"])
    assert r.status_code == 403 and r.json()["requires_2fa_setup"] is True                        # assistente reaparece
    db = SessionLocal()
    acts = {a.action for a in db.query(AuditLog)}
    assert {"create_user" not in acts, "reset_password" in acts, "reset_2fa" in acts} == {True}
    db.close()
    assert client.post("/admin/users/99999/reset-2fa", headers=h).status_code == 404


def test_admin_2fa_cannot_be_reset_via_panel(client):
    h = _admin(client)
    db = SessionLocal()
    admin_id = db.query(User).filter(User.is_admin.is_(True)).first().id
    db.close()
    assert client.post(f"/admin/users/{admin_id}/reset-2fa", headers=h).status_code == 403
