"""M2 — integridade: manifesto assinado, atestacao server-side, Nivel B (/license/check), modulos,
e conformidade cruzada Python <-> C++ (cliente)."""
import hashlib
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

import pytest
from nacl.signing import SigningKey
from sqlalchemy import select

from app import crypto, integrity, manifest as mf, modules, totp
from app.db import SessionLocal
from app.models import AuditLog, IntegrityManifest, License, User
from app.schemas import Integrity
from app.users import create_user_with_license
import helpers
from helpers import INTEG

ROOT = Path(__file__).resolve().parents[2]
SERVER = ROOT / "server"
FP1 = "c" * 64
EMAIL, PW, CPF = "m2@exemplo.com", "uma-senha-forte-123", "12345678909"
SEED = lambda: os.environ["TEST_RELEASE_SEED_B64"]  # noqa: E731
PUB = lambda: os.environ["RELEASE_PUBKEY_B64"]  # noqa: E731


# ------------------------------------------------------------------ helpers
@pytest.fixture()
def acct(client):
    db = SessionLocal()
    p = create_user_with_license(db, EMAIL, PW, CPF)
    db.commit()
    out = {"user_id": p.user.id, "license_id": p.license.id, "secret": p.totp_secret}
    db.close()
    return out


def login_body(acct, integ=None, fp=FP1):
    return {"email": EMAIL, "password": PW, "cpf": CPF, "fingerprint": fp,
            "integrity": integ or dict(INTEG), "totp": totp.code_at(acct["secret"])}


def token(client, acct):
    r = client.post("/auth/login", json=login_body(acct))
    assert r.status_code == 200, r.text
    return r.json()["license_token"]


def check(client, tok, feature="detalhamento", integ=None, nonce="ab" * 8, fp=FP1):
    return client.post("/license/check", json={"license_token": tok, "fingerprint": fp,
                                               "integrity": integ or dict(INTEG),
                                               "feature": feature, "nonce": nonce})


def att(**over):
    return Integrity(**{**INTEG, **over})


def make_release(tmp_path, with_symlink=False):
    d = tmp_path / "rel"
    (d / "lib" / "sub").mkdir(parents=True)
    (d / "user_data").mkdir()
    (d / "Structura.exe").write_bytes(b"MZ exe v1")
    (d / "rds.dll").write_bytes(b"dll rds")
    (d / "lib" / "a.dll").write_bytes(b"dll a")
    (d / "lib" / "sub" / "b.dat").write_bytes(b"data b")
    (d / "ção.dll").write_bytes(b"unicode name")
    (d / "user_data" / "prefs.ini").write_bytes(b"x=1")
    return d


def run_tool(d, version=1, monkeypatch=None):
    sys.path.insert(0, str(SERVER / "tools"))
    import make_manifest
    monkeypatch.setenv("RELEASE_SIGNING_SEED_B64", SEED())
    rc = make_manifest.main(["--dir", str(d), "--version", str(version), "--exe", "Structura.exe",
                             "--exclude", "user_data"])
    assert rc == 0
    return (d / "manifest.txt").read_text(encoding="utf-8"), (d / "manifest.sig").read_text()


# ------------------------------------------------------------------ manifesto: formato
def test_render_parse_roundtrip_and_root_stable():
    m = helpers.M
    again = mf.parse(mf.render(m))
    assert again.root == m.root and again.files == m.files and again.dirs == m.dirs


def test_root_changes_on_any_change():
    base = helpers.M.root
    f = dict(helpers.M.files); f["rds.dll"] = "0" * 64
    assert mf.compute_root(f, helpers.M.dirs) != base                 # alterado
    f = dict(helpers.M.files); f["extra.dll"] = "1" * 64
    assert mf.compute_root(f, helpers.M.dirs) != base                 # a mais
    f = dict(helpers.M.files); del f["rds.dll"]
    assert mf.compute_root(f, helpers.M.dirs) != base                 # a menos
    d = dict(helpers.M.dirs); d["pasta_vazia"] = 0
    assert mf.compute_root(helpers.M.files, d) != base                # pasta extra
    d = dict(helpers.M.dirs); d["lib"] = 2
    assert mf.compute_root(helpers.M.files, d) != base                # contagem por pasta


