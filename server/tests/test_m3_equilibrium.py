"""M3 — Validação de cálculo (equilíbrio) + Keystone K:
- Recomposição do lado aplicado (cargas nodais + cargas de barra locais/globais com rotação)
- Check de equilíbrio rigoroso e fail-closed
- Assinatura de K via Ed25519 e verificação
- Derivação de s = HKDF-SHA256(K, salt=nonce, info="finalize"||license_id)
- Cache por model_hash (memória e Redis mock)
"""
import hashlib
import time
from datetime import timedelta
import pytest
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes
from nacl.signing import VerifyKey

from app import crypto, totp
from app.db import SessionLocal
from app.equilibrium import applied_resultant, check_equilibrium, compute_member_orientation, transform_local_to_global
from app.model_cache import get_model_cache, set_model_cache, clear_model_cache
from app.schemas import (
    ModelIn, NodeIn, MemberIn, NodalLoadIn, MemberLoadIn, ReactionTotals,
    MemberMomentIn, MemberActionIn
)
from app.users import create_user_with_license
from helpers import INTEG

EMAIL, PW, CPF = "m3@exemplo.com", "senha-forte-m3-123", "12345678909"
FP1 = "e" * 64


@pytest.fixture()
def acct(client):
    db = SessionLocal()
    p = create_user_with_license(db, EMAIL, PW, CPF)
    db.commit()
    out = {"user_id": p.user.id, "license_id": p.license.id, "secret": p.totp_secret}
    db.close()
    return out


def get_token(client, acct):
    r = client.post("/auth/login", json={
        "email": EMAIL, "password": PW, "cpf": CPF, "fingerprint": FP1,
        "integrity": dict(INTEG), "totp": totp.code_at(acct["secret"])
    })
    assert r.status_code == 200
    return r.json()["license_token"]


def derive_s(K_b64: str, nonce: str, license_id: int) -> bytes:
    """Replica a derivação do cliente C++: s = HKDF-SHA256(ikm=K, salt=nonce, info="finalize"||license_id)"""
    K_bytes = crypto._b64d(K_b64)
    info = f"finalize{license_id}".encode("utf-8")
    salt = nonce.encode("utf-8")
    hkdf = HKDF(algorithm=hashes.SHA256(), length=32, salt=salt, info=info)
    return hkdf.derive(K_bytes)


# -----------------------------------------------------------------------------
# 1. Testes de Orientação e Transformação de Cargas de Barra
# -----------------------------------------------------------------------------
def test_member_orientation_horizontal_x():
    # Barra ao longo do eixo X global: nós (0,0,0) -> (5,0,0)
    ri = (0.0, 0.0, 0.0)
    rj = (5.0, 0.0, 0.0)
    x_loc, y_loc, z_loc, L = compute_member_orientation(ri, rj, 0.0)
    assert L == 5.0
    assert x_loc == [1.0, 0.0, 0.0]
    # Pela convenção Sennett com Z global como referência K:
    # z_local = x_local x K = (1,0,0) x (0,0,1) = (0, -1, 0)
    # y_local = z_local x x_local = (0,-1,0) x (1,0,0) = (0, 0, 1)
    assert y_loc == [0.0, 0.0, 1.0]
    assert z_loc == [0.0, -1.0, 0.0]


def test_member_orientation_vertical_z():
    # Coluna vertical ao longo de Z: (0,0,0) -> (0,0,3)
    ri = (0.0, 0.0, 0.0)
    rj = (0.0, 0.0, 3.0)
    x_loc, y_loc, z_loc, L = compute_member_orientation(ri, rj, 0.0)
    assert L == 3.0
    assert x_loc == [0.0, 0.0, 1.0]
    # K escolhe Y global (0,1,0):
    # z_local = (0,0,1) x (0,1,0) = (-1,0,0)
    # y_local = (-1,0,0) x (0,0,1) = (0,1,0)
    assert y_loc == [0.0, 1.0, 0.0]
    assert z_loc == [-1.0, 0.0, 0.0]


def test_member_orientation_with_rotation_90deg():
    ri = (0.0, 0.0, 0.0)
    rj = (5.0, 0.0, 0.0)
    _, y0, z0, _ = compute_member_orientation(ri, rj, 0.0)
    _, y90, z90, _ = compute_member_orientation(ri, rj, 90.0)
    # Rotação de 90° em torno de x: y' = z0, z' = -y0
    for k in range(3):
        assert abs(y90[k] - z0[k]) < 1e-9
        assert abs(z90[k] - (-y0[k])) < 1e-9


