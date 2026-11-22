"""Telemetria de antifraude: registro de eventos e sinais de abuso consultaveis no painel.

Principios (LGPD): so dados necessarios para identificar uso fraudulento (conta, IP observado pelo servidor,
maquina, versao); nada de senha, CPF ou conteudo de projetos; expurgo automatico apos
settings.TELEMETRY_RETENTION_DAYS; o usuario e informado na tela de login."""
from __future__ import annotations
from datetime import timedelta

from sqlalchemy import delete, func, select
from sqlalchemy.orm import Session

from .config import settings
from .models import TelemetryEvent
from .schemas import AdminFraudSignal, TelemetryIn
from .timeutil import utcnow

_counter = 0


def _clip(v: str | None, n: int) -> str:
    return (v or "").replace("\r", " ").replace("\n", " ")[:n]


def record(db: Session, event: str, ip: str, *, user_id: int | None = None, license_id: int | None = None,
           email: str = "", fingerprint: str = "", tel: TelemetryIn | None = None, is_vm: bool = False,
           vm_vendor: str = "", user_agent: str = "", detail: str = "", commit: bool = True) -> None:
    """Grava um evento. Nunca deve derrubar o fluxo de autenticacao: qualquer erro e engolido."""
    global _counter
    try:
        t = tel or TelemetryIn()
        db.add(TelemetryEvent(
            event=_clip(event, 32), user_id=user_id, license_id=license_id, email=_clip(email, 254).lower(),
            ip=_clip(ip, 64), fingerprint=_clip(fingerprint, 64), app_version=_clip(t.app_version, 32),
            os_version=_clip(t.os_version, 64), hostname=_clip(t.hostname, 64), cpu=_clip(t.cpu, 96),
            locale=_clip(t.locale, 16), timezone=_clip(t.timezone, 48), is_vm=bool(is_vm),
            vm_vendor=_clip(vm_vendor, 64), user_agent=_clip(user_agent, 128), detail=_clip(detail, 128)))
        if commit:
            db.commit()
        _counter += 1
        if _counter % 500 == 0:
            purge(db)
    except Exception:
        db.rollback()


def purge(db: Session) -> int:
    horizon = utcnow() - timedelta(days=settings.TELEMETRY_RETENTION_DAYS)
    res = db.execute(delete(TelemetryEvent).where(TelemetryEvent.ts < horizon))
    db.commit()
    return res.rowcount or 0


def fraud_signals(db: Session, hours: int = 24) -> list[AdminFraudSignal]:
    """Heuristicas simples sobre a janela recente (a decisao final e do administrador)."""
    since = utcnow() - timedelta(hours=hours)
    out: list[AdminFraudSignal] = []
    E = TelemetryEvent

    rows = db.execute(select(E.ip, func.count(func.distinct(E.user_id))).where(
        E.ts >= since, E.user_id.is_not(None), E.ip != "").group_by(E.ip)
        .having(func.count(func.distinct(E.user_id)) >= 3)).all()
    out += [AdminFraudSignal(kind="ip_many_accounts", subject=ip, count=n,
                             detail=f"{n} contas distintas no mesmo IP em {hours}h") for ip, n in rows]

    rows = db.execute(select(E.license_id, func.count(func.distinct(E.ip))).where(
        E.ts >= since, E.license_id.is_not(None), E.ip != "").group_by(E.license_id)
        .having(func.count(func.distinct(E.ip)) >= 4)).all()
    out += [AdminFraudSignal(kind="account_many_ips", subject=f"license:{lid}", count=n,
                             detail=f"{n} IPs distintos em {hours}h (possivel compartilhamento)") for lid, n in rows]

    rows = db.execute(select(E.license_id, func.count(func.distinct(E.fingerprint))).where(
        E.ts >= since, E.license_id.is_not(None), E.fingerprint != "").group_by(E.license_id)
        .having(func.count(func.distinct(E.fingerprint)) >= 2)).all()
    out += [AdminFraudSignal(kind="account_many_machines", subject=f"license:{lid}", count=n,
                             detail=f"{n} maquinas distintas em {hours}h") for lid, n in rows]

    rows = db.execute(select(E.ip, func.count(E.id)).where(
        E.ts >= since, E.event == "login_failed").group_by(E.ip).having(func.count(E.id) >= 10)).all()
    out += [AdminFraudSignal(kind="failed_burst", subject=ip, count=n,
                             detail=f"{n} logins falhos em {hours}h (forca bruta?)") for ip, n in rows]
    return sorted(out, key=lambda s: -s.count)
