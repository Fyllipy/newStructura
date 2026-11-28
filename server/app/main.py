"""API de licenciamento e validacao.

Fluxos: /auth/login, /auth/2fa/*, /auth/logout, /license/heartbeat, /compute/validate, /pubkey.
Admin e webhook de pagamento como stubs (M7/M8). Decisoes travadas na SPEC.md.

Rode:
    python gen_keys.py --write
    python -m app.seed
    uvicorn app.main:app --reload
"""
from __future__ import annotations
import hmac
import time
from datetime import timedelta

from fastapi import FastAPI, Depends, HTTPException, Request
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse, FileResponse, HTMLResponse
from fastapi.staticfiles import StaticFiles
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session
from sqlalchemy import select, update, delete, func

from .db import Base, engine, get_db
from .users import ProvisionError, create_user_with_license, generate_temp_password
from .models import TelemetryEvent, User, AuditLog, IntegrityManifest, VMAllowlist, License, DeviceAccessLog, Device, AbuseFlag
from .schemas import (
    LoginRequest, LoginResponse, LogoutRequest,
    HeartbeatRequest, HeartbeatResponse,
    ComputeValidateRequest, ComputeValidateResponse, Integrity,
    Credentials, TwoFASetupRequest, TwoFASetupResponse, TwoFAConfirmRequest,
    LicenseCheckRequest, LicenseCheckResponse,
    AdminVMAllowlistRequest, AdminLicenseStatusRequest, AdminTelemetryResponse, AdminFraudSignal, AdminCreateUserRequest, AdminCreateUserResponse,
    AdminResetPasswordResponse, ChangePasswordRequest,
    AdminLoginRequest, AdminLoginResponse, AdminCreateLicenseRequest,
    AdminLicenseResponse, AdminDeviceResponse, AdminReviewAbuseFlagRequest,
    AdminAbuseFlagResponse, AdminVMAllowlistEntryResponse,
    AdminUserResponse, AdminMetricsResponse, AdminRecentLogin,
    AdminMetricsLicenses, AdminMetricsDevices, AdminMetricsAbuse,
)
from .integrity import check_attestation, IntegrityResult
from . import crypto, totp, abuse, integrity, ratelimit, telemetry
from .config import settings
from .identity import normalize_cpf, normalize_email, mask_cpf
from .licensing import (
    license_is_usable, bind_or_check_device, release_device_by_id,
    authenticate_token, pick_license,
)
from .equilibrium import applied_resultant, check_equilibrium
from .model_cache import get_model_cache, set_model_cache, check_and_record_nonce
from .timeutil import utcnow, iso, iso_ts

settings.validate_secrets()  # fail-closed: sem chaves validas o servidor nao sobe
Base.metadata.create_all(bind=engine)


_NEW_COLUMNS = {
    "users": [
        ("must_change_password", "BOOLEAN NOT NULL DEFAULT FALSE"),
        ("stripe_customer_id", "VARCHAR(64)"), ("card_pm_id", "VARCHAR(64)"), ("card_brand", "VARCHAR(24)"),
        ("card_last4", "VARCHAR(4)"), ("card_exp_month", "INTEGER"), ("card_exp_year", "INTEGER"),
        ("auto_renew", "BOOLEAN NOT NULL DEFAULT FALSE"), ("totp_pending_enc", "VARCHAR(255)"),
    ],
    "licenses": [
        ("renew_attempts", "INTEGER NOT NULL DEFAULT 0"), ("last_renew_attempt_at", "TIMESTAMP"),
    ],
}


def _ensure_columns() -> None:
    """Migracao leve: create_all nao altera tabelas existentes (bancos criados antes destas colunas)."""
    from sqlalchemy import inspect, text as _text
    for table, cols_def in _NEW_COLUMNS.items():
        have = {c["name"] for c in inspect(engine).get_columns(table)}
        for name, ddl in cols_def:
            if name not in have:
                with engine.begin() as conn:
                    conn.execute(_text(f"ALTER TABLE {table} ADD COLUMN {name} {ddl}"))


_ensure_columns()
app = FastAPI(
    title="Structura Licensing",
    docs_url="/docs" if settings.EXPOSE_DOCS else None,
    redoc_url="/redoc" if settings.EXPOSE_DOCS else None,
    openapi_url="/openapi.json" if settings.EXPOSE_DOCS else None,
)

_CSP = ("default-src 'none'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; "
        "connect-src 'self'; base-uri 'none'; form-action 'self'; frame-ancestors 'none'")


@app.middleware("http")
async def _security_headers(request: Request, call_next):
    resp = await call_next(request)
    h = resp.headers
    h.setdefault("X-Content-Type-Options", "nosniff")
    h.setdefault("X-Frame-Options", "DENY")
    h.setdefault("Referrer-Policy", "no-referrer")
    h.setdefault("Cross-Origin-Opener-Policy", "same-origin")
    h.setdefault("Cross-Origin-Resource-Policy", "same-origin")
    h.setdefault("Permissions-Policy", "camera=(), microphone=(), geolocation=()")
    if request.url.scheme == "https" or request.headers.get("x-forwarded-proto") == "https":
        h.setdefault("Strict-Transport-Security", "max-age=31536000; includeSubDomains")
    p = request.url.path
    if p.startswith("/admin") and not p.startswith("/admin/assets"):
        h.setdefault("Content-Security-Policy", _CSP)
    if not p.startswith("/admin/assets"):
        h.setdefault("Cache-Control", "no-store")   # respostas de API/auth nunca em cache
    return resp


@app.exception_handler(RequestValidationError)
async def _validation_handler(_req: Request, _exc: RequestValidationError):
    # O handler padrao ecoa o corpo (senha, CPF) na resposta/logs de proxy. Erro generico.
    return JSONResponse(status_code=422, content={"detail": "INVALID_REQUEST"})


def _audit(db: Session, actor: str, action: str, target: str, meta: str = "") -> None:
    # NUNCA passar PII (email/CPF) em meta/target: apenas ids numericos e codigos.
    db.add(AuditLog(actor=actor, action=action, target=target, metadata_masked=meta))


def _extract_ip(http_req: Request | None) -> str:
    if http_req is None:
        return "127.0.0.1"
    forwarded = http_req.headers.get("x-forwarded-for")
    if forwarded and settings.TRUST_PROXY_HEADERS:
        # Atras do proxy confiavel, o ULTIMO salto e o que o proxy anexou (o primeiro e forjavel).
        return forwarded.split(",")[-1].strip() or "127.0.0.1"
    if http_req.client and http_req.client.host:
        return http_req.client.host
    return "127.0.0.1"


