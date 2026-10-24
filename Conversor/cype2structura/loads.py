"""Cargas superficiais (paineis do CYPE 3D) -> cargas lineares nas barras de apoio (kN/m), no estilo do Structura."""
from __future__ import annotations

import math
import re

from .model import Model, Panel, TF, add, cross, dot, mul, norm, sub, unit

GRAVITY = (0.0, 0.0, -1.0)


def hypothesis_case(model: Model, hyp_id: int) -> tuple[str, str]:
    """(nome, tipo) do caso de carga para o id de hipotese de usuario do CYPE (ids 0, 3, 6, 9... = posicao*3)."""
    names = model.hypothesis_names
    idx = 1 + hyp_id // 3 if hyp_id % 3 == 0 else None
    if idx is not None and idx < len(names):
        raw = names[idx]
    else:
        raw = f"H{hyp_id}"
    token = re.sub(r"\s+", "", raw) or f"H{hyp_id}"
    kind = "wind" if re.match(r"(?i)^(v\d|v\s?\d|vento|wind)", raw.strip()) or "vento" in raw.lower() else "gravity"
    return token, kind


def _basis(panel: Panel):
    u = unit(panel.u)
    v = unit(panel.v)
    n = unit(cross(u, v))
    return u, v, n


def _to3d(panel: Panel, p2):
    return add(panel.origin, add(mul(panel.u, p2[0]), mul(panel.v, p2[1])))


def polygon3d(panel: Panel):
    return [_to3d(panel, (p[0], p[1])) for p in panel.polygon2d]


def _in_polygon(pt, poly, tol):
    """Ponto (x, y) dentro do poligono 2D (ray casting) ou a menos de `tol` de uma aresta."""
    x, y = pt
    inside = False
    n = len(poly)
    for i in range(n):
        x1, y1 = poly[i]
        x2, y2 = poly[(i + 1) % n]
        if (y1 > y) != (y2 > y):
            xi = x1 + (y - y1) * (x2 - x1) / (y2 - y1)
            if x < xi:
                inside = not inside
        # distancia ao segmento
        dx, dy = x2 - x1, y2 - y1
        L2 = dx * dx + dy * dy
        t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((x - x1) * dx + (y - y1) * dy) / L2))
        if math.hypot(x - (x1 + t * dx), y - (y1 + t * dy)) <= tol:
            return True
    return inside


def supporting_bars(model: Model, panel: Panel, plane_tol=0.03, edge_tol=0.05):
    """Barras contidas no plano do painel e dentro do contorno. Devolve [(id, a, b)]."""
    u, v, n = _basis(panel)
    found = []
    for bar in model.bars.values():
        a = model.nodes[bar.ni].xyz
        b = model.nodes[bar.nj].xyz
        if abs(dot(sub(a, panel.origin), n)) > plane_tol or abs(dot(sub(b, panel.origin), n)) > plane_tol:
            continue
        mid = mul(add(a, b), 0.5)
        d = sub(mid, panel.origin)
        if _in_polygon((dot(d, u), dot(d, v)), panel.polygon2d, edge_tol):
            found.append((bar.id, a, b))
    return found


