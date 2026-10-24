"""Builds a reader schema from Ghidra decompiled *_graba functions (CYPE 3D .ed3)."""
import re,struct
from pe import PE
SP="C:/Users/JOO~1/AppData/Local/Temp/claude/C--Users-Jo-o-Documents-Repos-Conversor/62e829b0-04f7-4a4c-ac8d-ffd81bb16d8a/scratchpad"
APP="D:/CYPE Ingenieros/Versão 2026/CYPE Menu/app_302/"
PRIM={'uchar':'u8','char':'u8','bool':'bool','ushort':'u16','short':'u16','ulong':'u32','long':'u32','ulong64':'u64','long64':'u64','double':'f64','float':'f32','cadena_unicode':'ustr','cadena':'astr','enum':'u32'}
ARRS={'arr_ulong':'u32','arr_long':'u32','arr_ushort':'u16','arr_uchar':'u8','arr_double':'f64','arr_ulong64':'u64','arr_long64':'u64','arr_bool':'bool','arr_cadenas':'astr','arr_cadenas_unicode':'ustr'}

class Lib:
    def __init__(self,dll,txt):
        self.dll=dll; self.pe=PE(APP+dll); self.funcs={}
        t=open(SP+"/"+txt,encoding='utf-8',errors='replace').read()
        for b in t.split("\n===== ")[1:]:
            name=b.split(' ',1)[0]; body=b.split('-- callees')[0].split('\n',1)[1]
            self.funcs[name]=body
    def ops(self,name):
        body=self.funcs[name]
        body=re.sub(r'error_interno\((?:[^;]|\n)*?\);','',body)
        k=re.search(r'\)\s*\n\s*\{',body)
        if k: body=body[k.end():]
        mo=re.search(r'if \(\w+ == [^{]*0x0\) \{\s*cypesave_graba_bool\(0,param_2\);\s*\}\s*else \{(.*)\}\s*return;',body,re.S)
        if mo:
            sub=[o for o in self._scan(mo.group(1)) if not (o[0]=='cypesave_graba_bool' and o[1].startswith('1,'))]
            return [('hdr','')]+[('__opt__',sub)]
        return [('hdr','')]+self._scan(body)
    def _scan(self,body):
        out=[]
        for m in re.finditer(r'(?<![\w])(cypesave_\w+|FUN_\w+|\w+_graba\w*|ebaresul\w+|\w+_grabar?\w*)\s*\(([^;]*?)\);',body,re.S):
            fn,args=m.group(1),m.group(2)
            args=re.sub(r'\s+',' ',args)
            if fn.startswith('FUN_') and re.fullmatch(r'(\(\w+\s*\*?\))?\s*param_1',args.strip()): continue
            out.append((fn,args))
        return out

def sym_addr(arg):
    m=re.search(r'DAT_([0-9a-f]+)',arg); return int(m.group(1),16) if m else None


