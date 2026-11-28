"""C7.0 (3.1): onde a area do parafuso entra no modelo e quanto importa a escolha. Compara, nos 12 especimes, as 4 combinacoes
 (area na RESISTENCIA Bu = A*fub: bruta pi db^2/4 | resistente ISO)  x  (area no alongamento delta_b: bruta | resistente ISO).
Nao escolhe: reporta. Uso: PYTHONPATH=<esta pasta> python -I c7_area_study.py"""
import math
import statistics as st

import piluso_tstub as pt
import specimens as sp

lines = []


def say(s=""):
    lines.append(s)
    print(s)


def gross(i):
    return math.pi * sp._G[i][0] ** 2 / 4


def net(i):
    return sp.AS_ISO[sp._G[i][0]]


say("Combinacoes: R=area em Bu (beta_u, F_u,2, F_u,3); E=area em delta_b (eq. 40/51: so afeta delta_y/Ki quando Ki NAO e fornecido).")
say("%-4s %-9s %-9s | %-5s %-7s | %-8s %-8s %-8s | %-9s %-9s %-8s | %-s" % ("", "R", "E", "mec", "mecTab3", "beta_u", "b/blim", "Fu(kN)", "du(mm)", "Ki(kN/mm)", "psi", "obs"))
combos = [("bruta", "bruta"), ("bruta", "ISO"), ("ISO", "bruta"), ("ISO", "ISO")]
stats = {}
for (ra, ea) in combos:
    fu_r, du_r = [], []
    changed_mech = []
    for i in range(1, 13):
        pub = sp.published(i)
        Ar = gross(i) if ra == "bruta" else net(i)
        Ae = gross(i) if ea == "bruta" else net(i)
        s = sp.specimen(i, As=Ar)
        try:
            r = pt.analyse(s, As_stiffness=Ae)
        except Exception as e:                                  # entradas invalidas nunca geram numero
            say("#%d erro: %s" % (i, e))
            continue
        mec_tab3 = 1 if i == 8 else (2 if i in (1, 2, 3, 6) else 1)
        obs = 2 if i in (1, 2, 3, 6) else 1
        if r.mechanism != obs:
            changed_mech.append(i)
        fu = r.forces.get("Fu", r.F_u3) / 1e3
        du = r.disp.get("du", float("nan"))
        fu_r.append(fu / pub["Fu_exp"])
        du_r.append(du / pub["du_exp"])
        if (ra, ea) in (("bruta", "bruta"), ("ISO", "bruta")):
            say("#%-3d %-9s %-9s | %-5d %-7d | %-8.4f %-8.3f %-8.1f | %-9.2f %-9.1f %-8.3f | %d" % (i, ra, ea, r.mechanism, mec_tab3, r.beta_u, r.beta_u / r.beta_u_lim, fu, du, r.K_initial / 1e3, r.psi, obs))
    stats[(ra, ea)] = (fu_r, du_r, changed_mech)
say()
say("RESUMO por combinacao (12 especimes; mecanismo pelo criterio beta_u, sem forcar):")
say("%-22s | %-34s | %-34s | %s" % ("R / E", "Fu_th/Fu_exp  media  dp  min-max", "du_th/du_exp  media  dp  min-max", "mecanismo previsto != observado"))
for k, (fu, du, ch) in stats.items():
    say("%-22s | %6.3f %6.3f  %5.3f-%5.3f          | %6.3f %6.3f  %5.3f-%5.3f          | %s" % ("%s / %s" % k, st.mean(fu), st.pstdev(fu), min(fu), max(fu), st.mean(du), st.pstdev(du), min(du), max(du), ch))
say("[P2] publicou: Fu media 0,98 dp 0,07 (0,87-1,11); du media 0,95 dp 0,09 (0,84-1,09).")
say()
say("Efeito do alongamento (E) com Ki FORNECIDO: nenhum. Com Ki fornecido, delta_y = Fy/Ki e delta_e = F/Ki (eq. 41-46): as eq. (38)-(40)/(51) e a area de alongamento nao sao usadas.")
say("Variacao relativa de Ki (Piluso) ao trocar a area de alongamento (bruta -> ISO), para referencia:")
for i in (1, 4, 7, 12):
    s = sp.specimen(i, As=gross(i))
    a = pt.analyse(s, As_stiffness=gross(i), force_mechanism=1 if i == 8 else None)
    b = pt.analyse(s, As_stiffness=net(i), force_mechanism=1 if i == 8 else None)
    say("  #%2d  Ki(bruta)=%.1f  Ki(ISO)=%.1f kN/mm  (%+.2f%%)  delta_y %.3f -> %.3f mm" % (i, a.K_initial / 1e3, b.K_initial / 1e3, 100 * (b.K_initial / a.K_initial - 1), a.disp["dy"], b.disp["dy"]))
open("saida_c7_area.txt", "w", encoding="utf-8").write("\n".join(lines) + "\n")
