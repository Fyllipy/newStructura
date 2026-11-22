"""Recomputa o lado aplicado (SigmaF, SigmaM em torno da origem) a partir das entradas,
de forma barata (O(cargas)), e confere o equilibrio global contra os totais de reacao do cliente.

Trata cargas nodais e cargas de barra distribuídas uniformes:
- coord_system=1: sistema GLOBAL (qx, qy, qz)
- coord_system=0: sistema LOCAL (qx, qy, qz) transformado para global usando a orientação
  exata da barra e convenção Sennett/Structuradll (x_local, y_local, z_local, rotação da seção).
"""
from __future__ import annotations
import math
from typing import Tuple, List, Dict
from .schemas import ModelIn, MemberIn, NodeIn
from .config import settings

_GEOM_TOL = 1e-10


def _cross(a: List[float] | Tuple[float, float, float], b: List[float] | Tuple[float, float, float]) -> List[float]:
    return [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]


def _dot(a: List[float] | Tuple[float, float, float], b: List[float] | Tuple[float, float, float]) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _norm(v: List[float] | Tuple[float, float, float]) -> float:
    return math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])


def _normalize(v: List[float] | Tuple[float, float, float]) -> List[float]:
    n = _norm(v)
    if n < _GEOM_TOL:
        raise ValueError("Vetor nulo nao pode ser normalizado")
    return [v[0] / n, v[1] / n, v[2] / n]


def select_reference_vector(x_local: List[float]) -> List[float]:
    """Seleciona o vetor de referência K para definir o plano local x-y (Sennett / Structuradll)."""
    candidates = [
        [0.0, 0.0, 1.0],  # Z global
        [0.0, 1.0, 0.0],  # Y global
        [1.0, 0.0, 0.0],  # X global
    ]
    for cand in candidates:
        if _norm(_cross(x_local, cand)) > _GEOM_TOL:
            return _normalize(cand)
    # Caso degenerado
    fb = [1.0, 1.0, 0.0]
    if _norm(_cross(fb, x_local)) <= _GEOM_TOL:
        fb = [1.0, 0.0, 1.0]
    return _normalize(fb)


def compute_member_orientation(ri: Tuple[float, float, float],
                               rj: Tuple[float, float, float],
                               rotation_deg: float = 0.0) -> Tuple[List[float], List[float], List[float], float]:
    """Calcula os eixos locais ortonormais (x_local, y_local, z_local) e comprimento L.
    
    Reproduz exatamente a formulação de Member::ensureLocalAxesComputed() de Structuradll/src/member.cpp:
    - x_local = (rj - ri) / L
    - z_local = x_local x vectorK
    - y_local = z_local x x_local
    - Rotação sectionRotationDeg em torno de x (regra da mão direita):
        y' = cos * y + sin * z
        z' = -sin * y + cos * z
    """
    dx = rj[0] - ri[0]
    dy = rj[1] - ri[1]
    dz = rj[2] - ri[2]
    L = math.sqrt(dx * dx + dy * dy + dz * dz)
    if L < _GEOM_TOL:
        raise ValueError("Comprimento de barra invalido")
    x_local = [dx / L, dy / L, dz / L]

    vector_k = select_reference_vector(x_local)
    z_local = _cross(x_local, vector_k)
    z_norm = _norm(z_local)
    if z_norm < _GEOM_TOL:
        alternatives = [
            [0.0, 1.0, 0.0],
            [1.0, 0.0, 0.0],
            [1.0, 1.0, 0.0],
        ]
        found = False
        for alt in alternatives:
            alt_norm = _normalize(alt)
            test_z = _cross(x_local, alt_norm)
            if _norm(test_z) > _GEOM_TOL:
                z_local = test_z
                z_norm = _norm(test_z)
                vector_k = alt_norm
                found = True
                break
        if not found:
            raise ValueError("Nao foi possivel determinar o eixo local z da barra")

    z_local = [z_local[0] / z_norm, z_local[1] / z_norm, z_local[2] / z_norm]
    y_local = _cross(z_local, x_local)
    y_norm = _norm(y_local)
    if y_norm < _GEOM_TOL:
        raise ValueError("Nao foi possivel determinar o eixo local y da barra")
    y_local = [y_local[0] / y_norm, y_local[1] / y_norm, y_local[2] / y_norm]

    # Rotação da seção se houver
    if abs(rotation_deg) > 0.0:
        rad = math.radians(rotation_deg)
        c = math.cos(rad)
        s = math.sin(rad)
        y0 = list(y_local)
        z0 = list(z_local)
        y_local = [c * y0[k] + s * z0[k] for k in range(3)]
        z_local = [-s * y0[k] + c * z0[k] for k in range(3)]

    return x_local, y_local, z_local, L


def transform_local_to_global(q_local: List[float],
                              x_local: List[float],
                              y_local: List[float],
                              z_local: List[float]) -> List[float]:
    """Converte vetor no sistema local (qx, qy, qz) para o sistema global:
    F_global = qx * x_local + qy * y_local + qz * z_local
    """
    qx, qy, qz = q_local[0], q_local[1], q_local[2]
    return [
        qx * x_local[k] + qy * y_local[k] + qz * z_local[k]
        for k in range(3)
    ]


