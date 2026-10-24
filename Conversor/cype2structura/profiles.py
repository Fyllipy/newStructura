"""Perfis do CYPE 3D -> familias/parametros de secao do Structura (somente familias ja existentes)."""
from __future__ import annotations

import json
import math
import os
import re
import struct
from dataclasses import dataclass, field

DEFAULT_STRUCTURA_CATALOG = r"C:\Users\João\Documents\Repos\newStructura\data\catalog_steel.json"
INCH = 25.4


@dataclass
class SectionDef:
    key: tuple                      # (serie, nome) do CYPE
    family: str                     # familia do Structura
    params: dict                    # parametros em mm
    area: float                     # m2
    iz: float                       # m4 (flexao no plano da altura)
    iy: float                       # m4
    j: float                        # m4
    source: str = ""                # origem dos valores (catalogo Structura, biblioteca CYPE, geometria)
    display: str = ""
    warnings: list = field(default_factory=list)
    material: str = "steel"          # 'steel' | 'timber'


# ---------------------------------------------------------------- biblioteca de perfis embutida no .ed3
def _ustr(s: str) -> bytes:
    return struct.pack("<I", len(s)) + s.encode("utf-16le")


def find_library_record(buf: bytes, series: str, name: str, limit: int = 0x90000):
    """Posicao logo apos o nome do perfil dentro da serie `series` da biblioteca embutida."""
    sb, nb = _ustr(series), _ustr(name)
    pos = 0
    while True:
        i = buf.find(sb, pos, limit)
        if i < 0:
            return None
        pos = i + 1
        j = i + len(sb)
        if j + 4 > len(buf):
            return None
        ln = struct.unpack_from("<I", buf, j)[0]
        if not 10 <= ln <= 120:
            continue
        end = j + 4 + 2 * ln
        k = buf.find(nb, end, min(limit, end + 80000))
        if k >= 0:
            return k + len(nb)


def _decode_flagged(buf: bytes, p: int, n_raw: int, n_flag: int):
    vals = []
    for _ in range(n_raw):
        vals.append(struct.unpack_from("<d", buf, p)[0])
        p += 8
    for _ in range(n_flag):
        vals.append(struct.unpack_from("<d", buf, p + 1)[0])
        p += 9
    return vals


def library_i_or_u(buf: bytes, series: str, name: str):
    """Perfis I/H/W/U laminados: h, b, tw, tf (m) e A, Iy(forte), Iz(fraca), J (SI)."""
    p = find_library_record(buf, series, name)
    if p is None:
        return None
    try:
        v = _decode_flagged(buf, p, 4, 10)
    except struct.error:
        return None
    h, b, tw, tf = v[0:4]
    # v[4]=? (14.0), v[5]=r, v[6]=?, v[7]=A, v[8]=Iy(forte), v[9]=Iz(fraca), v[10]=J
    area, iy_strong, iz_weak, jt = v[7], v[8], v[9], v[10]
    if not (0.01 < h < 3 and 0.01 < b < 2 and 1e-4 < area < 1):
        return None
    return {"d": h * 1000, "bf": b * 1000, "tw": tw * 1000, "tf": tf * 1000, "A": area,
            "Istrong": iy_strong, "Iweak": iz_weak, "J": jt}


# ---------------------------------------------------------------- catalogo do Structura (somente leitura)
_catalog_cache = {}


def structura_catalog(path: str | None = None):
    path = path or DEFAULT_STRUCTURA_CATALOG
    if path not in _catalog_cache:
        try:
            with open(path, encoding="utf-8") as fh:
                _catalog_cache[path] = json.load(fh)["families"]
        except OSError:
            _catalog_cache[path] = {}
    return _catalog_cache[path]


def _norm(s: str) -> str:
    return re.sub(r"\(H\)", "", re.sub(r"[\s\"']", "", s.replace("(H)", ""))).upper()


def catalog_lookup(family: str, name: str, path: str | None = None):
    for prof in structura_catalog(path).get(family, []):
        if _norm(prof["name"]) == _norm(name):
            return prof
    return None


# ---------------------------------------------------------------- geometria (propriedades)
def polygon_props(loop):
    """Area, Iz (integral y^2 dA) e Iy (integral x^2 dA) de um poligono simples, em relacao ao centroide."""
    a = ix = iy = cx = cy = 0.0
    n = len(loop)
    for k in range(n):
        x0, y0 = loop[k]
        x1, y1 = loop[(k + 1) % n]
        cr = x0 * y1 - x1 * y0
        a += cr / 2
        cx += (x0 + x1) * cr
        cy += (y0 + y1) * cr
        ix += cr * (y0 * y0 + y0 * y1 + y1 * y1) / 12
        iy += cr * (x0 * x0 + x0 * x1 + x1 * x1) / 12
    if a < 0:
        a, ix, iy, cx, cy = -a, -ix, -iy, -cx, -cy
    cx /= 6 * a
    cy /= 6 * a
    return a, ix - a * cy * cy, iy - a * cx * cx


