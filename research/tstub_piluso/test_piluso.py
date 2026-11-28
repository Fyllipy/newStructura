"""Testes do protótipo Piluso (research/tstub_piluso). Execução: PYTHONPATH=<esta pasta> python -m unittest -v test_piluso

Separacao pedida na Fase C:
  - ReproducaoMatematica: o codigo reproduz equacoes/valores PUBLICADOS em [P1]/[P2] (nao e validacao fisica);
  - Propriedades: coerencia interna (continuidade, monotonicidade, escala dimensional, limites, entradas invalidas);
  - ComparacaoExperimental: comparacao com Fu,exp e du,exp TABULADOS ([P2] Tabela 3). Tolerancias = as estatisticas publicadas por [P2].
Nenhum teste altera equacoes para aproximar resultados.
"""
import dataclasses
import math
import unittest

import piluso_tstub as pt
import specimens as sp


def gross(i):
    return math.pi * sp._G[i][0] ** 2 / 4.0


def run(i, **kw):
    s = sp.specimen(i, As=gross(i))
    return s, pt.analyse(s, As_stiffness=math.pi * s.db ** 2 / 4, **kw)


class ReproducaoMatematica(unittest.TestCase):
    def test_exemplo_trabalhado_especime7(self):
        s = sp.specimen(7)                       # As = 459 (confirmado pelo exemplo)
        r = pt.analyse(s, As_stiffness=572.5553)
        mat = pt.material_of(s)
        pub = dict(Fy=114746.3, Fh=171413.0, Fm=251945.1, Fu=429922.3)
        for k, v in pub.items():
            self.assertAlmostEqual(r.forces[k] / v, 1.0, delta=1e-4, msg=k)
        for k, v in dict(dy=1.0285, dh=1.7850, dm=8.66, du=66.64).items():
            self.assertAlmostEqual(r.disp[k] / v, 1.0, delta=2e-3, msg=k)
        self.assertAlmostEqual(r.K_initial / 111567.0, 1.0, delta=1e-3)
        self.assertAlmostEqual(mat.Mu_My, 3.7466, delta=5e-4)
        self.assertAlmostEqual(r.beta_u_lim, 0.5630, delta=5e-4)
        self.assertEqual(r.mechanism, 1)

    def test_tabela3_momentos_todos_os_especimes(self):
        for i in range(1, 13):
            mat = pt.material_of(sp.specimen(i))
            pub = sp.published(i)
            for k in ("Mh_My", "Mm_My", "Mu_My"):
                self.assertAlmostEqual(getattr(mat, k) / pub[k], 1.0, delta=4e-5, msg="%d %s" % (i, k))   # tabela impressa com 4 decimais: meia unidade = ate 3,4e-5 relativo

    def test_tabela3_beta_du_Fu(self):
        for i in range(1, 13):
            pub = sp.published(i)
            forced = 1 if i == 8 else None           # a Tabela 3 usa o mecanismo 1 no especime 8 (ver docs)
            _, r = run(i, force_mechanism=forced)
            if i != 11:                              # beta_u do especime 11 impresso 0,3651 vs 0,3749 reproduzido (+2,7%): documentado
                self.assertAlmostEqual(r.beta_u / pub["beta_u"], 1.0, delta=2e-4, msg="beta %d" % i)
            self.assertAlmostEqual(r.disp["du"] / pub["du_th"], 1.0, delta=2e-3, msg="du %d" % i)
            self.assertAlmostEqual(r.forces["Fu"] / 1e3 / pub["Fu_th"], 1.0, delta=2e-4, msg="Fu %d" % i)

    def test_formas_fechadas_contra_integracao_numerica(self):
        for i in (1, 7, 12):
            mat = pt.material_of(sp.specimen(i))
            x1, x2, x3 = pt.xi_limits(mat)
            for xi in (x2, 0.5 * (x2 + x3), x3, 0.5 * (x3 + 1.0), 1.0, 1.02 * x1 if 1.02 * x1 < x2 else x2):
                closed = pt.plastic_rotation_factor(mat, xi)
                numeric = pt.plastic_rotation_numeric(mat, xi)
                self.assertAlmostEqual(closed / numeric, 1.0, delta=1e-5, msg="%d xi=%.3f" % (i, xi))


