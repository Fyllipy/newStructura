"""Testes do Marco M5: Chave de DLL vinculada a versao de manifesto, entrega no login e invalidacao por revogacao."""
import hashlib
import os
import sys
import uuid
from pathlib import Path
import pytest
from nacl.secret import SecretBox
from nacl.exceptions import CryptoError
from sqlalchemy import select
from fastapi import HTTPException

from app.db import Base, engine, SessionLocal
from app import integrity, models, crypto, totp, manifest as mf
from app.models import AuditLog, IntegrityManifest, User
from app.schemas import LoginRequest, Integrity
from app.users import create_user_with_license
from app.main import login
import helpers

FP = "a" * 64


@pytest.fixture(autouse=True)
def setup_db():
    Base.metadata.drop_all(bind=engine)
    Base.metadata.create_all(bind=engine)
    integrity._VERIFIED.clear()
    db = SessionLocal()
    integrity.publish(db, helpers.TEXT, helpers.SIG, "current")
    db.commit()
    db.close()
    yield
    Base.metadata.drop_all(bind=engine)
    Base.metadata.create_all(bind=engine)
    integrity._VERIFIED.clear()
    db = SessionLocal()
    integrity.publish(db, helpers.TEXT, helpers.SIG, "current")
    db.commit()
    db.close()


def _make_login_req(email: str, manifest_version: int, totp_secret: str) -> LoginRequest:
    code = totp.code_at(totp_secret)
    return LoginRequest(
        email=email,
        password="uma-senha-forte-123",
        cpf="12345678909",
        totp=code,
        fingerprint=FP,
        integrity=Integrity(
            manifest_version=manifest_version,
            files_root=helpers.M.root,
            file_count=helpers.M.file_count,
            exe_hash=helpers.M.exe_hash,
        ),
    )


def test_login_delivers_dll_key_for_current_manifest():
    """SPEC §5 e M5: Login autorizado com manifesto 'current' entrega dll_key vinculada a versao."""
    email = f"user_{uuid.uuid4().hex[:8]}@exemplo.com"
    db = SessionLocal()
    p = create_user_with_license(db, email, "uma-senha-forte-123", "12345678909")
    db.commit()

    expected_key = integrity.get_or_create_dll_key(db, 1)
    req = _make_login_req(email, 1, p.totp_secret)

    res = login(req, db)
    assert res.dll_key is not None
    assert res.dll_key == expected_key
    assert len(res.dll_key) == 64  # 32 bytes em hex

    # Confirma que dll_key NUNCA e registrada em texto claro no AuditLog
    logs = db.scalars(select(AuditLog)).all()
    for log in logs:
        assert res.dll_key not in (log.metadata_masked or "")
        assert res.dll_key not in log.target
    db.close()


def test_roundtrip_encrypt_and_decrypt_with_delivered_key():
    """Confirma que a chave entregue decifra com exatidao o payload cifrado para aquela versao."""
    email = f"user_{uuid.uuid4().hex[:8]}@exemplo.com"
    db = SessionLocal()
    p = create_user_with_license(db, email, "uma-senha-forte-123", "12345678909")
    db.commit()

    req = _make_login_req(email, 1, p.totp_secret)
    res = login(req, db)
    dll_key_bytes = bytes.fromhex(res.dll_key)

    # Simula arquivo de solver rds.dll cifrado
    fake_rds = b"MZ\x90\x00\x03\x00\x00\x00STRUCTURA_RDS_SOLVER_PAYLOAD"
    box = SecretBox(dll_key_bytes)
    encrypted = box.encrypt(fake_rds)

    # Decifra com a chave entregue pelo login
    decrypted = box.decrypt(encrypted)
    assert decrypted == fake_rds
    db.close()


def test_version_rotation_and_revocation_blocks_access():
    """Rotatividade de versao: Versao antiga revogada -> login 426 -> sem dll_key -> solver recusa."""
    email = f"user_{uuid.uuid4().hex[:8]}@exemplo.com"
    db = SessionLocal()
    p = create_user_with_license(db, email, "uma-senha-forte-123", "12345678909")
    db.commit()

    key_v1 = integrity.get_or_create_dll_key(db, 1)

    # 1. Publicar versao 2 do manifesto com status 'current'
    text_v2 = helpers.TEXT.replace("version 1", "version 2")
    from nacl.signing import SigningKey
    sk = SigningKey(mf._b64d(os.environ["TEST_RELEASE_SEED_B64"]))
    sig_v2 = mf._b64e(sk.sign(mf.DOM_MANIFEST + text_v2.encode("utf-8")).signature)
    integrity.publish(db, text_v2, sig_v2, status="current")

    # 2. Revogar explicitamente a versao 1
    integrity.set_status(db, 1, "revoked")
    db.commit()

    key_v2 = integrity.get_or_create_dll_key(db, 2)
    assert key_v1 != key_v2  # Chaves distintas por versao

    # 3. Payload do solver v2 cifrado com a chave da v2
    solver_v2 = b"MZ_VERSION_2_SOLVER_CORE"
    box_v2 = SecretBox(bytes.fromhex(key_v2))
    enc_v2 = box_v2.encrypt(solver_v2)

    # 4. Tentativa de login com a versao 1 (revogada)
    req_v1 = _make_login_req(email, 1, p.totp_secret)

    # O login DEVE falhar com HTTP 426 CLIENT_OUTDATED
    with pytest.raises(HTTPException) as exc_info:
        login(req_v1, db)

    assert exc_info.value.status_code == 426
    assert exc_info.value.detail == "CLIENT_OUTDATED"

    # 5. Mesmo se o atacante tiver retido key_v1 de uma sessao antiga, key_v1 NAO decifra o solver da v2
    box_v1_old = SecretBox(bytes.fromhex(key_v1))
    with pytest.raises(CryptoError):
        box_v1_old.decrypt(enc_v2)

    db.close()
