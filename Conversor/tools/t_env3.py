import sys
sys.path.insert(0,'.')
from t_interp import *
path=sys.argv[1]; off=int(sys.argv[2],0)
d=open(path,'rb').read()
I=make_interp(); st=Stream(d,off); args=[None]*10; args[9]=st
r,env=I.call_user('ed3dlib.dll','FUN_1801d20b0',args,want_env=True)
def fmt(a,depth,maxd):
    if isinstance(a,Ptr): return fmt(a.value,depth,maxd)
    if isinstance(a,Cells) and len(a)==1: return fmt(a[0],depth,maxd)
    if isinstance(a,Obj):
        if depth>=maxd: return a.name[-6:]+'{..}'
        return a.name[-6:]+'['+', '.join(fmt(x,depth+1,maxd) for x in a.args)+']'
    if isinstance(a,list): return '['+', '.join(fmt(x,depth+1,maxd) for x in a[:3])+(' ..%d'%len(a) if len(a)>3 else '')+']'
    if isinstance(a,float): return '%.4g'%a
    return repr(a)
for var in sys.argv[3].split(','):
    v=env.get(var)
    print('==',var,type(v).__name__)
    if isinstance(v,list) and v: print(fmt(v[0],0,int(sys.argv[4]))[:int(sys.argv[5])])
    elif isinstance(v,Obj): print(fmt(v,0,int(sys.argv[4]))[:int(sys.argv[5])])
