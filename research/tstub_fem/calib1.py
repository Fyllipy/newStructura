"""Calibracao: bloco A (10x10x10 mm) sobre bloco B rigido, contato unilateral; desloca o topo de A para baixo.
Esperado com contato: reacao no topo = E A d / L (A comprimido contra B). Sem contato (face errada): reacao ~ 0."""
import sys, os
import mwlib
FACE_B_TOP, FACE_A_BOT = int(sys.argv[1]), int(sys.argv[2])
m = mwlib.Model()
m.mats = {'steel': (210e9, 0.3), 'rigid': (210e12, 0.3)}
m.elsets_mat = {'A': 'steel', 'B': 'rigid'}
L = 0.01
def cube(body, z0):
    ids = []
    for k, dz in enumerate((0, 1)):
        for (i, j) in ((0, 0), (1, 0), (1, 1), (0, 1)):
            ids.append(m.node((body, i, j, k), i * L, j * L, z0 + dz * L))
    return ids
nB = cube('B', -L)
nA = cube('A', 0.0)
eB = m.hex('B', nB)
eA = m.hex('A', nA)
m.fix(0, nB[:4]); m.fix(1, nB[:4]); m.fix(2, nB[:4])          # base de B engastada
m.fix(0, [nA[0], nA[3]]); m.fix(1, [nA[0], nA[1]])             # evita corpo rigido lateral de A
m.force((0.0, 0.0, -2.1e4), nA[4:])                              # forca TOTAL -21 kN no topo (4 nos)
m.face_sets = {'faceB': [(eB, FACE_B_TOP)], 'faceA': [(eA, FACE_A_BOT)]}
m.contacts = [('faceB', 'faceA', 'unilateralelastic', 200000.0)]
os.makedirs('work', exist_ok=True)
m.to_liml('work/calib1.liml')
out = mwlib.run('work/calib1.liml', 'work/calib1.csv', timeout=240)
if not out:
    print('sem resultado (falha/timeout)'); sys.exit(1)
res = mwlib.read_results(out)
print('uz topo A =', [res[n][2] for n in nA[4:]], ' (esperado ~ -1e-5 se a forca for TOTAL de 21 kN; ~ -4e-5 se for por no)')
print('uz base A =', [res[n][2] for n in nA[:4]])