def _mutate(text, old, new):
    assert old in text
    return text.replace(old, new, 1)


@pytest.mark.parametrize("mut", [
    lambda t: t.replace("\n", "\r\n"),                                     # CRLF
    lambda t: t.rstrip("\n"),                                              # sem \n final
    lambda t: _mutate(t, "version 1", "version 01"),                      # zero a esquerda
    lambda t: _mutate(t, "version 1", "version +1"),
    lambda t: _mutate(t, "exe Structura.exe", "exe nao-existe.exe"),       # exe fora da lista
    lambda t: _mutate(t, "F lib/helper.dll", "F ../lib/helper.dll"),       # traversal
    lambda t: _mutate(t, "F lib/helper.dll", "F lib\\helper.dll"),         # backslash
    lambda t: _mutate(t, "F lib/helper.dll", "F /lib/helper.dll"),         # absoluto
    lambda t: _mutate(t, "D lib\t1", "D lib\t2"),                          # contagem errada
    lambda t: _mutate(t, "D .\t2", "D .\t02"),
    lambda t: t + "F zzz\t" + "a" * 64 + "\n",                             # F depois de D / nao canonico
    lambda t: _mutate(t, "STRUCTURA-MANIFEST 1", "STRUCTURA-MANIFEST 2"),
    lambda t: t.replace("exclude user_data\n", "exclude user_data\nexclude user_data\n"),  # duplicata
    lambda t: _mutate(t, "\n", "\n\n"),                                    # linha vazia
])
def test_parse_rejects_malformed(mut):
    with pytest.raises(mf.ManifestError):
        mf.parse(mut(helpers.TEXT))


def test_parse_rejects_bad_hash_and_unsorted():
    lines = helpers.TEXT.split("\n")
    bad = helpers.TEXT.replace(helpers.M.files["rds.dll"], helpers.M.files["rds.dll"].upper())
    with pytest.raises(mf.ManifestError):
        mf.parse(bad)
    fl = [i for i, l in enumerate(lines) if l.startswith("F ")]
    lines[fl[0]], lines[fl[1]] = lines[fl[1]], lines[fl[0]]
    with pytest.raises(mf.ManifestError):
        mf.parse("\n".join(lines))


# ------------------------------------------------------------------ assinatura
def test_signature_ok_and_tamper_detected():
    assert mf.verify(helpers.TEXT, helpers.SIG, PUB())
    assert not mf.verify(helpers.TEXT + " ", helpers.SIG, PUB())
    assert not mf.verify(helpers.TEXT.replace("rds.dll", "rdz.dll"), helpers.SIG, PUB())
    tampered_sig = helpers.SIG[:-2] + ("BB" if helpers.SIG.endswith("AA") else "AA")
    assert not mf.verify(helpers.TEXT, tampered_sig, PUB())
    assert not mf.verify(helpers.TEXT, "lixo", PUB())


def test_signature_other_key_rejected():
    other = mf._b64e(SigningKey.generate().encode())
    assert not mf.verify(helpers.TEXT, mf.sign(helpers.TEXT, other), PUB())


def test_signature_domain_separation():
    # assinatura SEM prefixo de dominio (ou com o de token/K/gate) nao vale como manifesto
    sk = SigningKey(mf._b64d(SEED()))
    raw = mf._b64e(sk.sign(helpers.TEXT.encode()).signature)
    assert not mf.verify(helpers.TEXT, raw, PUB())
    for dom in (crypto.DOM_TOKEN, crypto.DOM_KEYSTONE, crypto.DOM_GATE):
        s = mf._b64e(sk.sign(dom + helpers.TEXT.encode()).signature)
        assert not mf.verify(helpers.TEXT, s, PUB())
    assert mf.DOM_MANIFEST == crypto.DOM_MANIFEST


def test_server_key_cannot_sign_manifest(client):
    """A chave do SERVIDOR (comprometida) nao forja manifestos: so a chave de release vale."""
    forged = crypto.sign_blob(helpers.TEXT.encode(), crypto.DOM_MANIFEST)
    assert not mf.verify(helpers.TEXT, forged, PUB())


