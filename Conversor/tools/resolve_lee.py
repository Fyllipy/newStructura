"""Fecha o conjunto de funcoes de leitura (*_lee) entre as DLLs do CYPE 3D, usando o Ghidra (headless)."""
import json
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, ".")
SP = "C:/Users/JOO~1/AppData/Local/Temp/claude/C--Users-Jo-o-Documents-Repos-Conversor/62e829b0-04f7-4a4c-ac8d-ffd81bb16d8a/scratchpad"
APP = "D:/CYPE Ingenieros/Versão 2026/CYPE Menu/app_302/"
GH = "C:/Ghidra/support/analyzeHeadless.bat"
MAN = "out/manifest_lee.json"
SKIP = set()
idx = json.load(open("out/exportidx.json"))


def manifest():
    if os.path.exists(MAN):
        return json.load(open(MAN))
    return {"ed3dlib.dll": {"txt": "lee_closure.txt", "names": []}}


def save(m):
    json.dump(m, open(MAN, "w"), indent=1)


def defined_names(m):
    names = set()
    for dll, v in m.items():
        t = open(SP + "/" + v["txt"], encoding="utf-8", errors="replace").read()
        names |= set(re.findall(r"\n===== (\w+) @", t))
    return names


def called_lee(m):
    out = {}
    for dll, v in m.items():
        t = open(SP + "/" + v["txt"], encoding="utf-8", errors="replace").read()
        t = re.sub(r"-- callees:.*?(?=\n=====|\Z)", "", t, flags=re.S)
        for n in re.findall(r"\b(\w+_lee\w*)\b", t):
            if n.startswith("cypesave_") or n in SKIP or n.startswith("DelayLoad"):
                continue
            out.setdefault(n, dll)
    return out


m = manifest()
for it in range(14):
    defined = defined_names(m)
    miss = {n: d for n, d in called_lee(m).items() if n not in defined}
    print("iter", it, "faltam", len(miss), sorted(miss)[:30], flush=True)
    if not miss:
        break
    by = {}
    for n in miss:
        dl = idx.get(n)
        if not dl:
            print("  sem exportacao:", n)
            continue
        by.setdefault(dl[0], set()).add(n)
    if not by:
        break
    for dll, names in by.items():
        ent = m.setdefault(dll, {"txt": "leeext_" + dll[:-4] + ".txt", "names": []})
        ent["names"] = sorted(set(ent["names"]) | names)
        if not os.path.exists(SP + "/" + dll):
            shutil.copy(APP + dll, SP + "/" + dll)
            subprocess.run([GH, SP + "/gproj", "cype", "-import", SP + "/" + dll, "-overwrite"], capture_output=True)
        subprocess.run(["bash", "ghidra/run.sh", dll, ent["txt"], "name", "+".join(ent["names"]), "8"], capture_output=True)
        print("  closure", dll, ent["names"], flush=True)
    save(m)
save(m)
