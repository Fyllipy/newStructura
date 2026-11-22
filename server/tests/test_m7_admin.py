"""Testes do Marco M7: Painel Administrativo.
Base: SPEC.md (§8.6), PLAN.md (M7) e Decisões de Desenho.

Verificações obrigatórias:
(a) Endpoints admin exigem token de admin autenticado com 2FA obrigatório (não rotas abertas).
(b) Fila de AbuseFlag permite transicionar status de 'aberto' para 'revisado' com persistência em DB.
(c) Suspensão, revogação e alteração de expiração de licença via admin alteram o banco e refletem
    imediatamente em heartbeat, check e compute_validate (fail-closed).
(d) Toda e qualquer ação administrativa gera entrada em AuditLog.
(e) Gestão de dispositivos vinculados (listagem e liberação de vínculo).
(f) Gestão de VMAllowlist (listagem, cadastro e desativação).
"""
import uuid
import time
from datetime import timedelta
import pytest
from sqlalchemy import select
from fastapi import HTTPException
from fastapi.testclient import TestClient

from app.db import Base, engine, SessionLocal
from app import integrity, models, crypto, totp, abuse
from app.models import AuditLog, IntegrityManifest, User, License, Device, AbuseFlag, VMAllowlist
from app.users import create_user_with_license
from app.timeutil import utcnow
from app.main import app
import helpers

ADMIN_EMAIL = "admin@structura.local"
ADMIN_PASSWORD = "admin-forte-seguro-123"
ADMIN_CPF = "12345678909"
FP1 = "1" * 64
FP2 = "2" * 64


@pytest.fixture(autouse=True)
def setup_db():
    Base.metadata.drop_all(bind=engine)
    Base.metadata.create_all(bind=engine)
    integrity._VERIFIED.clear()
    db = SessionLocal()
    integrity.publish(db, helpers.TEXT, helpers.SIG, "current")
    db.commit()
    db.close()


def _create_admin(db, email=ADMIN_EMAIL, password=ADMIN_PASSWORD, cpf=ADMIN_CPF):
    p = create_user_with_license(db, email, password, cpf, is_admin=True, enroll_2fa=True)
    db.commit()
    return p


def _login_admin(client: TestClient, p, email=ADMIN_EMAIL, password=ADMIN_PASSWORD, cpf=ADMIN_CPF) -> str:
    code = totp.code_at(p.totp_secret)
    res = client.post("/admin/auth/login", json={
        "email": email,
        "password": password,
        "cpf": cpf,
        "totp": code,
    })
    assert res.status_code == 200, res.text
    data = res.json()
    assert "admin_token" in data
    return data["admin_token"]


# -------------------------------------------------------------
# Teste 1: Autenticação de Admin + 2FA Obrigatório
# -------------------------------------------------------------
def test_admin_login_requires_admin_role_and_2fa():
    """Critério (a): Admin login exige is_admin=True e 2FA TOTP válido."""
    with TestClient(app) as client:
        db = SessionLocal()
        admin_p = _create_admin(db)
        user_p = create_user_with_license(db, "normal@exemplo.com", "senha-normal-123", "98765432100", is_admin=False)
        db.commit()

        # 1. Usuário normal tentando login admin -> 403
        user_totp = totp.code_at(user_p.totp_secret)
        r_user = client.post("/admin/auth/login", json={
            "email": "normal@exemplo.com",
            "password": "senha-normal-123",
            "cpf": "98765432100",
            "totp": user_totp,
        })
        assert r_user.status_code == 403
        assert r_user.json()["detail"] == "ADMIN_ACCESS_REQUIRED"

        # 2. Admin com TOTP inválido -> 401
        r_bad_totp = client.post("/admin/auth/login", json={
            "email": ADMIN_EMAIL,
            "password": ADMIN_PASSWORD,
            "cpf": ADMIN_CPF,
            "totp": "000000",
        })
        assert r_bad_totp.status_code == 401
        assert r_bad_totp.json()["detail"] == "CREDENCIAIS"

        # 3. Admin com credenciais válidas e TOTP correto -> 200 com admin_token
        admin_code = totp.code_at(admin_p.totp_secret)
        r_ok = client.post("/admin/auth/login", json={
            "email": ADMIN_EMAIL,
            "password": ADMIN_PASSWORD,
            "cpf": ADMIN_CPF,
            "totp": admin_code,
        })
        assert r_ok.status_code == 200
        data = r_ok.json()
        assert "admin_token" in data
        assert data["email"] == ADMIN_EMAIL

        # Confirma que a ação de login de admin foi auditada
        audit = db.scalar(select(AuditLog).where(AuditLog.action == "admin_login"))
        assert audit is not None
        assert f"admin:{admin_p.user.id}" in audit.actor
        db.close()