def _integrity_status(db: Session, att: Integrity) -> IntegrityResult:
    """Atestacao server-side contra o manifesto ASSINADO publicado (app/integrity.py). Fail-closed."""
    return check_attestation(db, att)


@app.get("/health")
def health(db: Session = Depends(get_db)):
    """Liveness + DB. Permite ao cliente distinguir servidor fora do ar de credencial errada."""
    try:
        db.execute(select(1))
        return {"status": "ok", "db": "ok"}
    except Exception:
        return JSONResponse(status_code=503, content={"status": "degraded", "db": "error"})


@app.get("/pubkey")
def pubkey():
    # Chave publica para embutir no cliente (verificacao de token e K).
    return {"server_pubkey_b64": crypto.server_pubkey_b64(), "id": "k1"}


# ---------------- credenciais (compartilhado por login e 2FA) ----------------
def _fail(status: int, detail: str):
    raise HTTPException(status_code=status, detail=detail)


def _check_credentials(db: Session, c: Credentials, http_req: Request | None = None) -> User:
    """Valida e-mail+senha+CPF com lockout por conta e sem oraculos (mensagem/tempo uniformes)."""
    ip = _extract_ip(http_req)
    if ratelimit.is_blocked(ip):
        _fail(429, "TOO_MANY_ATTEMPTS")
    user = db.scalar(select(User).where(User.email == normalize_email(c.email)))
    ua = (http_req.headers.get("user-agent", "") if http_req is not None else "")
    if user is None:
        crypto.burn_password_time(c.password)
        ratelimit.register_failure(ip)
        telemetry.record(db, "login_failed", ip, email=normalize_email(c.email), user_agent=ua, detail="unknown_user")
        _fail(401, "CREDENCIAIS")
    now = utcnow()
    if user.locked_until is not None and user.locked_until > now:
        crypto.burn_password_time(c.password)  # mesmo custo: sem oraculo de timing de conta travada
        _audit(db, f"user:{user.id}", "login_blocked_lockout", f"user:{user.id}")
        db.commit()
        telemetry.record(db, "login_failed", ip, user_id=user.id, email=user.email, user_agent=ua, detail="locked")
        _fail(401, "CREDENCIAIS")
    pw_ok = crypto.verify_password(user.password_hash, c.password)
    if not pw_ok:
        ratelimit.register_failure(ip)
        telemetry.record(db, "login_failed", ip, user_id=user.id, email=user.email, user_agent=ua, detail="bad_password")
        _register_failure(db, user, "bad_credentials")
        _fail(401, "CREDENCIAIS")
    return user


def _register_failure(db: Session, user: User, why: str) -> None:
    user.failed_logins = (user.failed_logins or 0) + 1
    if user.failed_logins >= settings.LOCKOUT_THRESHOLD:
        user.locked_until = utcnow() + timedelta(minutes=settings.LOCKOUT_MINUTES)
        user.failed_logins = 0
        _audit(db, f"user:{user.id}", "account_locked", f"user:{user.id}", why)
    _audit(db, f"user:{user.id}", "login_failed", f"user:{user.id}", why)
    db.commit()


def _totp_step(user: User, code: str) -> int | None:
    """Valida o codigo SEM consumi-lo. Retorna o passo aceito ou None."""
    if not user.totp_secret_enc:
        return None
    try:
        secret = crypto.totp_decrypt(user.totp_secret_enc)
    except Exception:
        return None
    return totp.verify(secret, code, last_step=user.totp_last_step or 0, window=settings.TOTP_WINDOW)


def _consume_totp(db: Session, user: User, step: int) -> bool:
    # Anti-replay ATOMICO: UPDATE condicional. Dois logins concorrentes com o mesmo codigo
    # leem o mesmo last_step, mas so um UPDATE afeta a linha.
    res = db.execute(update(User).where(User.id == user.id, User.totp_last_step < step)
                     .values(totp_last_step=step))
    if res.rowcount != 1:
        return False
    user.totp_last_step = step
    return True


def _check_totp(db: Session, user: User, code: str) -> bool:
    step = _totp_step(user, code)
    return step is not None and _consume_totp(db, user, step)


# ---------------- 2FA enrollment ----------------
@app.post("/auth/2fa/setup", response_model=TwoFASetupResponse)
def twofa_setup(req: TwoFASetupRequest, db: Session = Depends(get_db), http_req: Request = None):
    """Gera segredo TOTP pendente. So permitido se 2FA ainda NAO esta ativo (reset = admin).
    Usuarios provisionados pelo admin/seed ja vem inscritos, o que fecha a janela de
    'enrollment por quem roubou a senha'."""
    user = _check_credentials(db, req, http_req)
    if user.totp_enabled:
        _fail(409, "2FA_ALREADY_ENABLED")
    secret = totp.gen_secret()
    user.totp_secret_enc = crypto.totp_encrypt(secret)
    user.totp_last_step = 0
    _audit(db, f"user:{user.id}", "2fa_setup", f"user:{user.id}")
    db.commit()
    return TwoFASetupResponse(secret=secret, otpauth_uri=totp.otpauth_uri(secret, user.email))


@app.post("/auth/2fa/confirm", status_code=204)
def twofa_confirm(req: TwoFAConfirmRequest, db: Session = Depends(get_db), http_req: Request = None):
    user = _check_credentials(db, req, http_req)
    if user.totp_enabled or not user.totp_secret_enc:
        _fail(409, "2FA_STATE")
    # NAO consome o codigo: o assistente do app faz login logo em seguida com o MESMO codigo, e e esse
    # login que o consome (antes, o login pos-confirmacao falhava por replay do proprio codigo).
    if _totp_step(user, req.totp) is None:
        _register_failure(db, user, "bad_totp_confirm")
        _fail(401, "CREDENCIAIS")
    user.totp_enabled = True
    _audit(db, f"user:{user.id}", "2fa_enabled", f"user:{user.id}")
    db.commit()
    return None


# ---------------- troca de senha (obrigatoria no 1o acesso com senha temporaria) ----------------
@app.post("/auth/password/change", status_code=204)
def change_password(req: ChangePasswordRequest, db: Session = Depends(get_db), http_req: Request = None):
    user = _check_credentials(db, req, http_req)    # exige a senha atual (lockout/rate limit normais)
    if req.new_password == req.password:
        _fail(422, "PASSWORD_UNCHANGED")
    user.password_hash = crypto.hash_password(req.new_password)
    user.must_change_password = False
    user.failed_logins = 0
    _audit(db, f"user:{user.id}", "password_changed", f"user:{user.id}")
    db.commit()
    return None


