"""Modelo estrutural extraido de um .ed3 (CYPE 3D 2026) e normalizado para o Structura.

Unidades do CYPE 3D (Brasil): tf, m  ->  Structura: kN, m  (1 tf = 9,80665 kN).
"""
from __future__ import annotations

import math
import re
import struct
from collections import defaultdict
from dataclasses import dataclass, field

from .lee_interp import Cells, Obj, Ptr
from .reader import Ed3ReadError, collect, program_version, read_obra
from .profiles import SectionDef, to_section

TF = 9.80665            # kN por tf
Vec = tuple


# ------------------------------------------------------------------------------ utilidades vetoriais
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def norm(a): return math.sqrt(dot(a, a))
def unit(a):
    n = norm(a)
    return (a[0] / n, a[1] / n, a[2] / n) if n > 1e-12 else (0.0, 0.0, 0.0)


# ------------------------------------------------------------------------------ dados
@dataclass
class Node:
    id: int
    xyz: Vec
    ext_type: int = 0                  # vinculacao exterior (enum ed3tipo_vinculacao_externa_t)
    int_type: int = 0                  # vinculacao interior: 0 rigida, 1 articulada, 3 generica
    int_groups: list = field(default_factory=list)   # [[ids de pecas rigidamente ligadas], ...]
    restraints: tuple = (0, 0, 0, 0, 0, 0)


@dataclass
class Bar:
    id: int
    ni: int
    nj: int
    piece: int = 0


@dataclass
class Piece:
    id: int
    bars: list
    desba: int


@dataclass
class Panel:
    id: int
    origin: Vec
    u: Vec
    v: Vec
    polygon2d: list
    span_dir: tuple | None            # direcao de vao no plano local (u, v)
    loads: list                       # [(tipo_hipotese, id_hipotese_usuario, valor_tf_m2)]


@dataclass
class Model:
    program: str
    nodes: dict
    bars: dict
    pieces: dict
    sections: dict                    # (serie, nome, slot) -> SectionDef
    bar_section: dict                 # bar id -> chave em `sections`
    panels: list
    hypothesis_names: list
    warnings: list = field(default_factory=list)
    materials: dict = field(default_factory=dict)   # bar id -> 'steel' | 'timber'


# Restricoes por tipo de vinculacao exterior: (ux, uy, uz, rx, ry, rz).
# Enumeracao do CYPE 3D (ed3tipo_vinculacion_externa_t, extraida do seletor de vinculacoes):
#   0 livre | 1 articulada | 2 generica (definida pelo usuario) | 3 engastada
#   4-7 engastada em reta X/Y/Z/usuario | 8-11 engastada em plano XY/XZ/YZ/usuario
#   12-15 articulada em reta X/Y/Z/usuario | 16-19 articulada em plano XY/XZ/YZ/usuario
# "Em reta/plano": o no fica preso a reta/plano e desliza ao longo dela (so os deslocamentos perpendiculares sao impedidos);
# "engastada" impede tambem as rotacoes.
_R = {
    0: (0, 0, 0, 0, 0, 0),
    1: (1, 1, 1, 0, 0, 0),
    3: (1, 1, 1, 1, 1, 1),
    4: (0, 1, 1, 1, 1, 1), 5: (1, 0, 1, 1, 1, 1), 6: (1, 1, 0, 1, 1, 1),
    8: (0, 0, 1, 1, 1, 1), 9: (0, 1, 0, 1, 1, 1), 10: (1, 0, 0, 1, 1, 1),
    12: (0, 1, 1, 0, 0, 0), 13: (1, 0, 1, 0, 0, 0), 14: (1, 1, 0, 0, 0, 0),
    16: (0, 0, 1, 0, 0, 0), 17: (0, 1, 0, 0, 0, 0), 18: (1, 0, 0, 0, 0, 0),
}


