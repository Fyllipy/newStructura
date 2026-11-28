"""Comparacao do modelo de Piluso et al. (reproduzido em piluso_tstub.py) com os 12 espécimes de [P2].

Referencias experimentais:
  - TABULADAS: Fu,exp e du,exp ([P2] Tabela 3, com a correcao de rotulos documentada em specimens.py).
  - DIGITALIZADAS (nao tabuladas): curva F-delta experimental da Fig. 8 ([P2]), extraida por digit/digitize.py (exp_curve_XX.csv).
Nenhum parametro foi ajustado. Uso: python -I validate_experimental.py   (PYTHONPATH = esta pasta e digit/)
"""
import math
import os
import statistics as st

import piluso_tstub as pt
import specimens as sp

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_TXT = os.path.join(HERE, "saida_validate_experimental.txt")
FIG = os.path.join(HERE, "..", "..", "docs", "validation", "figures", "tstub_piluso_comparison.svg")

lines = []


def say(s=""):
    lines.append(s)
    print(s)


def read_curve(i):
    pts = []
    with open(os.path.join(HERE, "digit", "exp_curve_%02d.csv" % i)) as f:
        for l in f:
            if l.startswith("#"):
                continue
            a, b = l.split(",")
            pts.append((float(a), float(b)))
    return pts


def interp(pts, x):
    """interpolacao linear numa lista (x,y) com x crescente; None fora do intervalo."""
    if not pts or x < pts[0][0] or x > pts[-1][0]:
        return None
    lo, hi = 0, len(pts) - 1
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if pts[mid][0] <= x:
            lo = mid
        else:
            hi = mid
    (x0, y0), (x1, y1) = pts[lo], pts[hi]
    return y0 if x1 == x0 else y0 + (y1 - y0) * (x - x0) / (x1 - x0)


def smooth_monotonic_x(pts):
    """a curva digitalizada e funcao de x (uma mediana por coluna); remove eventuais retrocessos de x."""
    out = []
    for d, f in pts:
        if not out or d > out[-1][0]:
            out.append((d, f))
    return out


def trapz(pts, x_end):
    s = 0.0
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        if x0 >= x_end:
            break
        xb = min(x1, x_end)
        yb = y1 if xb == x1 else y0 + (y1 - y0) * (xb - x0) / (x1 - x0)
        s += 0.5 * (y0 + yb) * (xb - x0)
    return s


results = []
curves_for_plot = {}
say("COMPARACAO MODELO x EXPERIMENTO (12 especimes de [P2])")
say("Area do parafuso na resistencia: bruta pi db^2/4 (reproduz a Tabela 3). Mecanismo: criterio beta_u de [P1] (11); para o especime 8 mostram-se os dois.")
say()
hdr = "%3s %3s | %7s %7s %6s | %7s %7s %6s | %7s %7s | %7s %7s %7s %7s"
say(hdr % ("#", "mec", "Fu_exp", "Fu_th", "th/ex", "du_exp", "du_th", "th/ex", "Fy_th", "Ki_th", "bias", "rms", "maxabs", "W th/ex"))
say(hdr % ("", "", "kN", "kN", "", "mm", "mm", "", "kN", "kN/mm", "%", "%", "%", ""))
for i in range(1, 13):
    pub = sp.published(i)
    s = sp.specimen(i, As=math.pi * sp._G[i][0] ** 2 / 4)
    variants = [("criterio", None)]
    if i == 8:
        variants = [("mec.2 (criterio)", None), ("mec.1 (usado na Tab.3)", 1)]
    exp = smooth_monotonic_x(read_curve(i))
    for label, forced in variants:
        r = pt.analyse(s, As_stiffness=math.pi * s.db ** 2 / 4, force_mechanism=forced)
        ml = [(0.0, 0.0), (r.disp["dy"], r.forces["Fy"] / 1e3), (r.disp["dh"], r.forces["Fh"] / 1e3), (r.disp["dm"], r.forces["Fm"] / 1e3), (r.disp["du"], r.forces["Fu"] / 1e3)]
        cont = [(d, f / 1e3) for d, f in pt.curve(s, As_stiffness=math.pi * s.db ** 2 / 4, force_mechanism=forced)]
        du_th = ml[-1][0]
        d_end = min(du_th, exp[-1][0])
        d_start = max(0.05 * pub["du_exp"], 1.0)      # exclui o trecho junto aos eixos (digitalizacao nao recuperavel) -- ver limitacoes
        grid = [d_start + (d_end - d_start) * k / 59.0 for k in range(60)]
        errs = []
        for d in grid:
            fe, ft = interp(exp, d), interp(ml, d)
            if fe and ft:
                errs.append((ft - fe) / fe)
        errs_c = []
        for d in grid:
            fe, fc = interp(exp, d), interp(cont, d)
            if fe and fc:
                errs_c.append((fc - fe) / fe)
        bias_c = 100 * st.mean(errs_c) if errs_c else float('nan')
        rms_c = 100 * math.sqrt(st.mean(e * e for e in errs_c)) if errs_c else float('nan')
        bias = 100 * st.mean(errs)
        rms = 100 * math.sqrt(st.mean(e * e for e in errs))
        mx = 100 * max(abs(e) for e in errs)
        # trabalho ate o menor deslocamento comum
        w_ratio = trapz(ml, d_end) / trapz(exp, d_end)
        results.append(dict(i=i, label=label, mech=r.mechanism, Fu_exp=pub["Fu_exp"], Fu_th=r.forces["Fu"] / 1e3, du_exp=pub["du_exp"], du_th=du_th,
                            Fy=r.forces["Fy"] / 1e3, Ki=r.K_initial, bias_c=bias_c, rms_c=rms_c, bias=bias, rms=rms, mx=mx, w=w_ratio, errs=errs, d_start=d_start, d_end=d_end))
        say(hdr % (i, str(r.mechanism) + ("*" if i == 8 else ""), "%.1f" % pub["Fu_exp"], "%.1f" % (r.forces["Fu"] / 1e3), "%.3f" % (r.forces["Fu"] / 1e3 / pub["Fu_exp"]),
                   "%.1f" % pub["du_exp"], "%.1f" % du_th, "%.3f" % (du_th / pub["du_exp"]), "%.1f" % (r.forces["Fy"] / 1e3), "%.1f" % (r.K_initial / 1e3),
                   "%+.1f" % bias, "%.1f" % rms, "%.1f" % mx, "%.3f" % w_ratio))
        curves_for_plot[(i, label)] = (exp, ml, cont)
