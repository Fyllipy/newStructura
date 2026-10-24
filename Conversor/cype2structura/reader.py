"""Localiza e le o bloco de dados da obra de um .ed3 (qualquer versao do CYPE 3D com leitor conhecido)."""
from __future__ import annotations

import re
from dataclasses import dataclass

from .lee_interp import Interp, Stream, Unsupported, load_funcs

ROOT = ("ed3dlib.dll", "FUN_1801d20b0")      # leitor do bloco `datos_obra`

# nomes das variaveis locais do leitor raiz que guardam cada colecao
VARS = {
    "planes": ("local_138", "local_140", "local_148"),   # planos X, Y, Z (posicoes das grelhas)
    "nodes": "local_150",
    "bars": "local_d8",
    "pieces": "local_158",
    "descriptions": "local_160",
    "groups": "local_e0",
    "panels": "local_168",
    "surface_loads": "local_100",
}


# funcoes construtoras (nome no executavel) que identificam o tipo de cada registro, independente da versao do arquivo
KINDS = {
    "FUN_1804c6680": "plane",
    "FUN_1803fbb50": "node",
    "FUN_1800d7e70": "bar",
    "FUN_1804bcec0": "piece",
    "FUN_180257050": "description",
    "FUN_1802fd950": "group",
    "FUN_180485d50": "panel",
    "FUN_1805d21d0": "surface_load",
}


class Ed3ReadError(Exception):
    pass


def collect(env: dict) -> dict:
    """Colecoes de registros lidas, por tipo. `plane` e uma lista [X, Y, Z] (ordem de leitura)."""
    from .lee_interp import Obj
    out = {k: [] for k in KINDS.values()}
    planes = []
    for v in env.values():
        if isinstance(v, list) and v and isinstance(v[0], Obj) and v[0].name in KINDS:
            kind = KINDS[v[0].name]
            if kind == "plane":
                planes.append(v)
            else:
                out[kind] = v
        elif isinstance(v, Obj) and v.name in KINDS:
            pass
    out["plane"] = planes
    return out


@dataclass
class ObraData:
    env: dict
    offset: int
    end: int
    partial: bool          # leitura interrompida (ex.: uniao nao suportada); colecoes iniciais ficam completas
    version: int           # versao do registro raiz


_interp = None


def interp() -> Interp:
    global _interp
    if _interp is None:
        _interp = Interp(load_funcs())
    return _interp


def program_version(buf: bytes) -> str:
    m = re.search(rb"(CYPE|EJPE)-\d{4}[.\-][\w.]+", buf[:64])
    return m.group().decode("latin-1") if m else "desconhecida"


def _try(buf: bytes, offset: int):
    it = interp()
    st = Stream(buf, offset)
    args = [None] * 10
    args[9] = st
    it.stack = []
    it.depth = 0
    partial = False
    try:
        _ret, env = it.call_user(ROOT[0], ROOT[1], args, want_env=True)
    except Unsupported as exc:
        env = getattr(exc, "env", None)
        partial = True
        if env is None:
            return None
    except (ValueError, KeyError, IndexError, TypeError, RecursionError, OverflowError):
        return None
    return env, st.pos, partial


# Registro de identificadores que antecede o bloco: tag, 6 x u32 (contadores), cadena unicode (nome do projeto)
_IDS = re.compile(rb"(?s)[\x08-\x14][\x00-\xff]{3}\x00(?:[\x00-\xff](?:[\x00-\xff]{3}\x00){6})?([\x00-\x40])\x00\x00\x00((?:[\x20-\x7e\xc0-\xff]\x00){0,64})([\x14-\x21])")


def _candidates(buf: bytes):
    """Posicoes provaveis do inicio do bloco (logo apos o registro de identificadores)."""
    seen = set()
    for m in _IDS.finditer(buf):
        if len(m.group(2)) // 2 == m.group(1)[0]:
            o = m.start(3)
            seen.add(o)
            yield o
    # varredura completa como reserva
    for o in range(len(buf) - 100):
        if o not in seen and 20 <= buf[o] <= 33 and buf[o + 3] == 0 and buf[o + 4] == 0:
            yield o


def read_obra(buf: bytes) -> ObraData:
    best = None
    for o in _candidates(buf):
        v = buf[o]
        res = _try(buf, o)
        if res is None:
            continue
        env, end, partial = res
        n_nodes = len(collect(env)["node"])
        if n_nodes > 0:
            return ObraData(env, o, end, partial, v)
        if best is None and not partial:
            best = ObraData(env, o, end, partial, v)
    if best is not None:
        return best
    raise Ed3ReadError("bloco de dados da obra nao encontrado (arquivo de versao sem leitor ou corrompido)")