def uw(x):
    """Desembrulha ponteiros/celulas do interpretador ate o valor."""
    while True:
        if isinstance(x, Ptr):
            x = x.value
        elif isinstance(x, Cells):
            x = x[0] if len(x) == 1 else list(x)
        else:
            return x


def _pid(x, default):
    x = uw(x)
    return int(x) if isinstance(x, (int, float)) and not isinstance(x, bool) else default


def _args(obj):
    return obj.args if isinstance(obj, Obj) else []


def _ustr_pairs(buf: bytes, start: int, limit: int):
    """Procura, antes do bloco de dados, as duas listas de nomes das hipoteses (nomes completos e abreviados)."""
    def parse_list(p):
        if p + 4 > len(buf):
            return None, p
        n = struct.unpack_from("<I", buf, p)[0]
        if not 1 <= n <= 40:
            return None, p
        p += 4
        out = []
        for _ in range(n):
            if p + 4 > len(buf):
                return None, p
            ln = struct.unpack_from("<I", buf, p)[0]
            if not 1 <= ln <= 60:
                return None, p
            raw = buf[p + 4:p + 4 + 2 * ln]
            if len(raw) < 2 * ln or any(raw[k + 1] != 0 for k in range(0, len(raw), 2)):
                return None, p
            try:
                out.append(raw.decode("utf-16le"))
            except UnicodeDecodeError:
                return None, p
            p += 4 + 2 * ln
        return out, p

    for p in range(start, limit - 8):
        a, q = parse_list(p)
        if a is None or len(a) < 2:
            continue
        b, _ = parse_list(q)
        if b is not None and len(b) == len(a) and all(len(y) <= len(x) for x, y in zip(a, b)) \
                and all(re.fullmatch(r"[\w .+\-/()ºª°çãõáéíóúâêôà]*", x) for x in a):
            # a primeira entrada e o peso proprio
            if re.match(r"(?i)peso|self|dead|pp", a[0]):
                return a
    return []


