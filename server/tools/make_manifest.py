"""Gera e ASSINA o manifesto de integridade de uma pasta de release (roda no CI/maquina de build).

  set RELEASE_SIGNING_SEED_B64=<seed da chave de release>      # nunca no repo / nunca no servidor
  python tools/make_manifest.py --dir dist/Structura --version 7 --exe Structura.exe ^
         --exclude user_data

Escreve manifest.txt e manifest.sig NA pasta (o cliente os le de la). Alternativamente
--key-file (recusado se o arquivo estiver dentro da pasta de release).
Depois: publique no servidor com `python -m app.manifest_cli publish ...`.
"""
from __future__ import annotations
import argparse
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nacl.signing import SigningKey  # noqa: E402
from app import manifest as mf  # noqa: E402


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", required=True)
    ap.add_argument("--version", type=int, required=True)
    ap.add_argument("--exe", required=True)
    ap.add_argument("--exclude", action="append", default=[])
    ap.add_argument("--key-file")
    args = ap.parse_args(argv)

    root = Path(args.dir).resolve()
    if args.key_file:
        kf = Path(args.key_file).resolve()
        if root in kf.parents:
            print("ERRO: key-file dentro da pasta de release (vazaria no pacote).", file=sys.stderr)
            return 2
        seed = kf.read_text(encoding="ascii").strip()
    else:
        seed = os.environ.get("RELEASE_SIGNING_SEED_B64", "")
    if not seed:
        print("ERRO: informe RELEASE_SIGNING_SEED_B64 (env) ou --key-file.", file=sys.stderr)
        return 2
    try:
        m = mf.build_from_dir(str(root), args.version, args.exe, args.exclude)
    except mf.ManifestError as e:
        print(f"ERRO: {e}", file=sys.stderr)
        return 1
    text = mf.render(m)
    sig = mf.sign(text, seed)
    pub = mf._b64e(bytes(SigningKey(mf._b64d(seed)).verify_key))
    assert mf.verify(text, sig, pub)
    (root / "manifest.txt").write_text(text, encoding="utf-8", newline="\n")
    (root / "manifest.sig").write_text(sig, encoding="ascii")
    print(f"manifesto v{m.version}: {m.file_count} arquivos, {len(m.dirs)} pastas, root={m.root}")
    print(f"RELEASE_PUBKEY_B64={pub}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
