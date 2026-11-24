# Torre de telecomunicacoes autoportante (4 pernas, trelica em X, afunilada) com perfis L e C: gera o .dat do Structura e a geometria (CSV) para o gerador do Mecway.
# Geometria: 30 m, 10 modulos de 3 m; lado de 3,2 m na base afunilando a 1,2 m na cota 27 m (reto nos 3 m do topo, plataforma das antenas).
#   pernas: L 101,6x9,525 (modulos 1-5) e L 76,2x6,35 (6-9); no modulo 10 as pernas sao perfis C (U laminado 102x50);
#   horizontais: L 63,5x6,35 (niveis 0-8) e perfis C 102x50 (aneis da plataforma, niveis 9 e 10);
#   diagonais em X (cruzadas e ligadas no centro da face): L 63,5x6,35 (modulos 1-5) e L 50,8x4,76 (6-10).
# Casos: PP (peso proprio), ANT (peso das antenas no topo), VENTO (forcas horizontais por nivel + arrasto das antenas).
# Simplificacao declarada (igual nos dois programas): flexao isotropica com I = momento de inercia em torno do eixo paralelo a aba (L) / eixo fraco (C); o produto de
# inercia do L e a diferenca entre eixos do C nao entram (a orientacao local de barras obliquas e automatica no Mecway). J = (1/3) soma b t^3.
# uso: python gen_torre.py <pasta_saida>
import math
import os
import sys

out = sys.argv[1]
os.makedirs(out, exist_ok=True)

E = 2.0e8            # kN/m2
G = 7.6923e7         # kN/m2 (nu = 0,3)
GAMMA = 7850 * 9.80665 / 1000.0   # kN/m3 (densidade 7850 kg/m3)
PANELS, HP = 10, 3.0
W0, W9 = 3.2, 1.2


def width(k):
    return W0 - (W0 - W9) * min(k, 9) / 9.0


def angle_props(b, t):
    """L de abas iguais, cantos vivos: A, I em torno do eixo paralelo a aba (Ix = Iy), J."""
    a1, a2 = t * b, (b - t) * t
    area = a1 + a2
    y1, y2 = b / 2.0, t / 2.0                  # altura do centroide de cada retangulo (aba vertical e aba horizontal), origem no calcanhar
    yc = (a1 * y1 + a2 * y2) / area
    ix = t * b ** 3 / 12.0 + a1 * (y1 - yc) ** 2 + (b - t) * t ** 3 / 12.0 + a2 * (y2 - yc) ** 2
    j = (b * t ** 3 + (b - t) * t ** 3) / 3.0
    return area, ix, j


def channel_props(d, bf, tw, tf):
    """U de cantos vivos: A, I do eixo fraco (vertical), J."""
    area = d * tw + 2.0 * (bf - tw) * tf
    a_web, a_fl = d * tw, (bf - tw) * tf
    xc = (a_web * tw / 2.0 + 2.0 * a_fl * (tw + (bf - tw) / 2.0)) / area
    iy = d * tw ** 3 / 12.0 + a_web * (tw / 2.0 - xc) ** 2 + 2.0 * ((bf - tw) ** 3 * tf / 12.0 + a_fl * (tw + (bf - tw) / 2.0 - xc) ** 2)
    j = (d * tw ** 3 + 2.0 * (bf - tw) * tf ** 3) / 3.0
    return area, iy, j


# perfis (mm): (familia, parametros)
PROFILES = {
    'L1': ('L_Laminado', dict(b=101.6, t=9.525)),
    'L2': ('L_Laminado', dict(b=76.2, t=6.35)),
    'L3': ('L_Laminado', dict(b=63.5, t=6.35)),
    'L5': ('L_Laminado', dict(b=50.8, t=4.76)),
    'C1': ('U_Laminado', dict(d=102.0, bf=50.0, tw=5.0, tf=7.5)),
}
sections = []   # (nome, A, I, J)
for name, (family, p) in PROFILES.items():
    if family == 'L_Laminado':
        a, i, j = angle_props(p['b'] / 1000.0, p['t'] / 1000.0)
    else:
        a, i, j = channel_props(p['d'] / 1000.0, p['bf'] / 1000.0, p['tw'] / 1000.0, p['tf'] / 1000.0)
    sections.append((name, a, i, j))
sec_id = {name: n + 1 for n, (name, _, _, _) in enumerate(sections)}

# nos
nodes = {}   # id -> (x, y, z)
corner = {}
nid = 0
for k in range(PANELS + 1):
    w = width(k) / 2.0
    for c, (sx, sy) in enumerate(((-1, -1), (1, -1), (1, 1), (-1, 1))):
        nid += 1
        corner[(k, c)] = nid
        nodes[nid] = (sx * w, sy * w, k * HP)
center = {}
for k in range(PANELS):
    for f in range(4):
        a, b = corner[(k, f)], corner[(k, (f + 1) % 4)]
        c, d = corner[(k + 1, f)], corner[(k + 1, (f + 1) % 4)]
        nid += 1
        center[(k, f)] = nid
        pts = [nodes[a], nodes[b], nodes[c], nodes[d]]
        nodes[nid] = tuple(sum(p[i] for p in pts) / 4.0 for i in range(3))

members = []   # (ni, nj, perfil)
for k in range(PANELS):
    leg = 'C1' if k == 9 else ('L1' if k < 5 else 'L2')
    diag = 'L3' if k < 5 else 'L5'
    for c in range(4):
        members.append((corner[(k, c)], corner[(k + 1, c)], leg))
    for f in range(4):
        m = center[(k, f)]
        for n_ in (corner[(k, f)], corner[(k, (f + 1) % 4)], corner[(k + 1, f)], corner[(k + 1, (f + 1) % 4)]):
            members.append((n_, m, diag))