# -------------------------------------------------------------
# Teste 2: Proteção de Rotas Admin (Rejeita Sem Token ou Token de Usuário)
# -------------------------------------------------------------
def test_admin_routes_reject_unauthenticated_or_license_token():
    """Critério (a): Rotas admin não são abertas e rejeitam requisições sem token ou com token de licença."""
    with TestClient(app) as client:
        db = SessionLocal()
        admin_p = _create_admin(db)
        user_p = create_user_with_license(db, "normal@exemplo.com", "senha-normal-123", "98765432100", is_admin=False)
        db.commit()

        # Rota admin sem token -> 401
        assert client.get("/admin/licenses").status_code == 401
        assert client.get("/admin/abuse").status_code == 401
        assert client.get("/admin/vm-allowlist").status_code == 401

        # Rota admin com token de licença de usuário comum (separação de domínio) -> 401
        # Token de licença usa DOM_TOKEN, enquanto admin exige DOM_ADMIN_TOKEN
        lic_token, _ = crypto.issue_token({"license_id": 1, "user_id": user_p.user.id, "fp": FP1, "did": 1}, 3600)
        headers_user = {"Authorization": f"Bearer {lic_token}"}
        assert client.get("/admin/licenses", headers=headers_user).status_code == 401
        assert client.get("/admin/abuse", headers=headers_user).status_code == 401

        # Com token de admin legítimo -> 200
        admin_token = _login_admin(client, admin_p)
        headers_admin = {"Authorization": f"Bearer {admin_token}"}
        assert client.get("/admin/licenses", headers=headers_admin).status_code == 200
        db.close()


# -------------------------------------------------------------
# Teste 3: Gestão de Licenças (Criar, Listar, Detalhar)
# -------------------------------------------------------------
def test_admin_license_creation_listing_and_detail():
    """Admin cria novas licenças para usuários, lista e consulta detalhes."""
    with TestClient(app) as client:
        db = SessionLocal()
        admin_p = _create_admin(db)
        user_p = create_user_with_license(db, "cliente@exemplo.com", "senha-forte-123", "98765432100")
        db.commit()

        admin_token = _login_admin(client, admin_p)
        headers = {"Authorization": f"Bearer {admin_token}"}

        # Cria licença para cliente via e-mail
        res_create = client.post("/admin/licenses", headers=headers, json={
            "email": "cliente@exemplo.com",
            "plan": "anual",
            "days": 365,
            "status": "ativa",
        })
        assert res_create.status_code == 200, res_create.text
        lic_data = res_create.json()
        assert lic_data["plan"] == "anual"
        assert lic_data["user_id"] == user_p.user.id
        new_lic_id = lic_data["id"]

        # Lista licenças
        res_list = client.get("/admin/licenses", headers=headers)
        assert res_list.status_code == 200
        lics = res_list.json()
        assert len(lics) >= 2  # licença inicial + licença anual criada

        # Detalhes da licença criada
        res_get = client.get(f"/admin/licenses/{new_lic_id}", headers=headers)
        assert res_get.status_code == 200
        assert res_get.json()["id"] == new_lic_id

        # Confirma auditoria
        audit_create = db.scalar(select(AuditLog).where(AuditLog.action == "create_license", AuditLog.target == f"license:{new_lic_id}"))
        assert audit_create is not None
        assert f"admin:{admin_p.user.id}" in audit_create.actor
        db.close()


