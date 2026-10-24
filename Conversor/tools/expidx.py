import struct,glob,os,json
APP="D:/CYPE Ingenieros/Versão 2026/CYPE Menu/app_302/"
def exports(p):
    d=open(p,'rb').read()
    try:
        pe=struct.unpack_from('<I',d,0x3c)[0]
        n=struct.unpack_from('<H',d,pe+6)[0]; opt=struct.unpack_from('<H',d,pe+20)[0]
        er,es=struct.unpack_from('<II',d,pe+24+112)
        if not er: return []
        secs=[]; o=pe+24+opt
        for i in range(n):
            vs,va,rs,ro=struct.unpack_from('<IIII',d,o+8); secs.append((va,vs,ro,rs)); o+=40
        def off(r):
            for a,vs,ro,rs in secs:
                if a<=r<a+max(vs,rs): return ro+r-a
        eo=off(er)
        nn=struct.unpack_from('<I',d,eo+24)[0]; names=off(struct.unpack_from('<I',d,eo+32)[0])
        out=[]
        for i in range(nn):
            nr=struct.unpack_from('<I',d,names+4*i)[0]; no=off(nr)
            e=d.index(b'\0',no); out.append(d[no:e].decode('latin1'))
        return out
    except Exception as ex: return []
if __name__=="__main__":
    idx={}
    for p in glob.glob(APP+"*.dll"):
        for n in exports(p): idx.setdefault(n,[]).append(os.path.basename(p))
    json.dump(idx,open('out/exportidx.json','w'))
    print(len(idx))
