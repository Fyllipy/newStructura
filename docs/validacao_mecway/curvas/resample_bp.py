# Reamostra a forma de flambagem do Mecway (solido hex20 do BucklingPlate.liml, nos do plano medio) nos nos da malha de casca do Structura (interpolacao bilinear) e
# normaliza o sinal de cada modo (deslocamento de maior modulo positivo) nos dois programas, para os prints lado a lado.
# uso: python resample_bp.py <pasta_mecway> <pasta_structura> <saida>
import bisect
import csv
import os
import sys

mw_dir, st_dir, out = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(out, exist_ok=True)
nodes = [[float(v) for v in r] for r in csv.reader(open(os.path.join(st_dir, 'nodes.csv'))) if r]


def read(path):
    return [[float(v) for v in r] for r in csv.reader(open(path)) if r]


def sign_fix(rows):
    # coluna 5 = w (deslocamento fora do plano)
    big = max(rows, key=lambda r: abs(r[5]))
    s = 1.0 if big[5] >= 0 else -1.0
    return [r[:3] + [s * r[3], s * r[4], s * r[5]] for r in rows]


for k in range(1, 5):
    allrows = read(os.path.join(mw_dir, 'mw_mode%d_all.csv' % k))
    # valor medio por (x, y) sobre a espessura (nos de canto da face, de meio de aresta e do plano medio): os elementos hex20 da malha sao retangulos 1/7 x 2/11 em planta
    acc = {}
    for r in allrows:
        key = (round(r[0], 5), round(r[1], 5))
        a_ = acc.setdefault(key, [0.0, 0.0, 0.0, 0])
        a_[0] += r[3]
        a_[1] += r[4]
        a_[2] += r[5]
        a_[3] += 1
    grid = {kk: [v[0] / v[3], v[1] / v[3], v[2] / v[3]] for kk, v in acc.items()}
    nx, ny = 7, 11
    dx, dy = 1.0 / nx, 2.0 / ny

    def at(x, y):
        key = (round(x, 5), round(y, 5))
        if key not in grid:
            # tolerancia por arredondamento de coordenadas
            best = min(grid, key=lambda g: (g[0] - x) ** 2 + (g[1] - y) ** 2)
            return grid[best]
        return grid[key]

    def serendipity(x, y):
        i = min(max(int(x / dx), 0), nx - 1)
        j = min(max(int(y / dy), 0), ny - 1)
        x0, y0 = i * dx, j * dy
        xi = 2.0 * (x - x0) / dx - 1.0
        eta = 2.0 * (y - y0) / dy - 1.0
        pts = [(x0, y0, -1, -1), (x0 + dx, y0, 1, -1), (x0 + dx, y0 + dy, 1, 1), (x0, y0 + dy, -1, 1)]
        out_v = [0.0, 0.0, 0.0]
        for (px, py, sx, sy) in pts:
            n = 0.25 * (1 + sx * xi) * (1 + sy * eta) * (sx * xi + sy * eta - 1)
            v = at(px, py)
            for c in range(3):
                out_v[c] += n * v[c]
        mids = [(x0 + 0.5 * dx, y0, 0, -1), (x0 + dx, y0 + 0.5 * dy, 1, 0), (x0 + 0.5 * dx, y0 + dy, 0, 1), (x0, y0 + 0.5 * dy, -1, 0)]
        for (px, py, sx, sy) in mids:
            if sx == 0:
                n = 0.5 * (1 - xi * xi) * (1 + sy * eta)
            else:
                n = 0.5 * (1 + sx * xi) * (1 - eta * eta)
            v = at(px, py)
            for c in range(3):
                out_v[c] += n * v[c]
        return out_v
    rows = []
    for n in nodes:
        u = serendipity(min(max(n[0], 0.0), 1.0), min(max(n[1], 0.0), 2.0))
        rows.append([n[0], n[1], n[2], u[0], u[1], u[2]])
    rows = sign_fix(rows)
    with open(os.path.join(out, 'mw_shape%d.csv' % k), 'w', newline='') as f:
        w = csv.writer(f)
        for r in rows:
            w.writerow(['%.17g' % v for v in r])
    st = sign_fix(read(os.path.join(st_dir, 's_mode%d.csv' % k)))
    with open(os.path.join(out, 's_shape%d.csv' % k), 'w', newline='') as f:
        w = csv.writer(f)
        for r in st:
            w.writerow(['%.17g' % v for v in r])
    print('modo', k, 'nos', len(rows))
