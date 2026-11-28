"""Dados dos 12 espécimes de [P2] (Piluso, Faella, Rizzano 2001, Parte II).

ORIGEM DOS VALORES (rastreabilidade):
  - GEOMETRIA: [P2] Tabela 1 (p. 696)        -> dado tabulado
  - ACO / PARAFUSO: [P2] Tabela 2 (p. 697)   -> dado tabulado (eps_h, eps_u em % na tabela; aqui em fracao)
  - RESULTADOS PUBLICADOS: [P2] Tabela 3 (p. 697) -> dado tabulado; ver TABLE3_NOTES para as correcoes de rotulos
  - E = 210000 N/mm2: NAO esta nas tabelas; vem do exemplo trabalhado (p. 702) -> hipotese de transcricao
  - As (area resistente do parafuso): NAO esta nas tabelas. Adotadas as areas resistentes ISO usuais (M20 245; M24 353; M27 459 mm2). So M27 (459) e
    confirmado pelo exemplo trabalhado de [P2] (p. 702). M20 e M24: HIPOTESE, confrontada com o beta_u publicado (ver validate_published.py).
Nenhum valor foi digitalizado de figura neste arquivo.
"""
from piluso_tstub import TStub

AS_ISO = {20.0: 245.0, 24.0: 353.0, 27.0: 459.0}

_G = {  # db, dh, dw, th, tn, tf, m, n, r, b, beff   [P2] Tabela 1
    1: (20.00, 33.53, 37.00, 12.72, 16.00, 14.40, 39.30, 40.80, 18.00, 126.50, 112.13),
    2: (20.00, 33.53, 37.00, 12.72, 16.00, 14.60, 39.30, 40.80, 18.00, 119.00, 112.13),
    3: (20.00, 33.53, 37.00, 12.72, 16.00, 13.00, 42.30, 22.20, 15.00, 124.00, 118.13),
    4: (24.00, 39.98, 44.00, 14.78, 19.00, 12.30, 48.95, 58.10, 24.00, 118.80, 118.80),
    5: (24.00, 39.98, 44.00, 14.78, 19.00, 13.80, 49.75, 74.90, 27.00, 115.00, 115.00),
    6: (24.00, 39.98, 44.00, 14.78, 19.00, 16.35, 45.65, 45.20, 18.00, 120.00, 120.00),
    7: (27.00, 45.20, 56.00, 17.35, 27.00, 13.90, 52.45, 71.70, 27.00, 122.80, 122.80),
    8: (24.00, 39.98, 35.38, 14.78, 19.00, 13.30, 45.20, 19.30, 15.00, 112.50, 112.50),
    9: (27.00, 45.20, 56.00, 17.35, 27.00, 12.50, 54.05, 53.00, 24.00, 125.10, 125.10),
    10: (20.00, 35.03, 37.00, 13.00, 16.00, 10.85, 32.60, 58.50, 18.00, 125.00, 95.20),
    11: (20.00, 33.53, 37.00, 12.72, 16.00, 9.50, 36.40, 38.00, 15.00, 159.00, 106.33),
    12: (20.00, 33.53, 37.00, 12.72, 16.00, 12.20, 53.50, 49.90, 7.50, 90.25, 90.25),
}
_M = {  # fy, fu, Eh, Eu, eps_h(%), eps_u(%), fub   [P2] Tabela 2
    1: (291.16, 517.21, 3276, 371.11, 1.358, 49.18, 800),
    2: (264.95, 501.11, 3171, 378.32, 1.135, 48.12, 800),
    3: (273.15, 504.33, 3087, 435.95, 1.171, 58.70, 800),
    4: (299.76, 543.59, 2877, 465.24, 1.285, 76.77, 800),
    5: (317.72, 546.84, 3339, 483.15, 1.362, 78.43, 800),
    6: (280.46, 527.76, 3360, 488.11, 1.202, 63.83, 800),
    7: (307.58, 543.57, 3423, 516.05, 1.318, 77.88, 800),
    8: (269.42, 482.70, 3276, 476.80, 1.055, 69.40, 800),
    9: (300.97, 552.27, 3234, 466.65, 1.290, 67.75, 800),
    10: (293.10, 514.87, 2163, 406.32, 0.600, 92.50, 1000),
    11: (324.33, 530.93, 2184, 347.93, 2.04, 96.98, 800),
    12: (346.50, 460.77, 2310, 383.02, 0.87, 95.29, 800),
}
# [P2] Tabela 3: Mh/My, Mm/My, Mu/My, beta_u, du_exp (mm), du_th (mm), du_th/du_exp, Fu_exp (kN), Fu_th (kN), Fu_th/Fu_exp  -- COMO IMPRESSOS
_R = {
    1: (1.4948, 2.2019, 3.1220, 0.7133, 21.91, 19.18, 0.88, 342.39, 343.99, 1.00),
    2: (1.4938, 2.3245, 3.3259, 0.7108, 22.71, 19.38, 0.85, 337.14, 343.68, 1.02),
    3: (1.4938, 2.2813, 3.4899, 0.5967, 39.3, 40.19, 1.02, 281.46, 271.36, 0.96),
    4: (1.4938, 2.2527, 3.6781, 0.3729, 63.71, 62.63, 0.98, 343.32, 324.23, 0.94),
    5: (1.4938, 2.1514, 3.5827, 0.4615, 68.16, 58.43, 0.85, 423.61, 394.36, 0.93),
    6: (1.4938, 2.3111, 3.7022, 0.6720, 40.80, 34.03, 0.84, 482.32, 445.76, 1.08),
    7: (1.4938, 2.1956, 3.7466, 0.3794, 70.81, 66.64, 0.94, 429.90, 479.40, 0.90),
    8: (1.4926, 2.2286, 3.7101, 0.4053, 47.61, 45.19, 0.95, 348.75, 385.99, 1.11),
    9: (1.4938, 2.2669, 3.5848, 0.2839, 57.99, 63.28, 1.09, 378.01, 329.00, 0.87),
    10: (1.4729, 2.2345, 3.6871, 0.3942, 38.03, 38.98, 1.02, 323.81, 305.35, 0.94),
    11: (1.4971, 2.0704, 3.3063, 0.3651, 59.08, 53.57, 0.91, 250.41, 235.61, 0.94),
    12: (1.4820, 1.8010, 2.9510, 0.3405, 81.00, 87.76, 1.08, 195.95, 198.84, 1.01),
}

