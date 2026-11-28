"""Digitalizacao da curva EXPERIMENTAL (linha continua) da Fig. 8 de [P2] (Piluso et al. 2001, Parte II).

DADOS DIGITALIZADOS, NAO TABULADOS: obtidos de figura raster 1-bit (300 dpi) por analise de pixels. Incertezas esperadas: posicao do eixo (+-2 px), espessura do traco (~4 px),
descontinuidade da curva junto aos eixos (nao recuperavel). Calibracao verificada contra o maximo da curva experimental, que [P2] Tabela 3 informa (Fu,exp e du,exp).
Python puro. Uso: python -I digitize.py  (executar com PYTHONPATH apontando para esta pasta)
"""
import sys
from collections import deque

import pbm

# retangulos dos quadros (px): (esq, dir, topo, base) e faixas de eixo (xmax mm, ymax kN) lidas dos rotulos da Fig. 8
PANELS = {
    # pagina 6 (fig-000.pbm)
    1: ("fig-000.pbm", (93, 711.5, 49, 465), (35, 400)),
    2: ("fig-000.pbm", (904.5, 1525, 43, 458), (35, 400)),
    3: ("fig-000.pbm", (92.5, 712, 591, 1008), (60, 300)),
    4: ("fig-000.pbm", (904.5, 1525.5, 587, 1004), (100, 400)),
    5: ("fig-000.pbm", (93, 712, 1134, 1550), (80, 500)),
    6: ("fig-000.pbm", (905, 1526, 1130, 1546), (50, 600)),
    7: ("fig-000.pbm", (94, 713.5, 1678, 2095), (80, 600)),
    8: ("fig-000.pbm", (906, 1527, 1674, 2091), (60, 500)),
    9: ("fig-000.pbm", (95, 714.5, 2221, 2637), (100, 400)),
    10: ("fig-000.pbm", (907, 1528, 2217, 2633), (50, 350)),
    # pagina 7 (fig-001.pbm)
    11: ("fig-001.pbm", (87.5, 788.5, 45, 463), (70, 300)),
    12: ("fig-001.pbm", (978.5, 1681.5, 44, 462), (100, 250)),
}


def components(mask, w, h, min_size):
    seen = bytearray(w * h)
    comps = []
    for y0 in range(h):
        for x0 in range(w):
            i0 = y0 * w + x0
            if mask[i0] and not seen[i0]:
                q = deque([(x0, y0)])
                seen[i0] = 1
                pts = []
                while q:
                    x, y = q.popleft()
                    pts.append((x, y))
                    for dy in (-1, 0, 1):
                        for dx in (-1, 0, 1):
                            nx, ny = x + dx, y + dy
                            if 0 <= nx < w and 0 <= ny < h:
                                j = ny * w + nx
                                if mask[j] and not seen[j]:
                                    seen[j] = 1
                                    q.append((nx, ny))
                if len(pts) >= min_size:
                    comps.append(pts)
    return comps


def digitize(i, images):
    fname, (xl, xr, yt, yb), (xmax, ymax) = PANELS[i]
    w, h, rows = images[fname]
    inset = 6
    x0, x1 = int(xl) + inset, int(xr) - inset
    y0, y1 = int(yt) + inset, int(yb) - inset
    W, H = x1 - x0 + 1, y1 - y0 + 1
    mask = bytearray(W * H)
    for y in range(H):
        for x in range(W):
            if pbm.px(rows, x0 + x, y0 + y):
                mask[y * W + x] = 1
    # legenda: caixa no terco inferior, a direita de ~0,27 da largura (ver Fig. 8); apagada inteira
    lx0, lx1 = int(0.26 * W), W
    ly0, ly1 = int(H - 0.23 * H), H
    for y in range(ly0, ly1):
        for x in range(lx0, lx1):
            mask[y * W + x] = 0
    comps = components(mask, W, H, 200)
    if not comps:
        return []
    comps.sort(key=len, reverse=True)
    pts = comps[0]
    by_x = {}
    for x, y in pts:
        by_x.setdefault(x, []).append(y)
    out = []
    for x in sorted(by_x):
        ys = sorted(by_x[x])
        y = ys[len(ys) // 2]
        d = (x0 + x - xl) / (xr - xl) * xmax
        f = (yb - (y0 + y)) / (yb - yt) * ymax
        out.append((d, f))
    return out


def main():
    images = {name: pbm.read_pbm(name) for name in ("fig-000.pbm", "fig-001.pbm")}
    for i in range(1, 13):
        curve = digitize(i, images)
        with open("exp_curve_%02d.csv" % i, "w") as f:
            f.write("# especime %d: curva experimental DIGITALIZADA de [P2] Fig. 8 (displacement mm; load kN)\n" % i)
            for d, fo in curve:
                f.write("%.4f,%.3f\n" % (d, fo))
        if curve:
            fmax = max(c[1] for c in curve)
            dmax = [c[0] for c in curve if c[1] == fmax][0]
            print("espec %2d: pontos=%4d  max digitalizado F=%.1f kN em d=%.1f mm  (d final=%.1f mm)" % (i, len(curve), fmax, dmax, curve[-1][0]))
        else:
            print("espec %2d: sem curva" % i)


if __name__ == "__main__":
    main()