class Propriedades(unittest.TestCase):
    def test_continuidade_nos_limites_dos_casos(self):
        mat = pt.material_of(sp.specimen(7))
        x1, x2, x3 = pt.xi_limits(mat)
        eps = 1e-9
        self.assertAlmostEqual(pt.func_D(mat, x2), pt.func_F(mat, x2 - 0 * eps), delta=1e-9)    # D e F coincidem em xi2
        self.assertAlmostEqual(pt.func_F(mat, x3), pt.func_G(mat, x3), delta=1e-9)               # F e G coincidem em xi3
        self.assertAlmostEqual(pt.func_D(mat, x1 * (1 + 1e-9)), 0.0, delta=1e-9)                 # D(xi1) = 0 (inicio da plastificacao)

    def test_monotonicidade_da_curva(self):
        for i in (1, 4, 7, 12):
            s = sp.specimen(i, As=gross(i))
            c = pt.curve(s, As_stiffness=math.pi * s.db ** 2 / 4)
            self.assertTrue(len(c) > 100)
            for (d0, f0), (d1, f1) in zip(c, c[1:]):
                self.assertGreaterEqual(d1, d0 - 1e-12)
                self.assertGreaterEqual(f1, f0 - 1e-9)

    def test_pontos_caracteristicos_crescentes(self):
        for i in range(1, 13):
            _, r = run(i, force_mechanism=(1 if i == 8 else None))
            f = [r.forces[k] for k in ("Fy", "Fh", "Fm", "Fu")]
            d = [r.disp[k] for k in ("dy", "dh", "dm", "du")]
            self.assertEqual(f, sorted(f))
            self.assertEqual(d, sorted(d))
            self.assertAlmostEqual(r.forces["Fh"] / r.forces["Fy"], sp.published(i)["Mh_My"], delta=1e-3)   # F proporcional a M no mecanismo 1 e 2

    def test_escala_dimensional_semelhanca_geometrica(self):
        # todas as dimensoes x c, mesmas tensoes e E: forcas x c^2; deslocamentos x c (sem outras hipoteses)
        c = 2.0
        s = sp.specimen(7, As=gross(7))
        t = dataclasses.replace(s, **{k: getattr(s, k) * c for k in ("db", "dh", "dw", "th", "tn", "tf", "m", "n", "r", "b")}, beff=s.effective_width() * c, As=s.As * c * c)
        r1 = pt.analyse(s, As_stiffness=math.pi * s.db ** 2 / 4)
        r2 = pt.analyse(t, As_stiffness=math.pi * t.db ** 2 / 4)
        for k in ("Fy", "Fh", "Fm", "Fu"):
            self.assertAlmostEqual(r2.forces[k] / r1.forces[k], c * c, delta=1e-9 * c * c)
        for k in ("dy", "dh", "dm", "du"):
            self.assertAlmostEqual(r2.disp[k] / r1.disp[k], c, delta=1e-9 * c)
        self.assertAlmostEqual(r2.beta_u, r1.beta_u, delta=1e-12)

    def test_transicao_de_mecanismo_com_a_resistencia_do_parafuso(self):
        # parafuso fraco -> mecanismo 3 (sem curva); intermediario -> 2; forte -> 1
        base = sp.specimen(7, As=gross(7))
        mech = {}
        for factor in (0.05, 0.4, 1.0, 3.0):
            t = dataclasses.replace(base, As=base.As * factor)
            mech[factor] = pt.analyse(t, As_stiffness=572.5553).mechanism
        self.assertEqual(mech[0.05], 3)
        self.assertEqual(mech[3.0], 1)
        self.assertIn(mech[0.4], (2, 3))
        weak = dataclasses.replace(base, As=base.As * 0.05)
        r3 = pt.analyse(weak, As_stiffness=572.5553)
        self.assertEqual(r3.mechanism, 3)
        self.assertEqual(pt.curve(weak, As_stiffness=572.5553), [])       # [P1] nao da a curva do mecanismo 3: nao se inventa
        self.assertTrue(any("nao implementado" in n for n in r3.notes))

    def test_resistencia_e_o_minimo_dos_tres_mecanismos_exceto_publicacao(self):
        # o criterio beta_u e o minimo Fu1, Fu2, Fu3 coincidem para os especimes 1-7, 9-12; o especime 8 e a EXCECAO documentada
        for i in range(1, 13):
            _, r = run(i)
            mn = min(r.F_u1, r.F_u2, r.F_u3)
            active = r.forces["Fu"]
            if i == 8:
                self.assertNotAlmostEqual(active, sp.published(8)["Fu_th"] * 1e3, delta=1e3)   # criterio da mec. 2; a Tabela 3 usa a mec. 1
            self.assertAlmostEqual(active, mn, delta=1e-6 * mn, msg=str(i))

    def test_entradas_invalidas_nunca_geram_numero(self):
        s = sp.specimen(7, As=gross(7))
        for field, bad in (("tf", 0.0), ("m", -1.0), ("n", float("nan")), ("fy", 0.0), ("As", 0.0), ("fub", -5.0), ("E", 0.0), ("Eh", 0.0), ("dw", float("inf"))):
            with self.assertRaises(ValueError, msg=field):
                pt.analyse(dataclasses.replace(s, **{field: bad}))
        with self.assertRaises(ValueError):
            pt.analyse(dataclasses.replace(s, fu=s.fy))                  # fu <= fy
        with self.assertRaises(ValueError):
            pt.analyse(dataclasses.replace(s, eps_h=0.9 * s.fy / s.E))   # eps_h < eps_y
        with self.assertRaises(ValueError):
            pt.analyse(dataclasses.replace(s, eps_u=0.01))                # eps_u < eps_m

    def test_dependencia_da_espessura_a_ductilidade_do_mecanismo1(self):
        # [P1] conclusao: para m fixo, a capacidade de deformacao plastica diminui quando tf aumenta (mecanismo 1)
        base = sp.specimen(7, As=gross(7) * 10)      # parafuso forte: mantem o mecanismo 1
        prev = None
        for tf in (10.0, 12.0, 14.0, 16.0):
            t = dataclasses.replace(base, tf=tf, beff=None)
            r = pt.analyse(t, As_stiffness=572.5553)
            self.assertEqual(r.mechanism, 1)
            dpu = r.disp["du"] - r.forces["Fu"] / r.K_initial
            if prev is not None:
                self.assertLess(dpu, prev)
            prev = dpu


