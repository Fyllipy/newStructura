"""Testes do Marco M6: Deteccao de Abuso, Concorrencia, Velocity, Bloqueio de VM com Allowlist e Marcas Forenses.
Base: SPEC.md (§11), PLAN.md (M6) e Decisoes de Desenho Travadas.
"""
import time
import uuid
import pytest
from datetime import timedelta
from sqlalchemy import select
from fastapi import HTTPException

from app.db import Base, engine, SessionLocal
from app import integrity, models, crypto, totp, abuse
from app.models import AuditLog, IntegrityManifest, User, License, AbuseFlag, VMAllowlist, DeviceAccessLog
from app.schemas import (
    LoginRequest, HeartbeatRequest, LicenseCheckRequest, ComputeValidateRequest,
    ReactionTotals, Integrity, AdminVMAllowlistRequest, AdminLicenseStatusRequest
)
from app.users import create_user_with_license
from app.main import login, heartbeat, license_check, compute_validate, admin_add_vm_allowlist, admin_update_license_status
from app.timeutil import utcnow
import helpers

FP1 = "1" * 64
FP2 = "2" * 64
FP3 = "3" * 64
FP4 = "4" * 64


class DummyRequest:
    def __init__(self, ip: str = "127.0.0.1"):
        self.headers = {}
        self.client = type("Client", (), {"host": ip})()


@pytest.fixture(autouse=True)
def setup_db():
    Base.metadata.drop_all(bind=engine)
    Base.metadata.create_all(bind=engine)
    integrity._VERIFIED.clear()
    db = SessionLocal()
    integrity.publish(db, helpers.TEXT, helpers.SIG, "current")
    db.commit()
    db.close()


def _make_login_req(email: str, totp_secret: str, fp: str = FP1, is_vm: bool = False, vm_vendor: str = "") -> LoginRequest:
    code = totp.code_at(totp_secret)
    return LoginRequest(
        email=email,
        password="uma-senha-forte-123",
        cpf="12345678909",
        totp=code,
        fingerprint=fp,
        is_vm=is_vm,
        vm_vendor=vm_vendor,
        integrity=Integrity(
            manifest_version=1,
            files_root=helpers.M.root,
            file_count=helpers.M.file_count,
            exe_hash=helpers.M.exe_hash,
        ),
    )


def test_concurrent_access_blocks_second_request_and_flags():
    """Decisao 2: Concorrencia (<30s com IPs/fingerprints distintos):
    Bloqueia a segunda requisicao imediatamente, registra flag de abuso, sem suspensao na 1a ocorrencia."""
    email = f"user_{uuid.uuid4().hex[:8]}@exemplo.com"
    db = SessionLocal()
    p = create_user_with_license(db, email, "uma-senha-forte-123", "12345678909")
    db.commit()

    req1 = _make_login_req(email, p.totp_secret, fp=FP1)
    res1 = login(req1, db=db, http_req=DummyRequest("192.168.1.100"))
    assert res1.license_token is not None

    # Segunda requisicao simultanea via Heartbeat com outro IP em <30s
    hb_req2 = HeartbeatRequest(
        license_token=res1.license_token,
        fingerprint=FP1,
        integrity=req1.integrity,
    )
    with pytest.raises(HTTPException) as exc:
        heartbeat(hb_req2, db=db, http_req=DummyRequest("192.168.1.200"))
    assert exc.value.status_code == 409
    assert "CONCURRENT_ACCESS" in exc.value.detail

    # Confirma que o flag foi gravado
    flags = db.scalars(select(AbuseFlag).where(AbuseFlag.license_id == p.license.id)).all()
    assert len(flags) == 1
    assert flags[0].kind == "concurrent_access"

    lic = db.get(License, p.license.id)
    assert lic.status == "ativa"  # Nao suspende na primeira ocorrencia


