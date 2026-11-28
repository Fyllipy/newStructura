"""Modelo MEF elástico 3D (hexaedros, CalculiX via Mecway) de UM elemento T de um T-stub de duas abas T com 2 parafusos (placa, SEM filete).

HIPOTESES DE MODELAGEM (todas explicitas; nada disto e dado experimental de [P2]):
 H1  Simetria em x (plano que contem os eixos dos parafusos, no meio da largura b) e em y (plano medio da alma). Quarto de modelo.
 H2  Plano de contato entre as duas abas T = plano de simetria z = 0: cada aba contra um plano rigido SEM atrito (contato unilateral). Valido porque as duas abas
     sao iguais, carregadas de forma oposta e o contato e sem atrito.
 H3  Parafuso = coluna de hexaedros ACIMA da aba, sobre a area da arruela (quadrado dw x dw), com modulo equivalente tal que a rigidez axial seja E*Ab/Lb
     (Lb = 2 tf + (th + tn)/2; metade do comprimento no modelo de simetria). So rigidez axial (sem flexao do parafuso, sem pre-tensao: aperto 'snug').
 H4  Material linear elastico, E = 210 GPa, nu = 0,3. Sem plasticidade, sem grandes deslocamentos.
 H6  Furo (opcional, p['hole']): quadrado de lado d_b+2 mm centrado no eixo do parafuso, vazado em toda a espessura da aba (a coluna do parafuso passa por ele).
 H5  Alma de espessura tw (NAO consta de [P2]) e altura hw: parametros de sensibilidade. Carga: forca vertical total F/4 distribuida em nos da face superior da alma
     (pesos = area tributaria); deslocamento da aba = media ponderada de uz nesses nos.
Rigidez do T-stub completo (duas abas T em serie, 2 parafusos): K = F/(2 d) com F = 4 F_q  =>  K = 2 F_q / d.
"""
import math
import os
import sys

import mwlib


def _subdiv(points, h, min_parts=None):
    """subdivide cada intervalo entre pontos consecutivos em partes de tamanho <= h; min_parts: dict intervalo->min."""
    out = [points[0]]
    for a, b in zip(points, points[1:]):
        k = max(1, math.ceil((b - a) / h - 1e-9))
        if min_parts and (a, b) in min_parts:
            k = max(k, min_parts[(a, b)])
        for i in range(1, k + 1):
            out.append(a + (b - a) * i / k)
    return out