def test_transform_local_to_global():
    x = [1.0, 0.0, 0.0]
    y = [0.0, 0.0, 1.0]
    z = [0.0, -1.0, 0.0]
    # Carga local [10, 20, 30]: 10 ao longo de x (X), 20 ao longo de y (Z), 30 ao longo de z (-Y)
    q_glob = transform_local_to_global([10.0, 20.0, 30.0], x, y, z)
    assert q_glob == [10.0, -30.0, 20.0]


# -----------------------------------------------------------------------------
# 2. Recomposição de Esforços Aplicados (applied_resultant)
# -----------------------------------------------------------------------------
def test_applied_resultant_nodal_and_member_loads():
    # Viga biapoiada em balanço de 4m ao longo de X (nó 1=(0,0,0), nó 2=(4,0,0))
    # Carga nodal em 2: Fz = -10 kN
    # Carga distribuída de barra local: qy = -2 kN/m (na viga ao longo de X, y_local é +Z global)
    # Logo qy local = -2 gera -2 kN/m em Z global.
    # Resultante de barra: Rz = -2 * 4 = -8 kN no meio da barra (x=2).
    # Total aplicado: Fz = -10 + (-8) = -18 kN.
    # Momento em torno da origem My: Fz_nodal * x_nodal + Rz_barra * x_mid = (-10 * 4) + (-8 * 2) = -40 - 16 = -56 kN.m
    # Como momento M = r x F: (x, 0, 0) x (0, 0, Fz) -> My = -x * Fz.
    # r x F = [0, -x*Fz, 0] => My = - (4 * -10 + 2 * -8) = - (-40 - 16) = +56 kN.m.
    model = ModelIn(
        nodes=[NodeIn(id=1, x=0.0, y=0.0, z=0.0), NodeIn(id=2, x=4.0, y=0.0, z=0.0)],
        members=[MemberIn(id=1, node_i=1, node_j=2, rotation_deg=0.0)],
        nodal_loads=[NodalLoadIn(node_id=2, F=[0.0, 0.0, -10.0])],
        member_loads=[MemberLoadIn(member_id=1, coord_system=0, q=[0.0, -2.0, 0.0])]
    )
    F, M = applied_resultant(model)
    assert abs(F[0]) < 1e-9 and abs(F[1]) < 1e-9
    assert abs(F[2] - (-18.0)) < 1e-9
    assert abs(M[1] - 56.0) < 1e-9


# -----------------------------------------------------------------------------
# 3. Validação do Equilíbrio (check_equilibrium)
# -----------------------------------------------------------------------------
def test_check_equilibrium_precision():
    app_F = [0.0, 0.0, -100.0]
    app_M = [0.0, 250.0, 0.0]
    # Reações exatas
    reac_F = [0.0, 0.0, 100.0]
    reac_M = [0.0, -250.0, 0.0]
    assert check_equilibrium(app_F, app_M, reac_F, reac_M)

    # Reações desequilibradas além da tolerância
    bad_reac_F = [0.0, 0.0, 99.0]
    assert not check_equilibrium(app_F, app_M, bad_reac_F, reac_M)


# -----------------------------------------------------------------------------
# 4. Cache do Modelo (Redis fallback e em memória)
# -----------------------------------------------------------------------------
def test_model_cache_get_set():
    clear_model_cache()
    h = "test_hash_" + "1" * 54
    assert get_model_cache(h) is None
    set_model_cache(h, [1.0, 2.0, 3.0], [4.0, 5.0, 6.0])
    cached = get_model_cache(h)
    assert cached is not None
    assert cached[0] == [1.0, 2.0, 3.0]
    assert cached[1] == [4.0, 5.0, 6.0]