def test_excessive_fingerprints_requires_reauth():
    """Decisao 2: excessive_fingerprints (>3 fingerprints distintos em 24h):
    Bloqueia o 4o fingerprint e exige reautenticacao / flag."""
    email = f"user_{uuid.uuid4().hex[:8]}@exemplo.com"
    db = SessionLocal()
    p = create_user_with_license(db, email, "uma-senha-forte-123", "12345678909")
    db.commit()
    lic_id = p.license.id

    # Simula 3 dispositivos acessando nas ultimas 24h (com espacamento > 30s)
    now = utcnow()
    for i, fp in enumerate([FP1, FP2, FP3]):
        db.add(DeviceAccessLog(
            license_id=lic_id,
            fingerprint_hash=fp,
            ip_address=f"10.0.0.{i+1}",
            action="login",
            ts=now - timedelta(hours=2, seconds=i * 60)
        ))
    db.commit()

    # Tentativa de acesso com o 4o fingerprint
    req4 = _make_login_req(email, p.totp_secret, fp=FP4)
    with pytest.raises(HTTPException) as exc:
        login(req4, db=db, http_req=DummyRequest("10.0.0.99"))
    assert exc.value.status_code == 401
    assert "REAUTH" in exc.value.detail or "EXCESSIVE_FINGERPRINTS" in exc.value.detail

    flags = db.scalars(select(AbuseFlag).where(AbuseFlag.license_id == lic_id)).all()
    assert any(f.kind == "excessive_fingerprints" for f in flags)


def test_velocity_impossible_travel_blocked():
    """Decisao 2: velocity / viagem impossivel (<10s com saltos entre sub-redes distintas):
    Bloqueia requisicao e registra AbuseFlag."""
    email = f"user_{uuid.uuid4().hex[:8]}@exemplo.com"
    db = SessionLocal()
    p = create_user_with_license(db, email, "uma-senha-forte-123", "12345678909")
    db.commit()
    lic_id = p.license.id

    # Acesso legitimo inicial de SP (ex: prefixo 177.x)
    ok, _ = abuse.record_access_and_check_abuse(db, lic_id, FP1, "177.10.20.30", "test")
    assert ok is True

    # Salto impossivel de rede em < 10s (ex: prefixo 200.x de Tokyo)
    ok2, reason2 = abuse.record_access_and_check_abuse(db, lic_id, FP1, "200.50.60.70", "test")
    assert ok2 is False
    assert reason2 == "VELOCITY_LIMIT_EXCEEDED"

    flags = db.scalars(select(AbuseFlag).where(AbuseFlag.license_id == lic_id)).all()
    assert any(f.kind == "velocity" for f in flags)


def test_auto_suspend_after_five_abuse_flags_in_seven_days():
    """Decisao 2 e Criterio (b): Suspensao automatica por 5+ flags de qualquer tipo em 7 dias.
    A licenca muda automaticamente para 'suspensa' e gera AuditLog com o motivo."""
    email = f"user_{uuid.uuid4().hex[:8]}@exemplo.com"
    db = SessionLocal()
    p = create_user_with_license(db, email, "uma-senha-forte-123", "12345678909")
    db.commit()
    lic_id = p.license.id
    lic = db.get(License, lic_id)
    assert lic.status == "ativa"

    # Flag 1: concorrência
    abuse.record_abuse_flag(db, lic_id, "concurrent_access", details="Teste 1")
    db.refresh(lic)
    assert lic.status == "ativa"

    # Flag 2: velocity
    abuse.record_abuse_flag(db, lic_id, "velocity", details="Teste 2")
    db.refresh(lic)
    assert lic.status == "ativa"

    abuse.record_abuse_flag(db, lic_id, "unauthorized_vm", details="Teste 3")
    abuse.record_abuse_flag(db, lic_id, "velocity", details="Teste 4")   # 4/5 -> aviso, ainda ativa
    db.refresh(lic)
    assert lic.status == "ativa"
    assert db.scalar(select(AuditLog).where(AuditLog.target == f"license:{lic_id}",
                                            AuditLog.action == "abuse_warning")) is not None

    # Flag 5: acumulo atinge o limiar de 5 flags em 7 dias -> DISPARA AUTO-SUSPENSAO
    abuse.record_abuse_flag(db, lic_id, "unauthorized_vm", details="Teste 5")
    db.refresh(lic)
    assert lic.status == "suspensa"

    # Confirma que AuditLog registrou a auto-suspensao
    audit = db.scalar(
        select(AuditLog).where(
            AuditLog.target == f"license:{lic_id}",
            AuditLog.action == "auto_suspend_license"
        )
    )
    assert audit is not None
    assert "Auto-suspensao" in audit.metadata_masked
    assert "5 flags" in audit.metadata_masked

    # Confirma que novas operacoes sao recusadas com LICENSE_SUSPENDED
    u = db.get(User, p.user.id)
    u.totp_last_step = 0
    db.commit()

    req = _make_login_req(email, p.totp_secret, fp=FP1)
    with pytest.raises(HTTPException) as exc:
        login(req, db=db, http_req=DummyRequest("127.0.0.1"))
    assert exc.value.status_code == 403
    assert exc.value.detail in ("LICENSE_INACTIVE", "LICENSE_SUSPENDED")