class Reader:
    def __init__(self,libs):
        self.libs=libs  # list of Lib; first is main
        self.ext={}     # external function name -> Lib
        for L in libs:
            for n in L.funcs:
                if not n.startswith('FUN_'): self.ext.setdefault(n,L)
        self.trace=False
    # ---- primitives
    def prim(self,buf,pos,k):
        if k=='u8' or k=='bool': return buf[pos],pos+1
        if k=='u16': return struct.unpack_from('<H',buf,pos)[0],pos+2
        if k=='u32': return struct.unpack_from('<I',buf,pos)[0],pos+4
        if k=='u64': return struct.unpack_from('<Q',buf,pos)[0],pos+8
        if k=='f64': return struct.unpack_from('<d',buf,pos)[0],pos+8
        if k=='f32': return struct.unpack_from('<f',buf,pos)[0],pos+4
        if k=='astr':
            n=struct.unpack_from('<H',buf,pos)[0]; return buf[pos+2:pos+2+n].decode('latin1'),pos+2+n
        if k=='ustr':
            n=struct.unpack_from('<I',buf,pos)[0]
            if n>100000: raise ValueError('bad ustr len %d'%n)
            return buf[pos+4:pos+4+2*n].decode('utf-16le'),pos+4+2*n
        raise ValueError(k)
    def resolve(self,lib,fn):
        if fn.startswith('FUN_') or fn in lib.funcs: 
            if fn in lib.funcs: return lib,fn
        if fn in self.ext: return self.ext[fn],fn
        raise KeyError('unresolved %s in %s'%(fn,lib.dll))
    def runops(self,lib,fn,ops,buf,pos,depth):
        # ops given directly (no header)
        self._override=ops
        return self.rec(lib,fn,buf,pos,depth,_ops=ops)
    def rec(self,lib,fn,buf,pos,depth=0,start_op=0,stop_op=None,_ops=None):
        lib,fn=self.resolve(lib,fn)
        if not hasattr(self,'stack'): self.stack=[]
        self.stack.append(lib.dll[:-4]+':'+fn)
        try:
            return self._rec(lib,fn,buf,pos,depth,start_op,stop_op,_ops)
        except Exception:
            if not hasattr(self,'errstack'): self.errstack=list(self.stack)
            raise
        finally:
            self.stack.pop()
    def _rec(self,lib,fn,buf,pos,depth,start_op,stop_op,_ops):
        ops=_ops if _ops is not None else lib.ops(fn)[1:]
        vals=[]
        for oi,(f,args) in enumerate(ops):
            if oi<start_op: continue
            if stop_op is not None and oi>=stop_op: break
            if f in ('error_interno',): continue
            m=re.match(r'cypesave_graba_(\w+)$',f)
            if f=='__opt__':
                flag,pos=self.prim(buf,pos,'bool')
                if flag:
                    v,pos=self.runops(lib,fn,args,buf,pos,depth+1); vals.append(('opt',v))
                else: vals.append(('opt',None))
                continue
            if f.startswith('cypesave_nousar_graba_apunorde'):
                opt=f.endswith('opcional'); sub=args.split(',')[-1].strip()
                if opt:
                    flag,pos=self.prim(buf,pos,'bool')
                    if not flag: vals.append(('optarr',None)); continue
                n,pos=self.prim(buf,pos,'u32')
                if n>20_000: raise ValueError('array too big %d in %s'%(n,fn))
                arr=[]
                for i in range(n):
                    v,pos=self.rec(lib,sub,buf,pos,depth+1); arr.append(v)
                vals.append(('arr:'+sub,arr)); continue
            if m:
                k=m.group(1)
                if k=='uchar' and args.startswith('DAT_'):
                    exp=lib.pe.u8(sym_addr(args)); v,pos=self.prim(buf,pos,'u8')
                    if v!=exp:
                        if sym_addr(args)>=0x181b00000 or getattr(self,'soft',False):
                            self.warn=getattr(self,'warn',[])+[(fn,pos-1,v,exp)]
                        else: raise ValueError('tag mismatch in %s@%x: got %d expect %d'%(fn,pos-1,v,exp))
                    vals.append(('tag',v)); continue
                if k in PRIM:
                    v,pos=self.prim(buf,pos,PRIM[k]); vals.append((PRIM[k],v)); continue
                if k in ('arr_punteros',) or k.startswith('arr_punteros') :
                    sub=args.split(',')[-1].strip()
                    opt=k.endswith('opcional')
                    if opt:
                        flag,pos=self.prim(buf,pos,'bool')
                        if not flag: vals.append(('optarr',None)); continue
                    n,pos=self.prim(buf,pos,'u32')
                    if n>20_000: raise ValueError('array too big %d in %s'%(n,fn))
                    arr=[]
                    for i in range(n):
                        v,pos=self.rec(lib,sub,buf,pos,depth+1); arr.append(v)
                    vals.append(('arr:'+sub,arr)); continue
                if k=='estructura_opcional2':
                    sub=args.split(',')[-1].strip()
                    flag,pos=self.prim(buf,pos,'bool')
                    if flag:
                        v,pos=self.rec(lib,sub,buf,pos,depth+1); vals.append(('opt:'+sub,v))
                    else: vals.append(('opt:'+sub,None))
                    continue
                if k.startswith('arr_puntos2D') or k.startswith('arr_puntos3D'):
                    dim=2 if '2D' in k else 3
                    if k.endswith('opcional'):
                        flag,pos=self.prim(buf,pos,'bool')
                        if not flag: vals.append(('optpts',None)); continue
                    n,pos=self.prim(buf,pos,'u32'); pts=[]
                    for i in range(n):
                        p=[]
                        for j in range(dim):
                            x,pos=self.prim(buf,pos,'f64'); p.append(x)
                        pts.append(tuple(p))
                    vals.append(('pts',pts)); continue
                if k in ARRS:
                    n,pos=self.prim(buf,pos,'u32')
                    arr=[]
                    for i in range(n):
                        v,pos=self.prim(buf,pos,ARRS[k]); arr.append(v)
                    vals.append(('arr_'+ARRS[k],arr)); continue
                raise NotImplementedError('%s in %s: %s'%(f,fn,args))
            # nested record (internal or external)
            if f.startswith('FUN_') or f in self.ext or f in lib.funcs:
                v,pos=self.rec(lib,f,buf,pos,depth+1); vals.append(('rec:'+f,v)); continue
            raise NotImplementedError('call %s in %s'%(f,fn))
        return vals,pos