# ---------------- login / logout / heartbeat ----------------
@app.post("/auth/login", response_model=LoginResponse)
def login(req: LoginRequest, db: Session = Depends(get_db), http_req: Request = None):
    user = _check_credentials(db, req, http_req)

    # 2FA (SPEC §16.5). So chega aqui com senha+CPF corretos.
    totp_step = None
    if settings.REQUIRE_2FA:
        if not user.totp_enabled:
            return JSONResponse(status_code=403, content={"detail": "ENROLL_2FA", "requires_2fa_setup": True})
        if not req.totp:
            _fail(401, "TOTP_REQUIRED")
        # Valida SEM consumir: o codigo so e queimado quando o login inteiro der certo. Antes, uma falha
        # posterior (integridade, licenca, dispositivo) consumia o codigo; o retry com o mesmo codigo
        # virava "bad_totp", contava como tentativa e travava a conta (admin e app) por 15 min.
        totp_step = _totp_step(user, req.totp)
        if totp_step is None:
            _register_failure(db, user, "bad_totp")
            _fail(401, "CREDENCIAIS")

    if user.status != "ativa":
        db.commit()
        _fail(403, "LICENSE_INACTIVE")
    ires = _integrity_status(db, req.integrity)
    if ires != IntegrityResult.OK:
        _audit(db, f"user:{user.id}", "login_integrity_fail", f"user:{user.id}", ires.value)
        db.commit()
        if ires == IntegrityResult.OUTDATED:
            _fail(426, "CLIENT_OUTDATED")
        _fail(409, "INTEGRITY_FAIL")

    lic = pick_license(db, user.id)
    if lic is None:
        db.commit()
        _fail(403, "LICENSE_INACTIVE")

    # M6: Inspecao de VM e allowlist
    vm_ok, vm_reason, vm_token = abuse.check_vm_authorized(db, lic.id, req.fingerprint, req.is_vm, req.vm_vendor, is_admin=user.is_admin)
    if not vm_ok:
        _fail(403, "VM_NOT_ALLOWED")

    try:
        ok, reason, dev = bind_or_check_device(db, lic, req.fingerprint)
        if not ok:
            _audit(db, f"user:{user.id}", "login_device_in_use", f"license:{lic.id}")
            db.commit()
            _fail(409, reason)

        # Se houve rebound de dispositivo stale, limpa histórico de concorrência do dispositivo anterior
        if reason == "rebound_stale":
            db.execute(delete(DeviceAccessLog).where(DeviceAccessLog.license_id == lic.id))

        ip = _extract_ip(http_req)
        abuse_ok, abuse_reason = abuse.record_access_and_check_abuse(db, lic.id, req.fingerprint, ip, "login")
        if not abuse_ok:
            if reason in ("bound", "rebound_stale"):   # nao deixa vinculo orfao de um login negado
                dev.active = False
                dev.released_at = utcnow()
                db.commit()
            status_code = 409 if abuse_reason == "CONCURRENT_ACCESS_DETECTED" else (401 if abuse_reason == "EXCESSIVE_FINGERPRINTS" else 403)
            _fail(status_code, abuse_reason)

        if totp_step is not None and not _consume_totp(db, user, totp_step):
            db.rollback()                       # outro login concorrente usou este codigo primeiro
            _register_failure(db, user, "totp_replay")
            _fail(401, "CREDENCIAIS")
        user.failed_logins = 0
        user.locked_until = None
        token, exp = crypto.issue_token(
            {"license_id": lic.id, "user_id": user.id, "fp": req.fingerprint, "did": dev.id},
            settings.TOKEN_TTL_SECONDS,
        )
        _audit(db, f"user:{user.id}", "login", f"license:{lic.id}", reason)
        db.commit()
        telemetry.record(db, "login", ip, user_id=user.id, license_id=lic.id, email=user.email,
                         fingerprint=req.fingerprint, tel=req.telemetry, is_vm=req.is_vm, vm_vendor=req.vm_vendor,
                         user_agent=(http_req.headers.get("user-agent", "") if http_req is not None else ""),
                         detail=reason)

        # Marco M5: Entrega da DLL_RELEASE_KEY vinculada exclusivamente a versao autorizada
        # (NUNCA logada em texto claro, auditada ou exposta fora do envelope TLS)
        manifest_row = db.get(IntegrityManifest, req.integrity.manifest_version)
        dll_key = None
        if manifest_row and manifest_row.status in ("current", "allowed"):
            dll_key = manifest_row.dll_release_key or integrity.get_or_create_dll_key(db, manifest_row.version)
    except IntegrityError:  # corrida: outro login vinculou primeiro (indice unico parcial)
        db.rollback()
        _fail(409, "DEVICE_IN_USE")
    return LoginResponse(license_token=token, expires_at=iso_ts(exp), license_id=lic.id, user_id=user.id,
                         must_change_password=bool(user.must_change_password),
                         dll_key=dll_key, vm_allow_token=vm_token)


@app.post("/auth/logout", status_code=204)
def logout(req: LogoutRequest, db: Session = Depends(get_db)):
    # Aceita token expirado (assinatura valida) para o cliente sempre poder liberar o vinculo.
    payload = crypto.verify_token(req.license_token, allow_expired=True)
    if payload:
        try:
            did = int(payload["did"])
            lic_id = int(payload.get("license_id", 0))
            if release_device_by_id(db, did):
                _audit(db, f"user:{payload.get('user_id')}", "logout", f"license:{lic_id}")
                if lic_id > 0:
                    db.execute(delete(DeviceAccessLog).where(DeviceAccessLog.license_id == lic_id))
            db.commit()
        except (KeyError, ValueError, TypeError):
            pass
    return None  # resposta identica com token valido/invalido