# ------------------------------------------------------------------ atestacao no servidor
def test_attestation_ok(client):
    db = SessionLocal()
    assert integrity.check_attestation(db, att()) == integrity.IntegrityResult.OK
    db.close()


@pytest.mark.parametrize("over", [
    {"files_root": "0" * 64}, {"file_count": 4}, {"file_count": 2}, {"exe_hash": "0" * 64},
    {"manifest_version": 2}, {"manifest_version": 999},
])
def test_attestation_mismatch_fails(client, over):
    db = SessionLocal()
    assert integrity.check_attestation(db, att(**over)) == integrity.IntegrityResult.FAIL
    db.close()


def test_no_manifest_published_fails_closed(client):
    db = SessionLocal()
    db.query(IntegrityManifest).delete()
    db.commit()
    assert integrity.check_attestation(db, att()) == integrity.IntegrityResult.FAIL
    db.close()


def test_revoked_is_outdated_not_ok(client):
    db = SessionLocal()
    assert integrity.set_status(db, 1, "revoked")
    db.commit()
    assert integrity.check_attestation(db, att()) == integrity.IntegrityResult.OUTDATED
    db.close()


def test_db_tampering_text_detected(client):
    """Atacante com escrita no DB troca o manifesto: assinatura reverificada -> FAIL."""
    db = SessionLocal()
    row = db.get(IntegrityManifest, 1)
    forged = mf.Manifest(version=1, exe="Structura.exe", exclude=["user_data"],
                         files={**helpers.M.files, "rds.dll": "e" * 64}, dirs=dict(helpers.M.dirs))
    row.text = mf.render(forged)
    db.commit()
    evil = Integrity(manifest_version=1, files_root=forged.root, file_count=forged.file_count,
                     exe_hash=forged.exe_hash)
    assert integrity.check_attestation(db, evil) == integrity.IntegrityResult.FAIL
    db.close()


def test_db_swap_version_row_detected(client):
    """Linha v2 recebe o (texto,assinatura) validos da v1: a versao interna diverge -> FAIL."""
    db = SessionLocal()
    db.add(IntegrityManifest(version=2, text=helpers.TEXT, sig=helpers.SIG, status="current"))
    db.commit()
    assert integrity.check_attestation(db, att(manifest_version=2)) == integrity.IntegrityResult.FAIL
    db.close()


def test_publish_requires_valid_release_signature(client):
    db = SessionLocal()
    m2 = mf.Manifest(version=2, exe="Structura.exe", files=dict(helpers.M.files), dirs=dict(helpers.M.dirs))
    t2 = mf.render(m2)
    with pytest.raises(mf.ManifestError):
        integrity.publish(db, t2, mf.sign(t2, mf._b64e(SigningKey.generate().encode())))
    with pytest.raises(mf.ManifestError):
        integrity.publish(db, t2, crypto.sign_blob(t2.encode(), crypto.DOM_MANIFEST))  # chave do servidor
    integrity.publish(db, t2, mf.sign(t2, SEED()), "allowed")
    db.commit()
    with pytest.raises(mf.ManifestError):   # versao imutavel
        integrity.publish(db, t2, mf.sign(t2, SEED()))
    with pytest.raises(mf.ManifestError):   # status invalido
        integrity.publish(db, mf.render(mf.Manifest(version=3, exe="Structura.exe", files=dict(helpers.M.files),
                                                    dirs=dict(helpers.M.dirs))), "x", "revoked")
    db.close()


# ------------------------------------------------------------------ endpoints
def test_login_integrity_fail_blocks_and_is_audited(client, acct):
    bad = {**INTEG, "files_root": "0" * 64}
    r = client.post("/auth/login", json=login_body(acct, bad))
    assert r.status_code == 409 and r.json()["detail"] == "INTEGRITY_FAIL"
    db = SessionLocal()
    assert db.scalar(select(AuditLog).where(AuditLog.action == "login_integrity_fail")) is not None
    db.close()


def test_login_revoked_manifest_is_426(client, acct):
    db = SessionLocal(); integrity.set_status(db, 1, "revoked"); db.commit(); db.close()
    r = client.post("/auth/login", json=login_body(acct))
    assert r.status_code == 426 and r.json()["detail"] == "CLIENT_OUTDATED"


