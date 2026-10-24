import sys,glob,os,time; sys.path.insert(0,'.')
from libs import *
L=libs(); R=Reader(L); R.soft=True
files=sorted(glob.glob("D:/CYPE Ingenieros/Projetos/CYPE 3D/*.ed3")+glob.glob("D:/CYPE Ingenieros/Exemplos/CYPE 3D/*.ed3"))
for f in files:
    d=open(f,'rb').read(); res=None; t=time.time()
    for o in range(0,len(d)-100):
        if d[o]==33:
            try:
                R.warn=[]
                v,np=R.rec(L[0],'FUN_1801cce10',d,o,stop_op=23); res=(o,np); break
            except Exception as e: err=str(e)[:60]
    print(os.path.basename(f), len(d), d[4:15], 'OK %x-%x'%res if res else 'FAIL', round(time.time()-t,1),flush=True)
