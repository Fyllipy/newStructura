# Validacao ANALITICA dos esforcos das barras num sistema laje (placa, MEF) + vigas + pilares (barras, rigidez direta).
# Entrada: saida do lv_dump (pasta com LV*/modo_{T,A,M}/s_bars.csv, bar_loads.csv, s_nodes.csv). Nao usa o Mecway.
# Referencias fechadas (Euler-Bernoulli, pequenos deslocamentos):
#   LV3  laje armada numa direcao sobre 2 vigas, cada viga e um portico plano de base engastada, carga uniforme w = q*ly/2:
#        deslocamento angular do no theta = (wL^2/12) / (EI (4/h + 2/L)) (sem translacao horizontal por simetria),
#        M_apoio = wL^2/12 - 2EI theta/L, M_meio = wL^2/8 - M_apoio, M_base = 2EI theta/h, M_topo(pilar) = 4EI theta/h,
#        H = (M_base + M_topo)/h (esforco normal da viga), N_pilar = wL/2, V_apoio = wL/2.
#   LV1  laje nos 2 sentidos, 4 vigas de borda: quinhoes a 45 graus (viga longa: trapezio, curta: triangulo);
#        por simetria V_apoio = (carga total da viga)/2 e N_pilar = q A / 4 (independe da rigidez), e o equilibrio do
#        meio vao fornece M_meio - M_apoio = V L/2 - (momento da carga no semi-vao).
#   LV4  laje lisa, 4 pilares: N_pilar = q A / 4.
# uso: python check_analitico.py <pasta_cases> <saida.json>
import csv
import json
import math
import os
import sys

E = 30.0e9
SIDE = 0.30
EI = E * SIDE ** 4 / 12.0
Q = 5000.0


def read(path):
    if not os.path.exists(path):
        return []
    with open(path, newline='') as f:
        return [r for r in csv.reader(f) if r]


def bars_of(cdir):
    rows = [[float(v) for v in r] for r in read(os.path.join(cdir, 'bars.csv'))]
    return {int(r[0]): {'a': r[1:4], 'b': r[4:7], 'col': r[7] == 1} for r in rows}


def forces_of(cdir, mode):
    return {int(r[0]): [float(v) for v in r[1:13]] for r in read(os.path.join(cdir, 'modo_' + mode, 's_bars.csv'))}


def close(p, q):
    return all(abs(p[i] - q[i]) < 1e-6 for i in range(3))


def line_elements(bars, y=None, x=None):
    """elementos de viga (horizontais) sobre y=cte (ao longo de x) ou x=cte (ao longo de y), ordenados."""
    out = []
    for i, b in bars.items():
        if b['col']:
            continue
        if y is not None and abs(b['a'][1] - y) < 1e-9 and abs(b['b'][1] - y) < 1e-9 and abs(b['a'][0] - b['b'][0]) > 1e-9:
            out.append((min(b['a'][0], b['b'][0]), i))
        if x is not None and abs(b['a'][0] - x) < 1e-9 and abs(b['b'][0] - x) < 1e-9 and abs(b['a'][1] - b['b'][1]) > 1e-9:
            out.append((min(b['a'][1], b['b'][1]), i))
    return [i for _, i in sorted(out)]


def column_at(bars, x, y):
    for i, b in bars.items():
        if b['col'] and abs(b['a'][0] - x) < 1e-9 and abs(b['a'][1] - y) < 1e-9:
            return i
    return None


def bend(f, end):
    """momento fletor dominante (My ou Mz) na extremidade (0 = i, 1 = j) e a componente usada."""
    base = 6 * end
    my, mz = f[base + 4], f[base + 5]
    return (my, 4) if abs(my) >= abs(mz) else (mz, 5)


