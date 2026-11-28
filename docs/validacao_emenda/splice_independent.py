"""Implementacao INDEPENDENTE (Python, so a biblioteca padrao) da emenda de viga com duas chapas de extremidade parafusadas
EN 1993-1-8:2005: rigidez (6.3.2 Tabela 6.11 k5 e k10; 6.3.3.1(5)) e resistencia (6.2.4 T-stub, 6.2.6.8, 6.2.7.2(10)). Entradas = cadastro padrao do catalogo 135.
Nao importa nada do projeto; serve para conferir os numeros do C++ (a conferencia e feita em src/tests/TestBeamSplice.cpp com os valores aqui impressos)."""
import math, json, sys

E = 200000.0
d, bf, tf, tw, fy_b = 300.0, 150.0, 10.7, 7.1, 250.0
bp, tp, fy_p = 170.0, 22.4, 250.0
w, pitch, first, edge = 90.0, 70.0, 40.0, 35.0
leg = 9.0
db = 20.0
Ab = math.pi * db * db / 4
As = 0.78 * Ab
fub = 825.0  # A325 (db 20): ver grade do catalogo
gamma_a2, gamma_a1 = 1.35, 1.10
Ft_bolt = 0.75 * Ab * fub / gamma_a2
washer = 4.0
head, nut = 0.7 * db, 0.8 * db
Lb = 2 * tp + 2 * washer + (head + nut) / 2


def rows_of(n):
    out = []
    for r in range(1, n + 1):
        x = first + (r - 1) * pitch
        ex = edge + (n - r) * pitch
        mx = x - 0.8 * leg
        ep = (bp - w) / 2
        leff_cp = 2 * math.pi * mx
        leff_nc = min(4 * mx + 1.25 * ex, ep + 2 * mx + 0.625 * ex, 0.5 * bp, 0.5 * w + 2 * mx + 0.625 * ex)
        out.append(dict(h=d - tf / 2 + x, m=mx, e=ep, cp=leff_cp, nc=leff_nc))
    out.sort(key=lambda r: -r['h'])
    for i, r in enumerate(out):
        r['row'] = i + 1
    return out


def tstub(leff1, leff2, m, e, sumFt):
    n = min(e, 1.25 * m)
    mpl1 = 0.25 * leff1 * tp * tp * fy_p / gamma_a1
    mpl2 = 0.25 * leff2 * tp * tp * fy_p / gamma_a1
    return min(4 * mpl1 / m, (2 * mpl2 + n * sumFt) / (m + n), sumFt)


def splice(n):
    rows = rows_of(n)
    sumKh = sumKh2 = 0.0
    for r in rows:
        leff = min(r['cp'], r['nc'])
        k5 = 0.9 * leff * tp ** 3 / r['m'] ** 3
        k10 = 1.6 * As / Lb
        r['k5'], r['k10'] = k5, k10
        r['keff'] = 1.0 / (1 / k5 + 1 / k5 + 1 / k10)
        sumKh += r['keff'] * r['h']
        sumKh2 += r['keff'] * r['h'] ** 2
    z = sumKh2 / sumKh
    keq = sumKh / z
    sini = E * z * z * keq * 1e-6  # kN.m/rad
    # resistencia
    wpl = bf * tf * (d - tf) + tw * (d - 2 * tf) ** 2 / 4
    cap = wpl * fy_b / gamma_a1 / (d - tf)
    sumFt = 2 * Ft_bolt
    total = 0.0
    xrow = None
    res = []
    for i, r in enumerate(rows):
        ep = tstub(min(r['cp'], r['nc']), r['nc'], r['m'], r['e'], sumFt)
        ftwb = min(r['cp'], r['nc']) * tw * fy_b / gamma_a1
        ind = min(ep, ftwb)
        v = ind
        if cap - total < v:
            v = max(0.0, cap - total)
        if xrow is not None:
            bound = res[xrow]['eff'] * r['h'] / res[xrow]['h']
            v = min(v, bound)
        res.append(dict(row=r['row'], h=r['h'], ind=ind, eff=v))
        if xrow is None and ind > 1.9 * Ft_bolt:
            xrow = i
        total += v
    mj = sum(x['eff'] * x['h'] for x in res) * 1e-6
    return dict(z=z, keq=keq, sini=sini, mj=mj, rows=[dict(row=r['row'], h=r['h'], keff=r['keff'], k5=r['k5'], k10=r['k10']) for r in rows], res=res)