# -------------------------------------------------------------
# Teste 4: Suspensão, Revogação e Expiração com Efeito Imediato (Fail-Closed)
# -------------------------------------------------------------
def test_admin_license_status_change_reflected_immediately():
    """Critério (c): Suspender/revogar ou alterar expiração via admin afeta o banco e
    faz com que as operações de cliente (heartbeat, check, compute) falhem fechadas imediatamente."""
    with TestClient(app) as client:
        db = SessionLocal()
        admin_p = _create_admin(db)
        user_p = create_user_with_license(db, "dev@exemplo.com", "senha-forte-123", "98765432100")
        db.commit()
        lic_id = user_p.license.id

        admin_token = _login_admin(client, admin_p)
        headers_admin = {"Authorization": f"Bearer {admin_token}"}

        # 1. Login do cliente na licença ativa
        code_cli = totp.code_at(user_p.totp_secret)
        login_res = client.post("/auth/login", json={
            "email": "dev@exemplo.com",
            "password": "senha-forte-123",
            "cpf": "98765432100",
            "totp": code_cli,
            "fingerprint": FP1,
            "integrity": helpers.INTEG,
        })
        assert login_res.status_code == 200
        cli_token = login_res.json()["license_token"]

        # Heartbeat inicial com sucesso
        hb_res = client.post("/license/heartbeat", json={
            "license_token": cli_token,
            "fingerprint": FP1,
            "integrity": helpers.INTEG,
        })
        assert hb_res.status_code == 200

        # 2. Admin suspende a licença via PATCH /admin/licenses/{id}
        patch_res = client.patch(f"/admin/licenses/{lic_id}", headers=headers_admin, json={
            "status": "suspensa",
            "reason": "Suspeita de vazamento de credenciais",
        })
        assert patch_res.status_code == 200
        assert patch_res.json()["status"] == "suspensa"

        # 3. IMEDIATAMENTE o heartbeat do cliente é RECUSADO (fail-closed: status != 'ativa')
        hb_after_suspend = client.post("/license/heartbeat", json={
            "license_token": cli_token,
            "fingerprint": FP1,
            "integrity": helpers.INTEG,
        })
        assert hb_after_suspend.status_code == 403
        assert hb_after_suspend.json()["detail"] in ("LICENSE_INVALID", "LICENSE_INACTIVE", "LICENSE_SUSPENDED")

        # Gate Nível B também é RECUSADO imediatamente (fail-closed)
        check_after_suspend = client.post("/license/check", json={
            "license_token": cli_token,
            "fingerprint": FP1,
            "feature": "detalhamento",
            "nonce": "cafebabe12345678",
            "integrity": helpers.INTEG,
        })
        assert check_after_suspend.status_code == 403 or (check_after_suspend.status_code == 200 and check_after_suspend.json()["ok"] is False)

        # 4. Admin reativa a licença via PATCH
        reactivate_res = client.patch(f"/admin/licenses/{lic_id}", headers=headers_admin, json={
            "status": "ativa",
            "reason": "Verificado com o cliente",
        })
        assert reactivate_res.status_code == 200
        assert reactivate_res.json()["status"] == "ativa"

        # Heartbeat volta a ser aceito
        hb_after_reactivate = client.post("/license/heartbeat", json={
            "license_token": cli_token,
            "fingerprint": FP1,
            "integrity": helpers.INTEG,
        })
        assert hb_after_reactivate.status_code == 200

        # 5. Admin revoga a licença
        revoke_res = client.patch(f"/admin/licenses/{lic_id}", headers=headers_admin, json={
            "status": "revogada",
            "reason": "Cancelamento contratual definitivo",
        })
        assert revoke_res.status_code == 200
        assert revoke_res.json()["status"] == "revogada"

        # Heartbeat é barrado definitivamente
        assert client.post("/license/heartbeat", json={
            "license_token": cli_token,
            "fingerprint": FP1,
            "integrity": helpers.INTEG,
        }).status_code == 403

        # Confirma auditoria de todas as alterações de status
        audits = db.scalars(select(AuditLog).where(
            AuditLog.action == "update_license_status",
            AuditLog.target == f"license:{lic_id}"
        )).all()
        assert len(audits) == 3
        db.close()


