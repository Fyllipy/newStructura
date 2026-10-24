"""Verificacao de estabilidade do modelo convertido e estabilizacao de mecanismos de corpo rigido.

O CYPE 3D tolera mecanismos que nao sao excitados pelas cargas (ex.: uma viga apoiada em pendulos pode girar em torno do proprio
eixo); o solver do Structura exige matriz definida positiva. Os graus de liberdade de rotacao que formam esses mecanismos sao
fixados (registrados no relatorio); mecanismos de translacao apenas sao avisados.
"""
from __future__ import annotations

import numpy as np

from .writer_support import release_masks, STEEL, TIMBER

MAX_DOF_DENSE = 4500


def local_axes(a, b):
    x = b - a
    L = np.linalg.norm(x)
    x = x / L
    for k in ([0, 0, 1.0], [0, 1.0, 0], [1.0, 0, 0]):
        k = np.array(k)
        z = np.cross(x, k)
        if np.linalg.norm(z) > 1e-9:
            break
    z /= np.linalg.norm(z)
    y = np.cross(z, x)
    y /= np.linalg.norm(y)
    return np.array([x, y, z]), L


def beam_k(E, G, A, Iz, Iy, J, L):
    k = np.zeros((12, 12))
    a = E * A / L
    k[0, 0] = k[6, 6] = a; k[0, 6] = k[6, 0] = -a
    t = G * J / L
    k[3, 3] = k[9, 9] = t; k[3, 9] = k[9, 3] = -t
    # flexao no plano x-y (Iz): dofs v (1,7) e theta_z (5,11)
    for (v1, r1, v2, r2, I, s) in ((1, 5, 7, 11, Iz, 1), (2, 4, 8, 10, Iy, -1)):
        c = E * I / L ** 3
        k[v1, v1] = k[v2, v2] = 12 * c; k[v1, v2] = k[v2, v1] = -12 * c
        k[v1, r1] = k[r1, v1] = 6 * c * L * s; k[v1, r2] = k[r2, v1] = 6 * c * L * s
        k[v2, r1] = k[r1, v2] = -6 * c * L * s; k[v2, r2] = k[r2, v2] = -6 * c * L * s
        k[r1, r1] = k[r2, r2] = 4 * c * L * L; k[r1, r2] = k[r2, r1] = 2 * c * L * L
    return k


def assemble(model, masks=None):
    bars = [b for b in model.bars.values() if b.id in model.bar_section]
    ids = sorted({n for b in bars for n in (b.ni, b.nj)})
    idx = {n: i for i, n in enumerate(ids)}
    N = len(ids)
    K = np.zeros((6 * N, 6 * N))
    if masks is None:
        masks, art, _ = release_masks(model)
    for b in bars:
        s = model.sections[model.bar_section[b.id]]
        mat = model.materials[b.id]
        E, G = (STEEL["E"], STEEL["E"] / 2.6) if mat == "steel" else (TIMBER["E"], TIMBER["G"])
        A = np.array(model.nodes[b.ni].xyz); B = np.array(model.nodes[b.nj].xyz)
        R, L = local_axes(A, B)
        kl = beam_k(E, G, s.area, s.iz, s.iy, s.j, L)
        m = masks.get(b.id, 0)
        rel = [i for i in range(12) if (m >> i) & 1]
        if rel:
            act = [i for i in range(12) if i not in rel]
            Kaa = kl[np.ix_(act, act)]; Kar = kl[np.ix_(act, rel)]; Krr = kl[np.ix_(rel, rel)]
            try:
                kc = Kaa - Kar @ np.linalg.solve(Krr, Kar.T)
            except np.linalg.LinAlgError:
                kc = Kaa
            kn = np.zeros((12, 12)); kn[np.ix_(act, act)] = kc
            kl = kn
        T = np.zeros((12, 12))
        for i in range(4):
            T[3 * i:3 * i + 3, 3 * i:3 * i + 3] = R
        kg = T.T @ kl @ T
        dofs = list(range(6 * idx[b.ni], 6 * idx[b.ni] + 6)) + list(range(6 * idx[b.nj], 6 * idx[b.nj] + 6))
        K[np.ix_(dofs, dofs)] += kg
    fixed = []
    for n in ids:
        for d, r in enumerate(model.nodes[n].restraints):
            if r:
                fixed.append(6 * idx[n] + d)
    free = [i for i in range(6 * N) if i not in set(fixed)]
    return K, free, ids


def mechanisms(model, tol=1e-9):
    K, free, ids = assemble(model)
    Kf = K[np.ix_(free, free)]
    # remove DOFs sem rigidez (o Structura os fixa automaticamente)
    diag = np.abs(np.diag(Kf))
    keep = [i for i in range(len(free)) if diag[i] > 1e-12 * diag.max()]
    Kf = Kf[np.ix_(keep, keep)]
    w, V = np.linalg.eigh(Kf)
    sc = w.max()
    null = [i for i in range(len(w)) if w[i] < tol * sc]
    out = []
    for i in null:
        v = V[:, i]
        top = np.argsort(-np.abs(v))[:4]
        out.append([(ids[free[keep[t]] // 6], "ux uy uz rx ry rz".split()[free[keep[t]] % 6], round(float(v[t]), 3)) for t in top])
    return w[:len(null) + 2] / sc, out




def stabilize(model, masks, tol=1e-8, max_iter=40):
    """Fixa rotacoes que formam mecanismos. Devolve lista de mensagens."""
    notes = []
    ids = sorted({n for b in model.bars.values() if b.id in model.bar_section for n in (b.ni, b.nj)})
    if 6 * len(ids) > MAX_DOF_DENSE:
        return ["modelo grande demais para a verificacao automatica de estabilidade (nao realizada)"]
    for _ in range(max_iter):
        K, free, ids = assemble(model, masks)
        if not free:
            break
        Kf = K[np.ix_(free, free)]
        diag = np.abs(np.diag(Kf))
        keep = [i for i in range(len(free)) if diag[i] > 1e-12 * diag.max()]
        w, V = np.linalg.eigh(Kf[np.ix_(keep, keep)])
        null = [i for i in range(len(w)) if w[i] < tol * w.max()]
        if not null:
            break
        v = V[:, null[0]]
        t = int(np.argmax(np.abs(v)))
        dof = free[keep[t]]
        node, comp = ids[dof // 6], dof % 6
        if comp < 3:
            notes.append(f"mecanismo de translacao no no {node} ({'ux uy uz'.split()[comp]}): o modelo pode ser instavel no Structura")
            break
        r = list(model.nodes[node].restraints)
        r[comp] = 1
        model.nodes[node].restraints = tuple(r)
        notes.append(f"no {node}: rotacao {'rx ry rz'.split()[comp - 3]} fixada para eliminar um mecanismo de corpo rigido "
                     f"(o CYPE 3D tolera mecanismos nao excitados pelas cargas; o Structura nao)")
    return notes