def _to_global(v: List[float], coord_system: int, x_loc: List[float], y_loc: List[float], z_loc: List[float]) -> List[float]:
    return transform_local_to_global(v, x_loc, y_loc, z_loc) if coord_system == 0 else list(v)


def _add_trapezoid(F: List[float], M: List[float], ri: Tuple[float, float, float], x_loc: List[float],
                   a: float, b: float, v1g: List[float], v2g: List[float]) -> None:
    """Carga distribuida linear (v1g em a, v2g em b), ja em GLOBAL (forca/comprimento).
    R = integral de q ds ; sq = integral de s*q(s) ds ; r(s) = ri + s*x_loc."""
    span = b - a
    if abs(span) < _GEOM_TOL:
        return
    R = [(v1g[k] + v2g[k]) * 0.5 * span for k in range(3)]
    sq = [span * (a * (v1g[k] + v2g[k]) * 0.5 + span * (v1g[k] / 6.0 + v2g[k] / 3.0)) for k in range(3)]
    m1 = _cross(ri, R)
    m2 = _cross(x_loc, sq)
    for k in range(3):
        F[k] += R[k]
        M[k] += m1[k] + m2[k]


def applied_resultant(model: ModelIn) -> tuple[list[float], list[float]]:
    """Retorna (F[3], M[3]) aplicados, momento em torno da origem."""
    F = [0.0, 0.0, 0.0]
    M = [0.0, 0.0, 0.0]
    coords: Dict[int, Tuple[float, float, float]] = {n.id: (n.x, n.y, n.z) for n in model.nodes}
    members: Dict[int, MemberIn] = {m.id: m for m in model.members}

    def axes(member_id: int):
        mem = members.get(member_id)
        if mem is None:
            return None
        ri = coords.get(mem.node_i)
        rj = coords.get(mem.node_j)
        if ri is None or rj is None:
            return None
        x_loc, y_loc, z_loc, L = compute_member_orientation(ri, rj, mem.rotation_deg)
        return ri, x_loc, y_loc, z_loc, L

    # 1. Cargas nodais (globais)
    for nl in model.nodal_loads:
        r = coords.get(nl.node_id, (0.0, 0.0, 0.0))
        m = _cross(r, nl.F)
        for k in range(3):
            F[k] += nl.F[k]
            M[k] += m[k] + nl.M[k]

    # 2. Carga de barra uniforme (full-length) = trapezio a=0, b=L, v1=v2=q
    for ml in model.member_loads:
        ax = axes(ml.member_id)
        if ax is None:
            continue
        ri, x_loc, y_loc, z_loc, L = ax
        qg = _to_global(ml.q, ml.coord_system, x_loc, y_loc, z_loc)
        _add_trapezoid(F, M, ri, x_loc, 0.0, L, qg, qg)

    # 3. Momento distribuido uniforme (solve_v2): binario livre m*L, sem forca
    for mm in model.member_moments:
        ax = axes(mm.member_id)
        if ax is None:
            continue
        ri, x_loc, y_loc, z_loc, L = ax
        mg = _to_global(mm.m, mm.coord_system, x_loc, y_loc, z_loc)
        for k in range(3):
            M[k] += mg[k] * L

    # 4. Acoes de barra com posicao
    for act in model.member_actions:
        if act.kind == 3:          # deformacao imposta/termica: autoequilibrada, resultante externa nula
            continue
        ax = axes(act.member_id)
        if ax is None:
            continue
        ri, x_loc, y_loc, z_loc, L = ax
        v1g = _to_global(act.v1, act.coord_system, x_loc, y_loc, z_loc)
        if act.kind == 1:          # forca pontual em a
            r = [ri[k] + act.a * x_loc[k] for k in range(3)]
            mm = _cross(r, v1g)
            for k in range(3):
                F[k] += v1g[k]
                M[k] += mm[k]
        elif act.kind == 2:        # momento pontual em a (binario livre: posicao nao importa)
            for k in range(3):
                M[k] += v1g[k]
        elif act.kind == 0:        # trapezoidal/parcial entre a e b
            b = act.b if act.b is not None else L
            v2g = _to_global(act.v2, act.coord_system, x_loc, y_loc, z_loc)
            _add_trapezoid(F, M, ri, x_loc, act.a, b, v1g, v2g)

    return F, M


def check_equilibrium(applied_F: List[float], applied_M: List[float],
                      reaction_F: List[float], reaction_M: List[float]) -> bool:
    """Validação da identidade física: ΣF_aplicado + ΣF_reações = 0 e ΣM_aplicado + ΣM_reações = 0."""
    res_F = [applied_F[k] + reaction_F[k] for k in range(3)]
    res_M = [applied_M[k] + reaction_M[k] for k in range(3)]
    scale_F = max(_norm(applied_F), _norm(reaction_F))
    scale_M = max(_norm(applied_M), _norm(reaction_M))
    tol_F = settings.EQUILIBRIUM_REL_TOL * scale_F + settings.EQUILIBRIUM_ABS_FLOOR
    tol_M = settings.EQUILIBRIUM_REL_TOL * scale_M + settings.EQUILIBRIUM_ABS_FLOOR
    return _norm(res_F) <= tol_F and _norm(res_M) <= tol_M