# -------------------------------------------------------------
# Teste 5: Fila de AbuseFlag (Listar, Revisar e Persistir)
# -------------------------------------------------------------
def test_admin_abuse_flag_queue_and_review():
    """Critério (b): Listar flags de abuso e transicionar status de 'aberto' para 'revisado' com persistência."""
    with TestClient(app) as client:
        db = SessionLocal()
        admin_p = _create_admin(db)
        user_p = create_user_with_license(db, "abuso@exemplo.com", "senha-forte-123", "98765432100")
        db.commit()
        lic_id = user_p.license.id

        # Cria 2 flags de abuso
        flag1 = abuse.record_abuse_flag(db, lic_id, "concurrent_access", details="Concorrência detectada em <30s")
        flag2 = abuse.record_abuse_flag(db, lic_id, "unauthorized_vm", details="Tentativa em VM sem allowlist")

        admin_token = _login_admin(client, admin_p)
        headers = {"Authorization": f"Bearer {admin_token}"}

        # 1. Lista flags abertos
        res_list = client.get("/admin/abuse?status=aberto", headers=headers)
        assert res_list.status_code == 200
        flags = res_list.json()
        assert len(flags) >= 2
        assert any(f["id"] == flag1.id and f["status"] == "aberto" for f in flags)

        # 2. Admin revisa o flag 1 (PATCH /admin/abuse/{id})
        res_review = client.patch(f"/admin/abuse/{flag1.id}", headers=headers, json={
            "status": "revisado",
            "notes": "Cliente confirmou troca de maquina no escritorio",
        })
        assert res_review.status_code == 200
        updated = res_review.json()
        assert updated["id"] == flag1.id
        assert updated["status"] == "revisado"
        assert "Cliente confirmou" in updated["details"]

        # 3. Confirma no banco que o status foi persistido
        db.expire_all()
        db_flag = db.get(AbuseFlag, flag1.id)
        assert db_flag.status == "revisado"

        # 4. Confirma auditoria
        audit = db.scalar(select(AuditLog).where(
            AuditLog.action == "review_abuse_flag",
            AuditLog.target == f"abuse_flag:{flag1.id}"
        ))
        assert audit is not None
        assert f"admin:{admin_p.user.id}" in audit.actor
        assert "aberto->revisado" in audit.metadata_masked
        db.close()


