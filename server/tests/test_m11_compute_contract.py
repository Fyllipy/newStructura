"""Regressao: /compute/validate com o JSON EXATO que o cliente C++ (SolverBridge.cpp::
buildModelJsonAndReactions) envia. O servidor devolvia 422 (-> COMPUTE_VALIDATE_REJECTED no app)
porque o cliente manda cargas nodais como {"node_id","values":[Fx,Fy,Fz,Mx,My,Mz]} e membros com
"section_rotation_deg", enquanto o schema exigia "F"/"M" e "rotation_deg"."""
import pytest

from app import totp
from app.db import SessionLocal
from app.users import create_user_with_license
from helpers import INTEG

EMAIL, PW, CPF, FP = "calc@exemplo.com", "uma-senha-forte-123", "12345678909", "f" * 64


@pytest.fixture()
def token(client):
    db = SessionLocal()
    p = create_user_with_license(db, EMAIL, PW, CPF)
    db.commit()
    secret = p.totp_secret
    db.close()
    r = client.post("/auth/login", json={"email": EMAIL, "password": PW, "cpf": CPF, "fingerprint": FP,
                                          "integrity": dict(INTEG), "totp": totp.code_at(secret)})
    assert r.status_code == 200, r.text
    return r.json()["license_token"]


def _req(token, model, F, M, nonce="0123456789abcdef"):
    return {"license_token": token, "fingerprint": FP, "integrity": dict(INTEG), "nonce": nonce,
            "model_hash": "h" * 64, "result_digest": "d" * 64, "model": model,
            "reaction_totals": {"F": F, "M_origin": M}}


def test_client_format_nodal_loads_and_rotation(client, token):
    model = {"nodes": [{"id": 1, "x": 0.0, "y": 0.0, "z": 0.0}, {"id": 2, "x": 3.0, "y": 0.0, "z": 0.0}],
             "members": [{"id": 1, "node_i": 1, "node_j": 2, "section_rotation_deg": 30.0}],
             "nodal_loads": [{"node_id": 2, "values": [1.0, 2.0, -10.0, 0.5, 0.0, 0.25]}],
             "member_loads": [], "member_moments": [], "member_actions": []}
    # aplicado: F=(1,2,-10); M = r x F + M = (0*-10-0*2+0.5, 0*1-3*-10+0, 3*2-0*1+0.25) = (0.5, 30, 6.25)
    r = client.post("/compute/validate", json=_req(token, model, [-1.0, -2.0, 10.0], [-0.5, -30.0, -6.25]))
    assert r.status_code == 200, r.text
    assert r.json()["ok"] is True and r.json()["K"]


def test_unbalanced_reactions_still_rejected(client, token):
    model = {"nodes": [{"id": 1, "x": 0.0, "y": 0.0, "z": 0.0}, {"id": 2, "x": 3.0, "y": 0.0, "z": 0.0}],
             "members": [{"id": 1, "node_i": 1, "node_j": 2}],
             "nodal_loads": [{"node_id": 2, "values": [0.0, 0.0, -10.0, 0.0, 0.0, 0.0]}]}
    r = client.post("/compute/validate", json=_req(token, model, [0.0, 0.0, 7.0], [0.0, 21.0, 0.0], nonce="aaaaaaaaaaaaaaaa"))
    assert r.status_code == 200 and r.json()["ok"] is False and r.json()["reason"] == "EQUILIBRIUM_FAIL"


def test_degenerate_member_is_clean_failure_not_500(client, token):
    model = {"nodes": [{"id": 1, "x": 0.0, "y": 0.0, "z": 0.0}, {"id": 2, "x": 0.0, "y": 0.0, "z": 0.0}],
             "members": [{"id": 1, "node_i": 1, "node_j": 2}],
             "member_loads": [{"member_id": 1, "coord_system": 1, "q": [0.0, 0.0, -1.0]}]}
    r = client.post("/compute/validate", json=_req(token, model, [0, 0, 0], [0, 0, 0], nonce="bbbbbbbbbbbbbbbb"))
    assert r.status_code == 200 and r.json()["ok"] is False
