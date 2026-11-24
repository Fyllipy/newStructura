# Resume, por caso e modo (T/A/M) do Structura: reacoes nas bases dos pilares, deslocamentos nos pontos de prova e equilibrio.
# uso: python resumo_modos.py <pasta_lvcases>
import csv
import os
import sys


def read(path):
    with open(path, newline='') as f:
        return [r for r in csv.reader(f) if r]


def near(rows, p, tol=1e-6):
    best = None
    for r in rows:
        d = max(abs(float(r[1]) - p[0]), abs(float(r[2]) - p[1]), abs(float(r[3]) - p[2]))
        if d <= tol and (best is None or d < best[0]):
            best = (d, r)
    return best[1] if best else None


def main(root):
    for case in sorted(os.listdir(root)):
        cdir = os.path.join(root, case)
        if not os.path.isdir(cdir):
            continue
        probes = [[float(v) for v in r] for r in read(os.path.join(cdir, 'probes.csv'))]
        bases = [[float(v) for v in r] for r in read(os.path.join(cdir, 'bases.csv'))]
        print('==', case)
        for mode in 'TAM':
            mdir = os.path.join(cdir, 'modo_' + mode)
            try:
                rows = read(os.path.join(mdir, 's_nodes.csv'))
            except OSError:
                print('  modo', mode, ': sem resultado')
                continue
            fz = []
            for b in bases:
                r = near(rows, b)
                fz.append(float(r[9 + 2]) if r else float('nan'))   # colunas: id,x,y,z, u(6), R(6) -> Fz = indice 4+6+2
            fz = []
            for b in bases:
                r = near(rows, b)
                fz.append(float(r[4 + 6 + 2]) if r else float('nan'))
            w = []
            for p in probes:
                r = near(rows, p)
                w.append(float(r[4 + 2]) if r else float('nan'))
            print('  modo %s  Rz pilares [kN]: %s  soma=%.3f' % (mode, ' '.join('%.2f' % (v / 1000) for v in fz), sum(fz) / 1000))
            print('          uz pontos [mm]: %s' % ' '.join('%.4f' % (v * 1000) for v in w))
            meta = open(os.path.join(mdir, 'meta.txt'), encoding='utf-8').read().splitlines()
            for line in meta:
                if line.startswith('warning=') or line.startswith('solver_warning='):
                    print('          ', line[:200])


if __name__ == '__main__':
    main(sys.argv[1])