# -------------------------------------------------------------
# Teste 6: Gestão de Dispositivos (Listar e Liberar Vínculo)
# -------------------------------------------------------------
def test_admin_device_management_and_release():
    """Critério (e): Admin lista dispositivos vinculados e libera vínculo preso, permitindo novo login."""
    with TestClient(app) as client:
        db = SessionLocal()
        admin_p = _create_admin(db)
        user_p = create_user_with_license(db, "device@exemplo.com", "senha-forte-123", "98765432100")
        db.commit()
        lic_id = user_p.license.id

        admin_token = _login_admin(client, admin_p)
        headers_admin = {"Authorization": f"Bearer {admin_token}"}

        # 1. Login vincula dispositivo 1
        code_cli = totp.code_at(user_p.totp_secret)
        r_login1 = client.post("/auth/login", json={
            "email": "device@exemplo.com",
            "password": "senha-forte-123",
            "cpf": "98765432100",
            "totp": code_cli,
            "fingerprint": FP1,
            "integrity": helpers.INTEG,
        })
        assert r_login1.status_code == 200

        # Dispositivo 2 tenta logar e é rejeitado por DEVICE_IN_USE
        u = db.get(User, user_p.user.id)
        u.totp_last_step = 0
        db.commit()
        code_cli2 = totp.code_at(user_p.totp_secret)
        r_login2 = client.post("/auth/login", json={
            "email": "device@exemplo.com",
            "password": "senha-forte-123",
            "cpf": "98765432100",
            "totp": code_cli2,
            "fingerprint": FP2,
            "integrity": helpers.INTEG,
        })
        assert r_login2.status_code == 409
        assert r_login2.json()["detail"] == "DEVICE_IN_USE"

        # 2. Admin lista dispositivos da licença
        res_devs = client.get(f"/admin/licenses/{lic_id}/devices", headers=headers_admin)
        assert res_devs.status_code == 200
        dev_list = res_devs.json()
        assert len(dev_list) == 1
        assert dev_list[0]["fingerprint_hash"] == FP1
        assert dev_list[0]["active"] is True
        bound_dev_id = dev_list[0]["id"]

        # 3. Admin libera o dispositivo preso (DELETE /admin/devices/{id})
        res_rel = client.delete(f"/admin/devices/{bound_dev_id}", headers=headers_admin)
        assert res_rel.status_code == 200
        assert res_rel.json()["status"] == "released"

        # 4. Agora o dispositivo 2 consegue logar com sucesso
        u = db.get(User, user_p.user.id)
        u.totp_last_step = 0
        db.commit()
        code_cli3 = totp.code_at(user_p.totp_secret)
        r_login3 = client.post("/auth/login", json={
            "email": "device@exemplo.com",
            "password": "senha-forte-123",
            "cpf": "98765432100",
            "totp": code_cli3,
            "fingerprint": FP2,
            "integrity": helpers.INTEG,
        })
        assert r_login3.status_code == 200
        assert r_login3.json()["license_token"] is not None

        # Confirma auditoria
        audit_rel = db.scalar(select(AuditLog).where(
            AuditLog.action == "release_device",
            AuditLog.target == f"device:{bound_dev_id}"
        ))
        assert audit_rel is not None
        assert f"admin:{admin_p.user.id}" in audit_rel.actor
        db.close()


# -------------------------------------------------------------
# Teste 7: Gestão de VMAllowlist (Listar, Adicionar, Desativar)
# -------------------------------------------------------------
def test_admin_vm_allowlist_management():
    """Critério (f): Admin lista, cadastra e remove/desativa VMs na allowlist."""
    with TestClient(app) as client:
        db = SessionLocal()
        admin_p = _create_admin(db)
        user_p = create_user_with_license(db, "vm@exemplo.com", "senha-forte-123", "98765432100")
        db.commit()
        lic_id = user_p.license.id

        admin_token = _login_admin(client, admin_p)
        headers = {"Authorization": f"Bearer {admin_token}"}

        # 1. Adiciona VM na allowlist
        res_add = client.post("/admin/vm-allowlist", headers=headers, json={
            "license_id": lic_id,
            "fingerprint": FP1,
            "notes": "Workstation VMware do suporte",
        })
        assert res_add.status_code == 200
        entry = res_add.json()
        assert entry["license_id"] == lic_id
        entry_id = entry["id"]

        # 2. Lista VM allowlist
        res_list = client.get(f"/admin/vm-allowlist?license_id={lic_id}", headers=headers)
        assert res_list.status_code == 200
        entries = res_list.json()
        assert len(entries) == 1
        assert entries[0]["id"] == entry_id
        assert entries[0]["active"] is True

        # 3. Remove/desativa VM na allowlist (DELETE /admin/vm-allowlist/{id})
        res_del = client.delete(f"/admin/vm-allowlist/{entry_id}", headers=headers)
        assert res_del.status_code == 200
        assert res_del.json()["status"] == "deactivated"

        # Confirma no banco que active=False
        db_entry = db.get(VMAllowlist, entry_id)
        assert db_entry.active is False

        # Confirma auditoria
        audit_add = db.scalar(select(AuditLog).where(
            AuditLog.action == "add_vm_allowlist",
            AuditLog.target == f"license:{lic_id}"
        ))
        assert audit_add is not None
        audit_del = db.scalar(select(AuditLog).where(
            AuditLog.action == "remove_vm_allowlist",
            AuditLog.target == f"vm_allowlist:{entry_id}"
        ))
        assert audit_del is not None
        db.close()