@app.post("/license/heartbeat", response_model=HeartbeatResponse)
def heartbeat(req: HeartbeatRequest, db: Session = Depends(get_db), http_req: Request = None):
    ctx = authenticate_token(db, req.license_token, req.fingerprint)
    if ctx is None:
        raise HTTPException(status_code=403, detail="LICENSE_INVALID")
    if ctx.license.status == "suspensa":
        raise HTTPException(status_code=403, detail="LICENSE_SUSPENDED")
    ires = _integrity_status(db, req.integrity)
    if ires != IntegrityResult.OK:
        _audit(db, f"user:{ctx.user.id}", "heartbeat_integrity_fail", f"license:{ctx.license.id}", ires.value)
        db.commit()
        raise HTTPException(status_code=426 if ires == IntegrityResult.OUTDATED else 409,
                            detail="CLIENT_OUTDATED" if ires == IntegrityResult.OUTDATED else "INTEGRITY_FAIL")

    vm_ok, vm_reason, vm_token = abuse.check_vm_authorized(db, ctx.license.id, req.fingerprint, req.is_vm, req.vm_vendor, is_admin=ctx.user.is_admin)
    if not vm_ok:
        raise HTTPException(status_code=403, detail="VM_NOT_ALLOWED")
    ip = _extract_ip(http_req)
    abuse_ok, abuse_reason = abuse.record_access_and_check_abuse(db, ctx.license.id, req.fingerprint, ip, "heartbeat")
    if not abuse_ok:
        status_code = 409 if abuse_reason == "CONCURRENT_ACCESS_DETECTED" else 403
        raise HTTPException(status_code=status_code, detail=abuse_reason)

    ctx.device.last_heartbeat_at = utcnow()
    token, exp = crypto.issue_token(
        {"license_id": ctx.license.id, "user_id": ctx.user.id, "fp": req.fingerprint, "did": ctx.device.id},
        settings.TOKEN_TTL_SECONDS,
    )
    db.commit()
    telemetry.record(db, "heartbeat", ip, user_id=ctx.user.id, license_id=ctx.license.id, email=ctx.user.email,
                     fingerprint=req.fingerprint, tel=req.telemetry, is_vm=req.is_vm, vm_vendor=req.vm_vendor)
    return HeartbeatResponse(license_token=token, expires_at=iso_ts(exp), license_id=ctx.license.id,
                             user_id=ctx.user.id, status=ctx.license.status, vm_allow_token=vm_token)


@app.post("/license/check", response_model=LicenseCheckResponse)
def license_check(req: LicenseCheckRequest, db: Session = Depends(get_db), http_req: Request = None):
    """Nivel B (detalhamento, memorial): licenca valida + integridade + presenca ONLINE, sem K.
    Sem internet o cliente nao obtem ok -> modulo bloqueado. Fail-closed: 403 em qualquer falha.
    Cada uso exige uma chamada nova (passe de vida curta, atrelado ao nonce do cliente)."""
    def deny(reason: str):
        return JSONResponse(status_code=403, content={"ok": False, "reason": reason})

    ctx = authenticate_token(db, req.license_token, req.fingerprint)
    if ctx is None:
        return deny("LICENSE_INVALID")
    if ctx.license.status == "suspensa":
        return deny("LICENSE_SUSPENDED")
    ires = _integrity_status(db, req.integrity)
    if ires != IntegrityResult.OK:
        _audit(db, f"user:{ctx.user.id}", "check_integrity_fail", f"license:{ctx.license.id}", req.feature)
        db.commit()
        return deny("CLIENT_OUTDATED" if ires == IntegrityResult.OUTDATED else "INTEGRITY_FAIL")

    vm_ok, vm_reason, _ = abuse.check_vm_authorized(db, ctx.license.id, req.fingerprint, req.is_vm, req.vm_vendor, is_admin=ctx.user.is_admin)
    if not vm_ok:
        return deny("VM_NOT_ALLOWED")
    ip = _extract_ip(http_req)
    abuse_ok, abuse_reason = abuse.record_access_and_check_abuse(db, ctx.license.id, req.fingerprint, ip, f"check:{req.feature}")
    if not abuse_ok:
        return deny(abuse_reason)

    exp = int(time.time()) + settings.GATE_PASS_TTL_SECONDS
    ctx.device.last_heartbeat_at = utcnow()
    _audit(db, f"user:{ctx.user.id}", "license_check", f"license:{ctx.license.id}", req.feature)
    db.commit()
    wm = abuse.issue_watermark_token(ctx.license.id, req.feature)
    return LicenseCheckResponse(
        ok=True,
        exp=exp,
        pass_sig=crypto.sign_gate_pass(req.feature, ctx.license.id, req.nonce, exp),
        watermark_token=wm,
    )


@app.post("/compute/validate", response_model=ComputeValidateResponse)
def compute_validate(req: ComputeValidateRequest, db: Session = Depends(get_db), http_req: Request = None):
    # Fail-closed: qualquer falha retorna ok=false (sem K).
    ctx = authenticate_token(db, req.license_token, req.fingerprint)
    if ctx is None:
        return ComputeValidateResponse(ok=False, reason="LICENSE_INVALID")
    lic = ctx.license
    if lic.status == "suspensa":
        return ComputeValidateResponse(ok=False, reason="LICENSE_SUSPENDED")
    if _integrity_status(db, req.integrity) != IntegrityResult.OK:
        _audit(db, f"user:{ctx.user.id}", "compute_integrity_fail", f"license:{ctx.license.id}")
        db.commit()
        return ComputeValidateResponse(ok=False, reason="INTEGRITY_FAIL")

    vm_ok, vm_reason, vm_token = abuse.check_vm_authorized(db, lic.id, req.fingerprint, req.is_vm, req.vm_vendor, is_admin=ctx.user.is_admin)
    if not vm_ok:
        return ComputeValidateResponse(ok=False, reason="VM_NOT_ALLOWED")
    ip = _extract_ip(http_req)
    abuse_ok, abuse_reason = abuse.record_access_and_check_abuse(db, lic.id, req.fingerprint, ip, "compute_validate")
    if not abuse_ok:
        return ComputeValidateResponse(ok=False, reason=abuse_reason)

    # Anti-replay de nonce por licença dentro da janela de validade (300 segundos)
    if not check_and_record_nonce(lic.id, req.nonce, ttl_seconds=300):
        _audit(db, f"user:{ctx.user.id}", "nonce_reused", f"license:{lic.id}", req.nonce)
        db.commit()
        return ComputeValidateResponse(ok=False, reason="NONCE_REUSED")

    # lado aplicado: do cache por model_hash (Redis/memoria), ou recomputa do model enviado
    cached = get_model_cache(req.model_hash)
    if cached is None:
        if req.model is None:
            return ComputeValidateResponse(ok=False, reason="MODEL_REQUIRED")
        try:
            cached = applied_resultant(req.model)
        except ValueError:   # barra de comprimento nulo / eixo local indeterminado
            return ComputeValidateResponse(ok=False, reason="MODEL_INVALID")
        set_model_cache(req.model_hash, cached[0], cached[1])
    applied_F, applied_M = cached

    if not check_equilibrium(applied_F, applied_M,
                             req.reaction_totals.F, req.reaction_totals.M_origin):
        return ComputeValidateResponse(ok=False, reason="EQUILIBRIUM_FAIL")

    ts = int(time.time())
    exp = ts + 300  # validade curta do K (5 minutos)
    ctx.device.last_heartbeat_at = utcnow()
    db.commit()
    K = crypto.sign_keystone(req.model_hash, req.result_digest, req.nonce, lic.id, exp, ts)
    wm = abuse.issue_watermark_token(lic.id, "compute_keystone", ts)
    return ComputeValidateResponse(ok=True, K=K, ts=ts, exp=exp, watermark_token=wm, vm_allow_token=vm_token)


