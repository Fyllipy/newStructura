"""Estudo MEF elastico do T-stub (Fase C6.3). Executa convergencia de malha, variantes fisicas e geometrias de 3 espécimes de [P2] SEM filete (r=0),
e compara com a rigidez de Piluso/Faella (r=0 => m'=m) e com a EN (Fase B). Saida: results_study.json e saida_study.txt.
Uso: PYTHONPATH=<esta pasta>;<../tstub_piluso> python run_study.py [convergencia|variantes|geometrias|todos]"""
import json
import math
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tstub_piluso'))
import fem_tstub as ft
import piluso_tstub as pt
import specimens as sp

HERE = os.path.dirname(os.path.abspath(__file__))
MODE = sys.argv[1] if len(sys.argv) > 1 else 'todos'
TAG = sys.argv[2] if len(sys.argv) > 2 else 'v2'
OUT = os.path.join(HERE, 'saida_%s_%s.txt' % (TAG, MODE))
lines = []


def say(s=''):
    lines.append(s)
    print(s)
    with open(OUT, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')


E = 210000.0


def base(idx=7, **kw):
    s = sp.specimen(idx, As=math.pi * sp._G[idx][0] ** 2 / 4)
    p = dict(tf=s.tf, m=s.m, n=s.n, b=s.b, tw=8.0, hw=30.0, dw=s.dw, Ab=math.pi * s.db ** 2 / 4, Lb=2 * s.tf + (s.th + s.tn) / 2,
             h=4.0, nz=3, contact=True, bolt='eq', FQ=25e3, kn=200000.0, th=s.th, tn=s.tn, dh=s.dh, spec=idx, db=s.db, hole=True)
    p.update(kw)
    return p


def formulas(p):
    """rigidez do T-stub completo (kN/mm) pelas duas formulacoes, mesma geometria (placa, r=0)."""
    m, n, tf, b, dh = p['m'], p['n'], p['tf'], p['b'], p['dh']
    beff = min(dh + 2 * m, b)
    Kfl_P = 0.25 * E * beff * tf ** 3 / m ** 3                       # 2 abas em serie, m' = m (r = 0)
    Kfl_P_elem = 0.5 * E * beff * tf ** 3 / m ** 3                    # um elemento T ([P1] eq. 38)
    # parafuso de Piluso: delta_b = By/(E Ab/Lb), By = F/2 + M_y/n (mecanismo 1: forca de alavanca proporcional); em regime elastico M_y/F e fixo pela geometria
    # usa-se o mesmo (F_y, M_y) do modelo: via analyse com a geometria do espécime
    s = sp.specimen(p['spec'], As=math.pi * sp._G[p['spec']][0] ** 2 / 4)
    s.r = 0.0
    import dataclasses
    s = dataclasses.replace(s, tf=tf, m=m, n=n, b=b, beff=beff, dw=p['dw'])
    r = pt.analyse(s, As_stiffness=p['Ab'], force_mechanism=1)
    Ki_P = r.K_initial / 1e3
    leff1 = min(2 * math.pi * m, 4 * m + 1.25 * n)
    leff1w = min(leff1, b)                                            # limitado pela largura do T-stub
    As_res = p['Ab'] * 0.80
    def en(leff, As):
        k4 = 0.9 * leff * tf ** 3 / m ** 3
        k10 = 1.6 * As / p['Lb']
        return 1.0 / (2.0 / (E * k4) + 1.0 / (E * k10)) / 1e3, E * k4 / 2 / 1e3
    en_beff, en_beff_fl = en(beff, As_res)
    en_leff, en_leff_fl = en(leff1w, As_res)
    return dict(Ki_P=Ki_P, Kfl_P=Kfl_P / 1e3, EN_beff=en_beff, EN_beff_fl=en_beff_fl, EN_leff=en_leff, EN_leff_fl=en_leff_fl, beff=beff, leff=leff1w)


results = {}


def run_case(name, p):
    t0 = time.time()
    r = ft.solve(p, TAG + '_' + name)
    if r is None:
        say('  %-28s FALHOU/timeout' % name)
        return None
    results[name] = dict(p={k: v for k, v in p.items()}, fem=r)
    return r


mode = sys.argv[1] if len(sys.argv) > 1 else 'todos'
if mode in ('convergencia', 'todos'):
    say('A) CONVERGENCIA DE MALHA (modelo v2: arruela em contato + furo; espécime 7 sem filete; tw=8 mm hw=30 mm hipoteses; F_q=25 kN)')
    say('%-10s %6s %8s %8s | %10s %9s %9s %9s %7s' % ('malha', 'h(mm)', 'elem', 'nos', 'K (kN/mm)', 'd (mm)', 'B_q (kN)', 'Q_q (kN)', 't (s)'))
    prev = None
    for (h, nz) in ((6, 2), (4, 3), (3, 4), (2.5, 5)):
        p = base(h=h, nz=nz)
        r = run_case('conv_h%s_nz%d' % (h, nz), p)
        if r:
            ch = '' if prev is None else '  dK=%+.2f%%' % (100 * (r['K'] / prev - 1))
            say('%-10s %6.1f %8d %8d | %10.2f %9.4f %9.2f %9.2f %7.1f%s' % ('h%g/nz%d' % (h, nz), h, r['nelem'], r['nnode'], r['K'], r['d_mm'], r['Bq'] / 1e3, r['Q'] / 1e3, r['time'], ch))
            prev = r['K']
    say()
if mode in ('variantes', 'todos'):
    H, NZ = 4, 3
    say('B) VARIANTES FISICAS (malha h=%g mm, nz=%d) -- espécime 7 sem filete' % (H, NZ))
    ref = run_case('var_ref', base(h=H, nz=NZ))
    say('  referencia: K = %.1f kN/mm (B_q=%.1f kN, Q_q=%.1f kN)' % (ref['K'], ref['Bq'] / 1e3, ref['Q'] / 1e3))
    cases = [('SEM furo (aba macica sob a arruela)', dict(hole=False)),
             ('parafuso RIGIDO (so flexao da aba + contato)', dict(bolt='rigid')),
             ('parafuso colado v1 (ARTEFATO, sem furo)', dict(bolt_model='glued', hole=False)),
             ('arruela de 3 mm', dict(twash=3.0)), ('arruela de 8 mm', dict(twash=8.0)),
             ('SEM contato (ponta livre; sem alavanca)', dict(contact=False)),
             ('contato 10x mais mole (kn=2e4)', dict(kn=2.0e4)), ('contato 10x mais duro (kn=2e6)', dict(kn=2.0e6)),
             ('tw = 4 mm', dict(tw=4.0)), ('tw = 12 mm', dict(tw=12.0)), ('tw = 16 mm', dict(tw=16.0)),
             ('hw = 15 mm', dict(hw=15.0)), ('hw = 60 mm', dict(hw=60.0))]
    for name, kw in cases:
        r = run_case('var_' + name[:12].replace(' ', '_'), base(h=H, nz=NZ, **kw))
        if r:
            say('  %-46s K = %8.1f kN/mm  (%+6.1f%% vs ref)  B_q=%.1f Q_q=%.1f kN' % (name, r['K'], 100 * (r['K'] / ref['K'] - 1), r['Bq'] / 1e3, r['Q'] / 1e3))
    say()
    say('C) DEPENDENCIA COM A LARGURA b (parafuso e geometria fixos; formulas: Piluso beff = min(dh+2m, b); EN leff = min(2 pi m, 4m+1,25 n, b))')
    say('%8s | %9s | %9s %9s | %9s %9s' % ('b (mm)', 'MEF', 'Piluso Ki', 'Piluso fl', 'EN leff', 'EN beff'))
    for b in (40.0, 60.0, 90.0, 122.8, 160.0, 200.0, 260.0):
        p = base(h=H, nz=NZ, b=b)
        r = run_case('var_b%g' % b, p)
        f = formulas(p)
        if r:
            say('%8.1f | %9.1f | %9.1f %9.1f | %9.1f %9.1f' % (b, r['K'], f['Ki_P'], f['Kfl_P'], f['EN_leff'], f['EN_beff']))
    say()
    say('D) DEPENDENCIA COM A ESPESSURA tf (m, n, b fixos; Lb = 2 tf + (th+tn)/2): expoente efetivo de tf entre pontos')
    prev = None
    for tf in (9.0, 11.0, 13.9, 16.0, 19.0):
        p = base(h=H, nz=NZ, tf=tf, Lb=2 * tf + (17.35 + 27.0) / 2)
        r = run_case('var_tf%g' % tf, p)
        f = formulas(p)
        if r:
            ex = '' if prev is None else '  expoente local = %.2f' % (math.log(r['K'] / prev[1]) / math.log(tf / prev[0]))
            say('  tf=%5.1f  MEF %8.1f | Piluso Ki %8.1f | EN(beff) %8.1f | EN(leff) %8.1f%s' % (tf, r['K'], f['Ki_P'], f['EN_beff'], f['EN_leff'], ex))
            prev = (tf, r['K'])
    say()
    say('E) DEPENDENCIA COM m (distancia eixo do parafuso - face da alma; n=71,7 fixo)')
    for mm_ in (30.0, 40.0, 52.45, 65.0, 80.0):
        p = base(h=H, nz=NZ, m=mm_)
        r = run_case('var_m%g' % mm_, p)
        f = formulas(p)
        if r:
            say('  m=%5.1f  MEF %8.1f | Piluso Ki %8.1f | EN(beff) %8.1f | EN(leff) %8.1f' % (mm_, r['K'], f['Ki_P'], f['EN_beff'], f['EN_leff']))
    say()
if mode in ('geometrias', 'todos'):
    say('F) TRES GEOMETRIAS de [P2] SEM filete (r=0; tw=8 e hw=30 ASSUMIDOS -- NAO sao os espécimes originais), malha h=4 mm nz=3')
    say('%4s | %9s | %9s %9s %9s %9s | %7s %7s %7s' % ('esp.', 'MEF', 'Piluso Ki', 'EN beff', 'EN leff', 'bolt rig.', 'P/MEF', 'ENb/MEF', 'ENl/MEF'))
    for idx in (1, 7, 12):
        p = base(idx, h=4, nz=3)
        r = run_case('geo_%d' % idx, p)
        rr = run_case('geo_%d_rigid' % idx, base(idx, h=4, nz=3, bolt="rigid"))
        f = formulas(p)
        if r and rr:
            say('%4d | %9.1f | %9.1f %9.1f %9.1f %9.1f | %7.2f %7.2f %7.2f' % (idx, r['K'], f['Ki_P'], f['EN_beff'], f['EN_leff'], rr['K'], f['Ki_P'] / r['K'], f['EN_beff'] / r['K'], f['EN_leff'] / r['K']))
with open(os.path.join(HERE, 'results_%s_%s.json' % (TAG, MODE)), 'w') as f:
    json.dump(results, f, indent=1)