# -----------------------------------------------------------------------------
# 5. Endpoint POST /compute/validate e Keystone K
# -----------------------------------------------------------------------------
def test_compute_validate_success_with_model(client, acct):
    tok = get_token(client, acct)
    model = ModelIn(
        nodes=[NodeIn(id=1, x=0.0, y=0.0, z=0.0), NodeIn(id=2, x=2.0, y=0.0, z=0.0)],
        members=[MemberIn(id=1, node_i=1, node_j=2)],
        nodal_loads=[NodalLoadIn(node_id=2, F=[0.0, 0.0, -10.0])],
        member_loads=[]
    )
    # Carga Fz = -10 kN em x=2 -> My_aplicado = - (2 * -10) = +20 kN.m
    # Reações equilibradas: Fz = +10 kN, My = -20 kN.m
    model_hash = hashlib.sha256(b"unique_model_1").hexdigest()
    result_digest = hashlib.sha256(b"results_def_1").hexdigest()
    nonce = "1234567890abcdef"

    req = {
        "license_token": tok,
        "nonce": nonce,
        "fingerprint": FP1,
        "integrity": dict(INTEG),
        "model_hash": model_hash,
        "model": model.model_dump(),
        "reaction_totals": {"F": [0.0, 0.0, 10.0], "M_origin": [0.0, -20.0, 0.0]},
        "result_digest": result_digest
    }
    r = client.post("/compute/validate", json=req)
    assert r.status_code == 200
    data = r.json()
    assert data["ok"] is True
    assert data["K"] is not None

    # Verifica assinatura de K com a chave pública do servidor
    pub = client.get("/pubkey").json()["server_pubkey_b64"]
    vk = VerifyKey(crypto._b64d(pub))
    assert isinstance(data["ts"], int)
    assert isinstance(data["exp"], int)
    assert data["exp"] == data["ts"] + 300
    k_sig = crypto._b64d(data["K"])
    kd = crypto.keystone_digest(model_hash, result_digest, nonce, acct["license_id"], data["exp"], data["ts"])
    vk.verify(crypto.DOM_KEYSTONE + kd, k_sig)

    # Derivação de s pelo cliente funciona
    s = derive_s(data["K"], nonce, acct["license_id"])
    assert len(s) == 32


def test_compute_validate_uses_cache_on_second_call(client, acct):
    tok = get_token(client, acct)
    model = ModelIn(
        nodes=[NodeIn(id=1, x=0.0, y=0.0, z=0.0), NodeIn(id=2, x=1.0, y=0.0, z=0.0)],
        members=[MemberIn(id=1, node_i=1, node_j=2)],
        nodal_loads=[NodalLoadIn(node_id=2, F=[5.0, 0.0, 0.0])],
    )
    model_hash = hashlib.sha256(b"cached_model_test").hexdigest()
    nonce = "aabbccddeeff0011"

    # Primeira chamada: envia model
    r1 = client.post("/compute/validate", json={
        "license_token": tok,
        "nonce": nonce,
        "fingerprint": FP1,
        "integrity": dict(INTEG),
        "model_hash": model_hash,
        "model": model.model_dump(),
        "reaction_totals": {"F": [-5.0, 0.0, 0.0], "M_origin": [0.0, 0.0, 0.0]},
        "result_digest": "a" * 64
    })
    assert r1.status_code == 200 and r1.json()["ok"] is True

    # Segunda chamada: SEM model (model=None) -> deve usar cache com sucesso
    r2 = client.post("/compute/validate", json={
        "license_token": tok,
        "nonce": nonce + "_2",
        "fingerprint": FP1,
        "integrity": dict(INTEG),
        "model_hash": model_hash,
        "model": None,
        "reaction_totals": {"F": [-5.0, 0.0, 0.0], "M_origin": [0.0, 0.0, 0.0]},
        "result_digest": "b" * 64
    })
    assert r2.status_code == 200 and r2.json()["ok"] is True


def test_compute_validate_fails_closed_when_equilibrium_broken(client, acct):
    tok = get_token(client, acct)
    model = ModelIn(
        nodes=[NodeIn(id=1, x=0.0, y=0.0, z=0.0), NodeIn(id=2, x=1.0, y=0.0, z=0.0)],
        members=[MemberIn(id=1, node_i=1, node_j=2)],
        nodal_loads=[NodalLoadIn(node_id=2, F=[10.0, 0.0, 0.0])],
    )
    # Reação falsificada/incorreta
    r = client.post("/compute/validate", json={
        "license_token": tok,
        "nonce": "1122334455667788",
        "fingerprint": FP1,
        "integrity": dict(INTEG),
        "model_hash": hashlib.sha256(b"m_bad_eq").hexdigest(),
        "model": model.model_dump(),
        "reaction_totals": {"F": [0.0, 0.0, 0.0], "M_origin": [0.0, 0.0, 0.0]},
        "result_digest": "c" * 64
    })
    assert r.status_code == 200
    res = r.json()
    assert res["ok"] is False
    assert res["reason"] == "EQUILIBRIUM_FAIL"
    assert res.get("K") is None


def test_compute_validate_missing_model_without_cache_returns_model_required(client, acct):
    tok = get_token(client, acct)
    r = client.post("/compute/validate", json={
        "license_token": tok,
        "nonce": "1122334455667788",
        "fingerprint": FP1,
        "integrity": dict(INTEG),
        "model_hash": hashlib.sha256(b"unknown_uncached_hash").hexdigest(),
        "model": None,
        "reaction_totals": {"F": [0.0, 0.0, 0.0], "M_origin": [0.0, 0.0, 0.0]},
        "result_digest": "d" * 64
    })
    assert r.status_code == 200
    assert r.json()["ok"] is False
    assert r.json()["reason"] == "MODEL_REQUIRED"
    assert r.json().get("K") is None


