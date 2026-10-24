import sys
sys.path.insert(0,'.')
from t_interp import *
path=sys.argv[1]; off=int(sys.argv[2],0)
d=open(path,'rb').read()
I=make_interp(); st=Stream(d,off); args=[None]*10; args[9]=st
r,env=I.call_user('ed3dlib.dll','FUN_1801d20b0',args,want_env=True)
print('end',hex(st.pos))
for k,v in env.items():
    if isinstance(v,list): print(k,'list',len(v), (v[0].name if v and isinstance(v[0],Obj) else type(v[0]).__name__ if v else ''))
n=env['local_150'][0]
print(n.name, [ (type(a).__name__, a if not isinstance(a,(list,Obj,Ptr,FuncRef)) else (a.name if isinstance(a,Obj) else (a.value if isinstance(a,Ptr) else a))) for a in n.args][:30])