# ---------------- dependência de autenticação do admin (M7) ----------------
def get_current_admin(req: Request, db: Session = Depends(get_db)) -> User:
    auth_header = req.headers.get("authorization") or req.headers.get("x-admin-token")
    if not auth_header:
        raise HTTPException(status_code=401, detail="ADMIN_AUTH_REQUIRED")
    token = auth_header
    if token.startswith("Bearer "):
        token = token[7:].strip()
    payload = crypto.verify_admin_token(token)
    if not payload:
        raise HTTPException(status_code=401, detail="ADMIN_AUTH_REQUIRED")
    user_id = payload.get("user_id")
    admin_user = db.get(User, user_id)
    if admin_user is None or not admin_user.is_admin or admin_user.status != "ativa":
        raise HTTPException(status_code=403, detail="ADMIN_ACCESS_REQUIRED")
    if not hmac.compare_digest(str(payload.get("pwv", "")), crypto.password_version(admin_user.password_hash)):
        raise HTTPException(status_code=401, detail="ADMIN_AUTH_REQUIRED")   # senha trocada apos a emissao
    return admin_user


# ---------------- rotas administrativas autenticadas com 2FA (M7) ----------------
@app.post("/admin/auth/login", response_model=AdminLoginResponse)
def admin_login(req: AdminLoginRequest, db: Session = Depends(get_db), http_req: Request = None):
    """Login do painel administrativo: exige credenciais fortes + 2FA TOTP obrigatório."""
    user = _check_credentials(db, req, http_req)
    if not user.is_admin:
        _audit(db, f"user:{user.id}", "admin_login_rejected_not_admin", f"user:{user.id}")
        db.commit()
        raise HTTPException(status_code=403, detail="ADMIN_ACCESS_REQUIRED")

    if not user.totp_enabled or not req.totp:
        raise HTTPException(status_code=401, detail="TOTP_REQUIRED")

    if not _check_totp(db, user, req.totp):
        _register_failure(db, user, "bad_admin_totp")
        raise HTTPException(status_code=401, detail="CREDENCIAIS")

    token, exp = crypto.issue_admin_token(user.id, user.email, settings.ADMIN_TOKEN_TTL_SECONDS,
                                          crypto.password_version(user.password_hash))
    _audit(db, f"admin:{user.id}", "admin_login", f"user:{user.id}")
    db.commit()
    return AdminLoginResponse(
        admin_token=token,
        expires_at=iso_ts(exp),
        user_id=user.id,
        email=user.email,
    )


@app.post("/admin/licenses", response_model=AdminLicenseResponse)
def admin_create_license(
    req: AdminCreateLicenseRequest,
    db: Session = Depends(get_db),
    admin: User = Depends(get_current_admin),
):
    target = None
    if req.user_id:
        target = db.get(User, req.user_id)
    elif req.email:
        target = db.scalar(select(User).where(User.email == normalize_email(req.email)))
    if target is None:
        raise HTTPException(status_code=404, detail="USER_NOT_FOUND")

    period_end = utcnow() + timedelta(days=req.days)
    lic = License(
        user_id=target.id,
        status=req.status,
        plan=req.plan,
        current_period_end=period_end,
    )
    db.add(lic)
    db.flush()
    admin_actor = f"admin:{admin.id}" if isinstance(admin, User) else "admin"
    _audit(db, admin_actor, "create_license", f"license:{lic.id}", f"user:{target.id},plan:{req.plan},days:{req.days}")
    db.commit()
    return AdminLicenseResponse(
        id=lic.id,
        user_id=lic.user_id,
        user_email=target.email,
        status=lic.status,
        current_period_end=lic.current_period_end,
        plan=lic.plan,
        created_at=lic.created_at,
        devices_count=0,
    )


@app.get("/admin/licenses", response_model=list[AdminLicenseResponse])
def admin_list_licenses(
    status: str | None = None,
    user_id: int | None = None,
    search: str | None = None,
    plan: str | None = None,
    db: Session = Depends(get_db),
    admin: User = Depends(get_current_admin),
):
    stmt = select(License)
    if status:
        stmt = stmt.where(License.status == status)
    if user_id:
        stmt = stmt.where(License.user_id == user_id)
    if plan:
        stmt = stmt.where(License.plan == plan)
    if search:
        s_clean = search.strip()
        if s_clean.isdigit():
            stmt = stmt.where((License.id == int(s_clean)) | (License.user_id == int(s_clean)))
        else:
            stmt = stmt.join(User, License.user_id == User.id).where(
                User.email.ilike(f"%{_like_escape(s_clean.lower()[:100])}%", escape="\\"))

    lics = db.scalars(stmt.order_by(License.id.desc())).all()
    admin_actor = f"admin:{admin.id}" if isinstance(admin, User) else "admin"
    _audit(db, admin_actor, "list_licenses", "licenses", f"count:{len(lics)}")
    db.commit()
    res = []
    for l in lics:
        active_count = len([d for d in l.devices if d.active])
        res.append(AdminLicenseResponse(
            id=l.id,
            user_id=l.user_id,
            user_email=l.user.email if l.user else None,
            status=l.status,
            current_period_end=l.current_period_end,
            plan=l.plan,
            created_at=l.created_at,
            devices_count=active_count,
        ))
    return res


@app.get("/admin/licenses/{license_id}", response_model=AdminLicenseResponse)
def admin_get_license(
    license_id: int,
    db: Session = Depends(get_db),
    admin: User = Depends(get_current_admin),
):
    lic = db.get(License, license_id)
    if lic is None:
        raise HTTPException(status_code=404, detail="LICENSE_NOT_FOUND")
    active_count = len([d for d in lic.devices if d.active])
    admin_actor = f"admin:{admin.id}" if isinstance(admin, User) else "admin"
    _audit(db, admin_actor, "get_license", f"license:{license_id}")
    db.commit()
    return AdminLicenseResponse(
        id=lic.id,
        user_id=lic.user_id,
        user_email=lic.user.email if lic.user else None,
        status=lic.status,
        current_period_end=lic.current_period_end,
        plan=lic.plan,
        created_at=lic.created_at,
        devices_count=active_count,
    )