TABLE3_NOTES = (
    "Rotulos da Tabela 3 de [P2]: nos espécimes 6 e 7 as colunas 'Fu,exp' e 'Fu,th' aparecem TROCADAS em relação à razão impressa Fu,th/Fu,exp e ao exemplo "
    "trabalhado (espécime 7: Fu teórico = 429,9 kN, p. 702; Fig. 8: curva-modelo termina em ~430 kN, experimental ~480 kN). Para 6 e 7, as colunas foram "
    "reinterpretadas (INFERENCIA documentada): Fu,exp(6) = 445,76; Fu,th(6) = 482,32; Fu,exp(7) = 479,40; Fu,th(7) = 429,90. Para os demais a razão impressa "
    "confere com os rótulos."
)


def specimen(i: int, As: float | None = None) -> TStub:
    g, mat = _G[i], _M[i]
    return TStub(db=g[0], dh=g[1], dw=g[2], th=g[3], tn=g[4], tf=g[5], m=g[6], n=g[7], r=g[8], b=g[9], beff=g[10],
                 fy=mat[0], fu=mat[1], Eh=mat[2], Eu=mat[3], eps_h=mat[4] / 100.0, eps_u=mat[5] / 100.0, fub=mat[6],
                 As=As if As is not None else AS_ISO[g[0]])


def published(i: int) -> dict:
    r = _R[i]
    d = dict(Mh_My=r[0], Mm_My=r[1], Mu_My=r[2], beta_u=r[3], du_exp=r[4], du_th=r[5], du_ratio=r[6], Fu_exp=r[7], Fu_th=r[8], Fu_ratio=r[9])
    if i in (6, 7):   # troca documentada em TABLE3_NOTES
        d["Fu_exp"], d["Fu_th"] = r[8], r[7]
    return d