def test_vm_detection_blocked_without_allowlist_and_allowed_with_admin():
    """Decisao 1 e Criterio (a): Bloqueio de VM sem allowlist e liberacao apos cadastro admin."""
    email = f"user_{uuid.uuid4().hex[:8]}@exemplo.com"
    db = SessionLocal()
    p = create_user_with_license(db, email, "uma-senha-forte-123", "12345678909")
    db.commit()
    lic_id = p.license.id

    # 1. Login indicando VM sem allowlist previa
    req_vm = _make_login_req(email, p.totp_secret, fp=FP1, is_vm=True, vm_vendor="VMware, Inc.")
    with pytest.raises(HTTPException) as exc:
        login(req_vm, db=db, http_req=DummyRequest("127.0.0.1"))
    assert exc.value.status_code == 403
    assert exc.value.detail == "VM_NOT_ALLOWED"

    # Confirma flag de abuso de VM nao autorizada
    flag = db.scalar(select(AbuseFlag).where(AbuseFlag.license_id == lic_id))
    assert flag is not None
    assert flag.kind == "unauthorized_vm"

    # 2. Administrador cadastra a VM na allowlist (POST /admin/vm-allowlist)
    adm_res = admin_add_vm_allowlist(AdminVMAllowlistRequest(
        license_id=lic_id,
        fingerprint=FP1,
        notes="Aprovado pelo suporte tecnico para workstation VMware autorizada"
    ), db=db)
    assert adm_res["ok"] is True

    # Reseta o step do TOTP para permitir novo login com o mesmo segredo no teste
    u = db.get(User, p.user.id)
    u.totp_last_step = 0
    db.commit()

    # 3. Agora o login com a VM autorizada e BEM-SUCEDIDO e entrega vm_allow_token
    res_allowed = login(req_vm, db=db, http_req=DummyRequest("127.0.0.1"))
    assert res_allowed.license_token is not None
    assert res_allowed.vm_allow_token is not None
    assert len(res_allowed.vm_allow_token) == 32  # token HMAC hex


def test_forensic_watermark_issuance_and_verification():
    """Decisao 4 e Criterio (d): Emissao de marca forense HMAC-SHA256 e decodificacao pericial."""
    db = SessionLocal()
    lic_id = 42
    export_type = "dxf_export"
    now_ts = int(time.time())

    token = abuse.issue_watermark_token(lic_id, export_type, ts=now_ts)
    assert token.startswith(f"STRUCTURA-WM1:{lic_id}:{now_ts}:{export_type}:")

    # Verificacao pericial da marca d'agua
    ok, decoded_lic, decoded_type, decoded_ts = abuse.verify_watermark_token(token)
    assert ok is True
    assert decoded_lic == lic_id
    assert decoded_type == export_type
    assert decoded_ts == now_ts

    # Se alguem tentar falsificar o token ou alterar a licenca/timestamp
    tampered = token.replace(f":{lic_id}:", ":999:")
    ok_tampered, _, _, _ = abuse.verify_watermark_token(tampered)
    assert ok_tampered is False


