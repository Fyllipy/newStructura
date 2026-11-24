# Compara, no. a no. (por coordenada), o modo M (monolitico) do Structura com o Mecway 28 para cada caso laje+vigas+pilares.
# uso: python compare_lv.py <pasta_lvcases> <pasta_lvmw> <saida.json> [modo=M]
import csv
import json
import math
import os
import sys


def read(path):
    with open(path, newline='') as f:
        return [r for r in csv.reader(f) if r]


def key(x, y, z):
    return (round(float(x) * 1e6), round(float(y) * 1e6), round(float(z) * 1e6))


def fl(s):
    return float(s) if s not in ('', None) else float('nan')


def compare(cdir, mwcsv, mode):
    st = {}
    for r in read(os.path.join(cdir, 'modo_' + mode, 's_nodes.csv')):
        st[key(r[1], r[2], r[3])] = [float(v) for v in r[4:16]]   # u(6), R(6)
    mw = {}
    rows = read(mwcsv)[1:]
    for r in rows:
        mw[key(r[1], r[2], r[3])] = [fl(v) for v in r[4:16]]     # u(6), rot ja inclusos: displ 3 + rot 3, reacao 3 + momento 3
    out = {'n_nodes_structura': len(st), 'n_nodes_mecway': len(mw), 'missing': len(set(st) ^ set(mw))}
    names = ['ux', 'uy', 'uz', 'rx', 'ry', 'rz']
    for i, nm in enumerate(names):
        pairs = [(st[k][i], mw[k][i]) for k in st if k in mw and not math.isnan(mw[k][i])]
        m = max(abs(b) for _, b in pairs) if pairs else 0.0
        d = max(abs(a - b) for a, b in pairs) if pairs else float('nan')
        out[nm] = {'max_ref': m, 'max_abs_diff': d, 'rel_max': d / m if m > 0 else float('nan')}
    # reacoes nos apoios (nos com reacao em qualquer das duas listas)
    rpairs = []
    for k in st:
        if k not in mw:
            continue
        if any(abs(v) > 0 for v in st[k][6:12]) or not any(math.isnan(v) for v in mw[k][6:12]):
            if not any(math.isnan(v) for v in mw[k][6:12]):
                rpairs.append((k, st[k][6:12], mw[k][6:12]))
    comp = ['Fx', 'Fy', 'Fz', 'Mx', 'My', 'Mz']
    out['reactions'] = {}
    for i, nm in enumerate(comp):
        m = max((abs(b[i]) for _, _, b in rpairs), default=0.0)
        d = max((abs(a[i] - b[i]) for _, a, b in rpairs), default=float('nan'))
        out['reactions'][nm] = {'max_ref': m, 'max_abs_diff': d, 'rel_max': d / m if m > 0 else float('nan')}
    out['supports'] = [{'x': k[0] / 1e6, 'y': k[1] / 1e6, 'structura_Fz': a[2], 'mecway_Fz': b[2], 'structura_My': a[4], 'mecway_My': b[4]} for k, a, b in rpairs]
    return out


def main(lvcases, lvmw, outjson, mode):
    result = {}
    for case in sorted(os.listdir(lvcases)):
        cdir = os.path.join(lvcases, case)
        mwcsv = os.path.join(lvmw, case + '_mw_nodes.csv')
        if not os.path.isdir(cdir) or not os.path.exists(mwcsv):
            continue
        result[case] = compare(cdir, mwcsv, mode)
        r = result[case]
        print('==', case, '(modo %s)  nos: %d/%d, sem par: %d' % (mode, r['n_nodes_structura'], r['n_nodes_mecway'], r['missing']))
        for nm in ['ux', 'uy', 'uz', 'rx', 'ry', 'rz']:
            print('   %-3s max ref %.4e  dif max %.3e  rel %.3e' % (nm, r[nm]['max_ref'], r[nm]['max_abs_diff'], r[nm]['rel_max']))
        for nm in ['Fz', 'Mx', 'My']:
            x = r['reactions'][nm]
            print('   R%-2s max ref %.4e  dif max %.3e  rel %.3e' % (nm, x['max_ref'], x['max_abs_diff'], x['rel_max']))
    json.dump(result, open(outjson, 'w'), indent=1)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4] if len(sys.argv) > 4 else 'M')