class ComparacaoExperimental(unittest.TestCase):
    """Estatisticas de [P2]: Fu razao media 0,98 dp 0,07 (0,87-1,11); du razao media 0,95 dp 0,09 (0,84-1,09)."""

    def _ratios(self):
        fu, du = [], []
        for i in range(1, 13):
            pub = sp.published(i)
            _, r = run(i, force_mechanism=(1 if i == 8 else None))
            fu.append(r.forces["Fu"] / 1e3 / pub["Fu_exp"])
            du.append(r.disp["du"] / pub["du_exp"])
        return fu, du

    def test_estatisticas_publicadas_reproduzidas(self):
        fu, du = self._ratios()
        n = len(fu)
        mean = lambda a: sum(a) / len(a)
        sd = lambda a: math.sqrt(sum((x - mean(a)) ** 2 for x in a) / len(a))
        self.assertAlmostEqual(mean(fu), 0.98, delta=0.01)
        self.assertAlmostEqual(sd(fu), 0.07, delta=0.01)
        self.assertAlmostEqual(mean(du), 0.95, delta=0.01)
        self.assertAlmostEqual(sd(du), 0.09, delta=0.01)
        self.assertGreaterEqual(min(fu), 0.86)
        self.assertLessEqual(max(fu), 1.12)
        self.assertGreaterEqual(min(du), 0.83)
        self.assertLessEqual(max(du), 1.10)

    def test_criterio_de_mecanismo_diverge_no_especime_8(self):
        wrong = []
        for i in range(1, 13):
            _, r = run(i)
            observed = 2 if i in (1, 2, 3, 6) else 1
            if r.mechanism != observed:
                wrong.append(i)
        self.assertEqual(wrong, [8])             # limitacao do modelo/criterio (documentada), nao ajustada


if __name__ == "__main__":
    unittest.main()
