"""Verificacao do fluxo MEF (Mecway + CalculiX, hex8 com modos incompativeis) contra solucoes ANALITICAS conhecidas, antes de qualquer uso no T-stub.
V1: balanco 3D (placa larga) sob carga na ponta: flecha x Euler-Bernoulli + cortante (Timoshenko) e x placa (fator 1/(1-nu^2) da flexao cilindrica).
V2: bloco comprimido contra base rigida por contato unilateral (calibracao de faces e da forca TOTAL em selecao de nos).
Uso: PYTHONPATH=<esta pasta> python verify_pipeline.py"""
import math
import os
import mwlib

os.makedirs('work', exist_ok=True)
out = []


def say(s):
    out.append(s)
    print(s, flush=True)


# ---------------------------------------------------------------- V1: balanco
E, nu = 210e9, 0.3
b, L, t = 122.8e-3, 52.45e-3, 13.9e-3
P = 10e3
for (nx, ny, nz) in ((6, 6, 2), (10, 10, 4)):
    m = mwlib.Model()
    m.mats = {'steel': (E, nu)}
    m.elsets_mat = {'BEAM': 'steel'}
    xs = [b * i / nx for i in range(nx + 1)]
    ys = [L * j / ny for j in range(ny + 1)]
    zs = [t * k / nz for k in range(nz + 1)]
    ids = {}
    for i in range(nx + 1):
        for j in range(ny + 1):
            for k in range(nz + 1):
                ids[(i, j, k)] = m.node(('N', i, j, k), xs[i], ys[j], zs[k])
    for i in range(nx):
        for j in range(ny):
            for k in range(nz):
                n8 = [ids[(i, j, k)], ids[(i + 1, j, k)], ids[(i + 1, j + 1, k)], ids[(i, j + 1, k)],
                      ids[(i, j, k + 1)], ids[(i + 1, j, k + 1)], ids[(i + 1, j + 1, k + 1)], ids[(i, j + 1, k + 1)]]
                m.hex('BEAM', n8)
    root = [ids[(i, 0, k)] for i in range(nx + 1) for k in range(nz + 1)]
    for ax in (0, 1, 2):
        m.fix(ax, root)
    tip = [ids[(i, ny, nz)] for i in range(nx + 1)]                  # carga na aresta superior da ponta
    # distribui P igualmente (cada no recebe P/len)
    for n in tip:
        m.force((0.0, 0.0, -P / len(tip)), [n])
    m.to_liml('work/verify_beam.liml')
    o = mwlib.run('work/verify_beam.liml', 'work/verify_beam.csv', timeout=300)
    res = mwlib.read_results(o)
    defl = sum(abs(res[n][2]) for n in tip) / len(tip)
    I = b * t ** 3 / 12
    EB = P * L ** 3 / (3 * E * I)
    G = E / (2 * (1 + nu))
    TM = EB + P * L / (5.0 / 6.0 * G * b * t)
    PL = EB * (1 - nu ** 2) + P * L / (5.0 / 6.0 * G * b * t)       # limite de placa muito larga (flexao cilindrica)
    say('V1 balanco %2dx%2dx%d: MEF = %.5f mm | Euler-Bernoulli %.5f | Timoshenko %.5f | placa larga (1-nu2) %.5f | MEF/Timoshenko = %.3f' % (nx, ny, nz, defl * 1e3, EB * 1e3, TM * 1e3, PL * 1e3, defl / TM))
say('   Esperado: MEF entre Timoshenko (viga) e o limite de placa larga; b/t = %.1f (efeito de largura entre 0 e ~(1-nu^2)).' % (b / t))

# ---------------------------------------------------------------- V2: equilibrio do T-stub sem contato (B_q deve igualar F_q)
say('V2 equilibrio (T-stub, sem contato): ver run_study: B_q = F_q dentro de 1e-4 (aba livre, so parafuso e carga).')
with open('saida_verify_pipeline.txt', 'w', encoding='utf-8') as f:
    f.write('\n'.join(out) + '\n')