@app.patch("/admin/licenses/{license_id}", response_model=AdminLicenseResponse)
def admin_update_license_status(
    license_id: int,
    req: AdminLicenseStatusRequest,
    db: Session = Depends(get_db),
    admin: User = Depends(get_current_admin),
):
    lic = db.get(License, license_id)
    if lic is None:
        raise HTTPException(status_code=404, detail="LICENSE_NOT_FOUND")
    old_status = lic.status
    if req.status:
        lic.status = req.status
    if req.current_period_end:
        lic.current_period_end = req.current_period_end
    if req.add_days:
        base = max(lic.current_period_end, utcnow())
        lic.current_period_end = base + timedelta(days=req.add_days)

    admin_actor = f"admin:{admin.id}" if isinstance(admin, User) else "admin"
    _audit(db, admin_actor, "update_license_status", f"license:{license_id}", f"{old_status}->{lic.status},exp:{lic.current_period_end},reason:{req.reason}")
    db.commit()
    active_count = len([d for d in lic.devices if d.active])
    return AdminLicenseResponse(
        id=lic.id,
        user_id=lic.user_id,
        user_email=lic.user.email if lic.user else None,
        status=lic.status,
        current_period_end=lic.current_period_end,
        plan=lic.plan,
        created_at=lic.created_at,
        devices_count=active_count,
    )


@app.get("/admin/devices", response_model=list[AdminDeviceResponse])
def admin_list_all_devices(
    license_id: int | None = None,
    active_only: bool = False,
    db: Session = Depends(get_db),
    admin: User = Depends(get_current_admin),
):
    stmt = select(Device)
    if license_id:
        stmt = stmt.where(Device.license_id == license_id)
    if active_only:
        stmt = stmt.where(Device.active == True)
    devs = db.scalars(stmt.order_by(Device.id.desc())).all()
    admin_actor = f"admin:{admin.id}" if isinstance(admin, User) else "admin"
    _audit(db, admin_actor, "list_all_devices", "devices", f"count:{len(devs)}")
    db.commit()
    return devs


@app.get("/admin/licenses/{license_id}/devices", response_model=list[AdminDeviceResponse])
def admin_list_devices(
    license_id: int,
    db: Session = Depends(get_db),
    admin: User = Depends(get_current_admin),
):
    lic = db.get(License, license_id)
    if lic is None:
        raise HTTPException(status_code=404, detail="LICENSE_NOT_FOUND")
    devs = db.scalars(select(Device).where(Device.license_id == license_id).order_by(Device.id.desc())).all()
    admin_actor = f"admin:{admin.id}" if isinstance(admin, User) else "admin"
    _audit(db, admin_actor, "list_devices", f"license:{license_id}", f"count:{len(devs)}")
    db.commit()
    return devs


@app.delete("/admin/devices/{device_id}")
def admin_release_device(
    device_id: int,
    db: Session = Depends(get_db),
    admin: User = Depends(get_current_admin),
):
    dev = db.get(Device, device_id)
    if dev is None:
        raise HTTPException(status_code=404, detail="DEVICE_NOT_FOUND")
    lic_id = dev.license_id
    dev.active = False
    dev.released_at = utcnow()
    db.execute(delete(DeviceAccessLog).where(DeviceAccessLog.license_id == lic_id))
    admin_actor = f"admin:{admin.id}" if isinstance(admin, User) else "admin"
    _audit(db, admin_actor, "release_device", f"device:{device_id}", f"license:{lic_id}")
    db.commit()
    return {"ok": True, "device_id": device_id, "status": "released"}


@app.get("/admin/abuse", response_model=list[AdminAbuseFlagResponse])
def admin_list_abuse_flags(
    status: str | None = None,
    license_id: int | None = None,
    db: Session = Depends(get_db),
    admin: User = Depends(get_current_admin),
):
    stmt = select(AbuseFlag)
    if status:
        stmt = stmt.where(AbuseFlag.status == status)
    if license_id:
        stmt = stmt.where(AbuseFlag.license_id == license_id)
    flags = db.scalars(stmt.order_by(AbuseFlag.id.desc())).all()
    admin_actor = f"admin:{admin.id}" if isinstance(admin, User) else "admin"
    _audit(db, admin_actor, "list_abuse_flags", "abuse_flags", f"count:{len(flags)}")
    db.commit()
    return flags


@app.patch("/admin/abuse/{flag_id}", response_model=AdminAbuseFlagResponse)
def admin_review_abuse_flag(
    flag_id: int,
    req: AdminReviewAbuseFlagRequest,
    db: Session = Depends(get_db),
    admin: User = Depends(get_current_admin),
):
    flag = db.get(AbuseFlag, flag_id)
    if flag is None:
        raise HTTPException(status_code=404, detail="ABUSE_FLAG_NOT_FOUND")
    old_status = flag.status
    flag.status = req.status
    if req.notes:
        flag.details = f"{flag.details} | Nota Admin: {req.notes}".strip(" |")
    admin_actor = f"admin:{admin.id}" if isinstance(admin, User) else "admin"
    _audit(db, admin_actor, "review_abuse_flag", f"abuse_flag:{flag_id}", f"{old_status}->{flag.status}")
    db.commit()
    return flag


@app.get("/admin/vm-allowlist", response_model=list[AdminVMAllowlistEntryResponse])
def admin_list_vm_allowlist(
    license_id: int | None = None,
    db: Session = Depends(get_db),
    admin: User = Depends(get_current_admin),
):
    stmt = select(VMAllowlist)
    if license_id:
        stmt = stmt.where(VMAllowlist.license_id == license_id)
    entries = db.scalars(stmt.order_by(VMAllowlist.id.desc())).all()
    admin_actor = f"admin:{admin.id}" if isinstance(admin, User) else "admin"
    _audit(db, admin_actor, "list_vm_allowlist", "vm_allowlist", f"count:{len(entries)}")
    db.commit()
    return entries


