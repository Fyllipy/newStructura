"""Procura mecanismos (modos de rigidez nula) de um modelo convertido, com um montador 3D proprio (mesma convencao de eixos do Structura)."""
import sys, os
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from cype2structura.model import build_model
from cype2structura.writer_support import release_masks, STEEL, TIMBER


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


def assemble(model):
    bars = [b for b in model.bars.values() if b.id in model.bar_section]
    ids = sorted({n for b in bars for n in (b.ni, b.nj)})
    idx = {n: i for i, n in enumerate(ids)}
    N = len(ids)
    K = np.zeros((6 * N, 6 * N))
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


if __name__ == "__main__":
    name = sys.argv[1]
    m = build_model(open(r"D:\CYPE Ingenieros\Projetos\CYPE 3D\%s.ed3" % name, "rb").read())
    w, modes = mechanisms(m)
    print(name, "menores autovalores/rigidez max:", np.array2string(w, precision=2))
    for md in modes[:8]:
        print(" modo:", md)
