"""M0 — setup/infra: /pubkey, gen_keys, config fail-closed, segredos fora do git."""
import base64
import subprocess
from pathlib import Path

import pytest
from nacl.signing import VerifyKey

import gen_keys

ROOT = Path(__file__).resolve().parents[2]
SERVER = ROOT / "server"


def _d(s):
    return base64.urlsafe_b64decode(s + "=" * (-len(s) % 4))


# ---------- CA: GET /pubkey ----------
def test_pubkey_ok_and_matches_signing(client):
    r = client.get("/pubkey")
    assert r.status_code == 200
    body = r.json()
    pub = _d(body["server_pubkey_b64"])
    assert len(pub) == 32 and body["id"] == "k1"
    from app import crypto
    sig = crypto.sign_blob(b"x", crypto.DOM_TOKEN)
    VerifyKey(pub).verify(crypto.DOM_TOKEN + b"x", _d(sig))  # lanca se invalido


def test_pubkey_never_exposes_private_material(client):
    from app.config import settings
    txt = client.get("/pubkey").text
    for secret in (settings.SERVER_SIGNING_SEED_B64, settings.CPF_ENC_KEY_B64, settings.CPF_HMAC_KEY_B64):
        assert secret not in txt


# ---------- config fail-closed ----------
@pytest.mark.parametrize("bad", ["abc", "!!!notb64!!!", gen_keys.b64(b"x" * 16), gen_keys.b64(b"x" * 33)])
def test_config_rejects_malformed_keys(bad):
    from app.config import Settings, SecretConfigError
    good = gen_keys.b64(b"k" * 32)
    s = Settings(_env_file=None, SERVER_SIGNING_SEED_B64=bad, CPF_ENC_KEY_B64=good, CPF_HMAC_KEY_B64=good)
    with pytest.raises(SecretConfigError) as e:
        s.validate_secrets()
    assert bad not in str(e.value)  # nunca ecoa o valor do segredo
    assert "SERVER_SIGNING_SEED_B64" in str(e.value)


def test_config_rejects_missing_keys():
    from app.config import Settings, SecretConfigError
    with pytest.raises(SecretConfigError):
        Settings(_env_file=None, SERVER_SIGNING_SEED_B64="", CPF_ENC_KEY_B64="", CPF_HMAC_KEY_B64="").validate_secrets()


def test_config_accepts_valid_keys():
    from app.config import Settings
    g = gen_keys.generate()
    Settings(_env_file=None, SERVER_SIGNING_SEED_B64=g["SERVER_SIGNING_SEED_B64"],
             CPF_ENC_KEY_B64=g["CPF_ENC_KEY_B64"], CPF_HMAC_KEY_B64=g["CPF_HMAC_KEY_B64"],
             RELEASE_PUBKEY_B64=g["RELEASE_PUBKEY_B64"]).validate_secrets()


def test_missing_secret_fails_closed_on_use(monkeypatch):
    from app import crypto
    monkeypatch.setattr(crypto.settings, "SERVER_SIGNING_SEED_B64", "")
    with pytest.raises(RuntimeError):
        crypto.sign_blob(b"x", crypto.DOM_TOKEN)
    monkeypatch.setattr(crypto.settings, "CPF_ENC_KEY_B64", "")
    with pytest.raises(RuntimeError):
        crypto.cpf_encrypt("12345678909")
    monkeypatch.setattr(crypto.settings, "CPF_HMAC_KEY_B64", "")
    with pytest.raises(RuntimeError):
        crypto.cpf_hmac("12345678909")


def test_app_refuses_to_start_without_secrets():
    import os, sys
    env = {k: v for k, v in os.environ.items() if not k.endswith(("_B64",))}
    env["DATABASE_URL"] = "sqlite://"
    r = subprocess.run([sys.executable, "-c", "import app.main"], cwd=SERVER, env=env,
                       capture_output=True, text=True)
    assert r.returncode != 0 and "SecretConfigError" in r.stderr


def test_missing_secrets_report():
    from app.config import Settings
    s = Settings(_env_file=None, SERVER_SIGNING_SEED_B64="", CPF_ENC_KEY_B64="", CPF_HMAC_KEY_B64="")
    assert set(s.missing_secrets()) == {"SERVER_SIGNING_SEED_B64", "CPF_ENC_KEY_B64", "CPF_HMAC_KEY_B64"}


# ---------- gen_keys ----------
def test_gen_keys_unique_and_32_bytes():
    a, b = gen_keys.generate(), gen_keys.generate()
    for k in ("SERVER_SIGNING_SEED_B64", "CPF_ENC_KEY_B64", "CPF_HMAC_KEY_B64", "_PUBKEY"):
        assert len(_d(a[k])) == 32
        assert a[k] != b[k]


def test_gen_keys_pubkey_matches_seed():
    from nacl.signing import SigningKey
    k = gen_keys.generate()
    assert bytes(SigningKey(_d(k["SERVER_SIGNING_SEED_B64"])).verify_key) == _d(k["_PUBKEY"])


def test_gen_keys_write_refuses_overwrite(tmp_path, capsys):
    (tmp_path / ".env.example").write_text("SERVER_SIGNING_SEED_B64=\nCPF_ENC_KEY_B64=\nCPF_HMAC_KEY_B64=\nTOKEN_TTL_SECONDS=1\n")
    assert gen_keys.main(["--write", "--dir", str(tmp_path)]) == 0
    env = (tmp_path / ".env").read_text()
    assert "TOKEN_TTL_SECONDS=1" in env and "SERVER_SIGNING_SEED_B64=" in env
    first = env
    assert gen_keys.main(["--write", "--dir", str(tmp_path)]) == 1  # nao sobrescreve
    assert (tmp_path / ".env").read_text() == first


# ---------- segredos fora do git ----------
def test_env_example_has_no_secret_values():
    for line in (SERVER / ".env.example").read_text().splitlines():
        if line.startswith(("SERVER_SIGNING_SEED_B64", "CPF_ENC_KEY_B64", "CPF_HMAC_KEY_B64")):
            assert line.split("=", 1)[1].strip() == ""


def test_gitignore_covers_secrets():
    gi = (ROOT / ".gitignore").read_text().splitlines()
    assert ".env" in gi and "*.db" in gi and "*.pem" in gi


def test_no_env_file_tracked_by_git():
    out = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True, text=True)
    if out.returncode != 0:
        pytest.skip("git indisponivel")
    tracked = [f for f in out.stdout.splitlines() if Path(f).name == ".env" or f.endswith((".pem", ".key", ".db"))]
    assert tracked == []


def test_no_private_key_literal_in_source():
    """Heuristica: nenhuma seed/chave base64url de 43 chars hardcoded no codigo do servidor/cliente."""
    import re
    pat = re.compile(r"(SEED|KEY|SECRET)[A-Z_0-9]*\s*=\s*[\"'][A-Za-z0-9_-]{43}[\"']")
    for base in (SERVER / "app", ROOT / "client"):
        for p in base.rglob("*"):
            if p.is_file() and p.suffix in {".py", ".h", ".cpp", ".hpp", ".md"}:
                assert not pat.search(p.read_text(encoding="utf-8", errors="ignore")), p