@app.post("/admin/vm-allowlist", response_model=AdminVMAllowlistEntryResponse)
def admin_add_vm_allowlist(
    req: AdminVMAllowlistRequest,
    db: Session = Depends(get_db),
    admin: User = Depends(get_current_admin),
):
    lic = db.get(License, req.license_id)
    if lic is None:
        raise HTTPException(status_code=404, detail="LICENSE_NOT_FOUND")
    entry = db.scalar(
        select(VMAllowlist).where(
            VMAllowlist.license_id == req.license_id,
            VMAllowlist.fingerprint_hash == req.fingerprint,
        )
    )
    if entry:
        entry.active = True
        entry.notes = req.notes
    else:
        entry = VMAllowlist(
            license_id=req.license_id,
            fingerprint_hash=req.fingerprint,
            notes=req.notes,
            active=True,
        )
        db.add(entry)
        db.flush()
    admin_actor = f"admin:{admin.id}" if isinstance(admin, User) else "admin"
    _audit(db, admin_actor, "add_vm_allowlist", f"license:{req.license_id}", req.fingerprint[:16])
    db.commit()
    return {
        "ok": True,
        "id": entry.id,
        "license_id": entry.license_id,
        "fingerprint_hash": entry.fingerprint_hash,
        "notes": entry.notes,
        "active": entry.active,
        "created_at": entry.created_at,
    }


@app.delete("/admin/vm-allowlist/{entry_id}")
def admin_remove_vm_allowlist(
    entry_id: int,
    db: Session = Depends(get_db),
    admin: User = Depends(get_current_admin),
):
    entry = db.get(VMAllowlist, entry_id)
    if entry is None:
        raise HTTPException(status_code=404, detail="ENTRY_NOT_FOUND")
    entry.active = False
    admin_actor = f"admin:{admin.id}" if isinstance(admin, User) else "admin"
    _audit(db, admin_actor, "remove_vm_allowlist", f"vm_allowlist:{entry_id}", f"license:{entry.license_id}")
    db.commit()
    return {"ok": True, "entry_id": entry_id, "status": "deactivated"}


@app.get("/admin/users", response_model=list[AdminUserResponse])
def admin_list_users(
    search: str | None = None,
    db: Session = Depends(get_db),
    admin: User = Depends(get_current_admin),
):
    stmt = select(User)
    if search:
        s_clean = search.strip().lower()[:100]
        esc = s_clean.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_")
        stmt = stmt.where(User.email.ilike(f"%{esc}%", escape="\\"))
    users = db.scalars(stmt.order_by(User.id.desc())).all()
    admin_actor = f"admin:{admin.id}" if isinstance(admin, User) else "admin"
    _audit(db, admin_actor, "list_users", "users", f"count:{len(users)}")
    db.commit()
    return [
        AdminUserResponse(
            id=u.id,
            email=u.email,
            status=u.status,
            is_admin=u.is_admin,
            totp_enabled=u.totp_enabled,
            created_at=u.created_at,
            licenses_count=len(u.licenses),
            must_change_password=bool(u.must_change_password),
            cpf_masked=_safe_masked_cpf(u),
        )
        for u in users
    ]


def _safe_masked_cpf(u: User) -> str:
    try:
        return mask_cpf(crypto.cpf_decrypt(u.cpf_enc))
    except Exception:
        return ""


@app.get("/admin/users/{user_id}/cpf")
def admin_reveal_user_cpf(user_id: int, db: Session = Depends(get_db), admin: User = Depends(get_current_admin)):
    """Revela o CPF completo de uma conta. Cada consulta fica na trilha de auditoria (LGPD)."""
    target = db.get(User, user_id)
    if target is None:
        raise HTTPException(status_code=404, detail="USER_NOT_FOUND")
    try:
        digits = crypto.cpf_decrypt(target.cpf_enc)
    except Exception:
        raise HTTPException(status_code=500, detail="CPF_UNAVAILABLE")
    _audit(db, f"admin:{admin.id}", "reveal_cpf", f"user:{user_id}")
    db.commit()
    formatted = f"{digits[:3]}.{digits[3:6]}.{digits[6:9]}-{digits[9:]}" if len(digits) == 11 else digits
    return {"user_id": user_id, "cpf": formatted}


@app.post("/admin/users", response_model=AdminCreateUserResponse, status_code=201)
def admin_create_user(req: AdminCreateUserRequest, db: Session = Depends(get_db),
                      admin: User = Depends(get_current_admin)):
    """Cria conta + licenca. Senha temporaria (gerada se nao informada) com troca obrigatoria no 1o login;
    CPF guardado cifrado na conta; 2FA configurado pelo proprio usuario no 1o login (assistente com QR Code)."""
    email = normalize_email(req.email)
    if db.scalar(select(User).where(User.email == email)) is not None:
        raise HTTPException(status_code=409, detail="EMAIL_ALREADY_EXISTS")
    password = req.password or generate_temp_password()
    try:
        prov = create_user_with_license(db, email, password, req.cpf, days=req.days, enroll_2fa=False,
                                        must_change_password=True, plan=req.plan)
    except ProvisionError as e:
        db.rollback()
        raise HTTPException(status_code=422, detail=str(e).upper().replace(" ", "_"))
    except IntegrityError:
        db.rollback()
        raise HTTPException(status_code=409, detail="EMAIL_ALREADY_EXISTS")
    _audit(db, f"admin:{admin.id}", "create_user", f"user:{prov.user.id}", f"license:{prov.license.id},plan:{req.plan},days:{req.days}")
    db.commit()
    return AdminCreateUserResponse(user_id=prov.user.id, license_id=prov.license.id, email=email,
                                   temporary_password=password)


@app.post("/admin/users/{user_id}/reset-password", response_model=AdminResetPasswordResponse)
def admin_reset_password(user_id: int, db: Session = Depends(get_db), admin: User = Depends(get_current_admin)):
    target = db.get(User, user_id)
    if target is None:
        raise HTTPException(status_code=404, detail="USER_NOT_FOUND")
    pw = generate_temp_password()
    target.password_hash = crypto.hash_password(pw)
    target.must_change_password = True
    target.failed_logins = 0
    target.locked_until = None
    _audit(db, f"admin:{admin.id}", "reset_password", f"user:{user_id}")
    db.commit()
    return AdminResetPasswordResponse(user_id=user_id, temporary_password=pw)


@app.post("/admin/users/{user_id}/reset-2fa")
def admin_reset_2fa(user_id: int, db: Session = Depends(get_db), admin: User = Depends(get_current_admin)):
    """Usuario perdeu o autenticador: limpa o 2FA; no proximo login o assistente (QR Code) reaparece."""
    target = db.get(User, user_id)
    if target is None:
        raise HTTPException(status_code=404, detail="USER_NOT_FOUND")
    if target.is_admin:
        raise HTTPException(status_code=403, detail="CANNOT_RESET_ADMIN_2FA")   # admin: so pelo servidor (CLI)
    target.totp_enabled = False
    target.totp_secret_enc = None
    target.totp_last_step = 0
    _audit(db, f"admin:{admin.id}", "reset_2fa", f"user:{user_id}")
    db.commit()
    return {"ok": True, "user_id": user_id}