def welded_part():
    """Parte A da ligacao fora do no: mesas da viga soldadas ao pilar (EN 1993-1-8 6.2.6, 6.2.7.1, 6.3.2). Pilar 260x146x12.7x7.6 r=10, fy=250; solda de penetracao total (ap = 0); beta = 1."""
    hc, bc, tfc, twc, rc, fyc = 260.0, 146.0, 12.7, 7.6, 10.0, 250.0
    beta, ap = 1.0, 0.0
    dc = hc - 2 * tfc - 2 * rc
    area = 2 * bc * tfc + (hc - 2 * tfc) * twc + (4 - math.pi) * rc * rc
    avc = area - 2 * bc * tfc + (twc + 2 * rc) * tfc
    z = d - tf
    beff = tf + 2 * math.sqrt(2) * ap + 5 * (tfc + rc)
    k1 = 0.38 * avc / (beta * z)
    k2 = 0.7 * beff * twc / dc
    k3 = 0.7 * beff * twc / dc
    sini = E * z * z / (1 / k1 + 1 / k2 + 1 / k3) * 1e-6
    # resistencia
    def omega(b, be, tw_, av):
        rr = be * tw_ / av
        w1 = 1 / math.sqrt(1 + 1.3 * rr * rr)
        w2 = 1 / math.sqrt(1 + 5.2 * rr * rr)
        if b <= 0.5: return 1.0
        if b < 1.0: return w1 + 2 * (1 - b) * (1 - w1)
        if b == 1.0: return w1
        if b < 2.0: return w1 + (b - 1) * (w2 - w1)
        return w2
    vwp = 0.9 * fyc * avc / (math.sqrt(3) * gamma_a1)
    panel = vwp / beta
    om = omega(beta, beff, twc, avc)
    lam = 0.932 * math.sqrt(beff * dc * fyc / (E * twc * twc))
    rho = 1.0 if lam <= 0.72 else (lam - 0.2) / (lam * lam)
    fcwc = min(om * beff * twc * fyc / gamma_a1, om * rho * beff * twc * fyc / gamma_a1)
    wpl = bf * tf * (d - tf) + tw * (d - 2 * tf) ** 2 / 4
    fcfb = wpl * fy_b / gamma_a1 / (d - tf)
    cap = min(panel, fcwc, fcfb)
    ftwc = om * beff * twc * fyc / gamma_a1
    mj = min(ftwc, cap) * z * 1e-6
    return dict(k1=k1, k2=k2, k3=k3, sini=sini, mj=mj, z=z)


def outside(t_rows, s_rows, negative=False):
    """Ligacao fora do no (147) = parte soldada A em serie com a emenda B (rotacoes somam; Mj = min)."""
    n = s_rows if negative else t_rows
    b = splice(n)
    a = welded_part()
    s_tot = 1.0 / (1.0 / a['sini'] + 1.0 / b['sini'])
    return dict(sini=s_tot, mj=min(a['mj'], b['mj']), a=a, b=b)


if __name__ == '__main__':
    for n in (1, 2, 3):
        o = splice(n)
        print(f"tRows={n}: Sini={o['sini']:.4f} Mj={o['mj']:.5f} z={o['z']:.4f}")
        for r, q in zip(o['rows'], o['res']):
            print(f"   row {r['row']} h={r['h']:.4f} keff={r['keff']:.6f} k5={r['k5']:.6f} k10={r['k10']:.6f} ind={q['ind']/1e3:.5f} eff={q['eff']/1e3:.5f}")

    a = welded_part()
    print(f"parte soldada: k1={a['k1']:.6f} k2={a['k2']:.6f} k3={a['k3']:.6f} Sini={a['sini']:.4f} Mj={a['mj']:.5f}")
    for t, sr, neg in ((1, 1, False), (2, 1, False), (2, 1, True)):
        o = outside(t, sr, neg)
        print(f"fora do no t={t} s={sr} {'negativo' if neg else 'positivo'}: Sini={o['sini']:.4f} Mj={o['mj']:.5f}")
