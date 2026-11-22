"""Manifesto de integridade assinado (SPEC §7/§9; PLAN M2).

Formato CANONICO em TEXTO (o cliente C++ parseia linhas; sem JSON):

    STRUCTURA-MANIFEST 1
    version <N>
    exe <relpath>
    exclude <dir>              (0..n, ordenado)  diretorios fora do escopo (ex.: dados do usuario)
    F <relpath>\\t<sha256hex>   (todos os arquivos, ordenados por bytes UTF-8 do relpath)
    D <relpath>\\t<contagem>    (todos os diretorios incl. vazios e "."; contagem = arquivos DIRETOS)

`files_root` = SHA-256 de (todas as linhas F, depois todas as D, cada uma terminada em "\\n").
Qualquer arquivo a mais/menos/alterado, ou diretorio a mais, muda a raiz. A contagem por pasta
esta na raiz via linhas D.  `manifest.txt` e `manifest.sig` ficam fora das listas.

Assinatura: Ed25519(DOM_MANIFEST || bytes_do_texto) com a chave de RELEASE (privada so no
CI/maquina de build; o servidor so conhece a PUBLICA — comprometer o servidor nao permite
forjar manifestos).
"""
from __future__ import annotations
import base64
import hashlib
import os
import re
from dataclasses import dataclass, field

from nacl.signing import SigningKey, VerifyKey

DOM_MANIFEST = b"structura-manifest-v1\x00"   # igual a crypto.DOM_MANIFEST (sem importar settings)
MANIFEST_FILES = ("manifest.txt", "manifest.sig", "Uninstall.exe", "uninstall.exe")
HEADER = "STRUCTURA-MANIFEST 1"
_HEX64 = re.compile(r"^[0-9a-f]{64}$")


class ManifestError(ValueError):
    pass


@dataclass
class Manifest:
    version: int
    exe: str
    exclude: list[str] = field(default_factory=list)
    files: dict[str, str] = field(default_factory=dict)   # relpath -> sha256 hex
    dirs: dict[str, int] = field(default_factory=dict)    # relpath -> n arquivos diretos

    @property
    def root(self) -> str:
        return compute_root(self.files, self.dirs)

    @property
    def file_count(self) -> int:
        return len(self.files)

    @property
    def exe_hash(self) -> str:
        return self.files[self.exe]


def _b64e(b: bytes) -> str:
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode()


def _b64d(s: str) -> bytes:
    return base64.urlsafe_b64decode(s + "=" * (-len(s) % 4))


def _key(s: str) -> bytes:
    return s.encode("utf-8")


def _check_path(p: str, allow_dot: bool = False) -> None:
    if allow_dot and p == ".":
        return
    if (not p or p.startswith("/") or "\\" in p or any(ord(c) < 32 for c in p)
            or any(seg in ("", ".", "..") for seg in p.split("/"))):
        raise ManifestError("caminho invalido")


def compute_root(files: dict[str, str], dirs: dict[str, int]) -> str:
    h = hashlib.sha256()
    for p in sorted(files, key=_key):
        h.update(b"F " + _key(p) + b"\t" + files[p].encode() + b"\n")
    for p in sorted(dirs, key=_key):
        h.update(b"D " + _key(p) + b"\t" + str(dirs[p]).encode() + b"\n")
    return h.hexdigest()


def render(m: Manifest) -> str:
    lines = [HEADER, f"version {m.version}", f"exe {m.exe}"]
    lines += [f"exclude {d}" for d in sorted(m.exclude, key=_key)]
    lines += [f"F {p}\t{m.files[p]}" for p in sorted(m.files, key=_key)]
    lines += [f"D {p}\t{m.dirs[p]}" for p in sorted(m.dirs, key=_key)]
    return "\n".join(lines) + "\n"