# -----------------------------------------------------------------------------
# 6. Testes do PATCH (M3) — Completude das Cargas Aplicadas
# -----------------------------------------------------------------------------
def test_patch_point_load_at_third_span():
    # Viga biapoiada de 6m ao longo de X: nós (0,0,0) -> (6,0,0)
    # Carga pontual vertical (kind 1) em a = 2.0 (L/3), Fz = -12 kN (em Y_local = +Z global)
    # Força pontual: Fz = -12 kN aplicada em r = (2, 0, 0)
    # Momento em torno da origem: r x F = (2, 0, 0) x (0, 0, -12) -> My = - (2 * -12) = +24 kN.m
    # Reações no nó 1 (x=0) e nó 2 (x=6):
    # R1_z = +8 kN, R2_z = +4 kN => Total Fz = +12 kN.
    # Momento das reações em torno da origem: r2 x R2 = (6, 0, 0) x (0, 0, 4) -> My_reac = - (6 * 4) = -24 kN.m.
    model = ModelIn(
        nodes=[NodeIn(id=1, x=0.0, y=0.0, z=0.0), NodeIn(id=2, x=6.0, y=0.0, z=0.0)],
        members=[MemberIn(id=1, node_i=1, node_j=2)],
        member_actions=[MemberActionIn(member_id=1, kind=1, coord_system=0, a=2.0, v1=[0.0, -12.0, 0.0])]
    )
    F, M = applied_resultant(model)
    assert abs(F[2] - (-12.0)) < 1e-9
    assert abs(M[1] - 24.0) < 1e-9
    assert check_equilibrium(F, M, [0.0, 0.0, 12.0], [0.0, -24.0, 0.0])


def test_patch_applied_point_moment():
    # Viga de 4m ao longo de X. Momento pontual aplicado (kind 2) em a = 2.0: My = 10 kN.m
    # Binário puro livre: resultante de força nula, momento My = 10 kN.m
    # Reações biapoiadas: R1_z = -2.5 kN, R2_z = +2.5 kN (binário de -10 kN.m)
    # Momento das reações em torno da origem: r2 x R2 = (4, 0, 0) x (0, 0, 2.5) -> My = - (4 * 2.5) = -10 kN.m.
    model = ModelIn(
        nodes=[NodeIn(id=1, x=0.0, y=0.0, z=0.0), NodeIn(id=2, x=4.0, y=0.0, z=0.0)],
        members=[MemberIn(id=1, node_i=1, node_j=2)],
        member_actions=[MemberActionIn(member_id=1, kind=2, coord_system=1, a=2.0, v1=[0.0, 10.0, 0.0])]
    )
    F, M = applied_resultant(model)
    assert abs(F[0]) < 1e-9 and abs(F[1]) < 1e-9 and abs(F[2]) < 1e-9
    assert abs(M[1] - 10.0) < 1e-9
    assert check_equilibrium(F, M, [0.0, 0.0, 0.0], [0.0, -10.0, 0.0])


def test_patch_trapezoidal_partial_load():
    # Viga de 6m ao longo de X. Carga trapezoidal parcial (kind 0) de a=1 a b=4 (span=3).
    # v1 = -2 kN/m, v2 = -8 kN/m (em Z global).
    # R_analitico = (v1 + v2)/2 * span = (-2 + -8)/2 * 3 = -15 kN.
    # Distância do centro de gravidade do trapézio a partir de a:
    # d_cg = span/3 * (v1 + 2*v2) / (v1 + v2) = 1.0 * (-2 - 16) / (-10) = 1.8 m.
    # Posição global do CG: x_cg = a + d_cg = 1.0 + 1.8 = 2.8 m.
    # Momento em torno da origem My: - (x_cg * R) = - (2.8 * -15) = +42 kN.m.
    model = ModelIn(
        nodes=[NodeIn(id=1, x=0.0, y=0.0, z=0.0), NodeIn(id=2, x=6.0, y=0.0, z=0.0)],
        members=[MemberIn(id=1, node_i=1, node_j=2)],
        member_actions=[MemberActionIn(
            member_id=1, kind=0, coord_system=1, a=1.0, b=4.0,
            v1=[0.0, 0.0, -2.0], v2=[0.0, 0.0, -8.0]
        )]
    )
    F, M = applied_resultant(model)
    assert abs(F[2] - (-15.0)) < 1e-9
    assert abs(M[1] - 42.0) < 1e-9
    # Reações equilibrando
    assert check_equilibrium(F, M, [0.0, 0.0, 15.0], [0.0, -42.0, 0.0])


