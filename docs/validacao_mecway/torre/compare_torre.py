# Comparacao Structura x Mecway da torre: estatica (deslocamentos e reacoes), flambagem (fatores) e vibracao (frequencias), alem das formas modais (MAC).
# Converte as saidas do Mecway para o formato do Structura (id,x,y,z,ux,...) para o renderizador dos prints.
# uso: python compare_torre.py <pasta_torre(geom.csv)> <pasta_structura> <pasta_mecway> <saida.json>
import csv
import json
import math
import os
import sys

geom, st, mw, out_json = sys.argv[1:5]
nodes = {}
for r in csv.reader(open(os.path.join(geom, 'torre_geom.csv'))):
    if r and r[0] == 'N':
        nodes[int(r[1])] = tuple(float(v) for v in r[2:5])


def read_kv(path):
    d = {}
    for line in open(path):
        if '=' in line:
            k, v = line.strip().split('=', 1)
            d[k] = float(v)
    return d


def read_mw_nodes(path):
    d = {}
    for r in csv.reader(open(path)):
        if r:
            d[int(r[0])] = [float(v) for v in r[1:10]]
    return d


def read_st_nodes(path):
    d = {}
    for r in csv.reader(open(path)):
        if r:
            d[int(r[0])] = [float(v) for v in r[4:]]
    return d


def to_structura_format(mwdict, path):
    with open(path, 'w', newline='') as f:
        w = csv.writer(f)
        for n, v in sorted(mwdict.items()):
            p = nodes[n]
            w.writerow([n, p[0], p[1], p[2]] + v[:6])


def mac(a, b):
    num = sum(x * y for x, y in zip(a, b)) ** 2
    den = sum(x * x for x in a) * sum(y * y for y in b)
    return num / den if den > 0 else 0.0


result = {}
# ---- estatica ----
s = read_st_nodes(os.path.join(st, 'static_nodes.csv'))   # u(6), reacao(6)
m = read_mw_nodes(os.path.join(mw, 'torre_estatica_mw_mode0.csv'))
to_structura_format(m, os.path.join(mw, 'static_mw_formatted.csv'))
top = max(nodes, key=lambda n: (nodes[n][2], nodes[n][0], nodes[n][1]))
umax_s = max(math.sqrt(sum(v * v for v in s[n][:3])) for n in s)
umax_m = max(math.sqrt(sum(v * v for v in m[n][:3])) for n in m)
diffs = [math.sqrt(sum((s[n][k] - m[n][k]) ** 2 for k in range(3))) for n in s if n in m]
result['estatica'] = {
    'no_do_topo': top, 'u_topo_structura': s[top][:3], 'u_topo_mecway': m[top][:3], 'u_max_structura': umax_s, 'u_max_mecway': umax_m,
    'dif_max_nodal_relativa': max(diffs) / umax_m,
    'reacao_total_structura': [sum(s[n][6 + k] for n in s) for k in range(3)],
    'reacao_total_mecway': [sum(m[n][6 + k] for n in m if n in nodes and nodes[n][2] < 1e-9) for k in range(3)],
}
# ---- flambagem e modal ----
for key, st_file, mw_result, mw_prefix, label in (('flambagem', 'buckling.csv', 'torre_flambagem_mw_result.txt', 'torre_flambagem_mw_mode', 'fator'), ('modal', 'modal.csv', 'torre_modal_mw_result.txt', 'torre_modal_mw_mode', 'f')):
    sv = read_kv(os.path.join(st, st_file))
    mv = read_kv(os.path.join(mw, mw_result))
    rows = []
    for k in range(1, 7):
        a, b = sv.get('%s%d' % (label, k)), mv.get('%s%d' % (label, k))
        if a is None or b is None:
            continue
        shape_s = read_st_nodes(os.path.join(st, '%s_mode%d.csv' % ('buckling' if key == 'flambagem' else 'modal', k)))
        shape_m = read_mw_nodes(os.path.join(mw, '%s%d.csv' % (mw_prefix, k)))
        ids = [n for n in shape_s if n in shape_m]
        vs = [c for n in ids for c in shape_s[n][:3]]
        vm = [c for n in ids for c in shape_m[n][:3]]
        rows.append({'modo': k, 'structura': a, 'mecway': b, 'dif_relativa': (a - b) / b, 'MAC': mac(vs, vm)})
        to_structura_format(shape_m, os.path.join(mw, '%s_mw_formatted%d.csv' % (key, k)))
    result[key] = rows
json.dump(result, open(out_json, 'w'), indent=1)
e = result['estatica']
print('ESTATICA no do topo %d: u Structura %s, Mecway %s; max %.5g x %.5g; dif nodal max %.2e do deslocamento maximo' % (top, ['%.5g' % v for v in e['u_topo_structura']], ['%.5g' % v for v in e['u_topo_mecway']], e['u_max_structura'], e['u_max_mecway'], e['dif_max_nodal_relativa']))
print('  reacao total Structura', ['%.5g' % v for v in e['reacao_total_structura']], 'Mecway', ['%.5g' % v for v in e['reacao_total_mecway']])
for key in ('flambagem', 'modal'):
    print(key.upper())
    for r in result[key]:
        print('  modo %d: Structura %.6g  Mecway %.6g  dif %+.2f%%  MAC %.4f' % (r['modo'], r['structura'], r['mecway'], 100 * r['dif_relativa'], r['MAC']))
