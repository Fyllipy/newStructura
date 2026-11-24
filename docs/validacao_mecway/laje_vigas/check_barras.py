# Validacao do acoplamento laje (casca/MEF) x barras (rigidez direta):
#  1. nos que a laje cria nas barras (cortes) e apoios (ligacoes casca-barra);
#  2. esforcos das barras (momento medio por elemento, no meio de cada viga) Structura x Mecway;
#  3. reacoes dos apoios (bases dos pilares).
# uso: python check_barras.py <pasta_lvcases> <pasta_lvmw> <saida.json>
import csv
import json
import math
import os
import sys

E = 30.0e9
SIDE = 0.30
EI = E * SIDE ** 4 / 12.0


def read(path):
    if not os.path.exists(path):
        return []
    with open(path, newline='') as f:
        return [r for r in csv.reader(f) if r]


def key(x, y, z):
    return (round(float(x) * 1e6), round(float(y) * 1e6), round(float(z) * 1e6))


def fl(s):
    return float(s) if s not in ('', None) else float('nan')


def kv(path):
    d = {}
    if os.path.exists(path):
        for line in open(path, encoding='utf-8'):
            if '=' in line:
                a, b = line.rstrip('\n').split('=', 1)
                d[a] = b
    return d


def check_nodes(cdir):
    """1. cortes esperados (independentes) x cortes do servico; 2. toda aresta de malha sobre barra tem no de barra e ligacao."""
    mesh = [(float(r[1]), float(r[2]), float(r[3])) for r in read(os.path.join(cdir, 'nodes.csv'))]
    unsplit = [[float(v) for v in r] for r in read(os.path.join(cdir, 'bars_unsplit.csv'))]
    expected = set()
    for b in unsplit:
        if b[7] == 1:   # pilar: vertical, nao recebe nos da laje
            continue
        a = b[1:4]
        c = b[4:7]
        d = [c[i] - a[i] for i in range(3)]
        L2 = sum(v * v for v in d)
        for p in mesh:
            ap = [p[i] - a[i] for i in range(3)]
            t = sum(ap[i] * d[i] for i in range(3)) / L2
            dist = math.sqrt(sum((ap[i] - t * d[i]) ** 2 for i in range(3)))
            if dist < 1e-6 and 1e-6 < t * math.sqrt(L2) and (1 - t) * math.sqrt(L2) > 1e-6:
                expected.add(key(*p))
    cuts = {key(r[2], r[3], r[4]) for r in read(os.path.join(cdir, 'cuts.csv'))}
    out = {'cuts_expected': len(expected), 'cuts_service': len(cuts), 'cuts_equal': expected == cuts}
    # com as barras divididas: toda no da malha sobre eixo de barra (horizontal) coincide com no de barra
    split = [[float(v) for v in r] for r in read(os.path.join(cdir, 'bars.csv'))]
    bar_nodes = set()
    for b in split:
        bar_nodes.add(key(*b[1:4]))
        bar_nodes.add(key(*b[4:7]))
    on_axis = set()
    for b in split:
        if b[7] == 1:
            continue
        a, c = b[1:4], b[4:7]
        d = [c[i] - a[i] for i in range(3)]
        L2 = sum(v * v for v in d)
        for p in mesh:
            ap = [p[i] - a[i] for i in range(3)]
            t = sum(ap[i] * d[i] for i in range(3)) / L2
            if -1e-9 <= t <= 1 + 1e-9 and math.sqrt(sum((ap[i] - t * d[i]) ** 2 for i in range(3))) < 1e-6:
                on_axis.add(key(*p))
    out['mesh_nodes_on_bars'] = len(on_axis)
    out['all_have_bar_node'] = on_axis <= bar_nodes
    for mode in 'AM':
        mp = kv(os.path.join(cdir, 'modo_' + mode, 'mapping.txt'))
        res = {'merged': int(mp.get('merged_nodes', -1)), 'interior_shell_nodes': int(mp.get('interior_shell_nodes', -1))}
        ties = read(os.path.join(cdir, 'modo_' + mode, 'ties.csv'))
        good = 0
        for r in ties:
            if int(r[4]) == 1 and key(*r[0:3]) == key(*r[5:8]) and int(r[3]) == 7:
                good += 1
        res['ties_translation_only_same_point'] = good
        res['ties_total'] = len(ties)
        out['modo_' + mode] = res
    return out


