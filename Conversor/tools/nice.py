import struct,sys
d=open(sys.argv[1],'rb').read()
lo=int(sys.argv[2],0); hi=int(sys.argv[3],0) if len(sys.argv)>3 else len(d)
def nice(v):
    if v!=v or abs(v)>1e5: return False
    if abs(v)<1e-3: return False
    for k in (1000,100,10000):
        r=v*k
        if abs(r-round(r))<1e-6*max(1,abs(r)): return True
    return False
out=[]
i=lo
while i<hi-8:
    v=struct.unpack_from('<d',d,i)[0]
    if nice(v): out.append((i,v))
    i+=1
# runs: group by offset difference <=12
runs=[];cur=[]
for o,v in out:
    if cur and o-cur[-1][0]<=9: cur.append((o,v))
    else:
        if len(cur)>=3: runs.append(cur)
        cur=[(o,v)]
if len(cur)>=3: runs.append(cur)
for r in runs[:int(sys.argv[4]) if len(sys.argv)>4 else 40]:
    print(hex(r[0][0]),len(r),[round(v,4) for o,v in r][:12])