def test_patch_uniform_member_moment():
    # Momento uniforme distribuído de barra m = [0, 5.0, 0] ao longo de viga de 4m
    # Resultante: F = 0, M = m * L = 5.0 * 4 = 20 kN.m
    model = ModelIn(
        nodes=[NodeIn(id=1, x=0.0, y=0.0, z=0.0), NodeIn(id=2, x=4.0, y=0.0, z=0.0)],
        members=[MemberIn(id=1, node_i=1, node_j=2)],
        member_moments=[MemberMomentIn(member_id=1, coord_system=1, m=[0.0, 5.0, 0.0])]
    )
    F, M = applied_resultant(model)
    assert abs(F[0]) < 1e-9 and abs(F[1]) < 1e-9 and abs(F[2]) < 1e-9
    assert abs(M[1] - 20.0) < 1e-9
    assert check_equilibrium(F, M, [0.0, 0.0, 0.0], [0.0, -20.0, 0.0])


def test_patch_thermal_load_is_self_equilibrating():
    # Deformação térmica / imposta (kind 3): autoequilibrada, resultante externa nula
    model = ModelIn(
        nodes=[NodeIn(id=1, x=0.0, y=0.0, z=0.0), NodeIn(id=2, x=5.0, y=0.0, z=0.0)],
        members=[MemberIn(id=1, node_i=1, node_j=2)],
        member_actions=[MemberActionIn(
            member_id=1, kind=3, coord_system=0, a=0.0, v1=[0.001, 20.0, 0.0]
        )]
    )
    F, M = applied_resultant(model)
    assert abs(F[0]) < 1e-9 and abs(F[1]) < 1e-9 and abs(F[2]) < 1e-9
    assert abs(M[0]) < 1e-9 and abs(M[1]) < 1e-9 and abs(M[2]) < 1e-9
    # Em estrutura restrita, as reações estáticas somam zero (ex: reações opostas R1=-R2)
    assert check_equilibrium(F, M, [0.0, 0.0, 0.0], [0.0, 0.0, 0.0])


def test_patch_inclined_member_local_vs_global_equivalence():
    # Barra inclinada de 45° no plano XZ: (0,0,0) -> (4,0,4). L = sqrt(32) ~= 5.656854 m
    # Eixo local x = [1/sqrt(2), 0, 1/sqrt(2)]
    # Vetor de referência K = Z global [0,0,1]
    # z_local = x x K = [1/sqrt(2), 0, 1/sqrt(2)] x [0, 0, 1] = [0, -1/sqrt(2), 0] -> normalizado [0, -1, 0]
    # y_local = z x x = [0, -1, 0] x [1/sqrt(2), 0, 1/sqrt(2)] = [-1/sqrt(2), 0, 1/sqrt(2)]
    # Se aplicarmos carga pontual transversal no meio da barra (a = L/2):
    # No sistema local: v1_local = [0, 10.0, 0] (ao longo de y_local)
    # No sistema global equivalente: v1_global = 10.0 * [-1/sqrt(2), 0, 1/sqrt(2)] = [-7.0710678, 0, 7.0710678]
    model_local = ModelIn(
        nodes=[NodeIn(id=1, x=0.0, y=0.0, z=0.0), NodeIn(id=2, x=4.0, y=0.0, z=4.0)],
        members=[MemberIn(id=1, node_i=1, node_j=2)],
        member_actions=[MemberActionIn(member_id=1, kind=1, coord_system=0, a=2.0, v1=[0.0, 10.0, 0.0])]
    )
    F_loc, M_loc = applied_resultant(model_local)

    v1_glob = [-10.0 / (2.0**0.5), 0.0, 10.0 / (2.0**0.5)]
    model_global = ModelIn(
        nodes=[NodeIn(id=1, x=0.0, y=0.0, z=0.0), NodeIn(id=2, x=4.0, y=0.0, z=4.0)],
        members=[MemberIn(id=1, node_i=1, node_j=2)],
        member_actions=[MemberActionIn(member_id=1, kind=1, coord_system=1, a=2.0, v1=v1_glob)]
    )
    F_glob, M_glob = applied_resultant(model_global)

    for k in range(3):
        assert abs(F_loc[k] - F_glob[k]) < 1e-9
        assert abs(M_loc[k] - M_glob[k]) < 1e-9

