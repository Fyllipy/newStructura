import sys, json
sys.path.insert(0, '.')
from cdec import split_functions
from cinterp import *
SP = "C:/Users/JOO~1/AppData/Local/Temp/claude/C--Users-Jo-o-Documents-Repos-Conversor/62e829b0-04f7-4a4c-ac8d-ffd81bb16d8a/scratchpad"
def make_interp():
    man = json.load(open('out/manifest_lee.json'))
    libs = {}
    for dll, v in man.items():
        t = open(SP + "/" + v["txt"], encoding="utf-8", errors="replace").read()
        libs[dll] = split_functions(t)
    return Interp(libs)
if __name__ == "__main__":
    I = make_interp()
    path = sys.argv[1]; off = int(sys.argv[2], 0)
    d = open(path, 'rb').read()
    st = Stream(d, off)
    args = [None] * 10; args[9] = st
    try:
        r = I.call_user("ed3dlib.dll", "FUN_1801d20b0", args)
        print("OK pos", hex(st.pos), type(r).__name__, (r.name if isinstance(r, Obj) else r))
        if isinstance(r, Obj):
            print([(type(a).__name__, len(a) if isinstance(a, list) else (a.name if isinstance(a, Obj) else a)) for a in r.args][:60])
    except Exception as e:
        print("ERRO", type(e).__name__, e, hex(st.pos), getattr(e, "stack", None))