def element_means(cdir, mode, nodes_csv, mwnodes):
    """momento medio por elemento de viga: Structura (esforcos do solver de barras) e Mecway (EI x variacao da rotacao / L)."""
    bars = [[float(v) for v in r] for r in read(os.path.join(cdir, 'bars.csv'))]
    forces = {int(r[0]): [float(v) for v in r[1:13]] for r in read(os.path.join(cdir, 'modo_' + mode, 's_bars.csv'))}
    st = {key(r[1], r[2], r[3]): [float(v) for v in r[4:16]] for r in read(nodes_csv)}
    mw = {key(r[1], r[2], r[3]): [fl(v) for v in r[4:16]] for r in read(mwnodes)[1:]}
    rows = []
    for b in bars:
        if b[7] == 1:
            continue
        a, c = b[1:4], b[4:7]
        L = math.sqrt(sum((c[i] - a[i]) ** 2 for i in range(3)))
        along_x = abs(c[0] - a[0]) > 1e-9
        axis = 4 if along_x else 3   # rotacao em torno de y (viga em x) ou de x (viga em y)
        ka, kc = key(*a), key(*c)
        m_mw = EI * abs(mw[kc][axis] - mw[ka][axis]) / L
        m_st_rot = EI * abs(st[kc][axis] - st[ka][axis]) / L
        f = forces[int(b[0])]
        best = None
        for comp in (4, 5):   # My, Mz (i) e (j = +6)
            mi, mj = f[comp], f[comp + 6]
            for s in (-1.0, 1.0):
                v = abs((s * mi + mj) / 2.0)
                cand = (abs(v - m_st_rot), v)
                if best is None or cand[0] < best[0]:
                    best = cand
        rows.append({'bar': int(b[0]), 'x0': a[0], 'y0': a[1], 'x1': c[0], 'y1': c[1], 'M_structura_forcas': best[1], 'M_structura_rot': m_st_rot, 'M_mecway': m_mw})
    return rows


def main(lvcases, lvmw, outjson):
    result = {}
    for case in sorted(os.listdir(lvcases)):
        cdir = os.path.join(lvcases, case)
        mwnodes = os.path.join(lvmw, case + '_mw_nodes.csv')
        if not os.path.isdir(cdir):
            continue
        r = {'nos_e_apoios': check_nodes(cdir)}
        if os.path.exists(mwnodes):
            rows = element_means(cdir, 'M', os.path.join(cdir, 'modo_M', 's_nodes.csv'), mwnodes)
            if not rows:
                result[case] = r
                print('==', case, '(sem vigas)')
                continue
            mx = max(abs(x['M_mecway']) for x in rows) or 1.0
            r['beam_elements'] = len(rows)
            r['max_M'] = mx
            r['max_err_forces_vs_mecway'] = max(abs(x['M_structura_forcas'] - x['M_mecway']) for x in rows) / mx
            r['max_err_rot_vs_mecway'] = max(abs(x['M_structura_rot'] - x['M_mecway']) for x in rows) / mx
            # elementos do meio de cada viga (o de maior |M| por linha de viga)
            lines = {}
            for x in rows:
                k = (round(x['y0'], 6) if abs(x['x1'] - x['x0']) > 1e-9 else round(x['x0'], 6), abs(x['x1'] - x['x0']) > 1e-9)
                lines.setdefault(k, []).append(x)
            mids = []
            for k, els in lines.items():
                els.sort(key=lambda e: (e['x0'] + e['x1'] + e['y0'] + e['y1']))
                mid = els[len(els) // 2]
                mids.append({'line': {'along_x': k[1], 'coord': k[0]}, 'bar': mid['bar'], 'M_structura': mid['M_structura_forcas'], 'M_mecway': mid['M_mecway']})
            r['midspan'] = mids
            r['rows'] = rows
        result[case] = r
        n = r['nos_e_apoios']
        print('==', case)
        print('   nos: cortes esperados %d / servico %d (%s); nos de malha sobre barras %d, todos com no de barra: %s' % (n['cuts_expected'], n['cuts_service'], 'IGUAIS' if n['cuts_equal'] else 'DIFERENTES', n['mesh_nodes_on_bars'], n['all_have_bar_node']))
        for mode in 'AM':
            m = n['modo_' + mode]
            print('   modo %s: nos fundidos %d, nos de casca sobre barra sem divisao %d, ligacoes (translacao, mesmo ponto) %d de %d' % (mode, m['merged'], m['interior_shell_nodes'], m['ties_translation_only_same_point'], m['ties_total']))
        if 'beam_elements' in r:
            print('   esforcos (modo M): %d elementos; erro max do momento medio: forcas Structura vs Mecway %.2e, rotacoes Structura vs Mecway %.2e (rel. ao maior M = %.1f N.m)' % (r['beam_elements'], r['max_err_forces_vs_mecway'], r['max_err_rot_vs_mecway'], r['max_M']))
            for m in r['midspan']:
                print('      viga %s=%g, barra do meio %d: M Structura %.1f  Mecway %.1f N.m' % ('y' if m['line']['along_x'] else 'x', m['line']['coord'], m['bar'], m['M_structura'], m['M_mecway']))
    json.dump(result, open(outjson, 'w'), indent=1)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3])
