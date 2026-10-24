"""Gera o .dat do Structura (formato de DatModelIoService) a partir do modelo do CYPE 3D."""
from __future__ import annotations

import os
import re
from collections import defaultdict

from .loads import panel_loads
from .writer_support import release_masks, STEEL, TIMBER
from .model import Model, TF

def _token(text: str) -> str:
    return re.sub(r"\s+", "_", text.strip()) or "-"


def write_dat(model: Model, out_path: str, source_name: str = "", steel=None, timber=None, unit_combinations: bool = False, stabilize_model: bool = True) -> dict:
    steel = {**STEEL, **(steel or {})}
    timber = {**TIMBER, **(timber or {})}
    report = {"warnings": list(model.warnings), "assumptions": []}

    # --- secoes e materiais usados
    used_bars = [b for b in model.bars.values() if b.id in model.bar_section]
    skipped = [b.id for b in model.bars.values() if b.id not in model.bar_section]
    sec_ids = {}
    for b in used_bars:
        key = model.bar_section[b.id]
        if key not in sec_ids:
            sec_ids[key] = len(sec_ids) + 1
    uses_steel = any(model.materials.get(b.id) == "steel" for b in used_bars)
    uses_timber = any(model.materials.get(b.id) == "timber" for b in used_bars)
    mat_ids = {}
    if uses_steel:
        mat_ids["steel"] = len(mat_ids) + 1
    if uses_timber:
        mat_ids["timber"] = len(mat_ids) + 1

    # --- nos referenciados
    used_nodes = sorted({n for b in used_bars for n in (b.ni, b.nj)})
    masks, articulated, notes = release_masks(model)
    report["warnings"] += notes
    if stabilize_model:
        try:
            from .stability import stabilize
            report["warnings"] += stabilize(model, masks)
        except ImportError:
            report["warnings"].append("numpy nao instalado: verificacao de estabilidade (mecanismos) nao realizada")

    area_loads, member_loads, cases, load_notes = panel_loads(model)
    used_ids = {b.id for b in used_bars}
    member_loads = [m for m in member_loads if m["member"] in used_ids]
    report["warnings"] += load_notes

    L = []
    w = L.append
    w("# Convertido do CYPE 3D (%s) por cype2structura" % (source_name or model.program))
    w("# Unidades: kN, m (E e G em kN/m2; cargas superficiais do CYPE em tf/m2 convertidas com 1 tf = 9,80665 kN)")
    w("")
    w("[MATERIALS]")
    w("# ID    E (kN/m2)       G (kN/m2)")
    if "steel" in mat_ids:
        E = steel["E"]
        G = steel.get("G", E / (2 * (1 + steel["nu"])))
        w("%-8d %14.6e %14.6e" % (mat_ids["steel"], E, G))
    if "timber" in mat_ids:
        w("%-8d %14.6e %14.6e" % (mat_ids["timber"], timber["E"], timber["G"]))
    w("")
    w("[MATERIAL_PROPS]")
    w("# ID  peso especifico (kN/m3)  Nome  Biblioteca  fy(MPa)  fu(MPa)")
    if "steel" in mat_ids:
        w("%-8d %14.6e %s - 0 0" % (mat_ids["steel"], steel["gamma"], steel["name"]))
    if "timber" in mat_ids:
        w("%-8d %14.6e %s - 0 0" % (mat_ids["timber"], timber["gamma"], timber["name"]))
    w("")
    w("[SECTIONS]")
    w("# ID    A (m^2)     Iz (m^4)      Iy (m^4)      J (m^4)")
    for key, sid in sec_ids.items():
        s = model.sections[key]
        w("%-8d %14.6e %14.6e %14.6e %14.6e" % (sid, s.area, s.iz, s.iy, s.j))
    w("")
    w("[SECTION_GEOMETRY]")
    w("# ID    Familia(_ no lugar de espaco)  Espelhada(0/1)  parametro=valor (mm) ...")
    for key, sid in sec_ids.items():
        s = model.sections[key]
        params = " ".join("%s=%g" % (k, v) for k, v in s.params.items())
        w("%-8d %s 0 %s" % (sid, s.family.replace(" ", "_"), params))
    w("")
    w("[NODES]")
    w("# ID    X (m)    Y (m)    Z (m)    UX  UY  UZ  RX  RY  RZ")
    for nid in used_nodes:
        n = model.nodes[nid]
        w("%-8d %10.6f %10.6f %10.6f    %d   %d   %d   %d   %d   %d" % ((nid,) + n.xyz + n.restraints))
    w("")
    w("[MEMBERS]")
    w("# ID    Node_i   Node_j   Material_ID   Section_ID")
    for b in sorted(used_bars, key=lambda b: b.id):
        w("%-8d %8d %8d %10d %12d" % (b.id, b.ni, b.nj, mat_ids[model.materials[b.id]], sec_ids[model.bar_section[b.id]]))
    w("")
    w("[LOAD_CASES]")
    w("# Nome  Sigla  PesoProprio(0/1)")
    w("%-16s %-8s %d" % ("PP", "PP", 1))
    for name in cases:
        w("%-16s %-8s %d" % (name, name[:6], 0))
    w("")
    if unit_combinations:
        w("[COMBINATIONS]")
        w("# Nome  caso=fator ...   (uma combinacao unitaria por caso; o CYPE gera as combinacoes pela norma e elas nao sao convertidas)")
        for name in ["PP"] + list(cases):
            w("C_%s %s=1" % (name, name))
        w("")
    w("[NODAL_LOADS]")
    w("# Node_ID   Fx (N)    Fy (N)   Fz (N)   Mx (Nm)   My (Nm)   Mz (Nm)   Caso")
    w("")
    w("[MEMBER_LOADS]")
    w("# Formato: Member_ID  Sistema(Local/Global)  qx (kN/m)  qy (kN/m)  qz (kN/m)  Caso")
    for m in member_loads:
        w("%-12d %-10s %12.6f %12.6f %12.6f %s" % (m["member"], "GLOBAL", m["q"][0], m["q"][1], m["q"][2], m["case"]))
    # liberacoes de extremidade e barras de trelica (as duas extremidades articuladas)
    placements = []
    for b in sorted(used_bars, key=lambda b: b.id):
        ends = articulated.get(b.id)
        if not ends:
            continue
        mtype = 1 if ends == {"I", "J"} else 0
        placements.append((b.id, masks[b.id], mtype))
    if placements:
        w("")
        w("[MEMBER_PLACEMENT]")
        w("# Member_ID  Giro(graus)  Ponto_ref(0-8)  Desloc_z_I(m)  Desloc_y_I(m)  J_diferente(0/1)  Desloc_z_J(m)  Desloc_y_J(m)  Liberacoes(mascara)  Tipo(0=normal,1=trelica,2=tirante)")
        for bid, mask, mtype in placements:
            w("%-12d %12.6f %6d %14.6f %14.6f %4d %14.6f %14.6f %6d %3d" % (bid, 0.0, 0, 0.0, 0.0, 0, 0.0, 0.0, mask, mtype))
    if area_loads:
        w("")
        w("[AREA_LOADS]")
        w("# ID Caso('_'=espaco) Tipo(AREA/WIND) Pressao(kN/m2) dirX dirY dirZ nPontos x y z ... nBarras barras...")
        for i, a in enumerate(area_loads, 1):
            line = "%-4d %s %s %.6g %.6g %.6g %.6g %d" % (i, a["case"], a["kind"], a["pressure"], *a["direction"], len(a["polygon"]))
            for p in a["polygon"]:
                line += " %.6f %.6f %.6f" % p
            members = [m for m in a["members"] if m in used_ids]
            line += " %d" % len(members) + "".join(" %d" % m for m in members)
            w(line)
    text = "\r\n".join(L) + "\r\n"
    with open(out_path, "w", encoding="cp1252", newline="") as fh:
        fh.write(text)
    report.update({
        "nodes": len(used_nodes), "members": len(used_bars), "sections": len(sec_ids), "skipped_members": skipped,
        "member_loads": len(member_loads), "area_loads": len(area_loads), "cases": list(cases),
        "supports": sum(1 for nid in used_nodes if any(model.nodes[nid].restraints)),
        "released_members": len(placements),
    })
    return report
