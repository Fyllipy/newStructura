# Scordelis-Lo: Structura x Mecway 28 (no a no) e flecha no meio da borda livre x referencia da literatura (0,3024; valor de memoria, nao conferido).
# uso: python compare_sl.py <pasta_slcases> <pasta_slmw> <saida.json>
import csv
import json
import math
import os
import sys

REFERENCE = 0.3024


def read(path):
    with open(path, newline='') as f:
        return [r for r in csv.reader(f) if r]


def main(cases, mwdir, outjson):
    result = {}
    prev = None
    for n in (4, 8, 16, 32):
        cdir = os.path.join(cases, 'scordelis_%d' % n)
        nodes = [[float(v) for v in r[1:4]] for r in read(os.path.join(cdir, 'nodes.csv'))]
        disp = [[float(v) for v in r[1:7]] for r in read(os.path.join(cdir, 's_disp.csv'))]
        mw = {}
        for r in read(os.path.join(mwdir, 'scordelis_%d_mw_nodes.csv' % n))[1:]:
            mw[(round(float(r[1]) * 1e6), round(float(r[2]) * 1e6), round(float(r[3]) * 1e6))] = [float(v) if v != '' else float('nan') for v in r[4:10]]
        a0 = math.radians(50.0)
        target = (25.0, 25.0 * math.cos(a0), 25.0 * math.sin(a0))
        rel = {}
        worst = [0.0] * 6
        peak = [0.0] * 6
        for i, p in enumerate(nodes):
            k = (round(p[0] * 1e6), round(p[1] * 1e6), round(p[2] * 1e6))
            if k not in mw:
                continue
            for c in range(6):
                worst[c] = max(worst[c], abs(disp[i][c] - mw[k][c]))
                peak[c] = max(peak[c], abs(mw[k][c]))
            if max(abs(p[j] - target[j]) for j in range(3)) < 1e-6:
                rel['structura'] = disp[i][2]
                rel['mecway'] = mw[k][2]
        entry = {'nodes': len(nodes), 'uz_structura': -rel['structura'], 'uz_mecway': -rel['mecway'],
                 'dif_structura_mecway_rel': abs(rel['structura'] - rel['mecway']) / abs(rel['mecway']),
                 'erro_vs_referencia': (-rel['structura'] - REFERENCE) / REFERENCE,
                 'max_dif_nodal_rel_ux_uy_uz': [worst[c] / peak[c] if peak[c] > 0 else float('nan') for c in range(3)]}
        if prev:
            entry['razao_ao_nivel_anterior'] = (prev - entry['uz_structura']) / 1.0
        prev = entry['uz_structura']
        result['scordelis_%d' % n] = entry
        print('malha %2d: %4d nos | uz Structura %.6f  Mecway %.6f  (dif %.1e) | vs 0,3024: %+.2f%% | dif nodal max (rel.) ux %.1e uy %.1e uz %.1e'
              % (n, entry['nodes'], entry['uz_structura'], entry['uz_mecway'], entry['dif_structura_mecway_rel'], 100 * entry['erro_vs_referencia'], *entry['max_dif_nodal_rel_ux_uy_uz']))
    # extrapolacao de Richardson com as tres malhas mais finas (taxa observada)
    u = [result['scordelis_%d' % n]['uz_structura'] for n in (8, 16, 32)]
    ratio = (u[0] - u[1]) / (u[1] - u[2])
    p = math.log(ratio) / math.log(2.0)
    extrap = u[2] - (u[1] - u[2]) / (ratio - 1.0)
    result['richardson'] = {'taxa_observada': p, 'valor_extrapolado': extrap}
    print('Richardson (8,16,32): taxa observada %.2f, valor extrapolado %.5f' % (p, extrap))
    json.dump(result, open(outjson, 'w'), indent=1)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3])