def build(p, path, tag):
    """p: dict com mm: tf, m, n, b, tw, hw, dw, Ab, Lb ; h (tamanho base mm), nz (camadas na espessura), contact (bool), bolt (modo: 'eq'|'rigid'), kn (rigidez do contato GPa/m)."""
    mm = 1e-3
    tf, m_, n_, b, tw, hw, dw = (p[k] * mm for k in ('tf', 'm', 'n', 'b', 'tw', 'hw', 'dw'))
    Ab, Lb = p['Ab'] * mm ** 2, p['Lb'] * mm
    h = p['h'] * mm
    E, nu = 210e9, 0.3
    yb = tw / 2 + m_
    yend = yb + n_
    hole = p.get('hole', False)
    dhole = (p['db'] + 2.0) * mm if hole else 0.0              # H6: furo = d_b + 2 mm (folga padrao; HIPOTESE), quadrado equivalente de lado dhole
    xl = [0.0, dw / 2, b / 2] + ([dhole / 2] if hole else [])
    yl = [0.0, tw / 2, yb - dw / 2, yb + dw / 2, yend] + ([yb - dhole / 2, yb + dhole / 2] if hole else [])
    xs = _subdiv(sorted(set(xl)), h)
    ys = _subdiv(sorted(set(yl)), h, {(0.0, tw / 2): 2})
    zf = [tf * i / p['nz'] for i in range(p['nz'] + 1)]
    zw = [tf + hw * i / 3 for i in range(1, 4)]
    zc_n = 4
    Lh = Lb / 2
    zc = [tf + Lh * i / zc_n for i in range(1, zc_n + 1)]
    mod = mwlib.Model()
    Ap = dw * dw / 2                                   # area do quarto de arruela (x>=0, quadrado dw x dw)
    Ec = E * Ab / (dw * dw)                            # modulo equivalente (ver H3)
    if p['bolt'] == 'rigid':
        Ec = E * 1e4
    mod.mats = {'steel': (E, nu), 'bolt': (Ec, 0.0), 'rigid': (E * 1e3, nu)}
    mod.elsets_mat = {'FLANGE': 'steel', 'BOLT': 'bolt', 'GROUND': 'rigid'}
    ix = {x: i for i, x in enumerate(xs)}
    iy = {y: j for j, y in enumerate(ys)}

    def nid(body, i, j, zkey, z):
        return mod.node((body, i, j, zkey), xs[i], ys[j], z)

    zlist = zf + zw                                   # linhas z do corpo "FLANGE" (aba + alma)
    flange_bottom_faces, ground_top_faces = [], []
    web_top_nodes = {}
    # elementos da aba e da alma
    for k in range(len(zlist) - 1):
        zlo, zhi = zlist[k], zlist[k + 1]
        in_web = zlo >= tf - 1e-12
        for i in range(len(xs) - 1):
            for j in range(len(ys) - 1):
                cy = 0.5 * (ys[j] + ys[j + 1])
                cx = 0.5 * (xs[i] + xs[i + 1])
                if in_web and cy > tw / 2:
                    continue
                if hole and (not in_web) and cx < dhole / 2 and abs(cy - yb) < dhole / 2:
                    continue                                   # furo do parafuso (H6)
                n8 = [nid('F', i, j, k, zlo), nid('F', i + 1, j, k, zlo), nid('F', i + 1, j + 1, k, zlo), nid('F', i, j + 1, k, zlo),
                      nid('F', i, j, k + 1, zhi), nid('F', i + 1, j, k + 1, zhi), nid('F', i + 1, j + 1, k + 1, zhi), nid('F', i, j + 1, k + 1, zhi)]
                eid = mod.hex('FLANGE', n8)
                if k == 0:
                    flange_bottom_faces.append((eid, 1))
    # coluna do parafuso (acima da aba, sobre o quarto de arruela): compartilha os nos da face superior da aba
    kt = len(zf) - 1
    ztop_f = zlist[kt]
    ci = [i for i in range(len(xs) - 1) if xs[i + 1] <= dw / 2 + 1e-12]
    cj = [j for j in range(len(ys) - 1) if ys[j] >= yb - dw / 2 - 1e-12 and ys[j + 1] <= yb + dw / 2 + 1e-12]
    prev_key = kt
    col_top = []
    col_bottom = []
    for q, zhi in enumerate(zc):
        zlo = ztop_f if q == 0 else zc[q - 1]
        for i in ci:
            for j in cj:
                def cn(ii, jj, lvl, z):
                    if lvl == 0:
                        return nid('F', ii, jj, kt, ztop_f)           # no compartilhado com a aba
                    return mod.node(('C', ii, jj, lvl), xs[ii], ys[jj], z)
                n8 = [cn(i, j, q, zlo), cn(i + 1, j, q, zlo), cn(i + 1, j + 1, q, zlo), cn(i, j + 1, q, zlo),
                      cn(i, j, q + 1, zhi), cn(i + 1, j, q + 1, zhi), cn(i + 1, j + 1, q + 1, zhi), cn(i, j + 1, q + 1, zhi)]
                mod.hex('BOLT', n8)
                if q == zc_n - 1:
                    col_top += [n8[4], n8[5], n8[6], n8[7]]
                if q == 0:
                    col_bottom += [n8[0], n8[1], n8[2], n8[3]]
    mod.fix(2, sorted(set(col_top)))
    # solo rigido sob a aba (contato)
    if p['contact']:
        tg = 10 * mm
        for i in range(len(xs) - 1):
            for j in range(len(ys) - 1):
                n8 = [nid('G', i, j, 0, -tg), nid('G', i + 1, j, 0, -tg), nid('G', i + 1, j + 1, 0, -tg), nid('G', i, j + 1, 0, -tg),
                      nid('G', i, j, 1, 0.0), nid('G', i + 1, j, 1, 0.0), nid('G', i + 1, j + 1, 1, 0.0), nid('G', i, j + 1, 1, 0.0)]
                eid = mod.hex('GROUND', n8)
                ground_top_faces.append((eid, 2))
        gb = [mod.nodes[('G', i, j, 0)][0] for i in range(len(xs)) for j in range(len(ys))]
        for ax in (0, 1, 2):
            mod.fix(ax, gb)
        mod.face_sets = {'g_top': ground_top_faces, 'f_bot': flange_bottom_faces}
        mod.contacts = [('g_top', 'f_bot', 'unilateralelastic', p.get('kn', 200000.0))]
    # simetrias: x = 0 -> ux = 0 ; y = 0 -> uy = 0 (somente nos do corpo F e coluna)
    fx = [v[0] for key, v in mod.nodes.items() if key[0] in ('F', 'C') and abs(v[1]) < 1e-15]
    fy = [v[0] for key, v in mod.nodes.items() if key[0] in ('F', 'C') and abs(v[2]) < 1e-15]
    mod.fix(0, fx)
    mod.fix(1, fy)
    # carga: forca total FQ sobre os nos do topo da alma, pesos = area tributaria
    FQ = p['FQ']
    ztop_w = zlist[-1]
    top_nodes = [(key, v) for key, v in mod.nodes.items() if key[0] == 'F' and abs(v[3] - ztop_w) < 1e-12]
    trib = {}
    for key, v in top_nodes:
        i, j = key[1], key[2]
        if ys[j] > tw / 2 + 1e-12:
            continue
        wx = (xs[i + 1] - xs[i - 1] if 0 < i < len(xs) - 1 else (xs[1] - xs[0] if i == 0 else xs[-1] - xs[-2])) / 2
        wy = (ys[j + 1] - ys[j - 1] if 0 < j < len(ys) - 1 else (ys[1] - ys[0] if j == 0 else ys[-1] - ys[-2])) / 2
        trib[v[0]] = wx * wy
    # restringe aos nos realmente do topo da alma (y <= tw/2)
    webtop = {nid_: w for nid_, w in trib.items()}
    tot = sum(webtop.values())
    for nid_, w in webtop.items():
        mod.force((0.0, 0.0, FQ * w / tot), [nid_])
    meta = dict(webtop=webtop, tot=tot, col_bottom=sorted(set(col_bottom)), Ec=Ec, Ap=Ap, Lh=Lh, nelem=len(mod.elems), nnode=len(mod.nodes), E=E,
                flange_bottom=[mod.nodes[k][0] for k in mod.nodes if k[0] == 'F' and abs(mod.nodes[k][3]) < 1e-15])
    mod.to_liml(path)
    return meta