def distribute_panel(model: Model, panel: Panel):
    """Largura de influencia (m) de cada barra de apoio: {id: (largura, versor_da_barra)}.

    Barras de apoio = barras no plano do painel perpendiculares a direcao de vao do painel (quando conhecida),
    senao a familia de barras paralelas mais numerosa. As larguras cobrem todo o painel (a primeira/ultima barra
    recebem tambem o balanço ate a borda), para conservar a carga total.
    """
    bars = supporting_bars(model, panel)
    if not bars:
        return {}, "nenhuma barra no plano do painel"
    u, v, n = _basis(panel)
    # agrupa por direcao (paralelas, 5 graus)
    families = []
    for bid, a, b in bars:
        d = unit(sub(b, a))
        for fam in families:
            if abs(dot(fam["dir"], d)) > math.cos(math.radians(5)):
                fam["items"].append((bid, a, b))
                break
        else:
            families.append({"dir": d, "items": [(bid, a, b)]})
    chosen = None
    note = ""
    if panel.span_dir is not None:
        s = add(mul(u, panel.span_dir[0]), mul(v, panel.span_dir[1]))
        s = unit(s)
        support_dir = unit(cross(n, s))      # barras de apoio sao perpendiculares ao vao
        best = max(families, key=lambda f: abs(dot(f["dir"], support_dir)))
        if abs(dot(best["dir"], support_dir)) > math.cos(math.radians(15)):
            chosen = best
        else:
            note = "direcao de vao do painel nao coincide com nenhuma familia de barras; usada a familia mais numerosa"
    if chosen is None:
        chosen = max(families, key=lambda f: len(f["items"]))
    d = chosen["dir"]
    w = unit(cross(n, d))                     # dentro do plano, perpendicular as barras
    ts = []
    for bid, a, b in chosen["items"]:
        mid = mul(add(a, b), 0.5)
        ts.append((dot(mid, w), bid))
    ts.sort()
    # barras colineares (mesma linha de apoio) compartilham a mesma largura de influencia
    lines = []                       # [t medio, [ids]]
    for t, bid in ts:
        if lines and abs(t - lines[-1][0]) < 0.02:
            lines[-1][1].append(bid)
        else:
            lines.append([t, [bid]])
    verts = [dot(p, w) for p in polygon3d(panel)]
    tmin, tmax = min(verts), max(verts)
    out = {}
    for k, (t, ids) in enumerate(lines):
        low = tmin if k == 0 else 0.5 * (lines[k - 1][0] + t)
        high = tmax if k == len(lines) - 1 else 0.5 * (t + lines[k + 1][0])
        low = min(low, t)
        high = max(high, t)
        for bid in ids:
            out[bid] = (high - low, d)
    return out, note


def structure_centroid(model: Model):
    pts = [n.xyz for n in model.nodes.values()]
    return tuple(sum(p[i] for p in pts) / len(pts) for i in range(3))


def panel_loads(model: Model):
    """Gera (area_loads, member_loads, cases, notes).

    - member_loads: lista de dict(member, case, q=(qx,qy,qz) kN/m global)
    - area_loads: lista de dict(case, kind, pressure kN/m2, direction, polygon, members, description)
    """
    centroid = structure_centroid(model)
    member_loads: dict[tuple, list] = {}
    area_loads = []
    cases: dict[str, str] = {}
    notes: list[str] = []
    for panel in model.panels:
        if not panel.loads:
            continue
        distribution, note = distribute_panel(model, panel)
        if note:
            notes.append(f"painel {panel.id}: {note}")
        if not distribution:
            notes.append(f"painel {panel.id}: cargas ignoradas (nenhuma barra de apoio encontrada)")
            continue
        u, v, n = _basis(panel)
        poly3 = polygon3d(panel)
        pc = tuple(sum(p[i] for p in poly3) / len(poly3) for i in range(3))
        outward = n if dot(n, sub(pc, centroid)) >= 0 else mul(n, -1)
        for (htype, hid, value_tf) in panel.loads:
            name, kind = hypothesis_case(model, hid)
            cases[name] = kind
            pressure = value_tf * TF
            if kind == "wind":
                direction = mul(outward, -1.0)       # pressao positiva atua contra a superficie
                if value_tf < 0:
                    notes.append(f"painel {panel.id}: vento {name} negativo (succao)")
            else:
                direction = GRAVITY
                if value_tf < 0:
                    notes.append(f"painel {panel.id}: carga {name} negativa ({value_tf:.4g} tf/m2): aplicada no sentido ascendente")
            for bid, (width, bar_dir) in distribution.items():
                q = mul(direction, pressure * width)
                member_loads.setdefault((bid, name), [0.0, 0.0, 0.0])
                acc = member_loads[(bid, name)]
                acc[0] += q[0]; acc[1] += q[1]; acc[2] += q[2]
            area_loads.append({
                "case": name, "kind": "WIND" if kind == "wind" else "AREA", "pressure": abs(pressure),
                "direction": direction if pressure >= 0 else mul(direction, -1.0),
                "polygon": poly3, "members": sorted(distribution), "description": f"CYPE painel {panel.id}",
            })
    ml = [{"member": b, "case": c, "q": tuple(q)} for (b, c), q in sorted(member_loads.items())]
    return area_loads, ml, cases, notes