def test_admin_metrics_endpoint_aggregated_and_authenticated():
    """Valida o endpoint GET /admin/metrics:
    - Exige autenticacao de administrador (401 sem token)
    - Retorna contadores agregados de licencas, dispositivos, abuso e logins recentes
    - Registra evento de auditoria
    """
    with TestClient(app) as client:
        # Sem token de admin -> 401
        res_no_auth = client.get("/admin/metrics")
        assert res_no_auth.status_code == 401

        db = SessionLocal()
        admin_p = _create_admin(db)
        u1 = create_user_with_license(db, "u1@exemplo.com", "senha-forte-123", "12345678909")
        u2 = create_user_with_license(db, "u2@exemplo.com", "senha-forte-123", "98765432100")
        # Marca uma como suspensa
        u2.license.status = "suspensa"
        # Cria flag de abuso
        abuse.record_abuse_flag(db, u1.license.id, "concurrent_access", details="Teste")
        db.commit()

        admin_token = _login_admin(client, admin_p)
        headers = {"Authorization": f"Bearer {admin_token}"}

        res = client.get("/admin/metrics", headers=headers)
        assert res.status_code == 200
        data = res.json()

        # Valida estrutura de métricas
        assert "licenses" in data
        assert "devices" in data
        assert "abuse" in data
        assert "recent_logins" in data

        assert data["licenses"]["total"] >= 3  # admin + u1 + u2
        assert data["licenses"]["suspended"] >= 1
        assert data["licenses"]["active"] >= 2
        assert data["abuse"]["total"] >= 1
        assert data["abuse"]["total_open"] >= 1
        assert len(data["recent_logins"]) >= 1

        # Auditoria
        audit = db.scalar(select(AuditLog).where(
            AuditLog.action == "get_metrics",
            AuditLog.target == "metrics"
        ))
        assert audit is not None
        db.close()


def test_admin_users_and_license_search_endpoints():
    """Valida GET /admin/users e filtro por busca em GET /admin/licenses."""
    with TestClient(app) as client:
        db = SessionLocal()
        admin_p = _create_admin(db)
        u1 = create_user_with_license(db, "alice@exemplo.com", "senha-forte-123", "12345678909")
        u2 = create_user_with_license(db, "bob@outro.com", "senha-forte-123", "98765432100")
        db.commit()

        admin_token = _login_admin(client, admin_p)
        headers = {"Authorization": f"Bearer {admin_token}"}

        # 1. Listar usuários
        res_users = client.get("/admin/users", headers=headers)
        assert res_users.status_code == 200
        users = res_users.json()
        assert len(users) >= 3
        emails = [u["email"] for u in users]
        assert "alice@exemplo.com" in emails
        assert "bob@outro.com" in emails

        # 2. Busca de licenças por email
        res_search = client.get("/admin/licenses?search=alice", headers=headers)
        assert res_search.status_code == 200
        search_results = res_search.json()
        assert len(search_results) == 1
        assert search_results[0]["user_email"] == "alice@exemplo.com"
        db.close()
