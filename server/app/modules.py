"""Registro de modulos do Structura e nivel de protecao (SPEC §7.1).

  A  -> saida so finalizavel com K assinado pelo servidor (s = HKDF(K) via _z8r4q).
  B  -> so gate de licenca ONLINE (sem K). Offline => bloqueado.
  EXE-> coberto pelo direito de executar (login/heartbeat/integridade): sem gate por acao.

Nenhuma decisao de nivel vem do cliente: o servidor mapeia modulo -> nivel aqui.
"""
from __future__ import annotations
from typing import Literal

LEVEL_A = "A"
LEVEL_B = "B"
LEVEL_EXE = "EXE"

MODULES: dict[str, str] = {
    # Nivel A (keystone)
    "calculo": LEVEL_A,
    "desenhos": LEVEL_A,          # geracao de desenhos/pranchas
    "export_dxf_pdf": LEVEL_A,
    "dimensionamento": LEVEL_A,   # verificacoes normativas
    "quantitativos": LEVEL_A,
    "relatorios": LEVEL_A,        # export/impressao/relatorio (slots que desselam com s)
    # Nivel B (gate de licenca online, sem K)
    "detalhamento": LEVEL_B,
    "memorial": LEVEL_B,
    # Cobertos so pelo direito de executar (nao produzem entregavel valioso por si)
    "modelagem": LEVEL_EXE,
}

LevelBFeature = Literal["detalhamento", "memorial"]
LevelAModule = Literal["calculo", "desenhos", "export_dxf_pdf", "dimensionamento",
                       "quantitativos", "relatorios"]


def level_of(module: str) -> str | None:
    return MODULES.get(module)