say("(* especime 8: duas linhas; a Tabela 3 de [P2] usa o mecanismo 1)")
say()


def summary(name, vals):
    say("  %-34s n=%2d  media %+.3f  desvio-padrao %.3f  min %.3f  max %.3f" % (name, len(vals), st.mean(vals), st.pstdev(vals), min(vals), max(vals)))


main = [r for r in results if not (r["i"] == 8 and r["mech"] == 2)]      # conjunto de [P2]: especime 8 com mecanismo 1
say("ESTATISTICAS (especimes 1-12; especime 8 com mecanismo 1, como em [P2])")
summary("Fu,th/Fu,exp  (esta reproducao)", [r["Fu_th"] / r["Fu_exp"] for r in main])
summary("du,th/du,exp  (esta reproducao)", [r["du_th"] / r["du_exp"] for r in main])
pub_fu = [sp.published(i)["Fu_th"] / sp.published(i)["Fu_exp"] for i in range(1, 13)]
pub_du = [sp.published(i)["du_th"] / sp.published(i)["du_exp"] for i in range(1, 13)]
summary("Fu,th/Fu,exp  (Tabela 3 recalculada)", pub_fu)
summary("du,th/du,exp  (Tabela 3 recalculada)", pub_du)
say("  [P2] declara: Fu razao 0,87-1,11, media 0,98, dp 0,07; du razao 0,84-1,09, media 0,95, dp 0,09.")
say()
say("ERRO DA CURVA (modelo multilinear de 4 pontos x curva experimental digitalizada), em forca relativa, no intervalo [max(5%% du_exp; 1 mm) ; min(du_th; fim da curva exp.)]:")
summary("vies (media dos erros relativos), %", [r["bias"] for r in main])
summary("RMS relativo, %", [r["rms"] for r in main])
summary("maximo |erro| relativo, %", [r["mx"] for r in main])
summary("razao de trabalho W_th/W_exp", [r["w"] for r in main])
say("CURVA CONTINUA (mesmas equacoes aplicadas a qualquer M; ate o menor deslocamento comum, so dentro do intervalo coberto pela curva):")
summary("vies, %", [r["bias_c"] for r in main if r["bias_c"] == r["bias_c"]])
summary("RMS relativo, %", [r["rms_c"] for r in main if r["rms_c"] == r["rms_c"]])
say()
say("Mecanismo: criterio beta_u previu os mecanismos publicados? (Fig. 6: 1, 2, 3 e 6 = tipo 2; Fig. 7: demais = tipo 1)")
for i in range(1, 13):
    s = sp.specimen(i, As=math.pi * sp._G[i][0] ** 2 / 4)
    r = pt.analyse(s, As_stiffness=math.pi * s.db ** 2 / 4)
    obs = 2 if i in (1, 2, 3, 6) else 1
    say("  #%2d  beta_u=%.4f  beta_u,lim=%.4f  previsto=%d  observado=%d  %s" % (i, r.beta_u, r.beta_u_lim, r.mechanism, obs, "ok" if r.mechanism == obs else "DIVERGE"))