def solve(p, tag, workdir='work', timeout=900):
    import time
    os.makedirs(workdir, exist_ok=True)
    path = os.path.join(workdir, tag + '.liml')
    meta = build(p, path, tag)
    t0 = time.time()
    out = mwlib.run(path, os.path.join(workdir, tag + '.csv'), timeout=timeout)
    dt = time.time() - t0
    if not out:
        return None
    res = mwlib.read_results(out)
    tot = meta['tot']
    d = sum(res[n][2] * w for n, w in meta['webtop'].items()) / tot            # deslocamento medio ponderado da aba (m), topo da alma
    FQ = p['FQ']
    K_spec = 2.0 * FQ / d                                                     # N/m
    # forca no parafuso (quarto): modulo equivalente x area x deformacao medio da coluna
    ub = sum(res[n][2] for n in meta['col_bottom']) / len(meta['col_bottom'])
    Bq = meta['Ec'] * meta['Ap'] * abs(ub) / meta['Lh']
    return dict(K=K_spec / 1e6, d_mm=d * 1e3, Bq=Bq, Q=Bq - FQ, time=dt, nelem=meta['nelem'], nnode=meta['nnode'], ub_mm=ub * 1e3)


if __name__ == '__main__':
    # espécime-tipo: geometria do espécime 7 de [P2] SEM filete (r = 0) e tw/hw ASSUMIDOS (hipotese H5)
    p = dict(db=27.0, tf=13.9, m=52.45, n=71.7, b=122.8, tw=8.0, hw=30.0, dw=56.0, Ab=572.5553, Lb=49.975, h=6.0, nz=2, contact=True, bolt='eq', FQ=25e3)
    r = solve(p, 'run0')
    print(r)
