"""Compila os writers decompilados (Ghidra) num schema.json autocontido para ler .ed3 (CYPE 3D 2026)."""
import json,re,sys
sys.path.insert(0,'.')
from libs import *
L=libs(); R=Reader(L)
ROOT='FUN_1801cce10'
MAXOP=24  # ate gencar (antes das uniões)
out={}
def key(lib,fn): return lib.dll[:-4]+':'+fn
def conv_ops(lib,fn,ops,seen):
    res=[]
    for (f,args) in ops:
        if f=='__opt__':
            res.append(['optblock',conv_ops(lib,fn,args,seen)]); continue
        m=re.match(r'cypesave_graba_(\w+)$',f)
        if f.startswith('cypesave_nousar_graba_apunorde'):
            sub=args.split(',')[-1].strip(); k=ref(lib,sub,seen)
            res.append(['optarr' if f.endswith('opcional') else 'arr',k]); continue
        if m:
            kd=m.group(1)
            if kd=='uchar' and args.startswith('DAT_'):
                res.append(['tag',lib.pe.u8(sym_addr(args)),True if sym_addr(args)>=0x181b00000 else False]); continue
            if kd in PRIM: res.append([PRIM[kd]]); continue
            if kd.startswith('arr_punteros'):
                sub=args.split(',')[-1].strip(); res.append(['optarr' if kd.endswith('opcional') else 'arr',ref(lib,sub,seen)]); continue
            if kd=='estructura_opcional2':
                sub=args.split(',')[-1].strip(); res.append(['opt',ref(lib,sub,seen)]); continue
            if kd.startswith('arr_puntos2D') or kd.startswith('arr_puntos3D'):
                res.append(['pts',2 if '2D' in kd else 3,kd.endswith('opcional')]); continue
            if kd in ARRS: res.append(['arrp',ARRS[kd]]); continue
            raise NotImplementedError((fn,f))
        res.append(['rec',ref(lib,f,seen)])
    return res
def ref(lib,fn,seen):
    try: l2,f2=R.resolve(lib,fn)
    except KeyError: return '?'+fn
    k=key(l2,f2)
    if k not in out and k not in seen:
        seen.add(k)
        out[k]=conv_ops(l2,f2,l2.ops(f2)[1:],seen)
    return k
seen=set()
rootlib=L[0]
rootops=rootlib.ops(ROOT)[1:MAXOP]
out['root']=conv_ops(rootlib,ROOT,rootops,seen)
json.dump(out,open('out/schema.json','w'),separators=(',',':'))
import os; print(len(out),'funcs',os.path.getsize('out/schema.json'),'bytes')
