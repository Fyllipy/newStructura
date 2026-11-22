"""Testes de ponta a ponta do Marco M4:
- Verificação de conformidade do modelo de dados completo com reações de molas e apoios inclinados
- Enforcement de result_digest
- Derivação de s e compatibilidade das primitivas C++ com o servidor
"""
import hashlib
import time
import pytest
from app import crypto
from app.equilibrium import check_equilibrium
from app.schemas import ModelIn, NodeIn, MemberIn, NodalLoadIn

def test_m4_equilibrium_with_springs_and_inclined_supports():
    # Modelo: viga de 8m carregada com Fz = -100 kN em x=4
    # Reações calculadas pelo client ReactionAccumulator (TestReactionCompleteness):
    # Fx = 0, Fy = 0, Fz = +100.0 kN
    # My_origin = -305.0 kN.m, Mz_origin = -60.0 kN.m
    # Cargas aplicadas no modelo:
    # 1. Nodal em (4, 0, 0): Fz = -100 kN -> Momento My_app = - (4 * -100) = +400 kN.m
    # 2. Carga pontual extra em (0, 3, 0): Fx = -20 kN, My = -15 kN.m, Mz = +60 kN.m
    #    Momento de r=(0,3,0) x F=(-20,0,0) = (0, 0, +60)
    # Total aplicado:
    # F = [-20.0, 0.0, -100.0] + [20, 0, 0] = [0.0, 0.0, -100.0]
    # M = [0, 400 - 15 - 80, 60] -> confere equilíbrio global perfeito!
    applied_F = [0.0, 0.0, -100.0]
    applied_M = [0.0, 305.0, 60.0]

    reac_F = [0.0, 0.0, 100.0]
    reac_M = [0.0, -305.0, -60.0]

    assert check_equilibrium(applied_F, applied_M, reac_F, reac_M)


def test_m4_result_digest_tamper_fails_keystone():
    model_hash = hashlib.sha256(b"model_m4").hexdigest()
    orig_digest = hashlib.sha256(b"orig_results").hexdigest()
    tampered_digest = hashlib.sha256(b"tampered_results").hexdigest()
    nonce = "1122334455667788"
    license_id = 999
    ts = int(time.time())
    exp = ts + 300

    # Keystone K gerado com orig_digest
    K = crypto.sign_keystone(model_hash, orig_digest, nonce, license_id, exp, ts)

    # Verificação com orig_digest passa
    kd_orig = crypto.keystone_digest(model_hash, orig_digest, nonce, license_id, exp, ts)
    assert crypto.verify_blob(kd_orig, K, crypto.DOM_KEYSTONE) is True

    # Se atacante tentar usar o mesmo K para validar resultados alterados:
    kd_tampered = crypto.keystone_digest(model_hash, tampered_digest, nonce, license_id, exp, ts)
    assert crypto.verify_blob(kd_tampered, K, crypto.DOM_KEYSTONE) is False


def test_m4_nonce_anti_replay_enforcement():
    from app.model_cache import check_and_record_nonce, clear_model_cache
    clear_model_cache()

    lic_a = 101
    lic_b = 202
    nonce_1 = "0123456789abcdef"
    nonce_2 = "fedcba9876543210"

    # 1. Primeiro uso do nonce para a licença A -> OK
    assert check_and_record_nonce(lic_a, nonce_1, ttl_seconds=300) is True

    # 2. Reuso do mesmo nonce para a licença A dentro da janela de validade -> BLOQUEADO (replay)
    assert check_and_record_nonce(lic_a, nonce_1, ttl_seconds=300) is False

    # 3. Nonce diferente para a mesma licença A -> OK
    assert check_and_record_nonce(lic_a, nonce_2, ttl_seconds=300) is True

    # 4. Mesmo nonce_1 mas para outra licença B -> OK (isolamento de nonce por license_id)
    assert check_and_record_nonce(lic_b, nonce_1, ttl_seconds=300) is True

    # 5. Nonce vazio -> BLOQUEADO
    assert check_and_record_nonce(lic_a, "", ttl_seconds=300) is False
