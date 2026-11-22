"""Dados de teste compartilhados: manifesto de fixture assinado com chave de release EFEMERA."""
import hashlib
import os

from app import manifest as mf

FILES = {"Structura.exe": b"MZ-fake-exe", "rds.dll": b"fake-dll-1", "lib/helper.dll": b"fake-dll-2"}
DIRS = {".": 2, "lib": 1}
M = mf.Manifest(
    version=1, exe="Structura.exe", exclude=["user_data"],
    files={p: hashlib.sha256(b).hexdigest() for p, b in FILES.items()}, dirs=dict(DIRS),
)
TEXT = mf.render(M)
SIG = mf.sign(TEXT, os.environ["TEST_RELEASE_SEED_B64"])
INTEG = {"manifest_version": M.version, "files_root": M.root,
         "file_count": M.file_count, "exe_hash": M.exe_hash}
