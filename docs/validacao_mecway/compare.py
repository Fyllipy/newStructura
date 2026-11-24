# Comparacao Structura x Mecway 28 (solver interno e CalculiX) para os casos exportados pelo val_dump.
# uso: python compare.py <pasta_cases> <pasta_mecway> <saida.json>
import csv
import json
import math
import os
import sys


def rows(path):
    if not os.path.exists(path):
        return None
    with open(path, newline='') as f:
        return [r for r in csv.reader(f) if r]


def fl(s):
    return float(s) if s not in ('', None) else float('nan')


def meta(path):
    d = {}
    for line in open(path, encoding='utf-8'):
        if '=' in line:
            k, v = line.rstrip('\n').split('=', 1)
            d[k] = v
    return d


def stats(diffs, ref):
    """max |dif| / max|ref|, erro L2 relativo e erro mediano nos pontos com |ref| > 5% do maximo."""
    pairs = [(d, r) for d, r in zip(diffs, ref) if not (math.isnan(d) or math.isnan(r))]
    if not pairs:
        return None
    mref = max(abs(r) for _, r in pairs)
    if mref == 0:
        return {'max_ref': 0.0, 'max_abs_diff': max(abs(d) for d, _ in pairs), 'rel_max': float('nan'), 'rel_l2': float('nan'), 'rel_median': float('nan'), 'n': len(pairs)}
    l2d = math.sqrt(sum(d * d for d, _ in pairs))
    l2r = math.sqrt(sum(r * r for _, r in pairs))
    big = sorted(abs(d) / abs(r) for d, r in pairs if abs(r) > 0.05 * mref)
    return {'max_ref': mref, 'max_abs_diff': max(abs(d) for d, _ in pairs), 'rel_max': max(abs(d) for d, _ in pairs) / mref,
            'rel_l2': l2d / l2r if l2r else float('nan'), 'rel_median': big[len(big) // 2] if big else float('nan'), 'n': len(pairs)}


def principal(a, b, c):
    """autovalores de [[a, c], [c, b]] (maior, menor)."""
    m = 0.5 * (a + b)
    r = math.sqrt((0.5 * (a - b)) ** 2 + c * c)
    return m + r, m - r


def navier_ss(a, q, D, terms=101):
    w = 0.0
    for m in range(1, terms, 2):
        for n in range(1, terms, 2):
            w += (-1) ** ((m + n) // 2 - 1) / (m * n * (m * m + n * n) ** 2) * 1.0
    return 16.0 * q * a ** 4 / (math.pi ** 6 * D) * w


def compare_case(case_dir, mw_dir, name, tag):
    m = meta(os.path.join(case_dir, 'meta.txt'))
    E, nu, p = float(m['E']), float(m['nu']), float(m['pressure'])
    nodes = [[float(v) for v in r[1:4]] for r in rows(os.path.join(case_dir, 'nodes.csv'))]
    quads = rows(os.path.join(case_dir, 'quads.csv'))
    t = float(quads[0][5])
    sd = {int(r[0]): [float(v) for v in r[1:7]] for r in rows(os.path.join(case_dir, 's_disp.csv'))}
    sr = {int(r[0]): [float(v) for v in r[1:7]] for r in rows(os.path.join(case_dir, 's_react.csv'))}
    se = {}
    for r in rows(os.path.join(case_dir, 's_elem.csv')):
        se[(int(r[0]), int(r[1]))] = [float(v) for v in r[2:]]
    mwn_rows = rows(os.path.join(mw_dir, '%s_mw_nodes.csv' % tag))
    mwe_rows = rows(os.path.join(mw_dir, '%s_mw_elem.csv' % tag))
    if mwn_rows is None:
        return {'case': name, 'solver': tag, 'error': 'sem resultados do Mecway'}
    hdr = mwn_rows[0]
    mwn_raw = {}
    for r in mwn_rows[1:]:
        mwn_raw[int(r[0])] = dict(zip(hdr[1:], [fl(v) for v in r[1:]]))
    if tag.endswith('_ccx'):
        # CalculiX: cada no de casca vira dois (superficies superior e inferior, a +-t/2 ao longo da normal): a media do par e o no da superficie media
        mwn = {}
        pts = [(nid, d['px'], d['py'], d['pz']) for nid, d in mwn_raw.items()]
        for i, (x, y, z) in enumerate(nodes):
            near = sorted(pts, key=lambda q: (q[1] - x) ** 2 + (q[2] - y) ** 2 + (q[3] - z) ** 2)[:2]
            ok = all(abs(math.sqrt((q[1] - x) ** 2 + (q[2] - y) ** 2 + (q[3] - z) ** 2) - 0.5 * t) < 0.02 * t + 1e-9 for q in near)
            if not ok:
                continue
            d = {}
            for k in ('displx', 'disply', 'displz'):
                d[k] = 0.5 * (mwn_raw[near[0][0]][k] + mwn_raw[near[1][0]][k])
            for k in ('rotx', 'roty', 'rotz', 'reactionforcex', 'reactionforcey', 'reactionforcez', 'reactionmomx', 'reactionmomy', 'reactionmomz'):
                a, b = mwn_raw[near[0][0]][k], mwn_raw[near[1][0]][k]
                d[k] = 0.5 * (a + b) if not (math.isnan(a) or math.isnan(b)) else float('nan')
            mwn[i + 1] = d
    else:
        mwn = mwn_raw
    hdre = mwe_rows[0]
    mwe = {}
    for r in mwe_rows[1:]:
        mwe[(int(r[0]), int(r[1]))] = dict(zip(hdre[2:], [fl(v) for v in r[2:]]))
    out = {'case': name, 'solver': tag, 'E': E, 'nu': nu, 'p': p, 't': t, 'nodes_structura': len(sd), 'nodes_mecway': len(mwn)}
    area = 0.0
    for q in quads:
        pts = [nodes[int(v)] for v in q[1:5]]
        # area do quadrilatero (duas triangulacoes em 3D)
        def tri(a, b, c):
            ux, uy, uz = [b[i] - a[i] for i in range(3)]
            vx, vy, vz = [c[i] - a[i] for i in range(3)]
            return 0.5 * math.sqrt((uy * vz - uz * vy) ** 2 + (uz * vx - ux * vz) ** 2 + (ux * vy - uy * vx) ** 2)
        area += tri(pts[0], pts[1], pts[2]) + tri(pts[0], pts[2], pts[3])
    out['area'] = area
    # ---- deslocamentos e rotacoes (no a no): Mecway usa nid = indice + 1; no CalculiX a solucao pode ter nos extras (ignorados)
    ids = sorted(sd.keys())
    comp = {}
    for label, sk, mk in [('ux', 0, 'displx'), ('uy', 1, 'disply'), ('uz', 2, 'displz'), ('rx', 3, 'rotx'), ('ry', 4, 'roty'), ('rz', 5, 'rotz')]:
        ref = [mwn[i + 1][mk] if (i + 1) in mwn else float('nan') for i in ids]
        mine = [sd[i][sk] for i in ids]
        comp[label] = stats([a - b for a, b in zip(mine, ref)], ref)
    out['disp'] = comp
    # deslocamento maximo (norma da translacao)
    def mag(v):
        return math.sqrt(sum(x * x for x in v[:3]))
    s_mag = [mag(sd[i]) for i in ids]
    m_mag = [math.sqrt(sum(mwn[i + 1][k] ** 2 for k in ('displx', 'disply', 'displz'))) if (i + 1) in mwn else float('nan') for i in ids]
    out['max_translation'] = {'structura': max(s_mag), 'mecway': max(x for x in m_mag if not math.isnan(x))}
    out['uz_extreme'] = {'structura': max(sd[i][2] for i in ids) if abs(max(sd[i][2] for i in ids)) >= abs(min(sd[i][2] for i in ids)) else min(sd[i][2] for i in ids)}
    mz = [mwn[i + 1]['displz'] for i in ids if (i + 1) in mwn]
    out['uz_extreme']['mecway'] = max(mz) if abs(max(mz)) >= abs(min(mz)) else min(mz)
    # ---- reacoes
    def total(react, k):
        return sum(v[k] for v in react.values())
    mr = {nid: [d.get(c, float('nan')) for c in ('reactionforcex', 'reactionforcey', 'reactionforcez')] for nid, d in mwn.items()}
    out['reaction'] = {'applied_Fz': p * area,
                       'structura_sum': [total(sr, k) for k in range(3)],
                       'mecway_sum': [sum(v[k] for v in mr.values() if not math.isnan(v[k])) for k in range(3)]}
    # ---- por elemento: von Mises na face inferior e momentos principais (invariantes: eixos locais diferem)
    vm_s, vm_m, mp_s_max, mp_m_max, mp_s_min, mp_m_min = [], [], [], [], [], []
    mu_signflip = 0
    for e in range(len(quads)):
        for k in range(4):
            s = se.get((e, k))
            w = mwe.get((e + 1, k + 1))
            if s is None or w is None:
                continue
            mxx, myy, mxy, nxx, nyy, nxy, vt, vb = s[:8]
            vm_s.append(vb)
            vm_m.append(w['vonmisesb'])
            smax, smin = principal(mxx, myy, mxy)
            mmax, mmin = principal(w['momentu'], w['momentv'], w['momentuv'])
            mp_s_max.append(smax)
            mp_s_min.append(smin)
            mp_m_max.append(mmax)
            mp_m_min.append(mmin)
    out['vonmises_bottom_corner'] = stats([a - b for a, b in zip(vm_s, vm_m)], vm_m)
    out['moment_principal_max'] = stats([a - b for a, b in zip(mp_s_max, mp_m_max)], mp_m_max)
    out['moment_principal_min'] = stats([a - b for a, b in zip(mp_s_min, mp_m_min)], mp_m_min)
    # sinal: Structura (m>0 traciona a face +normal) x Mecway: se os momentos principais tiverem sinal oposto, a razao media e negativa
    num = sum(a * b for a, b in zip(mp_s_max, mp_m_max))
    den = sum(b * b for b in mp_m_max)
    out['moment_sign_ratio_structura_over_mecway'] = num / den if den else float('nan')
    # centro do elemento: media dos 4 cantos (Mecway) x valor no centro (Structura)
    cvm_s, cvm_m = [], []
    for e in range(len(quads)):
        ws = [mwe.get((e + 1, k + 1)) for k in range(4)]
        if any(w is None for w in ws) or (e, 4) not in se:
            continue
        cvm_s.append(se[(e, 4)][7])
        cvm_m.append(sum(w['vonmisesb'] for w in ws) / 4.0)
    out['vonmises_bottom_center'] = stats([a - b for a, b in zip(cvm_s, cvm_m)], cvm_m)
    # ---- referencias analiticas
    D = E * t ** 3 / (12 * (1 - nu * nu))
    ref = {}
    if name == 'ss_plate':
        a = 1.0
        ref['uz_center_navier'] = navier_ss(a, p, D)
        ref['sigma_max_navier_note'] = 'sigma = 6 M/t^2 com M_max = 0,0479 q a^2 (Timoshenko, nu=0,3)'
        ref['vonmises_center_expected'] = 6 * 0.0479 * p * a * a / t ** 2
    elif name == 'clamped_plate':
        ref['uz_center_timoshenko'] = 0.00126 * p * 1.0 / D
    elif name == 'cantilever_plate':
        L = 4.0
        I = t ** 3 / 12.0
        ref['uz_tip_euler_bernoulli'] = p * 1.0 * L ** 4 / (8 * E * I)
        ref['uz_tip_with_shear'] = ref['uz_tip_euler_bernoulli'] + p * L * L / (2 * (E / (2 * (1 + nu))) * t * 5.0 / 6.0)
    elif name.startswith('cylinder_pressure'):
        R = 1.0
        ref['ur_membrane'] = p * R * R / (E * t)
        # deslocamento radial medio na metade superior (longe do engaste): comparacao com a teoria de membrana
        rs, rm = [], []
        for i in ids:
            x, y, z = nodes[i]
            if z > 1.4:
                r = math.hypot(x, y)
                ur_s = (sd[i][0] * x + sd[i][1] * y) / r
                rs.append(ur_s)
                if (i + 1) in mwn:
                    rm.append((mwn[i + 1]['displx'] * x + mwn[i + 1]['disply'] * y) / r)
        ref['ur_upper_structura_mean'] = sum(rs) / len(rs)
        ref['ur_upper_mecway_mean'] = sum(rm) / len(rm) if rm else float('nan')
        # momento de engaste (camada limite): M0 = p R t / (2 sqrt(3 (1 - nu^2))); maior |momento principal| dos elementos da primeira fileira
        ref['M_base_analytic'] = p * R * t / (2.0 * math.sqrt(3.0 * (1.0 - nu * nu)))
        base_s, base_m = 0.0, 0.0
        for e, q in enumerate(quads):
            zs = [nodes[int(v)][2] for v in q[1:5]]
            if min(zs) > 1e-9:
                continue
            for k in range(4):
                s_ = se.get((e, k))
                w_ = mwe.get((e + 1, k + 1))
                if s_ is None or w_ is None:
                    continue
                a, b = principal(s_[0], s_[1], s_[2])
                base_s = max(base_s, abs(a), abs(b))
                a, b = principal(w_['momentu'], w_['momentv'], w_['momentuv'])
                base_m = max(base_m, abs(a), abs(b))
        ref['M_base_structura_max'] = base_s
        ref['M_base_mecway_max'] = base_m
    out['reference'] = ref
    return out


if __name__ == '__main__':
    cases_dir, mw_dir, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
    result = []
    for name in sorted(os.listdir(cases_dir)):
        d = os.path.join(cases_dir, name)
        if not os.path.isdir(d):
            continue
        for tag in (name, name + '_ccx'):
            try:
                result.append(compare_case(d, mw_dir, name, tag))
            except Exception as e:  # noqa
                result.append({'case': name, 'solver': tag, 'error': repr(e)})
    json.dump(result, open(out_path, 'w'), indent=1)
    print('ok', len(result))