with open(OUT_TXT, "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")

# ---------------------------------------------------------------------------------------------------------------------
# grafico SVG (12 paineis)
# ---------------------------------------------------------------------------------------------------------------------
os.makedirs(os.path.dirname(FIG), exist_ok=True)
cols, pw, ph = 3, 330, 230
W, H = cols * pw + 20, 4 * ph + 70
svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" font-family="Segoe UI, Arial, sans-serif" font-size="11">' % (W, H, W, H),
       '<rect width="100%" height="100%" fill="#ffffff"/>',
       '<text x="10" y="18" font-size="14" font-weight="600">T-stubs de Piluso et al. (2001): experimento (digitalizado) x modelo reproduzido</text>',
       '<g transform="translate(10,28)">',
       '<line x1="0" y1="6" x2="30" y2="6" stroke="#222" stroke-width="2"/><text x="36" y="10">experimental (digitalizado da Fig. 8 de [P2])</text>',
       '<polyline points="300,6 330,6" stroke="#1f6fb5" stroke-width="2" fill="none"/><text x="336" y="10">modelo multilinear (4 pontos)</text>',
       '<line x1="545" y1="6" x2="575" y2="6" stroke="#2e8b57" stroke-width="1.5" stroke-dasharray="4 3"/><text x="581" y="10">curva continua (mesmas equacoes)</text></g>']
for idx in range(12):
    i = idx + 1
    key = (i, "mec.1 (usado na Tab.3)") if i == 8 else (i, "criterio")
    exp, ml, cont = curves_for_plot[key]
    ox, oy = 10 + (idx % cols) * pw + 42, 50 + (idx // cols) * ph + 18
    aw, ah = pw - 62, ph - 62
    xmax = max(exp[-1][0], ml[-1][0]) * 1.05
    ymax = max(max(f for _, f in exp), max(f for _, f in ml)) * 1.08
    sx, sy = lambda x: ox + aw * x / xmax, lambda y: oy + ah - ah * y / ymax
    svg.append('<rect x="%.1f" y="%.1f" width="%d" height="%d" fill="none" stroke="#999"/>' % (ox, oy, aw, ah))
    for k in range(1, 5):
        gx, gy = ox + aw * k / 5, oy + ah * k / 5
        svg.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#e5e5e5"/><line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#e5e5e5"/>' % (gx, oy, gx, oy + ah, ox, gy, ox + aw, gy))
    svg.append('<text x="%.1f" y="%.1f" font-weight="600">Espécime %d%s</text>' % (ox, oy - 5, i, " (mec. 1 como em [P2])" if i == 8 else ""))
    svg.append('<polyline fill="none" stroke="#222" stroke-width="1.8" points="%s"/>' % " ".join("%.1f,%.1f" % (sx(d), sy(f)) for d, f in exp[::3]))
    svg.append('<polyline fill="none" stroke="#2e8b57" stroke-width="1.3" stroke-dasharray="4 3" points="%s"/>' % " ".join("%.1f,%.1f" % (sx(d), sy(f)) for d, f in cont))
    svg.append('<polyline fill="none" stroke="#1f6fb5" stroke-width="1.8" points="%s"/>' % " ".join("%.1f,%.1f" % (sx(d), sy(f)) for d, f in ml))
    for d, f in ml[1:]:
        svg.append('<circle cx="%.1f" cy="%.1f" r="2.6" fill="#1f6fb5"/>' % (sx(d), sy(f)))
    svg.append('<text x="%.1f" y="%.1f" fill="#555">δ (mm)</text><text x="%.1f" y="%.1f" fill="#555">0</text><text x="%.1f" y="%.1f" fill="#555" text-anchor="end">%.0f</text>' % (ox + aw / 2 - 12, oy + ah + 27, ox - 2, oy + ah + 13, ox + aw, oy + ah + 13, xmax))
    svg.append('<text x="%.1f" y="%.1f" fill="#555" text-anchor="end">%.0f kN</text>' % (ox - 4, oy + 8, ymax))
svg.append("</svg>")
with open(FIG, "w", encoding="utf-8") as f:
    f.write("\n".join(svg))
print("grafico:", os.path.abspath(FIG))