def rect_torsion(a, b):
    if a < b:
        a, b = b, a
    return a * b ** 3 * (1 / 3 - 0.21 * (b / a) * (1 - b ** 4 / (12 * a ** 4)))


def _mm(area, iz, iy, j):
    return area * 1e-6, iz * 1e-12, iy * 1e-12, j * 1e-12


def _frac(txt: str) -> float:
    total = 0.0
    for part in txt.split():
        if "/" in part:
            a, b = part.split("/")
            total += float(a) / float(b)
        else:
            total += float(part)
    return total


def _thin_j(p):
    return (2 * p["bf"] * p["tf"] ** 3 + (p["d"] - p["tf"]) * p["tw"] ** 3) / 3


def _w_from_params(key, p, display):
    d, bf, tw, tf = p["d"], p["bf"], p["tw"], p["tf"]
    loop = [(-bf / 2, -d / 2), (bf / 2, -d / 2), (bf / 2, -d / 2 + tf), (tw / 2, -d / 2 + tf), (tw / 2, d / 2 - tf),
            (bf / 2, d / 2 - tf), (bf / 2, d / 2), (-bf / 2, d / 2), (-bf / 2, d / 2 - tf), (-tw / 2, d / 2 - tf),
            (-tw / 2, -d / 2 + tf), (-bf / 2, -d / 2 + tf)]
    a, ix, iy = polygon_props(loop)
    j = (2 * bf * tf ** 3 + (d - tf) * tw ** 3) / 3
    a, iz, iy, j = _mm(a, ix, iy, j)
    return SectionDef(key, "W Laminado", p, a, iz, iy, j, "catalogo Structura", display)


