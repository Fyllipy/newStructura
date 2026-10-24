import sys; sys.path.insert(0,'.')
from libs import *
# find the candidate with max ops parsed
path=sys.argv[1]
L=libs(); R=Reader(L); R.soft=True
d=open(path,'rb').read()
ops=L[0].ops('FUN_1801cce10')[1:]
best=(0,None,None)
for o in range(len(d)-100):
    if not (20<=d[o]<=33): continue
    pos=o+1; n=0; err=None
    for oi in range(1,23):
        try: v,pos=R.rec(L[0],'FUN_1801cce10',d,pos,start_op=oi,stop_op=oi+1); n+=1
        except Exception as e:
            err=(oi,hex(pos),str(e)[:100],R.errstack[-4:] if hasattr(R,'errstack') else None)
            if hasattr(R,'errstack'): del R.errstack
            break
    if n>best[0]: best=(n,hex(o),err)
print(best)
