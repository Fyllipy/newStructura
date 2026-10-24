import sys; sys.path.insert(0,'.')
from libs import *
def probe(path):
    L=libs(); R=Reader(L); R.soft=True
    d=open(path,'rb').read()
    best=(0,None,None); R.soft=True
    ops=L[0].ops('FUN_1801cce10')[1:]
    for o in range(0,len(d)-100):
        if not (20<=d[o]<=33): continue
        pos=o+1; n=0; err=None
        for oi in range(1,len(ops)):
            try:
                v,pos=R.rec(L[0],'FUN_1801cce10',d,pos,start_op=oi,stop_op=oi+1); n+=1
            except Exception as e:
                err=(str(e)[:90],list(getattr(R,'errstack',[]))[-3:]); 
                if hasattr(R,'errstack'): del R.errstack
                break
        if n>best[0]: best=(n,o,err)
    return best
if __name__=="__main__":
    print(probe(sys.argv[1]))