def test_compute_validate_wire_format_exp_ts_integers():
    """Anti-regressão crítica: Confirma que o payload do /compute/validate entrega exp e ts
    como inteiros Unix reais na wire JSON, e NÃO como ISO strings, garantindo que o cliente
    C++ não trunque em 2026 via stoi e a assinatura Ed25519 valide perfeitamente."""
    import json
    email = f"user_{uuid.uuid4().hex[:8]}@exemplo.com"
    db = SessionLocal()
    p = create_user_with_license(db, email, "uma-senha-forte-123", "12345678909")
    db.commit()

    req = _make_login_req(email, p.totp_secret, fp=FP1)
    res = login(req, db=db, http_req=DummyRequest("127.0.0.1"))

    from app.model_cache import set_model_cache
    model_hash = "a" * 64
    set_model_cache(model_hash, [0.0, 0.0, 0.0], [0.0, 0.0, 0.0])

    comp_req = ComputeValidateRequest(
        license_token=res.license_token,
        nonce="0123456789abcdef",
        fingerprint=FP1,
        integrity=req.integrity,
        model_hash=model_hash,
        reaction_totals=ReactionTotals(F=[0.0, 0.0, 0.0], M_origin=[0.0, 0.0, 0.0]),
        result_digest="b" * 64,
    )
    comp_res = compute_validate(comp_req, db=db, http_req=DummyRequest("127.0.0.1"))
    assert comp_res.ok is True
    assert isinstance(comp_res.exp, int)
    assert isinstance(comp_res.ts, int)
    assert comp_res.exp > 1700000000
    assert comp_res.ts > 1700000000
    assert comp_res.exp == comp_res.ts + 300

    # Validação rigorosa da serialização JSON da wire
    wire_json = comp_res.model_dump_json()
    wire_data = json.loads(wire_json)
    assert type(wire_data["exp"]) is int
    assert type(wire_data["ts"]) is int
    assert "T" not in str(wire_data["exp"])
    assert "Z" not in str(wire_data["exp"])
    assert "-" not in str(wire_data["exp"])


def test_admin_vm_auto_allowed_without_abuse_flag():
    """Administradores possuem permissao de execucao irrestrita em qualquer ambiente (fisico ou VM).
    O login deve gerar vm_allow_token valido, auto-inscrever na allowlist e nao registrar flag de abuso."""
    email = f"admin_{uuid.uuid4().hex[:8]}@exemplo.com"
    db = SessionLocal()
    p = create_user_with_license(db, email, "uma-senha-forte-123", "12345678909", is_admin=True)
    db.commit()

    # Login como admin com is_vm=True
    req_vm = _make_login_req(email, p.totp_secret, fp=FP1, is_vm=True, vm_vendor="Microsoft Hyper-V")
    res = login(req_vm, db=db, http_req=DummyRequest("127.0.0.1"))
    assert res.license_token is not None
    assert res.vm_allow_token is not None
    assert len(res.vm_allow_token) == 32

    # Confirma que NENHUM flag de abuso foi registrado
    flags = db.scalars(select(AbuseFlag).where(AbuseFlag.license_id == p.license.id)).all()
    assert len(flags) == 0

    # Confirma que entrada ativa foi criada na allowlist
    allow = db.scalar(select(VMAllowlist).where(VMAllowlist.license_id == p.license.id, VMAllowlist.fingerprint_hash == FP1))
    assert allow is not None
    assert allow.active is True

