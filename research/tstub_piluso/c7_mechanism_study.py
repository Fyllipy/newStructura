"""C7.0 (3.2): comportamento numerico do criterio de mecanismo (beta_u, beta_u,lim), a condicao psi<=1 do mecanismo 2, o espécime 8, margens de cada
espécime e consequencia de classificar errado. Nao altera o criterio. Uso: PYTHONPATH=<esta pasta> python -I c7_mechanism_study.py"""
import dataclasses
import math

import piluso_tstub as pt
import specimens as sp

lines = []


def say(s=""):
    lines.append(s)
    print(s)


def gross(i):
    return math.pi * sp._G[i][0] ** 2 / 4


say("A) Condicao de validade do mecanismo 2: o momento no eixo do parafuso e psi*Mu; a formulacao exige psi <= 1 (rotulas (60)-(62) definidas para xi=psi <= 1).")
say("   psi = (2 - beta_u) lambda / (beta_u (1+lambda))  [P1 (49)].  psi = 1  <=>  beta_u = 2 lambda/(1+2 lambda) =: beta_psi (independe de d_w).")
say("   beta_u,lim [P1 (11)] = beta_psi * [1 - (1+lambda) dw/(8n)]  <  beta_psi  quando dw > 0  =>  existe uma JANELA  beta_u,lim < beta_u < beta_psi  em que o criterio (11) manda")
say("   usar o mecanismo 2, mas (49) da psi > 1 (a rotula do eixo do parafuso excederia Mu: premissa do mecanismo 2 violada).")
say("%-4s %-7s %-9s %-9s %-9s | %-9s %-7s %-12s | %-7s %-s" % ("#", "lambda", "b_u,lim", "b_psi", "beta_u", "b_u/lim", "psi", "janela?", "mec", "obs"))
for i in range(1, 13):
    s = sp.specimen(i, As=gross(i))
    r = pt.analyse(s, As_stiffness=gross(i), force_mechanism=None)
    lam = s.n / s.m
    bpsi = 2 * lam / (1 + 2 * lam)
    window = r.beta_u_lim < r.beta_u < bpsi
    psi = (2 - r.beta_u) * lam / (r.beta_u * (1 + lam)) if r.beta_u <= 2 else float("nan")
    obs = 2 if i in (1, 2, 3, 6) else 1
    say("%-4d %-7.3f %-9.4f %-9.4f %-9.4f | %-9.3f %-7.3f %-12s | %-7d %d" % (i, lam, r.beta_u_lim, bpsi, r.beta_u, r.beta_u / r.beta_u_lim, psi, "DENTRO (psi>1)" if window else "-", r.mechanism, obs))
say("   -> o espécime 8 e o UNICO dentro da janela (psi=1,177): mecanismo 2 pelo criterio (11), psi>1 por (49); observado: tipo 1; a Tabela 3 de [P2] usa o tipo 1.")
say()
say("B) O que o PROTOTIPO Python faz hoje com psi > 1 (espécime 8, mecanismo 2 pelo criterio):")
s8 = sp.specimen(8, As=gross(8))
mat = pt.material_of(s8)
r8 = pt.analyse(s8, As_stiffness=gross(8))
xi_b = r8.psi * 1.0
say("   psi=%.4f -> xi no eixo do parafuso (ultimo ponto) = psi*1 = %.4f > 1 (dominio de G e do modelo M-chi: xi <= 1)" % (r8.psi, xi_b))
say("   chi_of_xi(xi=%.4f)/chi_y = %.3f ; c_u (limite do material) = %.3f  -> a inversao numerica fica TRUNCADA em c_u (valor silencioso, fora do dominio)" % (xi_b, pt.chi_of_xi(mat, xi_b), mat.c_u))
say("   Resultado do Python (mec. 2, psi>1): Fu=%.1f kN, du=%.2f mm  -- numeros sem fundamento no artigo (dominio violado)." % (r8.forces["Fu"] / 1e3, r8.disp["du"]))
r8a = pt.analyse(s8, As_stiffness=gross(8), force_mechanism=1)
say("   Mecanismo 1 (Tabela 3 de [P2]): Fu=%.2f kN, du=%.2f mm; publicado Fu,th=385,99 kN, du,th=45,19 mm; experimental Fu=348,75 kN, du=47,61 mm." % (r8a.forces["Fu"] / 1e3, r8a.disp["du"]))
say("   => DIVERGENCIA do Python com a sua propria documentacao: o relatorio da Fase C descreve o espécime 8 como 'o criterio previu 2, observado 1', sem registrar que o mecanismo 2 foi")
say("      calculado com psi>1 (fora do dominio). Os numeros do mecanismo 2 do espécime 8 (319,4 kN; 41,2 mm) NAO devem ser usados como referencia.")
say()
say("C) Margens (distancia ao limite) e cobertura experimental: beta_u/beta_u,lim dos 12 espécimes (area bruta):")
ratios = {}
for i in range(1, 13):
    s = sp.specimen(i, As=gross(i))
    r = pt.analyse(s, As_stiffness=gross(i))
    ratios[i] = r.beta_u / r.beta_u_lim