def test_heartbeat_integrity_checked(client, acct):
    t = token(client, acct)
    hb = lambda i: client.post("/license/heartbeat", json={"license_token": t, "fingerprint": FP1, "integrity": i})  # noqa: E731
    assert hb(dict(INTEG)).status_code == 200
    assert hb({**INTEG, "file_count": 99}).status_code == 409
    db = SessionLocal(); integrity.set_status(db, 1, "revoked"); db.commit(); db.close()
    assert hb(dict(INTEG)).status_code == 426


def test_compute_integrity_fail_gives_no_K(client, acct):
    t = token(client, acct)
    req = {"license_token": t, "nonce": "ab" * 8, "fingerprint": FP1, "integrity": {**INTEG, "exe_hash": "0" * 64},
           "model_hash": "e" * 64, "reaction_totals": {"F": [0, 0, 0], "M_origin": [0, 0, 0]},
           "result_digest": "f" * 64}
    r = client.post("/compute/validate", json=req).json()
    assert r["ok"] is False and r["reason"] == "INTEGRITY_FAIL" and r.get("K") is None


# ---------------- Nivel B: /license/check
@pytest.mark.parametrize("feature", ["detalhamento", "memorial"])
def test_level_b_check_ok_returns_signed_pass(client, acct, feature):
    t = token(client, acct)
    nonce = "cd" * 16
    r = check(client, t, feature, nonce=nonce)
    assert r.status_code == 200 and r.json()["ok"] is True
    p = crypto.verify_token(t)
    exp = int(time.time()) + 60
    sig = r.json()["pass_sig"]
    ok = False
    for e in range(exp - 3, exp + 3):   # exp = agora+TTL (tolerancia de relogio do teste)
        if crypto.verify_blob(crypto.gate_pass_digest(feature, p["license_id"], nonce, e), sig, crypto.DOM_GATE):
            ok = True
    assert ok
    # o passe nao vale para outra feature, outro nonce, nem em outro dominio
    assert not any(crypto.verify_blob(crypto.gate_pass_digest("memorial" if feature == "detalhamento" else "detalhamento",
                                                              p["license_id"], nonce, e), sig, crypto.DOM_GATE)
                   for e in range(exp - 3, exp + 3))
    assert not any(crypto.verify_blob(crypto.gate_pass_digest(feature, p["license_id"], "00" * 8, e), sig, crypto.DOM_GATE)
                   for e in range(exp - 3, exp + 3))
    assert not crypto.verify_blob(crypto.gate_pass_digest(feature, p["license_id"], nonce, exp), sig, crypto.DOM_KEYSTONE)


@pytest.mark.parametrize("feature", ["calculo", "desenhos", "export_dxf_pdf", "dimensionamento",
                                     "quantitativos", "relatorios", "modelagem", "xyz", ""])
def test_level_b_endpoint_rejects_non_b_features(client, acct, feature):
    t = token(client, acct)
    r = check(client, t, feature)
    assert r.status_code == 422 and "pass_sig" not in r.text   # Nivel A NUNCA ganha passe B


def test_level_b_denied_without_valid_token(client, acct):
    r = check(client, "x" * 40)
    assert r.status_code == 403 and r.json() == {"ok": False, "reason": "LICENSE_INVALID", "pass_sig": None, "exp": None} \
        or r.json()["reason"] == "LICENSE_INVALID"
    assert "pass_sig" not in r.text or r.json().get("pass_sig") is None


def test_level_b_denied_on_integrity_and_revocation(client, acct):
    t = token(client, acct)
    r = check(client, t, integ={**INTEG, "files_root": "0" * 64})
    assert r.status_code == 403 and r.json()["reason"] == "INTEGRITY_FAIL"
    db = SessionLocal(); integrity.set_status(db, 1, "revoked"); db.commit(); db.close()
    r = check(client, t)
    assert r.status_code == 403 and r.json()["reason"] == "CLIENT_OUTDATED"