for k in range(PANELS + 1):
    hor = 'C1' if k >= 9 else 'L3'
    for c in range(4):
        members.append((corner[(k, c)], corner[(k, (c + 1) % 4)], hor))

# cargas (kN): vento por nivel e antenas
wind = []   # (no, fx, fy, fz)
for k in range(1, PANELS + 1):
    total = 1.2 + 0.18 * k
    for c in range(4):
        wind.append((corner[(k, c)], total / 4.0, 0.0, 0.0))
for c in range(4):   # arrasto das antenas no topo (so no nivel 10): +2 kN em x, dividido nos 4 nos
    wind.append((corner[(PANELS, c)], 0.5, 0.0, 0.0))
ant = [(corner[(PANELS, c)], 0.0, 0.0, -2.0) for c in range(4)]

lines = ['# Torre de telecomunicacoes autoportante 30 m (4 pernas, trelica em X) com perfis L e C - modelo de validacao Structura x Mecway',
         '# Unidades: kN, m (E e G em kN/m2)', '', '[MATERIALS]', '# ID    E (kN/m2)       G (kN/m2)', '1 %.6e %.6e' % (E, G), '',
         '[MATERIAL_PROPS]', '# ID  peso especifico (kN/m3)  Nome  Biblioteca  fy(MPa)  fu(MPa)', '1 %.6f Aco_ASTM_A36 - 250 400' % GAMMA, '', '[SECTIONS]', '# ID    A (m^2)     Iz (m^4)      Iy (m^4)      J (m^4)']
for name, a, i, j in sections:
    lines.append('%d %.6e %.6e %.6e %.6e' % (sec_id[name], a, i, i, j))
lines += ['', '[SECTION_GEOMETRY]', '# ID    Familia(_ no lugar de espaco)  Espelhada(0/1)  parametro=valor (mm) ...']
for name, (family, p) in PROFILES.items():
    lines.append('%d %s 0 %s' % (sec_id[name], family, ' '.join('%s=%g' % (k, v) for k, v in p.items())))
lines += ['', '[NODES]', '# ID    X (m)    Y (m)    Z (m)    UX  UY  UZ  RX  RY  RZ']
for n, (x, y, z) in nodes.items():
    base = z < 1e-9 and n in [corner[(0, c)] for c in range(4)]
    lines.append('%d %.6f %.6f %.6f %s' % (n, x, y, z, '1 1 1 0 0 0' if base else '0 0 0 0 0 0'))
lines += ['', '[MEMBERS]', '# ID    Node_i   Node_j   Material_ID   Section_ID']
for m, (a, b, p) in enumerate(members, 1):
    lines.append('%d %d %d 1 %d' % (m, a, b, sec_id[p]))
# O aplicativo gira os perfis L em 45 graus por padrao (abas alinhadas aos eixos locais; eixos principais a 45 graus). O Mecway nao tem produto de inercia nem orientacao
# da secao de barras: para os dois programas verem a MESMA rigidez, os L sao girados +45 graus (eixos principais alinhados aos eixos locais da barra).
lines += ['', '[MEMBER_PLACEMENT]', '# Member_ID  Giro(graus)  Ponto_ref(0-8)  Desloc_z_I(m)  Desloc_y_I(m)  J_diferente(0/1)  Desloc_z_J(m)  Desloc_y_J(m)  Liberacoes  Tipo']
for m, (a, b, p) in enumerate(members, 1):
    if p.startswith('L'):
        lines.append('%-12d %12.6f %6d %14.6f %14.6f %4d %14.6f %14.6f %6d %3d' % (m, 45.0, 0, 0.0, 0.0, 0, 0.0, 0.0, 0, 0))
lines += ['', '[LOAD_CASES]', '# Nome  Sigla  PesoProprio(0/1)', 'PP PP 1', 'ANT ANT 0', 'VENTO VENTO 0', '', '[COMBINATIONS]', 'SERV PP=1 ANT=1 VENTO=1', '',
          '[NODAL_LOADS]', '# Node_ID   Fx   Fy   Fz   Mx   My   Mz   Caso (kN, kN.m)']
for n, fx, fy, fz in ant:
    lines.append('%d %g %g %g 0 0 0 ANT' % (n, fx, fy, fz))
for n, fx, fy, fz in wind:
    lines.append('%d %g %g %g 0 0 0 VENTO' % (n, fx, fy, fz))
lines += ['', '[MEMBER_LOADS]', '# Formato: Member_ID  Sistema(Local/Global)  qx (kN/m)  qy (kN/m)  qz (kN/m)  Caso', '']
open(os.path.join(out, 'torre_telecom.dat'), 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')

with open(os.path.join(out, 'torre_geom.csv'), 'w') as f:
    for n, (x, y, z) in nodes.items():
        f.write('N,%d,%.9g,%.9g,%.9g\n' % (n, x, y, z))
    for m, (a, b, p) in enumerate(members, 1):
        f.write('M,%d,%d,%d,%d\n' % (m, a, b, sec_id[p]))
    for n, fx, fy, fz in ant:
        f.write('LA,%d,%g,%g,%g\n' % (n, fx, fy, fz))
    for n, fx, fy, fz in wind:
        f.write('LW,%d,%g,%g,%g\n' % (n, fx, fy, fz))
    for name, a, i, j in sections:
        f.write('S,%d,%s,%.9e,%.9e,%.9e\n' % (sec_id[name], name, a, i, j))
print('nos %d, barras %d, perfis %s' % (len(nodes), len(members), [(s[0], round(s[1] * 1e4, 2), round(s[2] * 1e8, 1)) for s in sections]))
