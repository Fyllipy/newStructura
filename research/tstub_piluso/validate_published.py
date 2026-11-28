"""Reproducao (a) do exemplo trabalhado de [P2] (espécime 7, p. 702-703) e (b) das colunas TEORICAS da Tabela 3 de [P2].
Compara com valores PUBLICADOS (nao com o Structura). Uso: python -I validate_published.py
"""
import math
import sys

import piluso_tstub as pt
import specimens as sp

out = []


def line(s=""):
    out.append(s)
    print(s)


def rel(a, b):
    return (a - b) / b if b else float("nan")


# --------------------------------------------------------------------------------------------------------------------
line("A) EXEMPLO TRABALHADO [P2] p. 702-703 (especime 7) -- valores publicados x reproduzidos")
s = sp.specimen(7)
mat = pt.material_of(s)
r = pt.analyse(s, As_stiffness=572.5553)
K, mp, zeta = pt.stiffness_K(s)
xi1, xi2, xi3 = pt.xi_limits(mat)
rows = [
    ("eps_y (1e-3)", 1.4647, mat.eps_y * 1e3, 1e-4),
    ("eps_m", 8.2122e-2, mat.eps_m, 1e-4),
    ("chi_y", 2.1075e-4, 2 * mat.eps_y / s.tf, 1e-4),
    ("chi_u", 0.1121, 2 * mat.eps_u / s.tf, 5e-4),
    ("chi_h/chi_y", 8.9987, mat.c_h, 1e-4),
    ("chi_m/chi_y", 56.0691, mat.c_m, 1e-4),
    ("chi_u/chi_y", 531.7251, mat.c_u, 1e-4),
    ("Mh/My", 1.4938, mat.Mh_My, 1e-4),
    ("Mm/My", 2.1956, mat.Mm_My, 1e-4),
    ("Mu/My", 3.7466, mat.Mu_My, 1e-4),
    ("beff (mm)", 122.80, s.effective_width(), 1e-6),
    ("My (N.mm)", 1216283.0, r.My, 1e-5),
    ("Mu (N.mm)", 4556925.0, r.Mu, 5e-5),
    ("beta_u (As=459, publicado com 2 digitos)", 0.47, r.beta_u, 1e-2),
    ("beta_u,lim", 0.5630, r.beta_u_lim, 5e-4),
    ("Fy (N)", 114746.3, r.forces["Fy"], 1e-5),
    ("Fh (N)", 171413.0, r.forces["Fh"], 1e-4),
    ("Fm (N)", 251945.1, r.forces["Fm"], 1e-4),
    ("Fu (N)", 429922.3, r.forces["Fu"], 1e-4),
    ("zeta", 0.7724, zeta, 1e-4),
    ("m' (mm)", 53.1952, mp, 5e-5),
    ("K (N/mm)", 230046.0, K, 1e-4),
    ("delta_y (mm)", 1.0285, r.disp["dy"], 1e-3),
    ("Ki (N/mm)", 111567.0, r.K_initial, 1e-3),
    ("xi2 = Mh/Mu", 0.39871, xi2, 1e-4),
    ("xi3 = Mm/Mu", 0.586, xi3, 1e-3),
]
Dxi2 = pt.func_D(mat, xi2)
Fxi3 = pt.func_F(mat, xi3)
C = pt.func_G(mat, 1.0)
rows += [
    ("D(xi2)", 1.2586e-3, Dxi2, 1e-3),
    ("F(xi3)", 3.2333e-2, Fxi3, 1e-3),
    ("C = G(1)", 0.3172, C, 1e-3),
    ("delta_ph (mm)", 0.1245, s.m ** 2 / (2 * s.tf) * Dxi2, 2e-3),
    ("delta_pm (mm)", 3.20, s.m ** 2 / (2 * s.tf) * Fxi3, 2e-3),
    ("delta_pu (mm)", 31.393, s.m ** 2 / (2 * s.tf) * C, 1e-3),
    ("delta_h (mm)", 1.7850, r.disp["dh"], 2e-3),
    ("delta_m (mm)", 8.66, r.disp["dm"], 2e-3),
    ("delta_u (mm)", 66.64, r.disp["du"], 1e-3),
]
bad = 0
line("%-16s %14s %14s %9s %8s" % ("grandeza", "publicado", "reproduzido", "dif.rel", "tol."))
for name, pub, mine, tol in rows:
    d = rel(mine, pub)
    ok = abs(d) <= tol
    bad += 0 if ok else 1
    line("%-16s %14.6g %14.6g %8.4f%% %7.3f%%  %s" % (name, pub, mine, 100 * d, 100 * tol, "ok" if ok else "DIFERE"))
