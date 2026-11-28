"""Fase C2: compara a rigidez inicial de Piluso/Faella (eq. 38-41 de [P1]) com a formulacao EN 1993-1-8 Tab. 6.11 da Fase B do Structura,
aplicadas a geometria dos 12 espécimes de [P2]. NAO ha rigidez experimental tabulada em [P2]; esta e uma comparacao ENTRE FORMULACOES.

Modelo EN do espécime (duas abas T em serie, parafusos uma vez):  delta = F/E * (2/k4 + 1/k10),  k4 = 0,9 leff tf^3/m^3,  k10 = 1,6 As/Lb.
Hipoteses (sao HIPOTESES, nao dados de [P2]): leff da EN = min(2 pi m ; 4 m + 1,25 e) com e = n (aresta de aplicacao do 'prying' de [P1]); alternativa leff = beff.
Uso: PYTHONPATH=<esta pasta> python -I stiffness_compare.py
"""
import math
import os

import piluso_tstub as pt
import specimens as sp

HERE = os.path.dirname(os.path.abspath(__file__))
lines = []


def say(s=""):
    lines.append(s)
    print(s)


E = pt.E_DEFAULT
say("%2s | %8s %8s %8s | %8s %8s %8s | %7s %7s | %7s %7s | %7s" % ("#", "Ki_P", "Kfl_P", "Kb_P", "Ki_EN1", "Ki_EN2", "Ki_EN3", "EN1/P", "EN2/P", "leffEN", "beff", "leff/beff*"))
say("   | kN/mm: Piluso total, so flange (2 abas), so parafuso | EN: (1) leff=min(2pi m;4m+1,25n), As res.; (2) leff=beff, As res.; (3) leff=EN, As bruta")
rows = []
for i in range(1, 13):
    s = sp.specimen(i, As=math.pi * sp._G[i][0] ** 2 / 4)
    r = pt.analyse(s, As_stiffness=math.pi * s.db ** 2 / 4)
    Ki = r.K_initial
    K, mp, zeta = pt.stiffness_K(s)
    kfl_P = K / 2.0                                   # 2 abas em serie: F/delta_flange = K/2
    Ab_gross = math.pi * s.db ** 2 / 4
    Lb = 2 * s.tf + (s.th + s.tn) / 2
    by_over_F = 0.5 + (r.My / r.forces["Fy"]) / s.n if r.mechanism == 1 else 0.5 + r.psi * (r.My / r.forces["Fy"]) / s.n
    kb_P = E * Ab_gross / Lb / by_over_F             # rigidez do parafuso vista pela forca F (delta_b = By/(E Ab/Lb), By = F*by_over_F)
    leff_en = min(2 * math.pi * s.m, 4 * s.m + 1.25 * s.n)
    As_res = sp.AS_ISO[s.db]

    def k_en(leff, As):
        k4 = 0.9 * leff * s.tf ** 3 / s.m ** 3
        k10 = 1.6 * As / Lb
        return E / (2.0 / k4 + 1.0 / k10) / E * E / 1.0          # F/delta em N/mm: 1/(2/(E k4) + 1/(E k10))

    def stiff(leff, As):
        k4 = 0.9 * leff * s.tf ** 3 / s.m ** 3
        k10 = 1.6 * As / Lb
        return 1.0 / (2.0 / (E * k4) + 1.0 / (E * k10))

    en1 = stiff(leff_en, As_res)
    en2 = stiff(s.effective_width(), As_res)
    en3 = stiff(leff_en, Ab_gross)
    rows.append((i, Ki, kfl_P, kb_P, en1, en2, en3, leff_en, s.effective_width()))
    say("%2d | %8.1f %8.1f %8.1f | %8.1f %8.1f %8.1f | %7.2f %7.2f | %7.1f %7.1f | %7.3f" % (i, Ki / 1e3, kfl_P / 1e3, kb_P / 1e3, en1 / 1e3, en2 / 1e3, en3 / 1e3, en1 / Ki, en2 / Ki, leff_en, s.effective_width(), leff_en / s.effective_width()))
say("   * leff/beff: relacao entre o comprimento efetivo EN adotado e a largura efetiva de [P1]")
say()
import statistics as st
say("EN(1)/Piluso: media %.2f  min %.2f  max %.2f   | EN(2)/Piluso: media %.2f  min %.2f  max %.2f   | EN(3)/Piluso: media %.2f" % (
    st.mean(r[4] / r[1] for r in rows), min(r[4] / r[1] for r in rows), max(r[4] / r[1] for r in rows),
    st.mean(r[5] / r[1] for r in rows), min(r[5] / r[1] for r in rows), max(r[5] / r[1] for r in rows), st.mean(r[6] / r[1] for r in rows)))
say()
say("Equivalencia so da flange (sem parafuso): Piluso 0,25 E beff tf^3/m'^3 por 2 abas  vs  EN 0,45 E leff tf^3/m^3  =>  leff_equivalente = 0,556 beff (m/m')^3")
for i in (1, 4, 7, 12):
    s = sp.specimen(i, As=math.pi * sp._G[i][0] ** 2 / 4)
    K, mp, zeta = pt.stiffness_K(s)
    say("  #%2d: m=%.2f  m'=%.3f  (m/m')^3=%.3f  leff_equiv=%.1f mm  beff=%.1f mm  leff_EN(1)=%.1f mm" % (i, s.m, mp, (s.m / mp) ** 3, 0.5556 * s.effective_width() * (s.m / mp) ** 3, s.effective_width(), min(2 * math.pi * s.m, 4 * s.m + 1.25 * s.n)))
with open(os.path.join(HERE, "saida_stiffness_compare.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")

# --- efeito de trocar Ki (Piluso) por Ki (EN, variante 2) sobre os deslocamentos caracteristicos (alternativa B: deformacao plastica de [P1] + rigidez EN)
say()
say("EFEITO NA CURVA se Ki de [P1] for substituida pela rigidez EN (variante 2: leff = beff): delta = F/Ki + 2 delta_p  (delta_p de [P1] inalterado)")
say("%2s | %8s %8s | %8s %8s %8s | %8s %8s" % ("#", "du_P", "du_EN2", "dif_du", "elast/du", "dy_P", "dy_EN2", "dh_P"))
for (i, Ki, kfl, kb, en1, en2, en3, leff_en, beff) in rows:
    s = sp.specimen(i, As=math.pi * sp._G[i][0] ** 2 / 4)
    r = pt.analyse(s, As_stiffness=math.pi * s.db ** 2 / 4, force_mechanism=(1 if i == 8 else None))
    dpu2 = r.disp["du"] - r.forces["Fu"] / r.K_initial          # 2 delta_pu (parte plastica)
    du_en = r.forces["Fu"] / en2 + dpu2
    dy_en = r.forces["Fy"] / en2
    say("%2d | %8.2f %8.2f | %+7.1f%% %7.1f%% %8.3f %8.3f %8.2f" % (i, r.disp["du"], du_en, 100 * (du_en / r.disp["du"] - 1), 100 * (r.forces["Fu"] / r.K_initial) / r.disp["du"], r.disp["dy"], dy_en, r.disp["dh"]))
with open(os.path.join(HERE, "saida_stiffness_compare.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