def parse(text: str) -> Manifest:
    """Parser ESTRITO: rejeita duplicatas, hashes mal formados, caminhos perigosos, desordem."""
    try:
        if "\r" in text or not text.endswith("\n"):
            raise ManifestError("formato")
        lines = text[:-1].split("\n")
        if len(lines) < 4 or lines[0] != HEADER:
            raise ManifestError("cabecalho")
        if not lines[1].startswith("version ") or not lines[2].startswith("exe "):
            raise ManifestError("cabecalho")
        version = int(lines[1][8:])
        if version < 1 or str(version) != lines[1][8:]:
            raise ManifestError("versao")
        exe = lines[2][4:]
        _check_path(exe)
        m = Manifest(version=version, exe=exe)
        for ln in lines[3:]:
            if ln.startswith("exclude "):
                d = ln[8:]
                _check_path(d)
                if d in m.exclude or m.files or m.dirs:
                    raise ManifestError("exclude")
                m.exclude.append(d)
            elif ln.startswith("F "):
                p, _, h = ln[2:].partition("\t")
                _check_path(p)
                if not _HEX64.match(h) or p in m.files or p in MANIFEST_FILES or p.lower() in ("manifest.txt", "manifest.sig", "uninstall.exe") or m.dirs:
                    raise ManifestError("F")
                m.files[p] = h
            elif ln.startswith("D "):
                p, _, c = ln[2:].partition("\t")
                _check_path(p, allow_dot=True)
                if not c.isascii() or not c.isdigit() or str(int(c)) != c or p in m.dirs:
                    raise ManifestError("D")
                m.dirs[p] = int(c)
            else:
                raise ManifestError("linha")
        if m.exe not in m.files:
            raise ManifestError("exe fora da lista")
        if render(m) != text:  # forma canonica unica (ordem, sem variacoes)
            raise ManifestError("nao canonico")
        # consistencia: contagem por pasta bate com a lista de arquivos
        counts: dict[str, int] = {}
        for p in m.files:
            d = p.rsplit("/", 1)[0] if "/" in p else "."
            counts[d] = counts.get(d, 0) + 1
        for d, n in counts.items():
            if m.dirs.get(d) != n:
                raise ManifestError("contagem")
        if any(n != counts.get(d, 0) for d, n in m.dirs.items()):
            raise ManifestError("contagem")
        return m
    except ManifestError:
        raise
    except Exception:
        raise ManifestError("invalido") from None


def sign(text: str, release_seed_b64: str) -> str:
    sk = SigningKey(_b64d(release_seed_b64))
    return _b64e(sk.sign(DOM_MANIFEST + text.encode("utf-8")).signature)


def verify(text: str, sig_b64: str, release_pubkey_b64: str) -> bool:
    try:
        VerifyKey(_b64d(release_pubkey_b64)).verify(DOM_MANIFEST + text.encode("utf-8"), _b64d(sig_b64))
        return True
    except Exception:
        return False


def load_verified(text: str, sig_b64: str, release_pubkey_b64: str) -> Manifest:
    """Verifica assinatura ANTES de parsear. Lanca ManifestError se qualquer coisa falhar."""
    if not verify(text, sig_b64, release_pubkey_b64):
        raise ManifestError("assinatura")
    return parse(text)


# ---------------- construcao a partir de disco (script de build / atestacao simulada) ----------------
def scan_dir(root: str, exclude: list[str] | None = None) -> tuple[dict[str, str], dict[str, int]]:
    exclude = list(exclude or [])
    files: dict[str, str] = {}
    dirs: dict[str, int] = {}
    root = os.path.abspath(root)
    for cur, dnames, fnames in os.walk(root, followlinks=False):
        rel = os.path.relpath(cur, root).replace(os.sep, "/")
        rel = "." if rel == "." else rel
        dnames[:] = [d for d in dnames
                     if ((d if rel == "." else f"{rel}/{d}") not in exclude)]
        for d in dnames:
            if os.path.islink(os.path.join(cur, d)):
                raise ManifestError("symlink/junction nao permitido")
        n = 0
        for f in fnames:
            full = os.path.join(cur, f)
            if os.path.islink(full):
                raise ManifestError("symlink nao permitido")
            if rel == "." and (f in MANIFEST_FILES or f.lower() in ("manifest.txt", "manifest.sig", "uninstall.exe")):
                continue
            p = f if rel == "." else f"{rel}/{f}"
            with open(full, "rb") as fh:
                files[p] = hashlib.file_digest(fh, "sha256").hexdigest()
            n += 1
        dirs[rel] = n
    return files, dirs


def build_from_dir(root: str, version: int, exe: str, exclude: list[str] | None = None) -> Manifest:
    files, dirs = scan_dir(root, exclude)
    if exe not in files:
        raise ManifestError("exe nao encontrado")
    m = Manifest(version=version, exe=exe, exclude=sorted(exclude or [], key=_key), files=files, dirs=dirs)
    return parse(render(m))  # valida o proprio resultado


def attestation_from_dir(root: str, manifest: Manifest) -> dict:
    """O que o cliente calcula (referencia do algoritmo; o C++ replica isto)."""
    files, dirs = scan_dir(root, manifest.exclude)
    return {
        "manifest_version": manifest.version,
        "files_root": compute_root(files, dirs),
        "file_count": len(files),
        "exe_hash": files.get(manifest.exe, "0" * 64),
    }
