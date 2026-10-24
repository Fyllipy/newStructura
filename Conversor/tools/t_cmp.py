import sys
sys.path.insert(0,'.'); sys.path.insert(0,'..')
from t_interp import *
from cype2structura.ed3reader import Reader
path="D:/CYPE Ingenieros/Projetos/CYPE 3D/Tesouras Recepção Cambuci.ed3"
d=open(path,'rb').read()
# writer-side sequence for desba #0 (start 0xdd922 + count)
R=Reader(); root=R.schema['root']
pos=0xa1b37
for i,op in enumerate(root[:15]):
    v,pos=R._run([op],d,pos,False)
start=pos          # inicio do array de desbas
# primitivas do 1o elemento pela escrita
import struct
cnt=struct.unpack_from('<I',d,start)[0]
vals,endpos=R.read_fn(R.schema['ed3dlib:FUN_180255150'] and 'ed3dlib:FUN_180255150',d,start+4,False)
def prims(rec,out):
    for k,x in rec:
        if k.startswith('arr:'):
            for r in x: prims(r,out)
        elif (k.startswith('rec:') or k.startswith('opt')) and isinstance(x,list): prims(x,out)
        elif k=='pts' or k.startswith('arr_'): out.append((k,'list'))
        else: out.append((k,x))
    return out
w=prims(vals,[])
print('desba0 writer prims',len(w),'end',hex(endpos))
# interpretador: executa leitor do desba (FUN_1801ea220 via 1_dato_extra) com stream a partir de start+4
I=make_interp()
st=Stream(d,start+4); log=[]
orig=Stream.take
def take(self,fmt):
    p=self.pos; v=orig(self,fmt); log.append((hex(p),fmt,v)); return v
Stream.take=take
try:
    I.call_user('ed3dlib.dll','FUN_1801ea220',[None,None,None,None,None,None,None,None,None,None][:0] or [None,st][:2]) 
except Exception as e: print('ERR',e,getattr(e,'stack',None))
print(len(log))
# compare
j=0
for (p,fmt,v) in log:
    if j>=len(w): break
    k,x=w[j]
    ok = (isinstance(v,float) and isinstance(x,float) and abs(v-x)<1e-12) or v==x
    if not ok:
        print('DIVERGE at read',j,p,fmt,v,'writer',k,x); break
    j+=1
else: print('no divergence in',j)
