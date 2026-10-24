"""Valida um modelo convertido chamando o solver do Structura (rds.dll) diretamente: estabilidade e equilibrio."""
import ctypes
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from cype2structura.model import build_model, norm, sub, TF
from cype2structura.loads import panel_loads
from cype2structura.writer_support import release_masks, STEEL, TIMBER

TORSION_FREE = 0
DLL = r"C:\Users\João\Documents\Repos\newStructura\build\Release\rds.dll"


def arr(ctype, values):
    return (ctype * max(len(values), 1))(*values)


def solve(model, case, include_pp=True):
    lib = ctypes.CDLL(DLL)
    bars = [b for b in model.bars.values() if b.id in model.bar_section]
    nodes_ids = sorted({n for b in bars for n in (b.ni, b.nj)})
    sec_keys = sorted({model.bar_section[b.id] for b in bars})
    sid = {k: i + 1 for i, k in enumerate(sec_keys)}
    masks, articulated, _ = release_masks(model)
    from cype2structura.stability import stabilize
    stabilize(model, masks)
    area_loads, member_loads, cases, notes = panel_loads(model)

    mats = {"steel": (1, STEEL["E"], STEEL["E"] / (2 * (1 + STEEL["nu"])), STEEL["gamma"]),
            "timber": (2, TIMBER["E"], TIMBER["G"], TIMBER["gamma"])}
    mat_ids = [1, 2]
    E = [mats["steel"][1], mats["timber"][1]]
    G = [mats["steel"][2], mats["timber"][2]]

    lc = {}
    for ml in member_loads:
        if ml["case"] == case:
            lc[ml["member"]] = ml["q"]
    mload_ids, mqx, mqy, mqz = [], [], [], []
    for b in bars:
        q = list(lc.get(b.id, (0.0, 0.0, 0.0)))
        if include_pp:
            s = model.sections[model.bar_section[b.id]]
            gamma = mats[model.materials[b.id]][3]
            q[2] -= s.area * gamma
        if any(q):
            mload_ids.append(b.id)
            mqx.append(q[0]); mqy.append(q[1]); mqz.append(q[2])

    n_nodes = len(nodes_ids)
    node_x = [model.nodes[n].xyz[0] for n in nodes_ids]
    node_y = [model.nodes[n].xyz[1] for n in nodes_ids]
    node_z = [model.nodes[n].xyz[2] for n in nodes_ids]
    restr = [v for n in nodes_ids for v in model.nodes[n].restraints]
    rel = []
    for b in bars:
        m = masks.get(b.id, 0)
        ends = articulated.get(b.id, set())
        if ends == {"I", "J"}:
            pass   # a mascara de writer.release_masks ja inclui ry, rz nas duas extremidades e rx em J
        rel += [(m >> k) & 1 for k in range(12)]

    out_u = (ctypes.c_double * (n_nodes * 6))()
    out_r = (ctypes.c_double * (n_nodes * 6))()
    out_f = (ctypes.c_double * (len(bars) * 12))()
    out_k = (ctypes.c_double * (len(bars) * 3))()
    err = ctypes.create_string_buffer(2048)
    I, D = ctypes.c_int, ctypes.c_double
    rc = lib.structura_solve_v4(
        2, arr(I, mat_ids), arr(D, E), arr(D, G),
        len(sec_keys), arr(I, [sid[k] for k in sec_keys]), arr(D, [model.sections[k].area for k in sec_keys]),
        arr(D, [model.sections[k].iz for k in sec_keys]), arr(D, [model.sections[k].iy for k in sec_keys]),
        arr(D, [model.sections[k].j for k in sec_keys]),
        n_nodes, arr(I, nodes_ids), arr(D, node_x), arr(D, node_y), arr(D, node_z), arr(I, restr),
        len(bars), arr(I, [b.id for b in bars]), arr(I, [b.ni for b in bars]), arr(I, [b.nj for b in bars]),
        arr(I, [mats[model.materials[b.id]][0] for b in bars]), arr(I, [sid[model.bar_section[b.id]] for b in bars]),
        0, arr(I, []), arr(D, []),
        len(mload_ids), arr(I, mload_ids), arr(I, [1] * len(mload_ids)), arr(D, mqx), arr(D, mqy), arr(D, mqz),
        0, arr(I, []), arr(I, []), arr(D, []), arr(D, []), arr(D, []),
        None, None, arr(I, rel),
        out_u, out_r, out_f, out_k, 2048, err)
    if rc != 0:
        return rc, err.value.decode("latin-1"), None
    total = [0.0, 0.0, 0.0]
    for i in range(n_nodes):
        for c in range(3):
            total[c] += out_r[i * 6 + c]
    applied = [sum(mqx[i] * 0 for i in range(0)), 0.0, 0.0]
    lens = {b.id: norm(sub(model.nodes[b.nj].xyz, model.nodes[b.ni].xyz)) for b in bars}
    ax = sum(mqx[i] * lens[mload_ids[i]] for i in range(len(mload_ids)))
    ay = sum(mqy[i] * lens[mload_ids[i]] for i in range(len(mload_ids)))
    az = sum(mqz[i] * lens[mload_ids[i]] for i in range(len(mload_ids)))
    return 0, "ok", {"reacoes": tuple(round(x, 3) for x in total), "aplicado": (round(ax, 3), round(ay, 3), round(az, 3)),
                     "maxdesloc_mm": round(max(abs(out_u[i * 6 + c]) for i in range(n_nodes) for c in range(3)) * 1000, 3)}


if __name__ == "__main__":
    name = sys.argv[1]
    model = build_model(open(r"D:\CYPE Ingenieros\Projetos\CYPE 3D\%s.ed3" % name, "rb").read())
    _, _, cases, _ = panel_loads(model)
    for case in ["PP"] + list(cases):
        rc, msg, res = solve(model, case if case != "PP" else "__none__", include_pp=(case == "PP"))
        print(name, case, rc, msg, res)
