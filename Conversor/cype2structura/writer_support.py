"""Liberacoes de extremidade e propriedades de materiais (compartilhado entre o gerador do .dat e a verificacao de estabilidade)."""
from __future__ import annotations

from collections import defaultdict

from .model import Model, TF

# Propriedades de materiais (SI do Structura: kN, m)
STEEL = {"name": "Aco_CYPE", "E": 2.1e7 * TF, "nu": 0.3, "gamma": 7.85 * TF}
TIMBER = {"name": "Madeira_C24", "E": 1.1e7, "G": 6.9e5, "gamma": 4.2}


def release_masks(model: Model):
    """Liberacoes de extremidade por barra a partir da vinculacao interior dos nos.

    bits 0-5: ux uy uz rx ry rz no no I; bits 6-11: no no J.
    """
    at_node = defaultdict(list)
    for bar in model.bars.values():
        at_node[bar.ni].append((bar, "I"))
        at_node[bar.nj].append((bar, "J"))
    masks = defaultdict(int)
    articulated = defaultdict(set)         # bar -> {'I','J'} extremos articulados
    notes = []
    for nid, node in model.nodes.items():
        items = at_node.get(nid, [])
        if len(items) < 2:
            continue
        released = []
        if node.int_type in (1, 3):
            # No articulado: as pecas ficam articuladas entre si, mas as barras de uma mesma peca (membro continuo que
            # atravessa o no) continuam rigidas entre si. No generico, os grupos de pecas rigidamente ligadas vem do arquivo.
            if node.int_type == 3:
                cls = {}
                for gi, group in enumerate(node.int_groups):
                    for pid in group:
                        cls[pid] = gi
                key_of = lambda bar: cls.get(bar.piece, ("p", bar.piece))
            else:
                key_of = lambda bar: ("p", bar.piece)
            count = defaultdict(int)
            for bar, _end in items:
                count[key_of(bar)] += 1
            keep = max(count, key=lambda k: (count[k], str(k)))
            released = [(bar, end) for bar, end in items if key_of(bar) != keep]
        elif node.int_type == 2:
            notes.append(f"no {nid}: vinculacao interior com coeficiente parcial de engastamento nao convertida (tratada como rigida)")
        elif node.int_type not in (0,):
            notes.append(f"no {nid}: vinculacao interior tipo {node.int_type} desconhecida (tratada como rigida)")
        for bar, end in released:
            articulated[bar.id].add(end)
    for bid, ends in articulated.items():
        for end in ends:
            shift = 0 if end == "I" else 6
            masks[bid] |= (0b110000 << shift)       # ry, rz
        if ends == {"I", "J"}:
            # barra de trelica: sem rigidez a torcao (libera rx numa extremidade); sem isso nos so de trelica ficam com
            # rigidez de rotacao deficiente e o solver do Structura nao consegue decompor a matriz global
            masks[bid] |= 1 << 9
    return masks, articulated, notes


