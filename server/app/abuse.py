"""Modulo de Deteccao de Abuso (Marco M6):
- Concorrencia simultanea (<30s com IPs/fingerprints distintos)
- Velocity / Viagem impossivel
- Dispersao de fingerprints (>3 em 24h)
- Deteccao de VM/Sandbox com Allowlist de Admin
- Auto-suspensao por N AbuseFlags em 7 dias (settings.AUTO_SUSPEND_FLAG_THRESHOLD, padrao 5)
- Emissao de marcas d'agua forenses assinadas pelo servidor
"""
from __future__ import annotations
import hashlib
import hmac
import time
from datetime import timedelta
from typing import Tuple, Optional

from sqlalchemy import select, func, or_
from sqlalchemy.orm import Session

from .config import settings
from .manifest import _b64d
from .models import AbuseFlag, VMAllowlist, DeviceAccessLog, License, AuditLog, User
from .timeutil import utcnow

DOM_WATERMARK = b"structura-watermark-v1\x00"


def _get_watermark_key() -> bytes:
    """Deriva chave HMAC exclusiva para marca d'agua forense com dominio proprio."""
    seed_str = getattr(settings, "SERVER_SIGNING_SEED_B64", "")
    seed = _b64d(seed_str) if seed_str else b"structura-watermark-default-seed-bytes"
    return hmac.new(seed, DOM_WATERMARK, hashlib.sha256).digest()


def issue_watermark_token(license_id: int, export_type: str, ts: Optional[int] = None) -> str:
    """Assina token de marca d'agua forense autoritativo pelo servidor (M6).
    Formato: STRUCTURA-WM1:{license_id}:{ts}:{export_type}:{sig_truncada}"""
    t = int(time.time()) if ts is None else ts
    payload = f"LIC:{license_id}|TS:{t}|TYPE:{export_type}".encode("utf-8")
    sig = hmac.new(_get_watermark_key(), DOM_WATERMARK + payload, hashlib.sha256).hexdigest()[:32]
    return f"STRUCTURA-WM1:{license_id}:{t}:{export_type}:{sig}"


def verify_watermark_token(token: str) -> Tuple[bool, int, str, int]:
    """Verifica e decodifica a marca d'agua forense para rastreamento pericial."""
    parts = token.strip().split(":")
    if len(parts) != 5 or parts[0] != "STRUCTURA-WM1":
        return False, 0, "", 0
    try:
        lic_id = int(parts[1])
        ts = int(parts[2])
        exp_type = parts[3]
        sig = parts[4]
    except (ValueError, IndexError):
        return False, 0, "", 0

    expected = issue_watermark_token(lic_id, exp_type, ts)
    expected_sig = expected.split(":")[-1]
    ok = hmac.compare_digest(sig, expected_sig)
    return ok, lic_id, exp_type, ts


def record_abuse_flag(db: Session, license_id: int, kind: str, score: float = 1.0, details: str = "") -> AbuseFlag:
    """Registra flag de abuso e aplica a regra de suspensao automatica:
    Se acumular 3 ou mais flags de qualquer tipo em 7 dias, a licenca muda para 'suspensa'."""
    flag = AbuseFlag(
        license_id=license_id,
        kind=kind,
        score=score,
        details=details,
        status="aberto",
        ts=utcnow(),
    )
    db.add(flag)
    db.flush()

    # Suspensao automatica: N flags em janela movel de 7 dias (aviso na flag N-1)
    seven_days_ago = utcnow() - timedelta(days=7)
    recent_flag_count = db.scalar(
        select(func.count(AbuseFlag.id))
        .where(AbuseFlag.license_id == license_id, AbuseFlag.ts >= seven_days_ago)
    ) or 0

    limit = settings.AUTO_SUSPEND_FLAG_THRESHOLD
    if recent_flag_count == limit - 1 and limit > 1:
        db.add(AuditLog(actor="system", action="abuse_warning", target=f"license:{license_id}",
                        metadata_masked=f"{recent_flag_count}/{limit} flags em 7 dias; proxima suspende (ultimo: {kind})"))
    if recent_flag_count >= limit:
        lic = db.get(License, license_id)
        if lic and lic.status != "suspensa":
            lic.status = "suspensa"
            db.add(AuditLog(
                actor="system",
                action="auto_suspend_license",
                target=f"license:{license_id}",
                metadata_masked=f"Auto-suspensao: {recent_flag_count} flags de abuso em 7 dias (ultimo: {kind})"
            ))

    db.commit()
    return flag