def build_model(buf: bytes, material_policy=None) -> Model:
    obra = read_obra(buf)
    col = collect(obra.env)
    warnings: list[str] = []
    if obra.partial:
        warnings.append("leitura do arquivo interrompida apos as descricoes/cargas (ligacoes ou elementos de concreto nao lidos): "
                        "barras de concreto/pilares e dados de ligacao nao foram convertidos")

    planes = col["plane"]
    if len(planes) < 3:
        raise Ed3ReadError("grades de coordenadas (planos X, Y, Z) nao encontradas")
    px, py, pz = ({int(uw(e.args[0])): float(uw(e.args[1])) for e in lst} for lst in planes[:3])

    # ---- nos
    nodes: dict[int, Node] = {}
    for e in col["node"]:
        a = e.args
        nid = int(uw(a[0]))
        try:
            xyz = (px[int(uw(a[4]))], py[int(uw(a[5]))], pz[int(uw(a[6]))])
        except KeyError:
            warnings.append(f"no {nid}: plano de coordenada inexistente (ignorado)")
            continue
        interior = uw(a[18])
        exterior = uw(a[19])
        ext = int(uw(_args(exterior)[0])) if _args(exterior) else 0
        itype = int(uw(_args(interior)[0])) if _args(interior) else 0
        groups = []
        try:
            gobj = uw(_args(interior)[2])
            for g in uw(_args(gobj)[0]) or []:
                groups.append(list(uw(_args(g)[0]) or []))
        except (IndexError, TypeError):
            pass
        node = Node(nid, xyz, ext, itype, groups)
        if ext in _R:
            node.restraints = _R[ext]
        else:
            node.restraints = (0, 0, 0, 0, 0, 0)
            warnings.append(f"no {nid}: vinculacao exterior tipo {ext} (definida pelo usuario/generica) nao convertida; no deixado livre")
        nodes[nid] = node

    # ---- barras e pecas
    bars: dict[int, Bar] = {}
    for e in col["bar"]:
        a = e.args
        bars[int(uw(a[0]))] = Bar(int(uw(a[0])), int(uw(a[2])), int(uw(a[3])))
    pieces: dict[int, Piece] = {}
    for e in col["piece"]:
        a = e.args
        pc = Piece(int(uw(a[0])), list(uw(a[2]) or []), int(uw(a[3])))
        pieces[pc.id] = pc
        for b in pc.bars:
            if b in bars:
                bars[b].piece = pc.id

    # ---- descricoes (perfis)
    desba_profile: dict[int, tuple] = {}
    for e in col["description"]:
        a = e.args
        top = uw(a[1]) if len(a) > 1 else None
        types = uw(_args(top)[1]) if _args(top) else None
        if types is None:
            continue
        ta = _args(types)
        try:
            sel = int(uw(ta[0]))
            pzs = uw(ta[1 + sel])
            bp = uw(_args(pzs)[0])
            series, name = uw(_args(bp)[2]), uw(_args(bp)[3])
        except (IndexError, TypeError, ValueError):
            continue
        if isinstance(series, str) and isinstance(name, str):
            desba_profile[int(uw(a[0]))] = (series, name, sel)

    sections: dict[tuple, SectionDef] = {}
    bar_section: dict[int, tuple] = {}
    materials: dict[int, str] = {}
    for bar in bars.values():
        pc = pieces.get(bar.piece)
        if pc is None:
            warnings.append(f"barra {bar.id}: sem peca associada")
            continue
        prof = desba_profile.get(pc.desba)
        if prof is None:
            warnings.append(f"barra {bar.id}: peca {pc.id} sem descricao de perfil (secao nao convertida)")
            continue
        key = prof
        if key not in sections:
            try:
                sections[key] = to_section(buf, prof[0], prof[1])
            except ValueError as exc:
                warnings.append(str(exc))
                sections[key] = None
        if sections[key] is not None:
            bar_section[bar.id] = key
            materials[bar.id] = sections[key].material

    # ---- paineis de carga
    names = _ustr_pairs(buf, 0x1000, obra.offset)
    panels: list[Panel] = []
    pan_by_geom = {}

    def load_of(lobj):
        la = _args(lobj)
        hip = _args(uw(la[0])[0] if isinstance(uw(la[0]), list) else uw(la[0]))
        return (int(uw(hip[0])), int(uw(hip[2])), float(uw(la[2])))

    def geometry(a):
        origin = tuple(float(uw(a[k])) for k in (1, 2, 3))
        u = tuple(float(uw(a[k])) for k in (4, 5, 6))
        v = tuple(float(uw(a[k])) for k in (7, 8, 9))
        return origin, u, v, list(uw(a[10]))

    for e in col["panel"]:
        a = e.args
        origin, u, v, poly = geometry(a)
        loads = []
        for idx in (13, 14):
            for lo in uw(a[idx]) or []:
                loads.append(load_of(lo))
        p = Panel(_pid(a[0], len(panels) + 1), origin, u, v, poly, (float(uw(a[11])), float(uw(a[12]))), loads)
        panels.append(p)
        pan_by_geom[(tuple(round(c, 4) for c in origin), tuple(round(c, 4) for c in u), len(poly))] = p
    for e in col["surface_load"]:
        a = e.args
        origin, u, v, poly = geometry(a)
        key = (tuple(round(c, 4) for c in origin), tuple(round(c, 4) for c in u), len(poly))
        load = load_of(uw(a[11]))
        if key in pan_by_geom:
            if load not in pan_by_geom[key].loads:     # versoes antigas repetem a carga no painel e na superficie
                pan_by_geom[key].loads.append(load)
        else:
            p = Panel(_pid(a[0], len(panels) + 1), origin, u, v, poly, None, [load])
            panels.append(p)
            pan_by_geom[key] = p

    return Model(program_version(buf), nodes, bars, pieces, sections, bar_section, panels, names, warnings, materials)