t1 = sorted((v, i) for i, v in ratios.items() if i not in (1, 2, 3, 6, 8))
t2 = sorted((v, i) for i, v in ratios.items() if i in (1, 2, 3, 6))
say("   observados tipo 1 (7 espécimes + o 8): faixa de b_u/lim = %.3f-%.3f (#%d-#%d); tipo 2 (4 espécimes): %.3f-%.3f (#%d-#%d); #8: %.3f" % (t1[0][0], t1[-1][0], t1[0][1], t1[-1][1], t2[0][0], t2[-1][0], t2[0][1], t2[-1][1], ratios[8]))
say("   => NENHUM espécime ensaiado tem b_u/lim entre %.3f e %.3f: a regiao (%.2f ; %.2f) nao tem cobertura experimental." % (t1[-1][0], t2[0][0], t1[-1][0], t2[0][0]))
say("   Variacao relativa de beta_u necessaria para trocar o mecanismo: tipo 1: +%.0f%% a +%.0f%%; tipo 2: -%.0f%% a -%.0f%% (beta_u ~ Mu/(Bu m): a mesma ordem de erro em f_y, f_ub, area do parafuso ou m)." % (
    100 * (1 / t1[-1][0] - 1), 100 * (1 / t1[0][0] - 1), 100 * (1 - 1 / t2[0][0]), 100 * (1 - 1 / t2[-1][0])))
say()
say("D) Consequencia de classificar errado (mecanismo alternativo FORCADO; a curva/pontos mudam de quanto?):")
say("%-4s %-5s | %-24s | %-24s | %-18s | %-s" % ("#", "mec", "Fu (crit)  Fu (alt)  dif", "du (crit)  du (alt)  dif", "psi(mec 2)", "nota"))
for i in range(1, 13):
    s = sp.specimen(i, As=gross(i))
    rc = pt.analyse(s, As_stiffness=gross(i))
    alt = 1 if rc.mechanism == 2 else 2
    try:
        ra = pt.analyse(s, As_stiffness=gross(i), force_mechanism=alt)
    except Exception as e:
        say("#%d erro %s" % (i, e)); continue
    fu_c, fu_a = rc.forces["Fu"] / 1e3, ra.forces["Fu"] / 1e3
    du_c, du_a = rc.disp["du"], ra.disp["du"]
    psi = rc.psi if rc.mechanism == 2 else ra.psi
    say("%-4d %-5d | %7.1f %7.1f %+7.1f%% | %7.2f %7.2f %+7.1f%% | %-18.3f | %s" % (i, rc.mechanism, fu_c, fu_a, 100 * (fu_a / fu_c - 1), du_c, du_a, 100 * (du_a / du_c - 1), psi, "psi>1" if psi > 1 else ""))
say("   (a coluna 'alt' usa o mecanismo NAO escolhido pelo criterio; a diferenca mede o quanto uma classificacao errada altera Fu e du)")
say()
say("E) Continuidade na transicao: varrendo a area da armadura continuamente (espécime 7, so para analise), Fu e du ao cruzar b_u,lim e b_psi:")
base = sp.specimen(7, As=gross(7))
rows = []
for k in range(0, 41):
    f = 0.30 + 0.0175 * k                       # fator sobre a area do parafuso
    s = dataclasses.replace(base, As=gross(7) * f)
    try:
        r = pt.analyse(s, As_stiffness=gross(7))
    except Exception:
        continue
    lam = s.n / s.m
    bpsi = 2 * lam / (1 + 2 * lam)
    rows.append((f, r.beta_u, r.mechanism, r.forces.get("Fu", float("nan")) / 1e3, r.disp.get("du", float("nan")), r.beta_u < bpsi and r.mechanism == 2))
prev = None
for row in rows:
    if prev and prev[2] != row[2]:
        say("   mudanca de mecanismo entre f=%.3f (mec %d: beta=%.4f Fu=%.1f du=%.2f) e f=%.3f (mec %d: beta=%.4f Fu=%.1f du=%.2f): salto Fu=%+.2f%%, du=%+.2f%%" % (
            prev[0], prev[2], prev[1], prev[3], prev[4], row[0], row[2], row[1], row[3], row[4], 100 * (row[3] / prev[3] - 1), 100 * (row[4] / prev[4] - 1)))
    prev = row
say("   (Fu e continuo na transicao por construcao: (11) vem de Fu1=Fu2; du pode ter salto porque os dois mecanismos usam formulas de deslocamento diferentes.)")
open("saida_c7_mecanismo.txt", "w", encoding="utf-8").write("\n".join(lines) + "\n")
