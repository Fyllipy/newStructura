"""Renovacao automatica de licencas VENCIDAS (rodar de hora em hora: systemd timer / cron).

    python -m app.renew_cli

So cobra licencas cujo periodo ja terminou, com renovacao ligada e cartao cadastrado. Seguro rodar em paralelo ou
repetido: a chave de idempotencia por licenca+vencimento+tentativa impede cobranca dupla.
"""
from __future__ import annotations
import json
import sys

from .config import settings
from .db import SessionLocal, Base, engine
from . import models  # noqa: F401
from .billing import run_renewals
from .payments import get_provider, PaymentError


def main() -> int:
    settings.validate_secrets()
    Base.metadata.create_all(bind=engine)
    try:
        provider = get_provider()
    except PaymentError as e:
        print(f"ERRO: {e}", file=sys.stderr)
        return 2
    db = SessionLocal()
    try:
        print(json.dumps(run_renewals(db, provider)))
    finally:
        db.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
