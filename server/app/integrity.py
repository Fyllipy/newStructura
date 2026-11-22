"""Atestacao server-side: compara a atestacao do cliente com o manifesto ASSINADO publicado."""
from __future__ import annotations
import hashlib
import hmac
from enum import Enum

from sqlalchemy import select, update
from sqlalchemy.orm import Session

from . import manifest as mf
from .config import settings
from .models import IntegrityManifest
from .schemas import Integrity

ALLOWED_STATUS = ("current", "allowed")
_STATUS = ("current", "allowed", "revoked")

# cache de manifestos ja verificados: (version, sha256(text+sig)) -> Manifest
_VERIFIED: dict[tuple[int, str], mf.Manifest] = {}


class IntegrityResult(str, Enum):
    OK = "OK"
    FAIL = "FAIL"            # divergencia / manifesto desconhecido / assinatura invalida
    OUTDATED = "OUTDATED"    # versao revogada -> atualizar o cliente


def _load(row: IntegrityManifest) -> mf.Manifest | None:
    key = (row.version, hashlib.sha256((row.text + "\0" + row.sig).encode()).hexdigest())
    m = _VERIFIED.get(key)
    if m is None:
        try:
            m = mf.load_verified(row.text, row.sig, settings.RELEASE_PUBKEY_B64)
        except mf.ManifestError:
            return None
        if m.version != row.version:   # linha do DB trocada por outro manifesto assinado valido
            return None
        _VERIFIED[key] = m
    return m


def check_attestation(db: Session, att: Integrity) -> IntegrityResult:
    """Fail-closed: qualquer excecao/ausencia -> FAIL."""
    try:
        row = db.get(IntegrityManifest, att.manifest_version)
        if row is None or row.status not in _STATUS:
            return IntegrityResult.FAIL
        m = _load(row)
        if m is None:
            return IntegrityResult.FAIL
        if row.status == "revoked":
            return IntegrityResult.OUTDATED
        ok = (hmac.compare_digest(m.root, att.files_root)
              and att.file_count == m.file_count
              and hmac.compare_digest(m.exe_hash, att.exe_hash))
        return IntegrityResult.OK if ok else IntegrityResult.FAIL
    except Exception:
        return IntegrityResult.FAIL


# ---------------- publicacao e chaves de versao (CLI/admin/M5) ----------------
def derive_dll_key_for_version(version: int, master_seed_b64: str | None = None) -> str:
    """Deriva chave de 256 bits (64 hex chars) estavel e deterministica por versao de manifesto (M5)."""
    seed_str = master_seed_b64 or getattr(settings, "SERVER_SIGNING_SEED_B64", "")
    seed = mf._b64d(seed_str) if seed_str else b"structura-dll-default-seed-bytes"
    return hmac.new(seed, f"structura-dll-manifest-v{version}".encode(), hashlib.sha256).hexdigest()


def get_or_create_dll_key(db: Session, version: int, custom_key: str | None = None) -> str:
    """Retorna ou cria e persiste a DLL_RELEASE_KEY para uma versao especifica."""
    row = db.get(IntegrityManifest, version)
    if row and row.dll_release_key:
        return row.dll_release_key
    key = custom_key or derive_dll_key_for_version(version)
    if row:
        row.dll_release_key = key
        db.commit()
    return key


def publish(db: Session, text: str, sig: str, status: str = "current", dll_key: str | None = None, overwrite: bool = False) -> mf.Manifest:
    """So aceita manifesto com assinatura de release valida. Nova versao nao pode sobrescrever a menos que overwrite=True."""
    if status not in ALLOWED_STATUS:
        raise mf.ManifestError("status")
    m = mf.load_verified(text, sig, settings.RELEASE_PUBKEY_B64)
    row = db.get(IntegrityManifest, m.version)
    if row is not None:
        if not overwrite:
            raise mf.ManifestError("versao ja publicada")  # imutavel: nada de rollback silencioso
        actual_key = dll_key or row.dll_release_key or derive_dll_key_for_version(m.version)
        row.text = text
        row.sig = sig
        row.status = status
        row.dll_release_key = actual_key
        return m
    actual_key = dll_key or derive_dll_key_for_version(m.version)
    db.add(IntegrityManifest(version=m.version, text=text, sig=sig, status=status, dll_release_key=actual_key))
    return m


def set_status(db: Session, version: int, status: str) -> bool:
    if status not in _STATUS:
        raise mf.ManifestError("status")
    res = db.execute(update(IntegrityManifest).where(IntegrityManifest.version == version)
                     .values(status=status))
    return res.rowcount == 1


def list_versions(db: Session) -> list[tuple[int, str]]:
    return [(r.version, r.status) for r in db.scalars(select(IntegrityManifest).order_by(IntegrityManifest.version))]
