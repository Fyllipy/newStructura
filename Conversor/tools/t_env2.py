import sys
sys.path.insert(0,'.')
from t_interp import *
path=sys.argv[1]; off=int(sys.argv[2],0)
d=open(path,'rb').read()
I=make_interp(); st=Stream(d,off); args=[None]*10; args[9]=st
r,env=I.call_user('ed3dlib.dll','FUN_1801d20b0',args,want_env=True)
def fmt(a,depth=0):
    if isinstance(a,Ptr): return fmt(a.value,depth)
    if isinstance(a,Obj):
        if depth>=int(sys.argv[3]): return a.name[-6:]+'{..}'
        return a.name[-6:]+'['+', '.join(fmt(x,depth+1) for x in a.args)+']'
    if isinstance(a,list): return '['+', '.join(fmt(x,depth+1) for x in a[:4])+(' ..%d'%len(a) if len(a)>4 else '')+']'
    if isinstance(a,float): return '%.4g'%a
    return repr(a)
for var,label in [('local_d8','barra'),('local_158','pieza'),('local_160','desba'),('local_e0','grufle'),('local_168','panyo'),('local_100','supcar'),('local_150','nudo')]:
    v=env.get(var)
    print('==',label,var,type(v).__name__, len(v) if isinstance(v,list) else '')
    if isinstance(v,list) and v: print(fmt(v[0],0)[:int(sys.argv[4])])
