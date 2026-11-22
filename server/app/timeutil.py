"""Autoridade de tempo = servidor. UTC ingenuo (compativel com as colunas DateTime do DB)."""
from __future__ import annotations
from datetime import datetime, timezone


def utcnow() -> datetime:
    return datetime.now(timezone.utc).replace(tzinfo=None)


def iso(dt: datetime) -> str:
    return dt.replace(microsecond=0).isoformat() + "Z"


def iso_ts(ts: int) -> str:
    return iso(datetime.fromtimestamp(ts, timezone.utc).replace(tzinfo=None))
