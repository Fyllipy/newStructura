"""Conferencia independente da rigidez inicial do T-stub (EN 1993-1-8, 6.3.2, Tabela 6.11) contra valores PUBLICADOS.

Nao importa nada do Structura. Entradas e valores de referencia: exemplo de calculo manual da SCIA (EN 1993-1-8, HEB140 + IPE220, chapa 140x12, 6 M16 8.8)
e exemplo C2 do SCI P398 (New Steel Construction, fev/2020) para k10. Classificacao: esta conferencia verifica a ARITMETICA das equacoes contra numeros
publicados; ela NAO valida a forma das equacoes (que vem da propria EN) nem o comportamento mecanico (sem MEF nem ensaio).
Uso: python -I validate_stiffness.py
"""
import math

A_K4 = 0.9   # EN 1993-1-8, Tabela 6.11
A_K10 = 1.6


def k_flange(m, leff, t):
    return A_K4 * leff * t ** 3 / m ** 3


def k_bolt(As, Lb):
    return A_K10 * As / Lb


def series(ks):
    return 1.0 / sum(1.0 / k for k in ks)


rows = []


def check(name, value, published, tol):
    err = abs(value - published) / published
    rows.append((name, value, published, err, tol, err <= tol))


Lb = 12 + 12 + 2 * 1.65 + (10 + 13) / 2
check("Lb (mm)", Lb, 38.8, 1e-4)
mx = 30 - 0.8 * math.sqrt(2) * 5
check("k4 fileira 1 (mm)", k_flange(26.9, 107.55, 12), 8.59, 0.002)
check("k4 fileira 2 (mm)", k_flange(26.9, 105.0, 12), 8.39, 0.002)
check("k5 fileira 1 (mm)", k_flange(mx, 70.0, 12), 7.55, 0.005)
check("k5 fileira 2 (mm)", k_flange(33.66, 185.51, 12), 7.57, 0.005)
check("k10 (mm)", k_bolt(157.0, Lb), 6.47, 0.002)
check("keff fileira 1 (mm), ki publicados", series([5.73, 8.59, 7.55, 6.47]), 1.73, 0.01)
check("keff fileira 2 (mm), ki publicados", series([5.59, 8.39, 7.57, 6.47]), 1.71, 0.01)
check("k10 SCI P398 C2 (mm): 1,6 x 353 / 70,5", k_bolt(353.0, 70.5), 8.01, 0.002)

print("%-48s %10s %10s %8s %8s  %s" % ("grandeza", "calculado", "publicado", "erro", "tol", "ok"))
ok = True
for name, v, p, e, t, good in rows:
    ok &= good
    print("%-48s %10.4f %10.4f %7.3f%% %7.2f%%  %s" % (name, v, p, 100 * e, 100 * t, "ok" if good else "FALHOU"))
print("RESULTADO:", "todas dentro da tolerancia" if ok else "HA DIFERENCAS")
raise SystemExit(0 if ok else 1)