def beam_profile(bars, forces, ids):
    """Diagrama de uma viga horizontal a partir dos esforcos de extremidade das barras, na ordem da cadeia (a conectividade do modelo).
    Flexao vertical = Mz local e cortante vertical = Vy local (convencao do solver para barras horizontais, conferida nos dados).
    Valor do diagrama na juncao de dois elementos = esforco do no inicial do seguinte; no fim da viga, o oposto do no final do ultimo."""
    comp, shear = 5, 1
    first, last = forces[ids[0]], forces[ids[-1]]
    n = len(ids)
    m_start = first[comp]
    m_end = -last[6 + comp]
    m_mid = forces[ids[n // 2]][comp]
    s = 1.0 if m_mid >= 0 else -1.0   # sagrante positivo
    return {'M_apoio_i': -s * m_start, 'M_apoio_j': -s * m_end, 'M_meio': abs(m_mid), 'V_i': abs(first[shear]), 'V_j': abs(last[6 + shear]), 'N_i': first[0], 'N_j': last[6]}


def chain(bars, ids):
    """ordena os elementos da viga pela cadeia de conectividade (coordenadas), no sentido da primeira barra do desenho."""
    ids = sorted(ids, key=lambda i: tuple(bars[i]['a']))
    start = None
    ends = {tuple(round(v, 6) for v in bars[i]['b']): i for i in ids}
    for i in ids:
        if tuple(round(v, 6) for v in bars[i]['a']) not in ends:
            start = i
    ordered = [start]
    by_a = {tuple(round(v, 6) for v in bars[i]['a']): i for i in ids}
    while True:
        nxt = by_a.get(tuple(round(v, 6) for v in bars[ordered[-1]]['b']))
        if nxt is None or nxt in ordered:
            break
        ordered.append(nxt)
    return ordered


def rel(a, b):
    return abs(a - b) / max(abs(b), 1e-9)


def portal(w, L, h):
    th = (w * L * L / 12.0) / (EI * (4.0 / h + 2.0 / L))
    m_end = w * L * L / 12.0 - 2.0 * EI * th / L
    m_mid = w * L * L / 8.0 - m_end
    m_base = 2.0 * EI * th / h
    m_top = 4.0 * EI * th / h
    return {'theta': th, 'M_apoio': m_end, 'M_meio': m_mid, 'M_base': m_base, 'M_topo': m_top, 'H': (m_base + m_top) / h, 'N_pilar': w * L / 2.0, 'V_apoio': w * L / 2.0}


def portal_points(points, L, h):
    """portico plano de base engastada com cargas PONTUAIS verticais P_k em x_k (carga simetrica): superposicao das formulas de engastamento perfeito
    (M_AB = P a b^2 / L^2) e do mesmo equilibrio do no (theta = FEM / (EI (4/h + 2/L))); sagrante no meio por equilibrio do semi-vao."""
    fem = sum(P * x * (L - x) ** 2 / (L * L) for x, P in points)
    th = fem / (EI * (4.0 / h + 2.0 / L))
    m_end = fem - 2.0 * EI * th / L
    total = sum(P for _, P in points)
    # momento de viga biapoiada no meio (carga simetrica): R*L/2 - sum P (L/2 - x) para x < L/2
    m0 = total / 2.0 * L / 2.0 - sum(P * (L / 2.0 - x) for x, P in points if x < L / 2.0 - 1e-9)
    m_base = 2.0 * EI * th / h
    m_top = 4.0 * EI * th / h
    return {'M_apoio': m_end, 'M_meio': m0 - m_end, 'M_base': m_base, 'M_topo': m_top, 'H': (m_base + m_top) / h, 'N_pilar': total / 2.0}


def lv3(cdir):
    bars = bars_of(cdir)
    L, h = 6.0, 3.0
    ref = portal(Q * 4.0 / 2.0, L, h)
    # cargas nodais equivalentes da laje (quinhao 10 kN/m por no de 1 m; os nos de ponta, sobre o pilar, vao direto ao apoio)
    ref_pts = portal_points([(float(k), 10000.0) for k in range(1, 6)], L, h)
    out = {'ref': ref, 'ref_nodal': ref_pts, 'modos': {}}
    for mode in 'TAM':
        f = forces_of(cdir, mode)
        rows = {}
        for yline in (0.0, 4.0):
            ids = chain(bars, line_elements(bars, y=yline))
            p = beam_profile(bars, f, ids)
            col = column_at(bars, 0.0, yline)
            fc = f[col]
            comp = 4 if abs(fc[4]) >= abs(fc[5]) else 5
            rows['viga_y%g' % yline] = {
                'M_apoio': 0.5 * (p['M_apoio_i'] + p['M_apoio_j']), 'M_meio': p['M_meio'], 'V_apoio': 0.5 * (p['V_i'] + p['V_j']),
                'H_viga_N': 0.5 * (abs(p['N_i']) + abs(p['N_j'])),
                'N_pilar': abs(fc[6]) if abs(fc[6]) > abs(fc[0]) * 0.5 else abs(fc[0]),
                'M_base': abs(fc[comp]), 'M_topo': abs(fc[6 + comp]),
            }
        out['modos'][mode] = rows
    return out


def trapezoid_total(L, peak, run):
    """carga total (N) de um trapezio simetrico: rampas de comprimento `run` nas pontas e patamar `peak`."""
    return peak * (L - run)


def lv1(cdir):
    bars = bars_of(cdir)
    out = {'modos': {}}
    ref = {'N_pilar': Q * 6 * 4 / 4.0, 'V_longa': 0.5 * (Q * 2.0) * (6.0 - 2.0), 'V_curta': 0.5 * (Q * 2.0 * 4.0 / 2.0)}
    # viga longa (6 m): trapezio de pico q*a/2 = q*2 m, rampas de 2 m: total = pico*(L - 2) = 10 kN/m * 4 = 40 kN; curta (4 m): triangulo de pico 10 kN/m: 20 kN
    q_peak = Q * 2.0
    tot_long, tot_short = q_peak * (6.0 - 2.0), q_peak * 4.0 / 2.0
    # M_meio - M_apoio por equilibrio do semi-vao: V L/2 - momento da carga em torno do meio
    # longa: trapezio: semi-vao 3 m, rampa de 2 m (x de 0 a 2) e patamar de 2 a 3 m; momento da carga em torno do meio (x = 3):
    #   rampa: carga 10*(x/2) kN/m em [0,2]: forca = 10 kN, centroide em 2*2/3 = 1.3333 m -> braco 3 - 1.3333 = 1.6667; patamar: 10 kN/m x 1 m = 10 kN, braco 0.5
    m_load_long = 10e3 * (3.0 - 4.0 / 3.0) + 10e3 * 0.5
    d_long = tot_long / 2.0 * 3.0 - m_load_long
    # curta: triangulo, semi-vao 2 m: carga 10*(x/2) em [0,2]: forca 10 kN, centroide 1.3333 -> braco 0.6667
    d_short = tot_short / 2.0 * 2.0 - 10e3 * (2.0 - 4.0 / 3.0)
    # mesma carga, mas como o solver a recebe: valor medio do quinhao em cada elemento de 1 m (carga uniforme por barra)
    def disc(L, q, run):
        half = int(L / 2)
        moment = 0.0
        for k in range(half):
            a, b = float(k), float(k + 1)
            mean = q * (min(a + b, 2 * run) / 2.0 / run) if b <= run else q   # media da rampa 0..q em [0, run], patamar depois
            if b > run >= a:
                mean = q * ((run - a) * ((a + run) / (2.0 * run)) + (b - run)) / (b - a)
            moment += mean * 1.0 * (L / 2.0 - (a + b) / 2.0) * 1000.0
        total_half = sum(1 for _ in range(half))
        return moment
    # V = metade da carga total (simetria); diferenca = V L/2 - momento da carga do semi-vao em torno do meio
    def diff(L, q, run):
        half_load = 0.0
        for k in range(int(L / 2)):
            a, b = float(k), float(k + 1)
            if b <= run:
                mean = q * (a + b) / (2.0 * run)
            elif a >= run:
                mean = q
            else:
                mean = q * ((run - a) * ((a + run) / (2.0 * run)) + (b - run)) / (b - a)
            half_load += mean
        return half_load * 1000.0 * (L / 2.0) - disc(L, q, run)
    d_long_disc = diff(6.0, 10.0, 2.0)
    d_short_disc = diff(4.0, 10.0, 2.0)
    ref.update({'M_meio_menos_apoio_longa': d_long, 'M_meio_menos_apoio_curta': d_short, 'carga_longa': tot_long, 'carga_curta': tot_short, 'disc_longa': d_long_disc, 'disc_curta': d_short_disc})
    out['ref'] = ref
    for mode in 'TAM':
        f = forces_of(cdir, mode)
        rows = {}
        for name, ids in (('viga_longa_y0', chain(bars, line_elements(bars, y=0.0))), ('viga_longa_y4', chain(bars, line_elements(bars, y=4.0))), ('viga_curta_x0', chain(bars, line_elements(bars, x=0.0))), ('viga_curta_x6', chain(bars, line_elements(bars, x=6.0)))):
            p = beam_profile(bars, f, ids)
            rows[name] = {'V_apoio': 0.5 * (p['V_i'] + p['V_j']), 'M_apoio': 0.5 * (p['M_apoio_i'] + p['M_apoio_j']), 'M_meio': p['M_meio'],
                          'M_meio_menos_apoio': p['M_meio'] + 0.5 * (p['M_apoio_i'] + p['M_apoio_j'])}
        cols = {}
        for (x, y) in ((0, 0), (6, 0), (6, 4), (0, 4)):
            c = column_at(bars, x, y)
            cols['%g_%g' % (x, y)] = max(abs(f[c][0]), abs(f[c][6]))
        rows['pilares_N'] = cols
        out['modos'][mode] = rows
    return out


def lv4(cdir):
    bars = bars_of(cdir)
    out = {'ref': {'N_pilar': Q * 36.0 / 4.0}, 'modos': {}}
    for mode in 'AM':
        f = forces_of(cdir, mode)
        cols = {}
        for (x, y) in ((1.5, 1.5), (4.5, 1.5), (4.5, 4.5), (1.5, 4.5)):
            c = column_at(bars, x, y)
            cols['%g_%g' % (x, y)] = max(abs(f[c][0]), abs(f[c][6]))
        out['modos'][mode] = {'pilares_N': cols}
    return out


def main(root, outjson):
    res = {}
    for case, fn in (('LV3_laje_unidirecional_2vigas', lv3), ('LV1_laje_4vigas_4pilares', lv1), ('LV4_laje_lisa_4pilares', lv4)):
        res[case] = fn(os.path.join(root, case))
    json.dump(res, open(outjson, 'w'), indent=1)
    # relatorio no terminal
    r = res['LV3_laje_unidirecional_2vigas']
    ref = r['ref']
    print('== LV3 (porticos planos, w = %.0f N/m, L = 6, h = 3); ref: M_apoio %.1f, M_meio %.1f, M_base %.1f, M_topo %.1f, H %.1f, N %.1f, V %.1f' % (Q * 2, ref['M_apoio'], ref['M_meio'], ref['M_base'], ref['M_topo'], ref['H'], ref['N_pilar'], ref['V_apoio']))
    for mode, rows in r['modos'].items():
        for k, v in rows.items():
            print('  modo %s %s: M_apoio %.1f (%.2f%%)  M_meio %.1f (%.2f%%)  V %.1f (%.2f%%)  N_viga %.1f (%.2f%%)  N_pilar %.1f (%.2f%%)  M_base %.1f (%.2f%%)  M_topo %.1f (%.2f%%)' % (
                mode, k, v['M_apoio'], 100 * rel(v['M_apoio'], ref['M_apoio']), v['M_meio'], 100 * rel(v['M_meio'], ref['M_meio']), v['V_apoio'], 100 * rel(v['V_apoio'], ref['V_apoio']),
                v['H_viga_N'], 100 * rel(v['H_viga_N'], ref['H']), v['N_pilar'], 100 * rel(v['N_pilar'], ref['N_pilar']), v['M_base'], 100 * rel(v['M_base'], ref['M_base']), v['M_topo'], 100 * rel(v['M_topo'], ref['M_topo'])))
    r = res['LV1_laje_4vigas_4pilares']
    ref = r['ref']
    print('== LV1 (45 graus); ref: N_pilar %.1f, V_longa %.1f, V_curta %.1f, (M_meio-M_apoio) longa %.1f, curta %.1f' % (ref['N_pilar'], ref['V_longa'], ref['V_curta'], ref['M_meio_menos_apoio_longa'], ref['M_meio_menos_apoio_curta']))
    for mode, rows in r['modos'].items():
        for k, v in rows.items():
            if k == 'pilares_N':
                print('  modo %s pilares N: %s' % (mode, ', '.join('%s=%.1f (%.2f%%)' % (n, x, 100 * rel(x, ref['N_pilar'])) for n, x in v.items())))
            else:
                longa = 'longa' in k
                rv = ref['V_longa'] if longa else ref['V_curta']
                rd = ref['M_meio_menos_apoio_longa'] if longa else ref['M_meio_menos_apoio_curta']
                rdd = ref['disc_longa'] if longa else ref['disc_curta']
                print('  modo %s %s: V %.1f (%.2f%%)  M_apoio %.1f  M_meio %.1f  M_meio-M_apoio %.1f (%.2f%% vs %.1f ideal; %.2f%% vs %.1f com quinhao medio por elemento)' % (mode, k, v['V_apoio'], 100 * rel(v['V_apoio'], rv), v['M_apoio'], v['M_meio'], v['M_meio_menos_apoio'], 100 * rel(v['M_meio_menos_apoio'], rd), rd, 100 * rel(v['M_meio_menos_apoio'], rdd), rdd))
    r = res['LV4_laje_lisa_4pilares']
    for mode, rows in r['modos'].items():
        print('== LV4 modo %s pilares N (ref %.1f): %s' % (mode, r['ref']['N_pilar'], ', '.join('%s=%.1f' % (n, x) for n, x in rows['pilares_N'].items())))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
