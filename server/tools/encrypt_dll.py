"""Cifra as DLLs do solver (rds.dll e mefs.dll) para distribuicao segura em repouso (Marco M5).

Cada versao de manifesto tem sua propria DLL_RELEASE_KEY de 256 bits, garantindo que
a rotatividade de versoes invalide os binarios de versoes anteriores.

Uso:
  python server/tools/encrypt_dll.py --version 1 --in-rds rds.dll --in-mefs mefs.dll --out-dir .
  python server/tools/encrypt_dll.py --version 1 --key <64_hex_chars>
  python server/tools/encrypt_dll.py --version 1 --key-file secret.key
"""
from __future__ import annotations
import argparse
import hashlib
import hmac
import os
import sys
from pathlib import Path

# Adiciona o diretorio do servidor ao sys.path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

try:
    import nacl.secret
    import nacl.utils
except ImportError:
    print("ERRO: PyNaCl nao instalado. Execute no venv do servidor.", file=sys.stderr)
    sys.exit(1)

from app import integrity, manifest as mf  # noqa: E402
from app.config import settings  # noqa: E402
from app.db import SessionLocal  # noqa: E402
from app.models import IntegrityManifest  # noqa: E402


def encrypt_file(in_path: Path, out_path: Path, key_bytes: bytes) -> tuple[int, int]:
    """Cifra um arquivo usando XSalsa20-Poly1305 (TweetNaCl secretbox).
    Formato do envelope: [0..23]: Nonce (24 bytes) || [24..]: Ciphertext autenticado com tag Poly1305 (16 bytes)."""
    if not in_path.is_file():
        raise FileNotFoundError(f"Arquivo de entrada nao encontrado: {in_path}")

    raw_data = in_path.read_bytes()
    box = nacl.secret.SecretBox(key_bytes)
    # PyNaCl gera nonce de 24 bytes internamente e coloca no inicio do payload
    encrypted = box.encrypt(raw_data)

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(encrypted)
    return len(raw_data), len(encrypted)


def resolve_key(version: int, explicit_key: str | None, key_file: str | None) -> bytes:
    """Obtem a DLL_RELEASE_KEY de 32 bytes para a versao solicitada."""
    if explicit_key:
        k_hex = explicit_key.strip()
    elif key_file:
        kf = Path(key_file).resolve()
        if not kf.is_file():
            raise FileNotFoundError(f"Arquivo de chave nao encontrado: {kf}")
        k_hex = kf.read_text(encoding="ascii").strip()
    else:
        # Consulta DB do servidor se disponivel; caso contrario, deriva deterministicamente por versao
        k_hex = None
        try:
            db = SessionLocal()
            row = db.get(IntegrityManifest, version)
            if row and row.dll_release_key:
                k_hex = row.dll_release_key
            db.close()
        except Exception:
            pass

        if not k_hex:
            k_hex = integrity.derive_dll_key_for_version(version)

    if len(k_hex) != 64:
        raise ValueError(f"Chave invalida: esperado 64 caracteres hexadecimais (32 bytes), recebido {len(k_hex)}")

    try:
        return bytes.fromhex(k_hex)
    except ValueError as e:
        raise ValueError(f"Chave contem caracteres nao hexadecimais: {e}") from None


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Cifra DLLs do Structura com chave vinculada a versao do manifesto (M5).")
    ap.add_argument("--version", type=int, default=None, help="Versao do manifesto de integridade (padrao: versao 'current' do DB)")
    ap.add_argument("--in-rds", default="rds.dll", help="Caminho do rds.dll original (padrao: rds.dll)")
    ap.add_argument("--in-mefs", default="mefs.dll", help="Caminho do mefs.dll original (padrao: mefs.dll)")
    ap.add_argument("--out-dir", default=".", help="Diretorio de gravacao de rds.enc e mefs.enc (padrao: .)")
    ap.add_argument("--key", help="Chave hex de 64 caracteres (opcional)")
    ap.add_argument("--key-file", help="Arquivo contendo a chave hex (opcional)")

    args = ap.parse_args(argv)

    target_version = args.version
    if target_version is None:
        try:
            db = SessionLocal()
            from sqlalchemy import select
            row = db.execute(select(IntegrityManifest.version).where(IntegrityManifest.status == "current").order_by(IntegrityManifest.version.desc())).first()
            if row:
                target_version = row[0]
            db.close()
        except Exception:
            pass
        if target_version is None:
            target_version = 2

    try:
        key_bytes = resolve_key(target_version, args.key, args.key_file)
    except Exception as e:
        print(f"ERRO ao resolver chave para versao {target_version}: {e}", file=sys.stderr)
        return 1

    # NUNCA exibe a chave em claro nos logs. Exibe apenas um fingerprint SHA-256 curto para verificacao.
    key_fingerprint = hashlib.sha256(key_bytes).hexdigest()[:16]
    print(f"[*] Cifrando solvers para versao de manifesto v{target_version} (key_id: {key_fingerprint}...)")

    out_dir = Path(args.out_dir).resolve()
    rds_in = Path(args.in_rds).resolve()
    mefs_in = Path(args.in_mefs).resolve()

    rds_out = out_dir / "rds.enc"
    mefs_out = out_dir / "mefs.enc"

    try:
        raw_rds_len, enc_rds_len = encrypt_file(rds_in, rds_out, key_bytes)
        print(f"[+] rds.dll ({raw_rds_len} bytes) -> {rds_out.name} ({enc_rds_len} bytes)")
    except Exception as e:
        print(f"ERRO ao cifrar rds.dll: {e}", file=sys.stderr)
        return 2

    try:
        raw_mefs_len, enc_mefs_len = encrypt_file(mefs_in, mefs_out, key_bytes)
        print(f"[+] mefs.dll ({raw_mefs_len} bytes) -> {mefs_out.name} ({enc_mefs_len} bytes)")
    except Exception as e:
        print(f"ERRO ao cifrar mefs.dll: {e}", file=sys.stderr)
        return 3

    print(f"[OK] Cifra concluida com sucesso para versao {args.version}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
