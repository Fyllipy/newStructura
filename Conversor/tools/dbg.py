import sys; sys.path.insert(0,'.')
from libs import *
path,off=sys.argv[1],int(sys.argv[2],0)
L=libs(); R=Reader(L); R.soft=True
d=open(path,'rb').read()
ops=L[0].ops('FUN_1801cce10')[1:]
pos=off+1
for oi in range(1,len(ops)):
    try:
        v,pos=R.rec(L[0],'FUN_1801cce10',d,pos,start_op=oi,stop_op=oi+1)
    except Exception as e:
        print('op',oi,ops[oi][1][-40:],'pos',hex(pos),str(e)[:200]); print(R.errstack[-6:]); break
else: print('ALL OK',hex(pos))
