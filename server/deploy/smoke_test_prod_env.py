"""Teste de fumaca da configuracao de producao, SEM tocar no banco real: carrega secrets/prod.env em
memoria, troca o DATABASE_URL por SQLite em memoria e confere boot, /health, /pubkey (== chave embutida
no cliente), docs desligados e cabecalhos de seguranca.  Uso: python server/deploy/smoke_test_prod_env.py"""
from __future__ import annotations
import base64
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

for line in (HERE / "secrets" / "prod.env").read_text(encoding="utf-8").splitlines():
    if line and not line.startswith("#") and "=" in line:
        k, v = line.split("=", 1)
        os.environ[k] = v
os.environ["DATABASE_URL"] = "sqlite://"

from fastapi.testclient import TestClient  # noqa: E402
from app.main import app  # noqa: E402

expected = (HERE / "secrets" / "server_pubkey.hex").read_text().strip()
with TestClient(app) as c:
    assert c.get("/health").json() == {"status": "ok", "db": "ok"}
    pk = c.get("/pubkey").json()["server_pubkey_b64"]
    got = base64.urlsafe_b64decode(pk + "=" * (-len(pk) % 4)).hex()
    assert got == expected, "pubkey do servidor difere da embutida no cliente"
    assert c.get("/docs").status_code == 404 and c.get("/openapi.json").status_code == 404
    h = c.get("/health").headers
    assert h["x-frame-options"] == "DENY" and h["cache-control"] == "no-store"
    assert c.get("/admin/assets/..%2f..%2f.env").status_code == 404
print("OK: configuracao de producao valida (boot, pubkey == cliente, docs off, headers, traversal bloqueado)")