@app.post("/admin/users/{user_id}/unlock")
def admin_unlock_user(user_id: int, db: Session = Depends(get_db), admin: User = Depends(get_current_admin)):
    target = db.get(User, user_id)
    if target is None:
        raise HTTPException(status_code=404, detail="USER_NOT_FOUND")
    target.failed_logins = 0
    target.locked_until = None
    _audit(db, f"admin:{admin.id}", "unlock_user", f"user:{user_id}")
    db.commit()
    return {"ok": True, "user_id": user_id}


def _like_escape(v: str) -> str:
    return v.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_")


@app.get("/admin/telemetry", response_model=list[AdminTelemetryResponse])
def admin_list_telemetry(
    email: str | None = None, ip: str | None = None, license_id: int | None = None,
    event: str | None = None, hours: int = 168, limit: int = 200,
    db: Session = Depends(get_db), admin: User = Depends(get_current_admin),
):
    hours = max(1, min(hours, 24 * 365))
    limit = max(1, min(limit, 1000))
    stmt = select(TelemetryEvent).where(TelemetryEvent.ts >= utcnow() - timedelta(hours=hours))
    if email:
        stmt = stmt.where(TelemetryEvent.email.like(f"%{_like_escape(email.strip().lower()[:100])}%", escape="\\"))
    if ip:
        stmt = stmt.where(TelemetryEvent.ip == ip.strip()[:64])
    if license_id:
        stmt = stmt.where(TelemetryEvent.license_id == license_id)
    if event:
        stmt = stmt.where(TelemetryEvent.event == event[:32])
    rows = db.scalars(stmt.order_by(TelemetryEvent.id.desc()).limit(limit)).all()
    _audit(db, f"admin:{admin.id}", "view_telemetry", "telemetry", f"count:{len(rows)}")
    db.commit()
    return rows


@app.get("/admin/telemetry/signals", response_model=list[AdminFraudSignal])
def admin_telemetry_signals(hours: int = 24, db: Session = Depends(get_db), admin: User = Depends(get_current_admin)):
    return telemetry.fraud_signals(db, max(1, min(hours, 24 * 30)))


@app.get("/admin/metrics", response_model=AdminMetricsResponse)
def admin_get_metrics(
    db: Session = Depends(get_db),
    admin: User = Depends(get_current_admin),
):
    now = utcnow()
    seven_days_ago = now - timedelta(days=7)

    lics = db.scalars(select(License)).all()
    lic_total = len(lics)
    lic_active = 0
    lic_suspended = 0
    lic_revoked = 0
    lic_expired = 0
    for l in lics:
        if l.status == "suspensa":
            lic_suspended += 1
        elif l.status == "revogada":
            lic_revoked += 1
        elif l.status == "expirada" or l.current_period_end < now:
            lic_expired += 1
        elif l.status == "ativa":
            lic_active += 1

    devs = db.scalars(select(Device)).all()
    dev_total = len(devs)
    dev_active = len([d for d in devs if d.active])

    abuse_flags = db.scalars(select(AbuseFlag)).all()
    abuse_total = len(abuse_flags)
    abuse_open = len([f for f in abuse_flags if f.status == "aberto"])
    abuse_open_7d = len([f for f in abuse_flags if f.status == "aberto" and f.ts >= seven_days_ago])

    recent_logs = db.scalars(
        select(AuditLog)
        .where(AuditLog.action.in_(["login", "admin_login"]))
        .order_by(AuditLog.id.desc())
        .limit(10)
    ).all()

    recent_logins = [
        AdminRecentLogin(
            id=log.id,
            actor=log.actor,
            action=log.action,
            target=log.target,
            ts=log.ts,
        )
        for log in recent_logs
    ]

    admin_actor = f"admin:{admin.id}" if isinstance(admin, User) else "admin"
    _audit(db, admin_actor, "get_metrics", "metrics")
    db.commit()

    return AdminMetricsResponse(
        licenses=AdminMetricsLicenses(
            total=lic_total,
            active=lic_active,
            suspended=lic_suspended,
            revoked=lic_revoked,
            expired=lic_expired,
        ),
        devices=AdminMetricsDevices(
            total=dev_total,
            active=dev_active,
        ),
        abuse=AdminMetricsAbuse(
            total=abuse_total,
            total_open=abuse_open,
            open_7d=abuse_open_7d,
        ),
        recent_logins=recent_logins,
    )


# ---------------- Interface Web Administrativa (SPA estatica) ----------------
from pathlib import Path
_ui_dist = Path(__file__).resolve().parents[1] / "admin-ui" / "dist"


@app.get("/admin", include_in_schema=False)
@app.get("/admin/", include_in_schema=False)
def serve_admin_index():
    index_file = _ui_dist / "index.html"
    if index_file.is_file():
        return FileResponse(str(index_file))
    return HTMLResponse(
        "<h1>Structura Admin UI em construção. Execute 'npm run build' em server/admin-ui</h1>",
        status_code=503,
    )


@app.get("/admin/assets/{asset_path:path}", include_in_schema=False)
def serve_admin_asset(asset_path: str):
    # Anti path-traversal: o caminho resolvido TEM que ficar dentro de dist/assets
    # (antes, /admin/assets/..%2f..%2f..%2f.env devolvia o .env com as chaves do servidor).
    base = (_ui_dist / "assets").resolve()
    try:
        asset_file = (base / asset_path).resolve()
        asset_file.relative_to(base)
    except (ValueError, OSError):
        raise HTTPException(status_code=404, detail="ASSET_NOT_FOUND")
    if asset_file.is_file():
        return FileResponse(str(asset_file), headers={"Cache-Control": "public, max-age=3600"})
    raise HTTPException(status_code=404, detail="ASSET_NOT_FOUND")


# ---------------- loja, area do cliente, cobranca (app/portal.py) ----------------
from .portal import router as _portal_router, seed_plans as _seed_plans  # noqa: E402  (apos as dependencias acima)
app.include_router(_portal_router)
_seed_plans()

# Site estatico em desenvolvimento (em producao quem serve e o Caddy). Fica por ultimo: rotas da API tem prioridade.
if settings.SITE_DIR and Path(settings.SITE_DIR).is_dir():
    app.mount("/", StaticFiles(directory=settings.SITE_DIR, html=True), name="site")
