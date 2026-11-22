"""Converte uma chave PUBLICA Ed25519 (base64url ou hex, 32 bytes) no array C++ usado em client/license_config.cpp.

  python server/deploy/pubkey_to_cpp.py jLiZDIlCKNI6uGt-eqgbqre37huCF0bQh3pFiHsLKpM
  python server/deploy/pubkey_to_cpp.py abb8ed41cdbbacb74f39a8ebfd006d3f993bff1dd34111e11e760287efb0a5a5
  python server/deploy/pubkey_to_cpp.py --file server/deploy/secrets/server_pubkey.hex

Chaves PUBLICAS nao sao segredo (ficam dentro do app de qualquer jeito). NUNCA use isto com chaves privadas/seeds.
"""
from __future__ import annotations
import argparse
import base64
import re
import sys
from pathlib import Path


def parse(value: str) -> bytes:
    v = value.strip()
    if re.fullmatch(r"[0-9a-fA-F]{64}", v):
        return bytes.fromhex(v)
    try:
        raw = base64.urlsafe_b64decode(v + "=" * (-len(v) % 4))
    except Exception as e:  # noqa: BLE001
        raise SystemExit(f"valor invalido (nem hex nem base64url): {e}")
    if len(raw) != 32:
        raise SystemExit(f"chave deve ter 32 bytes; recebi {len(raw)}. Isto e uma chave PUBLICA Ed25519?")
    return raw


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("value", nargs="?")
    ap.add_argument("--file")
    ap.add_argument("--name", default="kServerPubKey")
    a = ap.parse_args()
    text = Path(a.file).read_text(encoding="ascii") if a.file else a.value
    if not text:
        ap.error("informe a chave ou --file")
    b = parse(text)
    rows = [", ".join(f"0x{x:02x}" for x in b[i:i + 8]) for i in range(0, 32, 8)]
    print(f"const uint8_t {a.name}[32] = {{")
    print(",\n".join("    " + r for r in rows))
    print("};")
    print(f"// hex: {b.hex()}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