def check_vm_authorized(
    db: Session,
    license_id: int,
    fingerprint: str,
    is_vm: bool,
    vm_vendor: str = "",
    is_admin: bool = False,
) -> Tuple[bool, str, Optional[str]]:
    """Valida se o ambiente virtualizado esta expressamente autorizado na allowlist de admin."""
    if not is_vm:
        return True, "OK", None

    # Se nao passado explicitamente, verifica se o titular da licenca eh administrador
    if not is_admin:
        is_admin_db = db.scalar(
            select(User.is_admin)
            .join(License, License.user_id == User.id)
            .where(License.id == license_id)
        )
        if is_admin_db:
            is_admin = True

    # Administrador tem autorizacao irrestrita de execucao em qualquer ambiente (fisico ou VM)
    if is_admin:
        allow_entry = db.scalar(
            select(VMAllowlist).where(
                VMAllowlist.license_id == license_id,
                VMAllowlist.fingerprint_hash == fingerprint,
            )
        )
        if allow_entry is None:
            allow_entry = VMAllowlist(
                license_id=license_id,
                fingerprint_hash=fingerprint,
                notes="Admin auto-allowlist",
                active=True,
                created_at=utcnow(),
            )
            db.add(allow_entry)
            db.commit()
        elif not allow_entry.active:
            allow_entry.active = True
            db.commit()

        token_payload = f"vm-allow:{license_id}:{fingerprint}".encode("utf-8")
        vm_token = hmac.new(_get_watermark_key(), token_payload, hashlib.sha256).hexdigest()[:32]
        return True, "VM_AUTHORIZED", vm_token

    allow_entry = db.scalar(
        select(VMAllowlist).where(
            VMAllowlist.license_id == license_id,
            VMAllowlist.fingerprint_hash == fingerprint,
            VMAllowlist.active.is_(True),
        )
    )

    if allow_entry is not None:
        # Gera token assinado de allowlist para neutralizar o poison no cliente
        token_payload = f"vm-allow:{license_id}:{fingerprint}".encode("utf-8")
        vm_token = hmac.new(_get_watermark_key(), token_payload, hashlib.sha256).hexdigest()[:32]
        return True, "VM_AUTHORIZED", vm_token

    # VM nao autorizada: registra abuso e bloqueia
    record_abuse_flag(db, license_id, "unauthorized_vm", details=f"Execucao em VM ({vm_vendor or 'desconhecido'}) sem allowlist")
    return False, "VM_NOT_ALLOWED", None


def record_access_and_check_abuse(db: Session, license_id: int, fingerprint: str, ip_address: str, action: str) -> Tuple[bool, str]:
    """Registra telemetria de acesso e inspeciona:
    1. Concorrencia (<30s com outro IP ou fingerprint)
    2. Velocity / Viagem impossivel (<10s com saltos de rede discrepantes)
    3. Excessive fingerprints (>3 fingerprints distintos em 24h)
    """
    now = utcnow()
    ip_clean = ip_address.strip() if ip_address else "127.0.0.1"

    # 1. Checagem de Velocity / Viagem Impossivel (<10s entre sub-redes distintas)
    last_access = db.scalar(
        select(DeviceAccessLog)
        .where(DeviceAccessLog.license_id == license_id)
        .order_by(DeviceAccessLog.id.desc())
    )
    if last_access and last_access.ip_address != ip_clean:
        dt = (now - last_access.ts).total_seconds()
        def _get_prefix(ip: str) -> str:
            return ip.split(".")[0] if "." in ip else ip.split(":")[0]

        if dt < 10 and _get_prefix(last_access.ip_address) != _get_prefix(ip_clean):
            record_abuse_flag(
                db, license_id, "velocity",
                details=f"Salto de rede impossivel em {dt:.1f}s ({last_access.ip_address} -> {ip_clean})"
            )
            return False, "VELOCITY_LIMIT_EXCEEDED"

    # 2. Checagem de Concorrencia (<30s para a mesma licenca com IPs/fingerprints distintos)
    window_30s = now - timedelta(seconds=30)
    concurrent_log = db.scalar(
        select(DeviceAccessLog)
        .where(
            DeviceAccessLog.license_id == license_id,
            DeviceAccessLog.ts >= window_30s,
            or_(
                DeviceAccessLog.fingerprint_hash != fingerprint,
                DeviceAccessLog.ip_address != ip_clean,
            ),
        )
    )
    if concurrent_log is not None:
        record_abuse_flag(
            db, license_id, "concurrent_access",
            details=f"Concorrencia em <30s: IP {ip_clean} (vs {concurrent_log.ip_address}), FP {fingerprint[:8]} (vs {concurrent_log.fingerprint_hash[:8]})"
        )
        return False, "CONCURRENT_ACCESS_DETECTED"

    # 3. Checagem de Excessive Fingerprints (>3 em 24h)
    window_24h = now - timedelta(hours=24)
    distinct_fps = db.scalars(
        select(func.distinct(DeviceAccessLog.fingerprint_hash))
        .where(DeviceAccessLog.license_id == license_id, DeviceAccessLog.ts >= window_24h)
    ).all()

    if fingerprint not in distinct_fps and len(distinct_fps) >= 3:
        record_abuse_flag(
            db, license_id, "excessive_fingerprints",
            details=f"Tentativa de registrar 4o fingerprint em 24h: {fingerprint[:16]}..."
        )
        return False, "EXCESSIVE_FINGERPRINTS"

    # Acesso conforme: registra no historico
    db.add(DeviceAccessLog(
        license_id=license_id,
        fingerprint_hash=fingerprint,
        ip_address=ip_clean,
        action=action,
        ts=now
    ))
    db.commit()
    return True, "OK"
