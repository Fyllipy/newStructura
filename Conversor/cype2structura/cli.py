"""Linha de comando: converte projetos do CYPE 3D (.ed3) em modelos do Structura (.dat)."""
from __future__ import annotations

import argparse
import glob
import os
import sys

from .reader import Ed3ReadError, program_version
from .model import build_model
from .writer import write_dat


def convert_file(src: str, dst: str, verbose=True, **options) -> dict | None:
    with open(src, "rb") as fh:
        buf = fh.read()
    version = program_version(buf)
    try:
        model = build_model(buf)
    except Ed3ReadError as exc:
        print(f"[ERRO] {os.path.basename(src)} ({version}): {exc}", file=sys.stderr)
        return None
    report = write_dat(model, dst, os.path.basename(src), **options)
    if verbose:
        print(f"[OK] {os.path.basename(src)} ({version}) -> {dst}")
        print(f"     nos={report['nodes']} barras={report['members']} perfis={report['sections']} apoios={report['supports']} "
              f"cargas lineares={report['member_loads']} paineis={report['area_loads']} casos={report['cases']}")
        if report["skipped_members"]:
            print(f"     {len(report['skipped_members'])} barra(s) sem perfil nao convertida(s)")
        for w in report["warnings"][:15]:
            print("     aviso:", w)
        if len(report["warnings"]) > 15:
            print(f"     ... e mais {len(report['warnings']) - 15} aviso(s)")
    return report


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Converte projetos CYPE 3D (.ed3) para o formato .dat do Structura")
    ap.add_argument("entrada", nargs="+", help="arquivo(s) .ed3 ou pasta com .ed3")
    ap.add_argument("-o", "--saida", help="pasta (ou arquivo, se houver uma unica entrada) de saida (padrao: ./saida)", default=None)
    ap.add_argument("--combinacoes", action="store_true",
                    help="grava uma combinacao unitaria por caso de carga (o CYPE gera as combinacoes pela norma; elas nao sao convertidas)")
    ap.add_argument("--sem-estabilizar", action="store_true",
                    help="nao fixar rotacoes que formam mecanismos de corpo rigido (o Structura nao resolve modelos com mecanismos)")
    ap.add_argument("--madeira-e-gpa", type=float, default=None, help="modulo de elasticidade da madeira (GPa); padrao 11 (C24)")
    ap.add_argument("--madeira-g-gpa", type=float, default=None, help="modulo de cisalhamento da madeira (GPa); padrao 0,69")
    ap.add_argument("--madeira-peso", type=float, default=None, help="peso especifico da madeira (kN/m3); padrao 4,2")
    args = ap.parse_args(argv)
    timber = {}
    if args.madeira_e_gpa is not None:
        timber["E"] = args.madeira_e_gpa * 1e6
    if args.madeira_g_gpa is not None:
        timber["G"] = args.madeira_g_gpa * 1e6
    if args.madeira_peso is not None:
        timber["gamma"] = args.madeira_peso
    options = {"unit_combinations": args.combinacoes, "timber": timber or None, "stabilize_model": not args.sem_estabilizar}
    files = []
    for e in args.entrada:
        files += sorted(glob.glob(os.path.join(e, "*.ed3"))) if os.path.isdir(e) else [e]
    ok = 0
    for f in files:
        if args.saida and len(files) == 1 and args.saida.lower().endswith(".dat"):
            dst = args.saida
        else:
            folder = args.saida or os.path.join(os.getcwd(), "saida")
            os.makedirs(folder, exist_ok=True)
            dst = os.path.join(folder, os.path.splitext(os.path.basename(f))[0] + ".dat")
        if convert_file(f, dst, **options):
            ok += 1
    print(f"{ok}/{len(files)} arquivo(s) convertido(s)")
    return 0 if ok == len(files) else 1


if __name__ == "__main__":
    sys.exit(main())