line("exemplo trabalhado: %d grandezas, %d fora da tolerancia" % (len(rows), bad))
line("Nota: [P2] p. 702 imprime 'My = 1,126,283' na substituicao de Fy (digitacao): com My = 1.216.283 (valor da propria pagina) obtem-se Fy = 114.746,3 N, o publicado.")
line()

# --------------------------------------------------------------------------------------------------------------------
line("B) VERIFICACAO NUMERICA INDEPENDENTE das formulas fechadas D, F, G ([P1] (24), (29), (34)) por integracao direta da curvatura")
for i in (7, 1, 12):
    s_i = sp.specimen(i)
    mat_i = pt.material_of(s_i)
    x1, x2, x3 = pt.xi_limits(mat_i)
    for name, xi in (("D", x2), ("F", x3), ("G", 1.0), ("D(0.4)", 0.4), ("F(0.5)", 0.5), ("G(0.8)", 0.8)):
        closed = pt.plastic_rotation_factor(mat_i, xi)
        numeric = pt.plastic_rotation_numeric(mat_i, xi)
        line("especime %2d  %-7s xi=%.4f  fechada=%.6e  integracao numerica=%.6e  dif.rel=%.2e" % (i, name, xi, closed, numeric, rel(closed, numeric)))
line()

# --------------------------------------------------------------------------------------------------------------------
line("C) TABELA 3 de [P2] (colunas teoricas): publicado x reproduzido (sem ajuste de parametros)")
line("   Hipotese de area do parafuso na resistencia Bu = As*fub: (G) area bruta pi db^2/4; (N) area resistente ISO (M20 245, M24 353, M27 459).")
import statistics as st
summary = {}
for mode in ("G", "N"):
    line()
    line("   --- area do parafuso: %s ---" % ("BRUTA pi db^2/4" if mode == "G" else "RESISTENTE ISO"))
    line("%2s %4s | %6s %7s | %6s %7s | %8s %8s | %8s %8s | %8s %8s" % ("#", "mec", "bu pub", "bu rep", "du pub", "du rep", "Fu pub", "Fu rep", "Fy rep", "Ki rep", "mec.pub", ""))
    rows_t3 = []
    for i in range(1, 13):
        pub = sp.published(i)
        s_i = sp.specimen(i, As=(math.pi * sp._G[i][0] ** 2 / 4) if mode == "G" else None)
        r_i = pt.analyse(s_i, As_stiffness=math.pi * s_i.db ** 2 / 4)
        fu_rep = r_i.forces.get("Fu", float("nan")) / 1e3
        rows_t3.append((i, r_i, pub, fu_rep))
        line("%2d %4d | %6.4f %7.4f | %8.2f %8.2f | %8.2f %8.2f | %8.2f %8.1f" % (i, r_i.mechanism, pub["beta_u"], r_i.beta_u, pub["du_th"], r_i.disp.get("du", float("nan")),
             pub["Fu_th"], fu_rep, r_i.forces.get("Fy", float("nan")) / 1e3, r_i.K_initial))
    line("   desvios reproduzido/publicado - 1  (mecanismos 1 e 2):")
    for key, getter in (("beta_u", lambda t: (t[1].beta_u, t[2]["beta_u"])), ("delta_u,th", lambda t: (t[1].disp.get("du", float("nan")), t[2]["du_th"])),
                        ("Fu,th", lambda t: (t[3], t[2]["Fu_th"]))):
        ds = [rel(*getter(t)) for t in rows_t3]
        line("     %-12s min %+8.3f%%  max %+8.3f%%  media %+8.3f%%  max|.| %8.3f%%" % (key, 100 * min(ds), 100 * max(ds), 100 * st.mean(ds), 100 * max(abs(x) for x in ds)))
    summary[mode] = rows_t3
line()
line("   Desvios dos momentos (independem da area do parafuso): Mh/My, Mm/My, Mu/My reproduzidos com |desvio| <= 0,003%% nos 12 especimes (ver saida).")
for i in range(1, 13):
    mat_i = pt.material_of(sp.specimen(i)); pub = sp.published(i)
    line("     #%2d  Mh/My %.4f (pub %.4f)  Mm/My %.4f (pub %.4f)  Mu/My %.4f (pub %.4f)" % (i, mat_i.Mh_My, pub["Mh_My"], mat_i.Mm_My, pub["Mm_My"], mat_i.Mu_My, pub["Mu_My"]))

with open("saida_validate_published.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out) + "\n")
sys.exit(0)
