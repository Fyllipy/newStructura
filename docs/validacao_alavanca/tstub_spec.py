"""Especificacao executavel (independente do Structura) das duas camadas de efeito de alavanca na mesa do pilar.

Camada B - EN 1993-1-8:2005, 6.2.4.1, 6.2.6.4, Tabelas 6.2 e 6.4 (coeficientes EN; unidades N, mm, MPa)
Camada A - NBR 8800:2024, 6.3.5.2 a 6.3.5.4 (coeficientes NBR)
"""
import math


# ---------------------------------------------------------------- Camada B (EN)
def mplrd(leff, tf, fy, gm0):
    return 0.25 * leff * tf ** 2 * fy / gm0


def tstub_modes(leff1, leff2, tf, fy, gm0, m, e, sum_ft, ew=0.0, method=2, emin=None):
    """Modos 1, 2 e 3 (Tabela 6.2). sum_ft = soma de Ft,Rd dos parafusos do T-stub. method=2 usa ew (sem chapa de reforco)."""
    n = min(emin if emin is not None else e, 1.25 * m)
    mp1 = mplrd(leff1, tf, fy, gm0)
    mp2 = mplrd(leff2, tf, fy, gm0)
    if method == 1:
        f1 = 4.0 * mp1 / m
    else:
        f1 = (8.0 * n - 2.0 * ew) * mp1 / (2.0 * m * n - ew * (m + n))
    f2 = (2.0 * mp2 + n * sum_ft) / (m + n)
    f3 = sum_ft
    modes = {1: f1, 2: f2, 3: f3}
    mode = min(modes, key=modes.get)
    return modes[mode], mode, modes, n


def ft_rd_bolt(fub, As, gm2, k2=0.9):
    return k2 * fub * As / gm2


def leff_isolated(m, e):
    """EN 1993-1-8 Tabela 6.4 (mesa de pilar nao enrijecida), fileira isolada: (circular, nao circular)."""
    return 2 * math.pi * m, 4 * m + 1.25 * e


def leff_group(m, e, spacings):
    """Tabela 6.4: grupo de fileiras consecutivas (spacings = [p1, p2, ...] entre fileiras do grupo).
    Fileiras de extremidade do grupo: cp = pi m + p ; nc = 2 m + 0,625 e + 0,5 p. Fileiras internas: cp = 2 p ; nc = p (p = media dos dois espacamentos).
    Retorna (soma_cp, soma_nc)."""
    n = len(spacings) + 1
    cp = nc = 0.0
    for i in range(n):
        if i == 0 or i == n - 1:
            p = spacings[0] if i == 0 else spacings[-1]
            cp += math.pi * m + p
            nc += 2 * m + 0.625 * e + 0.5 * p
        else:
            p = 0.5 * (spacings[i - 1] + spacings[i])
            cp += 2 * p
            nc += p
    return cp, nc


# ---------------------------------------------------------------- Camada A (NBR 6.3.5)
def nbr_alpha_beta(a, b, db, ft_rd, ft0, delta):
    """NBR 8800:2024, 6.3.5.2: beta, alpha. a <= 1,25 b."""
    a = min(a, 1.25 * b)
    beta = ((a + 0.5 * db) / (b - 0.5 * db)) * (ft_rd / ft0 - 1.0)
    if beta < 0.0:
        return None, None, a
    alpha = 1.0 if beta >= 1.0 else min(1.0, (1.0 / delta) * (beta / (1.0 - beta)))
    return beta, alpha, a


def nbr_t_required(ft0, ft_rd, a, b, p, db, df, fu, ga1):
    """NBR 8800:2024, 6.3.5.4: espessura minima da placa flexivel (e 6.3.5.3 para placa rigida)."""
    delta = 1.0 - df / p
    beta, alpha, a_eff = nbr_alpha_beta(a, b, db, ft_rd, ft0, delta)
    if beta is None:
        return None, None
    t_flex = math.sqrt(4.0 * (b - 0.5 * db) * ft0 * ga1 / (p * fu * (1.0 + delta * alpha)))
    t_rigid = math.sqrt(4.0 * (b - 0.5 * db) * ft0 * ga1 / (p * fu))
    return t_flex, t_rigid


def nbr_ft0_max(t, ft_rd, a, b, p, db, df, fu, ga1):
    """Maior Ft,0,Sd por parafuso admitido por 6.3.5.4 para a espessura t (bissecao). O parafuso nao pode estar sobrecarregado (Ft0 <= Ft,Rd)."""
    lo, hi = 1e-3, ft_rd
    for _ in range(100):
        mid = 0.5 * (lo + hi)
        tf_, _ = nbr_t_required(mid, ft_rd, a, b, p, db, df, fu, ga1)
        ok = tf_ is not None and tf_ <= t + 1e-9
        if ok:
            lo = mid
        else:
            hi = mid
    return lo


def nbr_tributary(b, e_ext_edges, spacings):
    """NBR 8800:2024, 6.3.5.2: larguras tributarias por fileira. e_ext_edges = (distancia da fileira extrema a borda inferior/superior da mesa
    na direcao do comprimento; None se for 'infinita'); spacings = lista de espacamentos entre fileiras consecutivas.
    p = soma de contribuicoes dos dois lados: lado de borda min(e_ext; 1,75 b); lado de fileira vizinha min(0,5 e_int; 1,75 b)."""
    rows = len(spacings) + 1
    out = []
    for i in range(rows):
        p = 0.0
        # lado superior
        if i == 0:
            top = e_ext_edges[0]
            p += min(top if top is not None else 1e9, 1.75 * b)
        else:
            p += min(0.5 * spacings[i - 1], 1.75 * b)
        # lado inferior
        if i == rows - 1:
            bot = e_ext_edges[1]
            p += min(bot if bot is not None else 1e9, 1.75 * b)
        else:
            p += min(0.5 * spacings[i], 1.75 * b)
        out.append(p)
    return out