def test_level_b_denied_after_logout_suspension_wrong_fp(client, acct):
    t = token(client, acct)
    assert check(client, t, fp="d" * 64).status_code == 403
    db = SessionLocal(); db.get(License, acct["license_id"]).status = "suspensa"; db.commit(); db.close()
    assert check(client, t).status_code == 403
    db = SessionLocal(); db.get(License, acct["license_id"]).status = "ativa"; db.commit(); db.close()
    assert check(client, t).status_code == 200
    client.post("/auth/logout", json={"license_token": t})
    assert check(client, t).status_code == 403


def test_level_b_bad_nonce_rejected(client, acct):
    t = token(client, acct)
    assert check(client, t, nonce="zz").status_code == 422
    assert check(client, t, nonce="A" * 16).status_code == 422


def test_level_b_audited_without_pii(client, acct):
    t = token(client, acct)
    check(client, t, "memorial")
    db = SessionLocal()
    row = db.scalar(select(AuditLog).where(AuditLog.action == "license_check"))
    assert row and row.metadata_masked == "memorial" and EMAIL not in row.target
    db.close()


# ------------------------------------------------------------------ registro de modulos
def test_module_matrix_matches_spec():
    A = {"calculo", "desenhos", "export_dxf_pdf", "dimensionamento", "quantitativos", "relatorios"}
    B = {"detalhamento", "memorial"}
    assert {m for m, l in modules.MODULES.items() if l == "A"} == A
    assert {m for m, l in modules.MODULES.items() if l == "B"} == B
    assert modules.level_of("modelagem") == "EXE" and modules.level_of("nao-existe") is None
    from typing import get_args
    assert set(get_args(modules.LevelBFeature)) == B and set(get_args(modules.LevelAModule)) == A


# ------------------------------------------------------------------ script de manifesto + disco
def test_make_manifest_tool_and_server_roundtrip(client, tmp_path, monkeypatch):
    d = make_release(tmp_path)
    text, sig = run_tool(d, 5, monkeypatch)
    assert mf.verify(text, sig, PUB())
    m = mf.parse(text)
    assert m.exclude == ["user_data"] and "user_data/prefs.ini" not in m.files and "ção.dll" in m.files
    assert "manifest.txt" not in m.files
    assert m.dirs["lib/sub"] == 1 and m.dirs["user_data".replace("user_data", ".")] == 3
    db = SessionLocal()
    integrity.publish(db, text, sig)
    db.commit()
    a = mf.attestation_from_dir(str(d), m)
    assert integrity.check_attestation(db, Integrity(**a)) == integrity.IntegrityResult.OK
    # --- adulteracoes (CA do M2): cada uma precisa quebrar a atestacao
    def bad(mutator):
        mutator()
        a2 = mf.attestation_from_dir(str(d), m)
        return integrity.check_attestation(db, Integrity(**a2))
    F = integrity.IntegrityResult.FAIL
    assert bad(lambda: (d / "rds.dll").write_bytes(b"patched")) == F
    (d / "rds.dll").write_bytes(b"dll rds")
    assert integrity.check_attestation(db, Integrity(**mf.attestation_from_dir(str(d), m))) == integrity.IntegrityResult.OK
    assert bad(lambda: (d / "evil.dll").write_bytes(b"x")) == F
    (d / "evil.dll").unlink()
    assert bad(lambda: (d / "lib" / "sub" / "evil.dll").write_bytes(b"x")) == F
    (d / "lib" / "sub" / "evil.dll").unlink()
    assert bad(lambda: (d / "lib" / "a.dll").unlink()) == F
    (d / "lib" / "a.dll").write_bytes(b"dll a")
    assert bad(lambda: (d / "pasta_extra").mkdir()) == F
    (d / "pasta_extra").rmdir()
    # dados do usuario na pasta excluida NAO quebram
    (d / "user_data" / "novo.txt").write_bytes(b"ok")
    a3 = mf.attestation_from_dir(str(d), m)
    assert integrity.check_attestation(db, Integrity(**a3)) == integrity.IntegrityResult.OK
    # trocar arquivo por outro de mesmo tamanho (mesma contagem) tambem quebra
    (d / "rds.dll").write_bytes(b"dll rdz")
    assert integrity.check_attestation(db, Integrity(**mf.attestation_from_dir(str(d), m))) == F
    db.close()


