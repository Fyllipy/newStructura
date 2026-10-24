import sys, time
sys.path.insert(0,'.')
from t_interp import *
path=sys.argv[1]
d=open(path,'rb').read()
I=make_interp()
t=time.time(); n=0
for o in range(len(d)-100):
    if not (20<=d[o]<=33): continue
    st=Stream(d,o); args=[None]*10; args[9]=st
    n+=1
    try:
        r,env=I.call_user('ed3dlib.dll','FUN_1801d20b0',args,want_env=True)
    except Exception as e:
        I.stack=[]; I.depth=0
        continue
    print('OK start',hex(o),'end',hex(st.pos),'tries',n,round(time.time()-t,1),'s',
          'nodes',len(env['local_150']),'bars',len(env['local_d8']),'pieces',len(env['local_158']),'desba',len(env['local_160']))
    break
else: print('nao encontrado',n,round(time.time()-t,1))
