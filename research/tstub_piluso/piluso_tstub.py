"""Reproducao independente do modelo de Piluso, Faella e Rizzano (2001) para T-stubs aparafusados.

Fontes (ver docs/technical/TStub_Piluso_Model_Reconstruction.md):
  [P1] Piluso, Faella, Rizzano (2001). Ultimate behavior of bolted T-stubs. I: Theoretical model. J. Struct. Eng. 127(6), 686-693.
  [P2] Piluso, Faella, Rizzano (2001). Ultimate behavior of bolted T-stubs. II: Model validation. J. Struct. Eng. 127(6), 694-704.
Numeracao "(n)" = equacao n do artigo citado. Nao importa nada do Structura. Python puro (sem numpy).

ESCOPO: mecanismos 1 e 2 (os unicos com formulacao completa em [P1]); o mecanismo 3 NAO tem formulacao em [P1] (apenas Fu,3 = 2 Bu) e nao e implementado.
Unidades: N, mm, N/mm2.  Nada aqui e uma norma: e a reproducao de um modelo publicado.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Dict, List, Optional

E_DEFAULT = 210000.0  # N/mm2. NAO consta das Tabelas de [P2]; vem do exemplo trabalhado de [P2] (p. 702-703). HIPOTESE de transcricao.


# ----------------------------------------------------------------------------------------------------------------------
# Dados de entrada
# ----------------------------------------------------------------------------------------------------------------------
@dataclass
class TStub:
    # geometria [P2, Tabela 1]
    db: float; dh: float; dw: float; th: float; tn: float
    tf: float; m: float; n: float; r: float; b: float
    # material do aco [P2, Tabela 2]; deformacoes ja em fracao (a tabela traz %)
    fy: float; fu: float; Eh: float; Eu: float; eps_h: float; eps_u: float
    fub: float
    # propriedades adotadas (nao tabeladas em [P2]) -- ver docs
    E: float = E_DEFAULT
    As: float = 0.0         # area resistente do parafuso (resistencia); 0 => pi/4 db^2 NAO e assumido: deve ser informado
    beff: Optional[float] = None   # [P2, Tabela 1]; se None usa a regra do exemplo de [P2]: min(dh + 2m ; b)

    def effective_width(self) -> float:
        return self.beff if self.beff is not None else min(self.dh + 2 * self.m, self.b)


# ----------------------------------------------------------------------------------------------------------------------
# Material e relacao momento-curvatura  [P1] (4)-(8), curva multilinear da Fig. 2
# ----------------------------------------------------------------------------------------------------------------------
@dataclass
class Material:
    eps_y: float; eps_h: float; eps_m: float; eps_u: float
    c_h: float; c_m: float; c_u: float      # curvaturas normalizadas chi/chi_y = eps/eps_y (chi = 2 eps/tf)
    Eh_E: float; Eu_E: float
    My_ratio: float = 1.0
    Mh_My: float = 0.0; Mm_My: float = 0.0; Mu_My: float = 0.0


def material_of(s: TStub) -> Material:
    eps_y = s.fy / s.E
    eps_m = s.eps_h + (s.fu - s.fy) / s.Eh     # ponto de carga maxima: fu atingida (Fig. 2); [P2] apendice
    mat = Material(eps_y=eps_y, eps_h=s.eps_h, eps_m=eps_m, eps_u=s.eps_u,
                   c_h=s.eps_h / eps_y, c_m=eps_m / eps_y, c_u=s.eps_u / eps_y,
                   Eh_E=s.Eh / s.E, Eu_E=s.Eu / s.E)
    mat.Mh_My = m_over_my(mat, mat.c_h)
    mat.Mm_My = m_over_my(mat, mat.c_m)
    mat.Mu_My = m_over_my(mat, mat.c_u)
    return mat


def m_over_my(mat: Material, cb: float) -> float:
    """M/My em funcao da curvatura normalizada cb = chi/chi_y.  [P1] (5), (6), (7), (8)."""
    if cb <= 1.0:
        return cb                                                        # (5)
    base = 0.5 * (3.0 - 1.0 / cb ** 2)                                   # (6)
    if cb <= mat.c_h:
        return base
    ch, cm = mat.c_h, mat.c_m
    v = base + 0.5 * mat.Eh_E * (cb - ch) * (1.0 - ch / cb) * (2.0 + ch / cb)   # (7)
    if cb <= cm:
        return v
    return v - 0.5 * (mat.Eh_E - mat.Eu_E) * (cb - cm) * (1.0 - cm / cb) * (2.0 + cm / cb)   # (8)


def chi_of_xi(mat: Material, xi: float) -> float:
    """chi_xi/chi_y para M = xi*Mu: inversao numerica de M-chi (unica parte numerica do modelo, [P1] p. 690)."""
    target = xi * mat.Mu_My
    if target <= 1.0:
        return target
    lo, hi = 1.0, mat.c_u * (1.0 + 1e-12)
    for _ in range(200):
        mid = 0.5 * (lo + hi)
        if m_over_my(mat, mid) < target:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)


# ----------------------------------------------------------------------------------------------------------------------
# Rotacoes plasticas por balanco  [P1] (16)-(36); funcoes D, F, G (adimensionais; multiplicam L/tf)
# ----------------------------------------------------------------------------------------------------------------------
def xi_limits(mat: Material):
    mu = mat.Mu_My
    return 1.0 / mu, mat.Mh_My / mu, mat.Mm_My / mu      # xi1 = My/Mu; xi2 = Mh/Mu (17); xi3 = Mm/Mu (25)


def func_D(mat: Material, xi: float) -> float:
    cx = chi_of_xi(mat, xi)
    return mat.eps_y * (2.0 * cx - (1.0 / xi) * (1.0 / mat.Mu_My) * (3.0 * cx + 1.0 / cx - 3.0) - 1.0)     # (24)


def func_F(mat: Material, xi: float) -> float:
    cx = chi_of_xi(mat, xi)
    t = 3.0 * cx + 1.0 / cx - 3.0 + mat.Eh_E * (cx - mat.c_h) ** 3 / cx
    return mat.eps_y * (2.0 * cx - (1.0 / xi) * (1.0 / mat.Mu_My) * t - 1.0)                              # (29)


def func_G(mat: Material, xi: float) -> float:
    cx = chi_of_xi(mat, xi)
    ch, cm = mat.c_h, mat.c_m
    gh = cm ** 3 / cx + 3.0 * cm * cx - 3.0 * cm ** 2 + 3.0 * ch ** 2 - 3.0 * ch * cx - ch ** 3 / cx    # (35)
    gu = cx ** 2 + 3.0 * cm ** 2 - 3.0 * cm * cx - cm ** 3 / cx                                          # (36)
    t = 3.0 * cx + 1.0 / cx - 3.0 + mat.Eh_E * gh + mat.Eu_E * gu
    return mat.eps_y * (2.0 * cx - (1.0 / xi) * (1.0 / mat.Mu_My) * t - 1.0)                              # (34)


def plastic_rotation_factor(mat: Material, xi: float) -> float:
    """theta_p / (L/tf) = D, F ou G conforme o caso 1-4 de [P1] (16), (23), (28), (33). xi = M/Mu no engaste do balanco."""
    xi1, xi2, xi3 = xi_limits(mat)
    if xi <= xi1:
        return 0.0                                  # caso 1 (16)
    if xi <= xi2:
        return func_D(mat, xi)                      # caso 2
    if xi <= xi3:
        return func_F(mat, xi)                      # caso 3
    return func_G(mat, xi)                          # caso 4


def plastic_rotation_numeric(mat: Material, xi: float, steps: int = 20000) -> float:
    """Verificacao INDEPENDENTE de (23)/(28)/(33): integra numericamente theta = int chi(z) dz num balanco de comprimento unitario
    com momento linear xi*Mu -> 0 e subtrai theta_y = chi_y L/2 [P1] (18), (22). Devolve theta_p/(L/tf)."""
    mu = mat.Mu_My
    total = 0.0
    for i in range(steps):
        z = (i + 0.5) / steps
        mr = xi * mu * (1.0 - z)               # M/My em z (L = 1)
        if mr <= 1.0:
            cb = mr
        else:
            lo, hi = 1.0, mat.c_u * (1 + 1e-12)
            for _ in range(80):
                mid = 0.5 * (lo + hi)
                if m_over_my(mat, mid) < mr:
                    lo = mid
                else:
                    hi = mid
            cb = 0.5 * (lo + hi)
        total += cb / steps                    # chi/chi_y integrado em z: theta/(chi_y L)
    # theta = chi_y L * total ; theta_y = chi_y L/2 ; chi_y = 2 eps_y/tf  => (theta - theta_y)/(L/tf) = 2 eps_y (total - 1/2)
    return 2.0 * mat.eps_y * (total - 0.5)


# ----------------------------------------------------------------------------------------------------------------------
# Resistencia, mecanismo e curva  [P1] (9)-(15), (37)-(62); [P2] (1)-(10), apendice
# ----------------------------------------------------------------------------------------------------------------------
@dataclass
class Result:
    mechanism: int = 0
    beta_u: float = 0.0
    beta_u_lim: float = 0.0
    psi: float = 0.0
    My: float = 0.0
    Mu: float = 0.0
    Bu: float = 0.0
    forces: Dict[str, float] = field(default_factory=dict)         # Fy, Fh, Fm, Fu  (N)
    disp: Dict[str, float] = field(default_factory=dict)           # dy, dh, dm, du (mm)
    K_element: float = 0.0           # K de (38) (N/mm) com m'
    K_initial: float = 0.0           # Ki = Fy/dy (41)
    F_u1: float = 0.0; F_u2: float = 0.0; F_u3: float = 0.0
    notes: List[str] = field(default_factory=list)


def stiffness_K(s: TStub):
    """K = 0.5 E beff tf^3 / m'^3  [P1] (38); m' = m + 0.8 r - zeta r, zeta = 0.16 (m + 0.8 r)/tf - 0.08 ([P2] apendice, de Faella et al. 1998)."""
    zeta = 0.16 * (s.m + 0.8 * s.r) / s.tf - 0.08
    mp = s.m + 0.8 * s.r - zeta * s.r
    return 0.5 * s.E * s.effective_width() * s.tf ** 3 / mp ** 3, mp, zeta


def validate(s: TStub) -> None:
    """Entradas invalidas NUNCA geram numero: levanta ValueError (sem defaults silenciosos)."""
    def pos(name, v):
        if not (isinstance(v, (int, float)) and math.isfinite(v) and v > 0):
            raise ValueError("%s invalido ou ausente: %r" % (name, v))
    for name in ("db", "dh", "dw", "th", "tn", "tf", "m", "n", "b", "fy", "fu", "Eh", "Eu", "eps_h", "eps_u", "fub", "E", "As"):
        pos(name, getattr(s, name))
    if not (isinstance(s.r, (int, float)) and math.isfinite(s.r) and s.r >= 0):
        raise ValueError("r invalido: %r" % (s.r,))
    if s.fu <= s.fy:
        raise ValueError("fu deve ser maior que fy")
    eps_y = s.fy / s.E
    eps_m = s.eps_h + (s.fu - s.fy) / s.Eh
    if not (eps_y < s.eps_h < eps_m < s.eps_u):
        raise ValueError("deformacoes fora de ordem: exigido eps_y < eps_h < eps_m < eps_u (%.4g, %.4g, %.4g, %.4g)" % (eps_y, s.eps_h, eps_m, s.eps_u))
    if s.effective_width() <= 0:
        raise ValueError("largura efetiva invalida")


def analyse(s: TStub, As_stiffness: Optional[float] = None, use_mprime: bool = True, force_mechanism: Optional[int] = None) -> Result:
    """Quatro pontos caracteristicos (Fy,dy), (Fh,dh), (Fm,dm), (Fu,du) para os mecanismos 1 e 2.
    As_stiffness: area do parafuso usada em delta_b ([P2] apendice usa pi db^2/4 = 572,5553 mm2 para M27, NAO a area resistente 459)."""
    validate(s)
    mat = material_of(s)
    res = Result()
    beff = s.effective_width()
    My = beff * s.tf ** 2 / 6.0 * s.fy
    Mu = mat.Mu_My * My
    res.My, res.Mu = My, Mu
    m, n, dw, tf, E = s.m, s.n, s.dw, s.tf, s.E
    lam = n / m
    Bu = s.As * s.fub
    res.Bu = Bu
    res.beta_u = 2.0 * Mu / (Bu * m)                                                         # (9)
    res.beta_u_lim = 2 * lam / (1 + 2 * lam) * (1.0 - (1 + lam) * dw / (8 * n))              # (11)
    res.F_u1 = (32 * n - 2 * dw) * Mu / (8 * m * n - (m + n) * dw)                           # (10)
    res.F_u2 = (2 * Mu + 2 * Bu * n) / (m + n)                                               # (12)
    res.F_u3 = 2 * Bu                                                                        # (13)
    if force_mechanism in (1, 2):
        res.mechanism = force_mechanism
        res.notes.append('mecanismo FORCADO pelo chamador (nao decorre do criterio beta_u)')
    elif res.beta_u <= res.beta_u_lim:
        res.mechanism = 1
    elif res.beta_u <= 2.0:
        res.mechanism = 2
    else:
        res.mechanism = 3
        res.notes.append("mecanismo 3: [P1] nao fornece a curva (so Fu,3 = 2Bu); nao implementado")
        res.forces = {"Fu": res.F_u3}
        return res
    moments = {"y": My, "h": mat.Mh_My * My, "m": mat.Mm_My * My, "u": Mu}
    K, mp, zeta = stiffness_K(s)
    if not use_mprime:
        K = 0.5 * E * beff * tf ** 3 / m ** 3
    res.K_element = K
    Ab = As_stiffness if As_stiffness else math.pi * s.db ** 2 / 4.0
    Lb = 2.0 * tf + (s.th + s.tn) / 2.0                                                      # [P2] apendice: 2 tf + (th + tn)/2
    kb = E * Ab / Lb
    if res.mechanism == 1:
        factor = (32 * n - 2 * dw) / (8 * m * n - (m + n) * dw)
        forces = {k: factor * M for k, M in moments.items()}                                  # (37)
        psi = 0.0
        by = forces["y"] / 2.0 + My / n                                                       # (40)
    else:
        psi = (2.0 - res.beta_u) * lam / (res.beta_u * (1 + lam))                            # (49)
        forces = {k: 2.0 * M / m * (1 + psi) for k, M in moments.items()}                     # (50)
        by = forces["y"] / 2.0 + psi * My / n                                                 # (51)
    res.psi = psi
    res.forces = {"F" + k: v for k, v in forces.items()}
    dy = 2.0 * forces["y"] / K + by / kb                                                      # (39)
    Ki = forces["y"] / dy                                                                     # (41)
    res.K_initial = Ki
    xi1, xi2, xi3 = xi_limits(mat)
    disp = {"y": dy}
    for key, xi_hinge1 in (("h", xi2), ("m", xi3), ("u", 1.0)):
        Fk = forces[key]
        de = Fk / Ki                                                                          # delta_e = F/Ki
        P1 = plastic_rotation_factor(mat, xi_hinge1)       # D(xi2), F(xi3), G(1) = C
        if res.mechanism == 1:
            dp = m * m / (2.0 * tf) * P1                                                      # (43), (45), (47)
        else:
            th1 = m / (tf * (1 + psi)) * P1                                                   # (52), (54), (58)
            xi_b = psi * xi_hinge1                                                            # momento no eixo do parafuso / Mu
            P2 = plastic_rotation_factor(mat, xi_b)
            th2 = m / tf * (psi / (1 + psi) + lam) * P2                                       # (53), (56)-(57), (59)-(62)
            if th1 > th2 and th2 >= 0.0:
                dp = th1 * m + (th1 - th2) * n                                                # (15)
            else:
                dp = th1 * m
        disp[key] = de + 2.0 * dp                                                             # (42), (44), (46)
    res.disp = {"d" + k: v for k, v in disp.items()}
    return res


def curve(s: TStub, As_stiffness: Optional[float] = None, points: int = 200, force_mechanism: Optional[int] = None):
    """Curva F-delta continua obtida aplicando as MESMAS formulas de [P1] a qualquer M entre My e Mu no engaste ([P1] p. 690-691, 'Generality';
    [P2] 'Theoretical prediction'). Mecanismo 1 ou 2. Devolve lista (delta, F)."""
    r = analyse(s, As_stiffness, force_mechanism=force_mechanism)
    if r.mechanism not in (1, 2):
        return []
    mat = material_of(s)
    beff = s.effective_width()
    My = r.My
    m, n, dw, tf = s.m, s.n, s.dw, s.tf
    lam = n / m
    Ki = r.K_initial
    out = [(0.0, 0.0), (r.disp["dy"], r.forces["Fy"])]
    for i in range(1, points + 1):
        mr = 1.0 + (mat.Mu_My - 1.0) * i / points        # M/My
        M = mr * My
        xi = M / r.Mu
        if r.mechanism == 1:
            F = (32 * n - 2 * dw) / (8 * m * n - (m + n) * dw) * M
            dp = m * m / (2 * tf) * plastic_rotation_factor(mat, xi)
        else:
            F = 2 * M / m * (1 + r.psi)
            th1 = m / (tf * (1 + r.psi)) * plastic_rotation_factor(mat, xi)
            th2 = m / tf * (r.psi / (1 + r.psi) + lam) * plastic_rotation_factor(mat, r.psi * xi)
            dp = th1 * m + (th1 - th2) * n if (th1 > th2 and th2 >= 0.0) else th1 * m
        out.append((F / Ki + 2 * dp, F))
    return out