def test_tool_refuses_key_inside_release_dir(tmp_path, monkeypatch):
    d = make_release(tmp_path)
    (d / "release.key").write_text(SEED())
    sys.path.insert(0, str(SERVER / "tools"))
    import make_manifest
    monkeypatch.delenv("RELEASE_SIGNING_SEED_B64", raising=False)
    assert make_manifest.main(["--dir", str(d), "--version", "1", "--exe", "Structura.exe",
                               "--key-file", str(d / "release.key")]) == 2
    assert not (d / "manifest.txt").exists()


def test_tool_requires_key(tmp_path, monkeypatch):
    d = make_release(tmp_path)
    sys.path.insert(0, str(SERVER / "tools"))
    import make_manifest
    monkeypatch.delenv("RELEASE_SIGNING_SEED_B64", raising=False)
    assert make_manifest.main(["--dir", str(d), "--version", "1", "--exe", "Structura.exe"]) == 2


def test_gen_keys_release_private_key_never_in_env(tmp_path):
    import gen_keys
    (tmp_path / ".env.example").write_text("SERVER_SIGNING_SEED_B64=\nRELEASE_PUBKEY_B64=\n")
    assert gen_keys.main(["--write", "--dir", str(tmp_path)]) == 0
    env = (tmp_path / ".env").read_text()
    k = gen_keys.generate()
    assert "RELEASE_SIGNING_SEED" not in env
    pub = [l for l in env.splitlines() if l.startswith("RELEASE_PUBKEY_B64=")][0].split("=", 1)[1]
    assert len(pub) == 43
    assert k["_RELEASE_SEED"] != k["SERVER_SIGNING_SEED_B64"]


def test_manifest_cli_publish_and_revoke(client, tmp_path, monkeypatch, capsys):
    from app import manifest_cli
    d = make_release(tmp_path)
    text, sig = run_tool(d, 7, monkeypatch)
    assert manifest_cli.main(["publish", "--text", str(d / "manifest.txt"), "--sig", str(d / "manifest.sig")]) == 0
    assert manifest_cli.main(["publish", "--text", str(d / "manifest.txt"), "--sig", str(d / "manifest.sig")]) == 1
    assert manifest_cli.main(["set-status", "--version", "7", "--status", "revoked"]) == 0
    assert manifest_cli.main(["set-status", "--version", "99", "--status", "revoked"]) == 1
    manifest_cli.main(["list"])
    assert "7 revoked" in capsys.readouterr().out
    (d / "manifest.sig").write_text(sig[:-2] + "AA")
    assert manifest_cli.main(["publish", "--text", str(d / "manifest.txt"), "--sig", str(d / "manifest.sig")]) == 1