# ---------------------------------------------------------------- mapeamento principal
def to_section(buf: bytes, series: str, name: str, catalog_path: str | None = None) -> SectionDef:
    key = (series, name)
    s = series.strip()
    n = name.strip()
    # madeira macica retangular: serie "Maciça hNNN", nome "S-HxB"
    m = re.match(r"(?:S-)?(\d+(?:[.,]\d+)?)x(\d+(?:[.,]\d+)?)$", n)
    if m and ("Maci" in s or "Caibro" in s or "Ter" in s or "Ripa" in s or "Madeira" in s):
        h = float(m.group(1).replace(",", "."))
        b = float(m.group(2).replace(",", "."))
        a, iz, iy, j = _mm(b * h, b * h ** 3 / 12, h * b ** 3 / 12, rect_torsion(b, h))
        return SectionDef(key, "Retangular", {"b": b, "h": h}, a, iz, iy, j, "geometria nominal", f"Madeira {b:g}x{h:g}", material="timber")
    # barra redonda (aco) ou redondo (madeira rolica)
    if s.lower().startswith("barra redonda") or s.lower().startswith("redondo"):
        d = None
        mm = re.search(r"(\d+)\s*/\s*(\d+)\s*\"", n)
        if mm:
            d = float(mm.group(1)) / float(mm.group(2)) * INCH
        else:
            mm = re.search(r"(\d+(?:[.,]\d+)?)", n)
            if mm:
                d = float(mm.group(1).replace(",", "."))
        if d:
            a, iz, iy, j = _mm(math.pi * d * d / 4, math.pi * d ** 4 / 64, math.pi * d ** 4 / 64, math.pi * d ** 4 / 32)
            return SectionDef(key, "Barra Circular", {"d": round(d, 3)}, a, iz, iy, j, "geometria nominal", f"Barra redonda {d:g}",
                              material="timber" if s.lower().startswith("redondo") else "steel")
    # perfis W (Gerdau) do catalogo do Structura (mesma nomenclatura do CYPE: W150X22.5)
    mw = re.match(r"W\s*(\d+)\s*[xX]\s*([\d.]+)$", n)
    if mw:
        prof = catalog_lookup("W Laminado", f"W {mw.group(1)} x {mw.group(2)}", catalog_path)
        lib = library_i_or_u(buf, s, n)
        if prof:
            params = {k: prof[k] for k in ("d", "bf", "tw", "tf") if k in prof}
            if lib:
                j_lib = lib["J"] if lib["J"] > 1.5e-9 else _thin_j(params) * 1e-12
                return SectionDef(key, "W Laminado", params, lib["A"], lib["Istrong"], lib["Iweak"], j_lib,
                                  "catalogo Structura (dimensoes) + biblioteca CYPE (A, I, J)", prof["name"])
            return _w_from_params(key, params, prof["name"])
    # perfis I/H laminados europeus da biblioteca do CYPE (IPE, HE, IPN...)
    lib = library_i_or_u(buf, s, n)
    if lib and re.match(r"(IPE|HE|HD|HL|HP|IPN|W|UB|UC)", n, re.I):
        params = {k: round(lib[k], 3) for k in ("d", "bf", "tw", "tf")}
        j_lib = lib["J"] if lib["J"] > 1.5e-9 else _thin_j(params) * 1e-12
        return SectionDef(key, "W Laminado", params, lib["A"], lib["Istrong"], lib["Iweak"], j_lib, "biblioteca CYPE", n)
    if lib and re.match(r"(UPE|UPN)", n, re.I):
        params = {k: round(lib[k], 3) for k in ("d", "bf", "tw", "tf")}
        return SectionDef(key, "U Laminado", params, lib["A"], lib["Istrong"], lib["Iweak"], lib["J"], "biblioteca CYPE", n)
    # U soldado/fabricado da biblioteca do CYPE: "UF-120x4" (d, bf, tw, tf nos quatro primeiros valores)
    mf = re.match(r"UF-(\d+(?:\.\d+)?)x(\d+(?:\.\d+)?)$", n)
    if mf:
        pos = find_library_record(buf, s, n)
        if pos is not None:
            d_, bf_, tw_, tf_ = (struct.unpack_from("<d", buf, pos + 8 * i)[0] * 1000 for i in range(4))
            if 10 < d_ < 2000 and 5 < bf_ < 1000:
                loop = [(0, 0), (bf_, 0), (bf_, tf_), (tw_, tf_), (tw_, d_ - tf_), (bf_, d_ - tf_), (bf_, d_), (0, d_)]
                a, ix, iy = polygon_props(loop)
                j = (2 * bf_ * tf_ ** 3 + (d_ - tf_) * tw_ ** 3) / 3
                a, iz, iy, j = _mm(a, ix, iy, j)
                return SectionDef(key, "U Laminado", {"d": round(d_, 3), "bf": round(bf_, 3), "tw": round(tw_, 3), "tf": round(tf_, 3)},
                                  a, iz, iy, j, "biblioteca CYPE (dimensoes) + geometria", n)
    # C enrijecido formado a frio: "C250X85X25X4.76" (d x bf x c x t)
    mc = re.match(r"C\s*(\d+(?:\.\d+)?)[xX](\d+(?:\.\d+)?)[xX](\d+(?:\.\d+)?)[xX](\d+(?:\.\d+)?)$", n)
    if mc:
        d, bf, c, t = (float(mc.group(i)) for i in (1, 2, 3, 4))
        loop = [(0, 0), (bf, 0), (bf, c), (bf - t, c), (bf - t, t), (t, t), (t, d - t), (bf - t, d - t), (bf - t, d - c),
                (bf, d - c), (bf, d), (0, d)]
        a, ix, iy = polygon_props(loop)
        j = (d + 2 * bf + 2 * c - 4 * t) * t ** 3 / 3
        a, iz, iy, j = _mm(a, ix, iy, j)
        return SectionDef(key, "C Formado", {"d": d, "bf": bf, "c": c, "t": t}, a, iz, iy, j, "geometria nominal", n)
    # tubo circular: "273 x 7.9" (D x t)
    mt = re.match(r"(\d+(?:\.\d+)?)\s*[xX]\s*(\d+(?:\.\d+)?)$", n)
    if mt and s.lower().startswith("tubo circular"):
        D, t = float(mt.group(1)), float(mt.group(2))
        di = D - 2 * t
        a = math.pi / 4 * (D * D - di * di)
        i = math.pi / 64 * (D ** 4 - di ** 4)
        a, iz, iy, j = _mm(a, i, i, 2 * i)
        return SectionDef(key, "Tubular Circular", {"D": D, "t": t}, a, iz, iy, j, "geometria nominal", n)
    # cantoneira de abas iguais em polegadas: "L 5/8 X 1/8"
    ml = re.match(r"L\s*([\d/ ]+?)\s*[xX]\s*([\d/ ]+)$", n)
    if ml and s.upper().startswith("L"):
        b = _frac(ml.group(1)) * INCH
        t = _frac(ml.group(2)) * INCH
        loop = [(0, 0), (b, 0), (b, t), (t, t), (t, b), (0, b)]
        a, ix, iy = polygon_props(loop)
        j = 2 * (b - t / 2) * t ** 3 / 3
        a, iz, iy, j = _mm(a, ix, iy, j)
        return SectionDef(key, "L Laminado", {"b": round(b, 3), "t": round(t, 3)}, a, iz, iy, j, "geometria nominal", n)
    # U formado a frio: "U100X50X3.04" (d x bf x t)
    mu = re.match(r"U\s*(\d+(?:\.\d+)?)\s*[xX]\s*(\d+(?:\.\d+)?)\s*[xX]\s*(\d+(?:\.\d+)?)$", n)
    if mu:
        d, bf, t = (float(mu.group(i)) for i in (1, 2, 3))
        loop = [(0, 0), (bf, 0), (bf, t), (t, t), (t, d - t), (bf, d - t), (bf, d), (0, d)]
        a, ix, iy = polygon_props(loop)
        length = d + 2 * bf - 2 * t
        j = length * t ** 3 / 3
        a, iz, iy, j = _mm(a, ix, iy, j)
        return SectionDef(key, "U Formado", {"d": d, "bf": bf, "t": t}, a, iz, iy, j, "geometria nominal", n)
    raise ValueError(f"perfil nao suportado: serie={series!r} nome={name!r}")
