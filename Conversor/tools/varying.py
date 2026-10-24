import sys,collections; sys.path.insert(0,'.')
from parse_obra import *
def varying(arr,skip=()):
    paths=collections.defaultdict(collections.Counter)
    for r in arr:
        cnt=collections.Counter()
        for p,k,x in iter_prims(r):
            key=('/'.join(p),k); cnt[key]+=1
            paths[(key,cnt[key])][repr(round(x,4) if isinstance(x,float) else x)]+=1
    return [(p,c) for p,c in paths.items() if len(c)>1 and p[0][1]!='ustr']
if __name__=="__main__":
    f=sys.argv[1]; fld=int(sys.argv[2])
    d,v,o,np=load("D:/CYPE Ingenieros/Projetos/CYPE 3D/%s.ed3"%f)
    for (key,i),c in varying(v[fld][1])[:int(sys.argv[3]) if len(sys.argv)>3 else 40]:
        print(key[0].replace('rec:','')[-60:],key[1],i,dict(c.most_common(4)))
