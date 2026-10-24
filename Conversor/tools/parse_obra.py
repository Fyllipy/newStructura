import sys; sys.path.insert(0,'.')
from libs import *
def load(path):
    L=libs(); R=Reader(L); R.soft=True
    d=open(path,'rb').read()
    for o in range(0,len(d)-100):
        if d[o]==33:
            try:
                v,np=R.rec(L[0],'FUN_1801cce10',d,o,stop_op=23); return d,v,o,np
            except Exception: pass
    return d,None,None,None
def flat(r,out=None,pre=''):
    out=[] if out is None else out
    for k,x in r:
        if isinstance(x,list) and x and isinstance(x[0],tuple) and isinstance(x[0][0],str) and isinstance(x[0][1],(int,float,str,list,type(None))) and not k.startswith('arr_') and k!='pts':
            if k.startswith('arr:'): out.append((pre+k,'['+str(len(x))+']'))
            else: flat(x,out,pre+k+'/')
        elif k.startswith('arr:'):
            out.append((pre+k,'['+str(len(x))+']'))
        elif isinstance(x,list): out.append((pre+k,x if len(x)<8 else '['+str(len(x))+']'))
        else: out.append((pre+k,x))
    return out

def getv(r,kind,n=0):
    c=0
    for k,x in r:
        if k==kind:
            if c==n: return x
            c+=1
def find(r,path):
    for k,x in r:
        if k==path: return x
def profiles(des):
    """des: desba record -> (typeA, typeB, [ (series,name) x5 ], active)"""
    o=find(des,'opt:FUN_1805a5cd0')
    if o is None: return None
    ta=getv(o,'u16',0)
    f=find(o,'rec:FUN_180737ac0')
    tb=getv(f,'u16',0)
    pzs=[x for k,x in f if k=='rec:pzmetstr_graba']
    out=[]
    for pz in pzs:
        bp=find(pz,'rec:bp3perfil_graba')
        out.append((getv(bp,'ustr',0),getv(bp,'ustr',1)))
    return ta,tb,out

def iter_prims(rec,path=()):
    for k,x in rec:
        if k.startswith('arr:'):
            for i,r in enumerate(x or []): yield from iter_prims(r,path+(k.split(':')[1],))
        elif (k.startswith('rec:') or k.startswith('opt')) and isinstance(x,list):
            yield from iter_prims(x,path+(k,))
        else: yield (path,k,x)

def nodes_of(v):
    pl={}
    for ax,i in zip('XYZ',(3,4,5)):
        pl[ax]={r[1][1]:r[2][1] for r in v[i][1]}
    out=[]
    for r in v[9][1]:
        nid=r[1][1]
        try: x=pl['X'][r[5][1]]; y=pl['Y'][r[6][1]]; z=pl['Z'][r[7][1]]
        except KeyError: x=y=z=None
        vinc=find(r,'rec:FUN_1806367e0'); vt=getv(vinc,'u16',0)
        out.append((nid,x,y,z,vt,r))
    return out
