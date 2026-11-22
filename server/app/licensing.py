"""Regras de licenca e vinculo de dispositivo (1 maquina ativa por licenca)."""
from __future__ import annotations
import hmac
from dataclasses import dataclass
from datetime import timedelta
from sqlalchemy.orm import Session
from sqlalchemy import select
from .models import License, Device, User
from .config import settings
from .timeutil import utcnow
from . import crypto


def license_is_usable(lic: License) -> bool:
    return lic.status == "ativa" and lic.current_period_end >= utcnow()


def active_device(db: Session, license_id: int) -> Device | None:
    return db.scalar(
        select(Device).where(Device.license_id == license_id, Device.active.is_(True))
    )


def pick_license(db: Session, user_id: int) -> License | None:
    """Licenca utilizavel mais recente do usuario (ou None)."""
    lics = db.scalars(select(License).where(License.user_id == user_id).order_by(License.id.desc())).all()
    return next((l for l in lics if license_is_usable(l)), None)


def bind_or_check_device(db: Session, lic: License, fingerprint: str) -> tuple[bool, str, Device | None]:
    """Retorna (ok, reason, device). Garante no maximo 1 dispositivo ativo por licenca.
    - sem vinculo ativo -> vincula este fingerprint
    - mesmo fingerprint -> ok (atualiza heartbeat)
    - fingerprint diferente e heartbeat recente -> DEVICE_IN_USE
    - fingerprint diferente e heartbeat >= DEVICE_STALE_DAYS -> libera e re-vincula
    O indice unico parcial do DB cobre a corrida entre logins simultaneos.
    """
    dev = active_device(db, lic.id)
    now = utcnow()
    if dev is None:
        new = Device(license_id=lic.id, fingerprint_hash=fingerprint,
                     active=True, bound_at=now, last_heartbeat_at=now)
        db.add(new)
        db.flush()
        return True, "bound", new
    if hmac.compare_digest(dev.fingerprint_hash, fingerprint):
        dev.last_heartbeat_at = now
        return True, "ok", dev
    if now - dev.last_heartbeat_at >= timedelta(days=settings.DEVICE_STALE_DAYS):
        dev.active = False
        dev.released_at = now
        db.flush()  # libera o indice unico antes de inserir o novo
        new = Device(license_id=lic.id, fingerprint_hash=fingerprint,
                     active=True, bound_at=now, last_heartbeat_at=now)
        db.add(new)
        db.flush()
        return True, "rebound_stale", new
    return False, "DEVICE_IN_USE", None


def release_device_by_id(db: Session, device_id: int) -> bool:
    dev = db.get(Device, device_id)
    if dev is not None and dev.active:
        dev.active = False
        dev.released_at = utcnow()
        return True
    return False


@dataclass
class AuthCtx:
    user: User
    license: License
    device: Device
    payload: dict


def authenticate_token(db: Session, token: str, fingerprint: str,
                       allow_expired: bool = False) -> AuthCtx | None:
    """Autoriza TODO uso de token (heartbeat/compute). Fail-closed: qualquer divergencia -> None.
    Alem da assinatura/exp, exige no DB: device ATIVO e igual ao fingerprint, licenca usavel,
    usuario ativo. Logout, suspensao, revogacao e rebind tem efeito imediato (token nao e a verdade)."""
    p = crypto.verify_token(token, allow_expired=allow_expired)
    if not p:
        return None
    try:
        did, lid, uid, fp = int(p["did"]), int(p["license_id"]), int(p["user_id"]), str(p["fp"])
    except (KeyError, ValueError, TypeError):
        return None
    if not (hmac.compare_digest(fp, fingerprint or "")):
        return None
    dev = db.get(Device, did)
    if dev is None or not dev.active or dev.license_id != lid:
        return None
    if not hmac.compare_digest(dev.fingerprint_hash, fingerprint):
        return None
    lic = db.get(License, lid)
    if lic is None or lic.user_id != uid or not license_is_usable(lic):
        return None
    user = db.get(User, uid)
    if user is None or user.status != "ativa":
        return None
    return AuthCtx(user=user, license=lic, device=dev, payload=p)
