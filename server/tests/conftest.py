"""Fixtures: DB SQLite em memoria e chaves efemeras geradas por teste (nada em disco/git)."""
import base64
import os
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


def _b64(b: bytes) -> str:
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode()


# Precisa estar no ambiente ANTES de importar app.config (Settings le no import).
os.environ.setdefault("SERVER_SIGNING_SEED_B64", _b64(os.urandom(32)))
os.environ.setdefault("CPF_ENC_KEY_B64", _b64(os.urandom(32)))
os.environ.setdefault("CPF_HMAC_KEY_B64", _b64(os.urandom(32)))
os.environ["DATABASE_URL"] = "sqlite://"  # em memoria


def _release_keys():
    from nacl.signing import SigningKey
    rk = SigningKey.generate()
    os.environ["TEST_RELEASE_SEED_B64"] = _b64(rk.encode())
    os.environ["RELEASE_PUBKEY_B64"] = _b64(bytes(rk.verify_key))


_release_keys()  # chave de release efemera; a privada so existe no processo de teste


@pytest.fixture()
def client():
    from fastapi.testclient import TestClient
    from app.main import app
    from app.model_cache import clear_model_cache
    from app.db import Base, engine, SessionLocal
    from app import models, integrity  # noqa: F401
    import helpers
    Base.metadata.drop_all(bind=engine)
    Base.metadata.create_all(bind=engine)
    clear_model_cache()
    from app import ratelimit
    ratelimit.reset()
    integrity._VERIFIED.clear()
    db = SessionLocal()
    integrity.publish(db, helpers.TEXT, helpers.SIG, "current")  # manifesto v1 publicado
    db.commit()
    db.close()
    with TestClient(app) as c:
        yield c