# ------------------------------------------------------------------ conformidade cruzada C++ <-> Python
@pytest.fixture(scope="module")
def cpp_exe(tmp_path_factory):
    gxx = shutil.which("g++")
    if not gxx:
        pytest.skip("g++ indisponivel")
    out = tmp_path_factory.mktemp("cpp") / ("integrity_test.exe" if os.name == "nt" else "integrity_test")
    r = subprocess.run([gxx, "-std=c++17", "-O1", "-Wall", "-Wextra", "-o", str(out),
                        str(ROOT / "client" / "tests" / "integrity_test.cpp"),
                        str(ROOT / "client" / "integrity_client.cpp")], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    return str(out)


def cpp(exe, mode, arg):
    r = subprocess.run([exe, mode, str(arg)], capture_output=True, text=True, encoding="utf-8")
    assert r.returncode == 0, r.stdout + r.stderr
    return dict(l.split("=", 1) for l in r.stdout.splitlines() if "=" in l)


def test_cpp_sha256_vectors(cpp_exe):
    r = subprocess.run([cpp_exe, "selftest"], capture_output=True, text=True)
    assert r.stdout.strip() == "OK"


def test_cpp_matches_python_root_and_detects_tamper(cpp_exe, tmp_path, monkeypatch):
    d = make_release(tmp_path)
    text, _ = run_tool(d, 3, monkeypatch)
    m = mf.parse(text)

    def both(expect_status):
        c = cpp(cpp_exe, "verify", d)
        p = mf.attestation_from_dir(str(d), m)
        assert c["status"] == expect_status, c
        assert c["root"] == p["files_root"] and int(c["count"]) == p["file_count"] and c["exe"] == p["exe_hash"]
        assert int(c["version"]) == 3

    both("Ok")                                       # inclui nome de arquivo UTF-8 (ção.dll)
    (d / "user_data" / "x.txt").write_bytes(b"1"); both("Ok"); (d / "user_data" / "x.txt").unlink()
    (d / "rds.dll").write_bytes(b"patched"); both("FileModified"); (d / "rds.dll").write_bytes(b"dll rds")
    (d / "evil.dll").write_bytes(b"x"); both("FileExtra"); (d / "evil.dll").unlink()
    (d / "lib" / "sub" / "evil.dll").write_bytes(b"x"); both("FileExtra"); (d / "lib" / "sub" / "evil.dll").unlink()
    (d / "pasta_extra").mkdir(); both("FileExtra"); (d / "pasta_extra").rmdir()
    (d / "lib" / "a.dll").unlink(); both("FileMissing"); (d / "lib" / "a.dll").write_bytes(b"dll a")
    (d / "ção.dll").unlink(); both("FileMissing"); (d / "ção.dll").write_bytes(b"unicode name")
    both("Ok")


def test_cpp_fail_closed_modes(cpp_exe, tmp_path, monkeypatch):
    d = make_release(tmp_path)
    run_tool(d, 1, monkeypatch)
    assert cpp(cpp_exe, "failclosed", d)["status"] == "ManifestBadSig"    # assinatura recusada
    assert cpp(cpp_exe, "nullverify", d)["status"] == "ManifestBadSig"    # sem verificador => bloqueia
    (d / "manifest.sig").unlink()
    assert cpp(cpp_exe, "verify", d)["status"] == "ManifestMissing"
    (d / "manifest.sig").write_text("nao-e-uma-assinatura")
    assert cpp(cpp_exe, "verify", d)["status"] == "ManifestBadSig"
    assert cpp(cpp_exe, "verify", tmp_path / "nao-existe")["status"] == "ManifestMissing"


def test_cpp_parser_agrees_with_python_on_malformed(cpp_exe, tmp_path):
    cases = [helpers.TEXT, helpers.TEXT.replace("\n", "\r\n"), helpers.TEXT.rstrip("\n"),
             helpers.TEXT.replace("version 1", "version 01"),
             helpers.TEXT.replace("F lib/helper.dll", "F ../lib/helper.dll"),
             helpers.TEXT.replace("D lib\t1", "D lib\t2"),
             helpers.TEXT.replace("exe Structura.exe", "exe nada.exe"),
             helpers.TEXT.replace(helpers.M.files["rds.dll"], helpers.M.files["rds.dll"].upper()),
             helpers.TEXT.replace("exclude user_data\n", "exclude user_data\nexclude user_data\n"),
             helpers.TEXT.replace("\n", "\n\n", 1),
             helpers.TEXT + "F zzz\t" + "a" * 64 + "\n"]
    for i, t in enumerate(cases):
        f = tmp_path / f"m{i}.txt"
        f.write_bytes(t.encode())
        try:
            mf.parse(t); py = "OK"
        except mf.ManifestError:
            py = "BAD"
        r = subprocess.run([cpp_exe, "parse", str(f)], capture_output=True, text=True)
        assert r.stdout.strip() == py, (i, py, r.stdout)
    assert subprocess.run([cpp_exe, "parse", str(tmp_path / "m0.txt")], capture_output=True, text=True).stdout.strip() == "OK"


def test_cpp_rejects_symlink(cpp_exe, tmp_path, monkeypatch):
    d = make_release(tmp_path)
    run_tool(d, 1, monkeypatch)
    try:
        os.symlink(d / "rds.dll", d / "link.dll")
    except (OSError, NotImplementedError):
        pytest.skip("sem privilegio para symlink")
    assert cpp(cpp_exe, "verify", d)["status"] == "ScanError"
