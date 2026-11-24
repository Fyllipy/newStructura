# Validacao dos quinhoes de carga (modo T): carga que a laje aplica em cada barra x envoltoria analitica (regra dos 45 graus: cada ponto
# descarrega na aresta apoiada mais proxima), calculada aqui de forma independente do codigo do Structura.
# uso: python check_quinhoes.py <pasta_lvcases> <saida.json>
import csv
import json
import math
import os
import sys

Q = 5000.0   # Pa (para baixo)

# geometria de cada caso: (x_max, y_max, arestas apoiadas, tipo de carga)
CASES = {
    'LV1_laje_4vigas_4pilares': (6.0, 4.0, 'all', 'area'),
    'LV2_duas_baias_viga_central': (8.0, 4.0, 'all', 'area'),   # viga central NAO esta numa aresta da chapa unica: nao recebe carga
    'LV3_laje_unidirecional_2vigas': (6.0, 4.0, 'y', 'area'),
    'LV5_carga_por_clique': (6.0, 4.0, 'all', 'nodal'),
    'LV6_vigas_sem_dividir': (6.0, 4.0, 'all', 'area'),
}


def read(path):
    if not os.path.exists(path):
        return []
    with open(path, newline='') as f:
        return [r for r in csv.reader(f) if r]


def depth(bx, ay, supported, axis, s):
    """profundidade do quinhao (m) no ponto s ao longo da aresta (axis 'x': arestas y=0/ay; 'y': arestas x=0/bx)."""
    if supported == 'y':
        return ay / 2.0 if axis == 'x' else 0.0
    if axis == 'x':
        return min(s, bx - s, ay / 2.0)
    return min(s, ay - s, ay / 2.0)


def mean_depth(bx, ay, supported, axis, s0, s1, n=2000):
    tot = 0.0
    for i in range(n):
        tot += depth(bx, ay, supported, axis, s0 + (s1 - s0) * (i + 0.5) / n)
    return tot / n


def main(lvcases, outjson):
    result = {}
    for case, (bx, ay, supported, kind) in CASES.items():
        cdir = os.path.join(lvcases, case)
        bars = [[float(v) for v in r] for r in read(os.path.join(cdir, 'bars.csv'))]
        mdir = os.path.join(cdir, 'modo_T')
        loads = {int(r[0]): [float(v) for v in r[1:4]] for r in read(os.path.join(mdir, 'bar_loads.csv'))}
        nodal = [[float(v) for v in r[1:7]] for r in read(os.path.join(mdir, 'nodal_loads.csv'))]
        rows = []
        for b in bars:
            if b[7] == 1:
                continue
            a, c = b[1:4], b[4:7]
            along_x = abs(c[0] - a[0]) > 1e-9
            if along_x:
                on_edge = (abs(a[1]) < 1e-9 or abs(a[1] - ay) < 1e-9) and abs(c[1] - a[1]) < 1e-9
                s0, s1 = sorted((a[0], c[0]))
                axis = 'x'
            else:
                on_edge = (abs(a[0]) < 1e-9 or abs(a[0] - bx) < 1e-9) and abs(c[0] - a[0]) < 1e-9
                s0, s1 = sorted((a[1], c[1]))
                axis = 'y'
            expected = -Q * mean_depth(bx, ay, supported, axis, s0, s1) if on_edge else 0.0
            L = s1 - s0
            got = loads.get(int(b[0]), [0, 0, 0])[2]
            rows.append({'bar': int(b[0]), 'axis': axis, 's0': s0, 's1': s1, 'L': L, 'q_expected': expected, 'q_structura': got})
        peak = max(abs(r['q_expected']) for r in rows) or 1.0
        res = {'bars': len(rows), 'case_kind': kind}
        if kind == 'area':
            res['max_err_rel_peak'] = max(abs(r['q_structura'] - r['q_expected']) for r in rows) / peak
            res['total_expected'] = sum(r['q_expected'] * r['L'] for r in rows)
            res['total_structura'] = sum(r['q_structura'] * r['L'] for r in rows)
            res['rows'] = rows
        else:
            # forcas nodais por clique: total por viga (linha) x total analitico dessa viga
            groups = {}
            for r in rows:
                key = (r['axis'], round(0.0, 3))
                pass
            tot_nodal = sum(n[5] for n in nodal)
            res['total_nodal_Fz'] = tot_nodal
            res['total_expected'] = sum(r['q_expected'] * r['L'] for r in rows)
            # por viga de aresta (linha completa): soma das forcas nodais nos nos da linha
            lines = {}
            for b in bars:
                if b[7] == 1:
                    continue
                a, c = b[1:4], b[4:7]
                if abs(c[0] - a[0]) > 1e-9:
                    lines.setdefault(('x', round(a[1], 6)), []).append((min(a[0], c[0]), max(a[0], c[0]), b))
                else:
                    lines.setdefault(('y', round(a[0], 6)), []).append((min(a[1], c[1]), max(a[1], c[1]), b))
            per_line = []
            for (axis, coord), els in lines.items():
                lo = min(e[0] for e in els)
                hi = max(e[1] for e in els)
                ids = {int(e[2][0]) for e in els}
                exp_total = sum(r['q_expected'] * r['L'] for r in rows if r['bar'] in ids)
                got_total = 0.0
                for n in nodal:
                    px, py = n[0], n[1]
                    if axis == 'x' and abs(py - coord) < 1e-9 and lo - 1e-9 <= px <= hi + 1e-9:
                        got_total += n[5]
                    if axis == 'y' and abs(px - coord) < 1e-9 and lo - 1e-9 <= py <= hi + 1e-9:
                        got_total += n[5]
                per_line.append({'axis': axis, 'coord': coord, 'F_expected': exp_total, 'F_structura': got_total})
            res['lines'] = per_line
        result[case] = res
        print('==', case)
        if kind == 'area':
            print('   %d barras, carga distribuida por barra: erro max %.2e do pico (%.1f N/m); total aplicado %.1f N (analitico %.1f N)' % (res['bars'], res['max_err_rel_peak'], peak, -res['total_structura'], -res['total_expected']))
        else:
            print('   forcas nodais por clique: soma %.1f N (carga total %.1f N)' % (-res['total_nodal_Fz'], -res['total_expected']))
            for l in res['lines']:
                print('      viga %s=%g: forca nas barras %.1f N, analitica %.1f N (%.1f%%)' % ('y' if l['axis'] == 'x' else 'x', l['coord'], -l['F_structura'], -l['F_expected'], 100.0 * (l['F_structura'] / l['F_expected'] - 1.0) if l['F_expected'] else 0.0))
    json.dump(result, open(outjson, 'w'), indent=1)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
